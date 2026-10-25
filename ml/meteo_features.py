"""
meteo_features.py - Transforme les données brutes Meteostat en un jeu
d'entraînement : entrées (X), étiquettes (y), découpage train/val/test.

Ce fichier ne dépend que de numpy et pandas (pas de TensorFlow) pour pouvoir
être testé et réutilisé facilement.

IMPORTANT : chaque formule de ce fichier doit être refaite À L'IDENTIQUE
dans MATLAB (TP3.2) et dans le code C de la carte (TP3.3). Si la carte
calcule l'écart au point de rosée autrement qu'ici, le modèle reçoit des
entrées qu'il n'a jamais vues et se trompe, même s'il est "bon".
"""
import numpy as np
import pandas as pd

# Constantes de Magnus (Sonntag 1990), valables de -45 °C à +60 °C
MAGNUS_A = 17.62
MAGNUS_B = 243.12  # °C


def point_de_rosee(temp_c, rhum_pct):
    """Température de rosée (°C) : température à laquelle l'air, refroidi,
    serait saturé (100 % d'humidité). Formule de Magnus."""
    rh = np.clip(rhum_pct, 1.0, 100.0) / 100.0           # évite log(0)
    gamma = np.log(rh) + MAGNUS_A * temp_c / (MAGNUS_B + temp_c)
    return MAGNUS_B * gamma / (MAGNUS_A - gamma)


def pression_niveau_mer(p_station_hpa, temp_c, altitude_m):
    """Ramène la pression mesurée par la carte au niveau de la mer.

    Meteostat (comme toutes les stations météo) donne la pression au niveau
    de la mer, pour pouvoir comparer des villes d'altitudes différentes. Le
    LPS22DF, lui, mesure la pression là où il est : à 235 m on lit environ
    28 hPa de moins. Sans cette correction le modèle verrait une "dépression"
    permanente. (Formule hypsométrique, atmosphère standard.)
    Cette fonction n'est pas utilisée pour l'entraînement : elle documente
    le calcul que la carte doit faire.
    """
    return p_station_hpa * (1.0 - 0.0065 * altitude_m /
                            (temp_c + 0.0065 * altitude_m + 273.15)) ** -5.257


def normaliser_index(df):
    """Met les données sur une grille horaire régulière (une ligne par heure).

    Les trous (heures manquantes) deviennent des lignes de NaN : ainsi
    shift(3) veut vraiment dire "3 heures avant", et non "3 lignes avant".
    """
    if isinstance(df.index, pd.MultiIndex):          # Meteostat 2.x multi-stations
        df = df.droplevel([n for n in df.index.names if n != "time"])
    df = df.sort_index()
    df = df[~df.index.duplicated(keep="first")]
    return df.asfreq("h")


def calculer_entrees(df):
    """Ajoute au DataFrame toutes les entrées possibles (cf. config.JEUX_ENTREES)."""
    df = df.copy()
    df["ecart_rosee"] = df["temp"] - point_de_rosee(df["temp"], df["rhum"])

    # L'heure et le jour sont cycliques : 23 h est proche de 0 h, le
    # 31 décembre du 1er janvier. Un seul nombre (0..23) ferait croire au
    # réseau que 23 h et 0 h sont très éloignés ; le couple (sin, cos) place
    # les valeurs sur un cercle et règle le problème.
    heure = df.index.hour + df.index.minute / 60.0
    jour  = df.index.dayofyear - 1
    df["heure_sin"] = np.sin(2 * np.pi * heure / 24.0)
    df["heure_cos"] = np.cos(2 * np.pi * heure / 24.0)
    df["jour_sin"]  = np.sin(2 * np.pi * jour / 365.25)
    df["jour_cos"]  = np.cos(2 * np.pi * jour / 365.25)

    # Une pression qui chute annonce souvent la pluie
    df["dp3h"] = df["pres"] - df["pres"].shift(3)
    return df


def calculer_etiquettes(df, schema, horizon_h=0):
    """Transforme le code Meteostat "coco" (1..27) en numéro de classe.

    schema    : dict {nom_classe: [codes coco]} (voir config.SCHEMAS_CLASSES)
    horizon_h : 0 -> on reconnaît le temps qu'il fait MAINTENANT ;
                3 -> on prédit le temps qu'il fera DANS 3 HEURES.
    Renvoie une Series d'entiers (NaN si pas d'étiquette).
    """
    table = {}
    for indice, codes in enumerate(schema.values()):
        for code in codes:
            table[code] = indice
    coco = df["coco"].shift(-horizon_h) if horizon_h else df["coco"]
    return coco.map(table)


def construire_jeu(df_brut, entrees, schema, horizon_h=0):
    """Renvoie (X, y, index_temps) sans aucune ligne incomplète."""
    df = calculer_entrees(normaliser_index(df_brut))
    df["classe"] = calculer_etiquettes(df, schema, horizon_h)
    df = df.dropna(subset=list(entrees) + ["classe"])
    X = df[list(entrees)].to_numpy(dtype=np.float32)
    y = df["classe"].to_numpy(dtype=np.int64)
    return X, y, df.index


def decouper_par_annee(X, y, temps, annee_val, annee_test):
    """Découpage CHRONOLOGIQUE (et non aléatoire).

    Deux heures qui se suivent se ressemblent presque toujours. Avec un
    tirage aléatoire, le test contiendrait 14 h et le train 13 h et 15 h du
    même jour : le modèle aurait l'air excellent... en trichant. En gardant
    une année entière de côté, on mesure ce qui compte : la performance sur
    des jours que le modèle n'a jamais vus.
    """
    annee = temps.year
    m_train = annee < annee_val
    m_val   = annee == annee_val
    m_test  = annee == annee_test
    return ((X[m_train], y[m_train]),
            (X[m_val],   y[m_val]),
            (X[m_test],  y[m_test]))


class Normaliseur:
    """Centre-réduit chaque entrée : x_norm = (x - moyenne) / écart-type.

    Sans ça, la pression (~1015) écraserait l'humidité (~70) et les sinus
    (~0) dans les premiers calculs du réseau. Moyenne et écart-type sont
    calculés sur le TRAIN uniquement, puis exportés pour MATLAB et la carte.
    """
    def __init__(self):
        self.moyenne = None
        self.ecart_type = None

    def ajuster(self, X):
        self.moyenne = X.mean(axis=0)
        self.ecart_type = X.std(axis=0)
        self.ecart_type[self.ecart_type < 1e-6] = 1.0
        return self

    def appliquer(self, X):
        return ((X - self.moyenne) / self.ecart_type).astype(np.float32)

    def vers_dict(self, entrees):
        return {"entrees": list(entrees),
                "moyenne": [float(v) for v in self.moyenne],
                "ecart_type": [float(v) for v in self.ecart_type]}
