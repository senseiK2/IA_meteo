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

**L'idée** : le modèle ne voit que des nombres. On peut l'aider en lui
donnant des nombres **calculés** à partir des mesures ou de l'heure, sans
capteur supplémentaire. Toutes les formules sont dans
`ml/meteo_features.py` et devront être refaites **à l'identique** dans
MATLAB et sur la carte.

| Entrée | Calcul | Pourquoi |
|---|---|---|
| temp, rhum | SHT40 | mesures de base |
| pres | LPS22DF **ramenée au niveau de la mer** | Meteostat donne la pression au niveau de la mer ; à 235 m la carte lit ≈ 28 hPa de moins |
| ecart_rosee | temp − point de rosée (formule de Magnus) | écart ≈ 0 → air saturé → brouillard ou pluie |
| heure_sin, heure_cos | heure UTC sur un cercle | le brouillard est surtout nocturne / au petit matin |
| jour_sin, jour_cos | jour de l'année sur un cercle | saisons : neige l'hiver, orages l'été, brouillard l'automne |
| dp3h | pression maintenant − pression il y a 3 h | **une pression qui baisse annonce la pluie** (le baromètre) |

**Pourquoi sinus et cosinus ?** Avec un seul nombre, 23 h et 0 h semblent
très éloignés (23 contre 0) alors qu'ils se suivent. On place l'heure sur un
cercle, comme l'aiguille d'une horloge : ses deux coordonnées (cos, sin)
mettent 23 h juste à côté de 0 h. Même chose pour le 31 décembre et le
1er janvier.

**Les contreparties (à dire à l'oral)**

- Heure et saison → la carte doit connaître la date et l'heure → synchronisation
  par le réseau (SNTP).
- dp3h → il faut **3 h de mesures** avant la première prédiction : gênant pour
  une démo « clé en main ». Solutions possibles : récupérer l'historique sur
  ThingSpeak, ou se passer de cette entrée.

**Décision** : comme pour les classes, on **mesure** ce que chaque entrée
apporte. On compare 4 jeux d'entrées : mesures seules → + point de rosée →
+ heure/saison → + tendance de pression.

**Phrase pour le pitch** : *« Pas de capteur en plus : on tire plus
d'information des mêmes mesures, en calculant le point de rosée et la
tendance de la pression, comme un baromètre. »*
