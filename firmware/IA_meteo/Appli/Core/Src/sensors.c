/**
 * @file    sensors.c
 * @brief   Lecture des capteurs météo (X-NUCLEO-IKS01A3 ou X-NUCLEO-IKS4A1)
 *          via I2C1.
 *
 * Organisation :
 *   1. Couche "plateforme" : platform_read / platform_write / platform_delay
 *      -> seules fonctions dépendantes du MCU (HAL I2C STM32N6).
 *   2. Drivers ST (xxx_reg.c) : accèdent aux registres via stmdev_ctx_t,
 *      sans rien savoir du MCU. Le SHT40 (capteur Sensirion) n'a pas de
 *      driver ST : il est piloté directement par commandes I2C.
 *   3. API haut niveau : sensors_init / sensors_read / sensors_print.
 *
 * Détection : pour chaque grandeur (humidité, pression, température) on
 * essaie le capteur de l'IKS01A3 puis celui de l'IKS4A1, en vérifiant
 * l'identifiant (WHO_AM_I) de chaque composant.
 *
 * Gestion des adresses 7/8 bits :
 *   Les datasheets donnent l'adresse 7 bits (ex. HTS221 = 0x5F).
 *   La HAL STM32 attend l'adresse décalée d'un bit à gauche (0x5F << 1 = 0xBE)
 *   et positionne elle-même le bit R/W. On stocke donc l'adresse 7 bits
 *   et on fait le décalage une seule fois, à l'initialisation.
 *   (Les drivers ST, eux, donnent des adresses 8 bits du type 0xBB :
 *    0xBB >> 1 = 0x5D.)
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
#include "lps22df_reg.h"
#include "stts22h_reg.h"

/* ------------------------------------------------------------------------- */
/* Paramètres                                                                */
/* ------------------------------------------------------------------------- */
#define I2C_TIMEOUT_MS        100U

/* Adresses 7 bits */
#define HTS221_ADDR_7B        0x5FU   /* IKS01A3 */
#define SHT40_ADDR_7B         0x44U   /* IKS4A1 (SHT40AD1B)               */
#define LPS22_ADDR_7B         0x5DU   /* LPS22HH (IKS01A3) / LPS22DF (IKS4A1), SA0 = 1 */
#define LPS22_ADDR_ALT_7B     0x5CU   /* même capteur avec SA0 = 0          */

/* STTS22H : 4 adresses possibles selon la résistance sur ADDR (stts22h_reg.h).
 * Sur l'IKS4A1 c'est normalement 0x38. */
static const uint8_t STTS22H_ADDR_CANDIDATES[] = { 0x38U, 0x3CU, 0x3EU, 0x3FU };

/* STTS751 : 8 adresses possibles selon la résistance et la version
 * (stts751_reg.h). Sur l'IKS01A3 c'est normalement 0x4A. */
static const uint8_t STTS751_ADDR_CANDIDATES[] =
{
  0x4AU, 0x48U, 0x49U, 0x4BU, 0x38U, 0x39U, 0x3AU, 0x3BU
};

/* SHT40 : commandes (datasheet Sensirion SHT4x) */
#define SHT40_CMD_MEASURE_HIGH   0xFDU  /* mesure haute précision, ~8,3 ms */
#define SHT40_CMD_READ_SERIAL    0x89U
#define SHT40_MEASURE_DELAY_MS   10U

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
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

typedef enum { HUM_NONE = 0, HUM_HTS221, HUM_SHT40 }        hum_sensor_t;
typedef enum { PRESS_NONE = 0, PRESS_LPS22HH, PRESS_LPS22DF } press_sensor_t;
typedef enum { TEMP_NONE = 0, TEMP_STTS751, TEMP_STTS22H }   temp_sensor_t;

/* ------------------------------------------------------------------------- */
/* État du module                                                            */
/* ------------------------------------------------------------------------- */
static I2C_HandleTypeDef *s_hi2c;

static hum_sensor_t   s_hum   = HUM_NONE;
static press_sensor_t s_press = PRESS_NONE;
static temp_sensor_t  s_temp  = TEMP_NONE;

static i2c_dev_t      s_hum_bus,   s_press_bus,   s_temp_bus;
static stmdev_ctx_t   s_hum_ctx,   s_press_ctx,   s_temp_ctx;

static lin_t          s_hts221_hum_cal;
static lin_t          s_hts221_temp_cal;
static stts22h_priv_t s_stts22h_priv;

/* ------------------------------------------------------------------------- */
/* 1. Couche plateforme                                                      */
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

static void ctx_setup(stmdev_ctx_t *ctx, i2c_dev_t *bus, uint8_t addr7, uint8_t autoinc)
{
  bus->hi2c        = s_hi2c;
  bus->addr8       = (uint16_t)(addr7 << 1);
  bus->reg_autoinc = autoinc;

  ctx->write_reg = platform_write;
  ctx->read_reg  = platform_read;
  ctx->mdelay    = platform_delay;
  ctx->handle    = bus;
  ctx->priv_data = NULL;
}

static bool device_present(uint8_t addr7)
{
  return HAL_I2C_IsDeviceReady(s_hi2c, (uint16_t)(addr7 << 1), 2, 10) == HAL_OK;
}

/* Lit un registre d'identification (WHO_AM_I) sans passer par un driver */
static bool read_id_reg(uint8_t addr7, uint8_t reg, uint8_t *id)
{
  return HAL_I2C_Mem_Read(s_hi2c, (uint16_t)(addr7 << 1), reg, I2C_MEMADD_SIZE_8BIT,
                          id, 1, I2C_TIMEOUT_MS) == HAL_OK;
}

/* Interpolation linéaire avec les coefficients de calibration du HTS221 */
static float_t linear_interpolation(const lin_t *lin, int16_t x)
{
  return ((lin->y1 - lin->y0) * x + ((lin->x1 * lin->y0) - (lin->x0 * lin->y1)))
         / (lin->x1 - lin->x0);
}

/* ------------------------------------------------------------------------- */
/* SHT40 (pas de driver ST : commandes I2C directes)                         */
/* ------------------------------------------------------------------------- */
/* CRC-8 Sensirion : polynôme 0x31, valeur initiale 0xFF */
static uint8_t sht40_crc8(const uint8_t *data, uint32_t len)
{
  uint8_t crc = 0xFFU;
  for (uint32_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
    {
      crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

/* Envoie une commande puis lit la réponse de 6 octets
 * (2 octets + CRC, 2 octets + CRC). */
static bool sht40_command(uint8_t cmd, uint32_t wait_ms, uint16_t *w0, uint16_t *w1)
{
  uint8_t rx[6];
  uint16_t addr8 = (uint16_t)(SHT40_ADDR_7B << 1);

  if (HAL_I2C_Master_Transmit(s_hi2c, addr8, &cmd, 1, I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }
  HAL_Delay(wait_ms);
  if (HAL_I2C_Master_Receive(s_hi2c, addr8, rx, 6, I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }
  if ((sht40_crc8(&rx[0], 2) != rx[2]) || (sht40_crc8(&rx[3], 2) != rx[5]))
  {
    return false;
  }
  *w0 = (uint16_t)((rx[0] << 8) | rx[1]);
  *w1 = (uint16_t)((rx[3] << 8) | rx[4]);
  return true;
}

/* ------------------------------------------------------------------------- */
/* Scan du bus I2C                                                           */
/* ------------------------------------------------------------------------- */
static const char *addr_name(uint8_t addr)
{
  switch (addr)
  {
    case 0x5F: return "HTS221 (humidite/temp., IKS01A3)";
    case 0x44: return "SHT40 (humidite/temp., IKS4A1)";
    case 0x5C:
    case 0x5D: return "LPS22HH / LPS22DF (pression)";
    case 0x38: return "STTS22H (IKS4A1) ou STTS751";
    case 0x3C: case 0x3E: case 0x3F: return "STTS22H (temperature)";
    case 0x48: case 0x49: case 0x4A: case 0x4B:
    case 0x39: case 0x3A: case 0x3B: return "STTS751 (temperature, IKS01A3)";
    case 0x6A: return "LSM6DSO16IS (accelero/gyro, IKS4A1)";
    case 0x6B: return "LSM6DSO / LSM6DSV16X (accelero/gyro)";
    case 0x1E: return "LIS2MDL (magnetometre)";
    case 0x19: return "LIS2DW12 / LIS2DUXS12 (accelerometre)";
    default:   return "?";
  }
}

void sensors_i2c_scan(I2C_HandleTypeDef *hi2c)
{
  int found = 0;

  printf("  Scan I2C1 (adresses 7 bits) :\r\n");
  for (uint8_t addr = 1; addr < 128; addr++)
  {
    if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 2, 10) == HAL_OK)
    {
      printf("    0x%02X  %s\r\n", addr, addr_name(addr));
      found++;
    }
  }
  printf("  %d peripherique(s) trouve(s)\r\n", found);

  if (found == 0)
  {
    /* Diagnostic : au repos, un bus I2C doit être à l'état haut (pull-ups).
     * Une ligne à 0 = pas de pull-up, carte mal enfichée ou court-circuit. */
    int sda = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_SET);
    int scl = (HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_9) == GPIO_PIN_SET);
    printf("  Diagnostic : SDA (PC1) = %d, SCL (PH9) = %d  (attendu : 1 et 1)\r\n", sda, scl);
    if (!sda || !scl)
    {
      printf("  -> ligne a 0 : verifier les pull-ups (GPIO_PULLUP) et la carte capteurs\r\n");
    }
    else
    {
      printf("  -> lignes OK : verifier que la carte capteurs est bien enfichee\r\n");
    }
  }
}

/* ------------------------------------------------------------------------- */
/* Détection et configuration de chaque grandeur                             */
/* ------------------------------------------------------------------------- */
static hum_sensor_t humidity_setup(void)
{
  uint8_t id = 0;

  /* --- HTS221 (IKS01A3) --- */
  if (device_present(HTS221_ADDR_7B))
  {
    /* Le HTS221 exige le bit 7 de l'adresse registre pour l'auto-incrément */
    ctx_setup(&s_hum_ctx, &s_hum_bus, HTS221_ADDR_7B, 0x80U);

    if ((hts221_device_id_get(&s_hum_ctx, &id) == 0) && (id == HTS221_ID))
    {
      /* Coefficients de calibration usine (stockés dans le capteur) */
      hts221_hum_adc_point_0_get(&s_hum_ctx, &s_hts221_hum_cal.x0);
      hts221_hum_rh_point_0_get(&s_hum_ctx,  &s_hts221_hum_cal.y0);
      hts221_hum_adc_point_1_get(&s_hum_ctx, &s_hts221_hum_cal.x1);
      hts221_hum_rh_point_1_get(&s_hum_ctx,  &s_hts221_hum_cal.y1);

      hts221_temp_adc_point_0_get(&s_hum_ctx, &s_hts221_temp_cal.x0);
      hts221_temp_deg_point_0_get(&s_hum_ctx, &s_hts221_temp_cal.y0);
      hts221_temp_adc_point_1_get(&s_hum_ctx, &s_hts221_temp_cal.x1);
      hts221_temp_deg_point_1_get(&s_hum_ctx, &s_hts221_temp_cal.y1);

      hts221_block_data_update_set(&s_hum_ctx, PROPERTY_ENABLE);
      hts221_data_rate_set(&s_hum_ctx, HTS221_ODR_1Hz);
      hts221_power_on_set(&s_hum_ctx, PROPERTY_ENABLE);

      printf("  Humidite    : HTS221 (0x%02X, WHO_AM_I 0x%02X)\r\n", HTS221_ADDR_7B, id);
      return HUM_HTS221;
    }
    printf("  Humidite    : 0x%02X repond mais WHO_AM_I = 0x%02X (HTS221 attendu 0x%02X)\r\n",
           HTS221_ADDR_7B, id, HTS221_ID);
  }

  /* --- SHT40 (IKS4A1) : on lit son numéro de série pour le reconnaître --- */
  if (device_present(SHT40_ADDR_7B))
  {
    uint16_t sn_hi, sn_lo;
    if (sht40_command(SHT40_CMD_READ_SERIAL, 1, &sn_hi, &sn_lo))
    {
      printf("  Humidite    : SHT40 (0x%02X, n. serie 0x%04X%04X)\r\n",
             SHT40_ADDR_7B, sn_hi, sn_lo);
      return HUM_SHT40;
    }
    printf("  Humidite    : 0x%02X repond mais n'est pas un SHT40\r\n", SHT40_ADDR_7B);
  }

  printf("  Humidite    : aucun capteur (ni HTS221 @0x5F, ni SHT40 @0x44)\r\n");
  return HUM_NONE;
}

static press_sensor_t pressure_setup(void)
{
  const uint8_t addrs[] = { LPS22_ADDR_7B, LPS22_ADDR_ALT_7B };

  for (uint32_t i = 0; i < sizeof(addrs); i++)
  {
    uint8_t addr7 = addrs[i];
    uint8_t id = 0;

    if (!device_present(addr7) || !read_id_reg(addr7, 0x0FU, &id))
    {
      continue;
    }

    /* Le LPS22HH et le LPS22DF auto-incrémentent : pas de bit à ajouter */
    ctx_setup(&s_press_ctx, &s_press_bus, addr7, 0x00U);

    if (id == LPS22HH_ID)               /* 0xB3 : IKS01A3 */
    {
      uint8_t rst = 1;
      uint32_t t0;

      lps22hh_reset_set(&s_press_ctx, PROPERTY_ENABLE);
      t0 = HAL_GetTick();
      do
      {
        lps22hh_reset_get(&s_press_ctx, &rst);
      } while (rst && ((HAL_GetTick() - t0) < 100U));

      lps22hh_block_data_update_set(&s_press_ctx, PROPERTY_ENABLE);
      lps22hh_data_rate_set(&s_press_ctx, LPS22HH_10_Hz_LOW_NOISE);

      printf("  Pression    : LPS22HH (0x%02X, WHO_AM_I 0x%02X)\r\n", addr7, id);
      return PRESS_LPS22HH;
    }

    if (id == LPS22DF_ID)               /* 0xB4 : IKS4A1 */
    {
      lps22df_bus_mode_t bus_mode;
      lps22df_md_t md;

      /* Séquence de l'exemple ST lps22df_read_data_polling.c */
      lps22df_init_set(&s_press_ctx, LPS22DF_BOOT);
      lps22df_init_set(&s_press_ctx, LPS22DF_RESET);
      lps22df_init_set(&s_press_ctx, LPS22DF_DRV_RDY);   /* BDU + auto-incrément */

      bus_mode.filter       = LPS22DF_FILTER_AUTO;
      bus_mode.interface    = LPS22DF_SEL_BY_HW;
      bus_mode.i3c_ibi_time = LPS22DF_IBI_50us;
      lps22df_bus_mode_set(&s_press_ctx, &bus_mode);

      md.odr = LPS22DF_4Hz;
      md.avg = LPS22DF_16_AVG;
      md.lpf = LPS22DF_LPF_ODR_DIV_4;
      lps22df_mode_set(&s_press_ctx, &md);

      printf("  Pression    : LPS22DF (0x%02X, WHO_AM_I 0x%02X)\r\n", addr7, id);
      return PRESS_LPS22DF;
    }

    printf("  Pression    : 0x%02X repond, WHO_AM_I = 0x%02X inconnu\r\n", addr7, id);
  }

  printf("  Pression    : aucun capteur (LPS22HH/LPS22DF @0x5D)\r\n");
  return PRESS_NONE;
}

static temp_sensor_t temperature_setup(void)
{
  /* --- STTS22H (IKS4A1) : WHO_AM_I (registre 0x01) = 0xA0 --- */
  for (uint32_t i = 0; i < sizeof(STTS22H_ADDR_CANDIDATES); i++)
  {
    uint8_t addr7 = STTS22H_ADDR_CANDIDATES[i];
    uint8_t id = 0;

    if (!device_present(addr7))
    {
      continue;
    }
    ctx_setup(&s_temp_ctx, &s_temp_bus, addr7, 0x00U);
    if ((stts22h_dev_id_get(&s_temp_ctx, &id) == 0) && (id == STTS22H_ID))
    {
      s_temp_ctx.priv_data = &s_stts22h_priv;
      stts22h_auto_increment_set(&s_temp_ctx, PROPERTY_ENABLE);
      stts22h_temp_data_rate_set(&s_temp_ctx, STTS22H_1Hz);

      printf("  Temperature : STTS22H (0x%02X, WHO_AM_I 0x%02X)\r\n", addr7, id);
      return TEMP_STTS22H;
    }
  }

  /* --- STTS751 (IKS01A3) : identifiant fabricant 0x53 --- */
  for (uint32_t i = 0; i < sizeof(STTS751_ADDR_CANDIDATES); i++)
  {
    uint8_t addr7 = STTS751_ADDR_CANDIDATES[i];
    stts751_id_t id = {0};

    if (!device_present(addr7))
    {
      continue;
    }
    ctx_setup(&s_temp_ctx, &s_temp_bus, addr7, 0x00U);
    if ((stts751_device_id_get(&s_temp_ctx, &id) == 0) &&
        (id.manufacturer_id == STTS751_ID_MAN))
    {
      stts751_temp_data_rate_set(&s_temp_ctx, STTS751_TEMP_ODR_1Hz);
      stts751_resolution_set(&s_temp_ctx, STTS751_11bit);

      printf("  Temperature : STTS751 (0x%02X, product 0x%02X, manuf 0x%02X)\r\n",
             addr7, id.product_id, id.manufacturer_id);
      return TEMP_STTS751;
    }
  }

  printf("  Temperature : aucun capteur (STTS22H @0x38 / STTS751 @0x4A)\r\n");
  return TEMP_NONE;
}

int sensors_init(I2C_HandleTypeDef *hi2c)
{
  s_hi2c = hi2c;
  HAL_Delay(10); /* temps de démarrage des capteurs */

  s_hum   = humidity_setup();
  s_press = pressure_setup();
  s_temp  = temperature_setup();

  int nb = (s_hum != HUM_NONE) + (s_press != PRESS_NONE) + (s_temp != TEMP_NONE);

  if ((s_hum == HUM_SHT40) || (s_press == PRESS_LPS22DF) || (s_temp == TEMP_STTS22H))
  {
    printf("  Carte detectee : X-NUCLEO-IKS4A1\r\n");
  }
  else if (nb > 0)
  {
    printf("  Carte detectee : X-NUCLEO-IKS01A3\r\n");
  }

  /* Les capteurs tournent à 1-4 Hz : on attend la première conversion,
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
static float clamp_rh(float rh)
{
  if (rh < 0.0f)   { return 0.0f; }
  if (rh > 100.0f) { return 100.0f; }
  return rh;
}

void sensors_read(sensors_data_t *data)
{
  int16_t  raw16;
  uint32_t raw32;

  data->humidity_ok    = false;
  data->pressure_ok    = false;
  data->temperature_ok = false;

  /* Humidité + température */
  if (s_hum == HUM_HTS221)
  {
    bool ok = (hts221_humidity_raw_get(&s_hum_ctx, &raw16) == 0);
    data->humidity_rh = clamp_rh(linear_interpolation(&s_hts221_hum_cal, raw16));
    ok &= (hts221_temperature_raw_get(&s_hum_ctx, &raw16) == 0);
    data->temp_hum_c = linear_interpolation(&s_hts221_temp_cal, raw16);
    data->humidity_ok = ok;
  }
  else if (s_hum == HUM_SHT40)
  {
    uint16_t st, srh;
    if (sht40_command(SHT40_CMD_MEASURE_HIGH, SHT40_MEASURE_DELAY_MS, &st, &srh))
    {
      /* Formules de conversion de la datasheet SHT4x */
      data->temp_hum_c  = -45.0f + 175.0f * (float)st  / 65535.0f;
      data->humidity_rh = clamp_rh(-6.0f + 125.0f * (float)srh / 65535.0f);
      data->humidity_ok = true;
    }
  }

  /* Pression + température */
  if (s_press == PRESS_LPS22HH)
  {
    bool ok = (lps22hh_pressure_raw_get(&s_press_ctx, &raw32) == 0);
    data->pressure_hpa = lps22hh_from_lsb_to_hpa(raw32);
    ok &= (lps22hh_temperature_raw_get(&s_press_ctx, &raw16) == 0);
    data->temp_press_c = lps22hh_from_lsb_to_celsius(raw16);
    data->pressure_ok = ok;
  }
  else if (s_press == PRESS_LPS22DF)
  {
    lps22df_data_t d;
    if (lps22df_data_get(&s_press_ctx, &d) == 0)
    {
      data->pressure_hpa = d.pressure.hpa;
      data->temp_press_c = d.heat.deg_c;
      data->pressure_ok  = true;
    }
  }

  /* Température dédiée */
  if (s_temp == TEMP_STTS22H)
  {
    if (stts22h_temperature_raw_get(&s_temp_ctx, &raw16) == 0)
    {
      data->temperature_c  = stts22h_from_lsb_to_celsius(raw16);
      data->temperature_ok = true;
    }
  }
  else if (s_temp == TEMP_STTS751)
  {
    if (stts751_temperature_raw_get(&s_temp_ctx, &raw16) == 0)
    {
      data->temperature_c  = stts751_from_lsb_to_celsius(raw16);
      data->temperature_ok = true;
    }
  }
}

/* ------------------------------------------------------------------------- */
/* Affichage                                                                 */
/* ------------------------------------------------------------------------- */
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

static const char *hum_name(void)
{
  return (s_hum == HUM_HTS221) ? "HTS221 " : (s_hum == HUM_SHT40) ? "SHT40  " : "Humid. ";
}

static const char *press_name(void)
{
  return (s_press == PRESS_LPS22HH) ? "LPS22HH" : (s_press == PRESS_LPS22DF) ? "LPS22DF" : "Press. ";
}

static const char *temp_name(void)
{
  return (s_temp == TEMP_STTS751) ? "STTS751" : (s_temp == TEMP_STTS22H) ? "STTS22H" : "Temp.  ";
}

void sensors_print(const sensors_data_t *data)
{
  char a[16];
  char b[16];

  if (data->humidity_ok)
  {
    printf("  %s : T = %s degC | H = %s %%RH\r\n", hum_name(),
           fmt_fixed(data->temp_hum_c, a, sizeof(a)),
           fmt_fixed(data->humidity_rh, b, sizeof(b)));
  }
  else
  {
    printf("  %s : indisponible\r\n", hum_name());
  }

  if (data->pressure_ok)
  {
    printf("  %s : T = %s degC | P = %s hPa\r\n", press_name(),
           fmt_fixed(data->temp_press_c, a, sizeof(a)),
           fmt_fixed(data->pressure_hpa, b, sizeof(b)));
  }
  else
  {
    printf("  %s : indisponible\r\n", press_name());
  }

  if (data->temperature_ok)
  {
    printf("  %s : T = %s degC\r\n", temp_name(),
           fmt_fixed(data->temperature_c, a, sizeof(a)));
  }
  else
  {
    printf("  %s : indisponible\r\n", temp_name());
  }
}

#endif /* HAL_I2C_MODULE_ENABLED */
