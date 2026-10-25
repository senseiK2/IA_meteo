# Journal de bord – TP3 (notes pour l'oral)

Chaque étape : ce qu'on a fait, ce qu'on a observé, ce qu'on a décidé et pourquoi.
Les phrases en **gras** sont des arguments réutilisables pendant le pitch.

---

## Étape 1 – Les données (Meteostat)

**Ce qu'on a fait** : téléchargé l'historique horaire de la station
**Chambéry–Aix (07491)**, la plus proche du campus, de 2015 à 2025
(`ml/01_donnees.py`).

**Ce qu'on a observé**

- 96 366 heures (11 ans).
- Chaque heure = des mesures (température, humidité, pression, vent, pluie…)
  et un code `coco` (1 à 27) qui décrit le temps observé : clair, couvert,
  brouillard, pluie, orage… C'est la « correction » que le modèle doit
  apprendre à retrouver.
- Valeurs manquantes :

  | Colonne | Manquant | Mesurable par la carte ? |
  |---|---|---|
  | temp (température) | 0,1 % | oui (SHT40) |
  | rhum (humidité) | 0,4 % | oui (SHT40) |
  | pres (pression) | 0,1 % | oui (LPS22DF) |
  | vent, pluie, soleil, nuages | 0 à 100 % | non : pas de capteur |
  | **coco (l'étiquette)** | **25,7 %** | – |

  → environ **71 500 heures utilisables** (avec étiquette).

- Répartition très déséquilibrée : **« Couvert » = 42 % des heures**. Les
  conditions rares (neige ≈ 420 h, tempête 53 h, orage violent 17 h en 11 ans)
  ont trop peu d'exemples pour être apprises.
- Code 0 (228 h) : hors de la liste officielle → ignoré.

**Ce qu'on a décidé**

- Entrées du modèle limitées à **température, humidité, pression** :
  **« le modèle n'utilise que ce que la carte sait mesurer, sinon il serait
  inutilisable sur le terrain »**.

---

## Étape 2 – Le choix des classes (regroupements)

**Le problème** : 27 codes, c'est trop. Certains sont très rares, d'autres
impossibles à reconnaître avec nos capteurs (la tempête demande un
anémomètre). On regroupe les codes en quelques **classes**.

**Proposition 1 – 3 classes** (notre première idée)

| Classe | Codes coco | Part des heures |
|---|---|---|
| Clair | 1, 2 | 17 % |
| Nuageux | 3, 4, 5 | 62 % |
| Précipitations | tout le reste | 21 % |

- Utile (« faut-il un parapluie ? »), chaque classe a des milliers d'exemples.
- Limite : un modèle « paresseux » qui répond toujours *Nuageux* a déjà
  62 % de bonnes réponses. **Il faut battre cette référence pour prouver que
  le modèle a appris quelque chose.**

**Proposition 2 – 4 classes** : on sort le **Brouillard** (code 5).

| Classe | Codes coco | Part des heures |
|---|---|---|
| Clair | 1, 2 | 17 % |
| Nuageux | 3, 4 | 53 % |
| Brouillard | 5 | 9 % |
| Précipitations | tout le reste | 21 % |

- Argument : **le brouillard, c'est de l'air saturé d'eau (humidité ≈ 100 %)
  → c'est la condition que nos capteurs voient le mieux**.

**Décision** : ne pas deviner, **mesurer**. On entraîne les deux (et des
versions à 6 et 13 classes) et on compare (question 3.1b). → résultats à
l'étape « étude du compromis ».

**Phrase pour le pitch** : *« Plus de classes = plus d'information, mais plus
d'erreurs et besoin de plus d'exemples. On a choisi le nombre de classes en le
mesurant, pas au hasard. »*

---

## Étape 3 – Les entrées du modèle

*(à venir)*
