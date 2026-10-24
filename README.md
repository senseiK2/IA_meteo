# ETRS923 – IA embarquée pour la prédiction météo sur STM32N6

Projet de Master 2 (Université Savoie Mont Blanc) : acquisition de grandeurs
météo via la carte capteurs X-NUCLEO-IKS01A3, communication réseau, puis
inférence d'un modèle d'IA sur la NUCLEO-N657X0-Q (NPU Neural-ART).

## Matériel

| Élément | Détail |
|---|---|
| Carte MCU | NUCLEO-N657X0-Q (STM32N657X0H3Q, Cortex-M55, 4,2 Mo SRAM, pas de Flash interne) |
| Carte capteurs | X-NUCLEO-IKS01A3 (HTS221, LPS22HH, STTS751, LSM6DSO, LIS2MDL, LIS2DW12) |
| Console | USART1 via ST-LINK VCP (PE5 TX / PE6 RX), 115200 8N1 |
| Bus capteurs | I2C1 sur connecteur Arduino : PH9 (SCL) / PC1 (SDA) |
| Adresses I²C (7 bits) | HTS221 0x5F, LPS22HH 0x5D, STTS751 0x48–0x4B (détectée au démarrage) |
| LEDs | Rouge PG10, Verte PG0, Bleue PG8 (actives à l'état bas) |

## Structure du dépôt

```
.
├── firmware/
│   └── IA_meteo/        # Projet STM32CubeIDE (FSBL + Appli)
├── ml/                  # TP3 : données, notebooks, modèles
├── docs/                # Captures de logs, schémas, notes pour l'oral
└── README.md
```

## Avancement

- [x] TP2.1 – LEDs, chenillard 3 s, logs console
- [ ] TP2.2 – Lecture capteurs I²C (HTS221, LPS22HH, STTS751) + LEDs d'état
- [ ] TP2.3 – FreeRTOS + LwIP (DHCP, ICMP, sockets BSD), ping, LED bleue
- [ ] TP3 – Modèle de prédiction météo
- [ ] Intégration de l'inférence sur le NPU

## Lancer le firmware

1. Placer la carte en **mode développement** (BOOT0 = 0, BOOT1 = 1), puis la réalimenter.
2. Ouvrir le projet dans STM32CubeIDE et lancer **Debug** sur le projet FSBL.
3. Ouvrir un terminal série sur le port ST-LINK VCP à 115200 bauds.

> En mode développement, le programme est chargé en RAM : il est perdu à la coupure d'alimentation.
