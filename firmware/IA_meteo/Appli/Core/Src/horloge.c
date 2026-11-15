/**
  ******************************************************************************
  * @file    horloge.c
  * @brief   Heure UTC de la carte (horloge logicielle), voir horloge.h.
  *
  * L'heure est mémorisée sous la forme d'un couple :
  *   (epoch de référence, tick FreeRTOS à ce moment-là)
  * L'heure actuelle = epoch de référence + (tick actuel - tick de référence).
  * Le tick FreeRTOS compte les millisecondes depuis le démarrage.
  ******************************************************************************
  */
#include "horloge.h"
#include "cmsis_os2.h"
#include <string.h>

static uint32_t s_epoch_ref = 0U;    /* secondes UTC depuis le 01/01/1970 */
static uint32_t s_tick_ref = 0U;     /* tick FreeRTOS (ms) au même instant */
static bool     s_exacte = false;    /* true : réglée par un serveur      */

/* Décalage entre l'heure du PC de compilation et l'heure UTC : le PC est à
 * l'heure de Paris, UTC+1 en hiver (UTC+2 en été : une heure d'erreur au
 * pire, sans importance tant que le serveur n'a pas répondu). */
#define DECALAGE_COMPILATION_S  3600

/* Algorithme "days from civil" (H. Hinnant), valable pour toutes les dates
 * usuelles : nombre de jours entre le 01/01/1970 et la date donnée. */
int32_t horloge_jours_depuis_1970(int32_t a, int32_t m, int32_t j)
{
  a -= (m <= 2) ? 1 : 0;
  int32_t ere = (a >= 0 ? a : a - 399) / 400;
  int32_t ae  = a - ere * 400;                                   /* [0, 399] */
  int32_t ja  = (153 * (m + ((m > 2) ? -3 : 9)) + 2) / 5 + j - 1; /* [0, 365] */
  int32_t je  = ae * 365 + ae / 4 - ae / 100 + ja;               /* [0, 146096] */
  return ere * 146097 + je - 719468;
}

void horloge_init(void)
{
  /* Le compilateur fournit la date et l'heure de compilation sous forme de
   * texte : __DATE__ = "Nov 15 2026", __TIME__ = "11:13:05" */
  static const char mois_noms[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *date = __DATE__;
  const char *heure = __TIME__;
  char mois[4] = { date[0], date[1], date[2], '\0' };
  int32_t m = (int32_t)((strstr(mois_noms, mois) - mois_noms) / 3) + 1;
  int32_t j = (date[4] == ' ' ? 0 : (date[4] - '0') * 10) + (date[5] - '0');
  int32_t a = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');
  int32_t h = (heure[0] - '0') * 10 + (heure[1] - '0');
  int32_t mi = (heure[3] - '0') * 10 + (heure[4] - '0');
  int32_t s = (heure[6] - '0') * 10 + (heure[7] - '0');

  s_epoch_ref = (uint32_t)(horloge_jours_depuis_1970(a, m, j) * 86400
                           + h * 3600 + mi * 60 + s - DECALAGE_COMPILATION_S);
  s_tick_ref = 0U;                     /* appelée avant le démarrage du noyau */
  s_exacte = false;
}

void horloge_regler(uint32_t epoch_utc)
{
  /* Deux tâches utilisent l'horloge (TacheReseau la règle, TacheIA la lit) :
   * on empêche tout changement de tâche pendant la mise à jour des deux
   * variables, pour qu'elles restent cohérentes entre elles. */
  int32_t verrou = osKernelLock();
  s_epoch_ref = epoch_utc;
  s_tick_ref = osKernelGetTickCount();
  s_exacte = true;
  osKernelRestoreLock(verrou);
}

bool horloge_lire(uint32_t *epoch_utc)
{
  int32_t verrou = osKernelLock();
  uint32_t epoch = s_epoch_ref, tick = s_tick_ref;
  bool exacte = s_exacte;
  osKernelRestoreLock(verrou);

  *epoch_utc = epoch + (osKernelGetTickCount() - tick) / 1000U;
  return exacte;
}
