/**
 * @file    led.h
 * @brief   Gestion des 3 LEDs utilisateur de la NUCLEO-N657X0-Q.
 *
 * Brochage (source : BSP ST stm32n6xx_nucleo.h) :
 *   - LED rouge : PG10   (label CubeMX : LED_RED)
 *   - LED verte : PG0    (label CubeMX : LED_GREEN)
 *   - LED bleue : PG8    (label CubeMX : LED_BLUE)
 *
 * ATTENTION : sur cette carte, les LEDs sont ACTIVES À L'ÉTAT BAS
 * (GPIO_PIN_RESET = allumée, GPIO_PIN_SET = éteinte).
 *
 * À placer dans : FSBL/Core/Inc/led.h
 */
#ifndef LED_H
#define LED_H

#include "main.h"

/* Valeurs par défaut si les labels n'ont pas été définis dans CubeMX */
#ifndef LED_RED_Pin
#define LED_RED_Pin          GPIO_PIN_10
#define LED_RED_GPIO_Port    GPIOG
#endif
#ifndef LED_GREEN_Pin
#define LED_GREEN_Pin        GPIO_PIN_0
#define LED_GREEN_GPIO_Port  GPIOG
#endif
#ifndef LED_BLUE_Pin
#define LED_BLUE_Pin         GPIO_PIN_8
#define LED_BLUE_GPIO_Port   GPIOG
#endif

typedef enum
{
  LED_ID_RED = 0,
  LED_ID_GREEN,
  LED_ID_BLUE,
  LED_ID_COUNT
} led_id_t;

void        led_on(led_id_t id);
void        led_off(led_id_t id);
void        led_toggle(led_id_t id);
void        led_all_on(void);
void        led_all_off(void);
void        led_only(led_id_t id);   /* allume une seule LED, éteint les autres */
const char *led_name(led_id_t id);   /* "ROUGE", "VERT", "BLEU" pour les logs   */

#endif /* LED_H */
