"""
02_entrainer.py - Entraîne UN modèle et l'enregistre dans models/<nom>/.

Utilisation :
    python 02_entrainer.py                                  # réglages par défaut
    python 02_entrainer.py --schema 6_classes --taille M
    python 02_entrainer.py --entrees tendance --horizon 3   # prévoir à +3 h
    python 02_entrainer.py --schema 3_classes --poids racine

Choix possibles : voir SCHEMAS_CLASSES, TAILLES_MODELE et JEUX_ENTREES dans
config.py. --poids : aucun / racine / complets (voir entrainement.py).

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
    p.add_argument("--poids", default=config.HYPERPARAMS["poids_classes"],
                   choices=["aucun", "racine", "complets"],
                   help="poids des classes rares dans l'erreur")
    p.add_argument("--station", default=config.STATION_ID)
    args = p.parse_args()

    res, _, _ = entrainer(charger_brut(args.station), args.schema, args.taille,
                          args.entrees, args.horizon, args.poids)

    t = res["test"]
    print("\nPar classe (année de test) :")
    print("  rappel    = part des heures de cette classe bien reconnues")
    print("  précision = quand le modèle annonce cette classe, part des fois où c'est vrai")
    print(f"  {'classe':<24} {'rappel':>7} {'précision':>10}   heures")
    for c in res["classes"]:
        print(f"  {c:<24} {t['rappel_par_classe'][c]:7.1%} "
              f"{t['precision_par_classe'][c]:10.1%}   {t['effectif_par_classe'][c]}")


if __name__ == "__main__":
    main()
