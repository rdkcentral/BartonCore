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

#include <arpa/inet.h>
#include <cmocka.h>
#include <icUtil/ping.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static void test_ping(void **state)
{

    struct sockaddr_in localhost;

    localhost.sin_family = AF_INET;

    /* Port is not used for ICMP echo */
    localhost.sin_port = 0;
    inet_pton(localhost.sin_family, "127.0.0.1", &localhost.sin_addr);

    struct pingResults results;

    int rc = icPing(NULL, (struct sockaddr *) &localhost, 4, 0, 1, 100, &results);

    if (rc != 0)
    {
        fail_msg("%s", strerror(rc));
    }

    assert_int_equal(results.transmitted, 4);
    assert_int_equal(results.received, 4);
    assert_int_not_equal(results.rtt_avg_usec, 0);

    printf("Avg rtt: %" PRIu32 ", max: %" PRIu32 ", min: %" PRIu32 " µsec \n",
           results.rtt_avg_usec,
           results.rtt_max_usec,
           results.rtt_min_usec);
}

int main(int argc, char *argv[])
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_ping)};

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}
