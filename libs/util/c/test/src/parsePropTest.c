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
 * parsePropTest.c
 *
 * Unit test the parsePropFile object
 *
 * Author: jelderton - 4/13/16
 *-----------------------------------------------*/

#include "parsePropTest.h"
#include <icUtil/parsePropFile.h>
#include <stdio.h>

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
