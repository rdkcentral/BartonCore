//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Cable
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// Comcast Cable retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * simpleProtectConfig.h
 *
 * simplified variation of functions within the protectConfig.h
 * headerfile.  that mechanism assumes the caller will store the
 * random key somewhere in the system.  although flexible, it allows
 * for non-standard procedures for safeguarding and saving the
 * generated keys.
 *
 * Author: jelderton -  6/7/19.
 *-----------------------------------------------*/

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <icConfig/obfuscation.h>
#include <icConfig/protectedConfig.h>
#include <icConfig/simpleProtectConfig.h>
#include <icConfig/storage.h>
#include <icUtil/base64.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>

#define DEFAULT_KEY_IDENTIFIER "default"
#define OBFUSCATE_KEY          "config" // simple yet not out of place.  need to make this better

struct ProtectSecret
{
    pcData *key;
};

// private functions
static ProtectSecret *readNamespaceKey(const char *namespace, const char *identifier);
static bool writeNamespaceKey(const char *namespace, const char *identifier, pcData *key);
static char *loadKeyFromKeyring(char *keyring, const char *identifier);

/**
 * encrypt 'dataToProtect' and return a BASE64 encoded string that is
 * NULL terminated, which allows the caller to safely save to storage.
 *
 * requires a "namespace" for storage and retrieval of the generated
 * keys (which happen under the hood).
 *
 * caller is responsible for releasing the returned string
 */
char *simpleProtectConfigData(const char *namespace, const char *dataToProtect)
{
    char *retVal = NULL;
    if (namespace == NULL || dataToProtect == NULL || openProtectConfigSession() == false)
    {
        return NULL;
    }

    ProtectSecret *encrKey = simpleProtectGetSecret(namespace, true);

    retVal = simpleProtectEncrypt(encrKey, dataToProtect);

    simpleProtectDestroySecret(encrKey);

    return retVal;
}

/**
 * decrypt 'protectedData' and return a NULL terminated string.
 *
 * requires a "namespace" for storage and retrieval of the generated
 * keys (which happen under the hood).
 *
 * caller is responsible for releasing the returned string.
 */
char *simpleUnprotectConfigData(const char *namespace, const char *protectedData)
{
    char *retVal = NULL;
    if (namespace == NULL || protectedData == NULL)
    {
        return NULL;
    }

    // see if we have an encryption key already associated with this namespace
    //
    ProtectSecret *encrKey = simpleProtectGetSecret(namespace, false);

    retVal = simpleProtectDecrypt(encrKey, protectedData);

    simpleProtectDestroySecret(encrKey);

    return retVal;
}

char *simpleProtectDecrypt(const ProtectSecret *secret, const char *protectedData)
{
    if (secret == NULL || protectedData == NULL || openProtectConfigSession() == false)
    {
        return NULL;
    }

    // use the encryption key to decode the protected data
    //
    char *retVal = NULL;
    pcData input;
    input.data = (unsigned char *) protectedData;
    input.length = (uint32_t) strlen(protectedData);
    pcData *data = unprotectConfigData(&input, secret->key);
    if (data != NULL && data->data[data->length] == 0)
    {
        // move from 'data' to 'retVal'
        retVal = (char *) data->data;
        data->data = NULL;
    }

    // cleanup and return
    //
    destroyProtectConfigData(data, true);
    closeProtectConfigSession();

    return retVal;
}

char *simpleProtectEncrypt(const ProtectSecret *secret, const char *dataToProtect)
{
    if (secret == NULL || dataToProtect == NULL || openProtectConfigSession() == false)
    {
        return NULL;
    }

    // use the encryption key to protect the data
    //
    char *retVal = NULL;
    pcData in;
    in.data = (unsigned char *) dataToProtect;
    in.length = (uint32_t) strlen(dataToProtect);
    pcData *encr = protectConfigData(&in, secret->key);

    if (encr != NULL)
    {
        // move from 'encr' to 'retVal'
        retVal = (char *) encr->data;
        encr->data = NULL;
    }

    // cleanup and return
    //
    destroyProtectConfigData(encr, false);
    closeProtectConfigSession();

    return retVal;
}

ProtectSecret *simpleProtectSecretLoad(const char *keyData, const char *magic)
{
    ProtectSecret *secret = NULL;

    if (keyData != NULL && keyData[0] != '\0')
    {
        // base64 decode first
        //
        uint8_t *decoded = NULL;
        uint16_t decodeLen = 0;
        if (icDecodeBase64(keyData, &decoded, &decodeLen) == true)
        {
            // successfully decoded, so un-obfuscate.  for simplicity, we use
            // a hard-coded obfuscation seed (requested by developers).
            //
            uint32_t keyLen = 0;

            if (magic == NULL)
            {
                magic = OBFUSCATE_KEY;
            }

            char *key = unobfuscate(magic, (uint32_t) strlen(magic), (const char *) decoded, decodeLen, &keyLen);
            if (key != NULL)
            {
                secret = malloc(sizeof(ProtectSecret));
                // save as our return object
                //
                secret->key = (pcData *) malloc(sizeof(pcData));
                secret->key->data = (unsigned char *) key;
                secret->key->length = keyLen;
            }
            free(decoded);
        }
    }

    return secret;
}

ProtectSecret *simpleProtectGetSecret(const char *ns, bool autoCreate)
{
    // see if we have an encryption key already associated with this namespace
    //
    ProtectSecret *secret = NULL;
    if (openProtectConfigSession() == false)
    {
        return NULL;
    }

    secret = readNamespaceKey(ns, DEFAULT_KEY_IDENTIFIER);
    if (secret == NULL && autoCreate == true)
    {
        // create one, then persist it
        //
        pcData *encrKey = generateProtectPassword();
        if (encrKey != NULL)
        {
            writeNamespaceKey(ns, DEFAULT_KEY_IDENTIFIER, encrKey);
        }

        secret = calloc(1, sizeof(ProtectSecret));
        secret->key = encrKey;
    }

    closeProtectConfigSession();

    return secret;
}

void simpleProtectDestroySecret(ProtectSecret *secret)
{
    if (secret != NULL)
    {
        destroyProtectConfigData(secret->key, true);
    }
    free(secret);
}

void simpleProtectDestroySecret__auto(ProtectSecret **secret)
{
    simpleProtectDestroySecret(*secret);
}

/*
 * extract the 'key' from this namespace
 */
static ProtectSecret *readNamespaceKey(const char *namespace, const char *identifier)
{
    // first extract the obfuscated 'identifier' from the 'key file' in this 'namespace'
    //
    char *value = NULL;
    scoped_generic char *encoded = NULL;
    if (storageLoad(namespace, STORAGE_KEY_FILE_NAME, &value) == true)
    {
        // file is in JSON format, so parse it and extract the 'identifier'
        //
        encoded = loadKeyFromKeyring(value, identifier);
    }
    free(value);

    return simpleProtectSecretLoad(encoded, NULL);
}

/*
 * store 'key' in the namespace
 * NOTE: at this time we are only saving a single key, even
 *       though we allow the caller to name is via 'identifier'.
 *       when we want to store more then 1 key, the file will need
 *       to be read, massaged, then saved.  currently this will just
 *       overwrite what is there.
 */
static bool writeNamespaceKey(const char *namespace, const char *identifier, pcData *key)
{
    bool retVal = false;
    char *encodedKey = NULL;
    if (key != NULL)
    {
        // obfuscate our key.  for simplicity, we use a hard-coded obfuscation seed
        //
        uint32_t obLen = 0;
        char *ob1 =
            obfuscate(OBFUSCATE_KEY, (uint32_t) strlen(OBFUSCATE_KEY), (const char *) key->data, key->length, &obLen);
        if (ob1 != NULL)
        {
            // base64 encode the key so we can save it in JSON
            //
            encodedKey = icEncodeBase64((uint8_t *) ob1, (uint16_t) obLen);
            free(ob1);
        }
    }

    if (encodedKey != NULL)
    {
        // create the JSON
        //
        cJSON *body = cJSON_CreateObject();
        cJSON_AddStringToObject(body, identifier, encodedKey);

        // convert to string
        //
        char *contents = cJSON_Print(body);
        cJSON_Delete(body);
        body = NULL;

        // save to the namespace file
        //
        retVal = storageSave(namespace, STORAGE_KEY_FILE_NAME, contents);
        free(contents);
        free(encodedKey);
        contents = NULL;
    }

    return retVal;
}

/**
 * Load a symmetric secret key for encrypting/decrypting arbitrary data.
 * @param keyring Absolute path of the file which contains the secret key
 * @param name The key name to load
 * @param magic The magic for the keyring obfuscation
 * @return The secret key (caller must release) or NULL on failure
 * @see simpleProtectDestroySecret to free key material
 */
ProtectSecret *simpleProtectGetSecretFromKeyring(char *keyring, char *name, char *magic)
{
    // see if we have an encryption key already associated with this filepath
    //
    ProtectSecret *secret = NULL;
    scoped_generic char *encoded = NULL;

    // load the secret key from json file
    scoped_generic char *data = readFileContents(keyring);

    if (data != NULL)
    {
        // parse the key information from json format
        encoded = loadKeyFromKeyring(data, name);
    }

    // de-obfuscate the key so it can be used for decryption.
    secret = simpleProtectSecretLoad(encoded, magic);

    return secret;
}

/*
 * Load the key from keyring which is specified in identifier
 * @param keyring
 * @param identifier
 * @return encoded
 */
static char *loadKeyFromKeyring(char *keyring, const char *identifier)
{
    char *encodedKey = NULL;

    if (keyring != NULL)
    {
        cJSON *body = cJSON_Parse(keyring);
        if (body != NULL)
        {
            cJSON *item = NULL;
            if (((item = cJSON_GetObjectItem(body, identifier)) != NULL) && (item->valuestring != NULL))
            {
                encodedKey = strdup(item->valuestring);
            }
            cJSON_Delete(body);
        }
    }

    return encodedKey;
}