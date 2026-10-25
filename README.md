# ETRS923 – IA embarquée pour la prédiction météo sur STM32N6

Projet de Master 2 (Université Savoie Mont Blanc) : acquisition de grandeurs
météo via la carte capteurs X-NUCLEO-IKS01A3, communication réseau, puis
inférence d'un modèle d'IA sur la NUCLEO-N657X0-Q (NPU Neural-ART).

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
├── ml/                  # TP3 : données, notebooks, modèles
├── docs/                # Captures de logs, schémas, notes pour l'oral
└── README.md
```

## Avancement

- [x] TP2.1 – LEDs, chenillard 3 s, logs console
- [x] TP2.2 – Lecture capteurs I²C (SHT40, LPS22DF, STTS22H sur IKS4A1) + LEDs d'état
- [x] TP2.3 – FreeRTOS (tâches capteurs / LED / réseau, timebase HAL sur TIM6)
- [x] TP2.3 – LwIP intégré (Ethernet RMII + PHY LAN8742, DHCP, DNS, ICMP, sockets BSD)
- [x] TP2.3 – Ping vers google.com (DNS + ICMP par socket RAW) + LED bleue
- [ ] TP3 – Modèle de prédiction météo
- [ ] Intégration de l'inférence sur le NPU

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
#define DEMO_PARTIE        3
```

| Valeur | Ce qui tourne | LEDs |
|---|---|---|
| `1` | 3 LEDs allumées 3 s, puis chenillard rouge → vert → bleu (3 s), logs `<ROUGE>`… | toutes |
| `2` | Lecture des 3 capteurs toutes les 5 s | rouge = lecture, vert = en attente |
| `3` | Capteurs + réseau : DHCP, ping google.com toutes les 10 s | + bleu = communication réseau |

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

## Après chaque génération de code CubeMX

- Vérifier que `Appli/Core/Src/app_freertos.c` et `Appli/Core/Inc/app_freertos.h` ne
  contiennent pas de texte `FreeMarker template error` (bug constaté avec X-CUBE-FREERTOS 1.3.1).
- Compiler avant de committer.
