/**
  ******************************************************************************
  * @file    net_ping.h
  * @brief   Ping (ICMP echo) vers une machine désignée par son nom, avec les
  *          sockets BSD de LwIP. TP2 partie 3, étape c.
  ******************************************************************************
  */
#ifndef NET_PING_H
#define NET_PING_H

#include <stdint.h>

typedef enum
{
  PING_OK = 0,        /* réponse reçue                                   */
  PING_ERR_DNS,       /* le nom n'a pas pu être traduit en adresse IP    */
  PING_ERR_SOCKET,    /* impossible de créer la socket                   */
  PING_ERR_ENVOI,     /* la requête n'a pas pu partir                    */
  PING_ERR_TIMEOUT    /* pas de réponse dans le délai                    */
} ping_resultat_t;

/**
  * Envoie UN ping vers "nom" (ex : "google.com") et attend la réponse.
  * @param nom         nom de la machine (ou adresse IP écrite en texte)
  * @param timeout_ms  temps d'attente maximum de la réponse
  * @param ip_texte    reçoit l'adresse IP résolue (au moins 16 octets), ou NULL
  * @param rtt_ms      reçoit le temps aller-retour en ms, ou NULL
  */
ping_resultat_t net_ping(const char *nom, uint32_t timeout_ms,
                         char *ip_texte, uint32_t *rtt_ms);

/* Texte lisible pour un résultat (pour les logs) */
const char *net_ping_texte(ping_resultat_t r);

#endif /* NET_PING_H */
