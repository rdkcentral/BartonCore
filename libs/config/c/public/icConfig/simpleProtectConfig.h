//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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

#pragma once

#include <stdbool.h>
#include <string.h>

#define STORAGE_KEY_FILE_NAME "store"

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
