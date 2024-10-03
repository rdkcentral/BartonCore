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

#include "icUtil/regexUtils.h"
#include <icLog/logging.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>
#include <memory.h>

#define LOG_TAG "regexUtils"

static void logRegexError(RegexReplacer *replacer, int err);

bool regexInitReplacers(RegexReplacer **replacers)
{
    bool ok = true;
    RegexReplacer **start = replacers;

    if (replacers == NULL)
    {
        icLogError(LOG_TAG, "Replacers array is NULL");
        return false;
    }

    while (*replacers != NULL)
    {
        RegexReplacer *replacer = *replacers;
        if (!regexInitReplacer(replacer))
        {
            icLogError(LOG_TAG, "Invalid replacer at %zd", replacers - start);
        }

        replacers++;
    }

    return ok;
}

bool regexInitReplacer(RegexReplacer *replacer)
{
    if (replacer->pattern == NULL)
    {
        icLogError(LOG_TAG, "No pattern specified for replacer");
        return false;
    }

    if (!replacer->ready)
    {
        int err = 0;
        err = regcomp(&replacer->regex, replacer->pattern, replacer->cflags);

        if (err == 0)
        {
            if (replacer->numReplacements == replacer->regex.re_nsub + 1)
            {
                replacer->ready = true;
            }
            else
            {
                icLogError(LOG_TAG,
                           "Replacements count %d != expression count %zu",
                           replacer->numReplacements,
                           replacer->regex.re_nsub + 1);
            }
        }
        else
        {
            char errStr[128];
            regerror(err, &replacer->regex, errStr, sizeof(errStr));
            icLogError(LOG_TAG, "Regular expression %s failed to compile: %s", replacer->pattern, errStr);
        }
    }

    return replacer->ready;
}

/*
 * Algorithm:
 *
 * 1. For each replacer, execute the regex if initialized
 * 2. While no regex error occurs:
 *  a. Look for a subexpression match that has a replacement and swap the subexpression match with its replacement
 *  b. If the global flag is set, increment the match offset, and re-execute the regex starting at the offset until
 *     out of matches
 * 3. Log any abnormal regex errors and return the string with any matches replaced. Not finding a match is OK
 */
char *regexReplace(const char *text, RegexReplacer **replacers)
{
    char *editedText = NULL;
    char *curText = strdup(text);

    while (replacers != NULL && *replacers != NULL)
    {
        RegexReplacer *replacer = *replacers;
        size_t numMatches = replacer->regex.re_nsub + 1;
        regmatch_t matches[numMatches];
        int err = 0;
        bool global = false;

        if (replacer->ready)
        {
            err = regexec(&replacer->regex, curText, numMatches, matches, replacer->eflags);
            global = (replacer->replaceFlags & REGEX_GLOBAL) != 0;
        }
        else
        {
            icLogError(LOG_TAG, "Cannot regexReplace with an uninitialized replacer");
            err = REG_NOMATCH;
        }

        size_t matchOffset = 0;
        while (!err)
        {
            bool matchedOne = false;
            for (int i = 0; i < numMatches; i++)
            {
                regmatch_t match = matches[i];
                const char *replacement = replacer->replacements[i];
                if (replacement != NULL && match.rm_so != -1 && match.rm_eo != -1)
                {
                    size_t matchLen = (size_t) match.rm_eo - (size_t) match.rm_so;

                    curText = stringEdit(curText, (size_t) match.rm_so + matchOffset, matchLen, replacement);
                    editedText = curText;

                    if (global)
                    {
                        size_t replacementLen = strlen(replacement);
                        matchOffset += (size_t) match.rm_eo;

                        if (replacementLen > matchLen)
                        {
                            matchOffset += replacementLen - matchLen;
                        }
                        else if (replacementLen < matchLen)
                        {
                            matchOffset -= matchLen - replacementLen;
                        }
                    }

                    matchedOne = true;
                    break;
                }
                else
                {
                    icLogTrace(LOG_TAG,
                               "Subexpression %d %s",
                               i,
                               replacement == NULL ? "has no replacement" : "did not match");
                }
            }

            if (global && matchedOne)
            {
                err = regexec(&replacer->regex, curText + matchOffset, numMatches, matches, replacer->eflags);
                if (err != REG_NOMATCH && err != 0)
                {
                    logRegexError(replacer, err);
                    break;
                }
            }
            else
            {
                break;
            }
        }

        replacers++;
    }

    return editedText == NULL ? curText : editedText;
}

static void logRegexError(RegexReplacer *replacer, int err)
{
    char errStr[128];
    regerror(err, &replacer->regex, errStr, sizeof(errStr));
    icLogWarn(LOG_TAG, "Text replacement failed: %s", errStr);
}

void regexDestroyReplacers(RegexReplacer **replacers)
{
    while (*replacers != NULL)
    {
        RegexReplacer *replacer = *replacers;
        regexDestroyReplacer(replacer);

        replacers++;
    }
}

void regexDestroyReplacer(RegexReplacer *replacer)
{
    if (replacer->ready)
    {
        regfree(&replacer->regex);
        replacer->ready = false;
    }
}

/**
 * Substitute text matching a patterns.
 * @param data to replace occurence(s) of pattern in
 * @param replacersList  list of a null terminated arrays of replacers
 * @return A pointer to a heap allocated string
 */
char *regexReplaceForListOfReplacers(const char *data, icLinkedList *replacersList)
{
    char *cleanData = NULL;

    if (data == NULL)
    {
        icLogWarn(LOG_TAG, "Data is not a C string");
        return NULL;
    }

    if (replacersList != NULL && linkedListCount(replacersList) > 0)
    {
        scoped_icLinkedListIterator *replacersListIterator = linkedListIteratorCreate(replacersList);

        RegexReplacer **replacer = NULL;
        cleanData = strdup(data);

        while (linkedListIteratorHasNext(replacersListIterator) == true)
        {
            replacer = linkedListIteratorGetNext(replacersListIterator);
            char *tempData = regexReplace(cleanData, replacer);
            free(cleanData);
            cleanData = tempData;
            tempData = NULL;
        }
    }

    if (cleanData == NULL)
    {
        cleanData = strdup(data);
    }

    return cleanData;
}