//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2018 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

#ifdef BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY

/*
 * This code is responsible for creating Zigbee telemetry files which contain data collected by a script that
 * typically contains Zigbee network captures for offline diagnostics.
 *
 * Once started (through server properties), it will start then stop the telemetry collection script every
 * CAPTURE_INTERVAL_MINS minutes, then start it up again.  When the script stops collection, it wraps up the collected
 * data into a telemetry file (e.g., "20190918141516.telemetry") in the storage directory used by the script.
 *
 * If configured to upload these files, they will be moved to a directory that is monitored by diag service which
 * will be responsible for uploading to the server and cleaning up.  If upload is not enabled, then the capture
 * files stay in the storage directory.
 *
 * If the storage space used by this system exceeds maxAllowedFileStorageMb, oldest telemetry files will be removed
 * until the limit is no longer exceeded.
 *
 * The server property "telemetry.hoursRemaining" turns the feature on or off.  When set to -1 it means start
 * capturing and never stop.  0 means turn the whole thing off.  Any positive number means "capture for this
 * many hours then stop".  The date that the capture started is stored in a device service system property.  This
 * property is only ever cleared if the "telemetry.hoursRemaining" property changes.  So each startup we will
 * examine "telemetry.hoursRemaining" and if it is greater than 0 we will compare with our start time property
 * to see if we need to continue to capture, or if we have already captured enough.
 *
 *  Created by tlea on 9/11/19.
 */

#include "zigbeeTelemetry.h"
#include <deviceService.h>
#include <dirent.h>
#include <errno.h>
#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/threadUtils.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icTypes/icSortedLinkedList.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/paths.h>
#include <propsMgr/propsHelper.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOG_TAG                     "zigbeeTelemetry"

#define MONITOR_EXEC_DELAY_MINS     1
#define CAPTURE_INTERVAL_MINS       15

#define MIN_FILE_STORAGE_MB         1
#define MAX_FILE_STORAGE_MB         10

#define TELEMETRY_SCRIPT_FILENAME   "zigbeeTelemetry.sh"
#define DATE_TELEMETRY_STARTED_PROP "zigbeeTelemetryStartDate"
#define TELEMETRY_FILE_EXTENSION    ".telemetry"

static pthread_mutex_t settingsMtx = PTHREAD_MUTEX_INITIALIZER;
static int32_t hoursRemaining = 0;
static uint32_t maxAllowedFileStorageMb = MIN_FILE_STORAGE_MB;
static bool allowUpload = false;
static uint64_t captureIntervalStartTimeMillis = 0;
static char *storageDir = NULL;

static pthread_mutex_t monitorMtx = PTHREAD_MUTEX_INITIALIZER;
static uint32_t monitorHandle = 0; // 0 is invalid

static void processProperties(void);
static int invokeCaptureScript(const char *args, char *output, uint32_t outputLen);
static bool startCapture(void);
static bool stopCapture(void);
static bool isCaptureRunning(void);
static void startMonitor(void);
static void stopMonitor(void);
static bool shouldCaptureBeRunning(void);
static bool captureIntervalExpired(void);
static void scrubTelemetryStorageDir(void);
static bool moveCompletedCapturesForUpload(void);

void zigbeeTelemetryInitialize(void)
{
    pthread_mutex_lock(&settingsMtx);

    hoursRemaining = getPropertyAsInt32(TELEMETRY_HOURS_REMAINING, 0);
    allowUpload = getPropertyAsBool(TELEMETRY_ALLOW_UPLOAD, false);
    maxAllowedFileStorageMb = getPropertyAsUInt32(TELEMETRY_MAX_ALLOWED_FILE_STORAGE, MIN_FILE_STORAGE_MB);

    if (maxAllowedFileStorageMb > MAX_FILE_STORAGE_MB)
    {
        icLogWarn(LOG_TAG,
                  "%s: %s exceeds maximum of %d. Using %d instead",
                  __FUNCTION__,
                  TELEMETRY_MAX_ALLOWED_FILE_STORAGE,
                  MAX_FILE_STORAGE_MB,
                  MAX_FILE_STORAGE_MB);

        maxAllowedFileStorageMb = MAX_FILE_STORAGE_MB;
    }

    char tmp[PATH_MAX];
    if (invokeCaptureScript("getStorageDir", tmp, PATH_MAX) != 0)
    {
        icLogError(LOG_TAG, "%s: unable to determine storage dir, telemetry not available.", __FUNCTION__);
    }

    // trim any whitespace
    storageDir = trimString(tmp);

    pthread_mutex_unlock(&settingsMtx);

    // if the script is running at this point, it should not be.  Stop it.
    if (isCaptureRunning() == true)
    {
        icLogWarn(LOG_TAG, "%s: capture running at startup, so we stop it.", __FUNCTION__);
        stopCapture();
    }

    processProperties();
}

void zigbeeTelemetryShutdown(void)
{
    stopCapture();

    pthread_mutex_lock(&settingsMtx);
    free(storageDir);
    storageDir = NULL;
    pthread_mutex_unlock(&settingsMtx);
}

void zigbeeTelemetrySetProperty(const char *key, const char *value)
{
    bool somethingChanged = false;

    if (key == NULL || value == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    pthread_mutex_lock(&settingsMtx);

    if (stringCompare(key, TELEMETRY_HOURS_REMAINING, true) == 0)
    {
        int32_t newVal = 0;
        if (stringToInt32(value, &newVal) == true && hoursRemaining != newVal)
        {
            hoursRemaining = newVal;
            somethingChanged = true;

            // clear our date started property since we have been given a new capture hours (or its been disabled)
            deviceServiceSetSystemProperty(DATE_TELEMETRY_STARTED_PROP, "");
        }
    }
    else if (stringCompare(key, TELEMETRY_ALLOW_UPLOAD, true) == 0)
    {
        bool newVal = stringToBool(value);

        if (allowUpload != newVal)
        {
            allowUpload = newVal;
            somethingChanged = true;
        }
    }
    else if (stringCompare(key, TELEMETRY_MAX_ALLOWED_FILE_STORAGE, true) == 0)
    {
        uint32_t newVal = 0;
        if (stringToUint32(value, &newVal) == true && maxAllowedFileStorageMb != newVal)
        {
            if (newVal > MAX_FILE_STORAGE_MB)
            {
                icLogWarn(LOG_TAG,
                          "%s: %s exceeds maximum of %d.  Ignoring and continuing to use %d",
                          __FUNCTION__,
                          TELEMETRY_MAX_ALLOWED_FILE_STORAGE,
                          MAX_FILE_STORAGE_MB,
                          maxAllowedFileStorageMb);
            }
            else
            {
                maxAllowedFileStorageMb = newVal;
                somethingChanged = true;
            }
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: unexpected telemetry property (%s) changed to %s", __FUNCTION__, key, value);
    }

    pthread_mutex_unlock(&settingsMtx);

    if (somethingChanged == true)
    {
        processProperties();
    }
}

/**
 * Examine our locally cached properties to see if we need to start/stop a capture or act on other changes to settings
 */
static void processProperties(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (storageDir == NULL)
    {
        icLogWarn(LOG_TAG, "%s: storageDir not set, telemetry disabled.", __FUNCTION__);
        return;
    }

    // Get the current capabilities (1 means we can capture) each time, since someone could have plugged USB stick in
    int capabilities = invokeCaptureScript("getCapabilities", NULL, 0);

    if (shouldCaptureBeRunning() == true && capabilities == 1)
    {
        // Set cpe property, in order to inform server of our capabilities
        setPropertyUInt32(TELEMETRY_CAPABILITIES, capabilities, true, PROPERTY_SRC_DEVICE);
        if (isCaptureRunning() == false)
        {
            startCapture();
        }
        else
        {
            icLogInfo(LOG_TAG, "%s: capture already running", __FUNCTION__);
        }
    }
    else if (shouldCaptureBeRunning() == true && capabilities == 0)
    {
        icLogWarn(LOG_TAG, "%s: capture is configured to run, but we are not capable", __FUNCTION__);
    }
    else
    {
        // we are not supposed to be running so make sure we are stopped
        stopCapture();
    }
}

static int invokeCaptureScript(const char *args, char *output, uint32_t outputLen)
{
    AUTO_CLEAN(free_generic__auto) char *homeDir = getStaticPath();
    AUTO_CLEAN(free_generic__auto)
    char *command = stringBuilder("%s/bin/%s %s", homeDir, TELEMETRY_SCRIPT_FILENAME, args);
    icLogDebug(LOG_TAG, "%s: executing'%s'", __FUNCTION__, command);

    // a bit odd, but if we dont want to capture the scripts output we set the pipe up to write to it instead of
    // reading from it.  If we set it up to read and dont capture the output we will get SIGPIPE
    FILE *fp = popen(command, output == NULL ? "w" : "r");
    if (fp == NULL)
    {
        icLogError(LOG_TAG, "%s: failed to run command (errno=%d)", __FUNCTION__, errno);
        return -1;
    }

    if (output != NULL)
    {
        if (fgets(output, outputLen, fp) == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to capture script output", __FUNCTION__);
        }
    }

    // the result code from the script
    int rc = pclose(fp);

    if (WIFEXITED(rc))
    {
        return WEXITSTATUS(rc);
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to get exit code from script", __FUNCTION__);
        return -1;
    }
}

/**
 * Start capture
 *
 * @return true if the capture started or was already running
 */
static bool startCapture(void)
{
    int rc = invokeCaptureScript("start", NULL, 0);

    switch (rc)
    {
        case 0: // started
        {
            // record the capture interval start time
            pthread_mutex_lock(&settingsMtx);
            captureIntervalStartTimeMillis = getCurrentUnixTimeMillis();
            pthread_mutex_unlock(&settingsMtx);

            // if we dont have a value for when telemetry started, record 'now' since we are starting for the
            // first time.
            char *origStartDate = NULL;
            if (deviceServiceGetSystemProperty(DATE_TELEMETRY_STARTED_PROP, &origStartDate) == false ||
                origStartDate == NULL || strlen(origStartDate) == 0)
            {
                icLogInfo(LOG_TAG, "%s: starting new capture run", __FUNCTION__);
                uint64_t startTime = getCurrentUnixTimeMillis();
                char *startTimeStr = stringBuilder("%" PRIu64, startTime);
                deviceServiceSetSystemProperty(DATE_TELEMETRY_STARTED_PROP, startTimeStr);
                free(startTimeStr);
            }
            else
            {
                icLogInfo(LOG_TAG, "%s: continuing previously started capture run", __FUNCTION__);
            }

            free(origStartDate);

            startMonitor();
            return true;
        }

        case 1: // already running
        {
            // make sure the monitor task is running
            startMonitor();
            return true;
        }

        case 2:  // not capable
        case 3:  // failed
        case -1: // script error
        default:
            return false;
    }
}

/**
 * stop a capture
 *
 * @return true if it was running and stopped or wasnt running previously.  False if it failed to stop a running capture
 */
static bool stopCapture(void)
{
    bool result = false;

    int rc = invokeCaptureScript("stop", NULL, 0);

    switch (rc)
    {
        case 0: // stopped or wasnt running previously
        {
            stopMonitor();
            result = true;
            break;
        }

        case 1:  // failed to stop (its still running! keep the monitor going)
        case -1: // script error
        default:
        {
            icLogError(LOG_TAG, "%s: failed to stop capture (rc=%d)", __FUNCTION__, rc);
            result = false;
            break;
        }
    }

    // regardless of whether or not we successfully stopped, we need to clean up enough files (if not uploading)
    //  if we are exceeding our storage limits.
    scrubTelemetryStorageDir();

    return result;
}

/**
 * Check if a capture is running
 *
 * @return true if its running
 */
static bool isCaptureRunning(void)
{
    int rc = invokeCaptureScript("status", NULL, 0);

    switch (rc)
    {
        case 0: // not running
            return false;

        case 1: // running
            return true;

        case -1: // script error
        default:
            icLogError(
                LOG_TAG, "%s: failed to determine if capture is running (rc=%d), assuming false", __FUNCTION__, rc);
            return false;
    }
}

static void *stopMonitorBackground(void *arg)
{
    stopMonitor();
    return NULL;
}

/*
 * This runs every MONITOR_EXEC_DELAY_MINS and is responsible for:
 *
 * - Stopping the running capture if the CAPTURE_INTERVAL_MINS has been reached or if the hoursRemaining for the
 *   capture period has been exceeded.
 *
 * If allowUpload is true:
 * - Move all completed capture files to directory monitored by diag service (they become its responsibility)
 * Else (not uploading):
 * - Calculate total storage space being used
 * - Remove oldest completed captures until storage space used is under maxAllowedStorageMb
 *
 * Finally, if we should continue capturing (we have been running less than hoursRemaining, or its -1 for 'never stop')
 * start capturing again.  Don't start capture if the system clock has not yet been set (we can check again next
 * iteration in this case).
 */
static void monitorFunc(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool stopCap = false;
    bool shouldBeRunning = shouldCaptureBeRunning();
    bool shouldStopMonitor = false;

    // get local copies of our settings for this iteration
    pthread_mutex_lock(&settingsMtx);
    bool localAllowUpload = allowUpload;
    pthread_mutex_unlock(&settingsMtx);

    // is it time to stop the capture (which creates the final telemetry file for upload)?
    if (shouldBeRunning == false)
    {
        // we should no longer be capturing at all (we reached the number of hours we were supposed to capture
        // or the capability has been turned off.  Stop the capture.
        icLogInfo(LOG_TAG, "%s: we should no longer be capturing.  Stopping.", __FUNCTION__);

        shouldStopMonitor = true;
        stopCap = true;
    }
    else if (captureIntervalExpired() == true)
    {
        // we have finished capturing for this interval.  Stop capture and it will restart if required below.
        icLogInfo(LOG_TAG, "%s: capture interval reached.  Stopping capture", __FUNCTION__);
        stopCap = true;
    }

    // if we stopped capture, do we need to start it up again if there time still remaining?
    if (stopCap == true)
    {
        if (invokeCaptureScript("stop", NULL, 0) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to stop capture script!", __FUNCTION__);
        }
        else if (shouldBeRunning == true)
        {
            int rc = invokeCaptureScript("start", NULL, 0);
            if (rc != 0 && rc != 1)
            {
                icLogError(LOG_TAG, "%s: failed to restart capture", __FUNCTION__);
                shouldStopMonitor = true;
            }
            else
            {
                // since we started up again, reset our interval start time
                pthread_mutex_lock(&settingsMtx);
                captureIntervalStartTimeMillis = getCurrentUnixTimeMillis();
                pthread_mutex_unlock(&settingsMtx);
            }
        }

        // we stopped a capture. move any final files into the target directory if we are uploading
        if (localAllowUpload == true)
        {
            if (moveCompletedCapturesForUpload() == false) // fatal error
            {
                shouldStopMonitor = true;
            }
        }
    }

    if (shouldStopMonitor == true)
    {
        // stop the monitor in the background since we are in the repeating task context
        createDetachedThread(stopMonitorBackground, NULL, "stopMon");
    }

    // finally, scrub our storage directory to be sure we arent exceeding our usage
    scrubTelemetryStorageDir();
}

static void startMonitor(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&monitorMtx);

    if (monitorHandle == 0)
    {
        monitorHandle = createRepeatingTask(MONITOR_EXEC_DELAY_MINS, DELAY_MINS, monitorFunc, NULL);
    }

    pthread_mutex_unlock(&monitorMtx);
}

static void stopMonitor(void)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&monitorMtx);

    if (monitorHandle != 0)
    {
        cancelRepeatingTask(monitorHandle);
        monitorHandle = 0;
    }

    pthread_mutex_unlock(&monitorMtx);
}

static bool shouldCaptureBeRunning(void)
{
    bool result = false;

    pthread_mutex_lock(&settingsMtx);
    int32_t localHoursRemaining = hoursRemaining;
    pthread_mutex_unlock(&settingsMtx);

    if (localHoursRemaining == -1) // always run
    {
        result = true;
    }
    else if (localHoursRemaining > 0)
    {
        // we were configured to run for some number of hours.  Check if that time is up.
        char *origStartDateStr = NULL;
        if (deviceServiceGetSystemProperty(DATE_TELEMETRY_STARTED_PROP, &origStartDateStr) == true &&
            origStartDateStr != NULL && strlen(origStartDateStr) > 0)
        {
            uint64_t origStartDate = 0;
            if (stringToUint64(origStartDateStr, &origStartDate) == true &&
                origStartDate + (localHoursRemaining * 60 * 60 * 1000) > getCurrentUnixTimeMillis())
            {
                result = true;
            }
        }
        else
        {
            // there was no date started prop set, but we are configured to run
            result = true;
        }
        free(origStartDateStr);
    }

    return result;
}

static bool captureIntervalExpired(void)
{
    bool result = true;

    pthread_mutex_lock(&settingsMtx);

    uint64_t now = getCurrentUnixTimeMillis();
    if (now - captureIntervalStartTimeMillis < CAPTURE_INTERVAL_MINS * 60 * 1000)
    {
        result = false;
    }

    pthread_mutex_unlock(&settingsMtx);

    return result;
}

/**
 * Iterate over our telemetry storage dir and remove the oldest files until we are under our storage limit
 */
static void scrubTelemetryStorageDir(void)
{
    if (storageDir == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid storage directory configured.", __FUNCTION__);
        return;
    }

    // if we are allowed to upload then we will be moving files to upload directory. Hence monitor
    // that directory in that case and if we are not allowed to upload then monitor local staging directory.
    pthread_mutex_lock(&settingsMtx);
    AUTO_CLEAN(free_generic__auto)
    char *storageDirToBeChecked =
        (allowUpload == true) ? strdup(CONFIG_DEBUG_TELEMETRY_UPLOAD_DIRECTORY) : strdup(storageDir);
    uint32_t localMaxAllowedFileStorageMb = maxAllowedFileStorageMb;
    pthread_mutex_unlock(&settingsMtx);

    if (fileUtilsDeleteOld(storageDirToBeChecked, TELEMETRY_FILE_EXTENSION, localMaxAllowedFileStorageMb, 0) == false)
    {
        // a failure to clean up will stop any running capture for safety
        icLogError(LOG_TAG, "%s: stopping any running capture due to fatal error cleaning up storage", __FUNCTION__);
        stopCapture();
    }
}

static bool moveCompletedCapturesForUpload(void)
{
    struct dirent *entry;
    DIR *d = opendir(storageDir);

    if (d == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to open storage dir '%s'", __FUNCTION__, storageDir);
        return false;
    }

    // Create CONFIG_DEBUG_TELEMETRY_UPLOAD_DIRECTORY if it don't exist
    if (mkdir_p(CONFIG_DEBUG_TELEMETRY_UPLOAD_DIRECTORY, 0777) != 0)
    {
        icLogError(LOG_TAG,
                   "%s: cannot create directory for upload (%s)!  errno=%d",
                   __FUNCTION__,
                   CONFIG_DEBUG_TELEMETRY_UPLOAD_DIRECTORY,
                   errno);
        closedir(d);
        return false;
    }

    while ((entry = readdir(d)) != NULL)
    {
        // if its a regular file and has TELEMETRY_FILE_EXTENSION in its name, move it to the upload directory
        if (entry->d_type == DT_REG && strstr(entry->d_name, TELEMETRY_FILE_EXTENSION) != NULL)
        {
            AUTO_CLEAN(free_generic__auto) char *origPath = stringBuilder("%s/%s", storageDir, entry->d_name);
            AUTO_CLEAN(free_generic__auto)
            char *destPath = stringBuilder("%s/%s", CONFIG_DEBUG_TELEMETRY_UPLOAD_DIRECTORY, entry->d_name);

            if (moveFile(origPath, destPath) == false)
            {
                icLogError(LOG_TAG, "%s: unable to move %s to %s!", __FUNCTION__, origPath, destPath);
                closedir(d);
                return false;
            }
        }
    }
    closedir(d);

    return true;
}

#endif // BARTON_CONFIG_SUPPORT_ZIGBEE_TELEMETRY