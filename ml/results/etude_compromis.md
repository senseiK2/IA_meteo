# Étude du compromis (station Chambery-Aix, test = année 2025)

Hyperparamètres : `{"learning_rate": 0.001, "batch_size": 256, "epochs_max": 150, "patience": 12, "dropout": 0.1, "poids_classes": "complets", "seed": 42}`

## A. Classes x taille

| schema     |   n_classes | taille   | couches      | entrees   |   n_entrees |   horizon_h | poids    |   parametres |   octets_int8 |   accuracy |   balanced_accuracy |   f1_macro |   precision_moyenne |   reference_majoritaire |   epoques |
|:-----------|------------:|:---------|:-------------|:----------|------------:|------------:|:---------|-------------:|--------------:|-----------:|--------------------:|-----------:|--------------------:|------------------------:|----------:|
| 3_classes  |           3 | XS       | [8]          | temps     |           8 |           0 | complets |           99 |            99 |     0.6056 |              0.7439 |     0.5888 |              0.5851 |                  0.7053 |        65 |
| 3_classes  |           3 | S        | [16, 16]     | temps     |           8 |           0 | complets |          467 |           467 |     0.6113 |              0.7447 |     0.5932 |              0.5874 |                  0.7053 |        23 |
| 3_classes  |           3 | M        | [32, 32]     | temps     |           8 |           0 | complets |         1443 |          1443 |     0.6241 |              0.7524 |     0.604  |              0.5933 |                  0.7053 |        20 |
| 3_classes  |           3 | L        | [64, 64, 32] | temps     |           8 |           0 | complets |         6915 |          6915 |     0.6311 |              0.7591 |     0.6102 |              0.5979 |                  0.7053 |        51 |
| 4_classes  |           4 | XS       | [8]          | temps     |           8 |           0 | complets |          108 |           108 |     0.5419 |              0.7079 |     0.5346 |              0.5225 |                  0.6353 |       103 |
| 4_classes  |           4 | S        | [16, 16]     | temps     |           8 |           0 | complets |          484 |           484 |     0.5438 |              0.717  |     0.5384 |              0.5294 |                  0.6353 |        90 |
| 4_classes  |           4 | M        | [32, 32]     | temps     |           8 |           0 | complets |         1476 |          1476 |     0.5538 |              0.7214 |     0.5443 |              0.5315 |                  0.6353 |        37 |
| 4_classes  |           4 | L        | [64, 64, 32] | temps     |           8 |           0 | complets |         6948 |          6948 |     0.5469 |              0.7202 |     0.5397 |              0.5305 |                  0.6353 |        23 |
| 6_classes  |           6 | XS       | [8]          | temps     |           8 |           0 | complets |          126 |           126 |     0.4128 |              0.6416 |     0.3183 |              0.3497 |                  0.6353 |       122 |
| 6_classes  |           6 | S        | [16, 16]     | temps     |           8 |           0 | complets |          518 |           518 |     0.4328 |              0.6353 |     0.3273 |              0.353  |                  0.6353 |       103 |
| 6_classes  |           6 | M        | [32, 32]     | temps     |           8 |           0 | complets |         1542 |          1542 |     0.4516 |              0.6608 |     0.3385 |              0.3569 |                  0.6353 |        44 |
| 6_classes  |           6 | L        | [64, 64, 32] | temps     |           8 |           0 | complets |         7014 |          7014 |     0.4457 |              0.6355 |     0.334  |              0.3553 |                  0.6353 |        26 |
| tp_complet |          13 | XS       | [8]          | temps     |           8 |           0 | complets |          189 |           189 |     0.2831 |              0.2757 |     0.1485 |              0.1813 |                  0.5305 |        52 |
| tp_complet |          13 | S        | [16, 16]     | temps     |           8 |           0 | complets |          637 |           637 |     0.2966 |              0.2876 |     0.1579 |              0.1806 |                  0.5305 |        62 |
| tp_complet |          13 | M        | [32, 32]     | temps     |           8 |           0 | complets |         1773 |          1773 |     0.3185 |              0.421  |     0.1699 |              0.1879 |                  0.5305 |        51 |
| tp_complet |          13 | L        | [64, 64, 32] | temps     |           8 |           0 | complets |         7245 |          7245 |     0.3166 |              0.3109 |     0.1647 |              0.1839 |                  0.5305 |        20 |

## B. Jeux d'entrées

| schema    |   n_classes | taille   | couches   | entrees   |   n_entrees |   horizon_h | poids    |   parametres |   octets_int8 |   accuracy |   balanced_accuracy |   f1_macro |   precision_moyenne |   reference_majoritaire |   epoques |
|:----------|------------:|:---------|:----------|:----------|------------:|------------:|:---------|-------------:|--------------:|-----------:|--------------------:|-----------:|--------------------:|------------------------:|----------:|
| 4_classes |           4 | S        | [16, 16]  | mesures   |           3 |           0 | complets |          404 |           404 |     0.4942 |              0.6628 |     0.4941 |              0.4847 |                  0.6353 |       108 |
| 4_classes |           4 | S        | [16, 16]  | derivees  |           4 |           0 | complets |          420 |           420 |     0.4951 |              0.6605 |     0.4921 |              0.4821 |                  0.6353 |        42 |
| 4_classes |           4 | S        | [16, 16]  | temps     |           8 |           0 | complets |          484 |           484 |     0.5438 |              0.717  |     0.5384 |              0.5294 |                  0.6353 |        90 |
| 4_classes |           4 | S        | [16, 16]  | tendance  |           9 |           0 | complets |          500 |           500 |     0.5527 |              0.7114 |     0.542  |              0.5292 |                  0.6353 |        21 |

## C. Horizon de prévision

| schema    |   n_classes | taille   | couches   | entrees   |   n_entrees |   horizon_h | poids    |   parametres |   octets_int8 |   accuracy |   balanced_accuracy |   f1_macro |   precision_moyenne |   reference_majoritaire |   epoques |
|:----------|------------:|:---------|:----------|:----------|------------:|------------:|:---------|-------------:|--------------:|-----------:|--------------------:|-----------:|--------------------:|------------------------:|----------:|
| 4_classes |           4 | S        | [16, 16]  | tendance  |           9 |           0 | complets |          500 |           500 |     0.5527 |              0.7114 |     0.542  |              0.5292 |                  0.6353 |        21 |
| 4_classes |           4 | S        | [16, 16]  | tendance  |           9 |           3 | complets |          500 |           500 |     0.5292 |              0.6866 |     0.5184 |              0.5144 |                  0.6355 |        26 |
| 4_classes |           4 | S        | [16, 16]  | tendance  |           9 |           6 | complets |          500 |           500 |     0.511  |              0.6684 |     0.5002 |              0.4981 |                  0.6357 |        46 |

## D. Poids des classes

| schema    |   n_classes | taille   | couches   | entrees   |   n_entrees |   horizon_h | poids    |   parametres |   octets_int8 |   accuracy |   balanced_accuracy |   f1_macro |   precision_moyenne |   reference_majoritaire |   epoques |
|:----------|------------:|:---------|:----------|:----------|------------:|------------:|:---------|-------------:|--------------:|-----------:|--------------------:|-----------:|--------------------:|------------------------:|----------:|
| 3_classes |           3 | S        | [16, 16]  | temps     |           8 |           0 | aucun    |          467 |           467 |     0.7599 |              0.7092 |     0.6829 |              0.6668 |                  0.7053 |        28 |
| 3_classes |           3 | S        | [16, 16]  | temps     |           8 |           0 | racine   |          467 |           467 |     0.6935 |              0.7439 |     0.6497 |              0.6176 |                  0.7053 |        28 |
| 3_classes |           3 | S        | [16, 16]  | temps     |           8 |           0 | complets |          467 |           467 |     0.6113 |              0.7447 |     0.5932 |              0.5874 |                  0.7053 |        23 |
| 4_classes |           4 | S        | [16, 16]  | temps     |           8 |           0 | aucun    |          484 |           484 |     0.687  |              0.6419 |     0.612  |              0.5962 |                  0.6353 |        43 |
| 4_classes |           4 | S        | [16, 16]  | temps     |           8 |           0 | racine   |          484 |           484 |     0.6275 |              0.7068 |     0.594  |              0.5569 |                  0.6353 |        40 |
| 4_classes |           4 | S        | [16, 16]  | temps     |           8 |           0 | complets |          484 |           484 |     0.5438 |              0.717  |     0.5384 |              0.5294 |                  0.6353 |        90 |

