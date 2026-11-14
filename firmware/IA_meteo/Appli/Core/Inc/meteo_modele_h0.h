/**
  * @file    meteo_modele_h0.h
  * @brief   Paramètres du modèle météo, générés par ml/04_exporter.py.
  *          NE PAS MODIFIER À LA MAIN : relancer le script.
  *          Modèle : 4_classes_S_temps_h0 (2026-11-06 08:31)
  *
  * Avant d'appeler le réseau, chaque entrée doit être :
  *   1. calculée comme dans ml/meteo_features.py (même ordre !)
  *   2. normalisée : x_norm[i] = (x[i] - METEO_H0_MOYENNE[i]) / METEO_H0_ECART_TYPE[i]
  * La sortie i du réseau est la probabilité de METEO_H0_CLASSES[i].
  */
#ifndef METEO_H0_MODELE_H
#define METEO_H0_MODELE_H

#define METEO_H0_NB_ENTREES   8
#define METEO_H0_NB_CLASSES   4
#define METEO_H0_HORIZON_H    0
#define METEO_H0_ALTITUDE_M   235.0f   /* altitude de la carte : pression -> niveau de la mer */

/* Ordre des entrées : */
#define METEO_H0_E_TEMP         0
#define METEO_H0_E_RHUM         1
#define METEO_H0_E_PRES         2
#define METEO_H0_E_ECART_ROSEE  3
#define METEO_H0_E_HEURE_SIN    4
#define METEO_H0_E_HEURE_COS    5
#define METEO_H0_E_JOUR_SIN     6
#define METEO_H0_E_JOUR_COS     7

static const float METEO_H0_MOYENNE[METEO_H0_NB_ENTREES]    = { 12.377593f, 76.479042f, 1018.075f, 4.6177049f, 0.0054623443f, 0.0070936289f, -0.035263691f, 0.027736975f };
static const float METEO_H0_ECART_TYPE[METEO_H0_NB_ENTREES] = { 8.280983f, 18.073133f, 7.9990473f, 4.3288283f, 0.70747614f, 0.70664883f, 0.70560318f, 0.70716858f };

static const char *const METEO_H0_CLASSES[METEO_H0_NB_CLASSES] = { "Clair", "Nuageux", "Brouillard", "Precipitations" };

/* Vecteurs de test : entrées brutes (avant normalisation) et classe
   attendue. La carte doit retrouver la même classe (auto-test). */
#define METEO_H0_NB_VECTEURS_TEST 12
static const float METEO_H0_TEST_ENTREES[METEO_H0_NB_VECTEURS_TEST][METEO_H0_NB_ENTREES] = {
  { 3.3000002f, 90.0f, 1022.3f, 1.4844959f, 0.70710677f, 0.70710677f, 0.15420404f, 0.98803902f },
  { 2.0f, 80.0f, 1029.0f, 3.0903318f, 0.96592575f, -0.25881904f, 0.28829065f, 0.95754296f },
  { 6.0f, 81.0f, 1024.0f, 3.0158436f, 0.96592575f, -0.25881904f, 0.50835645f, 0.86114675f },
  { 32.0f, 30.0f, 1009.0f, 19.746414f, -0.96592587f, -0.25881904f, -0.16057454f, -0.98702377f },
  { 18.0f, 94.0f, 1016.0f, 0.98115301f, -0.25881904f, 0.96592581f, -0.4071295f, -0.91337043f },
  { 23.0f, 65.0f, 1021.0f, 6.9361324f, 0.86602539f, -0.49999997f, -0.58569574f, -0.81053096f },
  { 18.0f, 100.0f, 1011.0f, 0.0f, 0.96592575f, 0.25881904f, -0.73939228f, -0.67327487f },
  { 16.0f, 94.0f, 1014.0f, 0.96620846f, -0.96592587f, 0.25881904f, -0.86169285f, -0.50743026f },
  { 11.0f, 94.0f, 1027.0f, 0.92934704f, 1.0f, 0.0f, -0.97533637f, -0.22072366f },
  { 13.0f, 94.0f, 1007.0f, 0.9440062f, 0.0f, -1.0f, -0.95211405f, 0.30574307f },
  { 7.0f, 87.0f, 1018.0f, 2.0173721f, -0.70710677f, 0.70710677f, -0.7931897f, 0.60897464f },
  { 7.0f, 100.0f, 1020.0f, 0.0f, 0.0f, 0.99999994f, -0.2091735f, 0.97787857f },
};
static const unsigned char METEO_H0_TEST_CLASSE[METEO_H0_NB_VECTEURS_TEST] = { 2, 1, 1, 0, 1, 0, 3, 3, 0, 3, 1, 2 };

#endif /* METEO_H0_MODELE_H */
