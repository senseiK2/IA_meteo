/**
  ******************************************************************************
  * @file    thingspeak.c
  * @brief   Envoi des mesures sur un canal ThingSpeak (API REST, HTTP).
  *          TP3 partie 2.
  *
  * La carte fait exactement ce que fait un navigateur quand on colle
  *   http://api.thingspeak.com/update?api_key=CLE&field1=21.6&field2=58.3&field3=988.1
  * dans la barre d'adresse :
  *
  *   1. DNS   : "api.thingspeak.com" -> adresse IP (comme pour le ping)
  *   2. TCP   : connexion au port 80 du serveur (socket SOCK_STREAM)
  *   3. HTTP  : envoi de la requête, un simple texte :
  *                GET /update?api_key=...&field1=...&field2=...&field3=... HTTP/1.0
  *                Host: api.thingspeak.com
  *                (ligne vide)
  *   4. Réponse : le serveur renvoie des en-têtes puis, après une ligne vide,
  *                le numéro de la ligne créée ("0" = refusé).
  *
  * HTTP/1.0 plutôt que 1.1 : le serveur ferme la connexion après sa réponse
  * et n'utilise pas l'encodage "chunked" ; on lit donc tout jusqu'à la
  * fermeture et la réponse est simple à analyser.
  *
  * HTTP et non HTTPS : le chiffrement TLS (mbedTLS) demanderait beaucoup de
  * mémoire et de configuration. En contrepartie, la clé API circule en clair
  * sur le réseau : acceptable pour un TP, pas pour un produit.
  ******************************************************************************
  */
#include "thingspeak.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/api.h"          /* netconn_gethostbyname : requête DNS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_HOTE        "api.thingspeak.com"
#define TS_PORT        80
#define TS_TIMEOUT_MS  5000U    /* attente maximum pour envoyer / recevoir */

/* Tampons statiques : ils seraient trop gros pour la pile de la tâche. Une
 * seule tâche (TacheReseau) appelle thingspeak_envoyer : pas de conflit. */
static char s_requete[256];
static char s_reponse[1024];

const char *thingspeak_texte(ts_resultat_t r)
{
  switch (r)
  {
    case TS_OK:            return "OK";
    case TS_ERR_DNS:       return "echec DNS (" TS_HOTE " introuvable)";
    case TS_ERR_SOCKET:    return "echec creation socket";
    case TS_ERR_CONNEXION: return "connexion TCP refusee ou impossible";
    case TS_ERR_ENVOI:     return "echec envoi de la requete";
    case TS_ERR_REPONSE:   return "pas de reponse ou reponse illisible";
    case TS_ERR_REFUS:     return "refuse par ThingSpeak (cle API ? envoi trop rapproche ?)";
    default:               return "?";
  }
}

/* Écrit v avec 2 décimales ("-3.25", "988.10"). On n'utilise pas %f :
 * printf avec float n'est pas activé dans newlib-nano (même choix que
 * sensors.c). */
static void flottant_texte(float v, char *buf, size_t len)
{
  int32_t centi = (int32_t)(v * 100.0f + ((v >= 0.0f) ? 0.5f : -0.5f));
  const char *signe = (centi < 0) ? "-" : "";
  if (centi < 0)
  {
    centi = -centi;
  }
  snprintf(buf, len, "%s%ld.%02ld", signe, (long)(centi / 100), (long)(centi % 100));
}

ts_resultat_t thingspeak_envoyer(const char *cle_ecriture,
                                 float temp_c, float humidite_rh,
                                 float pression_hpa, int32_t *numero)
{
  ip_addr_t serveur;
  int s;
  ts_resultat_t res;

  /* ---------------------------------------------------------------------
   * 1. DNS : nom -> adresse IP
   * ------------------------------------------------------------------- */
  if (netconn_gethostbyname(TS_HOTE, &serveur) != ERR_OK)
  {
    return TS_ERR_DNS;
  }

  /* ---------------------------------------------------------------------
   * 2. Socket TCP (SOCK_STREAM) avec délais d'envoi et de réception :
   *    sans eux, un serveur muet bloquerait la tâche indéfiniment.
   * ------------------------------------------------------------------- */
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
  {
    return TS_ERR_SOCKET;
  }
  struct timeval tv;
  tv.tv_sec  = (long)(TS_TIMEOUT_MS / 1000U);
  tv.tv_usec = 0;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  struct sockaddr_in adr;
  memset(&adr, 0, sizeof(adr));
  adr.sin_len    = sizeof(adr);
  adr.sin_family = AF_INET;
  adr.sin_port   = lwip_htons(TS_PORT);       /* ordre réseau (big endian) */
  inet_addr_from_ip4addr(&adr.sin_addr, ip_2_ip4(&serveur));

  if (connect(s, (struct sockaddr *)&adr, sizeof(adr)) < 0)
  {
    closesocket(s);
    return TS_ERR_CONNEXION;
  }

  /* ---------------------------------------------------------------------
   * 3. Requête HTTP. Chaque ligne se termine par "\r\n" ; une ligne vide
   *    marque la fin des en-têtes.
   * ------------------------------------------------------------------- */
  char t[16], h[16], p[16];
  flottant_texte(temp_c, t, sizeof(t));
  flottant_texte(humidite_rh, h, sizeof(h));
  flottant_texte(pression_hpa, p, sizeof(p));

  int n = snprintf(s_requete, sizeof(s_requete),
                   "GET /update?api_key=%s&field1=%s&field2=%s&field3=%s HTTP/1.0\r\n"
                   "Host: " TS_HOTE "\r\n"
                   "\r\n",
                   cle_ecriture, t, h, p);
  if ((n <= 0) || (n >= (int)sizeof(s_requete)))
  {
    closesocket(s);
    return TS_ERR_ENVOI;
  }

  /* send() peut n'envoyer qu'une partie : on boucle jusqu'au bout */
  for (int envoye = 0; envoye < n; )
  {
    int r = send(s, s_requete + envoye, (size_t)(n - envoye), 0);
    if (r <= 0)
    {
      closesocket(s);
      return TS_ERR_ENVOI;
    }
    envoye += r;
  }

  /* ---------------------------------------------------------------------
   * 4. Réponse : on lit jusqu'à ce que le serveur ferme (recv renvoie 0)
   *    ou que le tampon soit plein. Exemple de réponse :
   *      HTTP/1.1 200 OK
   *      Content-Type: text/plain; charset=utf-8
   *      ...
   *      (ligne vide)
   *      42                 <- numéro de la ligne créée, 0 si refusé
   * ------------------------------------------------------------------- */
  int total = 0;
  for (;;)
  {
    int r = recv(s, s_reponse + total, sizeof(s_reponse) - 1U - (size_t)total, 0);
    if (r <= 0)
    {
      break;              /* 0 : fermeture par le serveur ; <0 : timeout */
    }
    total += r;
    if (total >= (int)sizeof(s_reponse) - 1)
    {
      break;
    }
  }
  closesocket(s);
  s_reponse[total] = '\0';

  /* Ligne de statut : "HTTP/1.x 200 ..." */
  const char *corps = strstr(s_reponse, "\r\n\r\n");
  if ((total == 0) || (strncmp(s_reponse, "HTTP/1.", 7) != 0) ||
      (strncmp(s_reponse + 9, "200", 3) != 0) || (corps == NULL))
  {
    return TS_ERR_REPONSE;
  }

  long num = strtol(corps + 4, NULL, 10);
  if (numero != NULL)
  {
    *numero = (int32_t)num;
  }
  res = (num > 0) ? TS_OK : TS_ERR_REFUS;
  return res;
}
