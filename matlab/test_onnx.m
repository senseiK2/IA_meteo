% test_onnx.m - TP3.2 : vérifie que MATLAB (ThingSpeak > Apps > MATLAB
% Analysis) sait charger le modèle ONNX exporté par ml/04_exporter.py et
% retrouve les mêmes probabilités que Python.
%
% Le modèle et ses réglages sont lus directement sur le dépôt GitHub
% (public) : rien à téléverser sur ThingSpeak.

url = 'https://raw.githubusercontent.com/senseiK2/IA_meteo/main/ml/models/export/';

% 1. Téléchargement du modèle et de ses réglages (entrées, normalisation,
%    classes). GitHub sert le .json comme du texte : on force le décodage JSON.
fichier = fullfile(tempdir, 'modele_meteo_h0.onnx');
websave(fichier, [url 'modele_meteo_h0.onnx']);
opts = weboptions('ContentType', 'json');
info = webread([url 'meteo_modele_h0.json'], opts);
disp('Classes :'); disp(info.classes');

% 2. Chargement du modèle ONNX. 'BC' indique le sens des dimensions de
%    l'entrée : B (batch) = nombre d'exemples, C (channel) = les 8 entrées.
%    Sans cette précision, le réseau reste "non initialisé".
net = importNetworkFromONNX(fichier, 'InputDataFormats', 'BC');
disp('Modele charge');

% 3. Un exemple connu : 1re ligne de ml/models/export/vecteurs_test_h0.csv
%    temp, humidite, pression, ecart_rosee, heure sin/cos, jour sin/cos
x = [3.3 90 1022.3 1.4845 0.707107 0.707107 0.154204 0.988039];
x = (x - info.moyenne') ./ info.ecart_type';      % normalisation (comme en Python)
p = extractdata(predict(net, dlarray(single(x), 'BC')));
disp('Probabilites :'); disp(p');
% Attendu (Python) : 0.0001  0.3056  0.4328  0.2615  -> Brouillard
