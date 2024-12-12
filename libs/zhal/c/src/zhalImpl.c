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
#include <glib.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "zhalAsyncReceiver.h"
#include "zhalEventHandler.h"
#include "zhalPrivate.h"
#include <cjson/cJSON.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icQueue.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
#include <zhal/zhal.h>

/*
 * This is the core IPC processing to ZigbeeCore.  Individual requests are in zhalRequests.c
 *
 * Created by Thomas Lea on 3/14/16.
 */

#define LOG_TAG     "zhalImpl"
#define logFmt(fmt) "%s: " fmt, __func__

typedef struct
{
    icQueue *queue; // WorkItems pending for this device
    pthread_mutex_t mutex;
    int isBusy;
} DeviceQueue;

static void deviceQueueRelease(void *deviceQueue);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(DeviceQueue, deviceQueueRelease)
#define scoped_DeviceQueue g_autoptr(DeviceQueue)

typedef struct
{
    uint64_t eui64;
    uint32_t requestId;
    cJSON *request;
    cJSON *response;
    DeviceQueue *deviceQueue; // handle to the owning device queue
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    bool timedOut;
} WorkItem;

static WorkItem *workItemAcquire(WorkItem *item);
static void workItemRelease(void *item);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(WorkItem, workItemRelease)
#define scoped_WorkItem            g_autoptr(WorkItem)

#define SOCKET_RECEIVE_TIMEOUT_SEC 10
#define SOCKET_SEND_TIMEOUT_SEC    10

static icHashMap *deviceQueues = NULL; // maps uint64_t (EUI64) to DeviceQueue pointers
static pthread_mutex_t deviceQueuesMutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t requestId = 0;

static pthread_mutex_t requestIdMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t workerThread;

static pthread_cond_t workerCond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t workerMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t workerRunningMutex = PTHREAD_MUTEX_INITIALIZER;

static void *workerThreadProc(void *);

static bool workerThreadRunning = false;

static bool shouldWorkerContinue(void);

static icHashMap *asyncRequests = NULL; // maps uint32_t (requestId) to WorkItem
static pthread_mutex_t asyncRequestsMutex = PTHREAD_MUTEX_INITIALIZER;

static void asyncRequestsFreeFunc(void *key, void *value);

static char *zigbeeIp = NULL;

static int zigbeePort = 0;

static int handleIpcResponse(cJSON *response);

static void deviceQueuesDestroy(void *key, void *value);

static bool addQueueItemToPendingList(WorkItem *item, icLinkedList *pendingItems);

static zhalCallbacks *callbacks = NULL;

static void *callbackContext = NULL;

static zhalResponseHandler responseHandler = NULL;

static pthread_mutex_t initializedMtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
bool initialized = false;

zhalCallbacks *getCallbacks(void)
{
    return callbacks;
}

void *getCallbackContext(void)
{
    return callbackContext;
}

int zhalInit(const char *host, int port, zhalCallbacks *cbs, void *ctx, zhalResponseHandler handler)
{
    icLogDebug(LOG_TAG, "zhalInit %s:%d", host, port);

    zigbeeIp = strdup(host);
    zigbeePort = port;
    callbacks = cbs;
    callbackContext = ctx;
    responseHandler = handler;

    pthread_mutex_lock(&deviceQueuesMutex);
    deviceQueues = hashMapCreate();
    pthread_mutex_unlock(&deviceQueuesMutex);

    pthread_mutex_lock(&asyncRequestsMutex);
    asyncRequests = hashMapCreate();
    pthread_mutex_unlock(&asyncRequestsMutex);

    zhalEventHandlerInit();
    zhalAsyncReceiverStart(host, handleIpcResponse, zhalHandleEvent);

    pthread_mutex_lock(&workerRunningMutex);
    workerThreadRunning = true;
    pthread_mutex_unlock(&workerRunningMutex);

    createThread(&workerThread, workerThreadProc, NULL, "zhal");

    mutexLock(&initializedMtx);
    initialized = true;
    mutexUnlock(&initializedMtx);

    return 0;
}

int zhalTerm(void)
{
    icLogDebug(LOG_TAG, "zhalTerm");

    zhalNetworkTerm();

    mutexLock(&initializedMtx);
    initialized = false;
    mutexUnlock(&initializedMtx);

    // Shut down the receiver socket
    zhalAsyncReceiverStop();

    // Shutdown the worker
    pthread_mutex_lock(&workerRunningMutex);
    bool workerThreadWasRunning = workerThreadRunning;
    workerThreadRunning = false;
    pthread_mutex_unlock(&workerRunningMutex);
    pthread_mutex_lock(&workerMutex);
    pthread_cond_signal(&workerCond);
    pthread_mutex_unlock(&workerMutex);

    if (workerThreadWasRunning == true)
    {
        pthread_join(workerThread, NULL);
    }

    callbacks = NULL;
    free(zigbeeIp);
    zigbeeIp = NULL;

    // For anything pending get a copy, then signal the work item condition to simulate a response so callers will
    // wake up.
    icLinkedList *pendingWorkItems = linkedListCreate();

    // First take care of async requests
    pthread_mutex_lock(&asyncRequestsMutex);
    icHashMapIterator *iter = hashMapIteratorCreate(asyncRequests);
    while (hashMapIteratorHasNext(iter) == true)
    {
        uint32_t *key;
        uint16_t keyLen;
        WorkItem *item;
        hashMapIteratorGetNext(iter, (void **) &key, &keyLen, (void **) &item);

        if (!linkedListAppend(pendingWorkItems, workItemAcquire(item)))
        {
            // discard ref if list is null
            workItemRelease(item);
        }
    }
    hashMapIteratorDestroy(iter);
    hashMapDestroy(asyncRequests, asyncRequestsFreeFunc);
    asyncRequests = NULL;
    pthread_mutex_unlock(&asyncRequestsMutex);

    // Now anything in a device queue
    pthread_mutex_lock(&deviceQueuesMutex);
    icHashMapIterator *deviceQueuesIter = hashMapIteratorCreate(deviceQueues);
    while (hashMapIteratorHasNext(deviceQueuesIter) == true)
    {
        uint64_t *key;
        uint16_t keyLen;
        DeviceQueue *deviceQueue;
        hashMapIteratorGetNext(deviceQueuesIter, (void **) &key, &keyLen, (void **) &deviceQueue);

        queueIterate(deviceQueue->queue, (queueIterateFunc) addQueueItemToPendingList, pendingWorkItems);
    }
    hashMapIteratorDestroy(deviceQueuesIter);
    hashMapDestroy(deviceQueues, deviceQueuesDestroy);
    deviceQueues = NULL;
    pthread_mutex_unlock(&deviceQueuesMutex);

    // Now signal all these work items to wake up callers so they can clean up
    icLinkedListIterator *pendingItemsIter = linkedListIteratorCreate(pendingWorkItems);
    while (linkedListIteratorHasNext(pendingItemsIter) == true)
    {
        WorkItem *item = (WorkItem *) linkedListIteratorGetNext(pendingItemsIter);
        pthread_mutex_lock(&item->mtx);
        pthread_cond_signal(&item->cond);
        pthread_mutex_unlock(&item->mtx);
    }
    linkedListIteratorDestroy(pendingItemsIter);
    // The caller owns the WorkItem and should clean it up
    linkedListDestroy(pendingWorkItems, workItemRelease);

    zhalEventHandlerTerm();

    return 0;
}

bool zhalIsInitialized(void)
{
    mutexLock(&initializedMtx);
    bool result = initialized;
    mutexUnlock(&initializedMtx);
    return result;
}

// get the next request id between 0-INT32_MAX
static uint32_t getNextRequestId(void)
{
    uint32_t id;
    pthread_mutex_lock(&requestIdMutex);
    id = requestId++;
    if (requestId > INT32_MAX) // We are using ints in cJSON so wrap as if signed
    {
        requestId = 0;
    }
    pthread_mutex_unlock(&requestIdMutex);
    return id;
}

static bool itemQueueCompareFunc(void *searchVal, void *item)
{
    return searchVal == item ? true : false;
}

static void queueDoNotFreeFunc(void *item) {}

static DeviceQueue *createDeviceQueue(void)
{
    DeviceQueue *deviceQueue = g_atomic_rc_box_new0(DeviceQueue);
    deviceQueue->queue = queueCreate();
    mutexInitWithType(&deviceQueue->mutex, PTHREAD_MUTEX_ERRORCHECK);

    return g_steal_pointer(&deviceQueue);
}

static DeviceQueue *deviceQueueAcquire(DeviceQueue *deviceQueue)
{
    DeviceQueue *out = NULL;

    if (deviceQueue != NULL)
    {
        out = g_atomic_rc_box_acquire(deviceQueue);
    }

    return out;
}

static void destroyDeviceQueue(DeviceQueue *deviceQueue)
{
    queueDestroy(deviceQueue->queue, workItemRelease);
}

static void deviceQueueRelease(void *deviceQueue)
{
    if (deviceQueue != NULL)
    {
        g_atomic_rc_box_release_full((DeviceQueue *) deviceQueue, (GDestroyNotify) destroyDeviceQueue);
    }
}

static void deviceQueuesDestroy(void *key, void *value)
{
    DeviceQueue *deviceQueue = (DeviceQueue *) value;
    deviceQueueRelease(deviceQueue);
    free(key);
}

static WorkItem *createItem(uint64_t targetEui64, cJSON *requestJson, DeviceQueue *deviceQueue)
{
    if (requestJson == NULL || deviceQueue == NULL)
    {
        icError("invalid arguments");
        return NULL;
    }

    WorkItem *item = g_atomic_rc_box_new0(WorkItem);
    item->eui64 = targetEui64;
    item->requestId = getNextRequestId();
    cJSON_AddNumberToObject(requestJson, "requestId", item->requestId);
    item->request = requestJson;
    item->deviceQueue = deviceQueueAcquire(deviceQueue);
    item->timedOut = false;
    initTimedWaitCond(&item->cond);
    mutexInitWithType(&item->mtx, PTHREAD_MUTEX_ERRORCHECK);

    return g_steal_pointer(&item);
}

static WorkItem *workItemAcquire(WorkItem *item)
{
    WorkItem *out = NULL;

    if (item != NULL)
    {
        out = g_atomic_rc_box_acquire(item);
    }

    return out;
}

static void destroyItem(WorkItem *item)
{
    pthread_cond_destroy(&item->cond);
    pthread_mutex_destroy(&item->mtx);
    deviceQueueRelease(item->deviceQueue);
}

static void workItemRelease(void *item)
{
    if (item != NULL)
    {
        g_atomic_rc_box_release_full((WorkItem *) item, (GDestroyNotify) destroyItem);
    }
}

static void asyncRequestsFreeFunc(void *key, void *value)
{
    if (value != NULL)
    {
        workItemRelease(value);
    }
}

/*
 * This blocks until the full operation is complete or it times out
 * This function is not responsible for requestJson cleanup
 */
cJSON *zhalSendRequest(uint64_t targetEui64, cJSON *requestJson, int timeoutSecs)
{
    cJSON *result = NULL;

    if (!zhalIsInitialized())
    {
        icLogError(LOG_TAG, "%s: zhal not initialized", __func__);
        return NULL;
    }

    mutexLock(&deviceQueuesMutex);

    /*
     * Queues are lazily created, so always disambiguate a never-before-seen
     * device request (eui64 not in collection) from a shutdown
     * (no collection at all), both of which will produce a NULL queue, but any
     * resources would become orphans without the queue collection to store them
     * in.
     */
    if (deviceQueues == NULL)
    {
        icWarn("Already shut down, aborting request");
        return NULL;
    }

    scoped_DeviceQueue deviceQueue =
        deviceQueueAcquire((DeviceQueue *) hashMapGet(deviceQueues, &targetEui64, sizeof(uint64_t)));

    if (deviceQueue == NULL)
    {
        icLogDebug(LOG_TAG, "Creating device queue for %016" PRIx64, targetEui64);
        deviceQueue = createDeviceQueue();

        if (deviceQueue != NULL)
        {
            uint64_t *key = (uint64_t *) malloc(sizeof(targetEui64));
            *key = targetEui64;

            /* This is guaranteed as the key was not found above */
            hashMapPut(deviceQueues, key, sizeof(uint64_t), deviceQueueAcquire(deviceQueue));
        }
    }

    mutexUnlock(&deviceQueuesMutex);

    if (deviceQueue == NULL)
    {
        icError("No device queue, aborting request");
        return NULL;
    }

    WorkItem *item = createItem(targetEui64, requestJson, deviceQueue);
    if (item == NULL)
    {
        icError("error creating work item");
        return NULL;
    }

    // lock the item before the worker can get it so we wont miss its completion
    pthread_mutex_lock(&item->mtx);

    // enqueue the work item
    pthread_mutex_lock(&deviceQueue->mutex);
    queuePush(deviceQueue->queue, workItemAcquire(item));
    pthread_mutex_unlock(&deviceQueue->mutex);

    // signal our worker that there is stuff to do
    pthread_mutex_lock(&workerMutex);
    pthread_cond_signal(&workerCond);
    pthread_mutex_unlock(&workerMutex);

    if (ETIMEDOUT == incrementalCondTimedWait(&item->cond, &item->mtx, timeoutSecs))
    {
        icLogWarn(LOG_TAG, "requestId %" PRIu32 " timed out", item->requestId);

        // remove from asyncRequests
        pthread_mutex_lock(&asyncRequestsMutex);
        bool didDeleteFromAsyncRequests =
            hashMapDelete(asyncRequests, &item->requestId, sizeof(item->requestId), asyncRequestsFreeFunc);
        pthread_mutex_unlock(&asyncRequestsMutex);

        // lock the device queue and remove this item if its still there
        pthread_mutex_lock(&deviceQueue->mutex);
        bool didDeleteFromQueue = queueDelete(deviceQueue->queue, item, itemQueueCompareFunc, workItemRelease);

        // Two cases here, could have been never removed from queue to be sent, or it could have been removed and
        // sent, but we didn't get a reply in time.  The busy counter is only incremented in the latter case.  So if we
        // removed it from the queue, we don't need to decrement busy.  But if we are deleting it from our pending
        // async requests, then we should decrement busy.

        if (didDeleteFromAsyncRequests)
        {
            deviceQueue->isBusy--;
        }
        else
        {
            icLogDebug(LOG_TAG, "requestId %" PRIu32 " was not pending, so not changing busy counter", item->requestId);
        }

        // If this item exists in neither place, then it was taken as available work.  We can't clean it up because
        // the worker thread still holds a pointer, so instead we just mark it as timedOut and the other thread will
        // take care of cleanup.  This doesn't feel like a great way of handling this, but I couldn't come up with
        // anything cleaner.
        if (!didDeleteFromAsyncRequests && !didDeleteFromQueue)
        {
            item->timedOut = true;
        }
        pthread_mutex_unlock(&deviceQueue->mutex);

        // There may have been other requests queued up, so signal our worker that there might be stuff to do
        pthread_mutex_lock(&workerMutex);
        pthread_cond_signal(&workerCond);
        pthread_mutex_unlock(&workerMutex);
    }
    else
    {
        result = item->response;
        item->response = NULL;

        // if a response handler is defined, gather details and invoke
        if (responseHandler != NULL && result != NULL)
        {
            int code = ZHAL_STATUS_FAIL;
            cJSON *resultCode = cJSON_GetObjectItem(result, "resultCode");
            if (cJSON_IsNumber(resultCode))
            {
                code = resultCode->valueint;
            }
            else
            {
                icLogWarn(LOG_TAG,
                          "requestId %" PRIu32 " did not get the result code, setting default %d",
                          item->requestId,
                          code);
            }

            char *type = NULL;
            cJSON *responseType = cJSON_GetObjectItem(result, "responseType");
            if (cJSON_IsString(responseType))
            {
                type = responseType->valuestring;
                // invoke only on valid response
                responseHandler(type, code);
            }
        }
    }

    // finally unlock the item and clean up
    pthread_mutex_unlock(&item->mtx);

    workItemRelease(item);

    return result;
}

/*
 * Create a copy of the ReceivedAttributeReport
 */
ReceivedAttributeReport *cloneReceivedAttributeReport(ReceivedAttributeReport *report)
{
    ReceivedAttributeReport *reportCopy = NULL;

    if (report != NULL)
    {
        reportCopy = (ReceivedAttributeReport *) calloc(1, sizeof(ReceivedAttributeReport));
        memcpy(reportCopy, report, sizeof(ReceivedAttributeReport));

        reportCopy->reportData = (uint8_t *) calloc(report->reportDataLen, 1); // since sizeof(uint8_t) is 1
        memcpy(reportCopy->reportData, report->reportData, report->reportDataLen);
    }

    return reportCopy;
}

/*
 * Creates a copy of the ReceivedClusterCommand
 */
ReceivedClusterCommand *cloneReceivedClusterCommand(ReceivedClusterCommand *command)
{
    ReceivedClusterCommand *commandCopy = NULL;

    if (command != NULL)
    {
        commandCopy = (ReceivedClusterCommand *) calloc(1, sizeof(ReceivedClusterCommand));
        memcpy(commandCopy, command, sizeof(ReceivedClusterCommand));

        commandCopy->commandData = (uint8_t *) calloc(command->commandDataLen, 1); // since sizeof(uint8_t) is 1
        memcpy(commandCopy->commandData, command->commandData, command->commandDataLen);
    }

    return commandCopy;
}

/*
 * Free a ReceivedAttributeReport
 */
void freeReceivedAttributeReport(ReceivedAttributeReport *report)
{
    if (report != NULL)
    {
        free(report->reportData);
        free(report);
    }
}

ReceivedAttributeReport *receivedAttributeReportClone(const ReceivedAttributeReport *report)
{
    ReceivedAttributeReport *result = NULL;

    if (report != NULL)
    {
        result = malloc(sizeof(ReceivedAttributeReport));
        memcpy(result, report, sizeof(ReceivedAttributeReport));
        result->reportData = malloc(report->reportDataLen);
        memcpy(result->reportData, report->reportData, report->reportDataLen);
    }

    return result;
}

/*
 * Free a ReceivedClusterCommand
 */
void freeReceivedClusterCommand(ReceivedClusterCommand *command)
{
    if (command != NULL)
    {
        free(command->commandData);
        free(command);
    }
}

ReceivedClusterCommand *receivedClusterCommandClone(const ReceivedClusterCommand *command)
{
    ReceivedClusterCommand *result = NULL;

    if (command != NULL)
    {
        result = malloc(sizeof(ReceivedClusterCommand));
        memcpy(result, command, sizeof(ReceivedClusterCommand));
        result->commandData = malloc(command->commandDataLen);
        memcpy(result->commandData, command->commandData, command->commandDataLen);
    }

    return result;
}

/*
 * Free an OtaUpgradeEvent
 */
void freeOtaUpgradeEvent(OtaUpgradeEvent *otaEvent)
{
    if (otaEvent != NULL)
    {
        free(otaEvent->buffer);
        free(otaEvent->sentStatus);
        free(otaEvent);
    }
}

bool zhalEndpointHasServerCluster(zhalEndpointInfo *endpointInfo, uint16_t clusterId)
{
    bool result = false;

    if (endpointInfo != NULL)
    {
        for (int i = 0; i < endpointInfo->numServerClusterIds; i++)
        {
            if (endpointInfo->serverClusterIds[i] == clusterId)
            {
                result = true;
                break;
            }
        }
    }

    return result;
}

/*
 * Put any work items that are ready into the provided queue.  Return zero if there was no work to do.
 */
static int getAvailableWork(icQueue *availableWork)
{
    int result = 0;

    pthread_mutex_lock(&deviceQueuesMutex);
    icHashMapIterator *queuesIterator = hashMapIteratorCreate(deviceQueues);
    while (hashMapIteratorHasNext(queuesIterator))
    {
        uint64_t *eui64;
        uint16_t keyLen;
        DeviceQueue *dq;
        if (hashMapIteratorGetNext(queuesIterator, (void **) &eui64, &keyLen, (void **) &dq) == true)
        {
            pthread_mutex_lock(&dq->mutex);

            // if a device queue is busy, dont schedule another item
            if (dq->isBusy == 0)
            {
                WorkItem *item = (WorkItem *) queuePop(dq->queue);
                if (item != NULL)
                {
                    if (queuePush(availableWork, item))
                    {
                        item = NULL;
                        result++;
                    }
                    else
                    {
                        workItemRelease(item);
                    }
                }
            }

            pthread_mutex_unlock(&dq->mutex);
        }
    }
    hashMapIteratorDestroy(queuesIterator);
    pthread_mutex_unlock(&deviceQueuesMutex);

    return result;
}

/* send over the socket and await initial synchronous response (quick).  Returns true if that was successful */
static bool xmit(WorkItem *item)
{
    int sock;
    struct sockaddr_in addr;
    int sendFlags = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(zigbeePort);
    inet_aton(zigbeeIp, &(addr.sin_addr));

    if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1)
    {
        icLogError(LOG_TAG, "error opening socket: %s", strerror(errno));
        return false;
    }

#ifdef SO_NOSIGPIPE
    // set socket option to not SIGPIPE on write errors.  not available on some platforms.
    // hopefully if not, they support MSG_NOSIGNAL - which should be used on the send() call
    //
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(LOG_TAG, "unable to set SO_NOSIGPIPE flag on socket");
    }
#endif

    struct timeval timeout = {0, 0};

#ifdef SO_RCVTIMEO
    timeout.tv_sec = SOCKET_RECEIVE_TIMEOUT_SEC;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) == -1)
    {
        char *errstr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "failed setting receive timeout on socket: %s", errstr);
        free(errstr);
    }
#endif

#ifdef SO_SNDTIMEO
    timeout.tv_sec = SOCKET_SEND_TIMEOUT_SEC;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval)) == -1)
    {
        char *errstr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "failed setting send timeout on socket: %s", errstr);
        free(errstr);
    }
#endif

    if (connect(sock, (const struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        // failed to connect to service, close up the socket
        //
        icLogWarn(LOG_TAG, "error connecting to socket: %s", strerror(errno));
        close(sock);
        return false;
    }

    scoped_generic char *payload = cJSON_PrintUnformatted(item->request);
    uint16_t payloadLen = (uint16_t) strlen(payload);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t msgLen = payloadLen;
#else
    uint16_t msgLen = (uint16_t) (((payloadLen & 0xff) << 8) + ((payloadLen & 0xff00) >> 8));
#endif
    if (send(sock, &msgLen, sizeof(uint16_t), sendFlags) < 0)
    {
        icLogError(LOG_TAG, "error sending payload length: %s", strerror(errno));
        close(sock);
        return false;
    }

    if (send(sock, payload, payloadLen, sendFlags) < 0)
    {
        icLogError(LOG_TAG, "error sending payload: %s", strerror(errno));
        close(sock);
        return false;
    }

    // recv taints msgLen but we can only receive 2 bytes into a unit16 - there are no sane bounds checks
    // coverity[tainted_argument]
    if (recv(sock, &msgLen, sizeof(uint16_t), MSG_WAITALL) < 0)
    {
        icLogError(LOG_TAG, "error receiving payload length: %s", strerror(errno));
        close(sock);
        return false;
    }

    msgLen = ntohs(msgLen);

    /* Allocating up to 65K is okay */
    char *reply = (char *) calloc(1, msgLen + 1U);
    errno = 0;
    ssize_t recvLen = recv(sock, reply, msgLen, MSG_WAITALL);
    if (recvLen < 0 || recvLen != msgLen)
    {
        if (recvLen != msgLen)
        {
            icLogError(LOG_TAG, "error receiving payload: message truncated");
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "error receiving payload: %s", errStr);
            free(errStr);
        }

        free(reply);
        close(sock);
        return false;
    }

    cJSON *response = cJSON_Parse(reply);
    free(reply);

    if (response != NULL)
    {
        cJSON *resultCode = cJSON_GetObjectItem(response, "resultCode");
        if (resultCode != NULL && resultCode->valueint == 0)
        {
            close(sock);
            cJSON_Delete(response);
            return true;
        }
    }

    cJSON_Delete(response);

    close(sock);
    return false;
}

static bool workOnItem(WorkItem *item, void *arg)
{
    pthread_mutex_lock(&item->mtx);

    // The item timedOut but couldn't be cleaned up by zhalSendRequest because we were in the midst of trying to
    // work on the item.  So now we must do the clean up.
    if (item->timedOut)
    {
        pthread_mutex_unlock(&item->mtx);
        // continue to iterate
        return true;
    }

    char *json = cJSON_PrintUnformatted(item->request);
    icLogDebug(LOG_TAG, "Worker processing JSON: %s", json);
    free(json);

    // put in asyncRequests
    pthread_mutex_lock(&asyncRequestsMutex);
    if (!hashMapPut(asyncRequests, &item->requestId, sizeof(item->requestId), workItemAcquire(item)))
    {
        // discard previously acquired ref is asyncRequests is null
        workItemRelease(item);
    }
    pthread_mutex_unlock(&asyncRequestsMutex);

    // set device queue busy
    pthread_mutex_lock(&item->deviceQueue->mutex);
    if (item->deviceQueue->isBusy > 0)
    {
        icLogError(
            LOG_TAG, "device queue for %016" PRIx64 " is already busy (%d)!", item->eui64, item->deviceQueue->isBusy);
    }
    item->deviceQueue->isBusy++;
    pthread_mutex_unlock(&item->deviceQueue->mutex);

    // send to ZigbeeCore and wait for immediate/synchronous response
    //  if successful, our async receiver will find and complete it via asyncRequests hash map
    if (xmit(item) == true)
    {
        pthread_mutex_unlock(&item->mtx);
    }
    else
    {
        icLogWarn(LOG_TAG, "xmit failed... aborting work item %" PRIu32, item->requestId);

        // remove from asyncRequests
        pthread_mutex_lock(&asyncRequestsMutex);
        bool didDeleteFromAsyncRequests =
            hashMapDelete(asyncRequests, &item->requestId, sizeof(item->requestId), asyncRequestsFreeFunc);
        pthread_mutex_unlock(&asyncRequestsMutex);

        // clear busy for this device queue
        pthread_mutex_lock(&item->deviceQueue->mutex);

        if (didDeleteFromAsyncRequests == true && item->deviceQueue->isBusy > 0)
        {
            item->deviceQueue->isBusy--;
            if (item->deviceQueue->isBusy != 0)
            {
                icLogError(LOG_TAG,
                           "device queue for %016" PRIx64 " is still busy (%d)!",
                           item->eui64,
                           item->deviceQueue->isBusy);
            }
        }

        pthread_mutex_unlock(&item->deviceQueue->mutex);

        // since it failed, unlock the item here
        pthread_cond_signal(&item->cond);
        pthread_mutex_unlock(&item->mtx);
    }

    return true; // continue to iterate
}

static void *workerThreadProc(void *arg)
{
    icQueue *availableWork = queueCreate();

    while (shouldWorkerContinue())
    {
        // wait to be woken up
        pthread_mutex_lock(&workerMutex);
        while (!getAvailableWork(availableWork))
        {
            if (pthread_cond_wait(&workerCond, &workerMutex) != 0)
            {
                icLogError(LOG_TAG, "failed to wait on workerCond, terminating workerThreadProc!");
                break;
            }
            else if (!shouldWorkerContinue())
            {
                break;
            }
        }
        pthread_mutex_unlock(&workerMutex);

        // Process any items that were ready
        queueIterate(availableWork, (queueIterateFunc) workOnItem, NULL);

        // All items have been processed... clear the queue
        queueClear(availableWork, workItemRelease);
    }

    icLogInfo(LOG_TAG, "workerThreadProc exiting");

    queueDestroy(availableWork, NULL);

    return NULL;
}

static bool shouldWorkerContinue(void)
{
    bool keepRunning;

    pthread_mutex_lock(&workerRunningMutex);
    keepRunning = workerThreadRunning;
    pthread_mutex_unlock(&workerRunningMutex);

    return keepRunning;
}

static void logResponse(cJSON *response)
{
    scoped_generic char *responseString = NULL;

    // filter out anything deemed sensitive, otherwise print the raw JSON
    cJSON *responseType = cJSON_GetObjectItem(response, "ipcResponseType");
    if (responseType && stringCompare(responseType->valuestring, "getSystemStatusResponse", false) == 0)
    {
        cJSON *filteredJson = cJSON_Duplicate(response, true);
        cJSON *networkKey = cJSON_GetObjectItem(filteredJson, "networkKey");
        if (networkKey != NULL)
        {
            cJSON_SetValuestring(networkKey, "<REDACTED>");
        }

        // print the updated payload
        responseString = cJSON_PrintUnformatted(filteredJson);
        cJSON_Delete(filteredJson);
    }
    else
    {
        // print the whole payload
        responseString = cJSON_PrintUnformatted(response);
    }

    if (responseString != NULL)
    {
        icLogDebug(LOG_TAG, "got response: %s", responseString);
    }
}

static int handleIpcResponse(cJSON *response)
{
    int result = 0;

    logResponse(response);

    cJSON *requestIdJson = cJSON_GetObjectItem(response, "requestId");
    if (requestIdJson != NULL)
    {
        pthread_mutex_lock(&asyncRequestsMutex);
        uint32_t rid = requestIdJson->valueint;
        scoped_WorkItem item = workItemAcquire(hashMapGet(asyncRequests, &rid, sizeof(rid)));
        bool didDeleteFromAsyncRequests = hashMapDelete(asyncRequests, &rid, sizeof(rid), asyncRequestsFreeFunc);
        pthread_mutex_unlock(&asyncRequestsMutex);

        if (item != NULL)
        {
            // clear busy for this device queue
            pthread_mutex_lock(&item->deviceQueue->mutex);

            if (didDeleteFromAsyncRequests == true && item->deviceQueue->isBusy > 0)
            {
                item->deviceQueue->isBusy--;
                if (item->deviceQueue->isBusy != 0)
                {
                    icLogError(LOG_TAG,
                               "device queue for %016" PRIx64 " is still busy (%d)!",
                               item->eui64,
                               item->deviceQueue->isBusy);
                }
            }
            pthread_mutex_unlock(&item->deviceQueue->mutex);

            pthread_mutex_lock(&item->mtx);
            item->response = response;
            pthread_cond_signal(&item->cond);
            pthread_mutex_unlock(&item->mtx);
        }
        else
        {
            icLogDebug(LOG_TAG, "handleIpcResponse did not find %d", requestIdJson->valueint);
        }

        result = 1; // we handled it
    }

    // this might have freed up a device queue, let the worker thread take another look.
    pthread_mutex_lock(&workerMutex);
    pthread_cond_signal(&workerCond);
    pthread_mutex_unlock(&workerMutex);

    return result;
}

static bool addQueueItemToPendingList(WorkItem *item, icLinkedList *pendingItems)
{
    if (!linkedListAppend(pendingItems, workItemAcquire(item)))
    {
        // discard ref is append fails
        workItemRelease(item);
    }
    return true; // continue to iterate
}
