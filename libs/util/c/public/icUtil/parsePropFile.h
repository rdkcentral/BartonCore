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
 * parsePropFile.h
 *
 * Utilities for parsing .properties files.  Since we had
 * multiple uses for property files (search for key, search
 * for value, examine all properties), the best solution is
 * to implement a "property iterator" to allow looping through
 * each property found in a file.
 *
 * Author: jelderton - 4/13/16
 *-----------------------------------------------*/

#ifndef FLEXCORE_PARSEPROPFILE_H
#define FLEXCORE_PARSEPROPFILE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// opaque object definition
//
typedef struct _icPropIterator icPropertyIterator;

typedef struct _icProperty
{
    char *key;
    char *value;
} icProperty;

/*
 * create a "property iterator" to allow the caller to loop through
 * all of the property definitions within a file.
 * MUST BE FREED via propIteratorDestroy.  will return NULL if
 * there was a problem opening the file.
 *
 * @see propIteratorDestroy()
 */
icPropertyIterator *propIteratorCreate(const char *filename);

/*
 * similar to propIteratorCreate(), but allows a FILE to be supplied.
 * assumes the FILE has been rewound and is ready for read.  generally
 * used when a 'tmpfile' was made to process memory as properties.
 * MUST BE FREED via propIteratorDestroy.
 *
 * @see propIteratorDestroy()
 */
icPropertyIterator *propIteratorCreateFromFile(FILE *fp);

/*
 * free an icPropertyIterator
 */
void propIteratorDestroy(icPropertyIterator *iterator);
void propertyDestroy(icProperty *obj);

/*
 * return if there are more items in the iterator to examine
 */
bool propIteratorHasNext(icPropertyIterator *iterator);

/*
 * return the next item available in the file via the iterator
 * caller should free via propertyDestroy()
 */
icProperty *propIteratorGetNext(icPropertyIterator *iterator);


#endif // FLEXCORE_PARSEPROPFILE_H
