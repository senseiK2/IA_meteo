/**
  ******************************************************************************
  * @file    meteo_ia.h
  * @brief   Inférence du modèle météo sur la carte (TP3 partie 3).
  *
  * Le réseau de neurones lui-même est le code C généré par X-CUBE-AI
  * (ST Edge AI Core) à partir de ml/models/export/modele_meteo_h0.onnx :
  * dossier Appli/X-CUBE-AI/App. Ce module l'enveloppe :
  *   - normalisation des entrées (moyennes / écarts-types du TP3.1) ;
  *   - appel du réseau ;
  *   - mesure du temps de calcul (compteur de cycles du processeur).
  ******************************************************************************
  */
#ifndef METEO_IA_H
#define METEO_IA_H

#include <stdbool.h>
#include <stdint.h>

/* Tailles du modèle (vérifiées contre meteo_modele_h0.h dans meteo_ia.c ;
 * ce fichier-là n'est inclus qu'à un seul endroit car il définit des
 * tableaux) */
#define METEO_IA_NB_ENTREES  8
#define METEO_IA_NB_CLASSES  4
#define METEO_IA_E_PRES          2   /* indice de la pression (niveau de la mer) */
#define METEO_IA_E_ECART_ROSEE   3   /* indice de l'écart au point de rosée      */

/* Crée et initialise le réseau. À appeler une fois, avant tout le reste. */
bool meteo_ia_init(void);

/**
  * Fait une prédiction.
  * @param entrees  les 8 entrées BRUTES (avant normalisation), dans l'ordre
  *                 de meteo_modele_h0.h : temp, rhum, pres (niveau de la
  *                 mer), ecart_rosee, heure_sin, heure_cos, jour_sin, jour_cos
  * @param proba    reçoit les 4 probabilités (Clair, Nuageux, Brouillard,
  *                 Precipitations)
  * @param cycles   reçoit le nombre de cycles processeur du seul calcul du
  *                 réseau (ou NULL)
  * @return l'indice de la classe la plus probable, -1 en cas d'erreur
  */
int meteo_ia_predire(const float entrees[METEO_IA_NB_ENTREES],
                     float proba[METEO_IA_NB_CLASSES], uint32_t *cycles);

/**
  * Calcule les 8 entrées du modèle à partir des mesures de la carte et de
  * l'heure, avec les MÊMES formules que ml/meteo_features.py et
  * matlab/prediction_meteo.m :
  *   pression ramenée au niveau de la mer, écart au point de rosée (Magnus),
  *   heure et jour de l'année placés sur un cercle (sinus, cosinus).
  * @param epoch_utc  heure UTC en secondes depuis le 01/01/1970
  */
void meteo_ia_calculer_entrees(float temp_c, float humidite_rh,
                               float pression_hpa, uint32_t epoch_utc,
                               float entrees[METEO_IA_NB_ENTREES]);

/* Nom d'une classe ("Clair"...) */
const char *meteo_ia_classe(int k);

/* Convertit des cycles processeur en microsecondes */
uint32_t meteo_ia_cycles_en_us(uint32_t cycles);

/**
  * Auto-test : passe les vecteurs de test exportés par ml/04_exporter.py
  * (vraies heures de 2025 dont Python connaît la réponse) et vérifie que la
  * carte trouve la même classe. Affiche le détail sur la console.
  * @return nombre de vecteurs dont la classe est identique à Python
  */
int meteo_ia_autotest(void);

#endif /* METEO_IA_H */
