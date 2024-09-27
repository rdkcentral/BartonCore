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

#ifndef ZILKER_SIMPLEPROTECTCONFIG_H
#define ZILKER_SIMPLEPROTECTCONFIG_H

#include <string.h>
#include <stdbool.h>

#define STORAGE_KEY_FILE_NAME       "store"

typedef struct ProtectSecret ProtectSecret;

/**
 * encrypt 'dataToProtect' and return a BASE64 encoded string that is
 * NULL terminated, which allows the caller to safely save to storage.
 *
 * requires a "namespace" for storage and retrieval of the generated
 * keys (which happen under the hood).
 *
 * caller is responsible for releasing the returned string
 */
char *simpleProtectConfigData(const char *namespace, const char *dataToProtect);

/**
 * decrypt 'protectedData' and return a NULL terminated string.
 *
 * requires a "namespace" for storage and retrieval of the generated
 * keys (which happen under the hood).
 *
 * caller is responsible for releasing the returned string.
 */
char *simpleUnprotectConfigData(const char *namespace, const char *protectedData);

 /**
  * Decrypt a string
  * @param secret
  * @param protectedData
  * @return The heap allocated plaintext string (free when done)
  * @note An incorrect secret will produce garbage. Encrypted data should contain check codes or validatable structure.
  */
char *simpleProtectDecrypt(const ProtectSecret *secret, const char *protectedData);

/**
 * Encrypt a string
 * @param secret
 * @param dataToProtect
 * @return A heap allocated encrypted string (free when done)
 */
char *simpleProtectEncrypt(const ProtectSecret *secret, const char *dataToProtect);

/**
 * Load or optionally create a symmetric secret key for encrypting/decrypting arbitrary data.
 * @param ns The storage namespace that will hold the key
 * @param autoCreate when true, attempt to create a key when none exists for namespace
 * @return The secret key (caller must release) or NULL on failure
 * @see simpleProtectDestroySecret to free key material
 */
ProtectSecret *simpleProtectGetSecret(const char *ns, bool autoCreate);

/**
 * Load a secret key from obfuscated data
 * @param keyData Key data to load
 * @param magic Key magic
 * @return
 */
ProtectSecret *simpleProtectSecretLoad(const char *keyData, const char *magic);

/**
 * Clear and free a secret key
 * @param secret
 */
void simpleProtectDestroySecret(ProtectSecret *secret);

/**
 * Scope-bound resource helper for ProtectSecret
 * @param secret
 * @see AUTO_CLEAN
 */
void simpleProtectDestroySecret__auto(ProtectSecret **secret);

#define scoped_ProtectSecret AUTO_CLEAN(simpleProtectDestroySecret__auto) ProtectSecret

/**
 * Load a symmetric secret key for encrypting/decrypting arbitrary data.
 * @param keyring Absolute path of the file which contains the secret key
 * @param name The key name to load
 * @param magic The magic for the keyring obfuscation
 * @return The secret key (caller must release) or NULL on failure
 * @see simpleProtectDestroySecret to free key material
 */
ProtectSecret *simpleProtectGetSecretFromKeyring(char *keyring, char *name, char *magic);

#endif // ZILKER_SIMPLEPROTECTCONFIG_H
