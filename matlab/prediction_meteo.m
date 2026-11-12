% prediction_meteo.m - TP3.2 : prédiction météo dans le cloud.
%
% À coller dans ThingSpeak > Apps > MATLAB Analysis.
%
%   1. lit la dernière mesure envoyée par la carte sur le canal
%      (field1 = température, field2 = humidité, field3 = pression brute) ;
%   2. calcule les 8 entrées du modèle, avec les MÊMES formules que
%      ml/meteo_features.py (sinon le modèle reçoit des entrées qu'il n'a
%      jamais vues) ;
%   3. normalise, fait tourner les modèles ONNX (temps actuel et +3 h) et
%      affiche le résultat ;
%   4. dépose la prédiction dans la file TalkBack, où la carte vient la
%      chercher, sous la forme "h0=Nuageux,64;h3=Precipitations,55"
%      (classe et confiance en %, maintenant et dans 3 h).

% ---------------------------------------------------------------------------
% Réglages (page du canal : "Channel ID" ; onglet API Keys : "Read API Key" ;
% Apps > TalkBack : identifiant et API Key de la TalkBack).
% Ne pas publier les vraies clés sur GitHub.
% ---------------------------------------------------------------------------
channelID   = 0000000;             % <-- numéro du canal TP3_Meteo_STM32
readKey     = 'XXXXXXXXXXXXXXXX';  % <-- Read API Key du canal
talkbackID  = 00000;               % <-- identifiant de la TalkBack
talkbackKey = 'XXXXXXXXXXXXXXXX';  % <-- API Key de la TalkBack

url  = 'https://raw.githubusercontent.com/senseiK2/IA_meteo/main/ml/models/export/';
opts = weboptions('ContentType', 'json');   % GitHub sert le .json comme du texte

% ---------------------------------------------------------------------------
% 1. Dernière mesure de la carte
% ---------------------------------------------------------------------------
mesure = thingSpeakRead(channelID, 'Fields', [1 2 3], 'NumPoints', 1, ...
                        'ReadKey', readKey, 'OutputFormat', 'table');
temp  = mesure{1, 2};              % colonne 1 = date, puis field1, 2, 3
rh    = mesure{1, 3};
p_brute = mesure{1, 4};
fprintf('Mesure du %s : %.2f degC, %.1f %%, %.2f hPa\n', ...
        string(mesure{1, 1}), temp, rh, p_brute);

% Heure UTC (le modèle a appris avec les heures UTC de Meteostat). La
% mesure a au plus 20 s : l'heure d'exécution suffit à cette précision.
maintenant = datetime('now', 'TimeZone', 'UTC');

% ---------------------------------------------------------------------------
% 2. et 3. Prédiction avec les deux modèles
% ---------------------------------------------------------------------------
horizons   = [0 3];
classes    = strings(size(horizons));      % classe prédite pour chaque horizon
confiances = zeros(size(horizons));        % probabilité de cette classe (%)
for i = 1:numel(horizons)
    horizon = horizons(i);
    nom  = sprintf('h%d', horizon);
    info = webread([url 'meteo_modele_' nom '.json'], opts);
    fichier = fullfile(tempdir, ['modele_meteo_' nom '.onnx']);
    websave(fichier, [url 'modele_meteo_' nom '.onnx']);
    net = importNetworkFromONNX(fichier, 'InputDataFormats', 'BC');

    x = calculer_entrees(temp, rh, p_brute, maintenant, info);
    if horizon == 0
        fprintf('Pression ramenee au niveau de la mer : %.1f hPa (altitude %d m)\n', ...
                x(3), info.altitude_carte_m);
    end

    x = (x - info.moyenne') ./ info.ecart_type';           % normalisation
    p = extractdata(predict(net, dlarray(single(x), 'BC')));
    [pmax, k] = max(p);
    classes(i)    = info.classes{k};
    confiances(i) = round(100 * pmax);

    if horizon == 0
        fprintf('\nMaintenant   : %s (%.0f %%)\n', info.classes{k}, 100 * pmax);
    else
        fprintf('Dans %d h     : %s (%.0f %%)\n', horizon, info.classes{k}, 100 * pmax);
    end
    for c = 1:numel(info.classes)
        fprintf('    %-15s %5.1f %%\n', info.classes{c}, 100 * p(c));
    end
end

% ---------------------------------------------------------------------------
% 4. Réponse à la carte par TalkBack : on ajoute une commande à la file.
%    La carte la lit (et la retire) avec .../commands/execute.
% ---------------------------------------------------------------------------
commande = sprintf('h0=%s,%d;h3=%s,%d', classes(1), confiances(1), ...
                   classes(2), confiances(2));
adresse  = sprintf('https://api.thingspeak.com/talkbacks/%d/commands.json', talkbackID);
try
    webwrite(adresse, 'api_key', talkbackKey, 'command_string', commande);
    fprintf('\nTalkBack : commande "%s" deposee\n', commande);
catch err
    fprintf('\nTalkBack : echec (%s)\n', err.message);
end

% ---------------------------------------------------------------------------
function x = calculer_entrees(temp, rh, p_brute, t, info)
% Calcule les entrées dans l'ordre attendu par le modèle (info.entrees).
% Formules identiques à ml/meteo_features.py.

    % Pression au niveau de la mer (formule hypsométrique) : Meteostat donne
    % la pression au niveau de la mer, la carte la mesure à son altitude.
    h = info.altitude_carte_m;
    pres = p_brute * (1 - 0.0065 * h / (temp + 0.0065 * h + 273.15))^(-5.257);

    % Écart au point de rosée (formule de Magnus)
    a = 17.62;  b = 243.12;
    g = log(min(max(rh, 1), 100) / 100) + a * temp / (b + temp);
    ecart_rosee = temp - b * g / (a - g);

    % Heure et jour de l'année sur un cercle (sinus, cosinus)
    heure = hour(t) + minute(t) / 60;
    jour  = day(t, 'dayofyear') - 1;

    valeurs = struct( ...
        'temp', temp, 'rhum', rh, 'pres', pres, 'ecart_rosee', ecart_rosee, ...
        'heure_sin', sin(2 * pi * heure / 24), 'heure_cos', cos(2 * pi * heure / 24), ...
        'jour_sin',  sin(2 * pi * jour / 365.25), 'jour_cos', cos(2 * pi * jour / 365.25));

    x = zeros(1, numel(info.entrees));
    for i = 1:numel(info.entrees)
        x(i) = valeurs.(info.entrees{i});
    end
end
