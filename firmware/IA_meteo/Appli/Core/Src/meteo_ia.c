/**
  ******************************************************************************
  * @file    meteo_ia.c
  * @brief   Inférence des modèles météo sur la carte (TP3 partie 3).
  *
  * Chaîne complète :
  *   TensorFlow (ml/02_entrainer.py) -> ONNX (ml/04_exporter.py)
  *   -> code C (X-CUBE-AI / ST Edge AI Core, commande "stedgeai generate")
  *   -> ce fichier, qui appelle les réseaux générés.
  *
  * Les réseaux tournent sur le processeur Cortex-M55 (bibliothèque
  * NetworkRuntime de X-CUBE-AI), en calcul flottant 32 bits. Le NPU n'est
  * pas utilisé : pour 576 multiplications-additions, le démarrer coûterait
  * plus cher que le calcul lui-même.
  *
  * API d'un réseau généré (Appli/X-CUBE-AI/App/meteo_hX.h) :
  *   ai_meteo_hX_create_and_init() : prépare le réseau dans un tampon de
  *                                   travail ("activations", 128 octets)
  *   ai_meteo_hX_inputs_get()      : où écrire les 8 entrées
  *   ai_meteo_hX_outputs_get()     : où lire les 4 probabilités
  *   ai_meteo_hX_run()             : exécute le réseau
  *
  * Les deux modèles ont exactement la même API, seul le préfixe change
  * (ai_meteo_h0_..., ai_meteo_h3_...). On les décrit donc dans un tableau
  * (s_modeles) : le reste du code est écrit une seule fois pour les deux.
  ******************************************************************************
  */
#include "meteo_ia.h"
#include "meteo_modele_h0.h"   /* normalisation, classes, vecteurs de test h0 */
#include "meteo_modele_h3.h"   /* idem pour h3 */
#include "meteo_h0.h"
#include "meteo_h0_data.h"
#include "meteo_h3.h"
#include "meteo_h3_data.h"
#include "console.h"
#include "stm32n6xx_hal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Les fichiers générés doivent correspondre à ce que le code attend */
_Static_assert(METEO_H0_NB_ENTREES == METEO_IA_NB_ENTREES, "h0 : nombre d'entrees");
_Static_assert(METEO_H3_NB_ENTREES == METEO_IA_NB_ENTREES, "h3 : nombre d'entrees");
_Static_assert(METEO_H0_NB_CLASSES == METEO_IA_NB_CLASSES, "h0 : nombre de classes");
_Static_assert(METEO_H3_NB_CLASSES == METEO_IA_NB_CLASSES, "h3 : nombre de classes");
_Static_assert(METEO_H0_E_PRES == METEO_IA_E_PRES, "indice pression");
_Static_assert(METEO_H0_E_ECART_ROSEE == METEO_IA_E_ECART_ROSEE, "indice ecart rosee");
_Static_assert(AI_METEO_H0_IN_1_SIZE == METEO_IA_NB_ENTREES, "entree du reseau h0");
_Static_assert(AI_METEO_H3_IN_1_SIZE == METEO_IA_NB_ENTREES, "entree du reseau h3");
_Static_assert(AI_METEO_H0_OUT_1_SIZE == METEO_IA_NB_CLASSES, "sortie du reseau h0");
_Static_assert(AI_METEO_H3_OUT_1_SIZE == METEO_IA_NB_CLASSES, "sortie du reseau h3");

/* Mémoire de travail de chaque réseau : 128 octets (analyse de stedgeai).
 * Les entrées et sorties sont rangées dedans ("allocate-inputs/outputs"). */
AI_ALIGNED(32) static ai_u8 s_activations_h0[AI_METEO_H0_DATA_ACTIVATIONS_SIZE];
AI_ALIGNED(32) static ai_u8 s_activations_h3[AI_METEO_H3_DATA_ACTIVATIONS_SIZE];

/* Tout ce qui distingue un modèle de l'autre */
typedef struct
{
  const char *nom;
  /* fonctions générées par X-CUBE-AI pour ce réseau */
  ai_error   (*creer)(ai_handle *reseau, const ai_handle activations[],
                      const ai_handle poids[]);
  ai_buffer *(*entrees)(ai_handle reseau, ai_u16 *n);
  ai_buffer *(*sorties)(ai_handle reseau, ai_u16 *n);
  ai_i32     (*executer)(ai_handle reseau, const ai_buffer *in, ai_buffer *out);
  ai_u8       *activations;
  /* normalisation et vecteurs de test exportés par ml/04_exporter.py */
  const float *moyenne;
  const float *ecart_type;
  const float (*test_entrees)[METEO_IA_NB_ENTREES];
  const unsigned char *test_classe;
  int          nb_tests;
  /* état, rempli par meteo_ia_init() */
  ai_handle    reseau;
  ai_buffer   *entree;
  ai_buffer   *sortie;
} modele_t;

static modele_t s_modeles[METEO_IA_NB_MODELES] = {
  [METEO_IA_H0] = {
    .nom = "h0", .creer = ai_meteo_h0_create_and_init,
    .entrees = ai_meteo_h0_inputs_get, .sorties = ai_meteo_h0_outputs_get,
    .executer = ai_meteo_h0_run, .activations = s_activations_h0,
    .moyenne = METEO_H0_MOYENNE, .ecart_type = METEO_H0_ECART_TYPE,
    .test_entrees = METEO_H0_TEST_ENTREES, .test_classe = METEO_H0_TEST_CLASSE,
    .nb_tests = METEO_H0_NB_VECTEURS_TEST, .reseau = AI_HANDLE_NULL,
  },
  [METEO_IA_H3] = {
    .nom = "h3", .creer = ai_meteo_h3_create_and_init,
    .entrees = ai_meteo_h3_inputs_get, .sorties = ai_meteo_h3_outputs_get,
    .executer = ai_meteo_h3_run, .activations = s_activations_h3,
    .moyenne = METEO_H3_MOYENNE, .ecart_type = METEO_H3_ECART_TYPE,
    .test_entrees = METEO_H3_TEST_ENTREES, .test_classe = METEO_H3_TEST_CLASSE,
    .nb_tests = METEO_H3_NB_VECTEURS_TEST, .reseau = AI_HANDLE_NULL,
  },
};

/* --------------------------------------------------------------------------
 * Mesure du temps : compteur de cycles du processeur (DWT->CYCCNT).
 * Il avance d'une unité à chaque coup d'horloge du Cortex-M55 : bien plus
 * précis que HAL_GetTick() (1 ms), indispensable pour un calcul qui dure
 * quelques centaines de microsecondes.
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
  /* Les deux modèles ont les mêmes classes, dans le même ordre (vérifié
   * dans meteo_ia_init) */
  return ((k >= 0) && (k < METEO_IA_NB_CLASSES)) ? METEO_H0_CLASSES[k] : "?";
}

const char *meteo_ia_nom(meteo_ia_modele_t modele)
{
  return (modele < METEO_IA_NB_MODELES) ? s_modeles[modele].nom : "?";
}

bool meteo_ia_init(void)
{
  bool ok = true;

  compteur_cycles_init();

  for (int k = 0; k < METEO_IA_NB_CLASSES; k++)
  {
    if (strcmp(METEO_H0_CLASSES[k], METEO_H3_CLASSES[k]) != 0)
    {
      console_lock();
      printf("  IA : les modeles h0 et h3 n'ont pas les memes classes !\r\n");
      console_unlock();
      return false;
    }
  }

  for (int m = 0; m < METEO_IA_NB_MODELES; m++)
  {
    modele_t *md = &s_modeles[m];
    const ai_handle activations[] = { md->activations };

    /* poids = NULL : le réseau utilise les poids générés dans
     * meteo_hX_data_params.c (tableau constant en mémoire) */
    ai_error err = md->creer(&md->reseau, activations, NULL);
    if (err.type != AI_ERROR_NONE)
    {
      console_lock();
      printf("  IA %s : erreur d'initialisation (type %d, code %d)\r\n",
             md->nom, (int)err.type, (int)err.code);
      console_unlock();
      md->reseau = AI_HANDLE_NULL;
      ok = false;
      continue;
    }
    md->entree = md->entrees(md->reseau, NULL);
    md->sortie = md->sorties(md->reseau, NULL);
    ok = ok && (md->entree != NULL) && (md->sortie != NULL);
  }
  return ok;
}

int meteo_ia_predire(meteo_ia_modele_t modele,
                     const float entrees[METEO_IA_NB_ENTREES],
                     float proba[METEO_IA_NB_CLASSES], uint32_t *cycles)
{
  if ((modele >= METEO_IA_NB_MODELES) || (s_modeles[modele].reseau == AI_HANDLE_NULL))
  {
    return -1;
  }
  const modele_t *md = &s_modeles[modele];

  /* 1. Normalisation, exactement comme en Python et dans MATLAB :
   *    x_norm = (x - moyenne) / écart-type, avec les valeurs du modèle   */
  float *x = (float *)md->entree[0].data;
  for (int i = 0; i < METEO_IA_NB_ENTREES; i++)
  {
    x[i] = (entrees[i] - md->moyenne[i]) / md->ecart_type[i];
  }

  /* 2. Exécution du réseau, chronométrée en cycles processeur */
  uint32_t t0 = DWT->CYCCNT;
  ai_i32 n = md->executer(md->reseau, &md->entree[0], &md->sortie[0]);
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
  const float *p = (const float *)md->sortie[0].data;
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

int meteo_ia_autotest(meteo_ia_modele_t modele)
{
  if (modele >= METEO_IA_NB_MODELES)
  {
    return 0;
  }
  const modele_t *md = &s_modeles[modele];
  int identiques = 0;
  uint32_t cycles_max = 0;

  console_lock();
  printf("\r\n  === Auto-test IA embarquee, modele %s (%d vecteurs, reponses Python connues) ===\r\n",
         md->nom, md->nb_tests);
  printf("   n  attendu         carte           Clair  Nuag.  Brou.  Prec.  cycles\r\n");

  for (int v = 0; v < md->nb_tests; v++)
  {
    float proba[METEO_IA_NB_CLASSES];
    uint32_t cycles = 0;
    int k = meteo_ia_predire(modele, md->test_entrees[v], proba, &cycles);
    int attendu = (int)md->test_classe[v];
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

  printf("  Resultat %s : %d / %d identiques a Python\r\n", md->nom, identiques,
         md->nb_tests);
  printf("  Temps d'inference max : %lu cycles = %lu us (CPU a %lu MHz)\r\n",
         (unsigned long)cycles_max, (unsigned long)meteo_ia_cycles_en_us(cycles_max),
         (unsigned long)(HAL_RCC_GetCpuClockFreq() / 1000000U));
  console_unlock();
  return identiques;
}
