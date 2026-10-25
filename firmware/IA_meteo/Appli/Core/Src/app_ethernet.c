/**
  ******************************************************************************
  * @file    app_ethernet.c
  * @brief   Démarrage du réseau : pile LwIP, interface Ethernet, DHCP.
  *
  * Déroulement :
  *   1. reseau_init()  : tcpip_init() crée la tâche "tcpip_thread" de LwIP,
  *                       netif_add() déclare l'interface Ethernet (ethernetif.c),
  *                       la tâche "EthLink" surveille le câble.
  *   2. Câble branché  : link_callback() lance le DHCP.
  *   3. IP obtenue     : status_callback() affiche l'IP, le masque, la
  *                       passerelle (la box) et le serveur DNS.
  *
  * Règle d'or avec LwIP + RTOS : toute fonction "raw" de LwIP (netif_*,
  * dhcp_*...) appelée depuis une de NOS tâches doit être entourée de
  * LOCK_TCPIP_CORE() / UNLOCK_TCPIP_CORE(). Les callbacks ci-dessous sont,
  * eux, appelés par LwIP avec le verrou déjà pris.
  * Les sockets (socket(), connect()...) gèrent le verrou toutes seules.
  ******************************************************************************
  */
#include "app_ethernet.h"
#include "ethernetif.h"
#include "console.h"
#include "cmsis_os2.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include <stdio.h>

struct netif gnetif;

static void log_reseau(const char *etat)
{
  console_lock();
  printf("[%8lu ms] <Communication Reseau> %s\r\n",
         (unsigned long)osKernelGetTickCount(), etat);
  console_unlock();
}

/* Appelé par LwIP quand le câble est branché ou débranché */
static void link_callback(struct netif *netif)
{
  if (netif_is_link_up(netif))
  {
    log_reseau("cable branche, demande d'adresse IP (DHCP)...");
    dhcp_start(netif);
  }
  else
  {
    log_reseau("cable debranche");
    dhcp_release_and_stop(netif);
  }
}

/* Appelé par LwIP quand l'interface monte/descend ou que son IP change */
static void status_callback(struct netif *netif)
{
  if (!ip4_addr_isany_val(*netif_ip4_addr(netif)))
  {
    char ip[16], mask[16], gw[16], dns[16];

    ip4addr_ntoa_r(netif_ip4_addr(netif),    ip,   sizeof(ip));
    ip4addr_ntoa_r(netif_ip4_netmask(netif), mask, sizeof(mask));
    ip4addr_ntoa_r(netif_ip4_gw(netif),      gw,   sizeof(gw));
    ip4addr_ntoa_r(ip_2_ip4(dns_getserver(0)), dns, sizeof(dns));

    console_lock();                    /* les 5 lignes d'un seul tenant */
    log_reseau("adresse IP obtenue");  /* mutex récursif : on peut le reprendre */
    printf("  IP         : %s\r\n", ip);
    printf("  Masque     : %s\r\n", mask);
    printf("  Passerelle : %s\r\n", gw);
    printf("  DNS        : %s\r\n", dns);
    console_unlock();
  }
}

void reseau_init(void)
{
  log_reseau("demarrage de LwIP");

  /* Crée tcpip_thread (priorité/pile : TCPIP_THREAD_* dans lwipopts.h) */
  tcpip_init(NULL, NULL);

  /* Interface sans IP au départ : le DHCP la remplira.
   * tcpip_input : les trames reçues sont traitées dans tcpip_thread. */
  LOCK_TCPIP_CORE();
  netif_add(&gnetif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4,
            NULL, &ethernetif_init, &tcpip_input);
  netif_set_default(&gnetif);
  netif_set_link_callback(&gnetif, link_callback);
  netif_set_status_callback(&gnetif, status_callback);
  UNLOCK_TCPIP_CORE();

  /* Surveillance du câble (ethernetif.c) */
  const osThreadAttr_t link_attr = {
    .name       = "EthLink",
    .stack_size = 1536,
    .priority   = (osPriority_t)osPriorityBelowNormal,
  };
  osThreadNew(ethernet_link_thread, &gnetif, &link_attr);
}

bool reseau_est_pret(void)
{
  bool pret;

  LOCK_TCPIP_CORE();
  pret = netif_is_link_up(&gnetif) && dhcp_supplied_address(&gnetif);
  UNLOCK_TCPIP_CORE();
  return pret;
}

bool reseau_attendre_ip(uint32_t timeout_ms)
{
  uint32_t debut = osKernelGetTickCount();

  while (!reseau_est_pret())
  {
    if ((osKernelGetTickCount() - debut) >= timeout_ms)
    {
      log_reseau("pas d'adresse IP (cable ? box ? serveur DHCP ?)");
      return false;
    }
    osDelay(250);
  }
  return true;
}
