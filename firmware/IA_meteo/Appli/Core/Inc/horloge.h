/**
  ******************************************************************************
  * @file    horloge.h
  * @brief   Heure UTC de la carte (horloge logicielle).
  *
  * La carte n'a pas d'horloge réglée au démarrage. Le modèle météo a pourtant
  * besoin de l'heure et du jour de l'année (4 de ses 8 entrées). On procède
  * en deux temps :
  *   1. au démarrage : date et heure de COMPILATION du firmware (approximatif,
  *      mais la saison est juste) ;
  *   2. dès qu'un serveur répond (en-tête HTTP "Date:" de ThingSpeak) : heure
  *      exacte.
  * Entre deux réglages, l'heure avance avec le compteur de millisecondes de
  * FreeRTOS : elle continue de tourner même si le réseau disparaît.
  ******************************************************************************
  */
#ifndef HORLOGE_H
#define HORLOGE_H

#include <stdbool.h>
#include <stdint.h>

/* Part de la date de compilation. À appeler une fois, avant osKernelStart. */
void horloge_init(void);

/* Règle l'heure (secondes UTC depuis le 01/01/1970), reçue d'un serveur */
void horloge_regler(uint32_t epoch_utc);

/**
  * Heure actuelle, en secondes UTC depuis le 01/01/1970.
  * @return true si l'heure vient d'un serveur (exacte), false si elle vient
  *         encore de la date de compilation (approximative)
  */
bool horloge_lire(uint32_t *epoch_utc);

/* Nombre de jours entre le 01/01/1970 et une date (année, mois 1-12, jour) */
int32_t horloge_jours_depuis_1970(int32_t annee, int32_t mois, int32_t jour);

#endif /* HORLOGE_H */
