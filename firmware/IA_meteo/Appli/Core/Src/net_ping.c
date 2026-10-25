/**
  ******************************************************************************
  * @file    net_ping.c
  * @brief   Ping (ICMP echo) vers une machine désignée par son nom, avec les
  *          sockets BSD de LwIP. TP2 partie 3, étape c.
  *
  * Un ping se fait en deux temps :
  *
  *   1. DNS  : "google.com" -> adresse IP. La carte pose la question au
  *             serveur DNS donné par le DHCP (ici la box, 192.168.1.1).
  *
  *   2. ICMP : on envoie un paquet "echo request" (type 8) à cette adresse,
  *             la machine répond par un "echo reply" (type 0). Le temps entre
  *             les deux est le RTT (Round Trip Time).
  *
  *        Paquet ICMP echo (8 octets d'en-tête + données) :
  *        +--------+--------+----------------+
  *        | type   | code   |   checksum     |
  *        +--------+--------+----------------+
  *        |   identifiant   |  n° de séquence|
  *        +-----------------+----------------+
  *        |   données (32 octets ici)        |
  *        +----------------------------------+
  *
  * ICMP n'est ni TCP ni UDP : on utilise une socket "RAW" (SOCK_RAW), qui
  * donne accès directement aux paquets IP d'un protocole (LWIP_RAW = 1).
  ******************************************************************************
  */
#include "net_ping.h"
#include "cmsis_os2.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/api.h"          /* netconn_gethostbyname : requête DNS */
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/prot/ip4.h"
#include <string.h>

#define PING_ID          0xAFAFU   /* identifiant de nos pings              */
#define PING_DATA_SIZE   32U       /* octets de données dans chaque ping    */

static uint16_t s_seq = 0;         /* numéro de séquence, +1 à chaque ping */

const char *net_ping_texte(ping_resultat_t r)
{
  switch (r)
  {
    case PING_OK:          return "OK";
    case PING_ERR_DNS:     return "echec DNS (nom introuvable)";
    case PING_ERR_SOCKET:  return "echec creation socket";
    case PING_ERR_ENVOI:   return "echec envoi";
    case PING_ERR_TIMEOUT: return "pas de reponse (timeout)";
    default:               return "?";
  }
}

ping_resultat_t net_ping(const char *nom, uint32_t timeout_ms,
                         char *ip_texte, uint32_t *rtt_ms)
{
  ip_addr_t cible;
  int s;
  ping_resultat_t res = PING_ERR_TIMEOUT;

  /* ---------------------------------------------------------------------
   * 1. DNS : nom -> adresse IP
   * netconn_gethostbyname() envoie la requête DNS et attend la réponse.
   * ------------------------------------------------------------------- */
  if (netconn_gethostbyname(nom, &cible) != ERR_OK)
  {
    return PING_ERR_DNS;
  }
  if (ip_texte != NULL)
  {
    ip4addr_ntoa_r(ip_2_ip4(&cible), ip_texte, 16);
  }

  /* ---------------------------------------------------------------------
   * 2. Socket RAW pour le protocole ICMP, avec un délai de réception
   * ------------------------------------------------------------------- */
  s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (s < 0)
  {
    return PING_ERR_SOCKET;
  }

  struct timeval tv;
  tv.tv_sec  = (long)(timeout_ms / 1000U);
  tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  /* ---------------------------------------------------------------------
   * 3. Construction du paquet "echo request"
   * ------------------------------------------------------------------- */
  uint8_t paquet[sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE];
  struct icmp_echo_hdr *echo = (struct icmp_echo_hdr *)paquet;

  s_seq++;
  ICMPH_TYPE_SET(echo, ICMP_ECHO);          /* type 8 : echo request */
  ICMPH_CODE_SET(echo, 0);
  echo->id    = PING_ID;
  echo->seqno = lwip_htons(s_seq);          /* ordre réseau (big endian) */
  for (uint32_t i = 0; i < PING_DATA_SIZE; i++)
  {
    paquet[sizeof(struct icmp_echo_hdr) + i] = (uint8_t)i;
  }

  /* Checksum : avec CHECKSUM_BY_HARDWARE (lwipopts.h), c'est le MAC
   * Ethernet qui le calcule à l'émission ; il attend un champ à 0.
   * Si on le calculait aussi en logiciel ET que le MAC le recalculait par
   * dessus, on aurait le "CRC fail" dont parle l'étape 4 du sujet. */
  echo->chksum = 0;
#if CHECKSUM_GEN_ICMP
  echo->chksum = inet_chksum(echo, sizeof(paquet));
#endif

  /* ---------------------------------------------------------------------
   * 4. Envoi
   * ------------------------------------------------------------------- */
  struct sockaddr_in to;
  memset(&to, 0, sizeof(to));
  to.sin_len    = sizeof(to);
  to.sin_family = AF_INET;
  inet_addr_from_ip4addr(&to.sin_addr, ip_2_ip4(&cible));

  uint32_t t_envoi = osKernelGetTickCount();
  if (sendto(s, paquet, sizeof(paquet), 0, (struct sockaddr *)&to, sizeof(to)) < 0)
  {
    closesocket(s);
    return PING_ERR_ENVOI;
  }

  /* ---------------------------------------------------------------------
   * 5. Réception : une socket RAW reçoit TOUS les paquets ICMP (y compris
   * d'autres échanges) ; on garde seulement la réponse à NOTRE ping
   * (même identifiant, même numéro de séquence).
   * ------------------------------------------------------------------- */
  uint8_t buf[128];
  struct sockaddr_in from;
  socklen_t fromlen = sizeof(from);
  int len;

  while ((len = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen)) > 0)
  {
    /* Une socket RAW IPv4 rend le paquet IP complet : on saute l'en-tête IP */
    struct ip_hdr *iph = (struct ip_hdr *)buf;
    uint16_t ip_len = (uint16_t)IPH_HL_BYTES(iph);

    if (len >= (int)(ip_len + sizeof(struct icmp_echo_hdr)))
    {
      struct icmp_echo_hdr *rep = (struct icmp_echo_hdr *)(buf + ip_len);
      if ((ICMPH_TYPE(rep) == ICMP_ER) &&            /* type 0 : echo reply */
          (rep->id == PING_ID) &&
          (rep->seqno == lwip_htons(s_seq)))
      {
        if (rtt_ms != NULL)
        {
          *rtt_ms = osKernelGetTickCount() - t_envoi;
        }
        res = PING_OK;
        break;
      }
    }
    /* Ce n'était pas notre réponse : on continue d'écouter. Le délai
     * SO_RCVTIMEO s'applique à chaque recvfrom(). */
    if ((osKernelGetTickCount() - t_envoi) > timeout_ms)
    {
      break;
    }
  }

  closesocket(s);
  return res;
}
