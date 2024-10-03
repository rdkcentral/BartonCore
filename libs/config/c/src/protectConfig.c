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
 * protectConfig.c
 *
 * set of functions used to protect configuration file
 * data.  attempts to be as similar as possible to the
 * ProtectConfigFile.java counterpart; except this uses
 * a different encryption mechanism and is therefore
 * NOT BACKWARD COMPATIBLE with ProtectConfigFile.java.
 *
 * Author: jelderton -  8/30/16.
 *-----------------------------------------------*/

#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <icConfig/protectedConfig.h>
#include <icConfig/simpleProtectConfig.h>
#include <icLog/logging.h>
#include <icUtil/base64.h>
#include <stdio.h>

// the algorithm to use.  needs to stay consistant, however
// could become an input parm to the public functions
//
#define MY_ALGO      GNUTLS_CIPHER_AES_256_CBC
#define LOG_TAG      "protect"
#define ERR_LEN      128
/**
 * @var Output separator character for marking encoded message parts.
 */
#define SEP          '$'
/**
 * @var Length for the version conversion buffer. The value is clamped to uint8_t max (3 digits) by extractParts.
 */
#define VER_LEN      4
/**
 * @var AES cipher block size in bytes
 */
#define AES_BLK      16
#define AES_KEY_BITS 256

enum
{
    MSG_PART_VERSION = 0,
    MSG_PART_IV,
    MSG_PART_DATA,
    MSG_PARTS
};

/**
 * @struct encParts
 * @brief Describes b64 encoded encrypted messages
 * @note The message format is "$version$IV$encrypted" where each part is base64 encoded.
 */
struct encParts
{
    uint_fast8_t version;
    unsigned char *iv;
    unsigned char *data;
};

// private vars, to ensure proper initialization
//
/**
 * @var CTR-DRBG random source user identifier to protect against insufficient
 *               startup entropy
 */
static const char *pers = "protected_config";

static pthread_mutex_t INIT_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static unsigned int counter = 0;
static uint_fast8_t use_ver = PROTECT_AES_CBC;
static bool ver_was_set = false;

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context drbg;

static bool extractParts(char *b64Data, struct encParts *parts);
static bool isReady(void);

/**
 *
 * @param msg A copy of the input message.
 * @param parts message parts will be filled in here.
 * @return true if the message is valid. Invalid mesages are normally legacy encrypted values.
 * @note This function will modify msg!
 */
static bool extractParts(char *msg, struct encParts *parts)
{
    char *strtok_state;
    const char sep[2] = {SEP, '\0'};
    int i = 0;
    char *token = NULL;
    unsigned long tmp = 0;
    bool valid = true;

    if (!parts || !msg)
    {
        return false;
    }

    while ((token = strtok_r(msg, sep, &strtok_state)) != NULL)
    {
        switch (i)
        {
            case MSG_PART_VERSION:
                errno = 0;
                tmp = strtoul(token, NULL, 10);
                valid = (tmp <= UINT8_MAX && !errno);
                if (valid)
                {
                    parts->version = (uint_fast8_t) tmp;
                }
                break;
            case MSG_PART_IV:
                parts->iv = token;
                break;
            case MSG_PART_DATA:
                parts->data = token;
                break;
            default:
                valid = false;
                break;
        }
        if (!valid)
        {
            break;
        }
        i++;
        msg = NULL;
    }

    return valid && i == MSG_PARTS;
}

static bool isReady(void)
{
    bool ready;

    pthread_mutex_lock(&INIT_MUTEX);
    ready = counter > 0;
    pthread_mutex_unlock(&INIT_MUTEX);

    return ready;
}

/**
 * Start a protected configuration session.
 * This initializes random sources and prepares for
 * encrypt/decrypt operations. Call closeProtectConfigSession
 * when finished to release resources
 * \return false when session could not be opened
 */
bool openProtectConfigSession(void)
{
    int rc = 0;
    char tmp[ERR_LEN];
    bool init = false;

    pthread_mutex_lock(&INIT_MUTEX);
    if (!counter)
    {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);

        rc = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, pers, strlen(pers));
        if (rc)
        {
            mbedtls_strerror(rc, tmp, ERR_LEN);
            icLogWarn(LOG_TAG, "error initializing TLS; rc=%d error=%s", rc, tmp);
        }
        else
        {
            icLogInfo(LOG_TAG, "Initialized with encryption version %d", use_ver);
            init = true;
        }
    }
    if (init || counter)
    {
        counter++;
        init = true;
    }
    pthread_mutex_unlock(&INIT_MUTEX);

    return init;
}

/**
 * Release resources for this session.
 * \note This function automatically zeroizes crypto contexts created in openProtectConfigSession.
 */
void closeProtectConfigSession(void)
{
    pthread_mutex_lock(&INIT_MUTEX);
    if (counter == 1)
    {
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&drbg);
        counter = 0;
    }
    else if (counter > 0)
    {
        counter--;
    }
    pthread_mutex_unlock(&INIT_MUTEX);
}

/**
 * Force protected configuration data output version. This may be used to explicitly control when to change
 * encryption modes after new ones are introduced.
 * @param toVersion The maximum version to upgrade to. Anything less than or equal to current will be ignored unless
 * downgrade is also true.
 * @param downgrade set to true to force a downgrade.
 * @return true when the encryption version to be used is changed or a change is not required.
 */
bool forceProtectVersion(uint_fast8_t toVersion, bool downgrade)
{
    bool forced = false;

    pthread_mutex_lock(&INIT_MUTEX);
    if (toVersion > PROTECT_ID_LATEST)
    {
        icLogWarn(LOG_TAG, "Ignoring invalid version %d", toVersion);
    }
    else if (toVersion < use_ver && !downgrade)
    {
        icLogInfo(LOG_TAG, "Ignoring implicit downgrade from %d to %d", use_ver, toVersion);
    }
    else
    {
        icLogInfo(LOG_TAG, "Forcing version change from ID %d to %d", use_ver, toVersion);
        use_ver = toVersion;
        ver_was_set = forced = true;
    }
    pthread_mutex_unlock(&INIT_MUTEX);

    return forced;
}

/**
 * encrypt 'dataToProtect' using the supplied 'password'.
 * before returning, the data will be base64 encoded so
 * it can be safely stored in plain text.
 *
 * caller is responsible for releasing the returned memory
 * and the elements within
 * @see destroyProtectConfigData()
 *
 * @note The length property of the returned value will be a strlen, unlike in unprotectConfigData, which returns the
 * exact byte count of the decrypted data.
 */
pcData *protectConfigData(pcData *dataToProtect, pcData *password)
{
    mbedtls_aes_context aes;

    int rc = 0;
    char tmp[ERR_LEN];
    unsigned char iv[AES_BLK];
    unsigned char enc_iv[AES_BLK];
    uint_fast8_t enc_ver;

    if (!isReady())
    {
        icLogError(LOG_TAG, "session is not open (encrypt)");
        return NULL;
    }

    if (!dataToProtect || !dataToProtect->data)
    {
        icLogError(LOG_TAG, "no plaintext");
        return NULL;
    }

    if (!password || !password->data)
    {
        icLogError(LOG_TAG, "no key");
        return NULL;
    }

    pthread_mutex_lock(&INIT_MUTEX);
    enc_ver = use_ver;
    pthread_mutex_unlock(&INIT_MUTEX);

    /*
     * Set up encryption and generate an initialization vector. If a previous read found legacy values anywhere, this
     * module will continue to write only legacy values.
     */
    mbedtls_aes_init(&aes);
    if (enc_ver > PROTECT_AES_CBC_NO_IV)
    {
        rc = mbedtls_ctr_drbg_random(&drbg, iv, AES_BLK);
        if (rc != 0)
        {
            mbedtls_strerror(rc, tmp, ERR_LEN);
            icLogError(LOG_TAG, "error generating AES IV; rc=%d error=%s", rc, tmp);
            mbedtls_aes_free(&aes);
            return NULL;
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Encrypting with a zero IV (legacy)");
        memset(iv, 0, AES_BLK);
    }

    // Create a clone of the IV for the encrypt function - it will be altered by the crypt algo
    memcpy(enc_iv, iv, AES_BLK);
    if (password->length != AES_KEY_BITS / 8)
    {
        mbedtls_aes_free(&aes);
        icLogError(LOG_TAG, "invalid key length");
        return NULL;
    }

    rc = mbedtls_aes_setkey_enc(&aes, password->data, AES_KEY_BITS);
    if (rc != 0)
    {
        mbedtls_strerror(rc, tmp, ERR_LEN);
        icLogWarn(LOG_TAG, "error setting AES encryption key; rc=%d error=%s", rc, tmp);
        mbedtls_aes_free(&aes);
        return NULL;
    }

    // create output buffer (assume same size as the input), but allow for
    // padding due to the block size.  note that many platforms also want
    // the input string to be of the same block size.
    // Adding 2^x-1 and discarding the bits below the xth bit rounds up to a whole block
    // FIXME: Provide PCKS#7 padding if storing binaries is desired. To work around this, encode your data to a C string
    //        compatible format (base64, etc).
    const uint8_t bsLow = AES_BLK - 1;
    uint32_t len = (dataToProtect->length + bsLow) & ~bsLow;

    // create buffer to encode into (happens to be our return object)
    //
    pcData *retVal = (pcData *) malloc(sizeof(pcData));
    retVal->length = len;
    retVal->data = (unsigned char *) calloc(len, sizeof(unsigned char));

    // make input the same length
    //
    void *inputBlock = calloc(len, sizeof(unsigned char));
    memcpy(inputBlock, dataToProtect->data, dataToProtect->length);

    // encrypt into 'buffer'
    //
    rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, len, enc_iv, inputBlock, retVal->data);
    if (rc != 0)
    {
        // failed to encrypt
        //
        mbedtls_strerror(rc, tmp, ERR_LEN);
        icLogWarn(LOG_TAG, "error encrypting via TLS; rc=%d error=%s", rc, tmp);
        destroyProtectConfigData(retVal, true);
        retVal = NULL;
    }
    else
    {
        // good encrypt, so base64 encode
        //
        char *encoded = icEncodeBase64(retVal->data, (uint16_t) retVal->length);
        char *iv_encoded = icEncodeBase64(iv, AES_BLK);
        if (encoded != NULL && iv_encoded != NULL)
        {
            const size_t iv_encoded_len = strlen(iv_encoded);
            const size_t encoded_len = strlen(encoded);
            free(retVal->data);

            /*
             * Write the ciphertext with IV as SEP<VER>SEP<b64IV><SEP><b64CT>. If the decrypter finds this (it should),
             * it will feed the IV back into the symmetric algorithm to enable decryption. NB: The decrypter will read
             * the version if it exists and act accordingly. Legacy values have a zero IV that is not written out.
             */
            if (enc_ver > PROTECT_AES_CBC_NO_IV)
            {
                /* Version part is SEP<ver>SEP, e.g., $1$ */
                char ver_part[VER_LEN];
                uint_fast8_t ver_len;

                snprintf(ver_part, sizeof(ver_part), "%d", enc_ver);
                ver_len = (uint_fast8_t) strlen(ver_part);

                /* Add four bytes for the piece separators (SEP) and NUL */
                retVal->length = ver_len + (uint32_t) encoded_len + (uint32_t) iv_encoded_len + 4;
                retVal->data = malloc(retVal->length);
                snprintf(retVal->data, retVal->length, "%c%s%c%s%c%s", SEP, ver_part, SEP, iv_encoded, SEP, encoded);
                retVal->length--;

                free(encoded);
            }
            else
            {
                /* Legacy code does not understand the multipart format; only give the encrypted value (IV was zeroized)
                 */
                retVal->length = (uint32_t) strlen(encoded);
                retVal->data = encoded;
            }

            free(iv_encoded);
        }
        else
        {
            icLogError(LOG_TAG, "error base64 encoding encrypted data");
            free(encoded);
            free(iv_encoded);

            destroyProtectConfigData(retVal, true);
            retVal = NULL;
        }
    }

    // cleanup and return
    //
    free(inputBlock);
    mbedtls_aes_free(&aes);
    return retVal;
}

/**
 * decrypt data using the supplied passphrase.
 * assumes it was created via our corresponding
 * encrypt function as this will base64 decode
 * prior to decrypting.
 *
 * @param protectedData Base64 string containing ciphertext.
 * @param password key bytes (must be KEY_BITS long)
 * @note The protectedData->length is interpreted here as a strlen, unlike protectConfigData, which accepts raw bytes
 * caller is responsible for releasing the returned memory
 * and the elements within
 * @see destroyProtectConfigData()
 */
pcData *unprotectConfigData(pcData *protectedData, pcData *password)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    pcData *retVal = NULL;
    int rc = 0;
    char tmp[ERR_LEN];
    unsigned char iv[AES_BLK];
    unsigned char *input = NULL;
    uint32_t inputLen = 0;
    uint8_t *decoded = NULL;
    uint16_t decodedLen = 0;
    struct encParts parts = {.data = "", .version = PROTECT_ID_UNDEFINED, .iv = ""};
    unsigned char *b64_ct = NULL;
    unsigned char *msg = NULL;
    /* Least significant bits of block size (e.g., 0000 1111b) for block alignment computation */
    const int bsLow = AES_BLK - 1;

    if (!isReady())
    {
        icLogError(LOG_TAG, "session is not open (decrypt)");
        return NULL;
    }

    if (!protectedData || !protectedData->data)
    {
        icLogError(LOG_TAG, "no ciphertext");
        return NULL;
    }

    if (!password || !password->data)
    {
        icLogError(LOG_TAG, "no key");
        return NULL;
    }

    if (protectedData->data[protectedData->length] != '\0')
    {
        icLogError(LOG_TAG, "ciphertext must be a null terminated base64 string");
        return NULL;
    }

    memset(iv, 0, AES_BLK);
    msg = strdup(protectedData->data);

    if (extractParts(msg, &parts))
    {
        if (icDecodeBase64(parts.iv, &decoded, &decodedLen) == true)
        {
            if (decodedLen == AES_BLK)
            {
                memcpy(iv, decoded, AES_BLK);
            }

            free(decoded);
            decoded = NULL;

            if (decodedLen != AES_BLK)
            {
                icLogError(LOG_TAG, "Decryption error: Provided IV has incorrect length");
                goto cleanup;
            }

            decodedLen = 0;
        }
        else
        {
            icLogError(LOG_TAG, "Decryption error: unable to decode IV");
            goto cleanup;
        }
        b64_ct = parts.data;
    }
    else
    {
        b64_ct = msg;
        pthread_mutex_lock(&INIT_MUTEX);
        if (!ver_was_set)
        {
            icLogWarn(
                LOG_TAG, "Found encrypted value with version: %d, future writes will not upgrade it.", parts.version);
            use_ver = PROTECT_AES_CBC_NO_IV;
            parts.version = use_ver;
        }
        else
        {
            icLogInfo(LOG_TAG,
                      "Found encrypted value with version: %d, future writes will upgrade it to %d",
                      parts.version,
                      use_ver);
        }
        pthread_mutex_unlock(&INIT_MUTEX);
    }
    if (icDecodeBase64(b64_ct, &decoded, &decodedLen) == true)
    {
        // decoded, so use 'decoded' as input to decrypt
        //
        input = decoded;
        inputLen = (uint32_t) decodedLen;
    }
    else
    {
        // failed to decode, cancel
        //
        icLogError(LOG_TAG, "Unable to decode base64 input");
        decoded = NULL;
        goto cleanup;
    }

    // prime the key input, then init the cipher
    //
    if (password->length != AES_KEY_BITS / 8)
    {
        icLogError(LOG_TAG, "invalid key length");
        goto cleanup;
    }
    rc = mbedtls_aes_setkey_dec(&aes, password->data, AES_KEY_BITS);
    if (rc != 0)
    {
        mbedtls_strerror(rc, tmp, ERR_LEN);
        icLogWarn(LOG_TAG, "error setting AES decryption key; rc=%d error=%s", rc, tmp);
        goto cleanup;
    }

    // create output buffer (assume same size as the input), but allow for
    // padding due to the block size.
    //
    inputLen = (inputLen + bsLow) & ~bsLow;

    // create output buffer (assume same length as input), however add
    // one extra character to the calloc to allow for the terminating NULL char
    //
    retVal = (pcData *) malloc(sizeof(pcData));
    retVal->length = inputLen;
    retVal->data = (unsigned char *) calloc(inputLen + 1, sizeof(unsigned char));
    retVal->version = parts.version;

    // decrypt, saving result into 'retVal'
    //
    rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, inputLen, iv, input, retVal->data);
    if (rc != 0)
    {
        mbedtls_strerror(rc, tmp, ERR_LEN);
        icLogWarn(LOG_TAG, "error decrypting AES; rc=%d error=%s", rc, tmp);
        destroyProtectConfigData(retVal, true);
        retVal = NULL;
        goto cleanup;
    }

    // because of padding, the retVal->data buffer can have trailing NULL characters.
    // adjust the retVal->length accordingly
    //
    while (retVal->length > 0 && retVal->data[retVal->length - 1] == '\0')
    {
        retVal->length--;
    }

// cleanup and return
//
cleanup:
    if (decoded != NULL)
    {
        free(decoded);
    }
    free(msg);
    mbedtls_aes_free(&aes);
    return retVal;
}

/*
 * generate a random key to use for encrypt/decrypt.
 * NOTE: should be obfuscated or encrypted by another mechanism
 *       before being stored in plain text.
 *
 * caller is responsible for releasing the returned memory
 * and the elements within
 * @see destroyProtectConfigData()
 */
pcData *generateProtectPassword(void)
{
    const size_t len = AES_KEY_BITS / 8;
    uint8_t *key = protectedConfigGenerateBytes(len);

    if (key == NULL)
    {
        return NULL;
    }

    // good to go, so place in a pcData container
    //
    pcData *retVal = (pcData *) malloc(sizeof(pcData));
    retVal->data = key;
    retVal->length = (uint32_t) len;

    return retVal;
}

uint8_t *protectedConfigGenerateBytes(const size_t length)
{
    if (!isReady())
    {
        icLogError(LOG_TAG, "session is not open (keygen)");
        return NULL;
    }

    // allocate a key that's the correct size
    //
    char *key = malloc(length);

    // ask ctr-drbg to generate a random string
    //
    int rc = 0;
    if ((rc = mbedtls_ctr_drbg_random(&drbg, key, length)) != 0)
    {
        char tmp[ERR_LEN];
        mbedtls_strerror(rc, tmp, ERR_LEN);
        icLogWarn(LOG_TAG, "error creating random key via TLS; rc=%d error=%s\n", rc, tmp);
        free(key);
        return NULL;
    }

    return key;
}

void destroyProtectConfigData(pcData *data, bool includeData)
{
    if (data != NULL)
    {
        if (includeData == true && data->data != NULL)
        {
            memset(data->data, 0, data->length);
            free(data->data);
        }
        free(data);
    }
}
