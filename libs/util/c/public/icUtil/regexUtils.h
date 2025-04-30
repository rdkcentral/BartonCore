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
//

#pragma once

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
