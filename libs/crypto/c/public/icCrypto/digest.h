#ifndef ZILKER_DIGEST_H
#define ZILKER_DIGEST_H

#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <stdint.h>

typedef enum CryptoDigest
{
    CRYPTO_DIGEST_MD5,
    CRYPTO_DIGEST_SHA1,
    CRYPTO_DIGEST_SHA256,
    CRYPTO_DIGEST_SHA512
} CryptoDigest;

/**
 * Calculate a binary digest using a file.
 * @param filename
 * @param algorithm the digest to load
 * @param digestLength (output) the length of the digest, depending on algorithm. 0 on error.
 * @return A heap allocated digest value (binary). Read up to digestLength; NULL on error.
 * @sa stringBin2hex to convert to a hexadecimal string
 */
uint8_t *digestFile(const char *filename, CryptoDigest algorithm, uint8_t *digestLength);

/**
 * Convenience function to digest a file, encoding the digest in hex.
 * @param filename
 * @param algorithm the digest to use
 * @return hex-encoded digest (lowercase, no embellishments, like <algorithm>sum); NULL on error.
 */
inline char *digestFileHex(const char *filename, CryptoDigest algorithm)
{
    uint8_t len;
    scoped_generic uint8_t *digest = digestFile(filename, algorithm, &len);

    return stringBin2hex(digest, len);
}

#endif /* ZILKER_DIGEST_H */