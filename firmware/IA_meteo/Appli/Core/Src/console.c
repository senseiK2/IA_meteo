/**
  ******************************************************************************
  * @file    console.c
  * @brief   Accès exclusif à la console (printf -> USART1) entre les tâches.
  *
  * Mutex et non sémaphore : un mutex a un "propriétaire" (la tâche qui l'a
  * pris est la seule à pouvoir le rendre) et gère l'héritage de priorité.
  *
  *  - osMutexRecursive   : une tâche qui a déjà le verrou peut le reprendre
  *                         (ex. log_reseau() appelé depuis un bloc déjà
  *                         verrouillé) sans se bloquer elle-même.
  *  - osMutexPrioInherit : si une tâche peu prioritaire tient la console et
  *                         qu'une tâche prioritaire l'attend, la première
  *                         hérite temporairement de la priorité haute pour
  *                         finir vite (évite l'"inversion de priorité").
  ******************************************************************************
  */
#include "console.h"
#include "cmsis_os2.h"

static osMutexId_t s_console_mutex = NULL;

void console_init(void)
{
  const osMutexAttr_t attr = {
    .name      = "Console",
    .attr_bits = osMutexRecursive | osMutexPrioInherit,
  };
  s_console_mutex = osMutexNew(&attr);
}

void console_lock(void)
{
  /* Avant osKernelStart() il n'y a qu'un seul fil d'exécution : rien à protéger */
  if ((s_console_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    osMutexAcquire(s_console_mutex, osWaitForever);
  }
}

void console_unlock(void)
{
  if ((s_console_mutex != NULL) && (osKernelGetState() == osKernelRunning))
  {
    osMutexRelease(s_console_mutex);
  }
}
