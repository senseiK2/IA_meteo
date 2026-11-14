/**
  ******************************************************************************
  * @file    thingspeak.c
  * @brief   Échanges avec ThingSpeak par son API REST (HTTP). TP3 partie 2.
  *
  *   - thingspeak_envoyer()       : ajoute les mesures au canal
  *   - thingspeak_talkback_lire() : récupère la prédiction déposée par MATLAB
  *                                  dans la file TalkBack
  *
  * Les deux font exactement ce que fait un navigateur avec une adresse du type
  *   http://api.thingspeak.com/update?api_key=CLE&field1=21.6&field2=58.3&field3=988.1
  *
  *   1. DNS   : "api.thingspeak.com" -> adresse IP (comme pour le ping)
  *   2. TCP   : connexion au port 80 du serveur (socket SOCK_STREAM)
  *   3. HTTP  : envoi de la requête, un simple texte :
  *                GET /update?api_key=...&field1=... HTTP/1.0
  *                Host: api.thingspeak.com
  *                (ligne vide)
  *   4. Réponse : des en-têtes puis, après une ligne vide, le corps :
  *                - /update : le numéro de la ligne créée ("0" = refusé)
  *                - TalkBack : le texte de la commande (vide si file vide)
  *
  * HTTP/1.0 plutôt que 1.1 : le serveur ferme la connexion après sa réponse
  * et n'utilise pas l'encodage "chunked" ; on lit donc tout jusqu'à la
  * fermeture et la réponse est simple à analyser.
  *
  * HTTP et non HTTPS : le chiffrement TLS (mbedTLS) demanderait beaucoup de
  * mémoire et de configuration. En contrepartie, les clés API circulent en
  * clair sur le réseau : acceptable pour un TP, pas pour un produit.
  ******************************************************************************
  */
#include "thingspeak.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/api.h"          /* netconn_gethostbyname : requête DNS */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmsis_os2.h"         /* osKernelGetTickCount : faire avancer l'heure */

#define TS_HOTE        "api.thingspeak.com"
#define TS_PORT        80
#define TS_TIMEOUT_MS  5000U    /* attente maximum pour envoyer / recevoir */

/* Heure UTC donnée par le serveur (en-tête HTTP "Date:"), et le tick
 * FreeRTOS (ms) au moment où on l'a reçue : l'heure actuelle s'en déduit. */
static uint32_t s_epoch_recu = 0U;      /* secondes depuis le 01/01/1970 UTC */
static uint32_t s_tick_recu  = 0U;
static bool     s_heure_connue = false;

/* Tampons statiques : ils seraient trop gros pour la pile de la tâche. Une
 * seule tâche (TacheReseau) utilise ce module : pas de conflit. */
static char s_requete[320];
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
    case TS_VIDE:          return "aucune commande en attente";
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

/* Nombre de jours entre le 01/01/1970 et une date (algorithme classique
 * "days from civil" de H. Hinnant, valable pour toutes les dates usuelles) */
static int32_t jours_depuis_1970(int32_t a, int32_t m, int32_t j)
{
  a -= (m <= 2) ? 1 : 0;
  int32_t ere = (a >= 0 ? a : a - 399) / 400;
  int32_t ae  = a - ere * 400;                                   /* [0, 399] */
  int32_t ja  = (153 * (m + ((m > 2) ? -3 : 9)) + 2) / 5 + j - 1; /* [0, 365] */
  int32_t je  = ae * 365 + ae / 4 - ae / 100 + ja;               /* [0, 146096] */
  return ere * 146097 + je - 719468;
}

/* Lit l'en-tête "Date: Sat, 14 Nov 2026 10:50:00 GMT" de la réponse HTTP.
 * Tout serveur web l'envoie : la carte n'a pas d'horloge réglée, elle
 * récupère l'heure ici (précision : la seconde). */
static void lire_date_http(const char *entetes)
{
  static const char mois_noms[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *d = strstr(entetes, "\r\nDate: ");
  if (d == NULL)
  {
    d = strstr(entetes, "\r\ndate: ");      /* en-têtes insensibles à la casse */
  }
  if (d == NULL)
  {
    return;
  }

  char mois[4] = "";
  int jour, annee, h, mi, se;
  if (sscanf(d + 8, "%*3s, %d %3s %d %d:%d:%d", &jour, mois, &annee, &h, &mi, &se) != 6)
  {
    return;
  }
  const char *pos = strstr(mois_noms, mois);
  if ((pos == NULL) || (mois[0] == '\0'))
  {
    return;
  }
  int m = (int)((pos - mois_noms) / 3) + 1;

  s_epoch_recu = (uint32_t)jours_depuis_1970(annee, m, jour) * 86400U
               + (uint32_t)(h * 3600 + mi * 60 + se);
  s_tick_recu = osKernelGetTickCount();
  s_heure_connue = true;
}

bool thingspeak_heure_utc(uint32_t *epoch)
{
  if (!s_heure_connue)
  {
    return false;
  }
  /* heure reçue + temps écoulé depuis (tick FreeRTOS en ms) */
  *epoch = s_epoch_recu + (osKernelGetTickCount() - s_tick_recu) / 1000U;
  return true;
}

/**
  * Fait une requête "GET chemin" et renvoie le corps de la réponse.
  * s_requete doit déjà contenir la requête complète (n octets).
  * @param corps  reçoit un pointeur vers le corps (dans s_reponse)
  */
static ts_resultat_t http_get(int n, const char **corps)
{
  ip_addr_t serveur;
  int s;

  /* --- 1. DNS : nom -> adresse IP -------------------------------------- */
  if (netconn_gethostbyname(TS_HOTE, &serveur) != ERR_OK)
  {
    return TS_ERR_DNS;
  }

  /* --- 2. Socket TCP (SOCK_STREAM) avec délais d'envoi et de réception :
   *        sans eux, un serveur muet bloquerait la tâche indéfiniment. --- */
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

  /* --- 3. Envoi : send() peut n'envoyer qu'une partie, on boucle -------- */
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

  /* --- 4. Réponse : on lit jusqu'à ce que le serveur ferme (recv renvoie
   *        0) ou que le tampon soit plein. Exemple :
   *          HTTP/1.1 200 OK
   *          Content-Type: text/plain; charset=utf-8
   *          ...
   *          (ligne vide)
   *          42                 <- le corps                              */
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

  /* Ligne de statut "HTTP/1.x 200 ..." puis ligne vide avant le corps */
  const char *fin_entetes = strstr(s_reponse, "\r\n\r\n");
  if ((total == 0) || (strncmp(s_reponse, "HTTP/1.", 7) != 0) ||
      (strncmp(s_reponse + 9, "200", 3) != 0) || (fin_entetes == NULL))
  {
    return TS_ERR_REPONSE;
  }
  lire_date_http(s_reponse);           /* au passage : mise à l'heure */
  *corps = fin_entetes + 4;
  return TS_OK;
}

ts_resultat_t thingspeak_envoyer_champs(const char *cle_ecriture,
                                       const float champs[TS_NB_CHAMPS],
                                       uint8_t masque, int32_t *numero)
{
  /* Construction de "GET /update?api_key=...&field1=21.65&field4=1.00..."
   * en n'ajoutant que les champs présents (bit i du masque = field i+1) */
  int n = snprintf(s_requete, sizeof(s_requete), "GET /update?api_key=%s", cle_ecriture);
  for (int i = 0; (i < TS_NB_CHAMPS) && (n > 0) && (n < (int)sizeof(s_requete)); i++)
  {
    if ((masque & (1U << i)) != 0U)
    {
      char v[16];
      flottant_texte(champs[i], v, sizeof(v));
      n += snprintf(s_requete + n, sizeof(s_requete) - (size_t)n, "&field%d=%s", i + 1, v);
    }
  }
  /* Fin de la requête. Chaque ligne se termine par "\r\n" ; une ligne vide
   * marque la fin des en-têtes. */
  if ((n > 0) && (n < (int)sizeof(s_requete)))
  {
    n += snprintf(s_requete + n, sizeof(s_requete) - (size_t)n,
                  " HTTP/1.0\r\nHost: " TS_HOTE "\r\n\r\n");
  }
  if ((n <= 0) || (n >= (int)sizeof(s_requete)))
  {
    return TS_ERR_ENVOI;
  }

  const char *corps;
  ts_resultat_t r = http_get(n, &corps);
  if (r != TS_OK)
  {
    return r;
  }

  long num = strtol(corps, NULL, 10);    /* numéro de la ligne, 0 = refusé */
  if (numero != NULL)
  {
    *numero = (int32_t)num;
  }
  return (num > 0) ? TS_OK : TS_ERR_REFUS;
}

ts_resultat_t thingspeak_envoyer(const char *cle_ecriture,
                                 float temp_c, float humidite_rh,
                                 float pression_hpa, int32_t *numero)
{
  const float champs[TS_NB_CHAMPS] = { temp_c, humidite_rh, pression_hpa };
  return thingspeak_envoyer_champs(cle_ecriture, champs, 0x07U, numero);  /* field1..3 */
}

ts_resultat_t thingspeak_talkback_lire(const char *id, const char *cle,
                                       char *commande, size_t taille)
{
  /* "execute" renvoie la plus ancienne commande de la file et la retire.
   * Sans extension (.json / .xml), la réponse est le texte brut. */
  int n = snprintf(s_requete, sizeof(s_requete),
                   "GET /talkbacks/%s/commands/execute?api_key=%s HTTP/1.0\r\n"
                   "Host: " TS_HOTE "\r\n"
                   "\r\n",
                   id, cle);
  if ((n <= 0) || (n >= (int)sizeof(s_requete)))
  {
    return TS_ERR_ENVOI;
  }

  const char *corps;
  ts_resultat_t r = http_get(n, &corps);
  if (r != TS_OK)
  {
    return r;
  }

  /* Copie du corps sans les espaces / retours à la ligne de début et fin */
  while (isspace((unsigned char)*corps))
  {
    corps++;
  }
  size_t len = strlen(corps);
  while ((len > 0U) && isspace((unsigned char)corps[len - 1U]))
  {
    len--;
  }
  if (len == 0U)
  {
    commande[0] = '\0';
    return TS_VIDE;                    /* file vide : pas encore de réponse */
  }
  if (len >= taille)
  {
    len = taille - 1U;
  }
  memcpy(commande, corps, len);
  commande[len] = '\0';
  return TS_OK;
}
