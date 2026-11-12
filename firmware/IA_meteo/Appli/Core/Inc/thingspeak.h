/**
  ******************************************************************************
  * @file    thingspeak.h
  * @brief   Envoi des mesures sur un canal ThingSpeak (API REST, HTTP).
  *          TP3 partie 2.
  ******************************************************************************
  */
#ifndef THINGSPEAK_H
#define THINGSPEAK_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  TS_OK = 0,          /* mesures enregistrées dans le canal                */
  TS_ERR_DNS,         /* api.thingspeak.com introuvable                    */
  TS_ERR_SOCKET,      /* impossible de créer la socket                     */
  TS_ERR_CONNEXION,   /* le serveur n'a pas accepté la connexion TCP       */
  TS_ERR_ENVOI,       /* la requête n'a pas pu partir                      */
  TS_ERR_REPONSE,     /* pas de réponse, ou réponse HTTP illisible         */
  TS_ERR_REFUS,       /* réponse "0" : clé API fausse, ou envoi trop
                         rapproché (compte gratuit : 15 s minimum)         */
  TS_VIDE             /* TalkBack : aucune commande en attente             */
} ts_resultat_t;

/**
  * Ajoute une ligne au canal : field1 = température, field2 = humidité,
  * field3 = pression. Fonction bloquante (DNS + TCP + HTTP) : quelques
  * centaines de ms, jusqu'à quelques secondes si le réseau est lent.
  *
  * @param cle_ecriture  Write API Key du canal (voir secrets.h)
  * @param temp_c        température (°C)
  * @param humidite_rh   humidité relative (%)
  * @param pression_hpa  pression mesurée par la carte (hPa)
  * @param numero        reçoit le numéro de la ligne créée (ou NULL)
  */
ts_resultat_t thingspeak_envoyer(const char *cle_ecriture,
                                 float temp_c, float humidite_rh,
                                 float pression_hpa, int32_t *numero);

/**
  * Récupère (et retire) la plus ancienne commande de la file TalkBack.
  * MATLAB y dépose la prédiction, par exemple "h0=Nuageux,64;h3=Pluie,55".
  *
  * @param id        identifiant de la TalkBack (voir secrets.h)
  * @param cle       API Key de la TalkBack (différente de celle du canal)
  * @param commande  reçoit le texte de la commande
  * @param taille    taille du tampon commande
  * @return TS_OK, TS_VIDE si la file est vide, ou une erreur réseau
  */
ts_resultat_t thingspeak_talkback_lire(const char *id, const char *cle,
                                       char *commande, size_t taille);

/* Texte lisible pour un résultat (pour les logs) */
const char *thingspeak_texte(ts_resultat_t r);

#endif /* THINGSPEAK_H */
