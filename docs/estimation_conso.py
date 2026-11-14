"""
estimation_conso.py - TP3.3d : estimation théorique de l'énergie d'une
prédiction, sur la carte (edge) et dans le cloud (ThingSpeak + MATLAB).

Toutes les hypothèses sont regroupées en haut du fichier, avec leur source
(voir docs/demarche_tp3.md, section 10). Lancer : python estimation_conso.py
"""

# ---------------------------------------------------------------------------
# Mesuré sur la carte (auto-test, démo 5)
# ---------------------------------------------------------------------------
CYCLES_INFERENCE = 49_000          # cycles CPU pour une prédiction (un modèle)
F_ACTUELLE_MHZ = 64                # fréquence actuelle du Cortex-M55

# ---------------------------------------------------------------------------
# Datasheet STM32N657 (DS14791), tableau 34 : IDDCORE en Run, 25 °C, code en
# ITCM, périphériques désactivés, SMPS externe. Tableau 43 : Stop.
# ---------------------------------------------------------------------------
POINTS_RUN = {                     # f (MHz) : (IDDCORE mA, VDDCORE V)
    600: (66.0, 0.81),             # VOS low
    800: (85.0, 0.89),             # VOS high (overdrive)
}
IDDCORE_STOP_MA = 9.06             # part statique (fuites), VOS high

# ---------------------------------------------------------------------------
# Cloud
# ---------------------------------------------------------------------------
PHY_W = 0.163                      # LAN8742A, 100BASE-TX avec trafic (tableau 5.3)
DUREE_ECHANGES_S = (1.5, 20.0)     # PHY utile : ~6 requêtes HTTP / tout le cycle
VOLUME_GO = 50e-6                  # ~50 ko échangés par prédiction (estimation)
RESEAU_KWH_PAR_GO = (0.06 / 2**5, 0.06)  # Aslan 2018 : 0,06 en 2015, /2 tous les 2 ans
VCPU_MAX_W = 3.5                   # Cloud Carbon Footprint, AWS, max par vCPU
DUREE_MATLAB_S = (5.0, 10.0)       # exécution MATLAB par prédiction (estimation)
PUE = (1.1, 1.58)                  # data center efficace / moyenne Uptime Institute


def energie_carte(f_mhz):
    """Énergie (J) d'une prédiction à f_mhz. Entre deux points connus, modèle
    linéaire I = I_statique + k * f calé sur le point à 600 MHz."""
    if f_mhz in POINTS_RUN:
        i_ma, v = POINTS_RUN[f_mhz]
    else:
        i600, v = POINTS_RUN[600]
        i_ma = IDDCORE_STOP_MA + (i600 - IDDCORE_STOP_MA) * f_mhz / 600
    puissance_w = i_ma * 1e-3 * v
    duree_s = CYCLES_INFERENCE / (f_mhz * 1e6)
    return puissance_w, duree_s, puissance_w * duree_s


def main():
    print("=== Carte (une prédiction, un modèle) ===")
    for f in (F_ACTUELLE_MHZ, 600, 800):
        p, t, e = energie_carte(f)
        print(f"  {f:4d} MHz : {p*1e3:5.1f} mW x {t*1e6:6.1f} us = {e*1e6:5.2f} uJ")

    print("\n=== Cloud (une prédiction, bornes basse / haute) ===")
    e_phy = [PHY_W * d for d in DUREE_ECHANGES_S]
    e_net = [VOLUME_GO * k * 3.6e6 for k in RESEAU_KWH_PAR_GO]
    e_dc = [VCPU_MAX_W * DUREE_MATLAB_S[0] * PUE[0], VCPU_MAX_W * DUREE_MATLAB_S[1] * PUE[1]]
    for nom, e in (("PHY Ethernet de la carte", e_phy), ("Réseau Internet", e_net),
                   ("Serveur MATLAB (data center)", e_dc)):
        print(f"  {nom:30s} {e[0]:7.2f} J  a {e[1]:7.2f} J")
    tot = [sum(x) for x in zip(e_phy, e_net, e_dc)]
    print(f"  {'TOTAL':30s} {tot[0]:7.2f} J  a {tot[1]:7.2f} J")

    e_edge = energie_carte(F_ACTUELLE_MHZ)[2]
    print(f"\nRapport cloud / carte (64 MHz) : {tot[0]/e_edge:.1e} a {tot[1]/e_edge:.1e}")


if __name__ == "__main__":
    main()
