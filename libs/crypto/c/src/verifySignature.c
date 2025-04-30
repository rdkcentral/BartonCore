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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// NOTE: currently verifySignature is called by command-line utilities, so output goes to stdout/stderr instead
//       of our typical logging.

/*
 * read a file,  returns the raw contents, as-well-as fill in the fileLen
 * caller must free the returned buffer
 */
static void *readFileContents(const char *fileName, uint32_t *fileLen)
{
    // get the length of the file
    //
    struct stat info;
    if (stat(fileName, &info) != 0 || info.st_size < 1)
    {
        // error opening file
        fprintf(stderr, "error opening file %s; it does not seem to exist\n", fileName);
        *fileLen = 0;
        return NULL;
    }

    // save the file length
    //
    *fileLen = (uint32_t) info.st_size;

    // open the file
    //
    int fd = open(fileName, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        fprintf(stderr, "error opening file %s; %s\n", fileName, strerror(errno));
        return NULL;
    }

    // allocate enough memory to read the entire file
    //
    void *retVal = malloc(*fileLen);
    uint32_t offset = 0;
    ssize_t c = 0;
    while (offset < *fileLen)
    {
        // read a chunk (up to total len)
        //
        c = read(fd, (retVal + offset), (*fileLen - offset));
        if (c < 0)
        {
            // error reading
            //
            fprintf(stderr, "error reading file %s; %s\n", fileName, strerror(errno));
            close(fd);
            free(retVal);
            return NULL;
        }

        // loop around to get the next chunk
        //
        offset += c;
    }

    // cleanup and return
    //
    close(fd);
    return retVal;
}

/**
 * Load the contents up in openSSL so it can be worked with
 * @param publicKeyContents the contents of the key
 * @param publicKeyContentsLen the contents length
 * @return the key or NULL if its not valid
 */
static EVP_PKEY *loadPublicKey(void *publicKeyContents, uint32_t publicKeyContentsLen)
{
    BIO *bufio = NULL;
    EVP_PKEY *publicKeyEvp = NULL;

    // extract the RSA from the keyfile
    //
    if (!(bufio = BIO_new_mem_buf(publicKeyContents, publicKeyContentsLen)))
    {
        fprintf(stderr, "BIO not created.\n");
        return NULL;
    }
    publicKeyEvp = PEM_read_bio_PUBKEY(bufio, NULL, NULL, NULL);

    // Cleanup
    BIO_free(bufio);

    return publicKeyEvp;
}

/*
 * perform validation
 */
static bool RSAVerifySignature(EVP_PKEY *pubKey, void *digest, uint32_t digestLen, void *data, uint32_t dataLen)
{
    bool retVal = false;

    // create the context
    //
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();

    // init the context for a 'sha256' digest
    //
    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pubKey) <= 0)
    {
        EVP_MD_CTX_destroy(ctx);
        return false;
    }

    // load the data
    //
    if (EVP_DigestVerifyUpdate(ctx, data, dataLen) <= 0)
    {
        EVP_MD_CTX_destroy(ctx);
        return false;
    }

    // validate against the digest
    //
    int authStatus = EVP_DigestVerifyFinal(ctx, digest, digestLen);
    if (authStatus == 1)
    {
        // able to authenticate
        //
        retVal = true;
    }

    EVP_MD_CTX_destroy(ctx);
    return retVal;
}

/*
 * validate the signature of a file.  used during upgrade situations to
 * ensure the packaged file was un-touched and good-to-go for use
 *
 * @param keyFilename - the public key to use for the validation
 * @param baseFilename - the filename the signature will be validated against
 * @param signatureFilename - the .sig file (to accompany the baseFilename)
 */
bool verifySignature(char *keyFilename, char *baseFilename, char *signatureFilename)
{
    bool retVal = false;
    void *publicKey = NULL; // contents of keyFilename
    void *digest = NULL;    // contents of signatureFilename
    void *data = NULL;      // contents of baseFilename
    EVP_PKEY *publicKeyEvp = NULL;
    uint32_t dataLen = 0;
    uint32_t digestLen = 0;

    // load the contents of 'keyfilename' and extract the RSA
    //
    uint32_t publicKeyLen = 0;
    if ((publicKey = readFileContents(keyFilename, &publicKeyLen)) == NULL)
    {
        fprintf(stderr, "unable to read public key file %s\n", keyFilename);
        goto verify_cleanup;
    }

    // extract the RSA from the keyfile
    //
    if ((publicKeyEvp = loadPublicKey(publicKey, publicKeyLen)) == NULL)
    {
        fprintf(stderr, "unable to read public key %s\n", keyFilename);
        goto verify_cleanup;
    }

    // read the signature file & data file
    //
    if ((data = readFileContents(baseFilename, &dataLen)) == NULL)
    {
        fprintf(stderr, "unable to read data file %s\n", baseFilename);
        goto verify_cleanup;
    }
    if ((digest = readFileContents(signatureFilename, &digestLen)) == NULL)
    {
        fprintf(stderr, "unable to read signature file %s\n", signatureFilename);
        goto verify_cleanup;
    }

    // now that we have everything, perform the validation
    //
    retVal = RSAVerifySignature(publicKeyEvp, digest, digestLen, data, dataLen);

// cleanup
//
verify_cleanup:
    free(publicKey);
    free(digest);
    free(data);
    if (publicKeyEvp != NULL)
    {
        EVP_PKEY_free(publicKeyEvp);
    }

    return retVal;
}
