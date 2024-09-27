//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2019 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 6/11/19.
//

#include <stdio.h>
#include <icLog/logging.h>
#include <memory.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <icTime/timeUtils.h>
#include <inttypes.h>

#define LOG_CAT     "logTEST"

#define KEY_PREFIX_STR  "test %d"
#define VAL_PREFIX_STR  "test %d val"

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
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_unixTimeConversions),
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}