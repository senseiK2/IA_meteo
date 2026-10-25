"""
02_entrainer.py - Entraîne UN modèle et l'enregistre dans models/<nom>/.

Utilisation :
    python 02_entrainer.py                                  # réglages par défaut
    python 02_entrainer.py --schema 6_classes --taille M
    python 02_entrainer.py --entrees tendance --horizon 3   # prévoir à +3 h

Choix possibles : voir SCHEMAS_CLASSES, TAILLES_MODELE et JEUX_ENTREES dans
config.py.

Contenu de models/<nom>/ :
    model.keras          modèle TensorFlow (à exporter avec 04_exporter.py)
    normalisation.json   moyenne / écart-type de chaque entrée
    resultats.json       métriques sur l'année de test, taille, hyperparamètres
    courbes.png          perte et accuracy au fil des époques
    confusion.png        matrice de confusion sur l'année de test
"""
import argparse

import config
from entrainement import charger_brut, entrainer


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--schema", default=config.SCHEMA_DEFAUT,
                   choices=config.SCHEMAS_CLASSES)
    p.add_argument("--taille", default=config.TAILLE_DEFAUT,
                   choices=config.TAILLES_MODELE)
    p.add_argument("--entrees", default=config.JEU_ENTREES_DEFAUT,
                   choices=config.JEUX_ENTREES)
    p.add_argument("--horizon", type=int, default=0,
                   help="0 = temps actuel, N = temps dans N heures")
    p.add_argument("--station", default=config.STATION_ID)
    args = p.parse_args()

    res, _, _ = entrainer(charger_brut(args.station), args.schema, args.taille,
                          args.entrees, args.horizon)

    print("\nRappel par classe (part des heures de cette classe bien reconnues) :")
    for c, r in res["test"]["rappel_par_classe"].items():
        n = res["test"]["effectif_par_classe"][c]
        print(f"  {c:<24} {r:6.1%}   ({n} h dans l'année de test)")


if __name__ == "__main__":
    main()
