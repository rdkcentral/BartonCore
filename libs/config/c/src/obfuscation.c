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
 * obfuscation.c
 *
 * attempt to hide data into a larger buffer.  uses
 * masking and random generation to keep subsequent
 * obfuscations as dis-similar as possible.
 * designed internally, so please holler if you have
 * suggestions on making this better...
 *
 * NOTE: this is NOT encryption, so use at your own risk!
 *
 * Author: jelderton -  8/18/16.
 *-----------------------------------------------*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <icConfig/obfuscation.h>
#include <icTime/timeUtils.h>

// define a table of patterns
//
#define PATTERN_LEN   8
#define PATTERN_COUNT 36
static const uint32_t patterns[PATTERN_COUNT][PATTERN_LEN] = {
    {3, 0, 2, 6, 5, 0, 4, 1},
    {5, 6, 2, 0, 5, 1, 3, 2},
    {6, 3, 0, 5, 1, 3, 1, 2},
    {5, 2, 5, 0, 3, 1, 6, 2},
    {6, 0, 1, 3, 5, 6, 4, 3},
    {5, 6, 1, 1, 5, 5, 0, 5},
    {1, 0, 2, 0, 6, 4, 1, 4},
    {1, 6, 2, 4, 5, 0, 4, 2},
    {6, 3, 5, 2, 1, 4, 6, 0},
    {1, 0, 3, 5, 4, 5, 1, 3},
    {3, 1, 5, 6, 2, 5, 6, 5},
    {1, 6, 5, 4, 0, 6, 4, 0},
    {4, 5, 1, 4, 3, 2, 4, 6},
    {3, 0, 5, 3, 5, 3, 1, 0},
    {0, 6, 4, 5, 4, 6, 6, 5},
    {5, 2, 0, 3, 2, 1, 1, 0},
    {4, 0, 2, 0, 2, 4, 4, 3},
    {5, 1, 4, 3, 2, 3, 1, 0},
    {0, 4, 5, 4, 3, 2, 1, 2},
    {4, 0, 2, 4, 5, 4, 5, 1},
    {3, 2, 4, 5, 1, 4, 5, 1},
    {1, 4, 3, 4, 6, 4, 4, 2},
    {3, 0, 3, 2, 2, 1, 6, 4},
    {0, 6, 2, 3, 3, 4, 4, 0},
    {4, 3, 0, 1, 6, 2, 4, 2},
    {1, 0, 3, 3, 6, 2, 6, 4},
    {1, 5, 5, 2, 3, 0, 2, 0},
    {5, 6, 3, 6, 1, 4, 2, 3},
    {6, 0, 4, 3, 2, 1, 3, 3},
    {5, 0, 3, 6, 5, 3, 0, 3},
    {2, 3, 3, 1, 2, 5, 4, 1},
    {5, 2, 4, 0, 3, 1, 2, 1},
    {1, 5, 3, 0, 1, 2, 6, 3},
    {0, 1, 3, 6, 3, 3, 2, 4},
    {4, 5, 5, 0, 3, 1, 2, 6},
    {1, 0, 5, 2, 1, 0, 1, 0}
};

#define PI_STRING "3.1415926535897931159979634685441851615905761719"


// header stored at beginning of the buffer
//
typedef struct _dataHeader
{
    uint32_t pattern; // index into "patterns" used for encoding (need uint8, but make 32 for padding)
    uint32_t length;  // length of the original "input" string
    char mask;        // rand value used to 'mask' elements of this header
} dataHeader;

// private function declarations
//
static void initRandomness(int32_t seed);
static char *mask(const char *key, uint32_t keyLen, const char *input, uint32_t inputLen);
static char *createKey(const char *passphrase, uint32_t passLen, uint32_t *outputLen);
static void readHeader(const char *buffer, uint8_t *patternOut, uint32_t *inputLenOut);
static void writeHeader(char *buffer, uint8_t pattern, uint32_t inputLen);
static char *generateRandomBuff(uint32_t maxLength);
static char *fillByDuplication(const char *input, uint32_t inputLen, uint32_t outputLen);
static uint8_t choosePattern();
static uint32_t calculatePatternSpread(uint8_t row, uint32_t inputLen);
static void printHex(char *buff, int len);


/*
 * obfuscate some data into a buffer that can be saved or used
 * for other purposes (note: may want to base64 or uuencode the
 * result as the output will be binary).
 * the algorithm requires a 'passphrase' to serve as a key - which
 * is combined with the input data and then sprinkled across a
 * buffer of random bytes.
 *
 * caller must free the returned buffer, and take note of the in/out
 * parameter 'outputLen' - describing the length of the returned buffer.
 *
 * @param passphrase - well-known password to use while encoding 'input'
 * @param passLen - length of the password string
 * @param input - data to obfuscate
 * @param inputLen - length of the data to obfuscate
 * @param outputLen - length of the returned buffer.
 */
char *obfuscate(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen)
{
    // sanity check.  cannot look at the input strings
    // as those could start with a NULL character
    //
    if (passLen == 0 || inputLen == 0)
    {
        return NULL;
    }

    // init the random number generator, then chose a pattern
    //
    initRandomness(7);
    uint8_t row = choosePattern();
    const uint32_t *pattern = patterns[row];

    // create a key (munging the passphrase with PI).
    // this makes it a little more obscure and produces more of
    // a 'garbled' output (what we're going for here).
    //
    uint32_t keyLen = 0;
    char *key = createKey(passphrase, passLen, &keyLen);

    // obfuscate 'input' with the 'key' just created
    //
    char *munge = mask(key, keyLen, input, inputLen);

    // calculate the maximum size we will need when sprinkling
    // 'munge' via 'pattern'.  this tells us how many random chars
    // we need for the buffer.
    //
    uint32_t totalLen = calculatePatternSpread(row, inputLen);

    // make the buffer, but add additional bytes for our 'header'
    //
    uint32_t tmp = (totalLen + sizeof(dataHeader));
    char *buffer = generateRandomBuff(tmp);
    *outputLen = tmp;

    // apply our header to the buffer and skip over the
    // header size so we don't overwrite it
    //
    writeHeader(buffer, row, inputLen);
    uint32_t offset = (uint32_t) sizeof(dataHeader);

    // hide the obfuscated value (munge) by sprinkling each
    // byte into a random buffer, utilizing our pattern
    // so these bytes are not right next to each other.
    //
    int buffIdx = offset;
    int dataIdx, pattIdx = 0;
    for (dataIdx = 0; dataIdx < inputLen; dataIdx++)
    {
        // find number of chars to skip using pattern
        //
        uint32_t skipCount = pattern[pattIdx];
        pattIdx++;
        if (pattIdx >= PATTERN_LEN)
        {
            // back to beginning of pattern
            pattIdx = 0;
        }

        buffIdx += skipCount;
        buffer[buffIdx] = munge[dataIdx];
        buffIdx++;
    }

    // cleanup and return
    //
    free(munge);
    free(key);

    return buffer;
}

/*
 * given an obfuscated buffer, extract and return the original value.
 * requires the same 'passphrase' used when encoding the buffer.
 *
 * caller must free the returned buffer, and take note of the in/out
 * parameter - describing the length of the returned buffer.
 *
 * @param passphrase - well-known password used during obfuscation
 * @param passLen - length of the password string
 * @param input - data to unobfuscate
 * @param inputLen - length of the data to unobfuscate
 * @param outputLen - length of the returned buffer.
 */
char *unobfuscate(const char *passphrase, uint32_t passLen, const char *input, uint32_t inputLen, uint32_t *outputLen)
{
    // sanity check.  cannot look at the input strings
    // as those could start with a NULL character
    //
    if (passLen == 0 || inputLen == 0)
    {
        return NULL;
    }

    // read the header to see how large the encoded data was
    // and what pattern was used during the encoding phase
    //
    uint8_t row = 0;
    uint32_t length = 0;
    readHeader(input, &row, &length);
    if (length == 0 || row >= PATTERN_COUNT)
    {
        // unable to process - potentially bad header
        //
        return NULL;
    }

    // create a temp buffer to store the bytes as we
    // 'de-sprinkle' the obfuscated message from the noise
    // (i.e. remove random chars that are not needed)
    //
    char *temp = (char *) calloc(length + 1, sizeof(char));
    *outputLen = length;

    // extract desired chars from 'input' using the pattern,
    // and place them into our temp buffer
    //
    const uint32_t *pattern = patterns[row];
    int buffIdx = (int) sizeof(dataHeader);
    int tempIdx, pattIdx = 0;
    for (tempIdx = 0; tempIdx < length; tempIdx++)
    {
        // find number of chars to skip using pattern
        //
        uint32_t skipCount = pattern[pattIdx];
        pattIdx++;
        if (pattIdx >= PATTERN_LEN)
        {
            // back to beginning of pattern
            pattIdx = 0;
        }

        buffIdx += skipCount;
        temp[tempIdx] = input[buffIdx];
        buffIdx++;
    }

    // now that we have trimmed the cruft, we now have the
    // obvuscated value.  need to decode it using the same key
    // used during encoding
    //
    uint32_t keyLen = 0;
    char *key = createKey(passphrase, passLen, &keyLen);

    // decode 'temp' with 'key'
    //
    char *result = mask(key, keyLen, temp, length);

    // cleanup and return
    //
    free(temp);
    free(key);

    return result;
}

/*
 * initialize the random generator
 */
static void initRandomness(int32_t seed)
{
    // seed the random number generator
    // grab nanoseconds and apply to seed
    //
    struct timespec current;
    getCurrentTime(&current, true);
    if (seed != 0)
    {
        seed = (int32_t) (current.tv_nsec / seed);
    }
    srandom((unsigned int) time(NULL) + seed);
}

/*
 * bitwise magic of 'key' and 'input'.  returns a buffer with
 * the same size as 'inputLen' (assumes input is longer then key)
 */
static char *mask(const char *key, uint32_t keyLen, const char *input, uint32_t inputLen)
{
    // dup 'key' to make as-long-as 'input'
    //
    char *adjusted = fillByDuplication(key, keyLen, inputLen);

    // now that we have 2 character arrays of the same
    // length, loop through and bitwise XOR them together
    //
    char *buffer = (char *) calloc(inputLen + 1, sizeof(char));
    uint32_t x = 0;
    for (x = 0; x < inputLen; x++)
    {
        buffer[x] = (adjusted[x] ^ input[x]);
    }

    // cleanup
    //
    free(adjusted);
    return buffer;
}

/*
 * munge the 'passphrase' with PI to add more redirection to our logic
 */
static char *createKey(const char *passphrase, uint32_t passLen, uint32_t *outputLen)
{
    // represent PI as a string of 48 characters
    //
    uint32_t pieLen = (uint32_t) strlen(PI_STRING);

    // merge passphrase with pie, producing 'key'
    //
    if (passLen > pieLen)
    {
        // password is longer then 48, so use that as the long pole
        // (i.e. duplicate pie for the masking)
        //
        *outputLen = passLen;
        return mask(PI_STRING, pieLen, passphrase, passLen);
    }

    // password is less then 48, so use PI as the longer string
    // (i.e. duplicate passphrase for the masking)
    //
    *outputLen = pieLen;
    return mask(passphrase, passLen, PI_STRING, pieLen);
}

/*
 * extract the header from the buffer, and return the number
 * of bytes the header occupied as-well-as the length embedded
 * within the header.
 */
static void readHeader(const char *buffer, uint8_t *patternOut, uint32_t *inputLenOut)
{
    // overlay a header struct at the beginning of the buffer
    //
    dataHeader *head = (dataHeader *) buffer;

    // use the 'mask' to decode the other values
    //
    *patternOut = (uint8_t) (head->pattern ^ head->mask);
    *inputLenOut = (uint32_t) (head->length ^ head->mask);
}

/*
 * create and obscure a header, then copy over the beginning
 * of the supplied buffer.  note that this should occupy
 * sizeof(dataHeader) bytes (so skip over those)
 */
static void writeHeader(char *buffer, uint8_t pattern, uint32_t inputLen)
{
    // create a header struct and grab a random number to use for masking
    //
    dataHeader head;
    head.mask = (char) (random() % 127);

    // add the pattern & length, but mask against the rand value
    //
    head.pattern = (uint32_t) (pattern ^ head.mask);
    head.length = (uint32_t) (inputLen ^ head.mask);

    // now write the header at the beginning of buffer
    //
    memcpy(buffer, &head, sizeof(dataHeader));
}

/*
 * creates a buffer filled with random crud.
 * assumes initRandomness() has been called already.
 * caller must free the allocated token.
 */
static char *generateRandomBuff(uint32_t maxLength)
{
    if (maxLength == 0)
    {
        return NULL;
    }

    // allocate the buffer
    //
    char *buffer = (char *) malloc(sizeof(char) * (maxLength + 1));

    // Fill in the token using random values, keeping them
    // bounded between 0-127
    //
    int i;
    for (i = 0; i < maxLength; i++)
    {
        buffer[i] = (char) (random() % 127);
    }
    return buffer;
}

/*
 * generate a buffer of 'outputLen', then replicate 'input' as many
 * times as possible to fill the buffer.
 * (ex:  abc123 --> abc123abc123abc12)
 */
static char *fillByDuplication(const char *input, uint32_t inputLen, uint32_t outputLen)
{
    // get the length of the input string, and truncate
    // if already larger then the target length
    //
    if (inputLen > outputLen)
    {
        inputLen = outputLen;
    }

    // create the buffer to write into (leave room for NULL char)
    //
    char *buffer = (char *) calloc(outputLen + 1, sizeof(char));
    char *start = buffer;
    int offset = 0;

    // loop as much as necessary to duplicate 'input'
    // until we reach the desired length
    // (ex:  input=abc123 len=18 --> abc123abc123abc123)
    //
    while (offset < outputLen)
    {
        // see how much room we have
        //
        uint32_t remain = (outputLen - offset);
        if (remain > inputLen)
        {
            remain = inputLen;
        }

        // copy that many characters, starting at where
        // we left off the last copy
        //
        strncpy(start, input, remain);

        // update counters/pointers before looping again
        //
        offset += remain;
        start += remain;
    }

    return buffer;
}

/*
 * randomly pick a "pattern" to use.  will be bounded
 * to the PATTERN_COUNT to ensure the returned number
 * will be an index into the "patterns" array
 */
static uint8_t choosePattern()
{
    return (uint8_t) (random() % PATTERN_COUNT);
}

/*
 * given a pattern row, calculate the max length of the buffer
 * necessary to sprinkle 'inputLen' chars
 */
static uint32_t calculatePatternSpread(uint8_t row, uint32_t inputLen)
{
    // first get the pattern for this row, and total the number
    // of characters that will be skipped over.
    //
    const uint32_t *pattern = patterns[row];
    uint32_t perRow = PATTERN_LEN;
    int x = 0;
    for (x = 0; x < PATTERN_LEN; x++)
    {
        perRow += pattern[x];
    }

    // figure out how many times 'pattern' needs to be applied
    // to a buffer of 'inputLen' bytes
    //
    uint32_t sections = (inputLen / PATTERN_LEN) + 1;
    return (sections * perRow);
}

/*
 * debug print a buffer in hex
 */
static void printHex(char *buff, int len)
{
    int x = 0;
    while (x < len)
    {
        printf("%02X ", buff[x]);
        x++;
    }
    printf("\n");
}