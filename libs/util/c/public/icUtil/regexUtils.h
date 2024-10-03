//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//

#ifndef ZILKER_REGEXUTILS_H
#define ZILKER_REGEXUTILS_H

#include "array.h"
#include <icTypes/icLinkedList.h>
#include <regex.h>
#include <stdbool.h>

#define REGEX_GLOBAL 1U

/**
 * Convenience macro to create a replacer with replacement flags (e.g., REGEX_GLOBAL)
 */
#define REGEX_REPLFLAGS_REPLACER(name, rflags, p, r...)                                                                \
    static const char *name##_REPLACEMENTS[] = {r};                                                                    \
                                                                                                                       \
    static RegexReplacer name = {.pattern = p,                                                                         \
                                 .replacements = name##_REPLACEMENTS,                                                  \
                                 .numReplacements = ARRAY_LENGTH(name##_REPLACEMENTS),                                 \
                                 .replaceFlags = rflags}

/**
 * Convenience macro to create a statically allocated replacer with no flags set.
 */
#define REGEX_SIMPLE_REPLACER(name, p, r...) REGEX_REPLFLAGS_REPLACER(name, 0, p, r)

typedef struct
{
    /**
     * The regular expression to match
     */
    const char *pattern;
    regex_t regex;
    bool ready;
    /**
     *
     * Any of
     *  REG_NOTBOL,
     *  REG_NOTEOL,
     *  REG_STARTEND
     *  These can be bitwise ORed
     * @see regex.h
     */
    int eflags;
    /**
     * Any of
     *   REG_EXTENDED,
     *   REG_ICASE,
     *   REG_NOSUB,
     *   REG_NEWLINE
     * These can be bitwise ORed
     * @see regex.h
     */
    int cflags;
    /**
     * Flags for this replacer.
     * Any of
     *  REGEX_GLOBAL
     * These can be bitwise ORed
     */
    int replaceFlags;
    /**
     * Number of replacements in the replacements array.
     */
    const int numReplacements;
    /**
     * List of replacement strings for each subexpression (expressions in parens).
     * @note Any index may be NULL to not perform a replacement.
     * @note Index 0 represents a replacement for a match against the whole pattern
     */
    const char **replacements;
} RegexReplacer;


/**
 * Initialize an array of replacers.
 * @param replacers A null-terminated array of replacers
 * @return
 */
bool regexInitReplacers(RegexReplacer **replacers);

/**
 * Destroy a list of replacers.
 * @param replacers A null-terminated array of replacers
 */
void regexDestroyReplacers(RegexReplacer **replacers);

/**
 * Initialize a single replacer
 * @param replacer
 * @return
 */
bool regexInitReplacer(RegexReplacer *replacer);

/**
 * Free replacer resources
 * @param replacer
 */
void regexDestroyReplacer(RegexReplacer *replacer);

/**
 * Substitute text matching a pattern.
 * @param text to replace occurence(s) of pattern in
 * @param replacers A null terminated array of replacers
 * @return A pointer to a heap allocated string
 * @note: A subexpression match (anything matching a pattern in parens) will store the offsets for the last match
 *        opportunity. This usually means the last occurrence of the pattern. Even with the replacer's global flag
 *        set, earlier occurrences of a subexpression match will not be replaced.
 * @see RegexReplacer for setting up replacements
 */
char *regexReplace(const char *text, RegexReplacer **replacers);

/**
 * Substitute text matching a patterns.
 * @param data to replace occurence(s) of pattern in
 * @param replacersList  list of a null terminated arrays of replacers
 * @return A pointer to a heap allocated string
 */
char *regexReplaceForListOfReplacers(const char *data, icLinkedList *replacersList);


#endif // ZILKER_REGEXUTILS_H
