"""
04_exporter.py - Exporte un modèle entraîné pour MATLAB (TP3.2) et le STM32 (TP3.3).

Utilisation :
    python 04_exporter.py                          # modèle par défaut de config.py
    python 04_exporter.py --modele 6_classes_M_temps_h0

Chaîne d'export :
    model.keras ──► ONNX   (tf2onnx)        -> MATLAB : importONNXNetwork
                └─► TFLite float32 / int8   -> X-CUBE-AI / NPU du STM32N6
                    (+ l'ONNX, aussi accepté par X-CUBE-AI)

Fichiers produits dans models/export/ (noms fixes : MATLAB les télécharge
depuis GitHub, le firmware les inclut, sans changer de code à chaque modèle) :
    modele_meteo.onnx            réseau au format ONNX
    modele_meteo_float.tflite    réseau TFLite 32 bits (référence)
    modele_meteo_int8.tflite     réseau TFLite quantifié 8 bits (NPU)
    meteo_modele.json            entrées, normalisation, classes (pour MATLAB)
    meteo_modele.h               idem en C (pour le firmware)
    vecteurs_test.csv            exemples réels + réponse attendue : MATLAB et
                                 la carte doivent retrouver les MÊMES résultats
    export.json                  vérifications (écarts Keras/ONNX/TFLite)
"""
import argparse
import json
import shutil
from datetime import datetime

import numpy as np

import config
from entrainement import charger_brut, nom_experience, preparer, evaluer

EXPORT_DIR = config.MODELS_DIR / "export"
OPSET = 13   # version des opérateurs ONNX : bien supportée par MATLAB et X-CUBE-AI


# ---------------------------------------------------------------------------
# ONNX
# ---------------------------------------------------------------------------
def onnx_manuel(modele, n_entrees, chemin):
    """Plan C : écrit le graphe ONNX "à la main" à partir des poids.

    Un MLP n'est qu'une suite de  y = activation(x · W + b). On recrée
    exactement ces opérations (MatMul, Add, Relu, Softmax) avec la librairie
    onnx. Utile si tf2onnx est incompatible avec la version de TensorFlow
    installée, et instructif : c'est tout ce que contient le fichier .onnx.
    """
    import onnx
    from onnx import helper, numpy_helper, TensorProto

    noeuds, poids = [], []
    x = "input"
    denses = [c for c in modele.layers if c.__class__.__name__ == "Dense"]
    for i, couche in enumerate(denses):
        W, b = couche.get_weights()
        poids += [numpy_helper.from_array(W.astype(np.float32), f"W{i}"),
                  numpy_helper.from_array(b.astype(np.float32), f"b{i}")]
        noeuds += [helper.make_node("MatMul", [x, f"W{i}"], [f"mm{i}"]),
                   helper.make_node("Add", [f"mm{i}", f"b{i}"], [f"z{i}"])]
        act = couche.get_config()["activation"]
        sortie = "output" if i == len(denses) - 1 else f"a{i}"
        if act == "relu":
            noeuds.append(helper.make_node("Relu", [f"z{i}"], [sortie]))
        elif act == "softmax":
            noeuds.append(helper.make_node("Softmax", [f"z{i}"], [sortie], axis=-1))
        else:
            raise ValueError(f"activation non gérée : {act}")
        x = sortie
    n_classes = denses[-1].get_weights()[1].shape[0]
    graphe = helper.make_graph(
        noeuds, "meteo_mlp",
        [helper.make_tensor_value_info("input", TensorProto.FLOAT, ["N", n_entrees])],
        [helper.make_tensor_value_info("output", TensorProto.FLOAT, ["N", n_classes])],
        poids)
    m = helper.make_model(graphe, opset_imports=[helper.make_opsetid("", OPSET)],
                          producer_name="ia_meteo")
    m.ir_version = 8          # lisible par les versions récentes ET anciennes d'onnxruntime
    onnx.checker.check_model(m)
    onnx.save(m, str(chemin))


def exporter_onnx(modele, n_entrees, chemin):
    """Essaie tf2onnx comme dans le sujet, avec des solutions de repli.

    tf2onnx a du mal à suivre les nouvelles versions de Keras (Keras 3 depuis
    TensorFlow 2.16) : on tente from_keras (exemple du sujet), puis
    from_function, puis l'écriture manuelle. Le résultat est vérifié ensuite
    avec onnxruntime, quelle que soit la méthode.
    """
    import tensorflow as tf
    spec = (tf.TensorSpec((None, n_entrees), tf.float32, name="input"),)

    try:
        import tf2onnx
        if not hasattr(modele, "output_names"):          # contournement Keras 3
            modele.output_names = ["output"]
        tf2onnx.convert.from_keras(modele, input_signature=spec, opset=OPSET,
                                   output_path=str(chemin))
        return "tf2onnx.from_keras"
    except Exception as e:                               # noqa: BLE001
        print(f"  from_keras impossible ({type(e).__name__}: {e})")

    try:
        import tf2onnx

        @tf.function(input_signature=spec)
        def f(x):
            return modele(x, training=False)
        tf2onnx.convert.from_function(f, input_signature=spec, opset=OPSET,
                                      output_path=str(chemin))
        return "tf2onnx.from_function"
    except Exception as e:                               # noqa: BLE001
        print(f"  from_function impossible ({type(e).__name__}: {e})")

    onnx_manuel(modele, n_entrees, chemin)
    return "onnx manuel"


def predire_onnx(chemin, X):
    import onnxruntime as ort
    s = ort.InferenceSession(str(chemin), providers=["CPUExecutionProvider"])
    return s.run(None, {s.get_inputs()[0].name: X})[0]


# ---------------------------------------------------------------------------
# TFLite
# ---------------------------------------------------------------------------
def exporter_tflite(modele, n_entrees, X_repr, chemin_float, chemin_int8):
    """Batch fixé à 1 : sur la carte, on classe une mesure à la fois, et
    X-CUBE-AI préfère des dimensions fixes (mémoire réservée à l'avance)."""
    import tensorflow as tf

    @tf.function(input_signature=[tf.TensorSpec([1, n_entrees], tf.float32, name="input")])
    def f(x):
        return modele(x, training=False)
    concrete = f.get_concrete_function()

    conv = tf.lite.TFLiteConverter.from_concrete_functions([concrete], modele)
    chemin_float.write_bytes(conv.convert())

    # Quantification int8 : chaque poids et chaque activation est stocké sur
    # 8 bits au lieu de 32 (modèle 4x plus petit, calcul entier = ce que le
    # NPU sait faire). Le convertisseur a besoin d'exemples réels
    # ("representative dataset") pour connaître la plage de valeurs de chaque
    # couche et choisir l'échelle de chaque tenseur.
    def representatif():
        for i in range(min(1000, len(X_repr))):
            yield [X_repr[i:i + 1]]

    conv = tf.lite.TFLiteConverter.from_concrete_functions([concrete], modele)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = representatif
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    # Entrée / sortie laissées en float32 : le convertisseur ajoute lui-même
    # les étapes Quantize / Dequantize au début et à la fin du graphe.
    chemin_int8.write_bytes(conv.convert())


def predire_tflite(chemin, X):
    import tensorflow as tf
    it = tf.lite.Interpreter(model_path=str(chemin))
    it.allocate_tensors()
    e, s = it.get_input_details()[0], it.get_output_details()[0]
    sorties = []
    for i in range(len(X)):
        it.set_tensor(e["index"], X[i:i + 1])
        it.invoke()
        sorties.append(it.get_tensor(s["index"])[0].copy())
    return np.array(sorties)


class _ModeleNumpy:
    """Enveloppe minimale pour réutiliser evaluer() avec des probas déjà calculées."""
    def __init__(self, proba):
        self.proba = proba

    def predict(self, X, **_):
        return self.proba


# ---------------------------------------------------------------------------
# Fichiers pour MATLAB et le C
# ---------------------------------------------------------------------------
def vecteurs_test(d, proba, n_par_classe=3):
    """Quelques heures réelles de l'année de test, prises dans chaque classe."""
    Xn, y = d["test"]
    rng = np.random.default_rng(0)
    choix = []
    for c in range(len(d["classes"])):
        idx = np.flatnonzero(y == c)
        choix += list(rng.choice(idx, size=min(n_par_classe, len(idx)), replace=False))
    choix = np.array(sorted(choix))
    X_brut = Xn[choix] * d["norm"].ecart_type + d["norm"].moyenne   # avant normalisation
    return choix, X_brut, y[choix], proba[choix]


def ecrire_csv_vecteurs(chemin, d, X_brut, y, proba):
    entetes = d["entrees"] + ["classe_reelle", "classe_attendue"] + \
              [f"p_{c}" for c in d["classes"]]
    with open(chemin, "w", encoding="utf-8") as f:
        f.write(",".join(entetes) + "\n")
        for x, yr, p in zip(X_brut, y, proba):
            f.write(",".join([f"{v:.6g}" for v in x] + [str(int(yr)), str(int(p.argmax()))]
                             + [f"{v:.6f}" for v in p]) + "\n")


def ecrire_entete_c(chemin, info, X_brut, proba):
    n_e, n_c = len(info["entrees"]), len(info["classes"])

    def flottant_c(v):
        s = f"{float(v):.8g}"
        if not any(ch in s for ch in ".e"):     # "10f" n'est pas du C valide
            s += ".0"
        return s + "f"

    def tab(vals):
        return ", ".join(flottant_c(v) for v in vals)

    l = []
    l.append("/**")
    l.append("  * @file    meteo_modele.h")
    l.append("  * @brief   Paramètres du modèle météo, générés par ml/04_exporter.py.")
    l.append("  *          NE PAS MODIFIER À LA MAIN : relancer le script.")
    l.append(f"  *          Modèle : {info['modele']} ({info['date_export']})")
    l.append("  *")
    l.append("  * Avant d'appeler le réseau, chaque entrée doit être :")
    l.append("  *   1. calculée comme dans ml/meteo_features.py (même ordre !)")
    l.append("  *   2. normalisée : x_norm[i] = (x[i] - METEO_MOYENNE[i]) / METEO_ECART_TYPE[i]")
    l.append("  * La sortie i du réseau est la probabilité de METEO_CLASSES[i].")
    l.append("  */")
    l.append("#ifndef METEO_MODELE_H")
    l.append("#define METEO_MODELE_H")
    l.append("")
    l.append(f"#define METEO_NB_ENTREES   {n_e}")
    l.append(f"#define METEO_NB_CLASSES   {n_c}")
    l.append(f"#define METEO_HORIZON_H    {info['horizon_h']}")
    l.append(f"#define METEO_ALTITUDE_M   {config.ALTITUDE_CARTE}.0f   "
             "/* altitude de la carte : pression -> niveau de la mer */")
    l.append("")
    l.append("/* Ordre des entrées : */")
    for i, nom in enumerate(info["entrees"]):
        l.append(f"#define METEO_E_{nom.upper():<12} {i}")
    l.append("")
    l.append(f"static const float METEO_MOYENNE[METEO_NB_ENTREES]    = {{ {tab(info['moyenne'])} }};")
    l.append(f"static const float METEO_ECART_TYPE[METEO_NB_ENTREES] = {{ {tab(info['ecart_type'])} }};")
    l.append("")
    noms = ", ".join(f'"{c}"' for c in info["classes"])
    l.append(f"static const char *const METEO_CLASSES[METEO_NB_CLASSES] = {{ {noms} }};")
    l.append("")
    l.append("/* Vecteurs de test : entrées brutes (avant normalisation) et classe")
    l.append("   attendue. La carte doit retrouver la même classe (auto-test). */")
    l.append(f"#define METEO_NB_VECTEURS_TEST {len(X_brut)}")
    l.append("static const float METEO_TEST_ENTREES[METEO_NB_VECTEURS_TEST][METEO_NB_ENTREES] = {")
    for x in X_brut:
        l.append(f"  {{ {tab(x)} }},")
    l.append("};")
    l.append("static const unsigned char METEO_TEST_CLASSE[METEO_NB_VECTEURS_TEST] = { "
             + ", ".join(str(int(p.argmax())) for p in proba) + " };")
    l.append("")
    l.append("#endif /* METEO_MODELE_H */")
    chemin.write_text("\n".join(l) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
def main():
    defaut = nom_experience(config.SCHEMA_DEFAUT, config.TAILLE_DEFAUT,
                            config.JEU_ENTREES_DEFAUT, 0)
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--modele", default=defaut, help="dossier dans models/")
    args = p.parse_args()

    from tensorflow import keras

    dossier = config.MODELS_DIR / args.modele
    res = json.loads((dossier / "resultats.json").read_text(encoding="utf-8"))
    modele = keras.models.load_model(dossier / "model.keras")
    d = preparer(charger_brut(), res["schema"], res["jeu_entrees"], res["horizon_h"])
    Xte, yte = d["test"]
    n_e = len(d["entrees"])

    EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    p_keras = modele.predict(Xte, batch_size=4096, verbose=0)

    print("ONNX...")
    f_onnx = EXPORT_DIR / "modele_meteo.onnx"
    methode = exporter_onnx(modele, n_e, f_onnx)
    p_onnx = predire_onnx(f_onnx, Xte)
    print(f"  {methode} : écart max Keras/ONNX = {np.abs(p_onnx - p_keras).max():.2e}")

    print("TFLite...")
    f_float = EXPORT_DIR / "modele_meteo_float.tflite"
    f_int8 = EXPORT_DIR / "modele_meteo_int8.tflite"
    exporter_tflite(modele, n_e, d["train"][0], f_float, f_int8)
    p_float = predire_tflite(f_float, Xte)
    p_int8 = predire_tflite(f_int8, Xte)

    ev_keras = evaluer(_ModeleNumpy(p_keras), Xte, yte, d["classes"])
    ev_int8 = evaluer(_ModeleNumpy(p_int8), Xte, yte, d["classes"])
    verif = {
        "modele": args.modele,
        "methode_onnx": methode,
        "ecart_max_keras_onnx": float(np.abs(p_onnx - p_keras).max()),
        "ecart_max_keras_tflite_float": float(np.abs(p_float - p_keras).max()),
        "accord_classes_keras_int8": float(np.mean(p_int8.argmax(1) == p_keras.argmax(1))),
        "accuracy_test_float": ev_keras["accuracy"],
        "accuracy_test_int8": ev_int8["accuracy"],
        "balanced_accuracy_test_float": ev_keras["balanced_accuracy"],
        "balanced_accuracy_test_int8": ev_int8["balanced_accuracy"],
        "taille_octets": {f.name: f.stat().st_size for f in (f_onnx, f_float, f_int8)},
    }
    print(f"  écart max Keras/TFLite float = {verif['ecart_max_keras_tflite_float']:.2e}")
    print(f"  int8 : même classe que Keras pour {verif['accord_classes_keras_int8']:.1%} "
          f"des heures, accuracy {ev_keras['accuracy']:.3f} -> {ev_int8['accuracy']:.3f}")
    print(f"  tailles : {verif['taille_octets']}")

    info = {
        "modele": args.modele,
        "date_export": datetime.now().strftime("%Y-%m-%d %H:%M"),
        "station": config.STATION_ID, "altitude_carte_m": config.ALTITUDE_CARTE,
        "horizon_h": res["horizon_h"],
        "entrees": d["entrees"],
        "moyenne": [float(v) for v in d["norm"].moyenne],
        "ecart_type": [float(v) for v in d["norm"].ecart_type],
        "classes": d["classes"],
        "codes_coco": {c: config.SCHEMAS_CLASSES[res["schema"]][c] for c in d["classes"]},
    }
    (EXPORT_DIR / "meteo_modele.json").write_text(
        json.dumps(info, indent=2, ensure_ascii=False), encoding="utf-8")

    _, X_brut, y_v, p_v = vecteurs_test(d, p_keras)
    ecrire_csv_vecteurs(EXPORT_DIR / "vecteurs_test.csv", d, X_brut, y_v, p_v)
    ecrire_entete_c(EXPORT_DIR / "meteo_modele.h", info, X_brut, p_v)
    (EXPORT_DIR / "export.json").write_text(json.dumps(verif, indent=2), encoding="utf-8")
    shutil.copy(dossier / "model.keras", EXPORT_DIR / "modele_meteo.keras")
    print(f"\nExport terminé : {EXPORT_DIR}")


if __name__ == "__main__":
    main()
