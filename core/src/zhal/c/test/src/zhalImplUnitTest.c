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

#include <cjson/cJSON.h>
#include <cmocka.h>
#include <glib.h>
#include <icConcurrent/threadUtils.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <zhal/zhal.h>
#include <zhalRequests.c>

#define CLUSTER_ID_BASIC                 0x0000
#define ATTRIBUTE_ID_APPLICATION_VERSION 0x0001
#define ATTRIBUTE_ID_HARDWARE_VERSION    0x0003
#define ATTRIBUTE_ID_MANUFACTURER_NAME   0x0004
#define ATTRIBUTE_ID_MODEL_IDENTIFIER    0x0005

#define TEST_TARGET_EUI64                0x000d6f0003c04a7d

typedef struct
{
    void *data;
    size_t dataLen;
} asyncQueueData;

static GAsyncQueue *sendAsyncQueue;
static GAsyncQueue *recvAsyncQueue;

int __real_connect(int fd, const struct sockaddr *addr, socklen_t len);
int __wrap_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    return 0;
}

ssize_t __real_send(int fd, const void *buf, size_t n, int flags);
ssize_t __wrap_send(int fd, const void *buf, size_t n, int flags)
{
    asyncQueueData *sentData = malloc(sizeof(asyncQueueData));
    void *data = malloc(n);
    memcpy(data, buf, n);
    sentData->data = data;
    sentData->dataLen = n;
    g_async_queue_push(sendAsyncQueue, sentData);

    return n;
}

ssize_t __real_recv(int fd, void *buf, size_t n, int flags);
ssize_t __wrap_recv(int fd, void *buf, size_t n, int flags)
{
    asyncQueueData *replyData = g_async_queue_pop(recvAsyncQueue);
    n = (n < replyData->dataLen) ? n : replyData->dataLen;
    memcpy(buf, replyData->data, n);
    return n;
}

int __wrap_zhalNetworkTerm(void)
{
    return 0;
}

/**
 * Helper function for testZhalImpl(), run by a child thread.
 * This function simply makes a zhal request.
 */
static void *sendZhalRequestFunc(void *arg)
{
    cJSON *request = arg;
    cJSON *response;

    int retVal = sendRequestWithTimeout(TEST_TARGET_EUI64, request, 1, &response);
    assert_int_equal(retVal, -1);

    return NULL;
}

/**
 * This tests basic zhal functionality.
 * If there are no heap-use-after-frees or memory leaks, then this test should pass.
 */
static void testZhalImpl(void **state)
{
    zhalCallbacks *callbacks = NULL;

    int retVal = zhalInit("127.0.0.1", 18443, callbacks, NULL, NULL);
    assert_int_equal(retVal, 0);

    // Set up async queues for send() and recv()
    sendAsyncQueue = g_async_queue_new();
    recvAsyncQueue = g_async_queue_new();

    uint16_t attributeIds[] = {
        ATTRIBUTE_ID_HARDWARE_VERSION, ATTRIBUTE_ID_MANUFACTURER_NAME, ATTRIBUTE_ID_MODEL_IDENTIFIER};
    uint8_t numAttributeIds = 3;
    cJSON *request = cJSON_CreateObject();
    setAddress(TEST_TARGET_EUI64, request);
    cJSON *infosJson = cJSON_CreateArray();
    for (uint8_t i = 0; i < numAttributeIds; i++)
    {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "id", attributeIds[i]);
        cJSON_AddItemToArray(infosJson, entry);
    }
    cJSON_AddItemToObject(request, "infos", infosJson);

    // Create a separate copy of the request JSON for verification purposes later
    cJSON *requestVerification = cJSON_Duplicate(request, true);

    pthread_t sendZhalRequestThread;
    retVal = pthread_create(&sendZhalRequestThread, NULL, sendZhalRequestFunc, request);
    assert_int_equal(retVal, 0);

    // Verify that the data sent by the first send() call is correct
    cJSON_AddNumberToObject(requestVerification, "requestId", 0);
    char *payload = cJSON_PrintUnformatted(requestVerification);
    uint16_t payloadLen = strlen(payload);
    asyncQueueData *sentData = g_async_queue_pop(sendAsyncQueue);
    assert_memory_equal(&payloadLen, sentData->data, sentData->dataLen);
    free(sentData->data);
    free(sentData);

    // Verify that the data sent by the second send() call is correct
    asyncQueueData *sentData2 = g_async_queue_pop(sendAsyncQueue);
    assert_memory_equal(payload, sentData2->data, sentData2->dataLen);
    free(sentData2->data);
    free(sentData2);
    free(payload);
    cJSON_Delete(requestVerification);

    // After two successful send() calls, we respond with a JSON fixture that xmit() can
    // basically read as "OK"
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddNumberToObject(reply, "resultCode", 0);
    char *replyStr = cJSON_Print(reply);
    cJSON_Delete(reply);

    uint16_t replyLen = htons(strlen(replyStr));
    asyncQueueData *replyLenData = malloc(sizeof(asyncQueueData));
    replyLenData->data = &replyLen;
    replyLenData->dataLen = sizeof(uint16_t);
    g_async_queue_push(recvAsyncQueue, replyLenData);
    asyncQueueData *replyStrData = malloc(sizeof(asyncQueueData));
    replyStrData->data = replyStr;
    replyStrData->dataLen = strlen(replyStr) + 1;
    g_async_queue_push(recvAsyncQueue, replyStrData);

    zhalTerm();

    retVal = pthread_join(sendZhalRequestThread, NULL);
    assert_int_equal(retVal, 0);

    free(replyStr);
    free(replyLenData);
    free(replyStrData);

    return;
}

int main(int argc, char *argv[])
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(testZhalImpl),
    };

    int retVal = cmocka_run_group_tests(tests, NULL, NULL);

    return retVal;
}
