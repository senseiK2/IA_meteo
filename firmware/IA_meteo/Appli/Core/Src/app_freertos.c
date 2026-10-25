/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Tâches FreeRTOS de l'application (TP2).
  *
  *   DEMO_PARTIE choisit ce qui tourne, pour pouvoir montrer chaque partie
  *   du TP en démonstration en changeant une seule ligne :
  *     1 -> TacheChenillard : 3 LEDs allumées 3 s, puis chenillard R/V/B (3 s)
  *     2 -> TacheCapteurs   : lecture des capteurs, LED rouge/verte
  *     3 -> TacheCapteurs + TacheReseau : + LwIP, ping google.com, LED bleue
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
#include "main.h"
#include "led.h"
#include "sensors.h"
#include "app_ethernet.h"
#include "net_ping.h"
#include "console.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ===== Choix de la démo (voir l'en-tête du fichier) ===== */
#define DEMO_PARTIE        3

#define CHENILLARD_MS      3000U   /* partie 1 : temporisation imposée de 3 s     */
#define SENSOR_PERIOD_MS   5000U   /* partie 2 : une lecture des capteurs / 5 s   */
#define LED_BUSY_MIN_MS    300U    /* LED rouge allumée au moins 300 ms (visible) */
#define PING_HOTE          "google.com"
#define PING_PERIOD_MS     10000U  /* partie 3 : un ping toutes les 10 s          */
#define PING_TIMEOUT_MS    2000U   /* attente maximum de la réponse               */

#if (DEMO_PARTIE < 1) || (DEMO_PARTIE > 3)
#error "DEMO_PARTIE doit valoir 1, 2 ou 3"
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

static sensors_data_t s_data;

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
static void log_etat(const char *etat);
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
  console_init();/* USER CODE END RTOS_MUTEX */

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
#if (DEMO_PARTIE == 3)
  TacheReseauHandle = osThreadNew(TacheReseau, NULL, &TacheReseau_attributes);
  ok = ok && (TacheReseauHandle != NULL);
#endif
  if (!ok)
  {
    printf("ERREUR : creation des taches impossible (tas FreeRTOS trop petit ?)\r\n");
  }
  printf("  Demo : partie %d du TP2\r\n", DEMO_PARTIE);/* USER CODE END RTOS_THREADS */

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
  led_off(LED_ID_GREEN);
  led_on(LED_ID_RED);                   /* occupé */

  console_lock();                       /* tout le bloc d'init d'un seul tenant */
  printf("[%8lu ms] <scan du bus I2C>\r\n", (unsigned long)osKernelGetTickCount());
  sensors_i2c_scan(&hi2c1);
  printf("[%8lu ms] <initialisation des capteurs>\r\n", (unsigned long)osKernelGetTickCount());
  int nb = sensors_init(&hi2c1);
  printf("  %d/3 capteurs prets\r\n", nb);
  console_unlock();

  led_off(LED_ID_RED);
  led_on(LED_ID_GREEN);                 /* disponible */

  /* --- Boucle infinie : une tâche ne doit JAMAIS se terminer --- */
  for (;;)
  {
    osDelay(SENSOR_PERIOD_MS - LED_BUSY_MIN_MS);   /* VERT : en attente */

    led_off(LED_ID_GREEN);
    led_on(LED_ID_RED);                 /* ROUGE : lecture en cours */
    sensors_read(&s_data);

    console_lock();                     /* le log et les 3 lignes de mesures ensemble */
    printf("[%8lu ms] <Lecture Interface Capteurs>\r\n", (unsigned long)osKernelGetTickCount());
    sensors_print(&s_data);
    console_unlock();

    /* La lecture I2C ne dure que quelques ms : on garde la LED rouge
     * 300 ms pour qu'elle soit visible. On DORT (osDelay), on n'attend pas
     * activement : les autres tâches continuent de tourner. */
    osDelay(LED_BUSY_MIN_MS);
    led_off(LED_ID_RED);
    led_on(LED_ID_GREEN);               /* VERT : disponible */
  }
}

/* ========================================================================= */
/* Partie 3 : réseau                                                         */
/* ========================================================================= */
/**
  * @brief  Démarre LwIP, attend l'IP du DHCP, puis ping google.com toutes
  *         les 10 s. LED bleue = communication réseau en cours (3.d).
  */
void TacheReseau(void *argument)
{
  (void)argument;

  reseau_init();                       /* LwIP + Ethernet + DHCP (app_ethernet.c) */

  if (!reseau_attendre_ip(30000))      /* 30 s pour que la box donne une IP */
  {
    log_etat("Communication Reseau : pas d'IP pour l'instant, on reessaiera");
  }

  for (;;)
  {
    if (reseau_est_pret())
    {
      char ip[16] = "?";
      uint32_t rtt = 0;

      led_on(LED_ID_BLUE);                         /* BLEU : communication réseau */
      log_etat("Communication Reseau");

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
      led_off(LED_ID_BLUE);
    }
    else
    {
      log_etat("Communication Reseau : reseau pas pret (cable ? DHCP ?)");
    }

    osDelay(PING_PERIOD_MS);
  }
}

/* USER CODE END Application */

