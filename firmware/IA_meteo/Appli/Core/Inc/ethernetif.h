/**
  ******************************************************************************
  * @file    ethernetif.h
  * @brief   Interface entre le driver Ethernet de la HAL (ETH1 + PHY LAN8742)
  *          et la pile LwIP, version FreeRTOS.
  ******************************************************************************
  */
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"

/* À appeler AVANT MX_ETH1_Init() : donne au DMA Ethernet le droit d'accéder
 * à la RAM Secure (RIF du STM32N6). */
void  ethernetif_rif_config(void);

/* Passée à netif_add() : initialise le MAC, le PHY et les tâches de réception. */
err_t ethernetif_init(struct netif *netif);

/* Tâche qui surveille le câble (lien PHY) et démarre/arrête l'Ethernet. */
void  ethernet_link_thread(void *argument);

#endif /* ETHERNETIF_H */
