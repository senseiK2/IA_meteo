/**
  ******************************************************************************
  * @file    thingspeak.h
  * @brief   Envoi des mesures sur un canal ThingSpeak (API REST, HTTP).
  *          TP3 partie 2.
  ******************************************************************************
  */
#ifndef THINGSPEAK_H
#define THINGSPEAK_H

#include <stdint.h>

typedef enum
{
  TS_OK = 0,          /* mesures enregistrées dans le canal                */
  TS_ERR_DNS,         /* api.thingspeak.com introuvable                    */
  TS_ERR_SOCKET,      /* impossible de créer la socket                     */
  TS_ERR_CONNEXION,   /* le serveur n'a pas accepté la connexion TCP       */
  TS_ERR_ENVOI,       /* la requête n'a pas pu partir                      */
  TS_ERR_REPONSE,     /* pas de réponse, ou réponse HTTP illisible         */
  TS_ERR_REFUS        /* réponse "0" : clé API fausse, ou envoi trop
                         rapproché (compte gratuit : 15 s minimum)         */
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

/* Texte lisible pour un résultat (pour les logs) */
const char *thingspeak_texte(ts_resultat_t r);

#endif /* THINGSPEAK_H */
