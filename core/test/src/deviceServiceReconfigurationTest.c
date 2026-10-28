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

/*
 * Drives the real deviceService startup pass, reconfiguration scheduling, completion and cancellation with every
 * collaborator outside deviceService.c faked through linker --wrap. Devices live in the real JSON database in a
 * temporary directory; scheduled reconfiguration tasks are captured instead of run, so each test decides when they run.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <ftw.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "barton-core-client.h"
#include "barton-core-initialize-params-container.h"
#include "database/jsonDatabase.h"
#include "device-driver/device-driver.h"
#include "device/deviceModelHelper.h"
#include "deviceService.h"
#include "deviceServicePrivate.h"
#include "event/deviceEventProducer.h"
#include "icConcurrent/delayedTask.h"
#include "icConcurrent/threadPool.h"
#include "icTypes/icLinkedList.h"
#include "icUtil/stringUtils.h"
#include "observability/observabilityMetrics.h"
#include "subsystemManager.h"

#define FAKE_DRIVER_NAME      "fakeDriver"
#define FAKE_DEVICE_CLASS     "fakeClass"
#define CURRENT_CLASS_VERSION 2
#define STALE_CLASS_VERSION   1
#define MAX_SCHEDULED_TASKS   8
#define WAIT_TIMEOUT_SECS     5

// How long a test gives a thread that should stay blocked to get past the point it is blocked at. Only a regression can
// get past it, so a slow host can at worst miss one, never fail a correct build.
#define BLOCKED_PROBE_USECS   (500 * G_TIME_SPAN_MILLISECOND)

typedef struct
{
    taskCallbackFunc func;
    void *arg;
    uint32_t handle;
    uint64_t delay;
    bool consumed; // run or cancelled; the task's context reference is no longer ours to hand out
} ScheduledTask;

typedef struct
{
    BCoreClient *client;
    gchar *storageDir;
    bool stopped;
} Fixture;

static GMutex fakeLock;
static GCond fakeCond;

static subsystemManagerReadyForDevicesFunc readyForDevices;
static DeviceDriver fakeDriver;

static ScheduledTask scheduledTasks[MAX_SCHEDULED_TASKS];
static int scheduledCount;
static uint32_t nextHandle;
static bool failScheduling;
static bool readyWhenScheduled;
static bool startTaskWhenScheduled;
static GThread *startedTaskThread;
static int64_t lastPendingGaugeValue;
static ObservabilityGauge *pendingGauge;
static bool scheduleOverflowed;
static gint64 taskStartedAt;

static bool cancelSucceeds;
static int cancelCount;

static int readyEventCount;
static bool readyWhenReadyEventSent;
static bool pendingWhenReadyEventSent;
static const char *watchedUuid;

static int finishedEventCount;
static bool finishedEventSucceeded;
static bool pendingWhenFinishedEventSent;
static gchar *finishedEventUuid;
static void (*finishedEventHook)(const char *uuid);

static int removedEventCount;
static int finishedEventsWhenRemoved;
static void (*removedEventHook)(const char *uuid);
static bool failDeviceRemoval;

static GHashTable *syncCounts; // uuid -> number of synchronizeDevice() calls

static void (*classVersionHook)(void);

static bool failThreadPoolAdd;

static bool pauseShutdown; // pause shutdown just after it sets its shutting-down flag, until shutdownReleased
static bool shutdownReleased;
static bool shutdownBegan;
static bool commFailShutdownCalled; // shutdown got past waiting for reconfigurations
static bool passReachedDevice;
static bool passReleased;
static void (*readyEventHook)(void);

static bool probeShutdownWait;
static const char *requestWhileCancelling; // device to request a reconfiguration of once a cancel was attempted
static bool requestDuringSwap;             // make that request while its record is briefly missing, as mid-swap
// The missingRecordOnCheck'th record check (from 1) for this device reports none, as if mid-swap or just deleted.
static const char *missingRecordUuid;
static int missingRecordOnCheck;
static int recordChecks;
static bool shutdownContinuedBeforeTaskRan;

static GThread *removalThread;
static bool removalFinishedDuringEvent;

static BCoreClient *testClient;
static GThread *requestThread;
static GThread *eventStopThread; // started by StopFromOtherThreadDuringEvent()
static bool shutdownPassedDuringEvent;
static bool requestReturned;
static bool requestReturnedDuringEvent;
static bool requestScheduledDuringEvent;
static bool pendingAfterRequestDuringEvent;
static bool mergedCallbackCalled;
static bool stopCalledFromHandler;   // a handler started stopping the client, so teardown must not stop it again
static bool stopReturnedFromHandler; // ...and that stop returned

/* ---------------------------------------------------------------------------------------------------------------- */
/* Waiting                                                                                                          */
/* ---------------------------------------------------------------------------------------------------------------- */

// Wait (holding fakeLock) until predicate() holds or timeoutUsecs pass, and return predicate(). Bounded so a regression
// fails the test rather than hanging it.
static bool WaitLocked(bool (*predicate)(void), gint64 timeoutUsecs)
{
    gint64 deadline = g_get_monotonic_time() + timeoutUsecs;

    while (predicate() == false)
    {
        if (g_cond_wait_until(&fakeCond, &fakeLock, deadline) == FALSE)
        {
            break;
        }
    }

    return predicate();
}

static bool IsCancelAttempted(void)
{
    return cancelCount > 0;
}

static bool IsShutdownBegun(void)
{
    return shutdownBegan;
}

static bool IsShutdownReleased(void)
{
    return shutdownReleased;
}

static bool IsCommFailShutdownCalled(void)
{
    return commFailShutdownCalled;
}

static bool IsPassAtDevice(void)
{
    return passReachedDevice;
}

static bool IsDeviceRemovedEventSent(void)
{
    return removedEventCount > 0;
}

static bool IsRequestReturned(void)
{
    return requestReturned;
}

static bool IsPassReleased(void)
{
    return passReleased;
}

static bool IsStopReturnedFromHandler(void)
{
    return stopReturnedFromHandler;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Fake driver                                                                                                      */
/* ---------------------------------------------------------------------------------------------------------------- */

static bool FakeGetDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version)
{
    (void) ctx;
    (void) deviceClass;

    // One-shot, so the hook runs for the first (snapshot) check only.
    void (*hook)(void) = classVersionHook;
    classVersionHook = NULL;

    if (hook != NULL)
    {
        hook();
    }

    *version = CURRENT_CLASS_VERSION;

    return true;
}

static void FakeSynchronizeDevice(void *ctx, icDevice *device)
{
    (void) ctx;

    g_mutex_lock(&fakeLock);
    gpointer count = g_hash_table_lookup(syncCounts, device->uuid);
    g_hash_table_insert(syncCounts, g_strdup(device->uuid), GINT_TO_POINTER(GPOINTER_TO_INT(count) + 1));
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Wrapped collaborators                                                                                            */
/* ---------------------------------------------------------------------------------------------------------------- */

void __wrap_subsystemManagerInitialize(subsystemManagerReadyForDevicesFunc readyForDevicesCallback)
{
    readyForDevices = readyForDevicesCallback;
}

void __wrap_subsystemManagerAllDriversStarted(void) {}

void __wrap_subsystemManagerShutdown(void) {}

GPtrArray *__wrap_subsystemManagerGetRegisteredSubsystems(void)
{
    return g_ptr_array_new();
}

void __wrap_deviceEventProducerStartup(BCoreClient *service)
{
    (void) service;
}

void __wrap_deviceEventProducerShutdown(void) {}

void __wrap_deviceEventHandlerStartup(void) {}

// The first thing shutdown does after setting its shutting-down flag, so tests can pause shutdown there.
void __wrap_deviceEventHandlerShutdown(void)
{
    g_mutex_lock(&fakeLock);
    shutdownBegan = true;
    g_cond_broadcast(&fakeCond);

    if (pauseShutdown == true)
    {
        WaitLocked(IsShutdownReleased, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    }

    g_mutex_unlock(&fakeLock);
}

void __wrap_deviceServiceCommFailInit(void) {}

// Shutdown's next step after waiting for reconfigurations.
void __wrap_deviceServiceCommFailShutdown(void)
{
    g_mutex_lock(&fakeLock);
    commFailShutdownCalled = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);
}

void __wrap_deviceServiceCommFailSetTimeoutSecs(uint32_t commFailTimeoutSecs)
{
    (void) commFailTimeoutSecs;
}

void __wrap_deviceStorageMonitorStart(void) {}

void __wrap_deviceStorageMonitorStop(void) {}

void __wrap_deviceDescriptorsInit(const char *allowListPath, const char *denyListPath)
{
    (void) allowListPath;
    (void) denyListPath;
}

void __wrap_deviceServiceDeviceDescriptorsDestroy(void) {}

bool __wrap_deviceDriverManagerInitialize(void)
{
    return true;
}

bool __wrap_deviceDriverManagerStartDeviceDrivers(void)
{
    return true;
}

bool __wrap_deviceDriverManagerShutdown(void)
{
    return true;
}

DeviceDriver *__wrap_deviceDriverManagerGetDeviceDriver(const char *driverName)
{
    return g_strcmp0(driverName, FAKE_DRIVER_NAME) == 0 ? &fakeDriver : NULL;
}

icLinkedList *__wrap_deviceDriverManagerGetDeviceDrivers(void)
{
    return linkedListCreate();
}

void __wrap_initEventTracker(void) {}

void __wrap_shutDownEventTracker(void) {}

static gpointer RunTask(gpointer data)
{
    ScheduledTask *task = data;

    task->func(task->arg);
    g_free(task);

    return NULL;
}

uint32_t __wrap_scheduleDelayTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg)
{
    (void) units;

    g_mutex_lock(&fakeLock);
    uint32_t handle = 0;

    if (failScheduling == false && scheduledCount >= MAX_SCHEDULED_TASKS)
    {
        scheduleOverflowed = true;
    }

    if (failScheduling == false && scheduledCount < MAX_SCHEDULED_TASKS)
    {
        handle = nextHandle++;
        scheduledTasks[scheduledCount++] =
            (ScheduledTask) {.func = func, .arg = arg, .handle = handle, .delay = delayAmount};

        // Like a real zero-delay task: it starts at once, possibly before this call returns.
        if (startTaskWhenScheduled == true)
        {
            scheduledTasks[scheduledCount - 1].consumed = true;
            ScheduledTask *task = g_new(ScheduledTask, 1);
            *task = scheduledTasks[scheduledCount - 1];
            startedTaskThread = g_thread_new("task", RunTask, task);
        }
    }

    readyWhenScheduled = deviceServiceIsReadyForDeviceOperation();
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);

    return handle;
}

// Run inline, so every synchronization a startup pass queues has finished when the pass returns. With
// failThreadPoolAdd, reject the task (freeing its argument) as a full queue does.
bool __wrap_threadPoolAddTask(icThreadPool *pool, threadPoolTask task, void *arg, threadPoolTaskArgFreeFunc argFreeFunc)
{
    (void) pool;

    if (failThreadPoolAdd == false)
    {
        task(arg);
    }

    if (argFreeFunc != NULL)
    {
        argFreeFunc(arg);
    }
    else
    {
        free(arg);
    }

    return failThreadPoolAdd == false;
}

ObservabilityGauge *__real_observabilityGaugeCreate(const char *name, const char *description, const char *unit);
void __real_observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value);

ObservabilityGauge *__wrap_observabilityGaugeCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityGauge *gauge = __real_observabilityGaugeCreate(name, description, unit);

    if (g_strcmp0(name, "device.reconfiguration.pending") == 0)
    {
        g_mutex_lock(&fakeLock);
        pendingGauge = gauge;
        g_mutex_unlock(&fakeLock);
    }

    return gauge;
}

void __wrap_observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value)
{
    g_mutex_lock(&fakeLock);

    if (gauge != NULL && gauge == pendingGauge)
    {
        lastPendingGaugeValue = value;
    }

    g_mutex_unlock(&fakeLock);

    __real_observabilityGaugeRecord(gauge, value);
}

void *__wrap_cancelDelayTask(uint32_t task)
{
    void *arg = NULL;

    g_mutex_lock(&fakeLock);
    cancelCount++;
    g_cond_broadcast(&fakeCond);

    for (int i = 0; i < scheduledCount; i++)
    {
        if (scheduledTasks[i].handle == task && scheduledTasks[i].consumed == false && cancelSucceeds == true)
        {
            scheduledTasks[i].consumed = true;
            arg = scheduledTasks[i].arg;
        }
    }

    g_mutex_unlock(&fakeLock);

    return arg;
}

void __wrap_sendDeviceServiceStatusEvent(DeviceServiceStatusChangedReason reason)
{
    if (reason != DeviceServiceStatusChangedReasonReadyForDeviceOperation)
    {
        return;
    }

    bool ready = deviceServiceIsReadyForDeviceOperation();
    bool pending = watchedUuid != NULL && deviceServiceIsReconfigurationPending(watchedUuid);

    g_mutex_lock(&fakeLock);
    readyEventCount++;
    readyWhenReadyEventSent = ready;
    pendingWhenReadyEventSent = pending;
    void (*hook)(void) = readyEventHook;
    g_mutex_unlock(&fakeLock);

    // Runs as a client's status-changed handler would: on the thread publishing readiness.
    if (hook != NULL)
    {
        hook();
    }
}

void __wrap_sendDeviceReconfigurationFinishedEvent(const char *deviceClass, const char *uuid, bool succeeded)
{
    (void) deviceClass;

    // Before taking fakeLock: the gauge wrapper takes fakeLock under the reconfiguration lock this check takes.
    bool pending = deviceServiceIsReconfigurationPending(uuid);

    g_mutex_lock(&fakeLock);
    finishedEventCount++;
    finishedEventSucceeded = succeeded;
    pendingWhenFinishedEventSent = pending;
    g_free(finishedEventUuid);
    finishedEventUuid = g_strdup(uuid);
    void (*hook)(const char *) = finishedEventHook;
    g_mutex_unlock(&fakeLock);

    // Runs as a client's signal handler would: on the reconfiguration task's thread, while the event is being sent.
    if (hook != NULL)
    {
        hook(uuid);
    }
}

void __wrap_sendDeviceRemovedEvent(const char *uuid, const char *deviceClass)
{
    (void) deviceClass;

    g_mutex_lock(&fakeLock);
    removedEventCount++;
    finishedEventsWhenRemoved = finishedEventCount;
    g_cond_broadcast(&fakeCond);
    void (*hook)(const char *) = removedEventHook;
    g_mutex_unlock(&fakeLock);

    // Runs as a client's device-removed handler would: during the removal, after the record is deleted.
    if (hook != NULL)
    {
        hook(uuid);
    }
}

bool __real_jsonDatabaseIsDeviceKnown(const char *uuid);

bool __wrap_jsonDatabaseIsDeviceKnown(const char *uuid)
{
    g_mutex_lock(&fakeLock);
    bool missing = false;

    if (missingRecordUuid != NULL && g_strcmp0(uuid, missingRecordUuid) == 0)
    {
        missing = ++recordChecks == missingRecordOnCheck;
    }

    g_mutex_unlock(&fakeLock);

    return missing == true ? false : __real_jsonDatabaseIsDeviceKnown(uuid);
}

bool __real_jsonDatabaseRemoveDeviceById(const char *uuid);

// With failDeviceRemoval, deleting a device's record fails, as a storage failure would.
bool __wrap_jsonDatabaseRemoveDeviceById(const char *uuid)
{
    if (failDeviceRemoval == true)
    {
        return false;
    }

    return __real_jsonDatabaseRemoveDeviceById(uuid);
}

void __wrap_sendEndpointRemovedEvent(icDeviceEndpoint *endpoint, const char *deviceClass, bool deviceRemoved)
{
    (void) endpoint;
    (void) deviceClass;
    (void) deviceRemoved;
}

void __wrap_sendMetadataUpdatedEvent(icDeviceMetadata *metadata)
{
    (void) metadata;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Helpers                                                                                                          */
/* ---------------------------------------------------------------------------------------------------------------- */

static int RemovePath(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void) sb;
    (void) typeflag;
    (void) ftwbuf;

    return remove(path);
}

static void AddDevice(const char *uuid, uint8_t classVersion)
{
    // No manufacturer/model/version resources: a reconfiguration of this device fails early, before any driver
    // configuration, which is all these tests need to drive its completion.
    icDevice *device = createDevice(uuid, FAKE_DEVICE_CLASS, classVersion, FAKE_DRIVER_NAME, NULL);
    device->uri = stringBuilder("/%s", uuid);

    assert_true(jsonDatabaseAddDevice(device));
    deviceDestroy(device);
}

static int GetSyncCount(const char *uuid)
{
    g_mutex_lock(&fakeLock);
    int count = GPOINTER_TO_INT(g_hash_table_lookup(syncCounts, uuid));
    g_mutex_unlock(&fakeLock);

    return count;
}

#ifdef OBSERVABILITY_BACKEND_RECORDS_GAUGES
static int64_t GetPendingGaugeValue(void)
{
    g_mutex_lock(&fakeLock);
    int64_t value = lastPendingGaugeValue;
    g_mutex_unlock(&fakeLock);

    return value;
}
#endif

static int GetScheduledCount(void)
{
    g_mutex_lock(&fakeLock);
    int count = scheduledCount;
    g_mutex_unlock(&fakeLock);

    return count;
}

// Run a captured reconfiguration task as the delayed-task thread would.
static void RunScheduledTask(int index)
{
    g_mutex_lock(&fakeLock);
    ScheduledTask task = scheduledTasks[index];
    scheduledTasks[index].consumed = true;
    g_mutex_unlock(&fakeLock);

    assert_false(task.consumed);
    task.func(task.arg);
}

// Runs the captured task only after a canceller has tried (and, with cancelSucceeds false, failed) to cancel it, so
// that canceller is known to be waiting for this task rather than racing it. With probeShutdownWait, the task first
// gives a shutdown that is (correctly) blocked on it time to get past its wait anyway.
static gpointer RunFirstScheduledTaskOnceCancelAttempted(gpointer data)
{
    (void) data;

    g_mutex_lock(&fakeLock);

    bool attempted = WaitLocked(IsCancelAttempted, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);

    if (attempted == true && probeShutdownWait == true)
    {
        shutdownContinuedBeforeTaskRan = WaitLocked(IsCommFailShutdownCalled, BLOCKED_PROBE_USECS);
    }

    const char *requestUuid = requestWhileCancelling;
    bool duringSwap = requestDuringSwap;
    g_mutex_unlock(&fakeLock);

    // While the canceller waits for this (still pending) reconfiguration, as a driver's post-upgrade request might.
    if (attempted == true && requestUuid != NULL)
    {
        icDevice *saved = duringSwap ? jsonDatabaseGetDeviceById(requestUuid) : NULL;

        if (saved != NULL)
        {
            __real_jsonDatabaseRemoveDeviceById(requestUuid);
        }

        deviceServiceReconfigureDevice(requestUuid, 60, NULL, false);

        if (saved != NULL)
        {
            jsonDatabaseAddDevice(saved);
            deviceDestroy(saved);
        }
    }

    g_mutex_lock(&fakeLock);
    ScheduledTask task = scheduledTasks[0];
    scheduledTasks[0].consumed = true;
    taskStartedAt = g_get_monotonic_time();
    g_mutex_unlock(&fakeLock);

    task.func(task.arg);

    return NULL;
}

static gpointer RunReadyCallback(gpointer data)
{
    (void) data;

    readyForDevices();

    return NULL;
}

static gpointer StopClient(gpointer data)
{
    b_core_client_stop(data);

    return NULL;
}

static gpointer RemoveDevice(gpointer data)
{
    deviceServiceRemoveDevice(data);

    return NULL;
}

// Finished-event hook: a removal that starts while the completion event is being sent must wait until it has been sent.
static void StartRemovalDuringEvent(const char *uuid)
{
    removalThread = g_thread_new("remove", RemoveDevice, (gpointer) g_intern_string(uuid));

    g_mutex_lock(&fakeLock);
    removalFinishedDuringEvent = WaitLocked(IsDeviceRemovedEventSent, BLOCKED_PROBE_USECS);
    g_mutex_unlock(&fakeLock);
}

static gpointer RequestReconfiguration(gpointer data)
{
    deviceServiceReconfigureDevice(data, 0, NULL, false);

    g_mutex_lock(&fakeLock);
    requestReturned = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);

    return NULL;
}

// Finished-event hook: a reconfiguration requested from another thread while this one is finishing must return without
// waiting (the requester may be a thread this handler depends on) and be scheduled straight away.
static void RequestReconfigurationFromOtherThreadDuringEvent(const char *uuid)
{
    requestThread = g_thread_new("request", RequestReconfiguration, (gpointer) g_intern_string(uuid));

    g_mutex_lock(&fakeLock);
    requestReturnedDuringEvent = WaitLocked(IsRequestReturned, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    requestScheduledDuringEvent = scheduledCount >= 2;
    g_mutex_unlock(&fakeLock);

    // Outside fakeLock: the gauge wrapper takes fakeLock under the reconfiguration lock this check takes.
    pendingAfterRequestDuringEvent = deviceServiceIsReconfigurationPending(uuid);
}

// Finished-event hook: a client that requests another reconfiguration from its completion handler.
static void RequestReconfigurationFromHandler(const char *uuid)
{
    deviceServiceReconfigureDevice(uuid, 0, NULL, false);
}

static void RecordMergedCallback(bool result, const char *uuid)
{
    (void) result;
    (void) uuid;

    mergedCallbackCalled = true;
}

// Finished-event hook: two different requests, made while the device is being removed.
static void RequestTwoReconfigurationsFromHandler(const char *uuid)
{
    g_mutex_lock(&fakeLock);
    cancelSucceeds = true; // so teardown can cancel the re-requested one
    g_mutex_unlock(&fakeLock);

    deviceServiceReconfigureDevice(uuid, 0, NULL, false);
    deviceServiceReconfigureDevice(uuid, 60, RecordMergedCallback, false);
}

// Finished-event hook: as above, with the new reconfiguration cancellable (it hasn't started).
static void RequestCancellableReconfigurationFromHandler(const char *uuid)
{
    g_mutex_lock(&fakeLock);
    cancelSucceeds = true;
    g_mutex_unlock(&fakeLock);

    deviceServiceReconfigureDevice(uuid, 0, NULL, false);
}

// Finished-event hook: another thread stops the service while this completion event is being sent. Shutdown must wait
// for the event rather than tear down the event producer under it.
static void StopFromOtherThreadDuringEvent(const char *uuid)
{
    (void) uuid;

    eventStopThread = g_thread_new("stop", StopClient, testClient);

    g_mutex_lock(&fakeLock);
    shutdownPassedDuringEvent = WaitLocked(IsCommFailShutdownCalled, BLOCKED_PROBE_USECS);
    g_mutex_unlock(&fakeLock);
}

// Stops the client as a handler would, recording that it did so (and returned) for the test and its teardown.
static void StopClientAsHandler(void)
{
    g_mutex_lock(&fakeLock);
    stopCalledFromHandler = true;
    g_mutex_unlock(&fakeLock);

    b_core_client_stop(testClient);

    g_mutex_lock(&fakeLock);
    stopReturnedFromHandler = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);
}

// Finished-event hook: a client that stops the service from its completion handler.
static void StopClientFromHandler(const char *uuid)
{
    (void) uuid;

    StopClientAsHandler();
}

// A callback on the default main context, dispatched on the service's event thread: stops the client from there.
static gboolean StopClientFromEventLoop(gpointer data)
{
    (void) data;

    StopClientAsHandler();

    return G_SOURCE_REMOVE;
}

// Finished-event hook: a client that removes the device from its completion handler.
static void RemoveDeviceFromHandler(const char *uuid)
{
    assert_true(deviceServiceRemoveDevice(uuid));
}

// Completion callback that leaves no record of the device, as a failed save of the rebuilt device does.
static void RemoveRecordOnCompletion(bool result, const char *uuid)
{
    (void) result;

    assert_true(jsonDatabaseRemoveDeviceById(uuid));
}

// An earlier pass's reconfiguration of this device swapped in its rebuilt record after this pass took its snapshot.
static void ReplaceStaleDeviceWithCurrent(void)
{
    assert_true(jsonDatabaseRemoveDeviceById("stale"));
    AddDevice("stale", CURRENT_CLASS_VERSION);
}

static void ResetFakes(void)
{
    readyForDevices = NULL;
    memset(scheduledTasks, 0, sizeof(scheduledTasks));
    scheduledCount = 0;
    nextHandle = 100;
    failScheduling = false;
    readyWhenScheduled = false;
    startTaskWhenScheduled = false;
    startedTaskThread = NULL;
    lastPendingGaugeValue = -1;
    scheduleOverflowed = false;
    taskStartedAt = 0;
    cancelSucceeds = true;
    cancelCount = 0;
    readyEventCount = 0;
    readyWhenReadyEventSent = false;
    pendingWhenReadyEventSent = false;
    watchedUuid = NULL;
    finishedEventCount = 0;
    finishedEventSucceeded = true;
    pendingWhenFinishedEventSent = true;
    g_clear_pointer(&finishedEventUuid, g_free);
    finishedEventHook = NULL;
    removedEventCount = 0;
    removedEventHook = NULL;
    failDeviceRemoval = false;
    finishedEventsWhenRemoved = -1;
    g_hash_table_remove_all(syncCounts);
    classVersionHook = NULL;
    failThreadPoolAdd = false;
    pauseShutdown = false;
    shutdownReleased = false;
    shutdownBegan = false;
    commFailShutdownCalled = false;
    passReachedDevice = false;
    passReleased = false;
    readyEventHook = NULL;
    stopCalledFromHandler = false;
    stopReturnedFromHandler = false;
    probeShutdownWait = false;
    requestWhileCancelling = NULL;
    requestDuringSwap = false;
    missingRecordUuid = NULL;
    missingRecordOnCheck = 0;
    recordChecks = 0;
    shutdownContinuedBeforeTaskRan = false;
    removalThread = NULL;
    removalFinishedDuringEvent = false;
    requestThread = NULL;
    eventStopThread = NULL;
    shutdownPassedDuringEvent = false;
    requestReturned = false;
    requestReturnedDuringEvent = false;
    requestScheduledDuringEvent = false;
    pendingAfterRequestDuringEvent = false;
    mergedCallbackCalled = false;
}

static int SetupService(void **state)
{
    ResetFakes();

    memset(&fakeDriver, 0, sizeof(fakeDriver));
    fakeDriver.driverName = FAKE_DRIVER_NAME;
    fakeDriver.getDeviceClassVersion = FakeGetDeviceClassVersion;
    fakeDriver.synchronizeDevice = FakeSynchronizeDevice;

    Fixture *fixture = g_new0(Fixture, 1);
    fixture->storageDir = g_dir_make_tmp("deviceServiceReconfigurationTestXXXXXX", NULL);
    assert_non_null(fixture->storageDir);

    g_autoptr(BCoreInitializeParamsContainer) params = b_core_initialize_params_container_new();
    b_core_initialize_params_container_set_storage_dir(params, fixture->storageDir);

    fixture->client = b_core_client_new(params);
    testClient = fixture->client;
    assert_true(b_core_client_start(fixture->client));
    assert_non_null(readyForDevices);

    *state = fixture;

    return 0;
}

static int TeardownService(void **state)
{
    Fixture *fixture = *state;

    if (fixture->stopped == false)
    {
        b_core_client_stop(fixture->client);
    }

    g_object_unref(fixture->client);
    nftw(fixture->storageDir, RemovePath, 16, FTW_DEPTH | FTW_PHYS);
    g_free(fixture->storageDir);
    g_free(fixture);

    // Exceeding the fake's capacity would silently look like a scheduling failure.
    if (scheduleOverflowed == true)
    {
        print_error("more than %d reconfigurations were scheduled\n", MAX_SCHEDULED_TASKS);

        return -1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Tests                                                                                                            */
/* ---------------------------------------------------------------------------------------------------------------- */

// A device needing reconfiguration is pending before readiness is observable, and the startup failure fallback
// synchronizes it once its reconfiguration fails.
static void test_startupPassFlagsDevicesBeforePublishingReadiness(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    AddDevice("current", CURRENT_CLASS_VERSION);
    watchedUuid = "stale";

    assert_false(deviceServiceIsReadyForDeviceOperation());

    readyForDevices();

    assert_true(deviceServiceIsReadyForDeviceOperation());
    assert_int_equal(GetScheduledCount(), 1);
    assert_false(readyWhenScheduled);
    assert_int_equal(readyEventCount, 1);
    assert_true(readyWhenReadyEventSent);
    assert_true(pendingWhenReadyEventSent);
    assert_true(deviceServiceIsReconfigurationPending("stale"));
    assert_false(deviceServiceIsReconfigurationPending("current"));

    // Only the up-to-date device is synchronized by the pass; reconfiguration covers the other.
    assert_int_equal(GetSyncCount("current"), 1);
    assert_int_equal(GetSyncCount("stale"), 0);

    RunScheduledTask(0);

    assert_int_equal(finishedEventCount, 1);
    assert_string_equal(finishedEventUuid, "stale");
    assert_false(finishedEventSucceeded);
    assert_false(pendingWhenFinishedEventSent);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_int_equal(GetSyncCount("stale"), 1);
}

static void BlockUntilShutdownBegins(void)
{
    g_mutex_lock(&fakeLock);
    passReachedDevice = true;
    g_cond_broadcast(&fakeCond);
    WaitLocked(IsShutdownBegun, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    g_mutex_unlock(&fakeLock);
}

// Class version hook: holds the startup pass at its first device until the test releases it.
static void HoldPassUntilReleased(void)
{
    g_mutex_lock(&fakeLock);
    passReachedDevice = true;
    g_cond_broadcast(&fakeCond);
    WaitLocked(IsPassReleased, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    g_mutex_unlock(&fakeLock);
}

// Ready-event hook: a client that stops the service from its status-changed handler.
static void StopClientFromReadyHandler(void)
{
    StopClientAsHandler();
}

// Shutdown waits for a startup pass in progress before tearing down the drivers and thread pool it uses.
static void test_shutdownWaitsForStartupPassInProgress(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    classVersionHook = HoldPassUntilReleased;

    GThread *passThread = g_thread_new("readyPass", RunReadyCallback, NULL);

    g_mutex_lock(&fakeLock);
    bool passAtDevice = WaitLocked(IsPassAtDevice, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    g_mutex_unlock(&fakeLock);

    GThread *stopThread = g_thread_new("stop", StopClient, fixture->client);

    g_mutex_lock(&fakeLock);
    bool shutdownPassedDuringPass = WaitLocked(IsCommFailShutdownCalled, BLOCKED_PROBE_USECS);
    passReleased = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);

    g_thread_join(passThread);
    g_thread_join(stopThread);
    fixture->stopped = true;

    assert_true(passAtDevice);
    assert_false(shutdownPassedDuringPass);
    assert_int_equal(GetScheduledCount(), 0);
    assert_int_equal(readyEventCount, 0);
}

// A client may stop the service from its ready-event handler; shutdown must not wait for the pass it is in.
static void test_stopFromReadyEventHandlerDoesNotWaitForItself(void **state)
{
    Fixture *fixture = *state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    readyEventHook = StopClientFromReadyHandler;
    gint64 start = g_get_monotonic_time();

    readyForDevices();
    fixture->stopped = stopCalledFromHandler;

    assert_true(stopReturnedFromHandler);
    assert_true(g_get_monotonic_time() - start < WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    assert_int_equal(readyEventCount, 1);
}

// A client may stop the service from a callback the service's event loop dispatches (for example a signal handler, or
// an idle callback when the client runs no loop of its own); stopping must not wait for that loop to return.
static void test_stopFromEventLoopCallbackReturns(void **state)
{
    Fixture *fixture = *state;

    g_idle_add(StopClientFromEventLoop, NULL);

    g_mutex_lock(&fakeLock);
    bool returned = WaitLocked(IsStopReturnedFromHandler, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    fixture->stopped = stopCalledFromHandler;
    g_mutex_unlock(&fakeLock);

    assert_true(returned);
}

// Stopping again after the client was stopped from its event loop (as a host might at exit) is harmless.
static void test_stopAgainAfterStopFromEventLoop(void **state)
{
    Fixture *fixture = *state;

    g_idle_add(StopClientFromEventLoop, NULL);

    g_mutex_lock(&fakeLock);
    bool returned = WaitLocked(IsStopReturnedFromHandler, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    fixture->stopped = stopCalledFromHandler;
    g_mutex_unlock(&fakeLock);

    assert_true(returned);

    b_core_client_stop(fixture->client);
}

// Shutdown beginning mid-pass stops scheduling and withholds readiness.
static void test_shutdownDuringStartupPassWithholdsReadiness(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    classVersionHook = BlockUntilShutdownBegins;
    pauseShutdown = true;

    GThread *passThread = g_thread_new("readyPass", RunReadyCallback, NULL);

    g_mutex_lock(&fakeLock);
    bool passAtDevice = WaitLocked(IsPassAtDevice, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    g_mutex_unlock(&fakeLock);

    GThread *stopThread = g_thread_new("stop", StopClient, fixture->client);
    g_thread_join(passThread);

    // Observed while shutdown is paused, before it clears anything itself; asserted only after shutdown is released,
    // so a failure can't leave it blocked.
    bool ready = deviceServiceIsReadyForDeviceOperation();
    bool pending = deviceServiceIsReconfigurationPending("stale");
    int scheduled = GetScheduledCount();
    int readyEvents = readyEventCount;

    g_mutex_lock(&fakeLock);
    shutdownReleased = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);

    g_thread_join(stopThread);
    fixture->stopped = true;

    assert_true(passAtDevice);
    assert_false(ready);
    assert_false(pending);
    assert_int_equal(scheduled, 0);
    assert_int_equal(readyEvents, 0);
}

// Published readiness ends as soon as shutdown begins, and a subsystem that reports ready during shutdown starts no
// new startup pass.
static void test_readinessWithdrawnOnceShutdownBegins(void **state)
{
    Fixture *fixture = *state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    readyForDevices();
    assert_true(deviceServiceIsReadyForDeviceOperation());
    AddDevice("stale", STALE_CLASS_VERSION);

    pauseShutdown = true;
    GThread *stopThread = g_thread_new("stop", StopClient, fixture->client);

    g_mutex_lock(&fakeLock);
    bool began = WaitLocked(IsShutdownBegun, WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    g_mutex_unlock(&fakeLock);

    // Observed while shutdown is paused before it clears any readiness flag itself.
    bool ready = deviceServiceIsReadyForDeviceOperation();
    readyForDevices();
    int scheduled = GetScheduledCount();
    int readyEvents = readyEventCount;
    int currentSyncs = GetSyncCount("current");

    g_mutex_lock(&fakeLock);
    shutdownReleased = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);

    g_thread_join(stopThread);
    fixture->stopped = true;

    assert_true(began);
    assert_false(ready);
    assert_int_equal(scheduled, 0);
    assert_int_equal(readyEvents, 1);
    assert_int_equal(currentSyncs, 1);
}

static void test_reconfigurationFinishUnpendsBeforeNotifying(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    deviceServiceReconfigureDevice("stale", 0, NULL, false);

    // Already pending from the startup pass, so not scheduled again.
    assert_int_equal(GetScheduledCount(), 1);

    RunScheduledTask(0);

    assert_int_equal(finishedEventCount, 1);
    assert_false(pendingWhenFinishedEventSent);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
}

static void test_schedulingFailureLeavesDeviceNotPending(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    RunScheduledTask(0);
    failScheduling = true;

    deviceServiceReconfigureDevice("stale", 0, NULL, false);

    assert_false(deviceServiceIsReconfigurationPending("stale"));
}

static void test_removalCancelsNotYetStartedReconfiguration(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    assert_true(deviceServiceIsReconfigurationPending("stale"));

    assert_true(deviceServiceRemoveDevice("stale"));

    assert_int_equal(cancelCount, 1);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_null(jsonDatabaseGetDeviceById("stale"));
    assert_int_equal(finishedEventCount, 0);
}

// A reconfiguration that already started can't be cancelled; removal blocks until it finishes, then proceeds
// promptly rather than at its 60 second timeout. The failed startup reconfiguration doesn't fall back to synchronizing
// the device being removed.
static void test_removalWaitsForRunningReconfiguration(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);
    gint64 start = g_get_monotonic_time();

    assert_true(deviceServiceRemoveDevice("stale"));

    gint64 returnedAt = g_get_monotonic_time();
    g_thread_join(taskThread);

    assert_true(returnedAt >= taskStartedAt);
    assert_true(returnedAt - start < WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    assert_int_equal(finishedEventCount, 1);
    assert_int_equal(removedEventCount, 1);
    assert_int_equal(finishedEventsWhenRemoved, 1);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_null(jsonDatabaseGetDeviceById("stale"));
    assert_int_equal(GetSyncCount("stale"), 0);
}

// Nor does a failed reconfiguration a startup pass skipped the device for, when a removal is what it was waiting on.
static void test_skippedDeviceNotSynchronizedWhileBeingRemoved(void **state)
{
    (void) state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    deviceServiceReconfigureDevice("current", 60, NULL, false);
    readyForDevices();
    cancelSucceeds = false;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_true(deviceServiceRemoveDevice("current"));
    g_thread_join(taskThread);

    assert_int_equal(finishedEventCount, 1);
    assert_false(finishedEventSucceeded);
    assert_int_equal(removedEventCount, 1);
    assert_int_equal(GetSyncCount("current"), 0);
}

// A removal that starts while the completion event is being sent (after the device stopped being reported as
// reconfiguring) waits for the event, so device-removed never precedes it.
static void test_removalDuringCompletionEventWaitsForIt(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    finishedEventHook = StartRemovalDuringEvent;

    RunScheduledTask(0);
    g_thread_join(removalThread);

    assert_false(removalFinishedDuringEvent);
    assert_int_equal(finishedEventCount, 1);
    assert_int_equal(removedEventCount, 1);
    assert_int_equal(finishedEventsWhenRemoved, 1);
    assert_null(jsonDatabaseGetDeviceById("stale"));
}

// A client may remove the device from its completion handler; that removal must not wait for the event it is in.
static void test_removalFromCompletionHandlerDoesNotWaitForItself(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    finishedEventHook = RemoveDeviceFromHandler;
    gint64 start = g_get_monotonic_time();

    RunScheduledTask(0);

    assert_true(g_get_monotonic_time() - start < WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    assert_int_equal(removedEventCount, 1);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_null(jsonDatabaseGetDeviceById("stale"));
}

// A reconfiguration requested while the previous one is sending its completion event is scheduled, not dropped.
static void test_reconfigurationRequestedWhileFinishingIsScheduled(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    finishedEventHook = RequestReconfigurationFromOtherThreadDuringEvent;

    RunScheduledTask(0);
    g_thread_join(requestThread);

    assert_true(requestReturnedDuringEvent);
    assert_true(requestScheduledDuringEvent);
    assert_true(pendingAfterRequestDuringEvent);
    assert_int_equal(GetScheduledCount(), 2);
    assert_true(deviceServiceIsReconfigurationPending("stale"));
}

// A reconfiguration requested from the finishing one's own event handler is scheduled.
static void test_reconfigurationRequestedFromCompletionHandlerIsScheduled(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    finishedEventHook = RequestReconfigurationFromHandler;

    RunScheduledTask(0);

    assert_int_equal(GetScheduledCount(), 2);
    assert_true(deviceServiceIsReconfigurationPending("stale"));
}

// A reconfiguration requested while the device is being removed isn't scheduled: it is deferred, and discarded once the
// removal succeeds, so it can't run after the removal and re-save the removed device.
static void test_reconfigurationRequestedDuringRemovalIsRefused(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    finishedEventHook = RequestCancellableReconfigurationFromHandler;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_true(deviceServiceRemoveDevice("stale"));
    g_thread_join(taskThread);

    assert_int_equal(finishedEventCount, 1);
    assert_int_equal(GetScheduledCount(), 1);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_null(jsonDatabaseGetDeviceById("stale"));
}

// A fallback synchronization skipped because the device was being removed runs once that removal fails.
static void test_synchronizationSkippedByFailedRemovalRunsAfterIt(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    failDeviceRemoval = true;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_false(deviceServiceRemoveDevice("stale"));
    g_thread_join(taskThread);

    assert_false(finishedEventSucceeded);
    assert_true(jsonDatabaseIsDeviceKnown("stale"));
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_int_equal(GetSyncCount("stale"), 1);
}

// A reconfiguration refused because the device was being removed is requested again if that removal fails, so it isn't
// lost. The synchronization skipped during the removal is left to that reconfiguration, and runs if it fails.
static void test_reconfigurationRefusedByFailedRemovalIsRequestedAgain(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    failDeviceRemoval = true;
    finishedEventHook = RequestCancellableReconfigurationFromHandler; // so teardown can cancel the re-requested one

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_false(deviceServiceRemoveDevice("stale"));
    g_thread_join(taskThread);

    assert_int_equal(GetScheduledCount(), 2);
    assert_true(deviceServiceIsReconfigurationPending("stale"));
    assert_true(jsonDatabaseIsDeviceKnown("stale"));
    assert_int_equal(GetSyncCount("stale"), 0);

    finishedEventHook = NULL;
    RunScheduledTask(1);

    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_int_equal(GetSyncCount("stale"), 1);
}

// A request made while a removal waits for the device's still-pending reconfiguration is deferred like any other made
// during a removal, not dropped as a duplicate, so it is requested again when the removal fails.
static void test_reconfigurationRequestedWhileRemovalCancelsIsNotLost(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    failDeviceRemoval = true;
    requestWhileCancelling = "stale";

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_false(deviceServiceRemoveDevice("stale"));
    g_thread_join(taskThread);

    g_mutex_lock(&fakeLock);
    cancelSucceeds = true; // so teardown can cancel the re-requested one
    g_mutex_unlock(&fakeLock);

    assert_int_equal(GetScheduledCount(), 2);
    assert_int_equal(scheduledTasks[1].delay, 60);
    assert_true(deviceServiceIsReconfigurationPending("stale"));
}

// The same, with the request made while the device's record is briefly missing (as during a reconfiguration's swap): it
// is still deferred, not refused as an unknown device.
static void test_reconfigurationRequestedDuringSwapWhileRemovingIsNotLost(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    failDeviceRemoval = true;
    requestWhileCancelling = "stale";
    requestDuringSwap = true;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_false(deviceServiceRemoveDevice("stale"));
    g_thread_join(taskThread);

    g_mutex_lock(&fakeLock);
    cancelSucceeds = true; // so teardown can cancel the re-requested one
    g_mutex_unlock(&fakeLock);

    assert_int_equal(GetScheduledCount(), 2);
    assert_int_equal(scheduledTasks[1].delay, 60);
}

// A request whose first record check finds none (the record was briefly missing, e.g. a swap that has since finished)
// and that finds no removal in progress reads the record again rather than refusing an existing device.
static void test_reconfigurationOfBrieflyMissingRecordIsScheduled(void **state)
{
    (void) state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    readyForDevices();
    missingRecordUuid = "current";
    missingRecordOnCheck = 1;

    deviceServiceReconfigureDevice("current", 0, NULL, false);

    assert_int_equal(GetScheduledCount(), 1);
    assert_true(deviceServiceIsReconfigurationPending("current"));
}

// A device deleted between the record check and the insert is backed out of the pending map rather than scheduled.
static void test_reconfigurationOfDeviceDeletedDuringRequestIsBackedOut(void **state)
{
    (void) state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    readyForDevices();
    missingRecordUuid = "current";
    missingRecordOnCheck = 2; // the check after the insert

    deviceServiceReconfigureDevice("current", 0, NULL, false);

    assert_int_equal(GetScheduledCount(), 0);
    assert_false(deviceServiceIsReconfigurationPending("current"));
}

// Shutdown started on another thread while a completion event is being sent waits for that event.
static void test_shutdownWaitsForCompletionEventInProgress(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    finishedEventHook = StopFromOtherThreadDuringEvent;

    RunScheduledTask(0);

    // Without a completion event nothing stopped the service; leave that to teardown and fail here.
    assert_non_null(eventStopThread);
    g_thread_join(eventStopThread);
    fixture->stopped = true;

    assert_int_equal(finishedEventCount, 1);
    assert_false(shutdownPassedDuringEvent);
}

// A request without a device uuid is refused rather than crashing.
static void test_reconfigurationWithoutUuidIsRefused(void **state)
{
    (void) state;

    readyForDevices();
    deviceServiceReconfigureDevice(NULL, 0, NULL, false);

    assert_int_equal(GetScheduledCount(), 0);
}

// Requests refused during a removal that then fails are merged, so the one requested again keeps the longest delay and
// the completion callback.
static void test_reconfigurationsRefusedByFailedRemovalAreMerged(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    failDeviceRemoval = true;
    finishedEventHook = RequestTwoReconfigurationsFromHandler;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);

    assert_false(deviceServiceRemoveDevice("stale"));
    g_thread_join(taskThread);

    assert_int_equal(GetScheduledCount(), 2);
    assert_int_equal(scheduledTasks[1].delay, 60);

    finishedEventHook = NULL;
    RunScheduledTask(1);
    assert_true(mergedCallbackCalled);
}

// A reconfiguration requested for a device that has no record is refused, and one requested from a device-removed
// handler is deferred and then discarded with the removal; neither leaves the deleted device reported as reconfiguring.
static void test_reconfigurationOfDeviceWithoutRecordIsRefused(void **state)
{
    (void) state;

    deviceServiceReconfigureDevice("missing", 0, NULL, false);
    assert_int_equal(GetScheduledCount(), 0);
    assert_false(deviceServiceIsReconfigurationPending("missing"));

    AddDevice("current", CURRENT_CLASS_VERSION);
    readyForDevices();
    removedEventHook = RequestReconfigurationFromHandler;

    assert_true(deviceServiceRemoveDevice("current"));

    assert_int_equal(GetScheduledCount(), 0);
    assert_false(deviceServiceIsReconfigurationPending("current"));
}

// A client may stop the service from a completion handler; shutdown must not wait for the event it is in.
static void test_stopFromCompletionHandlerDoesNotWaitForItself(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    finishedEventHook = StopClientFromHandler;
    gint64 start = g_get_monotonic_time();

    RunScheduledTask(0);
    fixture->stopped = stopCalledFromHandler;

    assert_true(stopReturnedFromHandler);
    assert_true(g_get_monotonic_time() - start < WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    assert_int_equal(finishedEventCount, 1);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
}

// No completion event is sent for a device that no longer has a record (for example after a failed save).
static void test_noCompletionEventForDeviceWithoutRecord(void **state)
{
    (void) state;

    AddDevice("stale", CURRENT_CLASS_VERSION);
    deviceServiceReconfigureDevice("stale", 0, RemoveRecordOnCompletion, false);
    assert_int_equal(GetScheduledCount(), 1);

    RunScheduledTask(0);

    assert_int_equal(finishedEventCount, 0);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
}

// Shutdown waits for a running reconfiguration and sends no completion event for it.
static void test_shutdownWaitsForRunningReconfiguration(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;
    probeShutdownWait = true;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);
    gint64 start = g_get_monotonic_time();

    b_core_client_stop(fixture->client);
    fixture->stopped = true;

    gint64 returnedAt = g_get_monotonic_time();
    g_thread_join(taskThread);

    assert_false(shutdownContinuedBeforeTaskRan);
    assert_true(returnedAt >= taskStartedAt);
    assert_true(returnedAt - start < WAIT_TIMEOUT_SECS * G_TIME_SPAN_SECOND);
    assert_int_equal(finishedEventCount, 0);
}

// The startup failure fallback must not assume drivers implement synchronizeDevice().
static void test_failedStartupReconfigurationToleratesDriverWithoutSynchronize(void **state)
{
    (void) state;

    fakeDriver.synchronizeDevice = NULL;
    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();

    RunScheduledTask(0);

    assert_int_equal(finishedEventCount, 1);
    assert_false(finishedEventSucceeded);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_int_equal(GetSyncCount("stale"), 0);
}

// A failed startup reconfiguration's fallback synchronization still runs when the thread pool won't take it.
static void test_failedStartupReconfigurationSynchronizesWhenPoolRejectsTask(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    failThreadPoolAdd = true;

    RunScheduledTask(0);

    assert_int_equal(finishedEventCount, 1);
    assert_false(finishedEventSucceeded);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_int_equal(GetSyncCount("stale"), 1);
}

static void test_repeatedStartupPassSkipsPendingDevice(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    AddDevice("current", CURRENT_CLASS_VERSION);

    readyForDevices();

    // The earlier pass's reconfiguration has swapped in the rebuilt record but not finished yet, so the device is
    // still pending and must not be synchronized underneath it.
    classVersionHook = ReplaceStaleDeviceWithCurrent;
    readyForDevices();

    assert_int_equal(GetScheduledCount(), 1);
    assert_int_equal(GetSyncCount("current"), 2);
    assert_int_equal(GetSyncCount("stale"), 0);
}

// A device a repeated pass skipped because the first pass's reconfiguration of it was pending is synchronized once, not
// twice, when that reconfiguration fails: the startup fallback covers it.
static void test_repeatedStartupPassSkipsFailingStartupReconfigurationOnce(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    readyForDevices();
    assert_int_equal(GetScheduledCount(), 1);

    RunScheduledTask(0);

    assert_false(finishedEventSucceeded);
    assert_int_equal(GetSyncCount("stale"), 1);
}

// A repeated pass leaves alone a device whose record is current but that has a reconfiguration pending for another
// reason (here, as after a firmware upgrade): it is neither synchronized under that reconfiguration nor reconfigured
// again.
static void test_repeatedStartupPassSkipsDevicePendingForOtherReason(void **state)
{
    (void) state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    readyForDevices();
    assert_int_equal(GetSyncCount("current"), 1);

    deviceServiceReconfigureDevice("current", 60, NULL, false);
    assert_int_equal(GetScheduledCount(), 1);

    readyForDevices();

    assert_int_equal(GetScheduledCount(), 1);
    assert_int_equal(GetSyncCount("current"), 1);
}

// A device a startup pass skipped because a reconfiguration of it was pending for another reason is still synchronized
// if that reconfiguration fails.
static void test_skippedDeviceSynchronizedWhenPendingReconfigurationFails(void **state)
{
    (void) state;

    AddDevice("current", CURRENT_CLASS_VERSION);
    deviceServiceReconfigureDevice("current", 60, NULL, false);
    assert_int_equal(GetScheduledCount(), 1);

    readyForDevices();
    assert_int_equal(GetSyncCount("current"), 0);

    RunScheduledTask(0);

    assert_int_equal(finishedEventCount, 1);
    assert_false(finishedEventSucceeded);
    assert_int_equal(GetSyncCount("current"), 1);
}

static void test_repeatedStartupPassRechecksCurrentRecord(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    classVersionHook = ReplaceStaleDeviceWithCurrent;

    readyForDevices();

    assert_int_equal(GetScheduledCount(), 0);
    assert_int_equal(GetSyncCount("stale"), 1);
}

#ifdef OBSERVABILITY_BACKEND_RECORDS_GAUGES
static void test_pendingGaugeTracksPendingReconfigurations(void **state)
{
    (void) state;

    assert_int_equal(GetPendingGaugeValue(), 0);

    AddDevice("stale", STALE_CLASS_VERSION);
    AddDevice("stale2", STALE_CLASS_VERSION);
    readyForDevices();
    assert_int_equal(GetPendingGaugeValue(), 2);

    RunScheduledTask(0);
    assert_int_equal(GetPendingGaugeValue(), 1);

    // Which device's task was captured first follows database order, so remove whichever is still pending.
    const char *stillPending = deviceServiceIsReconfigurationPending("stale") ? "stale" : "stale2";
    assert_true(deviceServiceIsReconfigurationPending(stillPending));
    assert_true(deviceServiceRemoveDevice(stillPending));
    assert_int_equal(GetPendingGaugeValue(), 0);
}
#endif

// A zero-delay task can start before scheduling returns; it waits for its handle to be stored, then completes.
static void test_taskStartingBeforeSchedulingReturnsCompletes(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    startTaskWhenScheduled = true;

    readyForDevices();
    g_thread_join(startedTaskThread);
    startedTaskThread = NULL;

    assert_int_equal(GetScheduledCount(), 1);
    assert_int_equal(finishedEventCount, 1);
    assert_false(pendingWhenFinishedEventSent);
    assert_false(deviceServiceIsReconfigurationPending("stale"));
}

int main(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    syncCounts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_startupPassFlagsDevicesBeforePublishingReadiness, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_shutdownDuringStartupPassWithholdsReadiness, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_readinessWithdrawnOnceShutdownBegins, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_shutdownWaitsForStartupPassInProgress, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_stopFromReadyEventHandlerDoesNotWaitForItself, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_stopFromEventLoopCallbackReturns, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_stopAgainAfterStopFromEventLoop, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationFinishUnpendsBeforeNotifying, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_schedulingFailureLeavesDeviceNotPending, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_removalCancelsNotYetStartedReconfiguration, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_removalWaitsForRunningReconfiguration, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_skippedDeviceNotSynchronizedWhileBeingRemoved, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_synchronizationSkippedByFailedRemovalRunsAfterIt, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_removalDuringCompletionEventWaitsForIt, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_removalFromCompletionHandlerDoesNotWaitForItself, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_noCompletionEventForDeviceWithoutRecord, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationRequestedWhileFinishingIsScheduled, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationRequestedFromCompletionHandlerIsScheduled, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_stopFromCompletionHandlerDoesNotWaitForItself, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationRequestedDuringRemovalIsRefused, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationRefusedByFailedRemovalIsRequestedAgain, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationsRefusedByFailedRemovalAreMerged, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationRequestedWhileRemovalCancelsIsNotLost, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationRequestedDuringSwapWhileRemovingIsNotLost, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationOfBrieflyMissingRecordIsScheduled, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_reconfigurationWithoutUuidIsRefused, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationOfDeviceDeletedDuringRequestIsBackedOut, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_shutdownWaitsForCompletionEventInProgress, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_reconfigurationOfDeviceWithoutRecordIsRefused, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_shutdownWaitsForRunningReconfiguration, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_repeatedStartupPassSkipsPendingDevice, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_repeatedStartupPassRechecksCurrentRecord, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_repeatedStartupPassSkipsDevicePendingForOtherReason, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_skippedDeviceSynchronizedWhenPendingReconfigurationFails, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_repeatedStartupPassSkipsFailingStartupReconfigurationOnce, SetupService, TeardownService),
#ifdef OBSERVABILITY_BACKEND_RECORDS_GAUGES
        cmocka_unit_test_setup_teardown(test_pendingGaugeTracksPendingReconfigurations, SetupService, TeardownService),
#endif
        cmocka_unit_test_setup_teardown(
            test_taskStartingBeforeSchedulingReturnsCompletes, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_failedStartupReconfigurationToleratesDriverWithoutSynchronize, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_failedStartupReconfigurationSynchronizesWhenPoolRejectsTask, SetupService, TeardownService),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);

    g_clear_pointer(&finishedEventUuid, g_free);
    g_hash_table_unref(syncCounts);

    return result;
}
