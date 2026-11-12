/**
  ******************************************************************************
  * @file    secrets_exemple.h
  * @brief   Modèle du fichier secrets.h, qui contient les clés privées.
  *
  * 1. Copier ce fichier en "secrets.h", dans le même dossier (Core/Inc).
  * 2. Y mettre la Write API Key du canal ThingSpeak
  *    (ThingSpeak > Channels > My Channels > API Keys).
  *
  * secrets.h est listé dans .gitignore : il ne part jamais sur GitHub.
  * Une clé publiée permettrait à n'importe qui d'écrire dans le canal.
  ******************************************************************************
  */
#ifndef SECRETS_H
#define SECRETS_H

#define THINGSPEAK_WRITE_KEY   "XXXXXXXXXXXXXXXX"

#endif /* SECRETS_H */
