# TP3.1 – Modèle de classification météo (Python / TensorFlow)

On entraîne un petit réseau de neurones qui reconnaît la condition météo (clair,
nuageux, brouillard, précipitations…) à partir de **ce que la carte mesure** :
température, humidité et pression. Ensuite ce même modèle part dans le cloud
(MATLAB / ThingSpeak, TP3.2) et sur le STM32N6 (X-CUBE-AI, TP3.3).

## Lancer

**Le plus simple : Google Colab.** Rien à installer, TensorFlow est déjà là.
[Ouvrir le notebook dans Colab](https://colab.research.google.com/github/senseiK2/IA_meteo/blob/main/ml/TP3_colab.ipynb),
puis exécuter les cellules dans l'ordre.

**Sur le PC** (Python 3.10 à 3.12) :

```bash
cd ml
python -m venv .venv && source .venv/bin/activate   # Windows : .venv\Scripts\activate
pip install -r requirements.txt
pip install --no-deps tf2onnx        # sans ses dépendances : sinon il rétrograde protobuf

python 01_donnees.py                 # télécharge l'historique Meteostat
python 03_etude_compromis.py         # TP3.1b : compare classes / tailles / entrées
python 02_entrainer.py               # entraîne le modèle retenu
python 04_exporter.py                # ONNX + TFLite + fichiers pour MATLAB et le C
```

| Fichier | Rôle |
|---|---|
| `config.py` | **Tous les réglages** : station, période, entrées, regroupements de classes, hyperparamètres |
| `meteo_features.py` | Calcul des entrées et des étiquettes, découpage, normalisation (numpy / pandas) |
| `entrainement.py` | Modèle Keras, entraînement, métriques, graphiques |
| `01_donnees.py` … `04_exporter.py` | Les étapes, dans l'ordre |
| `TP3_colab.ipynb` | Les mêmes étapes dans un notebook Colab |
| `results/` | Résultats de l'étude du compromis (versionnés) |
| `models/<nom>/` | Un modèle entraîné : `.keras`, normalisation, métriques, courbes |
| `models/export/` | Le modèle retenu, exporté : `.onnx`, `.tflite`, `meteo_modele.h/.json`, vecteurs de test |

## Les choix, et pourquoi

### Données : Meteostat, station Chambéry–Aix (07491)

C'est la station synoptique la plus proche du campus, avec 11 ans (2015–2025)
d'observations horaires, soit environ 90 000 heures. Chaque heure contient la
température (`temp`), l'humidité (`rhum`), la pression **au niveau de la mer**
(`pres`) et un **code de condition météo** (`coco`, 1 à 27), qui nous sert
d'étiquette.

### Entrées : seulement ce que la carte sait calculer

Un modèle entraîné avec le vent ou la nébulosité serait meilleur sur le papier,
mais il serait inutilisable sur la carte, qui n'a pas ces capteurs. On se limite
donc à :

| Entrée | Origine sur la carte | Pourquoi |
|---|---|---|
| `temp`, `rhum` | SHT40 | |
| `pres` | LPS22DF, **ramenée au niveau de la mer** (altitude de la carte) | Meteostat donne la pression au niveau de la mer. À 235 m, la carte lit environ 28 hPa de moins. |
| `ecart_rosee` | calculé (formule de Magnus) | Écart proche de 0 = air saturé = brouillard ou pluie. C'est plus parlant pour le réseau que l'humidité seule. |
| `heure_sin/cos`, `jour_sin/cos` | heure UTC (SNTP) | Cycles jour/nuit et saisons. Le sinus et le cosinus évitent que 23 h paraisse « loin » de 0 h. |
| `dp3h` | historique de 3 h | Une pression qui chute annonce la pluie. En contrepartie, il faut 3 h de mesures avant la première prédiction. |

Les formules de `meteo_features.py` doivent être reproduites **à l'identique**
dans MATLAB et en C. Le fichier `vecteurs_test.csv` sert à le vérifier.

### Étiquettes : regrouper les 27 codes

On regroupe les 27 codes Meteostat en schémas de 3, 4, 6 ou 13 classes
(`SCHEMAS_CLASSES`). Le schéma « tp_complet » reprend la liste du sujet. Certaines
classes ne sont pas reconnaissables avec nos capteurs. Le **vent fort**, par
exemple, demande un anémomètre. D'autres sont trop rares à Chambéry pour être
apprises : l'**orage violent** compte quelques heures par an. Ces classes font
chuter les scores sans rien apporter. C'est l'argument principal du compromis.

### Découpage chronologique

On entraîne sur 2015–2023, on règle sur 2024 (validation) et on évalue sur 2025
(test). Un découpage aléatoire tricherait : 14 h dans le test et 13 h dans
l'entraînement, c'est presque la même mesure.

### Modèle : perceptron multicouche (MLP)

Le modèle est `entrées → Dense(ReLU) × 1 à 3 → Dense(softmax)`. Avec 3 à 9
nombres en entrée, un CNN ou un LSTM n'apporterait rien. Les opérations du MLP
(produit matriciel, ReLU, softmax) sont supportées partout : ONNX, MATLAB,
X-CUBE-AI et le NPU du STM32N6. Les tailles testées vont de `XS` (une couche
de 8 neurones, une centaine de paramètres) à `L` (64-64-32, plusieurs milliers
de paramètres).

### Hyperparamètres

| Hyperparamètre | Valeur | Justification |
|---|---|---|
| Optimiseur | Adam, lr = 1e-3 | Valeur standard, stable sans réglage fin. `ReduceLROnPlateau` divise le pas par 2 quand la validation stagne. |
| Fonction de perte | entropie croisée (sparse) | Classification multi-classes. Les étiquettes sont des entiers. |
| Batch | 256 | Environ 250 lots par époque : rapide sur CPU, gradient peu bruité. |
| Époques | max 150, *early stopping* (patience 12) | On s'arrête quand la perte de validation ne baisse plus, et on revient aux meilleurs poids. Pas de nombre d'époques arbitraire. |
| Dropout | 0,1 | Régularisation légère : le réseau est petit et les données nombreuses. |
| Poids des classes | inverses des fréquences | Sans eux, répondre « Nuageux » à tout donne déjà une bonne accuracy. |
| Normalisation | centrage-réduction (stats du train) | La pression (≈ 1015) écraserait les autres entrées. Moyenne et écart-type sont exportés pour MATLAB et le C. |
| Graine | 42 | Résultats reproductibles. |

### Métriques

- **accuracy** : part des heures bien classées. Elle est trompeuse quand une
  classe domine.
- **accuracy équilibrée** (*balanced accuracy*) : moyenne des rappels par
  classe. Chaque classe compte autant, d'où le choix de cet indicateur pour le
  compromis.
- **référence majoritaire** : l'accuracy obtenue en répondant toujours la classe
  la plus fréquente. Un modèle qui ne la bat pas n'a rien appris.
- **taille** : nombre de paramètres, soit environ le nombre d'octets en int8.

## Résultats

*(À compléter avec `results/etude_compromis.md` après exécution.)*
