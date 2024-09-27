//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 iControl Networks, Inc.
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
 * simpleProtectTest.c
 *
 * test utility for libicConfig.  also serves as an
 * example for protecting portions of the file.
 *
 * Author: munilakshmi -  25/02/22.
 * FIXME: upgrade to cmocka testcase
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>
#include <icConfig/simpleProtectConfig.h>

static void testProtectGetSecretFromKeyring(void **state)
{

    ProtectSecret *secret = simpleProtectGetSecretFromKeyring("secretKey", "default", NULL);

    /* Now lets check if the result is what we expect it to be */
    assert_non_null(secret);

    simpleProtectDestroySecret(secret);

}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] =
            {
                cmocka_unit_test(testProtectGetSecretFromKeyring)
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}