/**
 * @file    led.c
 * @brief   Gestion des 3 LEDs utilisateur de la NUCLEO-N657X0-Q.
 *
 * À placer dans : FSBL/Core/Src/led.c
 */
#include "led.h"

/* LEDs actives à l'état bas sur la NUCLEO-N657X0-Q.
 * Si tes LEDs se comportent à l'envers, inverse ces deux lignes. */
#define LED_LEVEL_ON   GPIO_PIN_RESET
#define LED_LEVEL_OFF  GPIO_PIN_SET

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t      pin;
  const char   *name;
} led_hw_t;

/* Les macros LED_xxx_GPIO_Port / LED_xxx_Pin sont générées par CubeMX
 * dans main.h à partir des labels donnés aux broches. */
static const led_hw_t s_leds[LED_ID_COUNT] =
{
  [LED_ID_RED]   = { LED_RED_GPIO_Port,   LED_RED_Pin,   "ROUGE" },
  [LED_ID_GREEN] = { LED_GREEN_GPIO_Port, LED_GREEN_Pin, "VERT"  },
  [LED_ID_BLUE]  = { LED_BLUE_GPIO_Port,  LED_BLUE_Pin,  "BLEU"  },
};

void led_on(led_id_t id)
{
  if (id < LED_ID_COUNT)
  {
    HAL_GPIO_WritePin(s_leds[id].port, s_leds[id].pin, LED_LEVEL_ON);
  }
}

void led_off(led_id_t id)
{
  if (id < LED_ID_COUNT)
  {
    HAL_GPIO_WritePin(s_leds[id].port, s_leds[id].pin, LED_LEVEL_OFF);
  }
}

void led_toggle(led_id_t id)
{
  if (id < LED_ID_COUNT)
  {
    HAL_GPIO_TogglePin(s_leds[id].port, s_leds[id].pin);
  }
}

void led_all_on(void)
{
  for (int i = 0; i < LED_ID_COUNT; i++)
  {
    led_on((led_id_t)i);
  }
}

void led_all_off(void)
{
  for (int i = 0; i < LED_ID_COUNT; i++)
  {
    led_off((led_id_t)i);
  }
}

void led_only(led_id_t id)
{
  led_all_off();
  led_on(id);
}

const char *led_name(led_id_t id)
{
  return (id < LED_ID_COUNT) ? s_leds[id].name : "?";
}
