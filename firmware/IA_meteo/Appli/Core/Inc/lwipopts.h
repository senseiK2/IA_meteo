/**
  ******************************************************************************
  * @file    lwipopts.h
  * @brief   Configuration de la pile LwIP (TP2 partie 3).
  *
  * Toutes les options non définies ici gardent leur valeur par défaut
  * (voir Middlewares/Third_Party/LwIP/src/include/lwip/opt.h, très commenté).
  *
  * Choix principaux :
  *   - NO_SYS = 0     : LwIP tourne avec FreeRTOS (tâche tcpip_thread), ce qui
  *                      permet l'API sockets BSD demandée par le sujet.
  *   - DHCP, DNS, ICMP, RAW : obtenir une IP, résoudre "google.com", ping.
  *   - Checksums calculés par le MAC Ethernet (étape 4 du sujet).
  ******************************************************************************
  */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* En-tête du MCU : le port FreeRTOS de ST (sys_arch.c) utilise les fonctions
 * CMSIS du cœur (__get_IPSR...). */
#include "stm32n6xx.h"

/* ======================= Système d'exploitation ========================== */
#define NO_SYS                          0   /* 0 = avec RTOS (FreeRTOS via CMSIS-RTOS2) */
#define SYS_LIGHTWEIGHT_PROT            1   /* sections critiques via mutex (sys_arch.c) */

/* Le cœur LwIP n'est pas réentrant : les appels depuis nos tâches passent par
 * un verrou ("core locking") ou par des messages à tcpip_thread. */
#define LWIP_TCPIP_CORE_LOCKING         1

/* Vérifications de multithreading du port ST (sys_arch.c) : si une fonction
 * LwIP est appelée sans le verrou, une assertion le signale tout de suite. */
#define LWIP_CHECK_MULTITHREADING       1
void sys_lock_tcpip_core(void);
void sys_unlock_tcpip_core(void);
void sys_check_core_locking(void);
void sys_mark_tcpip_thread(void);
#define LOCK_TCPIP_CORE()               sys_lock_tcpip_core()
#define UNLOCK_TCPIP_CORE()             sys_unlock_tcpip_core()
#define LWIP_ASSERT_CORE_LOCKED()       sys_check_core_locking()
#define LWIP_MARK_TCPIP_THREAD()        sys_mark_tcpip_thread()

/* Tâches créées par LwIP (stack_size en OCTETS, priorités CMSIS-RTOS2) */
#define TCPIP_THREAD_NAME               "tcpip_thread"
#define TCPIP_THREAD_STACKSIZE          (2048)
#define TCPIP_THREAD_PRIO               (24)    /* osPriorityNormal */
#define TCPIP_MBOX_SIZE                 (16)
#define DEFAULT_THREAD_STACKSIZE        (1024)
#define DEFAULT_RAW_RECVMBOX_SIZE       (8)
#define DEFAULT_UDP_RECVMBOX_SIZE       (8)
#define DEFAULT_TCP_RECVMBOX_SIZE       (8)
#define DEFAULT_ACCEPTMBOX_SIZE         (8)

/* ============================== Mémoire =================================== */
#define MEM_ALIGNMENT                   4
/* Tas LwIP (paquets à émettre, structures diverses). Le sujet recommande
 * au moins 5000 octets ; on prend de la marge pour les sockets et le DNS. */
#define MEM_SIZE                        (10 * 1024)
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_TCP_PCB                5
#define MEMP_NUM_TCP_SEG                16
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 4)
/* Pool générique de pbufs : la réception Ethernet utilise son propre pool
 * "zero-copy" (RX_POOL dans ethernetif.c), celui-ci peut rester petit. */
#define PBUF_POOL_SIZE                  8
#define PBUF_POOL_BUFSIZE               1536

/* ============================= Protocoles ================================= */
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ETHERNET                   1
#define LWIP_ARP                        1
#define ETHARP_SUPPORT_STATIC_ENTRIES   0
#define LWIP_ICMP                       1   /* répondre aux pings, en envoyer */
#define LWIP_RAW                        1   /* sockets RAW : nécessaires pour envoyer un ping */
#define LWIP_DHCP                       1   /* adresse IP donnée par la box */
#define LWIP_DNS                        1   /* "google.com" -> adresse IP */
#define LWIP_UDP                        1   /* DHCP et DNS utilisent UDP */
#define LWIP_TCP                        1
#define TCP_MSS                         (1500 - 40)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_WND                         (2 * TCP_MSS)
#define LWIP_IGMP                       0

/* =========================== Interface réseau ============================= */
#define LWIP_NETIF_HOSTNAME             1   /* nom envoyé à la box par DHCP */
#define LWIP_NETIF_LINK_CALLBACK        1   /* prévenu quand le câble est branché/débranché */
#define LWIP_NETIF_STATUS_CALLBACK      1   /* prévenu quand l'IP change (fin du DHCP) */

/* ================================ API ===================================== */
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1   /* API sockets BSD demandée par le sujet */
#define LWIP_COMPAT_SOCKETS             1   /* noms standards : socket(), connect()... */
#define LWIP_SO_RCVTIMEO                1   /* timeout de réception (utile pour le ping) */
#define LWIP_SO_SNDTIMEO                1

/* ====================== Checksums (étape 4 du sujet) ====================== */
/* Le MAC Ethernet du STM32N6 calcule et vérifie lui-même les sommes de
 * contrôle IP/UDP/TCP/ICMP (ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC dans
 * TxConfig) : on désactive le calcul logiciel pour ne pas le faire deux fois.
 * Si on oubliait, les paquets émis auraient un checksum faux ("CRC fail"). */
#define CHECKSUM_BY_HARDWARE
#ifdef CHECKSUM_BY_HARDWARE
  #define CHECKSUM_GEN_IP               0
  #define CHECKSUM_GEN_UDP              0
  #define CHECKSUM_GEN_TCP              0
  #define CHECKSUM_GEN_ICMP             0
  #define CHECKSUM_CHECK_IP             0
  #define CHECKSUM_CHECK_UDP            0
  #define CHECKSUM_CHECK_TCP            0
  #define CHECKSUM_CHECK_ICMP           0
#endif

/* ============================ Statistiques ================================ */
#define LWIP_STATS                      0

/* ====================== Logs LwIP (étape 3 du sujet) ====================== */
/* Mettre IA_METEO_LWIP_LOGS à 1 pour voir les messages internes de LwIP sur
 * la console (printf -> USART1). Activer seulement les modules utiles,
 * sinon la console est noyée.
 * ATTENTION : LwIP teste "#ifdef LWIP_DEBUG" ; définir LWIP_DEBUG à 0
 * l'activerait quand même. D'où ce second interrupteur. */
#define IA_METEO_LWIP_LOGS              0
#if IA_METEO_LWIP_LOGS
  #define LWIP_DEBUG
  #define LWIP_DBG_MIN_LEVEL            LWIP_DBG_LEVEL_ALL
  #define DHCP_DEBUG                    LWIP_DBG_ON
  #define DNS_DEBUG                     LWIP_DBG_ON
  #define ICMP_DEBUG                    LWIP_DBG_ON
  #define ETHARP_DEBUG                  LWIP_DBG_OFF
  #define NETIF_DEBUG                   LWIP_DBG_ON
#endif

#endif /* LWIPOPTS_H */
