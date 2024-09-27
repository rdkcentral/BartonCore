/*-----------------------------------------------
 * base64.h
 *
 * Helper functions for performing Base64
 * encode/decode operations.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IC_BASE64_H
#define IC_BASE64_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Encode a binary array into a Base-64 string.  If the return
 * is not-NULL, then the caller should free the memory.
 *
 * @return encoded string otherwise NULL
 */
extern char *icEncodeBase64(uint8_t *array, uint16_t arrayLen);

/**
 * Decode the base64 encoded string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
extern bool icDecodeBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen);

/**
 * Decode the base64 encoded url string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
extern bool icDecodeUrlBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen);

#endif
