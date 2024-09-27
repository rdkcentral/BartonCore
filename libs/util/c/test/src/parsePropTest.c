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
 * parsePropTest.c
 *
 * Unit test the parsePropFile object
 *
 * Author: jelderton - 4/13/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <icUtil/parsePropFile.h>
#include "parsePropTest.h"

bool runParsePropFileTests()
{
    // create iterator of etc/sample.properties
    //
    icPropertyIterator *loop = propIteratorCreate("etc/sample.properties");
    if (loop == NULL)
    {
        fprintf(stderr, "failed to open property file 'sample.properties'\n");
        return false;
    }

    // print each entry
    //
    while (propIteratorHasNext(loop) == true)
    {
        icProperty *p = propIteratorGetNext(loop);
        if (p == NULL || p->key == NULL || p->value == NULL)
        {
            propIteratorDestroy(loop);
            fprintf(stderr, "received NULL prop, key, or value'\n");
            return false;
        }

        fprintf(stdout, "prop k='%s' v='%s'\n", p->key, p->value);
        propertyDestroy(p);
    }
    propIteratorDestroy(loop);
    return true;
}

