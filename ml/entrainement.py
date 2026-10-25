"""
entrainement.py - Construction, entraînement et évaluation d'un modèle.

Utilisé par 02_entrainer.py (un modèle) et 03_etude_compromis.py (plusieurs).

Le modèle est un perceptron multicouche (MLP) : quelques couches "Dense".
Pourquoi pas un réseau plus sophistiqué (CNN, LSTM...) ?
  - nos entrées sont une poignée de nombres, pas une image ni une longue série ;
  - un MLP n'utilise que des opérations (produit matriciel, ReLU, softmax)
    supportées partout : ONNX, MATLAB, X-CUBE-AI et le NPU du STM32N6.
"""
import json
import time

import numpy as np
import pandas as pd

import config
from meteo_features import construire_jeu, decouper_par_annee, Normaliseur


# ---------------------------------------------------------------------------
# Données
# ---------------------------------------------------------------------------
def charger_brut(station=config.STATION_ID):
    chemin = config.DATA_RAW / f"hourly_{station}.csv"
    if not chemin.exists():
        raise SystemExit(f"{chemin} introuvable : lancer d'abord 01_donnees.py")
    return pd.read_csv(chemin, index_col="time", parse_dates=["time"])


def preparer(df_brut, schema_nom, jeu_nom, horizon_h=0):
    """Tout ce qui se passe avant le réseau de neurones."""
    entrees = config.JEUX_ENTREES[jeu_nom]
    schema = config.SCHEMAS_CLASSES[schema_nom]
    X, y, temps = construire_jeu(df_brut, entrees, schema, horizon_h)
    train, val, test = decouper_par_annee(X, y, temps,
                                          config.ANNEE_VAL, config.ANNEE_TEST)
    if len(val[0]) == 0 or len(test[0]) == 0:
        raise SystemExit("Validation ou test vide : vérifier ANNEE_VAL / ANNEE_TEST "
                         "et la période téléchargée.")
    norm = Normaliseur().ajuster(train[0])
    return {
        "entrees": entrees,
        "classes": list(schema.keys()),
        "norm": norm,
        "train": (norm.appliquer(train[0]), train[1]),
        "val":   (norm.appliquer(val[0]),   val[1]),
        "test":  (norm.appliquer(test[0]),  test[1]),
    }


def poids_des_classes(y, n_classes):
    """Poids inversement proportionnel à la fréquence de chaque classe.

    Sans poids, un modèle qui répond toujours "Nuageux" a déjà une bonne
    accuracy et n'apprend jamais à reconnaître la pluie ou le brouillard.
    Avec les poids, se tromper sur une classe rare "coûte" plus cher.
    """
    comptes = np.bincount(y, minlength=n_classes)
    poids = {}
    for c in range(n_classes):
        poids[c] = float(len(y) / (n_classes * comptes[c])) if comptes[c] else 0.0
    return poids


# ---------------------------------------------------------------------------
# Modèle
# ---------------------------------------------------------------------------
def construire_modele(n_entrees, n_classes, couches, dropout):
    import tensorflow as tf
    from tensorflow import keras

    modele = keras.Sequential(name="meteo_mlp")
    modele.add(keras.Input(shape=(n_entrees,), name="input"))
    for i, n in enumerate(couches):
        # ReLU : max(0, x). Simple, rapide, supportée par tous les outils.
        modele.add(keras.layers.Dense(n, activation="relu", name=f"dense_{i}"))
        if dropout > 0:
            # Désactive au hasard une fraction des neurones pendant
            # l'entraînement (pas en inférence) : limite le sur-apprentissage.
            modele.add(keras.layers.Dropout(dropout, name=f"dropout_{i}"))
    # Softmax : transforme les scores en probabilités qui somment à 1.
    modele.add(keras.layers.Dense(n_classes, activation="softmax", name="output"))

    modele.compile(
        optimizer=keras.optimizers.Adam(learning_rate=config.HYPERPARAMS["learning_rate"]),
        # "sparse" : les étiquettes sont des entiers (0, 1, 2...), pas du one-hot
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return modele


# ---------------------------------------------------------------------------
# Évaluation
# ---------------------------------------------------------------------------
def evaluer(modele, X, y, classes):
    from sklearn.metrics import (accuracy_score, balanced_accuracy_score,
                                 f1_score, confusion_matrix, recall_score)
    proba = modele.predict(X, batch_size=4096, verbose=0)
    pred = proba.argmax(axis=1)
    labels = list(range(len(classes)))
    return {
        # part des heures bien classées
        "accuracy": float(accuracy_score(y, pred)),
        # moyenne des rappels par classe : chaque classe compte autant,
        # qu'elle soit fréquente ou rare (l'indicateur honnête ici)
        "balanced_accuracy": float(balanced_accuracy_score(y, pred)),
        "f1_macro": float(f1_score(y, pred, labels=labels, average="macro",
                                   zero_division=0)),
        "rappel_par_classe": {c: float(r) for c, r in zip(
            classes, recall_score(y, pred, labels=labels, average=None,
                                  zero_division=0))},
        "effectif_par_classe": {c: int(n) for c, n in zip(
            classes, np.bincount(y, minlength=len(classes)))},
        "matrice_confusion": confusion_matrix(y, pred, labels=labels).tolist(),
    }


def taille_modele(modele):
    n = int(modele.count_params())
    return {"parametres": n,
            "octets_float32": 4 * n,     # chaque poids sur 4 octets
            "octets_int8": n}            # après quantification (~1 octet/poids)


# ---------------------------------------------------------------------------
# Entraînement complet
# ---------------------------------------------------------------------------
def nom_experience(schema_nom, taille_nom, jeu_nom, horizon_h):
    return f"{schema_nom}_{taille_nom}_{jeu_nom}_h{horizon_h}"


def entrainer(df_brut, schema_nom=config.SCHEMA_DEFAUT,
              taille_nom=config.TAILLE_DEFAUT, jeu_nom=config.JEU_ENTREES_DEFAUT,
              horizon_h=0, sauver=True, verbose=2):
    from tensorflow import keras

    hp = config.HYPERPARAMS
    keras.utils.set_random_seed(hp["seed"])      # mêmes résultats à chaque essai

    d = preparer(df_brut, schema_nom, jeu_nom, horizon_h)
    (Xtr, ytr), (Xva, yva), (Xte, yte) = d["train"], d["val"], d["test"]
    n_classes = len(d["classes"])
    couches = config.TAILLES_MODELE[taille_nom]
    nom = nom_experience(schema_nom, taille_nom, jeu_nom, horizon_h)
    print(f"\n=== {nom} : {len(Xtr)} train / {len(Xva)} val / {len(Xte)} test, "
          f"{len(d['entrees'])} entrées -> {couches} -> {n_classes} classes")

    modele = construire_modele(len(d["entrees"]), n_classes, couches, hp["dropout"])

    rappels = [
        # Arrête quand la perte de validation ne baisse plus, et revient aux
        # meilleurs poids : on n'a pas à deviner le bon nombre d'époques.
        keras.callbacks.EarlyStopping(monitor="val_loss", patience=hp["patience"],
                                      restore_best_weights=True),
        # Divise le pas d'apprentissage par 2 quand ça stagne : affine la fin.
        keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5,
                                          patience=hp["patience"] // 2, min_lr=1e-5),
    ]
    t0 = time.time()
    hist = modele.fit(
        Xtr, ytr, validation_data=(Xva, yva),
        epochs=hp["epochs_max"], batch_size=hp["batch_size"],
        class_weight=poids_des_classes(ytr, n_classes) if hp["poids_classes"] else None,
        callbacks=rappels, verbose=verbose)
    duree = time.time() - t0

    # Référence : toujours répondre la classe la plus fréquente du train.
    # Un modèle qui ne fait pas mieux que ça n'a rien appris.
    majoritaire = int(np.bincount(ytr).argmax())

    resultats = {
        "nom": nom, "schema": schema_nom, "taille": taille_nom,
        "couches": couches, "jeu_entrees": jeu_nom, "entrees": d["entrees"],
        "horizon_h": horizon_h, "classes": d["classes"],
        "hyperparametres": hp, "epoques": len(hist.history["loss"]),
        "duree_entrainement_s": round(duree, 1),
        **taille_modele(modele),
        "reference_majoritaire": {
            "classe": d["classes"][majoritaire],
            "accuracy_test": float(np.mean(yte == majoritaire)),
        },
        "test": evaluer(modele, Xte, yte, d["classes"]),
    }
    t = resultats["test"]
    print(f"    test : accuracy {t['accuracy']:.3f} | balanced {t['balanced_accuracy']:.3f}"
          f" | F1 macro {t['f1_macro']:.3f} | {resultats['parametres']} paramètres"
          f" | référence {resultats['reference_majoritaire']['accuracy_test']:.3f}")

    if sauver:
        dossier = config.MODELS_DIR / nom
        dossier.mkdir(parents=True, exist_ok=True)
        modele.save(dossier / "model.keras")
        (dossier / "normalisation.json").write_text(
            json.dumps(d["norm"].vers_dict(d["entrees"]), indent=2))
        (dossier / "resultats.json").write_text(
            json.dumps(resultats, indent=2, ensure_ascii=False))
        tracer_courbes(hist.history, dossier / "courbes.png", nom)
        tracer_confusion(t["matrice_confusion"], d["classes"],
                         dossier / "confusion.png", nom)
        print(f"    enregistré dans {dossier}")
    return resultats, modele, d


# ---------------------------------------------------------------------------
# Graphiques
# ---------------------------------------------------------------------------
def tracer_courbes(h, chemin, titre):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(1, 2, figsize=(10, 3.5))
    ax[0].plot(h["loss"], label="train"); ax[0].plot(h["val_loss"], label="validation")
    ax[0].set_title("Perte"); ax[0].set_xlabel("époque"); ax[0].legend()
    ax[1].plot(h["accuracy"], label="train"); ax[1].plot(h["val_accuracy"], label="validation")
    ax[1].set_title("Accuracy"); ax[1].set_xlabel("époque"); ax[1].legend()
    fig.suptitle(titre); fig.tight_layout(); fig.savefig(chemin, dpi=120); plt.close(fig)


def tracer_confusion(mat, classes, chemin, titre):
    """Matrice normalisée par ligne : la case (i, j) = part des heures de la
    vraie classe i que le modèle a classées en j. Diagonale = bonnes réponses."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    m = np.array(mat, dtype=float)
    m = m / np.maximum(m.sum(axis=1, keepdims=True), 1)
    fig, ax = plt.subplots(figsize=(1.0 + 0.6 * len(classes), 0.8 + 0.6 * len(classes)))
    ax.imshow(m, cmap="Blues", vmin=0, vmax=1)
    ax.set_xticks(range(len(classes)), classes, rotation=45, ha="right")
    ax.set_yticks(range(len(classes)), classes)
    ax.set_xlabel("prédit"); ax.set_ylabel("réel"); ax.set_title(titre, fontsize=9)
    for i in range(len(classes)):
        for j in range(len(classes)):
            ax.text(j, i, f"{m[i, j]:.2f}", ha="center", va="center", fontsize=7,
                    color="white" if m[i, j] > 0.5 else "black")
    fig.tight_layout(); fig.savefig(chemin, dpi=120); plt.close(fig)
