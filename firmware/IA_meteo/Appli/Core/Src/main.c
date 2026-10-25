/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Application (contexte Appli) : LEDs, capteurs I2C
  *                   NUCLEO-N657X0-Q + X-NUCLEO-IKS4A1 / IKS01A3
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "led.h"
#include "sensors.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Choix de la démo : 1 = chenillard (partie 1), 2 = capteurs (partie 2) */
#define TP_PARTIE             2

/* Tant que I2C1 n'est pas activé dans CubeMX, on reste sur la partie 1 */
#if (TP_PARTIE == 2) && !defined(HAL_I2C_MODULE_ENABLED)
#warning "I2C1 non active dans le .ioc : demo de la partie 1"
#undef  TP_PARTIE
#define TP_PARTIE             1
#endif

#define CHENILLARD_DELAY_MS   1000U   /* le sujet impose 3 s (3000U) */
#define SENSOR_PERIOD_MS      5000U   /* partie 2 : une lecture toutes les 5 s */
#define LED_BUSY_MIN_MS       300U    /* durée mini de la LED rouge, pour qu'elle
                                         soit visible (une lecture I2C dure
                                         quelques ms seulement) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#if (TP_PARTIE == 1)
static const led_id_t s_sequence[] = { LED_ID_RED, LED_ID_GREEN, LED_ID_BLUE };
#define SEQUENCE_LEN  (sizeof(s_sequence) / sizeof(s_sequence[0]))
#else
static sensors_data_t s_data;
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void log_state(const char *state);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Redirection de printf vers l'USART1 (Virtual COM Port du ST-LINK).
 * syscalls.c (généré par CubeIDE) appelle __io_putchar pour chaque caractère. */
#if defined(HAL_UART_MODULE_ENABLED)
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
#else
/* USART1 pas encore activée dans le .ioc : printf ne sort nulle part */
int __io_putchar(int ch)
{
  return ch;
}
#endif

/* Log horodaté : "[    3012 ms] <ROUGE>" */
static void log_state(const char *state)
{
  printf("[%8lu ms] <%s>\r\n", (unsigned long)HAL_GetTick(), state);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* USER CODE BEGIN 2 */

  /* printf sans buffer : chaque caractère part immédiatement sur l'UART */
  setvbuf(stdout, NULL, _IONBF, 0);

  log_state("debut d'application");

#if (TP_PARTIE == 1)
  /* 1.a : allumer les trois LEDs pendant 3 s */
  led_all_on();
  log_state("TOUTES LES LEDS ALLUMEES");
  HAL_Delay(CHENILLARD_DELAY_MS);
  led_all_off();
#else
  /* LED rouge = occupé pendant l'initialisation */
  led_only(LED_ID_RED);
  log_state("scan du bus I2C");
  sensors_i2c_scan(&hi2c1);

  log_state("initialisation des capteurs");
  int nb = sensors_init(&hi2c1);
  printf("  %d/3 capteurs prets\r\n", nb);

  /* LED verte = disponible */
  led_only(LED_ID_GREEN);
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

#if (TP_PARTIE == 1)
    /* 1.b + 1.c : chenillard ROUGE -> VERT -> BLEU avec logs */
    log_state("debut de chenillard");
    for (uint32_t i = 0; i < SEQUENCE_LEN; i++)
    {
      led_only(s_sequence[i]);
      log_state(led_name(s_sequence[i]));
      HAL_Delay(CHENILLARD_DELAY_MS);
    }
#else
    /* ROUGE : occupé, lecture des capteurs */
    uint32_t t_start = HAL_GetTick();
    led_only(LED_ID_RED);
    log_state("lecture capteurs");
    sensors_read(&s_data);
    sensors_print(&s_data);
    while ((HAL_GetTick() - t_start) < LED_BUSY_MIN_MS)
    {
    }

    /* VERT : disponible, en attente de la prochaine lecture */
    led_only(LED_ID_GREEN);
    log_state("attente");
    HAL_Delay(SENSOR_PERIOD_MS);
#endif
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
