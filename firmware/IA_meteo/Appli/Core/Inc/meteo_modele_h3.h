/**
  * @file    meteo_modele_h3.h
  * @brief   Paramètres du modèle météo, générés par ml/04_exporter.py.
  *          NE PAS MODIFIER À LA MAIN : relancer le script.
  *          Modèle : 4_classes_S_temps_h3 (2026-11-06 08:31)
  *
  * Avant d'appeler le réseau, chaque entrée doit être :
  *   1. calculée comme dans ml/meteo_features.py (même ordre !)
  *   2. normalisée : x_norm[i] = (x[i] - METEO_H3_MOYENNE[i]) / METEO_H3_ECART_TYPE[i]
  * La sortie i du réseau est la probabilité de METEO_H3_CLASSES[i].
  */
#ifndef METEO_H3_MODELE_H
#define METEO_H3_MODELE_H

#define METEO_H3_NB_ENTREES   8
#define METEO_H3_NB_CLASSES   4
#define METEO_H3_HORIZON_H    3
#define METEO_H3_ALTITUDE_M   235.0f   /* altitude de la carte : pression -> niveau de la mer */

/* Ordre des entrées : */
#define METEO_H3_E_TEMP         0
#define METEO_H3_E_RHUM         1
#define METEO_H3_E_PRES         2
#define METEO_H3_E_ECART_ROSEE  3
#define METEO_H3_E_HEURE_SIN    4
#define METEO_H3_E_HEURE_COS    5
#define METEO_H3_E_JOUR_SIN     6
#define METEO_H3_E_JOUR_COS     7

static const float METEO_H3_MOYENNE[METEO_H3_NB_ENTREES]    = { 12.41647f, 76.344131f, 1018.0587f, 4.6410284f, -0.0010959192f, 0.0083077215f, -0.035297748f, 0.027493816f };
static const float METEO_H3_ECART_TYPE[METEO_H3_NB_ENTREES] = { 8.2591934f, 18.007902f, 7.9947424f, 4.3141775f, 0.70496768f, 0.70927846f, 0.70559394f, 0.70719939f };

static const char *const METEO_H3_CLASSES[METEO_H3_NB_CLASSES] = { "Clair", "Nuageux", "Brouillard", "Precipitations" };

/* Vecteurs de test : entrées brutes (avant normalisation) et classe
   attendue. La carte doit retrouver la même classe (auto-test). */
#define METEO_H3_NB_VECTEURS_TEST 12
static const float METEO_H3_TEST_ENTREES[METEO_H3_NB_VECTEURS_TEST][METEO_H3_NB_ENTREES] = {
  { 4.0f, 88.0f, 1022.6f, 1.8090167f, 0.0f, 1.0f, 0.15420404f, 0.98803902f },
  { -1.0f, 100.0f, 1028.0f, 0.0f, 0.86602539f, 0.5f, 0.28829065f, 0.95754296f },
  { 6.0f, 81.0f, 1024.0f, 3.0158436f, 0.86602539f, 0.5f, 0.50835645f, 0.86114669f },
  { 31.0f, 36.0f, 1010.0f, 16.821112f, -0.5f, -0.86602533f, -0.16057454f, -0.98702371f },
  { 18.0f, 94.0f, 1016.0f, 0.98115301f, -0.86602545f, 0.5f, -0.4071295f, -0.91337049f },
  { 15.0f, 94.0f, 1021.0f, 0.9587791f, 0.96592581f, 0.25881904f, -0.58569574f, -0.81053102f },
  { 18.0f, 94.0f, 1010.0f, 0.98115301f, -1.0f, 0.0f, -0.73939228f, -0.67327487f },
  { 17.0f, 94.0f, 1012.0f, 0.97366667f, -0.86602545f, -0.49999997f, -0.86169285f, -0.50743026f },
  { 12.0f, 100.0f, 1026.0f, 0.0f, 0.70710677f, 0.70710677f, -0.97533637f, -0.22072364f },
  { 13.0f, 94.0f, 1008.0f, 0.94400597f, 0.70710677f, -0.70710677f, -0.95211405f, 0.30574307f },
  { 8.0f, 87.0f, 1018.0f, 2.0334702f, -0.96592581f, 0.25881904f, -0.7931897f, 0.60897464f },
  { 7.0f, 100.0f, 1022.0f, 0.0f, -0.70710677f, 0.70710677f, -0.22596361f, 0.97413582f },
};
static const unsigned char METEO_H3_TEST_CLASSE[METEO_H3_NB_VECTEURS_TEST] = { 1, 2, 1, 0, 1, 0, 3, 3, 0, 3, 1, 2 };

#endif /* METEO_H3_MODELE_H */
