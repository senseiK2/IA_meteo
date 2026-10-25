"""
03_etude_compromis.py - TP3.1b : compromis nombre de classes / taille / précision.

Trois expériences, chacune ne fait varier QU'UN facteur à la fois :
  A. schéma de classes x taille du réseau   (entrées par défaut)
  B. jeu d'entrées                           (schéma et taille par défaut)
  C. horizon de prévision 0 / 3 / 6 h        (schéma et taille par défaut)

Utilisation :
    python 03_etude_compromis.py            # A + B + C (~quelques minutes sur CPU)
    python 03_etude_compromis.py --seulement A

Sorties dans results/ :
    etude_compromis.csv   une ligne par modèle entraîné
    etude_compromis.md    tableaux prêts à copier dans le README / le rapport
    compromis_taille.png  accuracy équilibrée en fonction du nombre de paramètres
"""
import argparse
import json

import pandas as pd

import config
from entrainement import charger_brut, entrainer


def ligne(res, experience):
    t = res["test"]
    return {
        "experience": experience,
        "schema": res["schema"], "n_classes": len(res["classes"]),
        "taille": res["taille"], "couches": str(res["couches"]),
        "entrees": res["jeu_entrees"], "n_entrees": len(res["entrees"]),
        "horizon_h": res["horizon_h"],
        "parametres": res["parametres"], "octets_int8": res["octets_int8"],
        "accuracy": round(t["accuracy"], 4),
        "balanced_accuracy": round(t["balanced_accuracy"], 4),
        "f1_macro": round(t["f1_macro"], 4),
        "reference_majoritaire": round(res["reference_majoritaire"]["accuracy_test"], 4),
        "epoques": res["epoques"],
    }


def tracer(df, chemin):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    a = df[df.experience == "A"]
    fig, ax = plt.subplots(figsize=(7, 4))
    for schema, g in a.groupby("schema", sort=False):
        g = g.sort_values("parametres")
        ax.plot(g.parametres, g.balanced_accuracy, "o-",
                label=f"{schema} ({g.n_classes.iloc[0]} classes)")
        for _, r in g.iterrows():
            ax.annotate(r.taille, (r.parametres, r.balanced_accuracy),
                        textcoords="offset points", xytext=(4, 4), fontsize=8)
    ax.set_xscale("log")
    ax.set_xlabel("nombre de paramètres (= octets en int8)")
    ax.set_ylabel("accuracy équilibrée (test)")
    ax.set_title("Compromis classes / taille / précision")
    ax.grid(alpha=0.3); ax.legend(fontsize=8)
    fig.tight_layout(); fig.savefig(chemin, dpi=130); plt.close(fig)


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--seulement", choices=["A", "B", "C"])
    p.add_argument("--station", default=config.STATION_ID)
    args = p.parse_args()

    df_brut = charger_brut(args.station)
    lignes = []

    def lancer(exp, **kw):
        # sauver=False : on ne garde que les métriques, pas les 20 modèles
        res, _, _ = entrainer(df_brut, sauver=False, verbose=0, **kw)
        lignes.append(ligne(res, exp))

    if args.seulement in (None, "A"):
        for schema in config.SCHEMAS_CLASSES:
            for taille in config.TAILLES_MODELE:
                lancer("A", schema_nom=schema, taille_nom=taille)
    if args.seulement in (None, "B"):
        for jeu in config.JEUX_ENTREES:
            lancer("B", jeu_nom=jeu)
    if args.seulement in (None, "C"):
        for h in (0, 3, 6):
            lancer("C", jeu_nom="tendance", horizon_h=h)

    df = pd.DataFrame(lignes)
    config.RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    df.to_csv(config.RESULTS_DIR / "etude_compromis.csv", index=False)

    with open(config.RESULTS_DIR / "etude_compromis.md", "w", encoding="utf-8") as f:
        f.write(f"# Étude du compromis (station {config.STATION_NOM}, "
                f"test = année {config.ANNEE_TEST})\n\n")
        f.write("Hyperparamètres : `" + json.dumps(config.HYPERPARAMS) + "`\n\n")
        for exp, titre in [("A", "A. Classes x taille"), ("B", "B. Jeux d'entrées"),
                           ("C", "C. Horizon de prévision")]:
            sous = df[df.experience == exp].drop(columns="experience")
            if len(sous):
                f.write(f"## {titre}\n\n{sous.to_markdown(index=False)}\n\n")

    if (df.experience == "A").any():
        tracer(df, config.RESULTS_DIR / "compromis_taille.png")
    print("\n", df.to_string(index=False))
    print(f"\nRésultats dans {config.RESULTS_DIR}")


if __name__ == "__main__":
    main()
