/**
  ******************************************************************************
  * @file    ethernetif.c
  * @brief   Interface entre le driver Ethernet de la HAL (ETH1 + PHY LAN8742)
  *          et la pile LwIP, version FreeRTOS.
  *
  * Basé sur l'exemple ST "LwIP_TCP_Echo_Server" pour la NUCLEO-N657X0-Q
  * (stm32n6-classic-coremw-apps), écrit pour LwIP sans RTOS, et adapté ici à
  * FreeRTOS sur le modèle des ethernetif "RTOS" générés par CubeMX :
  *
  *   Réception : interruption ETH1 -> HAL_ETH_RxCpltCallback() libère un
  *               sémaphore -> la tâche "EthIf" se réveille, lit les trames
  *               (HAL_ETH_ReadData) et les passe à LwIP (tcpip_input).
  *   Émission  : LwIP appelle low_level_output() -> HAL_ETH_Transmit_IT().
  *   Lien      : la tâche "EthLink" interroge le PHY toutes les 100 ms et
  *               démarre/arrête l'Ethernet quand on branche/débranche le câble.
  *
  * Mode "zero-copy" : le DMA écrit les trames directement dans des buffers
  * pris dans un pool LwIP (RX_POOL), sans recopie par le CPU.
  *
  * Cache : le D-cache du Cortex-M55 n'est pas activé dans ce projet, donc pas
  * de problème de cohérence entre le DMA et le CPU. Si on l'active un jour,
  * il faudra placer descripteurs et buffers en mémoire non cachée (MPU),
  * comme dans l'exemple ST.
  ******************************************************************************
  */
#include "main.h"
#include "cmsis_os2.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "lan8742.h"
#include "console.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Paramètres                                                                */
/* ------------------------------------------------------------------------- */
#define IFNAME0 's'
#define IFNAME1 't'

/* Taille d'un buffer de réception : DOIT être égale à heth1.Init.RxBuffLen,
 * fixée par CubeMX dans MX_ETH1_Init() (1524). Vérifié dans low_level_init. */
#define ETH_RX_BUFFER_SIZE        1524U
/* Nombre de buffers de réception : doit dépasser ETH_RX_DESC_CNT (4). */
#define ETH_RX_BUFFER_CNT         12U

#define TIME_WAITING_FOR_INPUT    osWaitForever
#define ETHIF_TX_TIMEOUT          2000U          /* ms */

/* Piles des tâches (en OCTETS). Le sujet (étape 2) recommande d'augmenter la
 * pile de la tâche de réception en CMSIS-RTOS v2 : 1024 voire 2048. */
#define INTERFACE_THREAD_STACK_SIZE   2048U
#define LINK_THREAD_STACK_SIZE        1536U
#define LINK_CHECK_PERIOD_MS          100U

/* ------------------------------------------------------------------------- */
/* Objets fournis par le code généré (main.c)                                */
/* ------------------------------------------------------------------------- */
extern ETH_HandleTypeDef  heth1;
extern ETH_TxPacketConfig TxConfig;

/* ------------------------------------------------------------------------- */
/* Buffers de réception "zero-copy"                                          */
/* ------------------------------------------------------------------------- */
typedef enum
{
  RX_ALLOC_OK    = 0x00,
  RX_ALLOC_ERROR = 0x01
} RxAllocStatusTypeDef;

/* Chaque élément du pool = une structure pbuf LwIP + le buffer du DMA. */
typedef struct
{
  struct pbuf_custom pbuf_custom;
  uint8_t buff[(ETH_RX_BUFFER_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;

LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool");

static volatile uint8_t RxAllocStatus = RX_ALLOC_OK;

static osSemaphoreId_t RxPktSemaphore = NULL;   /* "une trame est arrivée"      */
static osSemaphoreId_t TxPktSemaphore = NULL;   /* "une émission est terminée"  */

/* ------------------------------------------------------------------------- */
/* PHY LAN8742 (piloté par MDIO PF4 / MDC PG11)                              */
/* ------------------------------------------------------------------------- */
static int32_t ETH_PHY_IO_Init(void);
static int32_t ETH_PHY_IO_DeInit(void);
static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
static int32_t ETH_PHY_IO_GetTick(void);

static lan8742_Object_t LAN8742;
static lan8742_IOCtx_t  LAN8742_IOCtx = { ETH_PHY_IO_Init,
                                          ETH_PHY_IO_DeInit,
                                          ETH_PHY_IO_WriteReg,
                                          ETH_PHY_IO_ReadReg,
                                          ETH_PHY_IO_GetTick };
static uint8_t s_phy_ok = 0;

static void ethernetif_input(void *argument);
static void pbuf_free_custom(struct pbuf *p);

/* ========================================================================= */
/* Sécurité RIF (spécifique STM32N6)                                         */
/* ========================================================================= */
/**
  * Le projet est "FullSecure" : nos buffers sont en RAM Secure. Le DMA de
  * l'Ethernet est un "maître" du bus, comme le CPU : par défaut il n'a pas
  * le droit d'y accéder, et aucune trame ne passerait. On lui donne les
  * attributs Secure + Privileged (CID 1), comme RISAF_Config() dans l'exemple
  * ST. Écrit avec les registres pour ne pas dépendre du module HAL RIF, que
  * CubeMX désactive à chaque génération.
  */
void ethernetif_rif_config(void)
{
  const uint32_t eth1_master = 6U;   /* RIF_MASTER_INDEX_ETH1                  */
  const uint32_t eth1_reg    = 1U;   /* RIF_RISC_PERIPH_INDEX_ETH1 : registre 1 */
  const uint32_t eth1_bit    = 28U;  /*                             bit 28      */
  uint32_t attr;

  __HAL_RCC_RIFSC_CLK_ENABLE();

  /* Maître ETH1 (DMA) : CID 1, Secure, Privileged */
  attr  = RIFSC->RIMC_ATTRx[eth1_master];
  attr &= ~(RIFSC_RIMC_ATTRx_MCID | RIFSC_RIMC_ATTRx_MSEC | RIFSC_RIMC_ATTRx_MPRIV);
  attr |= (1UL << RIFSC_RIMC_ATTRx_MCID_Pos) | RIFSC_RIMC_ATTRx_MSEC | RIFSC_RIMC_ATTRx_MPRIV;
  RIFSC->RIMC_ATTRx[eth1_master] = attr;

  /* Esclave ETH1 (registres du périphérique) : Secure, Privileged */
  RIFSC->RISC_SECCFGRx[eth1_reg]  |= (1UL << eth1_bit);
  RIFSC->RISC_PRIVCFGRx[eth1_reg] |= (1UL << eth1_bit);
}

/* ========================================================================= */
/* LwIP -> ETH                                                               */
/* ========================================================================= */
static void low_level_init(struct netif *netif)
{
  /* Adresse MAC : celle choisie dans MX_ETH1_Init() (00:80:E1:00:00:00).
   * Attention : deux cartes sur le même réseau doivent avoir des MAC
   * différentes (changer le dernier octet dans CubeMX). */
  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, heth1.Init.MACAddr, ETH_HWADDR_LEN);
  netif->mtu   = ETH_MAX_PAYLOAD;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  if (heth1.Init.RxBuffLen != ETH_RX_BUFFER_SIZE)
  {
    console_lock();
    printf("  [ETH] ERREUR : RxBuffLen = %lu, ethernetif attend %lu\r\n",
           (unsigned long)heth1.Init.RxBuffLen, (unsigned long)ETH_RX_BUFFER_SIZE);
    console_unlock();
  }

  LWIP_MEMPOOL_INIT(RX_POOL);

  /* Sémaphores (compteur max 1, valeur initiale 0 = "rien à faire") */
  RxPktSemaphore = osSemaphoreNew(1, 0, NULL);
  TxPktSemaphore = osSemaphoreNew(1, 0, NULL);

  /* Tâche de réception : priorité haute pour vider vite les buffers DMA */
  const osThreadAttr_t attributes = {
    .name       = "EthIf",
    .stack_size = INTERFACE_THREAD_STACK_SIZE,
    .priority   = (osPriority_t)osPriorityRealtime,
  };
  osThreadNew(ethernetif_input, netif, &attributes);

  /* PHY : le driver LAN8742 cherche le composant sur les 32 adresses MDIO */
  LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
  if (LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK)
  {
    console_lock();
    printf("  [ETH] ERREUR : PHY LAN8742 introuvable (verifier MDIO=PF4 / MDC=PG11)\r\n");
    console_unlock();
    netif_set_link_down(netif);
    netif_set_down(netif);
    return;
  }
  s_phy_ok = 1;
  console_lock();
  printf("  [ETH] PHY LAN8742 detecte (adresse MDIO %lu)\r\n", (unsigned long)LAN8742.DevAddr);
  console_unlock();
}

/**
  * Émission d'une trame. La trame peut être découpée en plusieurs pbufs
  * chaînés : chaque morceau devient un buffer du descripteur DMA.
  * pbuf_ref() garde la trame en vie jusqu'à ce que le DMA l'ait envoyée ;
  * elle est libérée dans HAL_ETH_TxFreeCallback().
  */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  uint32_t i = 0U;
  struct pbuf *q;
  err_t errval = ERR_OK;
  ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];

  (void)netif;
  memset(Txbuffer, 0, sizeof(Txbuffer));

  for (q = p; q != NULL; q = q->next)
  {
    if (i >= ETH_TX_DESC_CNT)
    {
      return ERR_IF;
    }
    Txbuffer[i].buffer = q->payload;
    Txbuffer[i].len    = q->len;
    if (i > 0)
    {
      Txbuffer[i - 1].next = &Txbuffer[i];
    }
    Txbuffer[i].next = NULL;
    i++;
  }

  TxConfig.Length   = p->tot_len;
  TxConfig.TxBuffer = Txbuffer;
  TxConfig.pData    = p;

  pbuf_ref(p);

  for (;;)
  {
    if (HAL_ETH_Transmit_IT(&heth1, &TxConfig) == HAL_OK)
    {
      errval = ERR_OK;
      break;
    }
    if ((HAL_ETH_GetError(&heth1) & HAL_ETH_ERROR_BUSY) != 0U)
    {
      /* Plus de descripteur libre : on attend la fin d'une émission,
       * on récupère les descripteurs terminés, puis on réessaie. */
      osSemaphoreAcquire(TxPktSemaphore, ETHIF_TX_TIMEOUT);
      HAL_ETH_ReleaseTxPacket(&heth1);
      continue;
    }
    pbuf_free(p);
    errval = ERR_IF;
    break;
  }

  /* Récupère les descripteurs des émissions terminées entre-temps */
  HAL_ETH_ReleaseTxPacket(&heth1);
  return errval;
}

static struct pbuf *low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;

  (void)netif;
  if (RxAllocStatus == RX_ALLOC_OK)
  {
    HAL_ETH_ReadData(&heth1, (void **)&p);
  }
  return p;
}

/**
  * Tâche de réception : dort jusqu'à ce que l'interruption Ethernet signale
  * une trame, puis vide tous les buffers reçus et les donne à LwIP.
  * netif->input vaut tcpip_input : la trame est traitée par tcpip_thread.
  */
static void ethernetif_input(void *argument)
{
  struct netif *netif = (struct netif *)argument;
  struct pbuf *p;

  for (;;)
  {
    if (osSemaphoreAcquire(RxPktSemaphore, TIME_WAITING_FOR_INPUT) == osOK)
    {
      do
      {
        p = low_level_input(netif);
        if (p != NULL)
        {
          if (netif->input(p, netif) != ERR_OK)
          {
            pbuf_free(p);
          }
        }
      } while (p != NULL);
    }
  }
}

/**
  * Passée à netif_add() : appelée une fois par LwIP pour préparer l'interface.
  */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "ia-meteo";   /* nom visible dans la liste des appareils de la box */
#endif

  netif->name[0]    = IFNAME0;
  netif->name[1]    = IFNAME1;
  netif->output     = etharp_output;    /* IP -> résolution ARP -> trame */
  netif->linkoutput = low_level_output; /* trame -> DMA                  */

  low_level_init(netif);
  return ERR_OK;
}

/* Libération d'un buffer de réception quand LwIP n'en a plus besoin */
static void pbuf_free_custom(struct pbuf *p)
{
  struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;
  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

  /* Si le pool était vide, la réception était bloquée : on la relance. */
  if (RxAllocStatus == RX_ALLOC_ERROR)
  {
    RxAllocStatus = RX_ALLOC_OK;
    osSemaphoreRelease(RxPktSemaphore);
  }
}

/* Horloge en ms utilisée par les timeouts de LwIP (DHCP, ARP, TCP...) */
u32_t sys_now(void)
{
  return HAL_GetTick();
}

/* ========================================================================= */
/* Surveillance du câble                                                     */
/* ========================================================================= */
void ethernet_link_thread(void *argument)
{
  struct netif *netif = (struct netif *)argument;
  ETH_MACConfigTypeDef MACConf = {0};
  int32_t PHYLinkState;
  uint32_t linkchanged, speed = 0U, duplex = 0U;

  for (;;)
  {
    if (!s_phy_ok)
    {
      osDelay(1000);
      continue;
    }

    PHYLinkState = LAN8742_GetLinkState(&LAN8742);
    linkchanged = 0U;

    if (netif_is_link_up(netif) && (PHYLinkState <= LAN8742_STATUS_LINK_DOWN))
    {
      /* Câble débranché */
      HAL_ETH_Stop_IT(&heth1);
      LOCK_TCPIP_CORE();
      netif_set_down(netif);
      netif_set_link_down(netif);
      UNLOCK_TCPIP_CORE();
    }
    else if (!netif_is_link_up(netif) && (PHYLinkState > LAN8742_STATUS_LINK_DOWN))
    {
      /* Câble branché : on règle le MAC sur la vitesse négociée par le PHY */
      switch (PHYLinkState)
      {
        case LAN8742_STATUS_100MBITS_FULLDUPLEX:
          duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; linkchanged = 1U; break;
        case LAN8742_STATUS_100MBITS_HALFDUPLEX:
          duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_100M; linkchanged = 1U; break;
        case LAN8742_STATUS_10MBITS_FULLDUPLEX:
          duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_10M;  linkchanged = 1U; break;
        case LAN8742_STATUS_10MBITS_HALFDUPLEX:
          duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_10M;  linkchanged = 1U; break;
        default:
          break;
      }

      if (linkchanged)
      {
        HAL_ETH_GetMACConfig(&heth1, &MACConf);
        MACConf.DuplexMode = duplex;
        MACConf.Speed      = speed;
        HAL_ETH_SetMACConfig(&heth1, &MACConf);
        HAL_ETH_Start_IT(&heth1);

        LOCK_TCPIP_CORE();
        netif_set_up(netif);
        netif_set_link_up(netif);   /* -> callback de lien (app_ethernet.c) */
        UNLOCK_TCPIP_CORE();
      }
    }

    osDelay(LINK_CHECK_PERIOD_MS);
  }
}

/* ========================================================================= */
/* Callbacks de la HAL ETH                                                   */
/* ========================================================================= */
/* Interruption "trame reçue" : on réveille la tâche EthIf (rien d'autre :
 * une interruption doit être courte). Priorité ETH1 = 15, donc >= 5
 * (configMAX_SYSCALL_INTERRUPT_PRIORITY) : on a le droit d'appeler FreeRTOS. */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
  (void)heth;
  if (RxPktSemaphore != NULL)
  {
    osSemaphoreRelease(RxPktSemaphore);
  }
}

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
  (void)heth;
  if (TxPktSemaphore != NULL)
  {
    osSemaphoreRelease(TxPktSemaphore);
  }
}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
  /* "Receive Buffer Unavailable" : plus de buffer libre, on relance la tâche */
  if (((HAL_ETH_GetDMAError(heth) & ETH_DMACxSR_RBU) == ETH_DMACxSR_RBU) && (RxPktSemaphore != NULL))
  {
    osSemaphoreRelease(RxPktSemaphore);
  }
}

/* La HAL demande un buffer vide pour un descripteur de réception */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
  if (p)
  {
    /* Le buffer est juste après la structure pbuf (voir RxBuff_t) */
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    p->custom_free_function = pbuf_free_custom;
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
  }
  else
  {
    RxAllocStatus = RX_ALLOC_ERROR;
    *buff = NULL;
  }
}

/* La HAL a rempli un buffer : on le chaîne au paquet en cours de réception */
void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd   = (struct pbuf **)pEnd;
  struct pbuf *p;

  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  p->next    = NULL;
  p->tot_len = 0;
  p->len     = Length;

  if (!*ppStart)
  {
    *ppStart = p;              /* premier buffer du paquet */
  }
  else
  {
    (*ppEnd)->next = p;        /* buffer suivant */
  }
  *ppEnd = p;

  /* tot_len de chaque pbuf = sa longueur + celle des suivants */
  for (p = *ppStart; p != NULL; p = p->next)
  {
    p->tot_len += Length;
  }
}

/* Le DMA a fini d'envoyer une trame : on rend la référence prise dans
 * low_level_output() */
void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
  pbuf_free((struct pbuf *)buff);
}

/* ========================================================================= */
/* Accès au PHY par MDIO                                                     */
/* ========================================================================= */
static int32_t ETH_PHY_IO_Init(void)
{
  HAL_ETH_SetMDIOClockRange(&heth1);   /* MDC <= 2,5 MHz selon l'horloge du MAC */
  return 0;
}

static int32_t ETH_PHY_IO_DeInit(void)
{
  return 0;
}

static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
  return (HAL_ETH_ReadPHYRegister(&heth1, DevAddr, RegAddr, pRegVal) == HAL_OK) ? 0 : -1;
}

static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
  return (HAL_ETH_WritePHYRegister(&heth1, DevAddr, RegAddr, RegVal) == HAL_OK) ? 0 : -1;
}

static int32_t ETH_PHY_IO_GetTick(void)
{
  return (int32_t)HAL_GetTick();
}
