/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Tâches FreeRTOS de l'application (TP2, TP3).
  *
  *   DEMO_PARTIE choisit ce qui tourne, pour pouvoir montrer chaque partie
  *   du TP en démonstration en changeant une seule ligne :
  *     1 -> TacheChenillard : 3 LEDs allumées 3 s, puis chenillard R/V/B (3 s)
  *     2 -> TacheCapteurs   : lecture des capteurs, LED rouge/verte
  *     3 -> TacheCapteurs + TacheReseau : + LwIP, ping google.com, LED bleue
  *     4 -> TacheCapteurs + TacheReseau : envoi des mesures sur ThingSpeak
  *          toutes les 20 s, puis attente de la prédiction calculée par
  *          MATLAB dans le cloud et renvoyée par TalkBack (TP3.2), LED bleue
  *     5 -> TacheCapteurs + TacheIA : réseaux de neurones exécutés SUR LA
  *          CARTE (TP3.3), code généré par X-CUBE-AI : auto-test des modèles
  *          h0 (maintenant) et h3 (dans 3 h) sur les vecteurs de test de
  *          Python, et temps de calcul.
  *     6 -> démo finale cloud / edge (TP3.3c), trois tâches indépendantes :
  *          TacheCapteurs mesure ; TacheIA prédit SUR LA CARTE toutes les
  *          20 s et affiche la météo prédite sur les LEDs ; TacheReseau
  *          envoie mesures et prédictions sur ThingSpeak et les compare à
  *          celles du cloud. Sans réseau, TacheIA continue seule.
  *
  *   Les deux tâches se partagent les dernières mesures par une "boîte aux
  *   lettres" protégée par un mutex (mesures_publier / mesures_lire).
  *
  *   Une LED = un seul propriétaire (pas de conflit entre tâches) :
  *     rouge/verte -> TacheCapteurs (ou TacheChenillard en démo 1)
  *     bleue       -> TacheReseau
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "led.h"
#include "sensors.h"
#include "app_ethernet.h"
#include "net_ping.h"
#include "console.h"
#include "thingspeak.h"
#include "meteo_ia.h"
#include "horloge.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ===== Choix de la démo (voir l'en-tête du fichier) ===== */
#define DEMO_PARTIE        6

#define CHENILLARD_MS      3000U   /* partie 1 : temporisation imposée de 3 s     */
#define SENSOR_PERIOD_MS   5000U   /* partie 2 : une lecture des capteurs / 5 s   */
#define LED_BUSY_MIN_MS    300U    /* LED rouge allumée au moins 300 ms (visible) */
#define PING_HOTE          "google.com"
#define PING_PERIOD_MS     10000U  /* partie 3 : un ping toutes les 10 s          */
#define PING_TIMEOUT_MS    2000U   /* attente maximum de la réponse               */
#define TS_PERIOD_MS       20000U  /* TP3.2 : un envoi ThingSpeak toutes les 20 s
                                      (compte gratuit : 15 s minimum)            */
#define TB_SCRUTATION_MS   2000U   /* TalkBack : on regarde la file toutes les 2 s */
#define TB_ATTENTE_MAX_MS  30000U  /* ... pendant 30 s au plus après l'envoi      */
#define IA_PERIODE_MS      20000U  /* démo 6 : une prédiction sur la carte / 20 s  */

#if (DEMO_PARTIE < 1) || (DEMO_PARTIE > 6)
#error "DEMO_PARTIE doit valoir 1 a 6"
#endif

/* La clé API ThingSpeak est dans secrets.h, absent de GitHub (.gitignore).
 * __has_include permet d'afficher un message clair s'il manque. */
#if (DEMO_PARTIE == 4) || (DEMO_PARTIE == 6)
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copier Core/Inc/secrets_exemple.h en Core/Inc/secrets.h et y mettre les cles ThingSpeak"
#endif
#if !defined(TALKBACK_ID) || !defined(TALKBACK_KEY)
#error "Ajouter TALKBACK_ID et TALKBACK_KEY dans Core/Inc/secrets.h (voir secrets_exemple.h)"
#endif
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* hi2c1 est créé par CubeMX dans main.c : "extern" dit au compilateur
 * qu'il existe ailleurs, sans le recréer. */
extern I2C_HandleTypeDef hi2c1;

/* ATTENTION : en CMSIS-RTOS v2, .stack_size est en OCTETS (d'où le "* 4"). */

/* --- TacheChenillard (démo 1) : ne fait que des LEDs et des logs ---------- */
osThreadId_t TacheChenillardHandle;
const osThreadAttr_t TacheChenillard_attributes = {
  .name       = "TacheChenillard",
  .priority   = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};

/* --- TacheCapteurs (démos 2 et 3) : printf consomme beaucoup de pile ------ */
osThreadId_t TacheCapteursHandle;
const osThreadAttr_t TacheCapteurs_attributes = {
  .name       = "TacheCapteurs",
  .priority   = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

/* --- TacheReseau (démo 3) : printf + appels LwIP ------------------------- */
osThreadId_t TacheReseauHandle;
const osThreadAttr_t TacheReseau_attributes = {
  .name       = "TacheReseau",
  .priority   = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

/* --- TacheIA (démo 5) : réseau de neurones embarqué -------------------- */
osThreadId_t TacheIAHandle;
const osThreadAttr_t TacheIA_attributes = {
  .name       = "TacheIA",
  .priority   = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

static sensors_data_t s_data;

/* Boîte aux lettres : dernières mesures lues par TacheCapteurs, utilisées
 * par TacheReseau. Deux tâches touchent à la même variable : sans mutex,
 * TacheReseau pourrait lire une structure à moitié mise à jour (la
 * température de la nouvelle mesure avec l'humidité de l'ancienne). */
static sensors_data_t s_derniere_mesure;
static bool           s_mesure_dispo = false;
static osMutexId_t    s_mesures_mutex = NULL;

/* Deuxième boîte aux lettres (démo 6) : la dernière prédiction de la carte.
 * TacheIA la remplit, TacheReseau l'envoie sur ThingSpeak et la compare à
 * la réponse du cloud. Elle contient aussi la mesure utilisée, pour que le
 * cloud travaille exactement sur les mêmes valeurs que la carte. */
typedef struct
{
  uint32_t       numero;                          /* 1, 2, 3... à chaque prédiction */
  sensors_data_t mesure;                          /* mesure utilisée                */
  uint32_t       epoch;                           /* heure UTC utilisée             */
  bool           heure_exacte;                    /* false : date de compilation    */
  int            classe[METEO_IA_NB_MODELES];     /* 0..3, ou -1 si erreur          */
  float          confiance[METEO_IA_NB_MODELES];  /* en %                           */
  uint32_t       us[METEO_IA_NB_MODELES];         /* temps de calcul (µs)           */
} prediction_t;

static prediction_t s_prediction;
static bool         s_prediction_dispo = false;
static osMutexId_t  s_prediction_mutex = NULL;

/* Démo 6 : les LEDs affichent la météo prédite (TacheIA). Les tâches qui
 * les utilisaient pour leur état (capteurs, réseau) ne doivent plus y
 * toucher : ces macros ne font alors rien. Démos 1 à 5 : LEDs d'état du TP2. */
#if (DEMO_PARTIE == 6)
#define LED_ETAT_ON(id)    ((void)(id))
#define LED_ETAT_OFF(id)   ((void)(id))
#else
#define LED_ETAT_ON(id)    led_on(id)
#define LED_ETAT_OFF(id)   led_off(id)
#endif

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void TacheChenillard(void *argument);
void TacheCapteurs(void *argument);
void TacheReseau(void *argument);
void TacheIA(void *argument);
static void log_etat(const char *etat);
static void mesures_publier(const sensors_data_t *m);
__attribute__((unused)) static bool mesures_lire(sensors_data_t *m);  /* démos 4 et 6 */
__attribute__((unused)) static void prediction_publier(const prediction_t *p);  /* démo 6 */
__attribute__((unused)) static bool prediction_lire(prediction_t *p);           /* démo 6 */
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* Mutex de la console : plusieurs tâches font des printf (console.c) */
  console_init();
  /* Mutex des boîtes aux lettres des mesures et des prédictions */
  s_mesures_mutex = osMutexNew(NULL);
  s_prediction_mutex = osMutexNew(NULL);
  /* Horloge logicielle : date de compilation en attendant un serveur */
  horloge_init();/* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* Les tâches sont seulement ENREGISTRÉES ici : elles ne démarreront
   * qu'au moment de osKernelStart() dans main.c.
   * osThreadNew renvoie NULL si le tas FreeRTOS (configTOTAL_HEAP_SIZE)
   * est trop petit pour allouer la pile : on le vérifie. */
  int ok = 1;
#if (DEMO_PARTIE == 1)
  TacheChenillardHandle = osThreadNew(TacheChenillard, NULL, &TacheChenillard_attributes);
  ok = (TacheChenillardHandle != NULL);
#else
  TacheCapteursHandle = osThreadNew(TacheCapteurs, NULL, &TacheCapteurs_attributes);
  ok = (TacheCapteursHandle != NULL);
#endif
#if (DEMO_PARTIE == 3) || (DEMO_PARTIE == 4) || (DEMO_PARTIE == 6)
  TacheReseauHandle = osThreadNew(TacheReseau, NULL, &TacheReseau_attributes);
  ok = ok && (TacheReseauHandle != NULL);
#endif
#if (DEMO_PARTIE == 5) || (DEMO_PARTIE == 6)
  TacheIAHandle = osThreadNew(TacheIA, NULL, &TacheIA_attributes);
  ok = ok && (TacheIAHandle != NULL);
#endif
  if (!ok)
  {
    printf("ERREUR : creation des taches impossible (tas FreeRTOS trop petit ?)\r\n");
  }
#if (DEMO_PARTIE == 4)
  printf("  Demo : TP3.2, envoi des mesures sur ThingSpeak\r\n");
#elif (DEMO_PARTIE == 5)
  printf("  Demo : TP3.3, reseau de neurones sur la carte\r\n");
#elif (DEMO_PARTIE == 6)
  printf("  Demo : TP3.3c, comparaison IA sur la carte / IA dans le cloud\r\n");
  printf("  LEDs : vert = Clair, vert + bleu = Nuageux, rouge = Brouillard, bleu = Precipitations\r\n");
#else
  printf("  Demo : partie %d du TP2\r\n", DEMO_PARTIE);
#endif/* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
*        Réservée à la communication réseau (étape suivante).
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
  /* Pour l'instant elle ne fait rien : elle dort 1 s en boucle.
   * (osDelay(1) généré par CubeMX la réveillait 1000 fois par seconde
   *  pour rien.) */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* Log horodaté. osKernelGetTickCount() = millisecondes depuis le démarrage
 * de FreeRTOS (équivalent RTOS de HAL_GetTick). Une seule ligne : on prend
 * la console le temps de l'écrire. */
static void log_etat(const char *etat)
{
  console_lock();
  printf("[%8lu ms] <%s>\r\n", (unsigned long)osKernelGetTickCount(), etat);
  console_unlock();
}

/* Dépose une copie des mesures dans la boîte aux lettres */
static void mesures_publier(const sensors_data_t *m)
{
  osMutexAcquire(s_mesures_mutex, osWaitForever);
  s_derniere_mesure = *m;             /* copie de toute la structure */
  s_mesure_dispo = true;
  osMutexRelease(s_mesures_mutex);
}

/* Récupère une copie des dernières mesures. false s'il n'y en a pas encore. */
static bool mesures_lire(sensors_data_t *m)
{
  bool dispo;
  osMutexAcquire(s_mesures_mutex, osWaitForever);
  dispo = s_mesure_dispo;
  if (dispo)
  {
    *m = s_derniere_mesure;
  }
  osMutexRelease(s_mesures_mutex);
  return dispo;
}

/* Dépose une copie de la dernière prédiction de la carte */
static void prediction_publier(const prediction_t *p)
{
  osMutexAcquire(s_prediction_mutex, osWaitForever);
  s_prediction = *p;
  s_prediction_dispo = true;
  osMutexRelease(s_prediction_mutex);
}

/* Récupère une copie de la dernière prédiction. false s'il n'y en a pas. */
static bool prediction_lire(prediction_t *p)
{
  bool dispo;
  osMutexAcquire(s_prediction_mutex, osWaitForever);
  dispo = s_prediction_dispo;
  if (dispo)
  {
    *p = s_prediction;
  }
  osMutexRelease(s_prediction_mutex);
  return dispo;
}

/* ========================================================================= */
/* Partie 1 : LEDs et chenillard                                             */
/* ========================================================================= */
/**
  * @brief  1.a : les trois LEDs allumées pendant 3 s.
  *         1.b : chenillard ROUGE -> VERT -> BLEU, 3 s par LED.
  *         1.c : un log à chaque état.
  */
void TacheChenillard(void *argument)
{
  static const led_id_t sequence[] = { LED_ID_RED, LED_ID_GREEN, LED_ID_BLUE };
  (void)argument;

  led_all_on();
  log_etat("TOUTES LES LEDS ALLUMEES");
  osDelay(CHENILLARD_MS);

  for (;;)
  {
    log_etat("debut de chenillard");
    for (uint32_t i = 0; i < (sizeof(sequence) / sizeof(sequence[0])); i++)
    {
      led_only(sequence[i]);
      log_etat(led_name(sequence[i]));        /* <ROUGE>, <VERT>, <BLEU> */
      osDelay(CHENILLARD_MS);
    }
  }
}

/* ========================================================================= */
/* Partie 2 : capteurs                                                       */
/* ========================================================================= */
/**
  * @brief  Lecture des capteurs toutes les 5 s (2.a) et LEDs d'état (2.b) :
  *           ROUGE = occupé, en train de lire les capteurs
  *           VERT  = disponible, en attente de la prochaine lecture
  */
void TacheCapteurs(void *argument)
{
  (void)argument;   /* paramètre non utilisé : évite un warning */

  /* --- Initialisation : exécutée UNE fois, au démarrage de la tâche --- */
  LED_ETAT_OFF(LED_ID_GREEN);
  LED_ETAT_ON(LED_ID_RED);                   /* occupé */

  console_lock();                       /* tout le bloc d'init d'un seul tenant */
  printf("[%8lu ms] <scan du bus I2C>\r\n", (unsigned long)osKernelGetTickCount());
  sensors_i2c_scan(&hi2c1);
  printf("[%8lu ms] <initialisation des capteurs>\r\n", (unsigned long)osKernelGetTickCount());
  int nb = sensors_init(&hi2c1);
  printf("  %d/3 capteurs prets\r\n", nb);
  console_unlock();

  LED_ETAT_OFF(LED_ID_RED);
  LED_ETAT_ON(LED_ID_GREEN);                 /* disponible */

  /* --- Boucle infinie : une tâche ne doit JAMAIS se terminer --- */
  for (;;)
  {
    osDelay(SENSOR_PERIOD_MS - LED_BUSY_MIN_MS);   /* VERT : en attente */

    LED_ETAT_OFF(LED_ID_GREEN);
    LED_ETAT_ON(LED_ID_RED);                 /* ROUGE : lecture en cours */
    sensors_read(&s_data);
    mesures_publier(&s_data);           /* pour TacheReseau et TacheIA */

    console_lock();                     /* le log et les 3 lignes de mesures ensemble */
    printf("[%8lu ms] <Lecture Interface Capteurs>\r\n", (unsigned long)osKernelGetTickCount());
    sensors_print(&s_data);
    console_unlock();

    /* La lecture I2C ne dure que quelques ms : on garde la LED rouge
     * 300 ms pour qu'elle soit visible. On DORT (osDelay), on n'attend pas
     * activement : les autres tâches continuent de tourner. */
    osDelay(LED_BUSY_MIN_MS);
    LED_ETAT_OFF(LED_ID_RED);
    LED_ETAT_ON(LED_ID_GREEN);               /* VERT : disponible */
  }
}

/* ========================================================================= */
/* Partie 3 : réseau (TP2 : ping) et TP3.2 : envoi sur ThingSpeak          */
/* ========================================================================= */
#if (DEMO_PARTIE == 3)
/* TP2 partie 3 : ping google.com, affiche le résultat */
static void reseau_action(void)
{
  char ip[16] = "?";
  uint32_t rtt = 0;

  /* Le ping peut prendre jusqu'à 2 s : on ne garde PAS la console pendant
   * ce temps, sinon TacheCapteurs serait bloquée pour afficher. */
  ping_resultat_t r = net_ping(PING_HOTE, PING_TIMEOUT_MS, ip, &rtt);

  console_lock();
  if (r == PING_OK)
  {
    printf("  ping %s (%s) : reponse en %lu ms\r\n", PING_HOTE, ip, (unsigned long)rtt);
  }
  else
  {
    printf("  ping %s (%s) : %s\r\n", PING_HOTE, ip, net_ping_texte(r));
  }
  console_unlock();
}
#define RESEAU_PERIODE_MS  PING_PERIOD_MS

#elif (DEMO_PARTIE == 4) || (DEMO_PARTIE == 6)
#if (DEMO_PARTIE == 6)
/* Extrait d'une réponse du cloud la classe prédite par un modèle :
 * "h0=Nuageux,64;h3=Precipitations,55", "h3" -> "Precipitations" */
static bool classe_cloud(const char *commande, const char *modele,
                         char *classe, size_t taille)
{
  char cle[8];
  snprintf(cle, sizeof(cle), "%s=", modele);
  const char *debut = strstr(commande, cle);
  if (debut == NULL)
  {
    return false;
  }
  debut += strlen(cle);
  size_t n = 0;
  while ((debut[n] != ',') && (debut[n] != ';') && (debut[n] != '\0') && (n < taille - 1U))
  {
    classe[n] = debut[n];
    n++;
  }
  classe[n] = '\0';
  return n > 0U;
}

/* Bilan depuis le démarrage, pour chaque modèle : combien de fois la carte
 * et le cloud ont donné la même classe */
static uint32_t s_nb_comparaisons[METEO_IA_NB_MODELES];
static uint32_t s_nb_accords[METEO_IA_NB_MODELES];
#endif

/* TP3.2 : envoie les dernières mesures sur le canal ThingSpeak, puis attend
 * la prédiction que MATLAB dépose dans la file TalkBack.
 *
 *   carte --mesures--> canal --(React)--> MATLAB --commande--> TalkBack
 *     ^                                                           |
 *     +--------------------- lecture toutes les 2 s --------------+
 *
 * Le temps entre l'envoi et la réception est la latence de l'inférence dans
 * le cloud (réseau + déclenchement + MATLAB), à comparer au TP3.3 avec
 * l'inférence sur la carte. Précision : la période de scrutation (2 s). */
static void reseau_action(void)
{
  sensors_data_t m;
  static char commande[128];

  if (!mesures_lire(&m) || !m.humidity_ok || !m.pressure_ok)
  {
    log_etat("ThingSpeak : pas encore de mesure valide, envoi reporte");
    return;
  }

  /* 1. Vider la file : une ancienne réponse (carte éteinte, envoi précédent
   *    sans réponse à temps...) serait prise pour la réponse à CET envoi. */
  for (int i = 0; i < 10; i++)
  {
    if (thingspeak_talkback_lire(TALKBACK_ID, TALKBACK_KEY,
                                 commande, sizeof(commande)) != TS_OK)
    {
      break;
    }
  }

#if (DEMO_PARTIE == 6)
  /* 2. Dernière prédiction de la carte, calculée par TacheIA. On envoie la
   *    mesure qu'elle a utilisée : le cloud travaille ainsi sur exactement
   *    les mêmes valeurs que la carte. */
  prediction_t p;
  bool avec_prediction = prediction_lire(&p) && (p.classe[METEO_IA_H0] >= 0)
                         && (p.classe[METEO_IA_H3] >= 0);
  if (avec_prediction)
  {
    m = p.mesure;
  }
#endif

  /* 3. Envoi des mesures. Température du SHT40 : même capteur que
   *    l'humidité, donc point de rosée cohérent. Pression brute : MATLAB la
   *    ramène au niveau de la mer. */
  int32_t numero = 0;
  uint32_t t0 = osKernelGetTickCount();
#if (DEMO_PARTIE == 6)
  /* Démo 6 : on joint les prédictions de la carte, pour la page web
   * (web/index.html) qui affiche carte et cloud côte à côte :
   *   field4 / field5 : classe h0 (0..3) et sa confiance (%)
   *   field6 / field7 : classe h3 (0..3) et sa confiance (%)
   *   field8          : temps d'inférence du modèle h0 sur la carte (µs) */
  float champs[TS_NB_CHAMPS] = { m.temp_hum_c, m.humidity_rh, m.pressure_hpa };
  uint8_t masque = 0x07U;                         /* field1..3 : les mesures */
  if (avec_prediction)
  {
    champs[3] = (float)p.classe[METEO_IA_H0];
    champs[4] = p.confiance[METEO_IA_H0];
    champs[5] = (float)p.classe[METEO_IA_H3];
    champs[6] = p.confiance[METEO_IA_H3];
    champs[7] = (float)p.us[METEO_IA_H0];
    masque = 0xFFU;                               /* field1..8 */
  }
  ts_resultat_t r = thingspeak_envoyer_champs(THINGSPEAK_WRITE_KEY, champs, masque, &numero);
#else
  ts_resultat_t r = thingspeak_envoyer(THINGSPEAK_WRITE_KEY, m.temp_hum_c,
                                       m.humidity_rh, m.pressure_hpa, &numero);
#endif
  uint32_t duree = osKernelGetTickCount() - t0;

  console_lock();
  if (r == TS_OK)
  {
    printf("  ThingSpeak : ligne %ld enregistree (%lu ms)\r\n",
           (long)numero, (unsigned long)duree);
  }
  else
  {
    printf("  ThingSpeak : %s (%lu ms)\r\n", thingspeak_texte(r), (unsigned long)duree);
  }
  console_unlock();
  if (r != TS_OK)
  {
    return;
  }

  /* 4. Attente de la prédiction du cloud */
  while ((osKernelGetTickCount() - t0) < TB_ATTENTE_MAX_MS)
  {
    osDelay(TB_SCRUTATION_MS);
    r = thingspeak_talkback_lire(TALKBACK_ID, TALKBACK_KEY, commande, sizeof(commande));
    if (r == TS_VIDE)
    {
      continue;                        /* MATLAB n'a pas encore répondu */
    }

    console_lock();
    if (r == TS_OK)
    {
      uint32_t latence = osKernelGetTickCount() - t0;
      printf("  Prediction cloud : %s (recue %lu ms apres l'envoi)\r\n",
             commande, (unsigned long)latence);
#if (DEMO_PARTIE == 6)
      for (int md = 0; md < METEO_IA_NB_MODELES; md++)
      {
        char cl_cloud[24];
        const char *nom = meteo_ia_nom((meteo_ia_modele_t)md);
        if (avec_prediction && classe_cloud(commande, nom, cl_cloud, sizeof(cl_cloud)))
        {
          bool accord = (strcmp(cl_cloud, meteo_ia_classe(p.classe[md])) == 0);
          s_nb_comparaisons[md]++;
          s_nb_accords[md] += accord ? 1U : 0U;
          printf("  => %s carte : %s en %lu us | cloud : %s en %lu ms | %s (%lu/%lu)\r\n",
                 nom, meteo_ia_classe(p.classe[md]), (unsigned long)p.us[md],
                 cl_cloud, (unsigned long)latence,
                 accord ? "ACCORD" : "DESACCORD",
                 (unsigned long)s_nb_accords[md], (unsigned long)s_nb_comparaisons[md]);
        }
      }
#endif
    }
    else
    {
      printf("  TalkBack : %s\r\n", thingspeak_texte(r));
    }
    console_unlock();
    return;
  }
  log_etat("TalkBack : pas de prediction recue en 30 s (MATLAB lance ?)");
}
#define RESEAU_PERIODE_MS  TS_PERIOD_MS
#endif

#if (DEMO_PARTIE == 3) || (DEMO_PARTIE == 4) || (DEMO_PARTIE == 6)
/**
  * @brief  Démarre LwIP, attend l'IP du DHCP, puis fait l'action réseau de
  *         la démo (ping ou ThingSpeak) à intervalle régulier.
  *         LED bleue = communication réseau en cours (TP2 3.d).
  */
void TacheReseau(void *argument)
{
  (void)argument;

  reseau_init();                       /* LwIP + Ethernet + DHCP (app_ethernet.c) */

  if (!reseau_attendre_ip(30000))      /* 30 s pour obtenir une IP */
  {
    log_etat("Communication Reseau : pas d'IP pour l'instant, on reessaiera");
  }

  for (;;)
  {
    if (reseau_est_pret())
    {
      LED_ETAT_ON(LED_ID_BLUE);                    /* BLEU : communication réseau */
      log_etat("Communication Reseau");
      reseau_action();
      LED_ETAT_OFF(LED_ID_BLUE);
    }
    else
    {
      log_etat("Communication Reseau : reseau pas pret (cable ? DHCP ?)");
    }

    osDelay(RESEAU_PERIODE_MS);
  }
}
#endif

/* ========================================================================= */
/* TP3.3 : réseau de neurones sur la carte                                    */
/* ========================================================================= */
#if (DEMO_PARTIE == 6)
/* Écrit v avec une décimale ("1012.3"), sans printf flottant */
static void fx1(float v, char *buf, size_t len)
{
  int32_t d = (int32_t)(v * 10.0f + ((v >= 0.0f) ? 0.5f : -0.5f));
  snprintf(buf, len, "%s%ld.%01ld", (d < 0) ? "-" : "",
           (long)(((d < 0) ? -d : d) / 10), (long)(((d < 0) ? -d : d) % 10));
}

/* LEDs météo : la prédiction "maintenant" (h0) de la carte.
 *   Clair = vert, Nuageux = vert + bleu, Brouillard = rouge,
 *   Précipitations = bleu. Toutes éteintes si pas de prédiction. */
static void leds_meteo(int classe)
{
  led_all_off();
  switch (classe)
  {
    case 0: led_on(LED_ID_GREEN);                       break;
    case 1: led_on(LED_ID_GREEN); led_on(LED_ID_BLUE);  break;
    case 2: led_on(LED_ID_RED);                         break;
    case 3: led_on(LED_ID_BLUE);                        break;
    default:                                            break;
  }
}

/* Une prédiction complète sur la carte : mesure -> 8 entrées -> h0 et h3.
 * Renvoie false s'il n'y a pas encore de mesure valide. */
static bool predire_sur_la_carte(void)
{
  static uint32_t numero = 0U;
  prediction_t p;
  float x[METEO_IA_NB_ENTREES];

  if (!mesures_lire(&p.mesure) || !p.mesure.humidity_ok || !p.mesure.pressure_ok)
  {
    return false;
  }

  /* Heure : exacte si un serveur a répondu, sinon date de compilation */
  p.heure_exacte = horloge_lire(&p.epoch);
  meteo_ia_calculer_entrees(p.mesure.temp_hum_c, p.mesure.humidity_rh,
                            p.mesure.pressure_hpa, p.epoch, x);

  for (int md = 0; md < METEO_IA_NB_MODELES; md++)
  {
    float proba[METEO_IA_NB_CLASSES];
    uint32_t cycles = 0U;
    p.classe[md] = meteo_ia_predire((meteo_ia_modele_t)md, x, proba, &cycles);
    p.confiance[md] = (p.classe[md] >= 0) ? proba[p.classe[md]] * 100.0f : 0.0f;
    p.us[md] = meteo_ia_cycles_en_us(cycles);
  }
  p.numero = ++numero;

  leds_meteo(p.classe[METEO_IA_H0]);       /* la météo prédite, sur la carte */
  prediction_publier(&p);                  /* pour TacheReseau               */

  char pmer[16], ecart[16];
  fx1(x[METEO_IA_E_PRES], pmer, sizeof(pmer));
  fx1(x[METEO_IA_E_ECART_ROSEE], ecart, sizeof(ecart));
  console_lock();
  printf("[%8lu ms] <IA carte n%lu> heure UTC %02lu:%02lu (%s), P mer %s hPa, ecart rosee %s\r\n",
         (unsigned long)osKernelGetTickCount(), (unsigned long)p.numero,
         (unsigned long)((p.epoch % 86400U) / 3600U), (unsigned long)((p.epoch % 3600U) / 60U),
         p.heure_exacte ? "serveur" : "approx., date de compilation", pmer, ecart);
  for (int md = 0; md < METEO_IA_NB_MODELES; md++)
  {
    printf("  Prediction carte : %s=%s (%lu %%) en %lu us\r\n",
           meteo_ia_nom((meteo_ia_modele_t)md), meteo_ia_classe(p.classe[md]),
           (unsigned long)(p.confiance[md] + 0.5f), (unsigned long)p.us[md]);
  }
  console_unlock();
  return true;
}
#endif

#if (DEMO_PARTIE == 5) || (DEMO_PARTIE == 6)
/**
  * @brief  Initialise les réseaux générés par X-CUBE-AI et vérifie qu'ils
  *         donnent les mêmes réponses que Python (vecteurs de test).
  *         Démo 6 : prédit ensuite toutes les 20 s, que le réseau Ethernet
  *         fonctionne ou non, et affiche la météo prédite sur les LEDs.
  */
void TacheIA(void *argument)
{
  (void)argument;

  if (!meteo_ia_init())
  {
    log_etat("IA : initialisation des reseaux impossible");
    for (;;)
    {
      osDelay(1000);
    }
  }
  log_etat("IA : reseaux meteo_h0 et meteo_h3 prets (X-CUBE-AI, Cortex-M55)");
  meteo_ia_autotest(METEO_IA_H0);
  meteo_ia_autotest(METEO_IA_H3);

  for (;;)
  {
#if (DEMO_PARTIE == 6)
    /* Pas encore de mesure (capteurs en démarrage) : on réessaie dans 1 s */
    osDelay(predire_sur_la_carte() ? IA_PERIODE_MS : 1000U);
#else
    osDelay(1000);
#endif
  }
}
#endif

/* USER CODE END Application */

