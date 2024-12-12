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
// Created by wboyd747 on 5/30/18.
//

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <icTime/timeUtils.h>
#include <icTypes/icQueue.h>

#include "icConcurrent/icBlockingQueue.h"
#include "icConcurrent/timedWait.h"

struct _icBlockingQueue
{
    uint16_t capacity;

    icQueue *queue;

    pthread_mutex_t mutex;
    pthread_cond_t fullCondition;
    pthread_cond_t emptyCondition;

    bool enabled;
};

static int timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, struct timespec *timeout)
{
    int ret = 0;

    if (timeout->tv_sec == BLOCKINGQUEUE_TIMEOUT_INFINITE && timeout->tv_nsec == 0)
    {
        pthread_cond_wait(cond, mutex);
    }
    else if (timeout->tv_sec < 0 || (timeout->tv_sec == 0 && timeout->tv_nsec == 0))
    {
        ret = ETIMEDOUT;
    }
    else
    {
        ret = granularIncrementalCondTimedWait(cond, mutex, timeout);
    }

    return ret;
}

icBlockingQueue *blockingQueueCreate(uint16_t maxCapacity)
{
    icBlockingQueue *bq = malloc(sizeof(struct _icBlockingQueue));
    if (bq)
    {
        bq->queue = queueCreate();
        if (bq->queue)
        {
            bq->capacity = (maxCapacity > 0) ? maxCapacity : BLOCKINGQUEUE_MAX_CAPACITY;
            bq->enabled = true;

            pthread_mutex_init(&bq->mutex, NULL);
            initTimedWaitCond(&bq->fullCondition);
            initTimedWaitCond(&bq->emptyCondition);
        }
        else
        {
            free(bq);
            bq = NULL;
        }
    }

    return bq;
}

void blockingQueueDestroy(icBlockingQueue *queue, blockingQueueItemFreeFunc helper)
{
    if (queue)
    {
        pthread_mutex_lock(&queue->mutex);
        queue->enabled = false;
        pthread_cond_broadcast(&queue->fullCondition);
        pthread_cond_broadcast(&queue->emptyCondition);
        pthread_mutex_unlock(&queue->mutex);

        /* Yield the thread so that other threads can exit their block status. */
        sched_yield();

        pthread_mutex_lock(&queue->mutex);
        queueDestroy(queue->queue, helper);

        pthread_cond_destroy(&queue->fullCondition);
        pthread_cond_destroy(&queue->emptyCondition);
        pthread_mutex_unlock(&queue->mutex);
        pthread_mutex_destroy(&queue->mutex);

        free(queue);
    }
}

uint16_t blockingQueueCount(icBlockingQueue *queue)
{
    uint16_t ret = 0;

    if (queue)
    {
        pthread_mutex_lock(&queue->mutex);
        ret = queueCount(queue->queue);
        pthread_mutex_unlock(&queue->mutex);
    }

    return ret;
}

bool blockingQueuePush(icBlockingQueue *queue, void *item)
{
    return blockingQueuePushTimeout(queue, item, BLOCKINGQUEUE_TIMEOUT_INFINITE);
}

bool blockingQueuePushTimeout(icBlockingQueue *queue, void *item, int timeout)
{
    struct timespec timeoutSpec = {.tv_sec = timeout, .tv_nsec = 0};
    return blockingQueuePushTimeoutGranular(queue, item, &timeoutSpec);
}

bool blockingQueuePushTimeoutGranular(icBlockingQueue *queue, void *item, struct timespec *timeout)
{
    bool ret = false;

    if (queue && item)
    {
        int condRet = 0;
        uint16_t count;

        pthread_mutex_lock(&queue->mutex);
        count = queueCount(queue->queue);
        bool keepWaiting = true;
        bool firstTime = true;
        struct timespec startTime;
        // We can get woken up but the queue is still full, so we should loop and try again
        // until our timeout expires
        while (keepWaiting)
        {
            if (queue->enabled && (count == queue->capacity))
            {
                struct timespec remainingTime = *timeout;
                if (!firstTime)
                {
                    struct timespec currTime;
                    getCurrentTime(&currTime, true);
                    timespecDiff(&currTime, &startTime, &remainingTime);
                    timespecDiff(timeout, &remainingTime, &remainingTime);
                }
                else
                {
                    getCurrentTime(&startTime, true);
                    firstTime = false;
                }

                condRet = timed_wait(&queue->fullCondition, &queue->mutex, &remainingTime);
                count = queueCount(queue->queue);
            }

            /* If we are being destroyed then we just want to bail out. */
            if (!queue->enabled)
            {
                condRet = EINTR;
            }

            if ((condRet == 0) && (count < queue->capacity))
            {
                queuePush(queue->queue, item);

                // Unblock a single waiting thread if there is one waiting
                pthread_cond_signal(&queue->emptyCondition);

                ret = true;
                keepWaiting = false;
            }
            else if (condRet == ETIMEDOUT)
            {
                errno = ETIMEDOUT;
                keepWaiting = false;
            }
            else if (condRet == EINTR)
            {
                errno = EINTR;
                keepWaiting = false;
            }
        }
        pthread_mutex_unlock(&queue->mutex);
    }

    return ret;
}

void *blockingQueuePop(icBlockingQueue *queue)
{
    return blockingQueuePopTimeout(queue, BLOCKINGQUEUE_TIMEOUT_INFINITE);
}

void *blockingQueuePopTimeout(icBlockingQueue *queue, int timeout)
{
    struct timespec timeoutSpec = {.tv_sec = timeout, .tv_nsec = 0};
    return blockingQueuePopTimeoutGranular(queue, &timeoutSpec);
}

void *blockingQueuePopTimeoutGranular(icBlockingQueue *queue, struct timespec *timeout)
{
    void *ret = NULL;

    if (queue)
    {
        int condRet = 0;
        uint16_t count;

        pthread_mutex_lock(&queue->mutex);
        count = queueCount(queue->queue);
        bool keepWaiting = true;
        bool firstTime = true;
        struct timespec startTime;
        // We can get woken up but the queue is still full, so we should loop and try again
        // until our timeout expires
        while (keepWaiting)
        {
            if (queue->enabled && (count == 0))
            {
                struct timespec remainingTime = *timeout;
                if (!firstTime)
                {
                    struct timespec currTime;
                    getCurrentTime(&currTime, true);
                    timespecDiff(&currTime, &startTime, &remainingTime);
                    timespecDiff(timeout, &remainingTime, &remainingTime);
                }
                else
                {
                    getCurrentTime(&startTime, true);
                    firstTime = false;
                }
                condRet = timed_wait(&queue->emptyCondition, &queue->mutex, &remainingTime);
                count = queueCount(queue->queue);
            }

            /* If we are being destroyed then we just want to bail out. */
            if (!queue->enabled)
            {
                condRet = EINTR;
            }

            if ((condRet == 0) && (count > 0))
            {
                ret = queuePop(queue->queue);

                // Unblock a single waiting thread if there is one waiting
                pthread_cond_signal(&queue->fullCondition);

                keepWaiting = false;
            }
            else if (condRet == ETIMEDOUT)
            {
                errno = ETIMEDOUT;
                keepWaiting = false;
            }
            else if (condRet == EINTR)
            {
                errno = EINTR;
                keepWaiting = false;
            }
        }
        pthread_mutex_unlock(&queue->mutex);
    }

    return ret;
}

bool blockingQueueDelete(icBlockingQueue *queue,
                         void *searchVal,
                         blockingQueueCompareFunc searchFunc,
                         blockingQueueItemFreeFunc freeFunc)
{
    bool ret = false;

    if (queue)
    {
        pthread_mutex_lock(&queue->mutex);
        ret = queueDelete(queue->queue, searchVal, searchFunc, freeFunc);

        if (ret)
        {
            // Signal no matter what, we deleted something so if there is a waiter, wake up them up
            pthread_cond_signal(&queue->fullCondition);
        }
        pthread_mutex_unlock(&queue->mutex);
    }

    return ret;
}

void blockingQueueIterate(icBlockingQueue *queue, blockingQueueIterateFunc callback, void *arg)
{
    if (queue)
    {
        pthread_mutex_lock(&queue->mutex);
        queueIterate(queue->queue, callback, arg);
        pthread_mutex_unlock(&queue->mutex);
    }
}

void blockingQueueClear(icBlockingQueue *queue, blockingQueueItemFreeFunc helper)
{
    if (queue)
    {
        uint16_t count;

        pthread_mutex_lock(&queue->mutex);
        count = queueCount(queue->queue);
        queueClear(queue->queue, helper);

        if (count == queue->capacity)
        {
            // Wake up all waiting to push now that we are cleared
            pthread_cond_broadcast(&queue->fullCondition);
        }
        pthread_mutex_unlock(&queue->mutex);
    }
}

void blockingQueueDisable(icBlockingQueue *queue)
{
    if (queue)
    {
        pthread_mutex_lock(&queue->mutex);
        // Mark the queue disabled
        queue->enabled = false;
        // Wake everyone up
        pthread_cond_broadcast(&queue->fullCondition);
        pthread_cond_broadcast(&queue->emptyCondition);
        pthread_mutex_unlock(&queue->mutex);
    }
}

/*
 * returns if the queue is disabled or not
 */
bool blockingQueueIsDisabled(icBlockingQueue *queue)
{
    bool retVal = true;
    if (queue != NULL)
    {
        // TODO: how do we check that the mutex isn't destroyed/bad?
        pthread_mutex_lock(&queue->mutex);
        if (queue->enabled == true)
        {
            retVal = false;
        }
        pthread_mutex_unlock(&queue->mutex);
    }
    return retVal;
}
