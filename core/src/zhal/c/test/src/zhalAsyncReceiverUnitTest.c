//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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

#include <arpa/inet.h>
#include <cmocka.h>
#include <netinet/in.h>
#include <stdbool.h>

#include "zhalAsyncReceiver.h"

static struct in_addr address(const char *str)
{
    struct in_addr addr;
    assert_int_equal(inet_pton(AF_INET, str, &addr), 1);
    return addr;
}

static void testLoopbackIsAlwaysTrusted(void **state)
{
    (void) state;

    struct in_addr loopback = address("127.0.0.1");
    struct in_addr producer = address("172.18.0.5");

    assert_true(zhalAsyncReceiverIsTrustedSource(&loopback, &loopback));
    assert_true(zhalAsyncReceiverIsTrustedSource(&loopback, &producer));
    assert_true(zhalAsyncReceiverIsTrustedSource(&loopback, NULL));
}

static void testProducerAddressIsTrusted(void **state)
{
    (void) state;

    struct in_addr producer = address("172.18.0.5");
    struct in_addr source = address("172.18.0.5");

    assert_true(zhalAsyncReceiverIsTrustedSource(&source, &producer));
}

static void testOtherSourcesAreRejected(void **state)
{
    (void) state;

    struct in_addr producer = address("172.18.0.5");
    struct in_addr loopback = address("127.0.0.1");
    struct in_addr other = address("172.18.0.6");
    struct in_addr any = address("0.0.0.0");

    assert_false(zhalAsyncReceiverIsTrustedSource(&other, &producer));
    // With ZigbeeCore on loopback, only loopback is trusted
    assert_false(zhalAsyncReceiverIsTrustedSource(&other, &loopback));
    // An unset producer must not trust a source of 0.0.0.0
    assert_false(zhalAsyncReceiverIsTrustedSource(&any, &any));
    assert_false(zhalAsyncReceiverIsTrustedSource(&other, NULL));
    assert_false(zhalAsyncReceiverIsTrustedSource(NULL, &producer));
}

static void testStartRejectsNonNumericProducer(void **state)
{
    (void) state;

    assert_int_equal(zhalAsyncReceiverStart("remote-zigbee", NULL, NULL), -1);
    assert_int_equal(zhalAsyncReceiverStart(NULL, NULL, NULL), -1);
}

int main(int argc, char *argv[])
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(testLoopbackIsAlwaysTrusted),
        cmocka_unit_test(testProducerAddressIsTrusted),
        cmocka_unit_test(testOtherSourcesAreRejected),
        cmocka_unit_test(testStartRejectsNonNumericProducer),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
