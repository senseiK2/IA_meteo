# ETRS923 – IA embarquée pour la prédiction météo sur STM32N6

Projet réalisé dans le cadre du Master 2, semestre 9 (Université Savoie Mont
Blanc, module ETRS923 « IA et Objets Connectés ») : acquisition de grandeurs
météo via la carte capteurs X-NUCLEO-IKS4A1, envoi sur le cloud (ThingSpeak +
MATLAB), puis prédiction de la météo par un réseau de neurones exécuté sur la
NUCLEO-N657X0-Q (Cortex-M55) et comparée à celle du cloud.

## Équipe

- Kherradji Yacine
- Trimech Wadah
- Trache Aris

## Matériel

| Élément | Détail |
|---|---|
| Carte MCU | NUCLEO-N657X0-Q (STM32N657X0H3Q, Cortex-M55, 4,2 Mo SRAM, pas de Flash interne) |
| Carte capteurs | X-NUCLEO-IKS4A1 (SHT40, LPS22DF, STTS22H…) ou X-NUCLEO-IKS01A3 (HTS221, LPS22HH, STTS751…), détectée automatiquement |
| Console | USART1 via ST-LINK VCP (PE5 TX / PE6 RX), 115200 8N1 |
| Bus capteurs | I2C1 sur connecteur Arduino : PH9 (SCL) / PC1 (SDA) |
| Adresses I²C (7 bits) | IKS4A1 : SHT40 0x44, LPS22DF 0x5D (ID 0xB4), STTS22H 0x38 — IKS01A3 : HTS221 0x5F, LPS22HH 0x5D (ID 0xB3), STTS751 0x4A |
| LEDs | Rouge PG10, Verte PG0, Bleue PG8 (actives à l'état bas) |
| Ethernet | ETH1 en RMII : PF7 REF_CLK, PF10 CRS_DV, PF11 TX_EN, PF12/PF13 TXD0/1, PF14/PF15 RXD0/1 ; PHY LAN8742 sur MDIO PF4 / MDC PG11 |

## Structure du dépôt

```
.
├── firmware/
│   └── IA_meteo/        # Projet STM32CubeIDE : FSBL (démarrage) + Appli (application)
├── ml/                  # TP3 : données Meteostat, entraînement TensorFlow, export ONNX/TFLite (voir ml/README.md)
├── web/                 # Page de suivi de la démo (carte et cloud côte à côte)
├── docs/                # Documentation : démarche, choix de conception, logs
└── README.md
```

## Avancement

- [x] TP2.1 – LEDs, chenillard 3 s, logs console
- [x] TP2.2 – Lecture capteurs I²C (SHT40, LPS22DF, STTS22H sur IKS4A1) + LEDs d'état
- [x] TP2.3 – FreeRTOS (tâches capteurs / LED / réseau, timebase HAL sur TIM6)
- [x] TP2.3 – LwIP intégré (Ethernet RMII + PHY LAN8742, DHCP, DNS, ICMP, sockets BSD)
- [x] TP2.3 – Ping vers google.com (DNS + ICMP par socket RAW) + LED bleue
- [x] TP3.1 – Pipeline d'entraînement (Meteostat → TensorFlow → ONNX / TFLite), étude du compromis
- [x] TP3.1 – Étude du compromis, choix et export des modèles (ONNX, TFLite float32 / int8)
- [x] TP3.2 – Inférence MATLAB sur ThingSpeak, déclenchée par React, réponse par TalkBack
- [x] TP3.3 – Inférence sur le STM32N6 (X-CUBE-AI), comparaison cloud / edge, consommation (estimation théorique)

## Lancer le firmware

1. Placer la carte en **mode développement** (BOOT0 = 0, BOOT1 = 1), puis la réalimenter.
2. Ouvrir le projet dans STM32CubeIDE et lancer **Debug** sur le projet **IA_meteo_Appli**.
3. Ouvrir un terminal série sur le port ST-LINK VCP à 115200 bauds.

Tout le code applicatif est dans `Appli/Core` : le FSBL est limité à 255 Ko de code
et 256 Ko de RAM, Appli dispose de 2 Mo de RAM (nécessaire pour FreeRTOS, LwIP et
le modèle d'IA). En mode développement, le débogueur charge Appli directement en RAM.

> En mode développement, le programme est chargé en RAM : il est perdu à la coupure d'alimentation.

### Choisir la partie du TP à montrer

Dans `Appli/Core/Src/app_freertos.c`, une seule ligne à changer :

```c
#define DEMO_PARTIE        6
```

| Valeur | Ce qui tourne | LEDs |
|---|---|---|
| `1` | 3 LEDs allumées 3 s, puis chenillard rouge → vert → bleu (3 s), logs `<ROUGE>`… | toutes |
| `2` | Lecture des 3 capteurs toutes les 5 s | rouge = lecture, vert = en attente |
| `3` | Capteurs + réseau : DHCP, ping google.com toutes les 10 s | + bleu = communication réseau |
| `4` | Capteurs + envoi des mesures sur ThingSpeak toutes les 20 s (TP3.2) | + bleu = communication réseau |
| `5` | Capteurs + réseau de neurones exécuté sur la carte (TP3.3) : auto-test et temps de calcul | rouge / vert |
| `6` | Démo finale (TP3.3c) : prédiction sur la carte toutes les 20 s (fonctionne sans réseau) ; mesures et prédictions envoyées sur ThingSpeak et comparées à celles du cloud | météo prédite (h0) : vert = Clair, vert + bleu = Nuageux, rouge = Brouillard, bleu = Précipitations |

**Démo 6 sans réseau.** La tâche `TacheIA` prédit indépendamment de
`TacheReseau`. L'heure (4 des 8 entrées du modèle) vient de l'horloge
logicielle `horloge.c` : date de compilation au démarrage (approximative),
puis heure exacte dès qu'un serveur répond (en-tête HTTP `Date:`). La date de
compilation est celle de la dernière compilation de `horloge.c` : faire
*Project → Clean* avant de compiler pour la mettre à jour.

**Démos 4 et 6 : clés ThingSpeak.** Copier `Appli/Core/Inc/secrets_exemple.h` en
`Appli/Core/Inc/secrets.h` et y mettre la *Write API Key* du canal.
`secrets.h` est exclu du dépôt (`.gitignore`) : la clé ne doit jamais être
publiée. Sans ce fichier, la compilation s'arrête avec un message explicite.

Canal ThingSpeak : `field1` = température (°C, SHT40), `field2` = humidité (%),
`field3` = pression mesurée par la carte (hPa, non corrigée de l'altitude).
Sans box, la carte peut être branchée sur le port Ethernet d'un PC Windows qui
partage sa connexion Wi-Fi (*Partage de connexion Internet* : la carte reçoit
une adresse `192.168.137.x`).

Les messages de plusieurs tâches ne se mélangent pas sur la console : chaque bloc
de `printf` est protégé par un mutex (`console.c`).

## Réseau (LwIP)

LwIP n'est pas proposé par CubeMX pour le STM32N6 : il est intégré à la main.

| Élément | Emplacement |
|---|---|
| Sources LwIP 2.2.1 (version ST, port CMSIS-RTOS2) | `Appli/Middlewares/Third_Party/LwIP` |
| Driver du PHY LAN8742 | `Appli/Drivers/BSP/Components/lan8742` |
| Liaison HAL ETH ↔ LwIP (mode FreeRTOS) | `Appli/Core/Src/ethernetif.c` |
| Démarrage du réseau, DHCP, affichage de l'IP | `Appli/Core/Src/app_ethernet.c` |
| Options LwIP | `Appli/Core/Inc/lwipopts.h` |

Points propres au STM32N6 :
- **RIF** : le DMA Ethernet doit être déclaré Secure pour accéder à la RAM
  (`ethernetif_rif_config()`, appelé avant `MX_ETH1_Init()`).
- **Horloges** ETH1 MAC/TX/RX activées dans `HAL_ETH_MspInit` (zone USER CODE).
- **Descripteurs DMA** alignés sur 32 octets (section `.eth_descriptors` du script de link).

Chemins d'include ajoutés au projet Appli (*Properties > C/C++ Build > Settings >
MCU GCC Compiler > Include paths*). Si une génération CubeMX les supprime, les remettre :
```
../Middlewares/Third_Party/LwIP/src/include
../Middlewares/Third_Party/LwIP/system
../Drivers/BSP/Components/lan8742
```

## IA embarquée (X-CUBE-AI)

Les deux réseaux de neurones (`h0` : temps actuel, `h3` : prévision à +3 h)
sont convertis en C par **X-CUBE-AI 10.2.0** (ST Edge AI
Core 2.2.0) en ligne de commande, à partir du modèle ONNX. Il n'est pas
intégré par CubeMX : dans ce projet à trois contextes, CubeMX rattache le pack
au contexte *External Memory Loader*, et une régénération complète mettrait en
risque l'intégration manuelle de LwIP.

```bat
set STEDGEAI=%USERPROFILE%\STM32Cube\Repository\Packs\STMicroelectronics\X-CUBE-AI\10.2.0\Utilities\windows\stedgeai.exe
"%STEDGEAI%" analyze  -m ml\models\export\modele_meteo_h0.onnx --target stm32n6 --name meteo_h0
"%STEDGEAI%" generate -m ml\models\export\modele_meteo_h0.onnx --target stm32n6 --name meteo_h0 -o st_ai_output\h0
"%STEDGEAI%" generate -m ml\models\export\modele_meteo_h3.onnx --target stm32n6 --name meteo_h3 -o st_ai_output\h3
```

| Élément | Emplacement |
|---|---|
| Réseaux générés (`meteo_h0*.c/.h`, `meteo_h3*.c/.h`, rapports de génération) | `Appli/X-CUBE-AI/App` |
| Bibliothèque d'exécution Cortex-M55 et ses en-têtes | `Appli/Middlewares/ST/AI` |
| Normalisation, classes, vecteurs de test (générés par `ml/04_exporter.py`) | `Appli/Core/Inc/meteo_modele_h0.h`, `meteo_modele_h3.h` |
| Calcul des entrées, appel des réseaux, chronométrage, auto-test | `Appli/Core/Src/meteo_ia.c` |

Chaque réseau (484 paramètres, 576 multiplications-additions, 128 octets de
mémoire de travail) tourne sur le Cortex-M55 en flottant 32 bits ; les deux
partagent la même bibliothèque d'exécution.

Réglages ajoutés au projet Appli (à remettre si une génération CubeMX les
supprime) :
- *Include paths* : `../X-CUBE-AI/App`, `../Middlewares/ST/AI/Inc` ;
- *Source location* : dossier `X-CUBE-AI` ;
- *Linker > Libraries* : `:NetworkRuntime1020_CM55_GCC.a`, chemin
  `../Middlewares/ST/AI/Lib`.

## Page de suivi (`web/index.html`)

Page autonome à ouvrir dans un navigateur pendant la démo 6. En bandeau, une
scène animée (soleil ou lune, nuages, brouillard, pluie ou neige, jour ou nuit
selon la position du soleil à Chambéry) montre la prédiction de la carte, avec
une vignette pour la prévision à +3 h. Elle lit les deux
canaux ThingSpeak toutes les 20 s et affiche, pour chaque mesure, la
prédiction de la carte et celle du cloud, leur accord, les temps de réponse sur
une échelle logarithmique, les mesures et l'historique.

| Canal | Écrit par | Champs |
|---|---|---|
| `TP3_Meteo_STM32` | la carte (démo 6) | 1–3 mesures ; 4/5 classe et confiance h0 ; 6/7 classe et confiance h3 ; 8 temps d'inférence h0 (µs) |
| `TP3_Meteo_Cloud` | MATLAB (`matlab/prediction_meteo.m`) | 1/2 classe et confiance h0 ; 3/4 classe et confiance h3 ; 5 numéro de la mesure ; 6 durée du script (s) |

Aucune clé n'est écrite dans le fichier : au premier lancement, la page demande
les numéros des canaux et leurs clés de lecture, puis les garde dans le
navigateur.

## Après chaque génération de code CubeMX

- Vérifier que `Appli/Core/Src/app_freertos.c` et `Appli/Core/Inc/app_freertos.h` ne
  contiennent pas de texte `FreeMarker template error` (bug constaté avec X-CUBE-FREERTOS 1.3.1).
- Compiler avant de committer.
