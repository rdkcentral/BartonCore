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

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <icUtil/ping.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <string.h>

static void test_ping(void** state)
{

    struct sockaddr_in localhost;

    localhost.sin_family = AF_INET;

    /* Port is not used for ICMP echo */
    localhost.sin_port = 0;
    inet_pton(localhost.sin_family, "127.0.0.1", &localhost.sin_addr);

    struct pingResults results;

    int rc = icPing(NULL, (struct sockaddr*) &localhost, 4, 0, 1, 100, &results);

    if (rc != 0) {
        fail_msg("%s", strerror(rc));
    }

    assert_int_equal(results.transmitted, 4);
    assert_int_equal(results.received, 4);
    assert_int_not_equal(results.rtt_avg_usec, 0);

    printf("Avg rtt: %"PRIu32 ", max: %"PRIu32 ", min: %"PRIu32 " µsec \n",
           results.rtt_avg_usec, results.rtt_max_usec, results.rtt_min_usec);
}

int main(int argc, char* argv[])
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ping)
    };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;

}
