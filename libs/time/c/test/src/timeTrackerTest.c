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
// Created by mkoch201 on 3/22/22.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icLog/logging.h>
#include <icTime/timeTracker.h>
#include <inttypes.h>
#include <memory.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_CAT "timeTrackerTest"

static void testTimeTracker(void **state)
{
    (void) state;

    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    timeTracker *tracker = timeTrackerCreate();

    // Assert starting state
    assert_int_equal(timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS), 0);
    assert_int_equal(timeTrackerSecondsUntilExpiration(tracker), 0);
    assert_int_equal(timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS), 0);
    assert_int_equal(timeTrackerElapsedSeconds(tracker), 0);
    assert_false(timeTrackerExpired(tracker));
    assert_false(timeTrackerRunning(tracker));

    timeTrackerStart(tracker, 1);
    // Sleep 50 millis
    usleep(1000 * 50);

    // Make sure values are reasonable with some buffer
    assert_int_equal(timeTrackerElapsedSeconds(tracker), 0);
    assert_in_range(timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS), 50, 150);
    assert_int_equal(timeTrackerSecondsUntilExpiration(tracker), 1);
    assert_in_range(timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS), 850, 950);
    assert_false(timeTrackerExpired(tracker));
    assert_true(timeTrackerRunning(tracker));

    timeTrackerStop(tracker);
    assert_false(timeTrackerExpired(tracker));
    assert_false(timeTrackerRunning(tracker));

    timeTrackerStartWithUnit(tracker, 250, TIME_TRACKER_MILLIS);
    // Sleep 50 millis
    usleep(1000 * 50);
    // Make sure values are reasonable with some buffer
    assert_int_equal(timeTrackerElapsedSeconds(tracker), 0);
    assert_in_range(timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS), 50, 150);
    assert_int_equal(timeTrackerSecondsUntilExpiration(tracker), 1);
    assert_in_range(timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS), 100, 200);
    assert_false(timeTrackerExpired(tracker));
    assert_true(timeTrackerRunning(tracker));

    // Let expire
    usleep(1000 * 250);
    assert_int_equal(timeTrackerElapsedSeconds(tracker), 0);
    assert_in_range(timeTrackerElapsedTime(tracker, TIME_TRACKER_MILLIS), 300, 400);
    assert_int_equal(timeTrackerSecondsUntilExpiration(tracker), 0);
    assert_int_equal(timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS), 0);
    assert_true(timeTrackerExpired(tracker));
    assert_true(timeTrackerRunning(tracker));

    // Stop and reset
    timeTrackerStop(tracker);
    assert_false(timeTrackerExpired(tracker));
    assert_false(timeTrackerRunning(tracker));

    // Try the other units for sanity
    timeTrackerStartWithUnit(tracker, 1, TIME_TRACKER_MINS);
    assert_in_range(timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS), 59 * 1000, 60 * 1000);
    timeTrackerStartWithUnit(tracker, 1, TIME_TRACKER_HOURS);
    assert_in_range(timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS), 59 * 60 * 1000, 60 * 60 * 1000);

    timeTrackerDebug(tracker);

    timeTrackerDestroy(tracker);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(testTimeTracker),
    };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
