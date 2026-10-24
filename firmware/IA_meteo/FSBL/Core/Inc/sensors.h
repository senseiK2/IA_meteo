/**
 * @file    sensors.h
 * @brief   Lecture des capteurs météo de la X-NUCLEO-IKS01A3 via I2C1
 *          (HTS221 : humidité + température, LPS22HH : pression + température,
 *           STTS751 : température).
 *
 * Le module n'est compilé que si I2C est activé dans CubeMX
 * (HAL_I2C_MODULE_ENABLED défini dans stm32n6xx_hal_conf.h). Avant ça,
 * le projet compile toujours et main.c reste sur la démo de la partie 1.
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "main.h"
#include <stdbool.h>

#if defined(HAL_I2C_MODULE_ENABLED)

typedef struct
{
  /* Présence / validité de chaque capteur */
  bool  hts221_ok;
  bool  lps22hh_ok;
  bool  stts751_ok;

  /* Mesures */
  float humidity_rh;       /* HTS221  - humidité relative (%RH) */
  float temp_hts221_c;     /* HTS221  - température (°C)        */
  float pressure_hpa;      /* LPS22HH - pression (hPa)          */
  float temp_lps22hh_c;    /* LPS22HH - température (°C)        */
  float temp_stts751_c;    /* STTS751 - température (°C)        */
} sensors_data_t;

/* Scanne les 127 adresses 7 bits et affiche les périphériques présents */
void sensors_i2c_scan(I2C_HandleTypeDef *hi2c);

/* Vérifie les WHO_AM_I et configure les 3 capteurs.
 * Retourne le nombre de capteurs détectés (0 à 3). */
int  sensors_init(I2C_HandleTypeDef *hi2c);

/* Lit toutes les grandeurs disponibles */
void sensors_read(sensors_data_t *data);

/* Affiche les mesures sur la console */
void sensors_print(const sensors_data_t *data);

#endif /* HAL_I2C_MODULE_ENABLED */
#endif /* SENSORS_H */
