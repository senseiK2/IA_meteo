/**
  ******************************************************************************
  * @file    meteo_ia.h
  * @brief   Inférence des modèles météo sur la carte (TP3 partie 3).
  *
  * Deux réseaux de neurones, de même architecture (8 -> 16 -> 16 -> 4) :
  *   METEO_IA_H0 : le temps qu'il fait maintenant
  *   METEO_IA_H3 : le temps qu'il fera dans 3 heures
  *
  * Chacun est le code C généré par X-CUBE-AI (ST Edge AI Core) à partir de
  * son fichier ONNX (ml/models/export/modele_meteo_hX.onnx) : dossier
  * Appli/X-CUBE-AI/App. Les deux partagent la même bibliothèque
  * d'exécution. Ce module les enveloppe :
  *   - calcul des 8 entrées à partir des mesures et de l'heure ;
  *   - normalisation (moyennes / écarts-types propres à chaque modèle) ;
  *   - appel du réseau et mesure du temps de calcul.
  ******************************************************************************
  */
#ifndef METEO_IA_H
#define METEO_IA_H

#include <stdbool.h>
#include <stdint.h>

/* Tailles des modèles (vérifiées contre meteo_modele_hX.h dans meteo_ia.c ;
 * ces fichiers-là ne sont inclus qu'à un seul endroit car ils définissent
 * des tableaux) */
#define METEO_IA_NB_ENTREES      8
#define METEO_IA_NB_CLASSES      4
#define METEO_IA_E_PRES          2   /* indice de la pression (niveau de la mer) */
#define METEO_IA_E_ECART_ROSEE   3   /* indice de l'écart au point de rosée      */

typedef enum
{
  METEO_IA_H0 = 0,      /* maintenant   */
  METEO_IA_H3,          /* dans 3 h     */
  METEO_IA_NB_MODELES
} meteo_ia_modele_t;

/* Crée et initialise les deux réseaux. À appeler une fois, avant le reste. */
bool meteo_ia_init(void);

/**
  * Calcule les 8 entrées (communes aux deux modèles) à partir des mesures de
  * la carte et de l'heure, avec les MÊMES formules que ml/meteo_features.py
  * et matlab/prediction_meteo.m : pression ramenée au niveau de la mer,
  * écart au point de rosée (Magnus), heure et jour de l'année placés sur un
  * cercle (sinus, cosinus).
  * @param epoch_utc  heure UTC en secondes depuis le 01/01/1970
  */
void meteo_ia_calculer_entrees(float temp_c, float humidite_rh,
                               float pression_hpa, uint32_t epoch_utc,
                               float entrees[METEO_IA_NB_ENTREES]);

/**
  * Fait une prédiction avec l'un des modèles.
  * @param entrees  les 8 entrées BRUTES (avant normalisation)
  * @param proba    reçoit les 4 probabilités (Clair, Nuageux, Brouillard,
  *                 Precipitations)
  * @param cycles   reçoit le nombre de cycles processeur du seul calcul du
  *                 réseau (ou NULL)
  * @return l'indice de la classe la plus probable, -1 en cas d'erreur
  */
int meteo_ia_predire(meteo_ia_modele_t modele,
                     const float entrees[METEO_IA_NB_ENTREES],
                     float proba[METEO_IA_NB_CLASSES], uint32_t *cycles);

/* Nom d'une classe ("Clair"...) et d'un modèle ("h0", "h3") */
const char *meteo_ia_classe(int k);
const char *meteo_ia_nom(meteo_ia_modele_t modele);

/* Convertit des cycles processeur en microsecondes */
uint32_t meteo_ia_cycles_en_us(uint32_t cycles);

/**
  * Auto-test d'un modèle : passe ses vecteurs de test exportés par
  * ml/04_exporter.py (vraies heures de 2025 dont Python connaît la réponse)
  * et vérifie que la carte trouve la même classe. Affiche le détail.
  * @return nombre de vecteurs dont la classe est identique à Python
  */
int meteo_ia_autotest(meteo_ia_modele_t modele);

#endif /* METEO_IA_H */
