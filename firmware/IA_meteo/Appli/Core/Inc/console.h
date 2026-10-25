/**
  ******************************************************************************
  * @file    console.h
  * @brief   Accès exclusif à la console (printf -> USART1) entre les tâches.
  *
  * Plusieurs tâches écrivent sur la console. printf envoie les caractères un
  * par un : si une tâche est interrompue au milieu d'une ligne par une autre
  * qui écrit aussi, les deux messages se mélangent. On protège donc chaque
  * bloc de messages par un mutex :
  *
  *     console_lock();
  *     printf("...");
  *     printf("...");
  *     console_unlock();
  ******************************************************************************
  */
#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);     /* crée le mutex : dans MX_FREERTOS_Init()     */
void console_lock(void);     /* sans effet avant le démarrage de FreeRTOS   */
void console_unlock(void);

#endif /* CONSOLE_H */
