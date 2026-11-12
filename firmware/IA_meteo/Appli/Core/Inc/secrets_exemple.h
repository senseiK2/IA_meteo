/**
  ******************************************************************************
  * @file    secrets_exemple.h
  * @brief   Modèle du fichier secrets.h, qui contient les clés privées.
  *
  * 1. Copier ce fichier en "secrets.h", dans le même dossier (Core/Inc).
  * 2. Y mettre les clés ThingSpeak (voir ci-dessous).
  *
  * secrets.h est listé dans .gitignore : il ne part jamais sur GitHub.
  * Une clé publiée permettrait à n'importe qui d'écrire dans le canal.
  ******************************************************************************
  */
#ifndef SECRETS_H
#define SECRETS_H

/* Canal : Write API Key (onglet API Keys du canal) */
#define THINGSPEAK_WRITE_KEY   "XXXXXXXXXXXXXXXX"

/* TalkBack : identifiant et API Key (Apps > TalkBack > la TalkBack créée) */
#define TALKBACK_ID            "00000"
#define TALKBACK_KEY           "XXXXXXXXXXXXXXXX"

#endif /* SECRETS_H */
