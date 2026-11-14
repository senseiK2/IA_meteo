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
%      (classe et confiance en %, maintenant et dans 3 h) ;
%   5. enregistre la prédiction dans un 2e canal (TP3_Meteo_Cloud), pour la
%      page web de suivi (web/index.html) :
%        field1 / field2 : classe h0 (0..3) et confiance (%)
%        field3 / field4 : classe h3 (0..3) et confiance (%)
%        field5          : numéro de la mesure traitée (entry_id du canal 1)
%        field6          : durée d'exécution de ce script MATLAB (s)

% ---------------------------------------------------------------------------
% Réglages (page du canal : "Channel ID" ; onglet API Keys : "Read API Key" ;
% Apps > TalkBack : identifiant et API Key de la TalkBack).
% Ne pas publier les vraies clés sur GitHub.
% ---------------------------------------------------------------------------
channelID     = 0000000;             % <-- numéro du canal TP3_Meteo_STM32
readKey       = 'XXXXXXXXXXXXXXXX';  % <-- Read API Key du canal
talkbackID    = 00000;               % <-- identifiant de la TalkBack
talkbackKey   = 'XXXXXXXXXXXXXXXX';  % <-- API Key de la TalkBack
cloudChannel  = 0000000;             % <-- numéro du canal TP3_Meteo_Cloud
cloudWriteKey = 'XXXXXXXXXXXXXXXX';  % <-- Write API Key du canal TP3_Meteo_Cloud

debut = tic;                         % chronomètre : durée de l'exécution

url  = 'https://raw.githubusercontent.com/senseiK2/IA_meteo/main/ml/models/export/';
opts = weboptions('ContentType', 'json');   % GitHub sert le .json comme du texte

% ---------------------------------------------------------------------------
% 1. Dernière mesure de la carte
% ---------------------------------------------------------------------------
% API REST "feeds/last.json" plutôt que thingSpeakRead : elle donne aussi le
% numéro de la mesure (entry_id), qui relie la prédiction du cloud à la
% mesure et à la prédiction de la carte sur la page web.
derniere = webread(sprintf('https://api.thingspeak.com/channels/%d/feeds/last.json', ...
                           channelID), 'api_key', readKey, opts);
temp    = str2double(derniere.field1);
rh      = str2double(derniere.field2);
p_brute = str2double(derniere.field3);
fprintf('Mesure n°%d du %s : %.2f degC, %.1f %%, %.2f hPa\n', ...
        derniere.entry_id, derniere.created_at, temp, rh, p_brute);

% Heure UTC de la mesure (le modèle a appris avec les heures UTC de
% Meteostat ; ThingSpeak date chaque mesure en UTC, format ISO 8601).
maintenant = datetime(derniere.created_at, 'InputFormat', 'yyyy-MM-dd''T''HH:mm:ss''Z''', ...
                      'TimeZone', 'UTC');

% ---------------------------------------------------------------------------
% 2. et 3. Prédiction avec les deux modèles
% ---------------------------------------------------------------------------
horizons   = [0 3];
classes    = strings(size(horizons));      % classe prédite pour chaque horizon
indices    = zeros(size(horizons));        % son numéro (0..3, comme sur la carte)
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
    indices(i)    = k - 1;                 % MATLAB compte à partir de 1, le C de 0
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
% 5. Historique des prédictions du cloud (canal TP3_Meteo_Cloud)
% ---------------------------------------------------------------------------
duree = toc(debut);
try
    thingSpeakWrite(cloudChannel, [indices(1), confiances(1), indices(2), ...
                    confiances(2), derniere.entry_id, round(duree, 2)], ...
                    'WriteKey', cloudWriteKey);
    fprintf('Canal TP3_Meteo_Cloud : prediction enregistree (script : %.1f s)\n', duree);
catch err
    fprintf('Canal TP3_Meteo_Cloud : echec (%s)\n', err.message);
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
