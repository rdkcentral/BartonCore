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
//
// Created by mkoch201 on 6/11/19.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <inttypes.h>
#include <memory.h>
#include <stdio.h>

#define LOG_CAT        "logTEST"

#define KEY_PREFIX_STR "test %d"
#define VAL_PREFIX_STR "test %d val"

static void test_unixTimeConversions(void **state)
{
    (void) state;

    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    uint64_t millis = getCurrentUnixTimeMillis();

    icLogDebug(LOG_CAT, "%s: unix time %" PRIu64, __FUNCTION__, millis);

    struct timespec ts;
    convertUnixTimeMillisToTimespec(millis, &ts);

    icLogDebug(LOG_CAT, "%s: timespec secs %" PRIu64 " nanos %" PRIu64, __FUNCTION__, ts.tv_sec, ts.tv_nsec);

    uint64_t convertedMillis = convertTimespecToUnixTimeMillis(&ts);

    assert_int_equal(millis, convertedMillis);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_unixTimeConversions),
    };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
