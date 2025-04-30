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

/*-----------------------------------------------
 * parsePropFile.c
 *
 * Utilities for parsing .properties files.  Since we had
 * multiple uses for property files (search for key, search
 * for value, examine all properties), the best solution is
 * to implement a "property iterator" to allow looping through
 * each property found in a file.
 *
 * Author: jelderton - 4/13/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icUtil/parsePropFile.h>
#include <icUtil/stringUtils.h>

#define MAX_LINE_LEN 2048

// model the icPropertyIterator
//
struct _icPropIterator
{
    FILE *fp;    // property file currently being parsed
    char *key;   //
    char *value; //
};

/*
 * create a "property iterator" to allow the caller to loop through
 * all of the property definitions within a file.
 * MUST BE FREED via propIteratorDestroy.  will return NULL if
 * there was a problem opening the file.
 *
 * @see propIteratorDestroy()
 */
icPropertyIterator *propIteratorCreate(const char *filename)
{
    // allocate the return object
    //
    icPropertyIterator *retVal = (icPropertyIterator *) malloc(sizeof(icPropertyIterator));
    if (retVal != NULL)
    {
        memset(retVal, 0, sizeof(icPropertyIterator));

        // attempt to open the file
        //
        if ((retVal->fp = fopen(filename, "r")) == NULL)
        {
            // error opening file
            //
            free(retVal);
            retVal = NULL;
        }
    }
    return retVal;
}

/*
 * similar to propIteratorCreate(), but allows a FILE to be supplied.
 * assumes the FILE has been rewound and is ready for read.  generally
 * used when a 'tmpfile' was made to process memory as properties.
 * MUST BE FREED via propIteratorDestroy.
 *
 * @see propIteratorDestroy()
 */
icPropertyIterator *propIteratorCreateFromFile(FILE *fp)
{
    // allocate the return object
    //
    icPropertyIterator *retVal = (icPropertyIterator *) malloc(sizeof(icPropertyIterator));
    if (retVal != NULL)
    {
        memset(retVal, 0, sizeof(icPropertyIterator));
        retVal->fp = fp;
    }
    return retVal;
}

/*
 * free an icPropertyIterator
 */
void propIteratorDestroy(icPropertyIterator *iterator)
{
    if (iterator == NULL)
    {
        return;
    }

    // close the file
    //
    if (iterator->fp != NULL)
    {
        fclose(iterator->fp);
        iterator->fp = NULL;
    }

    // now free mem
    //
    if (iterator->key != NULL)
    {
        free(iterator->key);
        iterator->key = NULL;
    }
    if (iterator->value != NULL)
    {
        free(iterator->value);
        iterator->value = NULL;
    }
    free(iterator);
}

/*
 * free an icProperty
 */
void propertyDestroy(icProperty *obj)
{
    if (obj == NULL)
    {
        return;
    }

    // now free mem
    //
    if (obj->key != NULL)
    {
        free(obj->key);
        obj->key = NULL;
    }
    if (obj->value != NULL)
    {
        free(obj->value);
        obj->value = NULL;
    }
    free(obj);
}

/*
 * return if there are more items in the iterator to examine
 */
bool propIteratorHasNext(icPropertyIterator *iterator)
{
    if (iterator == NULL)
    {
        return false;
    }

    // read the next 'valid' row from the file.  if we hit EOF,
    // then close our file and bail.  otherwise, prime our 'iterator'
    // with the key/value so they can be placed into a 'property'
    // when the caller hits the 'getNext' function
    //
    bool retVal = false;
    char tempKey[MAX_LINE_LEN];
    char tempVal[MAX_LINE_LEN];
    char readBuffer[MAX_LINE_LEN + 1];
    while (fgets(readBuffer, MAX_LINE_LEN, iterator->fp) != (char *) 0)
    {
        // since this is a .properties file, skip blank lines
        // and ones that begin with '#'
        //
        if (stringStartsWith(readBuffer, "#", false) == true)
        {
            // comment line
            continue;
        }
        if (strlen(readBuffer) < 3)
        {
            // probably blank.  need at least 3 chars for x=y
            continue;
        }

        // extract the key & value from the line
        //
        if (stringSplitOnToken(readBuffer, '=', tempKey, tempVal) == true)
        {
            // save key/value into the iterator and set
            // the return flag to true since there is data to 'get'
            //
            iterator->key = strdup(tempKey);
            iterator->value = strdup(tempVal);
            retVal = true;
            break;
        }
    }

    if (retVal == false)
    {
        // go ahead and close the file in case
        // caller forgets to call 'destroy'
        //
        fclose(iterator->fp);
        iterator->fp = NULL;
    }

    return retVal;
}

/*
 * return the next item available in the file via the iterator
 * caller should free via propertyDestroy()
 */
icProperty *propIteratorGetNext(icPropertyIterator *iterator)
{
    if (iterator == NULL)
    {
        return NULL;
    }

    // create 'property' object, and stuff into it our 'key' and 'value'
    //
    icProperty *retVal = (icProperty *) malloc(sizeof(icProperty));
    retVal->key = iterator->key;
    retVal->value = iterator->value;

    // set iterator values to NULL so we don't free them twice
    //
    iterator->key = NULL;
    iterator->value = NULL;

    return retVal;
}
