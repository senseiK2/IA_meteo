"""
config.py - Paramètres communs à tous les scripts du TP3.

Tout ce qui peut changer d'une expérience à l'autre est ici, et nulle part
ailleurs : la station, la période, les entrées du modèle, les regroupements
de classes. Les autres scripts importent ce fichier.
"""
from pathlib import Path

# ---------------------------------------------------------------------------
# Dossiers
# ---------------------------------------------------------------------------
ML_DIR      = Path(__file__).resolve().parent
DATA_RAW    = ML_DIR / "data" / "raw"        # CSV bruts Meteostat (non versionnés)
DATA_PROC   = ML_DIR / "data" / "processed"  # jeu de données prêt (non versionné)
MODELS_DIR  = ML_DIR / "models"              # modèles exportés (versionnés : petits)
RESULTS_DIR = ML_DIR / "results"             # métriques, graphes de l'étude

# ---------------------------------------------------------------------------
# Station météo
# ---------------------------------------------------------------------------
# Chambéry / Aix-les-Bains (aéroport de Voglans, à 3 km du campus du Bourget).
# L'identifiant Meteostat d'une station synoptique est son numéro OMM.
# Pour en chercher une autre : python 01_donnees.py --chercher 45.64 5.87
STATION_ID       = "07491"
STATION_NOM      = "Chambery-Aix"
STATION_ALTITUDE = 235           # m (information)

# Altitude de la CARTE (pas de la station !) : la carte la sert à ramener sa
# pression mesurée au niveau de la mer, comme Meteostat. À régler selon la
# salle de TP. Vérification simple : la pression "niveau de la mer" calculée
# par la carte doit être proche de celle affichée par un site météo local.
ALTITUDE_CARTE = 235             # m

# Période d'historique. Le découpage train / validation / test est
# CHRONOLOGIQUE (voir 02_entrainer.py) : on apprend sur le passé et on teste
# sur l'année la plus récente, comme en conditions réelles.
DATE_DEBUT   = "2015-01-01"
DATE_FIN     = "2025-12-31"
ANNEE_VAL    = 2024              # année de validation (réglage, early stopping)
ANNEE_TEST   = 2025              # année de test (jamais vue pendant l'entraînement)

# ---------------------------------------------------------------------------
# Entrées du modèle
# ---------------------------------------------------------------------------
# Règle : une entrée n'a de sens que si la carte STM32 peut la calculer.
#   temp, rhum, pres : mesurés (SHT40 / LPS22DF)
#   ecart_rosee      : calculé à partir de temp et rhum (formule de Magnus)
#   heure_*, jour_*  : heure UTC et jour de l'année -> la carte doit avoir
#                      l'heure (SNTP par le réseau)
#   dp3h             : variation de pression sur 3 h -> la carte doit avoir
#                      3 h d'historique (ou le récupérer sur ThingSpeak)
#
# Chaque "jeu" est une liste d'entrées. L'étude (03_etude_compromis.py)
# compare ces jeux pour savoir si les entrées supplémentaires valent le coup.
JEUX_ENTREES = {
    "mesures":  ["temp", "rhum", "pres"],
    "derivees": ["temp", "rhum", "pres", "ecart_rosee"],
    "temps":    ["temp", "rhum", "pres", "ecart_rosee",
                 "heure_sin", "heure_cos", "jour_sin", "jour_cos"],
    "tendance": ["temp", "rhum", "pres", "ecart_rosee",
                 "heure_sin", "heure_cos", "jour_sin", "jour_cos", "dp3h"],
}
JEU_ENTREES_DEFAUT = "temps"

# ---------------------------------------------------------------------------
# Classes
# ---------------------------------------------------------------------------
# Meteostat code la "condition météo" observée chaque heure dans la colonne
# "coco" (1 à 27) :
#  1 Clear          2 Fair            3 Cloudy           4 Overcast
#  5 Fog            6 Freezing Fog    7 Light Rain       8 Rain
#  9 Heavy Rain    10 Freezing Rain  11 Heavy Freezing  12 Sleet
# 13 Heavy Sleet   14 Light Snowfall 15 Snowfall        16 Heavy Snowfall
# 17 Rain Shower   18 Heavy Rain Sh. 19 Sleet Shower    20 Heavy Sleet Sh.
# 21 Snow Shower   22 Heavy Snow Sh. 23 Lightning       24 Hail
# 25 Thunderstorm  26 Heavy Thunder. 27 Storm
#
# Un "schéma" regroupe ces 27 codes en N classes. Plus N est grand, plus le
# modèle est informatif... et plus il se trompe : c'est le compromis étudié
# en 3.1b. L'ordre des classes = l'indice de sortie du réseau (0, 1, 2...),
# il doit être identique dans MATLAB et dans le code C.
SCHEMAS_CLASSES = {
    # Le minimum utile : "faut-il un parapluie ?"
    "3_classes": {
        "Clair":          [1, 2],
        "Nuageux":        [3, 4, 5, 6],
        "Precipitations": list(range(7, 28)),
    },
    # Le brouillard est séparé : l'humidité à ~100 % le rend reconnaissable
    "4_classes": {
        "Clair":          [1, 2],
        "Nuageux":        [3, 4],
        "Brouillard":     [5, 6],
        "Precipitations": list(range(7, 28)),
    },
    # On distingue pluie / neige (la température aide) et orage
    "6_classes": {
        "Clair":      [1, 2],
        "Nuageux":    [3, 4],
        "Brouillard": [5, 6],
        "Pluie":      [7, 8, 9, 10, 11, 17, 18],
        "Neige":      [12, 13, 14, 15, 16, 19, 20, 21, 22],
        "Orage":      [23, 24, 25, 26, 27],
    },
    # La liste complète du sujet (13 classes)
    "tp_complet": {
        "Clair":                 [1],
        "Peu_nuageux":           [2],
        "Partiellement_nuageux": [3],
        "Couvert":               [4],
        "Brouillard":            [5, 6],
        "Pluie":                 [7, 8, 9, 10, 11],
        "Averses":               [17, 18],
        "Neige":                 [15, 16],
        "Neige_legere":          [14, 21, 22],
        "Pluie_neige":           [12, 13, 19, 20],
        "Orage":                 [23, 25],
        "Orage_violent":         [24, 26],
        "Vent_fort":             [27],
    },
}
SCHEMA_DEFAUT = "4_classes"

# ---------------------------------------------------------------------------
# Hyperparamètres (justifiés dans ml/README.md)
# ---------------------------------------------------------------------------
# Tailles de réseau testées : liste des neurones de chaque couche cachée.
TAILLES_MODELE = {
    "XS": [8],
    "S":  [16, 16],
    "M":  [32, 32],
    "L":  [64, 64, 32],
}
TAILLE_DEFAUT = "S"

HYPERPARAMS = {
    "learning_rate":  1e-3,   # valeur standard d'Adam, stable sur un petit MLP
    "batch_size":     256,    # ~250 lots par époque : rapide et gradient peu bruité
    "epochs_max":     150,    # plafond : l'early stopping arrête bien avant
    "patience":       12,     # époques sans progrès (val_loss) avant arrêt
    "dropout":        0.1,    # régularisation légère (petit réseau, beaucoup de données)
    "poids_classes":  True,   # compense le déséquilibre (beaucoup de "Nuageux")
    "seed":           42,     # reproductibilité
}
