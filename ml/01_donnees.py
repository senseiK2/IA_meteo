"""
01_donnees.py - Télécharge l'historique horaire d'une station avec Meteostat.

Utilisation :
    python 01_donnees.py                       # station de config.py
    python 01_donnees.py --chercher 45.64 5.87 # liste les stations proches
    python 01_donnees.py --station 07491 --debut 2015-01-01 --fin 2025-12-31

Sortie : data/raw/hourly_<station>.csv (une ligne par heure, temps en UTC)
Colonnes utiles : temp (°C), rhum (%), pres (hPa, niveau de la mer),
                  coco (code de condition météo 1..27 -> nos étiquettes)

Fonctionne avec Meteostat 2.x (API "ms.hourly") et 1.x (API "Hourly").
"""
import argparse
from datetime import datetime

import pandas as pd

import config


def chercher_stations(lat, lon):
    """Affiche les stations les plus proches d'un point."""
    import meteostat as ms
    if hasattr(ms, "hourly"):                # Meteostat 2.x
        stations = ms.stations.nearby(ms.Point(lat, lon), radius=100_000, limit=10)
    else:                                    # Meteostat 1.x
        stations = ms.Stations().nearby(lat, lon).fetch(10)
    with pd.option_context("display.max_columns", 12, "display.width", 160):
        print(stations)


def telecharger(station, debut, fin):
    """Renvoie un DataFrame horaire indexé par le temps (UTC)."""
    debut = datetime.fromisoformat(debut)
    fin = datetime.fromisoformat(fin).replace(hour=23, minute=59)
    import meteostat as ms
    if hasattr(ms, "hourly"):                # Meteostat 2.x
        df = ms.hourly(station, debut, fin).fetch()
    else:                                    # Meteostat 1.x
        df = ms.Hourly(station, debut, fin).fetch()
    if df is None or df.empty:
        raise SystemExit(f"Aucune donnée pour la station {station}.")
    if isinstance(df.index, pd.MultiIndex):  # (station, time) -> time
        df = df.droplevel([n for n in df.index.names if n != "time"])
    df.index.name = "time"
    return df


def resume(df):
    """Ce qu'il faut regarder avant d'entraîner quoi que ce soit."""
    print(f"\n{len(df)} heures, du {df.index.min()} au {df.index.max()}")
    print("\nValeurs manquantes par colonne (%) :")
    print((df.isna().mean() * 100).round(1).to_string())
    if "coco" in df:
        print("\nRépartition des codes 'coco' (nombre d'heures) :")
        print(df["coco"].value_counts().sort_index().astype(int).to_string())


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--station", default=config.STATION_ID)
    p.add_argument("--debut", default=config.DATE_DEBUT)
    p.add_argument("--fin", default=config.DATE_FIN)
    p.add_argument("--chercher", nargs=2, type=float, metavar=("LAT", "LON"))
    args = p.parse_args()

    if args.chercher:
        chercher_stations(*args.chercher)
        return

    print(f"Téléchargement station {args.station} : {args.debut} -> {args.fin}")
    df = telecharger(args.station, args.debut, args.fin)
    resume(df)

    config.DATA_RAW.mkdir(parents=True, exist_ok=True)
    sortie = config.DATA_RAW / f"hourly_{args.station}.csv"
    df.to_csv(sortie)
    print(f"\nEnregistré : {sortie}")


if __name__ == "__main__":
    main()
