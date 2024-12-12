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
