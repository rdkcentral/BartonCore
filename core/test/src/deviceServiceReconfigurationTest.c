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

typedef struct
{
    taskCallbackFunc func;
    void *arg;
    uint32_t handle;
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

static GHashTable *syncCounts; // uuid -> number of synchronizeDevice() calls

static void (*classVersionHook)(void);

static bool blockShutdownUntilPassDone;
static bool shutdownBegan;
static bool passDone;
static bool passReachedDevice;

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

    while (blockShutdownUntilPassDone == true && passDone == false)
    {
        g_cond_wait(&fakeCond, &fakeLock);
    }

    g_mutex_unlock(&fakeLock);
}

void __wrap_deviceServiceCommFailInit(void) {}

void __wrap_deviceServiceCommFailShutdown(void) {}

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
    (void) delayAmount;
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
        scheduledTasks[scheduledCount++] = (ScheduledTask) {.func = func, .arg = arg, .handle = handle};

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
    g_mutex_unlock(&fakeLock);

    return handle;
}

// Run inline, so every synchronization a startup pass queues has finished when the pass returns.
bool __wrap_threadPoolAddTask(icThreadPool *pool, threadPoolTask task, void *arg, threadPoolTaskArgFreeFunc argFreeFunc)
{
    (void) pool;

    task(arg);

    if (argFreeFunc != NULL)
    {
        argFreeFunc(arg);
    }
    else
    {
        free(arg);
    }

    return true;
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

    g_mutex_lock(&fakeLock);
    readyEventCount++;
    readyWhenReadyEventSent = deviceServiceIsReadyForDeviceOperation();
    pendingWhenReadyEventSent = watchedUuid != NULL && deviceServiceIsReconfigurationPending(watchedUuid);
    g_mutex_unlock(&fakeLock);
}

void __wrap_sendDeviceReconfigurationFinishedEvent(const char *deviceClass, const char *uuid, bool succeeded)
{
    (void) deviceClass;

    g_mutex_lock(&fakeLock);
    finishedEventCount++;
    finishedEventSucceeded = succeeded;
    pendingWhenFinishedEventSent = deviceServiceIsReconfigurationPending(uuid);
    g_free(finishedEventUuid);
    finishedEventUuid = g_strdup(uuid);
    g_mutex_unlock(&fakeLock);
}

void __wrap_sendDeviceRemovedEvent(const char *uuid, const char *deviceClass)
{
    (void) uuid;
    (void) deviceClass;
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

static int64_t GetPendingGaugeValue(void)
{
    g_mutex_lock(&fakeLock);
    int64_t value = lastPendingGaugeValue;
    g_mutex_unlock(&fakeLock);

    return value;
}

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
// that canceller is known to be waiting for this task rather than racing it.
static gpointer RunFirstScheduledTaskOnceCancelAttempted(gpointer data)
{
    (void) data;

    g_mutex_lock(&fakeLock);

    while (cancelCount == 0)
    {
        g_cond_wait(&fakeCond, &fakeLock);
    }

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
    g_hash_table_remove_all(syncCounts);
    classVersionHook = NULL;
    blockShutdownUntilPassDone = false;
    shutdownBegan = false;
    passDone = false;
    passReachedDevice = false;
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

// Shutdown beginning mid-pass stops scheduling and withholds readiness.
static void BlockUntilShutdownBegins(void)
{
    g_mutex_lock(&fakeLock);
    passReachedDevice = true;
    g_cond_broadcast(&fakeCond);

    while (shutdownBegan == false)
    {
        g_cond_wait(&fakeCond, &fakeLock);
    }

    g_mutex_unlock(&fakeLock);
}

static void test_shutdownDuringStartupPassWithholdsReadiness(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    classVersionHook = BlockUntilShutdownBegins;
    blockShutdownUntilPassDone = true;

    GThread *passThread = g_thread_new("readyPass", RunReadyCallback, NULL);

    g_mutex_lock(&fakeLock);

    while (passReachedDevice == false)
    {
        g_cond_wait(&fakeCond, &fakeLock);
    }

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
    passDone = true;
    g_cond_broadcast(&fakeCond);
    g_mutex_unlock(&fakeLock);

    g_thread_join(stopThread);
    fixture->stopped = true;

    assert_false(ready);
    assert_false(pending);
    assert_int_equal(scheduled, 0);
    assert_int_equal(readyEvents, 0);
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
// promptly rather than at its 60 second timeout.
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
    assert_false(deviceServiceIsReconfigurationPending("stale"));
    assert_null(jsonDatabaseGetDeviceById("stale"));
}

// Shutdown waits for a running reconfiguration (blocking, not polling) and sends no completion event for it.
static void test_shutdownWaitsForRunningReconfiguration(void **state)
{
    Fixture *fixture = *state;

    AddDevice("stale", STALE_CLASS_VERSION);
    readyForDevices();
    cancelSucceeds = false;

    GThread *taskThread = g_thread_new("task", RunFirstScheduledTaskOnceCancelAttempted, NULL);
    gint64 start = g_get_monotonic_time();

    b_core_client_stop(fixture->client);
    fixture->stopped = true;

    gint64 returnedAt = g_get_monotonic_time();
    g_thread_join(taskThread);

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

static void test_repeatedStartupPassRechecksCurrentRecord(void **state)
{
    (void) state;

    AddDevice("stale", STALE_CLASS_VERSION);
    classVersionHook = ReplaceStaleDeviceWithCurrent;

    readyForDevices();

    assert_int_equal(GetScheduledCount(), 0);
    assert_int_equal(GetSyncCount("stale"), 1);
}

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
        cmocka_unit_test_setup_teardown(
            test_reconfigurationFinishUnpendsBeforeNotifying, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_schedulingFailureLeavesDeviceNotPending, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_removalCancelsNotYetStartedReconfiguration, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_removalWaitsForRunningReconfiguration, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_shutdownWaitsForRunningReconfiguration, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_repeatedStartupPassSkipsPendingDevice, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_repeatedStartupPassRechecksCurrentRecord, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(test_pendingGaugeTracksPendingReconfigurations, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_taskStartingBeforeSchedulingReturnsCompletes, SetupService, TeardownService),
        cmocka_unit_test_setup_teardown(
            test_failedStartupReconfigurationToleratesDriverWithoutSynchronize, SetupService, TeardownService),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);

    g_clear_pointer(&finishedEventUuid, g_free);
    g_hash_table_unref(syncCounts);

    return result;
}
