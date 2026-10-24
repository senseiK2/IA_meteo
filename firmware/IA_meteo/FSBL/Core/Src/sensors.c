/**
 * @file    sensors.c
 * @brief   Lecture des capteurs météo de la X-NUCLEO-IKS01A3 via I2C1.
 *
 * Organisation :
 *   1. Couche "plateforme" : platform_read / platform_write / platform_delay
 *      -> seules fonctions dépendantes du MCU (HAL I2C STM32N6).
 *   2. Drivers ST (xxx_reg.c) : accèdent aux registres via stmdev_ctx_t,
 *      sans rien savoir du MCU.
 *   3. API haut niveau : sensors_init / sensors_read / sensors_print.
 *
 * Gestion des adresses 7/8 bits :
 *   Les datasheets donnent l'adresse 7 bits (ex. HTS221 = 0x5F).
 *   La HAL STM32 attend l'adresse décalée d'un bit à gauche (0x5F << 1 = 0xBE)
 *   et positionne elle-même le bit R/W. On stocke donc l'adresse 7 bits
 *   et on fait le décalage une seule fois, à l'initialisation.
 *
 * Affichage : les float sont imprimés en "entier.décimales" (fmt_fixed) pour
 *   ne pas dépendre de l'option "Use float with printf" de newlib-nano.
 */
#include "sensors.h"

#if defined(HAL_I2C_MODULE_ENABLED)

#include <stdio.h>

#include "hts221_reg.h"
#include "lps22hh_reg.h"
#include "stts751_reg.h"

/* ------------------------------------------------------------------------- */
/* Paramètres                                                                */
/* ------------------------------------------------------------------------- */
#define I2C_TIMEOUT_MS        100U

/* Adresses 7 bits sur la X-NUCLEO-IKS01A3 */
#define HTS221_ADDR_7B        0x5FU
#define LPS22HH_ADDR_7B       0x5DU   /* SA0 = 1 sur l'IKS01A3 */

/* L'adresse du STTS751 dépend de sa résistance de pull-up sur ADDR/THERM
 * et de sa version (0xxxx / 1xxxx) : 8 adresses possibles (stts751_reg.h,
 * qui les donne en 8 bits). Sur l'IKS01A3 c'est normalement 0x4A.
 * On teste les candidats au démarrage. */
static const uint8_t STTS751_ADDR_CANDIDATES[] =
{
  0x4AU, 0x48U, 0x49U, 0x4BU, 0x38U, 0x39U, 0x3AU, 0x3BU
};

/* ------------------------------------------------------------------------- */
/* Contexte bus d'un capteur (passé aux drivers via ctx.handle)              */
/* ------------------------------------------------------------------------- */
typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint16_t           addr8;          /* adresse 7 bits << 1 (format HAL)      */
  uint8_t            reg_autoinc;    /* 0x80 pour le HTS221 (multi-octets), 0 sinon */
} i2c_dev_t;

typedef struct
{
  float_t x0, y0, x1, y1;
} lin_t;

static i2c_dev_t    s_hts221_bus;
static i2c_dev_t    s_lps22hh_bus;
static i2c_dev_t    s_stts751_bus;

static stmdev_ctx_t s_hts221_ctx;
static stmdev_ctx_t s_lps22hh_ctx;
static stmdev_ctx_t s_stts751_ctx;

static lin_t        s_hts221_hum_cal;
static lin_t        s_hts221_temp_cal;

static bool         s_hts221_ok;
static bool         s_lps22hh_ok;
static bool         s_stts751_ok;

/* ------------------------------------------------------------------------- */
/* 1. Couche plateforme (implémentation des fonctions bas niveau)           */
/* ------------------------------------------------------------------------- */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len)
{
  i2c_dev_t *dev = (i2c_dev_t *)handle;
  reg |= dev->reg_autoinc;

  if (HAL_I2C_Mem_Write(dev->hi2c, dev->addr8, reg, I2C_MEMADD_SIZE_8BIT,
                        (uint8_t *)bufp, len, I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0;
  }
  return -1;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  i2c_dev_t *dev = (i2c_dev_t *)handle;
  reg |= dev->reg_autoinc;

  if (HAL_I2C_Mem_Read(dev->hi2c, dev->addr8, reg, I2C_MEMADD_SIZE_8BIT,
                       bufp, len, I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0;
  }
  return -1;
}

static void platform_delay(uint32_t ms)
{
  HAL_Delay(ms);
}

static void ctx_setup(stmdev_ctx_t *ctx, i2c_dev_t *bus,
                      I2C_HandleTypeDef *hi2c, uint8_t addr7, uint8_t autoinc)
{
  bus->hi2c        = hi2c;
  bus->addr8       = (uint16_t)(addr7 << 1);
  bus->reg_autoinc = autoinc;

  ctx->write_reg = platform_write;
  ctx->read_reg  = platform_read;
  ctx->mdelay    = platform_delay;
  ctx->handle    = bus;
}

/* Interpolation linéaire avec les coefficients de calibration du HTS221 */
static float_t linear_interpolation(const lin_t *lin, int16_t x)
{
  return ((lin->y1 - lin->y0) * x + ((lin->x1 * lin->y0) - (lin->x0 * lin->y1)))
         / (lin->x1 - lin->x0);
}

/* ------------------------------------------------------------------------- */
/* Scan du bus I2C                                                           */
/* ------------------------------------------------------------------------- */
void sensors_i2c_scan(I2C_HandleTypeDef *hi2c)
{
  int found = 0;

  printf("  Scan I2C1 (adresses 7 bits) :\r\n");
  for (uint8_t addr = 1; addr < 128; addr++)
  {
    if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 2, 10) == HAL_OK)
    {
      const char *name = "?";
      switch (addr)
      {
        case 0x5F: name = "HTS221 (humidite/temperature)"; break;
        case 0x5D: name = "LPS22HH (pression)";            break;
        case 0x48: case 0x49: case 0x4A: case 0x4B:
        case 0x38: case 0x39: case 0x3A: case 0x3B:
                   name = "STTS751 (temperature)";         break;
        case 0x6B: name = "LSM6DSO (accelero/gyro)";       break;
        case 0x1E: name = "LIS2MDL (magnetometre)";        break;
        case 0x19: name = "LIS2DW12 (accelerometre)";      break;
        default: break;
      }
      printf("    0x%02X  %s\r\n", addr, name);
      found++;
    }
  }
  printf("  %d peripherique(s) trouve(s)\r\n", found);
}

/* ------------------------------------------------------------------------- */
/* Initialisation de chaque capteur                                          */
/* ------------------------------------------------------------------------- */
static bool hts221_setup(I2C_HandleTypeDef *hi2c)
{
  uint8_t whoami = 0;

  /* Le HTS221 exige le bit 7 de l'adresse registre pour l'auto-incrément */
  ctx_setup(&s_hts221_ctx, &s_hts221_bus, hi2c, HTS221_ADDR_7B, 0x80U);

  if ((hts221_device_id_get(&s_hts221_ctx, &whoami) != 0) || (whoami != HTS221_ID))
  {
    printf("  HTS221  : ABSENT (WHO_AM_I = 0x%02X, attendu 0x%02X)\r\n", whoami, HTS221_ID);
    return false;
  }

  /* Coefficients de calibration usine (stockés dans le capteur) */
  hts221_hum_adc_point_0_get(&s_hts221_ctx, &s_hts221_hum_cal.x0);
  hts221_hum_rh_point_0_get(&s_hts221_ctx,  &s_hts221_hum_cal.y0);
  hts221_hum_adc_point_1_get(&s_hts221_ctx, &s_hts221_hum_cal.x1);
  hts221_hum_rh_point_1_get(&s_hts221_ctx,  &s_hts221_hum_cal.y1);

  hts221_temp_adc_point_0_get(&s_hts221_ctx, &s_hts221_temp_cal.x0);
  hts221_temp_deg_point_0_get(&s_hts221_ctx, &s_hts221_temp_cal.y0);
  hts221_temp_adc_point_1_get(&s_hts221_ctx, &s_hts221_temp_cal.x1);
  hts221_temp_deg_point_1_get(&s_hts221_ctx, &s_hts221_temp_cal.y1);

  hts221_block_data_update_set(&s_hts221_ctx, PROPERTY_ENABLE);
  hts221_data_rate_set(&s_hts221_ctx, HTS221_ODR_1Hz);
  hts221_power_on_set(&s_hts221_ctx, PROPERTY_ENABLE);

  printf("  HTS221  : OK (WHO_AM_I = 0x%02X)\r\n", whoami);
  return true;
}

static bool lps22hh_setup(I2C_HandleTypeDef *hi2c)
{
  uint8_t whoami = 0;
  uint8_t rst    = 1;
  uint32_t t0;

  /* Le LPS22HH auto-incrémente par défaut (bit IF_ADD_INC) : pas de bit à ajouter */
  ctx_setup(&s_lps22hh_ctx, &s_lps22hh_bus, hi2c, LPS22HH_ADDR_7B, 0x00U);

  HAL_Delay(5); /* temps de boot du capteur */

  if ((lps22hh_device_id_get(&s_lps22hh_ctx, &whoami) != 0) || (whoami != LPS22HH_ID))
  {
    printf("  LPS22HH : ABSENT (WHO_AM_I = 0x%02X, attendu 0x%02X)\r\n", whoami, LPS22HH_ID);
    return false;
  }

  /* Remise à la configuration par défaut, avec timeout */
  lps22hh_reset_set(&s_lps22hh_ctx, PROPERTY_ENABLE);
  t0 = HAL_GetTick();
  do
  {
    lps22hh_reset_get(&s_lps22hh_ctx, &rst);
  } while (rst && ((HAL_GetTick() - t0) < 100U));

  lps22hh_block_data_update_set(&s_lps22hh_ctx, PROPERTY_ENABLE);
  lps22hh_data_rate_set(&s_lps22hh_ctx, LPS22HH_10_Hz_LOW_NOISE);

  printf("  LPS22HH : OK (WHO_AM_I = 0x%02X)\r\n", whoami);
  return true;
}

static bool stts751_setup(I2C_HandleTypeDef *hi2c)
{
  stts751_id_t id = {0};

  for (uint32_t i = 0; i < sizeof(STTS751_ADDR_CANDIDATES); i++)
  {
    uint8_t addr7 = STTS751_ADDR_CANDIDATES[i];

    if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr7 << 1), 2, 10) != HAL_OK)
    {
      continue;
    }

    ctx_setup(&s_stts751_ctx, &s_stts751_bus, hi2c, addr7, 0x00U);

    if ((stts751_device_id_get(&s_stts751_ctx, &id) == 0) &&
        (id.manufacturer_id == STTS751_ID_MAN))
    {
      stts751_temp_data_rate_set(&s_stts751_ctx, STTS751_TEMP_ODR_1Hz);
      stts751_resolution_set(&s_stts751_ctx, STTS751_11bit);

      printf("  STTS751 : OK (adresse 0x%02X, product 0x%02X, manuf 0x%02X)\r\n",
             addr7, id.product_id, id.manufacturer_id);
      return true;
    }
  }

  printf("  STTS751 : ABSENT\r\n");
  return false;
}

int sensors_init(I2C_HandleTypeDef *hi2c)
{
  s_hts221_ok  = hts221_setup(hi2c);
  s_lps22hh_ok = lps22hh_setup(hi2c);
  s_stts751_ok = stts751_setup(hi2c);

  int nb = (int)s_hts221_ok + (int)s_lps22hh_ok + (int)s_stts751_ok;

  /* HTS221 et STTS751 tournent à 1 Hz : on attend la première conversion,
   * sinon la première lecture renvoie des registres encore à zéro. */
  if (nb > 0)
  {
    HAL_Delay(1100);
  }
  return nb;
}

/* ------------------------------------------------------------------------- */
/* Lecture                                                                   */
/* ------------------------------------------------------------------------- */
void sensors_read(sensors_data_t *data)
{
  int16_t  raw16;
  uint32_t raw32;

  data->hts221_ok  = false;
  data->lps22hh_ok = false;
  data->stts751_ok = false;

  /* HTS221 : humidité + température (valeurs brutes -> calibration) */
  if (s_hts221_ok)
  {
    bool ok = true;

    ok &= (hts221_humidity_raw_get(&s_hts221_ctx, &raw16) == 0);
    float_t hum = linear_interpolation(&s_hts221_hum_cal, raw16);
    if (hum < 0.0f)   { hum = 0.0f; }
    if (hum > 100.0f) { hum = 100.0f; }
    data->humidity_rh = hum;

    ok &= (hts221_temperature_raw_get(&s_hts221_ctx, &raw16) == 0);
    data->temp_hts221_c = linear_interpolation(&s_hts221_temp_cal, raw16);

    data->hts221_ok = ok;
  }

  /* LPS22HH : pression + température */
  if (s_lps22hh_ok)
  {
    bool ok = true;

    ok &= (lps22hh_pressure_raw_get(&s_lps22hh_ctx, &raw32) == 0);
    data->pressure_hpa = lps22hh_from_lsb_to_hpa(raw32);

    ok &= (lps22hh_temperature_raw_get(&s_lps22hh_ctx, &raw16) == 0);
    data->temp_lps22hh_c = lps22hh_from_lsb_to_celsius(raw16);

    data->lps22hh_ok = ok;
  }

  /* STTS751 : température */
  if (s_stts751_ok)
  {
    if (stts751_temperature_raw_get(&s_stts751_ctx, &raw16) == 0)
    {
      data->temp_stts751_c = stts751_from_lsb_to_celsius(raw16);
      data->stts751_ok = true;
    }
  }
}

/* Convertit un float en texte "-12.34" (2 décimales) sans printf("%f") */
static const char *fmt_fixed(float v, char *buf, size_t len)
{
  int32_t centi = (int32_t)(v * 100.0f + ((v >= 0.0f) ? 0.5f : -0.5f));
  const char *sign = (centi < 0) ? "-" : "";
  if (centi < 0)
  {
    centi = -centi;
  }
  snprintf(buf, len, "%s%ld.%02ld", sign, (long)(centi / 100), (long)(centi % 100));
  return buf;
}

void sensors_print(const sensors_data_t *data)
{
  char a[16];
  char b[16];

  if (data->hts221_ok)
  {
    printf("  HTS221  : T = %s degC | H = %s %%RH\r\n",
           fmt_fixed(data->temp_hts221_c, a, sizeof(a)),
           fmt_fixed(data->humidity_rh, b, sizeof(b)));
  }
  else
  {
    printf("  HTS221  : indisponible\r\n");
  }

  if (data->lps22hh_ok)
  {
    printf("  LPS22HH : T = %s degC | P = %s hPa\r\n",
           fmt_fixed(data->temp_lps22hh_c, a, sizeof(a)),
           fmt_fixed(data->pressure_hpa, b, sizeof(b)));
  }
  else
  {
    printf("  LPS22HH : indisponible\r\n");
  }

  if (data->stts751_ok)
  {
    printf("  STTS751 : T = %s degC\r\n",
           fmt_fixed(data->temp_stts751_c, a, sizeof(a)));
  }
  else
  {
    printf("  STTS751 : indisponible\r\n");
  }
}

#endif /* HAL_I2C_MODULE_ENABLED */
