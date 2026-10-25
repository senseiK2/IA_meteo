/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  *                      TP2 partie 3 - étape 1 : deux tâches
  *                        - TacheCapteurs : lecture des capteurs toutes les 5 s
  *                        - TacheLed      : LED verte qui clignote (signe de vie)
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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SENSOR_PERIOD_MS   5000U   /* une lecture des capteurs toutes les 5 s   */
#define LED_BUSY_MIN_MS    300U    /* LED rouge allumée au moins 300 ms (visible) */
#define LED_BLINK_MS       500U    /* demi-période du clignotement de la LED verte */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* hi2c1 est créé par CubeMX dans main.c : "extern" dit au compilateur
 * qu'il existe ailleurs, sans le recréer. */
extern I2C_HandleTypeDef hi2c1;

/* --- TacheCapteurs ---------------------------------------------------------
 * Priorité Normal : c'est elle qui fait le travail utile.
 * Pile 1024 mots = 4096 octets : printf + snprintf consomment beaucoup de pile.
 * ATTENTION : en CMSIS-RTOS v2, .stack_size est en OCTETS (d'où le "* 4"). */
osThreadId_t TacheCapteursHandle;
const osThreadAttr_t TacheCapteurs_attributes = {
  .name       = "TacheCapteurs",
  .priority   = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

/* --- TacheLed --------------------------------------------------------------
 * Priorité Low : elle ne fait presque rien, elle passe après les autres.
 * Pile 256 mots = 1024 octets : suffisant, elle n'appelle pas printf. */
osThreadId_t TacheLedHandle;
const osThreadAttr_t TacheLed_attributes = {
  .name       = "TacheLed",
  .priority   = (osPriority_t) osPriorityLow,
  .stack_size = 256 * 4
};

/* --- TacheReseau ------------------------------------------------------------
 * Démarre LwIP, attend l'adresse IP du DHCP, puis (à compléter) ping et
 * LED bleue. Pile 1024 mots = 4096 octets : printf + appels LwIP. */
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
void TacheCapteurs(void *argument);
void TacheLed(void *argument);
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
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

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
  TacheCapteursHandle = osThreadNew(TacheCapteurs, NULL, &TacheCapteurs_attributes);
  TacheLedHandle      = osThreadNew(TacheLed,      NULL, &TacheLed_attributes);
  TacheReseauHandle   = osThreadNew(TacheReseau,   NULL, &TacheReseau_attributes);

  if ((TacheCapteursHandle == NULL) || (TacheLedHandle == NULL) || (TacheReseauHandle == NULL))
  {
    printf("ERREUR : creation des taches impossible (tas FreeRTOS trop petit ?)\r\n");
  }
  /* USER CODE END RTOS_THREADS */

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
 * de FreeRTOS (équivalent RTOS de HAL_GetTick). */
static void log_etat(const char *etat)
{
  printf("[%8lu ms] <%s>\r\n", (unsigned long)osKernelGetTickCount(), etat);
}

/**
  * @brief  Tâche de lecture des capteurs.
  *         LED rouge = occupé (lecture en cours).
  */
void TacheCapteurs(void *argument)
{
  (void)argument;   /* paramètre non utilisé : évite un warning */

  /* --- Initialisation : exécutée UNE fois, au démarrage de la tâche --- */
  led_on(LED_ID_RED);
  log_etat("scan du bus I2C");
  sensors_i2c_scan(&hi2c1);

  log_etat("initialisation des capteurs");
  int nb = sensors_init(&hi2c1);
  printf("  %d/3 capteurs prets\r\n", nb);
  led_off(LED_ID_RED);

  /* --- Boucle infinie : une tâche ne doit JAMAIS se terminer --- */
  for (;;)
  {
    led_on(LED_ID_RED);                 /* occupé */
    log_etat("Lecture Interface Capteurs");
    sensors_read(&s_data);
    sensors_print(&s_data);

    /* Avant, on faisait une attente active (while ... HAL_GetTick) pour que
     * la LED rouge reste visible. Avec FreeRTOS on DORT : pendant ces 300 ms,
     * TacheLed peut continuer à faire clignoter la LED verte. */
    osDelay(LED_BUSY_MIN_MS);
    led_off(LED_ID_RED);                /* lecture terminée */

    /* La tâche dort jusqu'à la prochaine lecture : le CPU est libre. */
    osDelay(SENSOR_PERIOD_MS - LED_BUSY_MIN_MS);
  }
}

/**
  * @brief  Tâche "signe de vie" : la LED verte clignote tant que
  *         l'ordonnanceur tourne. Si elle s'arrête, le système est bloqué.
  */
void TacheLed(void *argument)
{
  (void)argument;

  for (;;)
  {
    led_toggle(LED_ID_GREEN);
    osDelay(LED_BLINK_MS);
  }
}

/**
  * @brief  Tâche réseau (TP2 partie 3).
  *         Déjà fait : démarrage de LwIP, DHCP, affichage de l'adresse IP.
  *         À toi de compléter la boucle : ping google.com + LED bleue.
  */
void TacheReseau(void *argument)
{
  (void)argument;

  reseau_init();                       /* LwIP + Ethernet + DHCP (app_ethernet.c) */

  if (!reseau_attendre_ip(30000))      /* 30 s pour que la box donne une IP */
  {
    printf("  [NET] Pas d'IP : la tache reseau continue d'attendre\r\n");
  }

  for (;;)
  {
    /* TODO (exercice) :
     *  1. log_etat("Communication Reseau");
     *  2. allumer la LED bleue pendant l'échange réseau
     *  3. résoudre "google.com" (DNS) puis envoyer un ping (ICMP)
     *  4. éteindre la LED bleue, afficher le résultat
     * Pour l'instant, la carte répond déjà aux pings envoyés depuis ton PC. */
    osDelay(10000);
  }
}

/* USER CODE END Application */

