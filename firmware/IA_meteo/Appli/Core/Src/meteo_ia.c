/**
  ******************************************************************************
  * @file    meteo_ia.c
  * @brief   Inférence du modèle météo sur la carte (TP3 partie 3).
  *
  * Chaîne complète :
  *   TensorFlow (ml/02_entrainer.py) -> ONNX (ml/04_exporter.py)
  *   -> code C (X-CUBE-AI / ST Edge AI Core, commande "stedgeai generate")
  *   -> ce fichier, qui appelle le réseau généré.
  *
  * Le réseau tourne sur le processeur Cortex-M55 (bibliothèque
  * NetworkRuntime de X-CUBE-AI), en calcul flottant 32 bits. Le NPU n'est
  * pas utilisé : pour 576 multiplications-additions, le démarrer coûterait
  * plus cher que le calcul lui-même.
  *
  * API du réseau généré (Appli/X-CUBE-AI/App/meteo_h0.h) :
  *   ai_meteo_h0_create_and_init() : prépare le réseau dans un tampon de
  *                                   travail ("activations", 128 octets)
  *   ai_meteo_h0_inputs_get()      : où écrire les 8 entrées
  *   ai_meteo_h0_outputs_get()     : où lire les 4 probabilités
  *   ai_meteo_h0_run()             : exécute le réseau
  ******************************************************************************
  */
#include "meteo_ia.h"
#include "meteo_modele_h0.h"   /* normalisation, classes, vecteurs de test */
#include "meteo_h0.h"
#include "meteo_h0_data.h"
#include "console.h"
#include "stm32n6xx_hal.h"
#include <math.h>
#include <stdio.h>

_Static_assert(METEO_H0_NB_ENTREES == METEO_IA_NB_ENTREES, "nombre d'entrees");
_Static_assert(METEO_H0_NB_CLASSES == METEO_IA_NB_CLASSES, "nombre de classes");
_Static_assert(METEO_H0_E_PRES == METEO_IA_E_PRES, "indice pression");
_Static_assert(METEO_H0_E_ECART_ROSEE == METEO_IA_E_ECART_ROSEE, "indice ecart rosee");
_Static_assert(AI_METEO_H0_IN_1_SIZE == METEO_IA_NB_ENTREES, "entree du reseau genere");
_Static_assert(AI_METEO_H0_OUT_1_SIZE == METEO_IA_NB_CLASSES, "sortie du reseau genere");

/* Réseau (identifiant opaque) et ses tampons d'entrée / sortie */
static ai_handle  s_reseau = AI_HANDLE_NULL;
static ai_buffer *s_entree = NULL;
static ai_buffer *s_sortie = NULL;

/* Mémoire de travail du réseau : 128 octets (voir l'analyse de stedgeai).
 * Les entrées et sorties sont rangées dedans ("allocate-inputs/outputs"). */
AI_ALIGNED(32)
static ai_u8 s_activations[AI_METEO_H0_DATA_ACTIVATIONS_SIZE];

/* --------------------------------------------------------------------------
 * Mesure du temps : compteur de cycles du processeur (DWT->CYCCNT).
 * Il avance d'une unité à chaque coup d'horloge du Cortex-M55 : bien plus
 * précis que HAL_GetTick() (1 ms), indispensable pour un calcul qui dure
 * quelques microsecondes.
 * ------------------------------------------------------------------------ */
static void compteur_cycles_init(void)
{
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;   /* active les blocs de trace/mesure */
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk; /* démarre le compteur de cycles   */
}

uint32_t meteo_ia_cycles_en_us(uint32_t cycles)
{
  uint32_t mhz = HAL_RCC_GetCpuClockFreq() / 1000000U;
  return (mhz != 0U) ? (cycles / mhz) : 0U;
}

const char *meteo_ia_classe(int k)
{
  return ((k >= 0) && (k < METEO_IA_NB_CLASSES)) ? METEO_H0_CLASSES[k] : "?";
}

bool meteo_ia_init(void)
{
  const ai_handle activations[] = { s_activations };

  compteur_cycles_init();

  /* weights = NULL : le réseau utilise les poids générés dans
   * meteo_h0_data_params.c (tableau constant en mémoire) */
  ai_error err = ai_meteo_h0_create_and_init(&s_reseau, activations, NULL);
  if (err.type != AI_ERROR_NONE)
  {
    console_lock();
    printf("  IA : erreur d'initialisation (type %d, code %d)\r\n",
           (int)err.type, (int)err.code);
    console_unlock();
    s_reseau = AI_HANDLE_NULL;
    return false;
  }

  s_entree = ai_meteo_h0_inputs_get(s_reseau, NULL);
  s_sortie = ai_meteo_h0_outputs_get(s_reseau, NULL);
  return (s_entree != NULL) && (s_sortie != NULL);
}

int meteo_ia_predire(const float entrees[METEO_IA_NB_ENTREES],
                     float proba[METEO_IA_NB_CLASSES], uint32_t *cycles)
{
  if (s_reseau == AI_HANDLE_NULL)
  {
    return -1;
  }

  /* 1. Normalisation, exactement comme en Python et dans MATLAB :
   *    x_norm = (x - moyenne) / écart-type                              */
  float *x = (float *)s_entree[0].data;
  for (int i = 0; i < METEO_IA_NB_ENTREES; i++)
  {
    x[i] = (entrees[i] - METEO_H0_MOYENNE[i]) / METEO_H0_ECART_TYPE[i];
  }

  /* 2. Exécution du réseau, chronométrée en cycles processeur */
  uint32_t t0 = DWT->CYCCNT;
  ai_i32 n = ai_meteo_h0_run(s_reseau, &s_entree[0], &s_sortie[0]);
  uint32_t duree = DWT->CYCCNT - t0;
  if (cycles != NULL)
  {
    *cycles = duree;
  }
  if (n != 1)                              /* 1 = un exemple traité */
  {
    return -1;
  }

  /* 3. Lecture des 4 probabilités et choix de la plus grande */
  const float *p = (const float *)s_sortie[0].data;
  int meilleure = 0;
  for (int k = 0; k < METEO_IA_NB_CLASSES; k++)
  {
    proba[k] = p[k];
    if (p[k] > p[meilleure])
    {
      meilleure = k;
    }
  }
  return meilleure;
}

/* Constantes de Magnus (comme ml/meteo_features.py) */
#define MAGNUS_A  17.62f
#define MAGNUS_B  243.12f
#define PI_F      3.14159265f

static bool bissextile(uint32_t a)
{
  return ((a % 4U == 0U) && (a % 100U != 0U)) || (a % 400U == 0U);
}

void meteo_ia_calculer_entrees(float temp_c, float humidite_rh,
                               float pression_hpa, uint32_t epoch_utc,
                               float entrees[METEO_IA_NB_ENTREES])
{
  /* Pression au niveau de la mer (formule hypsométrique) : Meteostat donne
   * la pression au niveau de la mer, la carte la mesure à son altitude. */
  const float h = METEO_H0_ALTITUDE_M;
  float pres = pression_hpa *
               powf(1.0f - 0.0065f * h / (temp_c + 0.0065f * h + 273.15f), -5.257f);

  /* Écart au point de rosée (formule de Magnus) */
  float rh = fminf(fmaxf(humidite_rh, 1.0f), 100.0f) / 100.0f;
  float g = logf(rh) + MAGNUS_A * temp_c / (MAGNUS_B + temp_c);
  float rosee = MAGNUS_B * g / (MAGNUS_A - g);

  /* Heure UTC et jour de l'année (0 = 1er janvier) à partir de l'epoch */
  uint32_t jours = epoch_utc / 86400U;
  uint32_t secondes = epoch_utc % 86400U;
  uint32_t annee = 1970U;
  for (;;)
  {
    uint32_t n = bissextile(annee) ? 366U : 365U;
    if (jours < n)
    {
      break;
    }
    jours -= n;
    annee++;
  }
  float heure = (float)(secondes / 3600U) + (float)((secondes % 3600U) / 60U) / 60.0f;
  float jour = (float)jours;

  entrees[METEO_H0_E_TEMP]        = temp_c;
  entrees[METEO_H0_E_RHUM]        = humidite_rh;
  entrees[METEO_H0_E_PRES]        = pres;
  entrees[METEO_H0_E_ECART_ROSEE] = temp_c - rosee;
  entrees[METEO_H0_E_HEURE_SIN]   = sinf(2.0f * PI_F * heure / 24.0f);
  entrees[METEO_H0_E_HEURE_COS]   = cosf(2.0f * PI_F * heure / 24.0f);
  entrees[METEO_H0_E_JOUR_SIN]    = sinf(2.0f * PI_F * jour / 365.25f);
  entrees[METEO_H0_E_JOUR_COS]    = cosf(2.0f * PI_F * jour / 365.25f);
}

/* Écrit une probabilité en pourcentage avec une décimale ("43.3"), sans
 * printf flottant (non activé dans newlib-nano) */
static void pourcent(float p, char *buf, int len)
{
  int32_t d = (int32_t)(p * 1000.0f + 0.5f);       /* dixièmes de % */
  snprintf(buf, (size_t)len, "%3ld.%01ld", (long)(d / 10), (long)(d % 10));
}

int meteo_ia_autotest(void)
{
  int identiques = 0;
  uint32_t cycles_max = 0;

  console_lock();
  printf("\r\n  === Auto-test IA embarquee (%d vecteurs, reponses Python connues) ===\r\n",
         METEO_H0_NB_VECTEURS_TEST);
  printf("   n  attendu         carte           Clair  Nuag.  Brou.  Prec.  cycles\r\n");

  for (int v = 0; v < METEO_H0_NB_VECTEURS_TEST; v++)
  {
    float proba[METEO_IA_NB_CLASSES];
    uint32_t cycles = 0;
    int k = meteo_ia_predire(METEO_H0_TEST_ENTREES[v], proba, &cycles);
    int attendu = (int)METEO_H0_TEST_CLASSE[v];
    char p[METEO_IA_NB_CLASSES][8];

    for (int c = 0; c < METEO_IA_NB_CLASSES; c++)
    {
      pourcent(proba[c], p[c], sizeof(p[c]));
    }
    printf("  %2d  %-15s %-15s %s  %s  %s  %s  %5lu  %s\r\n",
           v + 1, meteo_ia_classe(attendu), meteo_ia_classe(k),
           p[0], p[1], p[2], p[3], (unsigned long)cycles,
           (k == attendu) ? "OK" : "DIFFERENT");
    if (k == attendu)
    {
      identiques++;
    }
    if (cycles > cycles_max)
    {
      cycles_max = cycles;
    }
  }

  printf("  Resultat : %d / %d identiques a Python\r\n", identiques,
         METEO_H0_NB_VECTEURS_TEST);
  printf("  Temps d'inference max : %lu cycles = %lu us (CPU a %lu MHz)\r\n",
         (unsigned long)cycles_max, (unsigned long)meteo_ia_cycles_en_us(cycles_max),
         (unsigned long)(HAL_RCC_GetCpuClockFreq() / 1000000U));
  console_unlock();
  return identiques;
}
