/**
  ******************************************************************************
  * @file    app_ethernet.h
  * @brief   Démarrage du réseau : pile LwIP, interface Ethernet, DHCP.
  ******************************************************************************
  */
#ifndef APP_ETHERNET_H
#define APP_ETHERNET_H

#include <stdbool.h>
#include <stdint.h>
#include "lwip/netif.h"

/* L'interface réseau de la carte (adresse MAC, IP, état du lien...) */
extern struct netif gnetif;

/* Démarre LwIP (tâche tcpip_thread), ajoute l'interface Ethernet et lance la
 * surveillance du câble. Le DHCP démarre tout seul quand le câble est branché.
 * À appeler UNE fois, depuis une tâche FreeRTOS (pas avant osKernelStart). */
void reseau_init(void);

/* Attend que le DHCP ait fourni une adresse IP.
 * Retourne true si on a une IP avant timeout_ms, false sinon. */
bool reseau_attendre_ip(uint32_t timeout_ms);

/* true si la carte a une adresse IP (câble branché + DHCP terminé) */
bool reseau_est_pret(void);

#endif /* APP_ETHERNET_H */
