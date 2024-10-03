//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
 * stringUtils.c
 *
 * Utilities for strings
 *
 * Author: jgleason - 9/23/15 (moved tlea's stringReplace function here)
 *-----------------------------------------------*/

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <icLog/logging.h>
#include <icTypes/icStringBuffer.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>

#define LOG_TAG "STRINGUTILS"

/*
 * take an original string a replace a substring within it
 * returns a new string with the substring replacement
 *
 * Inputs:
 *   orig - the original string
 *   rep  - the substring being replaced
 *   with - the new substring to replace rep in orig
 * Returns:
 *   new string with substring replaced
 */
char *stringReplace(const char *orig, const char *rep, const char *with)
{
    // sanity check
    //
    if (stringIsEmpty(orig) == true)
    {
        if (orig == NULL)
        {
            return NULL;
        }
        return strdup(orig);
    }
    if (stringIsEmpty(rep) == true)
    {
        // nothing to search for, return original string
        return strdup(orig);
    }

    // create a string buffer to append strings into
    //
    icStringBuffer *buffer = stringBufferCreate(strlen(orig));
    char *ptr = NULL;
    char *start = (char *) orig;
    unsigned long repLen = strlen(rep);

    // search for instances of 'rep' in the 'orig' string
    //
    while ((ptr = strstr(start, rep)) != NULL)
    {
        // append from 'start' to 'ptr'
        stringBufferAppendLength(buffer, start, (uint16_t) (ptr - start));

        // append the replacement
        stringBufferAppend(buffer, with);

        // move to the end of the search string, so we can look for the next one
        start = ptr + repLen;
    }

    // get the rest
    //
    stringBufferAppend(buffer, start);

    // create the return string
    //
    char *retVal = stringBufferToString(buffer);
    stringBufferDestroy(buffer);
    return retVal;
}

/*
 * Algorithm:
 * 1. Measure string dimensions and validate offset and removeCount
 * 2. Decrease buffer size by removeCount and increase it by newText length (if not NULL)
 * 3. Reduce removeCount by the number of characters excluded by removeCount
 * 4. If the string must grow, reallocate and move characters beyond offset to their new position after newText
 * 5. If the string must shrink, shift characters after the deleted region to the left and reallocate
 * 6. Copy newText, if it exists, into its position at offset (space is allocated above or this is a direct replacement)
 * 7. Null terminate the string
 */
char *stringEdit(char *str, const size_t offset, const size_t removeCount, const char *newText)
{
    size_t origSize = 0;
    size_t newSize = 0;
    size_t replacementSize = 0;
    size_t removeCharsFromNew = removeCount;

    if (str == NULL)
    {
        return NULL;
    }

    origSize = strlen(str);
    newSize = origSize;

    if (offset > origSize)
    {
        icLogError(LOG_TAG, "offset is beyond end of input string");
        free(str);
        return NULL;
    }

    if (removeCount <= origSize)
    {
        newSize -= removeCount;
    }
    else
    {
        icLogError(LOG_TAG, "removeCount %zu longer than input string length %zu", removeCount, newSize);
        free(str);
        return NULL;
    }

    if (offset + removeCount > origSize)
    {
        size_t overage = offset + removeCount - origSize;
        icLogError(LOG_TAG, "removeCount %zu would remove %zu character(s) beyond string end", removeCount, overage);
        free(str);
        return NULL;
    }

    if (newText != NULL)
    {
        replacementSize = strlen(newText);
        newSize += replacementSize;
    }

    /* Characters to remove that are beyond the planned new string end were already discarded by the size change */
    if (offset + removeCount > newSize)
    {
        removeCharsFromNew -= origSize - newSize;
    }

    if (newSize > origSize)
    {
        size_t shift = newSize - origSize + removeCharsFromNew;
        size_t displacedPartSize = origSize - offset - removeCharsFromNew;
        char *displacedPart = NULL;

        str = realloc(str, newSize + 1);
        displacedPart = str + offset + removeCharsFromNew;
        memmove(str + offset + shift, displacedPart, displacedPartSize);
    }
    else if (newSize < origSize)
    {
        size_t shift = 0;
        size_t shrinkBy = origSize - newSize;
        size_t displacedPartSize = 0;
        char *displacedPart = NULL;

        /*
         * When there are more characters to remove than to insert, shift the displaced part to the end of the
         * inserted string. Otherwise we are doing a drop-in replacement or the string grew.
         */
        if (replacementSize < removeCharsFromNew)
        {
            shift = replacementSize;
        }

        /*
         * The displaced part is at the end of the region that will be deleted. removeCharsFromNew always represents how
         * many characters will be erased/replaced within the planned string bounds.
         *
         * When the planned bounds include the entire removeCharsFromNew count, it is added to offset to get the
         * displaced part. The net size change may be less than this, as characters will be overwritten by newText if
         * present.
         *
         * When the planned bounds exclude part or all of the discard region, the net size change is exactly the number
         * of characters excluded from the planned string. The removeCharsFromNew was already reduced by this amount.
         */
        if (shrinkBy < removeCharsFromNew)
        {
            shrinkBy = removeCharsFromNew;
        }

        displacedPartSize = origSize - offset - shrinkBy;
        displacedPart = str + offset + shrinkBy;

        memmove(str + offset + shift, displacedPart, displacedPartSize);
        str = realloc(str, newSize + 1);
    }

    if (newText != NULL)
    {
        memcpy(str + offset, newText, replacementSize);
    }

    str[newSize] = 0;
    return str;
}

/*
 * creates a random alpha-numeric token
 *
 * Inputs:
 *   minLength  - the minimum length (in chars) of the token
 *   maxLength  - the maximum length (in chars) of the token
 *   seedAdder  - this number is added to the current time when generating the seed
 *                   (useful if calling multiple times in a row) *
 * Returns:
 *   random alpha-numeric string or NULL if error occurs
 *
 * Caller MUST free the allocated token
 */
char *generateRandomToken(uint16_t minLength, uint16_t maxLength, int seedAdder)
{
    char tokenBits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char *token;
    uint16_t len;

    /* Check for valid parameters */
    if (minLength > maxLength)
    {
        icLogError(LOG_TAG, "Incorrect parameter values. minLength > maxLength");
        return NULL;
    }

    /* Generate a random token length using 'time' plus the 'seedAdder' */
    srandom((unsigned int) time(NULL) + seedAdder);
    len = (uint16_t) (random() % (maxLength - minLength + 1) + minLength);

    /* Allocate and zero memory for our token */
    token = (char *) calloc(len + 1, sizeof(char));

    /* Check to see if we were successful */
    if (token == NULL)
    {
        icLogError(LOG_TAG, "Could not allocate memory for token");
        return NULL;
    }

    /* Fill in the token using random selections from tokenBits[] */
    srandom((unsigned int) time(NULL) + seedAdder + 2);
    for (int i = 0; i < len; i++)
    {
        token[i] = tokenBits[random() % strlen(tokenBits)];
    }
    return token;
}

/*
 * safely copy a source string into a destination string, preventing
 * overrunning memory of 'dest'. 'dest' will be null terminated on success.
 * uses the strlen of 'src' and the supplied 'maxDestChars' to copy
 * as many characters as it can without overflowing the buffer.
 *
 * returns false if something went wrong and not able to do the copy.
 */
bool safeStringCopy(char *dest, uint16_t maxDestChars, const char *src)
{
    if (dest == NULL || src == NULL || maxDestChars == 0)
    {
        return false;
    }

    // determine how many characters we can copy
    //
    size_t srcLen = strlen(src);
    size_t max = srcLen;
    if (max >= maxDestChars)
    {
        // source is longer then destination
        //
        max = (size_t) (maxDestChars - 1);
    }

    // do the copy and NULL terminate the string
    //
    strncpy(dest, src, max);
    dest[max] = '\0';

    return true;
}

/*
 * safely appends a source string to the end of a destination string,
 * preventing overrunning memory of 'dest'. 'dest' will be null terminated on success.
 * uses the strlen of 'src' and the supplied 'maxDestChars' to copy
 * as many characters as it can without overflowing the buffer.
 *
 * returns false if something went wrong and not able to do the append.
 */
bool safeStringAppend(char *dest, uint16_t maxDestChars, const char *src)
{
    if (dest == NULL || src == NULL || maxDestChars == 0)
    {
        return false;
    }

    // get length of both strings to see what is possible
    //
    size_t curLen = strlen(dest);
    size_t srcLen = strlen(src);

    size_t charCount = srcLen;
    if ((curLen + srcLen) >= maxDestChars)
    {
        // cannot fit all of 'src', so need to truncate
        //
        charCount = maxDestChars - (curLen + 1);
    }

    // do the copy and NULL terminate the string
    //
    strncat(dest, src, charCount);

    // null terminate at currLen + charCount
    //
    dest[curLen + charCount] = '\0';

    return true;
}

/*
 * performs a comparison against the two strings.  can be used
 * for equality or sorting.
 *
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal (or both NULL)
 * positive-num - if 'right' is greater.
 */
int8_t stringCompare(const char *left, const char *right, bool ignoreCase)
{
    if (left == NULL && right == NULL)
    {
        // both null, return 0;
        return 0;
    }
    else if (left != NULL && right != NULL)
    {
        // do the comparison
        //
        int rc = 0;
        if (ignoreCase == true)
        {
            rc = strcasecmp(left, right);
        }
        else
        {
            rc = strcmp(left, right);
        }

        // seems dumb to do vs return 'rc', but Coverity complained about this...
        if (rc == 0)
        {
            return 0;
        }
        else if (rc < 0)
        {
            return -1;
        }
        return 1;
    }
    else if (left == NULL)
    {
        // left is NULL
        //
        return 1;
    }
    else
    {
        // right is NULL
        //
        return -1;
    }
}

/*
 * return if the 'string' starts with the same 'prefix'
 */
bool stringStartsWith(const char *string, const char *prefix, bool ignoreCase)
{
    // sanity check
    //
    if (string == NULL || prefix == NULL)
    {
        return false;
    }

    // get length of both strings
    //
    size_t prefLen = strlen(prefix);
    size_t stringLen = strlen(string);
    if (prefLen > stringLen)
    {
        return false;
    }

    if (ignoreCase == true)
    {
        if (strncasecmp(string, prefix, prefLen) == 0)
        {
            return true;
        }
    }
    else
    {
        if (strncmp(string, prefix, prefLen) == 0)
        {
            return true;
        }
    }

    return false;
}

/*
 * return if the 'string' ends with the same 'suffix'
 */
bool stringEndsWith(const char *string, const char *suffix, bool ignoreCase)
{
    // sanity check
    //
    if (string == NULL || suffix == NULL)
    {
        return false;
    }

    // get length of both strings
    //
    size_t suffLen = strlen(suffix);
    size_t len = strlen(string);
    if (suffLen > len)
    {
        return false;
    }

    if (ignoreCase == true)
    {
        if (strcasecmp(string + (len - suffLen), suffix) == 0)
        {
            return true;
        }
    }
    else
    {
        if (strcmp(string + (len - suffLen), suffix) == 0)
        {
            return true;
        }
    }

    return false;
}

/*
 * return a new string with the leading/trailing whitespace removed.
 * caller must release returned memory via free().
 */
char *trimString(const char *src)
{
    if (src == NULL || strlen(src) == 0)
    {
        return strdup("");
    }

    // make return string at least the same size as 'src'
    //
    char *retVal = (char *) malloc(sizeof(char) * strlen(src) + 1);
    int offset = 0;
    char *ptr = (char *) src;
    bool gotChar = false;

    // loop until our ptr points hits 'end of string'
    //
    while (ptr != NULL && *ptr != '\0')
    {
        // see if we've copied any chars yet
        // if not, then check for whitespace
        //
        if (gotChar == false && isspace(*ptr))
        {
            // whitspace out front, skip
            //
            ptr++;
            continue;
        }

        // not space, so copy over
        //
        gotChar = true;
        retVal[offset] = *ptr;
        ptr++;
        offset++;
    }

    if (offset >= 0)
    {
        // null terminate the string
        //
        retVal[offset] = '\0';
    }

    // walk backward replacing any whitespace chars with NULL chars
    // to remove the trailing spaces
    //
    while (offset > 0 && isspace(retVal[offset - 1]))
    {
        retVal[offset - 1] = '\0';
        offset--;
    }

    return retVal;
}

/*
 * string function to copy chars from 'src' into 'dest', until
 * we hit NULL or the 'stop' location.  will skip over leading
 * whitespace and attempt to trim the trailing whitespace.
 */
static int copyAndTrimBuffer(char *dest, const char *src, char *stop)
{
    int offset = 0;
    const char *ptr = src;
    bool gotChar = false;

    // loop until our ptr points to 'stop'
    // or hit 'end of string'
    //
    while (ptr != stop && ptr != NULL && *ptr != '\0')
    {
        // see if we've copied any chars yet
        // if not, then check for whitespace
        //
        if (gotChar == false && isspace(*ptr))
        {
            ptr++;
            continue;
        }

        // not space, so copy over
        //
        gotChar = true;
        dest[offset] = *ptr;
        ptr++;
        offset++;
    }

    if (offset >= 0)
    {
        // null terminate the string
        //
        dest[offset] = '\0';
    }

    // walk backward replacing any whitespace chars with NULL chars
    // to remove the trailing spaces
    //
    while (offset > 0 && isspace(dest[offset - 1]))
    {
        dest[offset - 1] = '\0';
        offset--;
    }

    return offset;
}

/*
 * break a string in two using a token character.
 * ex:  abc.123 --> abc 123
 *
 * the 'leftOut' and 'rightOut' should exist and be
 * large enough to hold the characters.
 *
 * returns false if not able to perform the operation
 */
bool stringSplitOnToken(const char *inputStr, const char token, char *leftOut, char *rightOut)
{
    // find the token within 'inputStr'
    //
    if (inputStr == NULL || token == '\0')
    {
        return false;
    }
    char *mid = strchr(inputStr, token);
    if (mid != NULL)
    {
        // copy from beginning to 'mid' into 'leftOut'
        //
        if (copyAndTrimBuffer(leftOut, inputStr, mid) < 0)
        {
            return false;
        }
        if (copyAndTrimBuffer(rightOut, mid + 1, NULL) < 0)
        {
            return false;
        }

        return true;
    }
    return false;
}

/*
 * convert the string to all lower case characters.
 */
void stringToLowerCase(char *inputStr)
{
    if (inputStr == NULL)
    {
        return;
    }

    size_t len = strlen(inputStr);
    int loop = 0;
    while (loop < len)
    {
        inputStr[loop] = (char) tolower(inputStr[loop]);
        loop++;
    }
}

/*
 * convert the string to all upper case characters.
 */
void stringToUpperCase(char *inputStr)
{
    if (inputStr == NULL)
    {
        return;
    }

    size_t len = strlen(inputStr);
    int loop = 0;
    while (loop < len)
    {
        inputStr[loop] = (char) toupper(inputStr[loop]);
        loop++;
    }
}

/*
 * convert the string to camel case and returns the new string
 * Uses chars: '_', '-', ' '
 * To separate the desired words
 * NOTE: caller must free return string
 */
char *stringToCamelCase(const char *inputStr)
{
    if (inputStr == NULL)
    {
        return NULL;
    }

    size_t len = strlen(inputStr);

    if (len == 0)
    {
        return NULL;
    }

    char *retVal = calloc(len + 1, 1);
    int dest = 0, loop = 0;
    bool nextUpper = false;

    while (loop < len)
    {
        // if current is one of these char, uppercase the next char
        //
        if ((inputStr[loop] == '_') || (inputStr[loop] == '-') || (inputStr[loop] == ' '))
        {
            nextUpper = true;
            loop++;
            continue;
        }

        if (nextUpper)
        {
            retVal[dest] = (char) toupper(inputStr[loop]);
            nextUpper = false;
        }
        else
        {
            retVal[dest] = (char) tolower(inputStr[loop]);
        }

        dest++;
        loop++;
    }

    return retVal;
}

/*
 * helper function to safely allocate and create a string
 * using var_args (think of this as a sprintf() that creates mem)
 *
 * NOTE: caller must free the returned string
 */
char *stringBuilder(const char *format, ...)
{
    va_list arglist;

    // preprocess the format & args
    //
    va_start(arglist, format);

    char *retVal = NULL;

    // vasprintf gives truncated stack frames in address sanitizer when we leak the return
    // so for angelsenvy just use our vsnprintf implementation
#if defined _GNU_SOURCE && !defined CONFIG_PRODUCT_ANGELSENVY
    vasprintf(&retVal, format, arglist);
#else
    // send to snprintf once to get the length needed
    //
    int strLen = vsnprintf(NULL, 0, format, arglist);
    if (strLen > 0)
    {
        // reset the var args
        va_end(arglist);
        va_start(arglist, format);

        // create the string, then populate it
        //
        retVal = (char *) malloc(sizeof(char) * (strLen + 1));
        vsnprintf(retVal, (size_t) strLen + 1, format, arglist);
    }
#endif // defined _GNU_SOURCE && !defined CONFIG_PRODUCT_ANGELSENVY

    // cleanup and return
    //
    va_end(arglist);
    return retVal;
}

char *strerrorSafe(int errnum)
{
    char buf[1024] = {0};
#if !defined(__GLIBC__) || ((_POSIX_C_SOURCE >= 200112L) && !defined(_GNU_SOURCE))
    strerror_r(errnum, buf, ARRAY_LENGTH(buf));
    return strdup(buf);
#else
    return strdup(strerror_r(errnum, buf, ARRAY_LENGTH(buf)));
#endif
}

/**
 * Convert a string to a uint8_t using standard base detection conventions
 * @param str string to convert
 * @param num uint8_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint8(const char *str, uint8_t *num)
{
    uint64_t result;
    bool retVal = stringToUnsignedNumberWithinRange(str, &result, 0, 0, UINT8_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a hexadecimal string to a uint8_t using standard base detection conventions
 * @param str string to convert
 * @param num uint8_t to write into
 * @return true if success, false otherwise
 */
bool hexStringToUint8(const char *str, uint8_t *num)
{
    uint64_t result;
    bool retVal = stringToUnsignedNumberWithinRange(str, &result, 16, 0, UINT8_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a hexadecimal string to a uint16_t using standard base detection conventions
 * @param str string to convert
 * @param num uint16_t to write into
 * @return true if success, false otherwise
 */
bool hexStringToUint16(const char *str, uint16_t *num)
{
    uint64_t result;
    bool retVal = stringToUnsignedNumberWithinRange(str, &result, 16, 0, UINT16_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a string to a uint16_t using standard base detection conventions
 * @param str string to convert
 * @param num uint16_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint16(const char *str, uint16_t *num)
{
    uint64_t result;
    bool retVal = stringToUnsignedNumberWithinRange(str, &result, 0, 0, UINT16_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a string to a uint32_t using standard base detection conventions
 * @param str string to convert
 * @param num uint32_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint32(const char *str, uint32_t *num)
{
    uint64_t result;
    bool retVal = stringToUnsignedNumberWithinRange(str, &result, 0, 0, UINT32_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a string to a uint64_t using standard base detection conventions
 * @param str string to convert
 * @param num uint64_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint64(const char *str, uint64_t *num)
{
    return stringToUnsignedNumberWithinRange(str, num, 0, 0, UINT64_MAX);
}

/**
 * Convert a string to a number in the given range
 * @param str string to convert
 * @param num uint64_t to write into
 * @param base base of the number in the string
 * @param minValue minimum value in range(inclusive)
 * @param maxValue maximum value in range(inclusive)
 * @return true if success and is within range, false otherwise
 */
bool stringToUnsignedNumberWithinRange(const char *str,
                                       uint64_t *num,
                                       uint8_t base,
                                       uint64_t minValue,
                                       uint64_t maxValue)
{
    bool retVal = true;
    if (str == NULL)
    {
        retVal = false;
    }
    else
    {
        // Handle special case, the way strtoull handles negatives is it first parses the number then negates it.  So -1
        // gets turned into UINT64_MAX.
        if (strchr(str, '-') != NULL)
        {
            retVal = false;
        }
        else
        {
            char *bad = NULL;
            errno = 0;
            unsigned long long int result = strtoull(str, &bad, base);

            if (errno || *bad || result < minValue || result > maxValue)
            {
                retVal = false;
            }
            else
            {
                *num = result;
            }
        }
    }

    return retVal;
}

/**
 * Convert a string to a int8_t using standard base detection conventions
 * @param str string to convert
 * @param num int8_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt8(const char *str, int8_t *num)
{
    int64_t result;
    bool retVal = stringToNumberWithinRange(str, &result, 0, INT8_MIN, INT8_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a string to a int16_t using standard base detection conventions
 * @param str string to convert
 * @param num int16_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt16(const char *str, int16_t *num)
{
    int64_t result;
    bool retVal = stringToNumberWithinRange(str, &result, 0, INT16_MIN, INT16_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a string to a int32_t using standard base detection conventions
 * @param str string to convert
 * @param num int32_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt32(const char *str, int32_t *num)
{
    int64_t result;
    bool retVal = stringToNumberWithinRange(str, &result, 0, INT32_MIN, INT32_MAX);
    if (retVal == true && num != NULL)
    {
        *num = result;
    }
    return retVal;
}

/**
 * Convert a string to a int64_t using standard base detection conventions
 * @param str string to convert
 * @param num int64_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt64(const char *str, int64_t *num)
{
    return stringToNumberWithinRange(str, num, 0, INT64_MIN, INT64_MAX);
}

/**
 * Convert a string to a number in the given range
 * @param str string to convert
 * @param num uint64_t to write into
 * @param base base of the number in the string
 * @param minValue minimum value in range(inclusive)
 * @param maxValue maximum value in range(inclusive)
 * @return true if success and is within range, false otherwise
 */
bool stringToNumberWithinRange(const char *str, int64_t *num, uint8_t base, int64_t minValue, int64_t maxValue)
{
    bool retVal = true;
    if (str == NULL)
    {
        retVal = false;
    }
    else
    {
        char *bad = NULL;
        errno = 0;
        long long int result = strtoll(str, &bad, base);

        if (errno || *bad || result < minValue || result > maxValue)
        {
            retVal = false;
        }
        else if (num != NULL)
        {
            *num = result;
        }
    }

    return retVal;
}

/**
 * Convert a string to a double
 *
 * @param str string to convert
 * @param num double to write to
 * @return true if success, false otherwise
 */
bool stringToDouble(const char *str, double *num)
{
    bool retVal = false;

    if (str != NULL && num != NULL)
    {
        char *end = NULL;
        errno = 0;
        double value = strtod(str, &end);
        if (end != str && errno == 0)
        {
            retVal = true;
            *num = value;
        }
    }

    return retVal;
}

/**
 * convert a string to a bool; returning whether the string could be converted or not
 * @param str the string to convert
 * @param result the result of the conversion
 * @return true if the string could be converted, false otherwise
 */
bool stringToBoolStrict(const char *str, bool *result)
{
    bool success = false;
    if (str != NULL)
    {
        if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0 || strcmp(str, "1") == 0)
        {
            *result = true;
            success = true;
        }
        else if (strcasecmp(str, "false") == 0 || strcasecmp(str, "no") == 0 || strcmp(str, "0") == 0)
        {
            *result = false;
            success = true;
        }
    }
    return success;
}

/**
 * Convert a string to a boolean
 * @param str string to convert
 * @return true if string is true, yes, or 1. False if otherwise
 */
bool stringToBool(const char *str)
{
    bool retVal = false;

    // coverity[check_return] if unparseable, retVal will be false. Warnings will only clutter logs.
    stringToBoolStrict(str, &retVal);

    return retVal;
}

/*
 * return the string version of the boolean.  handy for logging
 */
const char *stringValueOfBool(bool flag)
{
    if (flag == true)
    {
        return "true";
    }
    return "false";
}

extern inline const char *stringCoalesce(const char *str);
extern inline const char *stringCoalesceAlt(const char *str, const char *alt);

/**
 * Write a bitmap (up to 64 bits) as a string, indicating set bit numbers (1-indexed).
 * @param bitmap A bitmap (8-64 bits)
 * @param mapSize the map size, in bytes
 * @return the string which the caller must free
 */
char *bitmapToStr(const uint64_t bitmap, const size_t mapSize)
{
    char *slots = NULL;
    if (mapSize > sizeof(uint64_t))
    {
        return NULL;
    }

    const uint8_t mapBits = mapSize * 8;

    // 1-9 take 2 characters(separator plus digit), 10-64 take 3 characters, so max
    // buffer size is 2*9 + 55*3 + 1(final separator) + 1(null terminator) = 185
    char buf[185];
    memset(buf, 0, sizeof(buf));

    // Keep track of where we are at in the buffer
    char *currentPos = buf;
    for (uint8_t i = 0; i < mapBits; i++)
    {
        if ((bitmap >> i) & 1U)
        {
            sprintf(currentPos, "|%u", i + 1);
            if (i >= 9)
            {
                // added |xx
                currentPos += 3;
            }
            else
            {
                // added |x
                currentPos += 2;
            }
        }
    }

    if (currentPos == buf)
    {
        slots = strdup("(none)");
    }
    else
    {
        *currentPos = '|';
        slots = strdup(buf);
    }

    return slots;
}

/**
 * Returns true if the given string is NULL or if its first character is a null-terminating character.
 *
 * @param str - The string the check
 * @return
 */
extern inline bool stringIsEmpty(const char *str);

char *stringBin2hex(const unsigned char *data, size_t length)
{
    static const char *hexits = "0123456789abcdef";

    if (data == NULL)
    {
        return NULL;
    }

    size_t hexLen = length * 2;
    char *hex = malloc(hexLen + 1);

    for (size_t i = 0; i < length; i++)
    {
        size_t j = i * 2;
        hex[j] = hexits[(data[i] >> 4) & 0xf];
        hex[j + 1] = hexits[data[i] & 0xf];
    }

    hex[hexLen] = '\0';

    return hex;
}

extern inline char *strdupOpt(const char *in);

bool stringIsPrintable(const char *src, bool allowSpaces)
{
    if (stringIsEmpty(src) == true)
    {
        return false;
    }

    bool result = true;
    const char *ptr = src;

    while ((ptr != NULL) && (*ptr != '\0'))
    {
        if (isgraph(*ptr) == 0 && (allowSpaces == false || isblank(*ptr) == 0))
        {
            result = false;
            break;
        }
        ptr++;
    }

    return result;
}

/**
 * string function to left or right pad the string with supplied character.
 * @param src - source string
 * @param padLen - number of bytes to pad
 * @param padChar - padding character
 * @param isLeftPad - left padding or right padding
 * @return - returns left or right padded string as per supplied param.
 * NOTE: caller must release returned string via free().
 */
char *stringPad(const char *src, uint16_t padLen, char padChar, bool isLeftPad)
{
    if (src == NULL)
    {
        return NULL;
    }

    char *padStr;
    uint16_t srcLen = strlen(src);
    uint16_t padStrLen = srcLen + padLen + 1;

    padStr = (char *) malloc(sizeof(char) * padStrLen);
    if (padStr == NULL)
    {
        return NULL;
    }

    if (isLeftPad == true)
    {
        memset(padStr, padChar, padLen);
        memcpy(padStr + padLen, src, srcLen);
    }
    else
    {
        memcpy(padStr, src, srcLen);
        memset(padStr + srcLen, padChar, padLen);
    }
    padStr[padStrLen - 1] = '\0';

    return padStr;
}
