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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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


#endif //FLEXCORE_PARSEPROPFILE_H
