//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * protectedConfig.h
 *
 * set of functions used to protect configuration file
 * data.  attempts to be as similar as possible to the
 * ProtectConfigFile.java counterpart; except this uses
 * a different encryption mechanism and is therefore
 * NOT BACKWARD COMPATIBLE with ProtectConfigFile.java.
 *
 * Assumes the caller will perform their own
 *
 * Author: jelderton -  8/30/16.
 *-----------------------------------------------*/

#ifndef FLEXCORE_PROTECTEDCONFIG_H
#define FLEXCORE_PROTECTEDCONFIG_H

#include <stdint.h>
#include <stdbool.h>

// container that holds data + lengh.  used for
// protecting, unprotecting, and passwords
//
typedef struct {
    unsigned char *data;    // input data
    uint32_t       length;  // size in bytes of 'data'
    uint_fast8_t  version;  // The encrypted value version detected by unprotectConfig
} pcData;

enum {
    /**
     * @var reserved for unrecognized or legacy inputs
     */
    PROTECT_ID_UNDEFINED = 0,
    /**
     * @var The original AES256 format with no version and no IV
     */
    PROTECT_AES_CBC_NO_IV,
    /**
     * @var AES256 with version and IV
     */
    PROTECT_AES_CBC,
    PROTECT_ID_LATEST = PROTECT_AES_CBC
};

/**
 * Start a protected configuration session.
 * This initializes random sources and prepares for
 * encrypt/decrypt operations. Call closeProtectConfigSession
 * when finished to release resources
 * \return false when session could not be opened
 */
bool openProtectConfigSession(void);

/**
 * Release resources for this session.
 * \note This function automatically zeroizes crypto contexts created in openProtectConfigSession.
 */
void closeProtectConfigSession(void);

/**
 * Force protected configuration data output version. This may be used to explicitly control when to change
 * encryption modes after new ones are introduced.
 * @param toVersion The maximum version to upgrade to. Anything less than or equal to current will be ignored unless
 * downgrade is also true.
 * @param downgrade set to true to force a downgrade.
 * @return true when the encryption version to be used is changed or a change is not required.
 */
bool forceProtectVersion(uint_fast8_t toVersion, bool downgrade);

/**
 * encrypt 'dataToProtect' using the supplied 'password'.
 * before returning, the data will be base64 encoded so
 * it can be safely stored in plain text.
 *
 * @param dataToProtect Container holding plaintext bytes and their length
 * @param password AES Key. This is not a passphrase, so derive the key randomly or with PKCS#5.
 * @param flags
 *
 * @see generateProtectPassword() to create passphrase
 *
 * caller is responsible for releasing the returned memory
 * and the elements within
 * @see destroyProtectConfigData()
 *
 * @note The length property of the returned value will be a strlen, unlike in unprotectConfigData, which returns the
 * exact byte count of the decrypted data.
 *
 * TODO: This uses AES_CBC for comptability with the original e2_* encrypted message schema in XML. The output format
 *       can be further extended to include an encryption ID, similar to crypt(3)
 */
pcData *protectConfigData(pcData *dataToProtect, pcData *password);

/**
 * decrypt data using the supplied passphrase.
 * assumes it was created via our corresponding
 * encrypt function as this will base64 decode
 * prior to decrypting.
 *
 * @param protectedData Base64 string containing ciphertext.
 * @param password key bytes (must be KEY_BITS long)
 * @note The protectedData->length is interpreted here as a strlen, unlike in protectConfigData, which accepts raw bytes
 * with an exact length.
 *
 * caller is responsible for releasing the returned memory
 * and the elements within
 * @see destroyProtectConfigData()
 */
pcData *unprotectConfigData(pcData *protectedData, pcData *password);

/**
 * generate a random key to use for encrypt/decrypt.
 * NOTE: should be obfuscated or encrypted by another mechanism
 *       before being stored in plain text.
 *
 * caller is responsible for releasing the returned memory
 * and the elements within
 * @see destroyProtectConfigData()
 */
pcData *generateProtectPassword(void);

/**
 * Generate random bytes with a requested length.
 * @return a byte array exactly length long.
 * @warning The returned value is not a C string.
 */
uint8_t * protectedConfigGenerateBytes(const size_t length);

/**
 * destroy the 'container', and if told to also the
 * internal 'data' (i.e. data->data)
 * @param data
 * @param includeData Clear and free contained data
 */
void destroyProtectConfigData(pcData *data, bool includeData);


#endif // FLEXCORE_PROTECTEDCONFIG_H

