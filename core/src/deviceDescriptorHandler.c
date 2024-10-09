//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
/*
 * Created by Thomas Lea on 10/23/15.
 *
 * This code is responsible for ensuring that we have successfully downloaded the latest allow and denylists.
 * It will spawn a repeating task to download each if required.  These tasks will continue to run until we have
 * success.  The interval between each download attempt will increase until we reach our maximum interval.
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "deviceDescriptorHandler.h"
#include "deviceServiceConfiguration.h"
#include "deviceServicePrivate.h"
#include "glib.h"
#include "provider/device-service-property-provider.h"
#include <deviceDescriptors.h>
#include <devicePrivateProperties.h>
#include <deviceService.h>
#include <icConcurrent/repeatingTask.h>
#include <icCrypto/digest.h>
#include <icLog/logging.h>
#include <icLog/telemetryMarkers.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <urlHelper/urlHelper.h>

#define LOG_TAG                           "deviceDescriptorHandler"
#define DEFAULT_INVALID_DENYLIST_URL      "http://toBeReplaced"

// a basic URL length check.  Shortest I could fathom is "file:///a"
#define MIN_URL_LENGTH                    9

#define DOWNLOAD_TIMEOUT_SECS             60

#define INIT_DD_TASK_WAIT_TIME_SECONDS    15
#define INTERVAL_DD_TASK_TIME_SECONDS     15
#define MAX_DD_TASK_WAIT_TIME_SECONDS     120

#define BASE_DD_EXPONENTIAL_DELAY_SECONDS 2
#define MAX_DD_EXPONENTIAL_DELAY_SECONDS  (60 * 60 * 24)

static pthread_mutex_t deviceDescriptorMutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t allowlistTaskId = 0;
static char *currentAllowlistUrl;
static bool updateAllowlistTaskFunc(void *taskArg);
static void destroyAllowlistTaskObjectsFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy);
static void destroyDenylistTaskObjectsFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy);

static void cancelAllowlistUpdate();

static uint32_t denylistTaskId = 0;
static char *currentDenylistUrl;
static bool updateDenylistTaskFunc(void *taskArg);
static void cancelDenylistUpdate();

static bool fileNeedsUpdating(const char *currentUrlSystemKey,
                              const char *newUrl,
                              const char *currentFilePath,
                              const char *currentFileMd5SystemKey);
typedef bool (*deviceDescriptorFileValidator)(const char *path);
static bool downloadFile(const char *url, const char *destFile, deviceDescriptorFileValidator fileValidator);
static bool localAllowlistIsValid(void);
static bool localDenylistIsValid(void);
static bool checkAndSetReadyForPairing(void);

// FIXME: Copy Paste Tech Debt (duplicated in zigbeeDriverCommon)
static sslVerify convertVerifyPropValToModeBarton(const char *strVal);
static const char *sslVerifyPropKeyForCategoryBarton(sslVerifyCategory cat);

static deviceDescriptorsReadyForPairingFunc readyForPairingCB;
static deviceDescriptorsUpdatedFunc descriptorsUpdatedCB;

// indicates if allowlist/denylist are valid
//
static bool isDenylistValid = false;
static bool isAllowlistValid = false;

/*
 * Standard initialization performed at system startup.  This will retrieve the last known allow and denylist
 * URLs from prop service and ensure that we have them downloaded successfully.
 */
void deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForPairingFunc readyForPairingCallback,
                                        deviceDescriptorsUpdatedFunc descriptorsUpdatedCallback)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *allowlistUrl = NULL;
    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();

    if (b_device_service_property_provider_has_property(propertyProvider, DEVICE_DESC_ALLOWLIST_URL_OVERRIDE))
    {
        allowlistUrl = b_device_service_property_provider_get_property_as_string(
            propertyProvider, DEVICE_DESC_ALLOWLIST_URL_OVERRIDE, NULL);
    }
    else if (b_device_service_property_provider_has_property(propertyProvider, DEVICE_DESCRIPTOR_LIST))
    {
        allowlistUrl =
            b_device_service_property_provider_get_property_as_string(propertyProvider, DEVICE_DESCRIPTOR_LIST, NULL);
    }

    char *denylistUrl =
        b_device_service_property_provider_get_property_as_string(propertyProvider, DEVICE_DESC_DENYLIST, NULL);

    // device service can be informed for pairing is possible or not based on
    // valid local allowlist & denyList. If url is valid, url will be used to check
    // if update of allowlist/denylist is needed or not.
    //
    bool allowListValid = localAllowlistIsValid();
    bool denyListValid = localDenylistIsValid();
    // Denylist is optional.  Handle empty/missing as well as the special testing/default URL as valid
    if (stringIsEmpty(denylistUrl) == true || strcasecmp(denylistUrl, DEFAULT_INVALID_DENYLIST_URL) == 0)
    {
        denyListValid = true;
    }

    pthread_mutex_lock(&deviceDescriptorMutex);

    isAllowlistValid = allowListValid;
    isDenylistValid = denyListValid;

    readyForPairingCB = readyForPairingCallback;
    descriptorsUpdatedCB = descriptorsUpdatedCallback;

    if (checkAndSetReadyForPairing() == false)
    {
        icLogWarn(LOG_TAG, "%s: No valid local device descriptors found", __FUNCTION__);
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    if (stringIsEmpty(allowlistUrl) == true)
    {
        icLogWarn(LOG_TAG,
                  "%s: At least one of the %s or %s property should be set",
                  __FUNCTION__,
                  DEVICE_DESC_ALLOWLIST_URL_OVERRIDE,
                  DEVICE_DESCRIPTOR_LIST);
    }
    else
    {
        deviceDescriptorsUpdateAllowlist(allowlistUrl);
        free(allowlistUrl);
    }

    // If the denylist url property gets deleted, we still need to process a NULL/empty value.
    deviceDescriptorsUpdateDenylist(denylistUrl);
    free(denylistUrl);
}

void deviceServiceDeviceDescriptorsDestroy()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // Stop any scheduled updates
    cancelAllowlistUpdate();
    cancelDenylistUpdate();

    pthread_mutex_lock(&deviceDescriptorMutex);

    readyForPairingCB = NULL;
    descriptorsUpdatedCB = NULL;

    pthread_mutex_unlock(&deviceDescriptorMutex);
}

/*
 * deviceDescriptorMutex needs to be locked prior to calling this function to check
 * if we are ready to pair and call readyForPairingCB.
 */
static bool checkAndSetReadyForPairing(void)
{
    bool descriptorsReady = isAllowlistValid && isDenylistValid;
    if (descriptorsReady)
    {
        if (readyForPairingCB != NULL)
        {
            readyForPairingCB();
            // ready for pairing should be set once per startup. Once it is set,
            // we should/cannot move back to "not ready for pairing state".
            //
            readyForPairingCB = NULL;
        }
    }

    return descriptorsReady;
}

static bool localAllowlistIsValid(void)
{
    bool isValid = false;

    scoped_generic char *allowListPath = getAllowListPath();
    isValid = checkAllowListValid(allowListPath);

    return isValid;
}

static bool localDenylistIsValid(void)
{
    bool isValid = false;

    scoped_generic char *denyListPath = getDenyListPath();
    isValid = checkDenyListValid(denyListPath);

    return isValid;
}

void deviceDescriptorsUpdateAllowlist(const char *url)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, url);

    // cancel any task that is already running
    cancelAllowlistUpdate();

    pthread_mutex_lock(&deviceDescriptorMutex);

    // Only start the task if we have a allowlist url
    if (url != NULL && strlen(url) >= MIN_URL_LENGTH)
    {
        // Save off the URL. This is exclusively so we can call urlHelperCancel if a curl call from
        // updateAllowlistTaskFunc is running when we need to cancel/shutdown.
        free(currentAllowlistUrl);
        currentAllowlistUrl = strdup(url);

        scoped_generic char *pendingAllowlistDownloadUrl = strdup(url);

        // When updating the allowList, a repeating task policy will be utilized in order to ensure the update
        // compeletes eventually. However, the rate at which the task repeats must vary dependent opon the device
        // and circumstance.
        // - Touchscreens: If the Touchscreen is currently 'in activation', a more aggressive policy is necessary
        //                 to ensure activation completes as soon as possible. Otherwise, a less aggressive
        //                 policy will be implemented.
        // - Gateways: A less aggressive policy will be implemented to reduce server load since %99.99 of
        //             Gateways are considered 'unactivated' as far as XHF is concerned.
        bool useAggressivePolicy = false;

#ifdef BARTON_CONFIG_SETUP_WIZARD
        g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
        int32_t activationState = b_device_service_property_provider_get_property_as_int32(
            propertyProvider, PERSIST_CPE_SETUPWIZARD_STATE, ACTIVATION_NOT_STARTED);
        if (activationState < ACTIVATION_COMPLETE)
        {
            useAggressivePolicy = true;
        }
#endif

        RepeatingTaskPolicy *policy = NULL;

        if (useAggressivePolicy == true)
        {
            policy = createIncrementalRepeatingTaskPolicy(INIT_DD_TASK_WAIT_TIME_SECONDS,
                                                          MAX_DD_TASK_WAIT_TIME_SECONDS,
                                                          INTERVAL_DD_TASK_TIME_SECONDS,
                                                          DELAY_SECS);
        }
        else
        {
            policy = createExponentialRepeatingTaskPolicy(
                BASE_DD_EXPONENTIAL_DELAY_SECONDS, MAX_DD_EXPONENTIAL_DELAY_SECONDS, DELAY_SECS);
        }

        // Kick it off in the background, with increasing backoff until it eventually completes
        // since we will pass 'currentAllowlistUrl' to the task, create custom cleanup function
        // so that we don't destroy that unless holding the lock
        allowlistTaskId = createPolicyRepeatingTask(updateAllowlistTaskFunc,
                                                    destroyAllowlistTaskObjectsFunc,
                                                    policy,
                                                    g_steal_pointer(&pendingAllowlistDownloadUrl));
    }

    pthread_mutex_unlock(&deviceDescriptorMutex);
}

static void cancelAllowlistUpdate()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // Avoid holding the lock while we cancel as that can result in deadlock
    pthread_mutex_lock(&deviceDescriptorMutex);
    uint32_t localAllowlistTaskId = allowlistTaskId;
    if (localAllowlistTaskId != 0)
    {
        // Cancel the URL request first, or this thread may block
        // for many seconds waiting for a running download to finish.
        urlHelperCancel(currentAllowlistUrl);
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    // see if there is already a task running
    //
    if (localAllowlistTaskId != 0)
    {
        // since there is one running go ahead and kill it
        //
        if (cancelPolicyRepeatingTask(localAllowlistTaskId) == false)
        {
            icLogWarn(LOG_TAG, "%s unable to cancel allow-list task!", __FUNCTION__);

            pthread_mutex_lock(&deviceDescriptorMutex);
            free(currentAllowlistUrl);
            currentAllowlistUrl = NULL;
            allowlistTaskId = 0;
            pthread_mutex_unlock(&deviceDescriptorMutex);
        }
    }
}

static bool updateAllowlistTaskFunc(void *taskArg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(free_generic__auto) char *url = strdup(taskArg);
    bool fileUpdated = false;  // file got updated or not
    bool fileUptoDate = false; // file needs update or not
    AUTO_CLEAN(free_generic__auto) char *allowlistPath = getAllowListPath();

    if (allowlistPath == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to fetch allowlist, no local file path configured!", __FUNCTION__);

        return false; // this will cause us to try again.  Perhaps it wasnt set yet or props service not ready
    }

    // is an update even needed?
    if (fileNeedsUpdating(CURRENT_DEVICE_DESCRIPTOR_URL, url, allowlistPath, CURRENT_DEVICE_DESCRIPTOR_MD5) == true)
    {
        if (downloadFile(url, allowlistPath, checkAllowListValid) == false)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogError(LOG_TAG, "%s: failed to download allowlist!", __FUNCTION__);
            TELEMETRY_COUNTER(TELEMETRY_MARKER_WL_DOWNLOAD_ERROR);
        }
        else
        {
            fileUptoDate = true;
            fileUpdated = true;

            // update our system properties
            deviceServiceSetSystemProperty(CURRENT_DEVICE_DESCRIPTOR_URL, url);
            scoped_generic char *md5 = digestFileHex(allowlistPath, CRYPTO_DIGEST_MD5);
            deviceServiceSetSystemProperty(CURRENT_DEVICE_DESCRIPTOR_MD5, md5);
        }
    }
    else
    {
        // no need to download anything, we are up to date.
        //
        fileUptoDate = true;
    }

    pthread_mutex_lock(&deviceDescriptorMutex);
    if (fileUptoDate == true)
    {
        // since we are done, we can clear out our task id here
        //
        allowlistTaskId = 0;
        isAllowlistValid = true;
    }
    // If we do not have valid local allowlist and/or denylist files, devices won't be paired.
    // So call readyForPairingCb if files got updated.
    //
    if (fileUpdated)
    {
        checkAndSetReadyForPairing();
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    if (fileUpdated == true && descriptorsUpdatedCB != NULL)
    {
        descriptorsUpdatedCB();
    }

    icLogDebug(LOG_TAG, "%s completed", __FUNCTION__);

    return fileUptoDate;
}

/*
 * destroyRepeatingTaskObjectsFunc impl for updateAllowlistTaskFunc
 */
static void destroyAllowlistTaskObjectsFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    // destroy policy then currentAllowlistUrl (which is the same as userArg)
    destroyRepeatingPolicy(policy);

    pthread_mutex_lock(&deviceDescriptorMutex);
    allowlistTaskId = 0;
    pthread_mutex_unlock(&deviceDescriptorMutex);
    free(userArg);
}

void deviceDescriptorsUpdateDenylist(const char *url)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, url == NULL ? "(null)" : url);

    // cancel any task that is already running
    cancelDenylistUpdate();

    pthread_mutex_lock(&deviceDescriptorMutex);

    // Only start the task if we have a valid denylist url
    if (url != NULL && strlen(url) >= MIN_URL_LENGTH && strcasecmp(url, DEFAULT_INVALID_DENYLIST_URL) != 0)
    {
        // Save off the URL. This is exclusively so we can call urlHelperCancel if a curl call from
        // updateDenylistTaskFunc is running when we need to cancel/shutdown.
        free(currentDenylistUrl);
        currentDenylistUrl = strdup(url);

        scoped_generic char *pendingDenylistDownloadUrl = strdup(url);

        // Kick it off in the background, with increasing backoff until it eventually completes
        // Kick it off in the background, with increasing backoff until it eventually completes
        RepeatingTaskPolicy *policy = createIncrementalRepeatingTaskPolicy(
            INIT_DD_TASK_WAIT_TIME_SECONDS, MAX_DD_TASK_WAIT_TIME_SECONDS, INTERVAL_DD_TASK_TIME_SECONDS, DELAY_SECS);
        denylistTaskId = createPolicyRepeatingTask(updateDenylistTaskFunc,
                                                   destroyDenylistTaskObjectsFunc,
                                                   policy,
                                                   g_steal_pointer(&pendingDenylistDownloadUrl));
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *denylistPath = getDenyListPath();
        if (denylistPath != NULL)
        {
            unlink(denylistPath);

            // clear out our related properties
            deviceServiceSetSystemProperty(CURRENT_DENYLIST_URL, "");
            deviceServiceSetSystemProperty(CURRENT_DENYLIST_MD5, "");
        }
    }

    pthread_mutex_unlock(&deviceDescriptorMutex);
}

static void cancelDenylistUpdate()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // Avoid holding the lock while we cancel as that can result in deadlock
    pthread_mutex_lock(&deviceDescriptorMutex);
    uint32_t localDenylistTaskId = denylistTaskId;
    if (localDenylistTaskId != 0)
    {
        // Cancel the URL request first, or this thread may block
        // for many seconds waiting for a running download to finish.
        urlHelperCancel(currentDenylistUrl);
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    // see if there is already a task running
    //
    if (localDenylistTaskId != 0)
    {
        // since there is one running go ahead and kill it
        //
        if (cancelPolicyRepeatingTask(localDenylistTaskId) == false)
        {
            icLogWarn(LOG_TAG, "%s unable to cancel deny-list task!", __FUNCTION__);

            pthread_mutex_lock(&deviceDescriptorMutex);
            free(currentDenylistUrl);
            currentDenylistUrl = NULL;
            denylistTaskId = 0;
            pthread_mutex_unlock(&deviceDescriptorMutex);
        }
    }
}

static bool updateDenylistTaskFunc(void *taskArg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(free_generic__auto) char *url = strdup(taskArg);
    bool fileUpdated = false;  // file got updated or not
    bool fileUptoDate = false; // file needs update or not
    AUTO_CLEAN(free_generic__auto) char *denylistPath = getDenyListPath();

    if (denylistPath == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to fetch denylist, no local file path configured!", __FUNCTION__);

        return false; // this will cause us to try again.  Perhaps it wasnt set yet or props service not ready
    }

    if (fileNeedsUpdating(CURRENT_DENYLIST_URL, url, denylistPath, CURRENT_DENYLIST_MD5) == true)
    {
        if (downloadFile(url, denylistPath, checkDenyListValid) == false)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogError(LOG_TAG, "%s: failed to download denylist!", __FUNCTION__);
            TELEMETRY_COUNTER(TELEMETRY_MARKER_BL_DOWNLOAD_ERROR);
        }
        else
        {
            fileUpdated = true;
            fileUptoDate = true;

            // update our system properties
            deviceServiceSetSystemProperty(CURRENT_DENYLIST_URL, url);
            scoped_generic char *md5 = digestFileHex(denylistPath, CRYPTO_DIGEST_MD5);
            deviceServiceSetSystemProperty(CURRENT_DENYLIST_MD5, md5);
        }
    }
    else
    {
        fileUptoDate = true;
    }

    pthread_mutex_lock(&deviceDescriptorMutex);
    if (fileUptoDate == true)
    {
        // since we are done, we can clear out our task id here
        denylistTaskId = 0;
        isDenylistValid = true;
    }
    // If we do not have valid local allowlist and/or denylist files, devices won't be paired.
    // So call readyForPairingCb if files got updated.
    //
    if (fileUpdated)
    {
        checkAndSetReadyForPairing();
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    if (fileUpdated == true && descriptorsUpdatedCB != NULL)
    {
        descriptorsUpdatedCB();
    }

    return fileUptoDate;
}

/*
 * destroyRepeatingTaskObjectsFunc impl for updateDenylistTaskFunc
 */
static void destroyDenylistTaskObjectsFunc(uint32_t taskHandle, void *userArg, RepeatingTaskPolicy *policy)
{
    // destroy policy then currentDenylistUrl (which is the same as userArg)
    destroyRepeatingPolicy(policy);

    pthread_mutex_lock(&deviceDescriptorMutex);
    denylistTaskId = 0;
    pthread_mutex_unlock(&deviceDescriptorMutex);
    free(userArg);
}


static bool downloadFile(const char *url, const char *destFile, deviceDescriptorFileValidator fileValidator)
{
    icLogDebug(LOG_TAG, "%s: url=%s, destFile=%s", __FUNCTION__, url, destFile);

    bool result = false;
    long httpCode = -1;
    size_t fileSize = 0;

    // Write to a temp file so we don't end up with a bogus allowlist in case of any failure
    AUTO_CLEAN(free_generic__auto) char *tmpfilename = stringBuilder("%s.tmp", destFile);

    const char *propKey = sslVerifyPropKeyForCategoryBarton(SSL_VERIFY_HTTP_FOR_SERVER);
    g_autoptr(BDeviceServicePropertyProvider) propertyProvider = deviceServiceConfigurationGetPropertyProvider();
    g_autofree char *propValue =
        b_device_service_property_provider_get_property_as_string(propertyProvider, propKey, NULL);

    sslVerify verifyFlag = convertVerifyPropValToModeBarton(propValue);

    fileSize = urlHelperDownloadFile(url, &httpCode, NULL, NULL, DOWNLOAD_TIMEOUT_SECS, verifyFlag, true, tmpfilename);

    if (fileSize > 0 && (httpCode == 200 || httpCode == 0))
    {
        if (fileValidator == NULL || fileValidator(tmpfilename) == true)
        {
            // Move into place
            if (rename(tmpfilename, destFile))
            {
                icLogError(LOG_TAG, "%s: failed to move %s to %s", __FUNCTION__, tmpfilename, destFile);
            }
            else
            {
                result = true;
                icLogInfo(LOG_TAG, "%s: %s downloaded to %s", __FUNCTION__, url, destFile);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: downloaded file failed to validate", __FUNCTION__);
        }
    }
    else
    {
        icLogError(
            LOG_TAG, "%s: failed to download %s.  httpCode=%ld, fileSize=%zu", __FUNCTION__, url, httpCode, fileSize);
    }

    if (result == false)
    {
        if (remove(tmpfilename))
        {
            icLogError(LOG_TAG, "%s: failed to remove %s", __FUNCTION__, tmpfilename);
        }
    }

    return result;
}

/*
 * See if the provided URLs are different or if the provided file does not match the provided md5sum.  Any of this
 * would indicate a difference that should trigger a download.
 *
 * @return true if we need to download.
 */
static bool fileNeedsUpdating(const char *currentUrlSystemKey,
                              const char *newUrl,
                              const char *currentFilePath,
                              const char *currentFileMd5SystemKey)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    // Fast exit:
    // If either the URL or MD5 is missing from our system properties (or if we fail to fetch them), we need to download
    char *currentUrl = NULL;
    if (deviceServiceGetSystemProperty(currentUrlSystemKey, &currentUrl) == false || currentUrl == NULL)
    {
        icLogWarn(
            LOG_TAG, "%s: unable to get %s system prop -- triggering download", __FUNCTION__, currentUrlSystemKey);
        return true;
    }

    char *currentMd5 = NULL;
    if (deviceServiceGetSystemProperty(currentFileMd5SystemKey, &currentMd5) == false || currentMd5 == NULL)
    {
        icLogWarn(
            LOG_TAG, "%s: unable to get %s system prop -- triggering download", __FUNCTION__, currentFileMd5SystemKey);
        free(currentUrl);
        return true;
    }

    // see if our target file (local) exists
    //
    if (doesFileExist(currentFilePath) == false)
    {
        // missing local file
        //
        icLogWarn(LOG_TAG, "%s: stat failed with error \"%s\", attempting download.", __FUNCTION__, strerror(errno));
    }
    else
    {
        // see if the URL changed
        if (strcmp(currentUrl, newUrl) == 0)
        {
            // URL is the same, so compare the MD5
            scoped_generic char *localMd5 = digestFileHex(currentFilePath, CRYPTO_DIGEST_MD5);

            // compare to what we have in our database (to see if the local file was altered, replaced, etc)
            if (stringCompare(currentMd5, localMd5, true) != 0)
            {
                // MD5 mis-match
                icLogWarn(LOG_TAG, "%s: md5 mismatch between dbase and local file, attempting download.", __FUNCTION__);
            }
            else
            {
                // URL and MD5 match
                icLogDebug(LOG_TAG,
                           "%s: URL (%s) and MD5 sums match (%s), no need to download",
                           __FUNCTION__,
                           newUrl,
                           currentFilePath);

                result = false;
            }
        }
        else
        {
            // URL mismatch
            icLogDebug(
                LOG_TAG, "%s: currentUrl = %s, new url = %s -- need to update", __FUNCTION__, currentUrl, newUrl);
        }
    }

    // cleanup and return
    //
    free(currentUrl);
    free(currentMd5);

    return result;
}

// FIXME: Copy Paste Tech Debt (duplicated in zigbeeDriverCommon)

#define DEFAULT_SSL_VERIFY_MODE               SSL_VERIFY_BOTH
#define SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER "cpe.sslCert.validateForHttpsToServer"
#define SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE "cpe.sslCert.validateForHttpsToDevice"
#define SSL_VERIFICATION_TYPE_NONE            "none"
#define SSL_VERIFICATION_TYPE_HOST            "host"
#define SSL_VERIFICATION_TYPE_PEER            "peer"
#define SSL_VERIFICATION_TYPE_BOTH            "both"

static const char *sslVerifyCategoryToProp[] = {
    SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER,
    SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE,
};

static sslVerify convertVerifyPropValToModeBarton(const char *strVal)
{
    sslVerify retVal = DEFAULT_SSL_VERIFY_MODE;
    if (strVal == NULL || strlen(strVal) == 0 || strcasecmp(strVal, SSL_VERIFICATION_TYPE_NONE) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_NONE option");
        retVal = SSL_VERIFY_NONE;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_HOST) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_HOST option");
        retVal = SSL_VERIFY_HOST;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_PEER) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_PEER option");
        retVal = SSL_VERIFY_PEER;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_BOTH) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_BOTH option");
        retVal = SSL_VERIFY_BOTH;
    }
    else
    {
        icLogDebug(LOG_TAG, "using default option [%d]", retVal);
    }

    return retVal;
}

static const char *sslVerifyPropKeyForCategoryBarton(sslVerifyCategory cat)
{
    const char *propKey = NULL;

    if (cat >= SSL_VERIFY_CATEGORY_FIRST && cat <= SSL_VERIFY_CATEGORY_LAST)
    {
        propKey = sslVerifyCategoryToProp[cat];
    }

    return propKey;
}
