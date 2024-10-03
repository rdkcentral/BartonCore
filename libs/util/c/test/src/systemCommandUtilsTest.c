//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2022 Comcast Corporation
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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icUtil/systemCommandUtils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_execute_ok(void **state)
{
    assert_int_equal(0, executeSystemCommand("true"));
    assert_int_equal(1, executeSystemCommand("false"));
}

void test_execute_nofile(void **state)
{
    assert_int_equal(255, executeSystemCommand("/bin/sofake12345"));
}

void test_shell_ok(void **state)
{
    assert_int_equal(0, execShellCommand("true"));
    assert_int_equal(1, execShellCommand("false"));
}

void test_shell_nofile(void **state)
{
    assert_int_equal(0x7f, execShellCommand("/bin/sofake12345"));
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_execute_ok),
                                       cmocka_unit_test(test_execute_nofile),
                                       cmocka_unit_test(test_shell_ok),
                                       cmocka_unit_test(test_shell_nofile)};

    return cmocka_run_group_tests(tests, NULL, NULL);
}
