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
 * simpleProtectTest.c
 *
 * test utility for libicConfig.  also serves as an
 * example for protecting portions of the file.
 *
 * Author: munilakshmi -  25/02/22.
 * FIXME: upgrade to cmocka testcase
 *-----------------------------------------------*/

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icConfig/simpleProtectConfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void testProtectGetSecretFromKeyring(void **state)
{

    ProtectSecret *secret = simpleProtectGetSecretFromKeyring("secretKey", "default", NULL);

    /* Now lets check if the result is what we expect it to be */
    assert_non_null(secret);

    simpleProtectDestroySecret(secret);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(testProtectGetSecretFromKeyring)};

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
