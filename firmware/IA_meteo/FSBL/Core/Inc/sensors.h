/**
 * @file    sensors.h
 * @brief   Lecture des capteurs météo de la carte d'extension via I2C1.
 *
 * Deux cartes sont supportées, détectées automatiquement au démarrage :
 *
 *   Grandeur              X-NUCLEO-IKS01A3        X-NUCLEO-IKS4A1
 *   --------------------  ----------------------  ----------------------
 *   Humidité + temp.      HTS221   (0x5F)         SHT40AD1B (0x44)
 *   Pression + temp.      LPS22HH  (0x5D, 0xB3)   LPS22DF   (0x5D, 0xB4)
 *   Température           STTS751  (0x4A)         STTS22H   (0x38)
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
  /* Validité de chaque mesure (capteur présent ET lecture réussie) */
  bool  humidity_ok;
  bool  pressure_ok;
  bool  temperature_ok;

  /* Capteur d'humidité (HTS221 ou SHT40) */
  float humidity_rh;       /* humidité relative (%RH) */
  float temp_hum_c;        /* température mesurée par ce capteur (°C) */

  /* Capteur de pression (LPS22HH ou LPS22DF) */
  float pressure_hpa;      /* pression (hPa) */
  float temp_press_c;      /* température mesurée par ce capteur (°C) */

  /* Capteur de température dédié (STTS751 ou STTS22H) */
  float temperature_c;     /* température (°C) */
} sensors_data_t;

/* Scanne les 127 adresses 7 bits et affiche les périphériques présents */
void sensors_i2c_scan(I2C_HandleTypeDef *hi2c);

/* Détecte et configure les 3 capteurs.
 * Retourne le nombre de capteurs prêts (0 à 3). */
int  sensors_init(I2C_HandleTypeDef *hi2c);

/* Lit toutes les grandeurs disponibles */
void sensors_read(sensors_data_t *data);

/* Affiche les mesures sur la console */
void sensors_print(const sensors_data_t *data);

#endif /* HAL_I2C_MODULE_ENABLED */
#endif /* SENSORS_H */
