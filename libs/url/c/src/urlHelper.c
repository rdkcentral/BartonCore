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

/*
 * Created by Thomas Lea on 8/13/15.
 */

#include <curl/curl.h>
#include <inttypes.h>
#include <pthread.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>

#include "config_url_helper.h"
#include "icConcurrent/timedWait.h"
#include "icTime/timeTracker.h"
#include "urlHelper/urlHelper.h"
#include <errno.h>
#include <icConcurrent/threadUtils.h>
#include <icTypes/icStringBuffer.h>
#include <icUtil/array.h>
#include <urlHelper/properties.h>

#define LOG_TAG                  "urlHelper"

#define CONNECT_TIMEOUT          15
#define CELLULAR_CONNECT_TIMEOUT 30

/*
 * 118 is the cURL default
 * @ref https://curl.haxx.se/mail/lib-2019-04/0040.html
 */
#define DEFAULT_CONN_IDLE_SECS   118

pthread_mutex_t cancelMtx;
icStringHashMap *cancelUrls;

pthread_once_t initOnce = PTHREAD_ONCE_INIT;

#define DEFAULT_LOW_BYTES_PER_SEC   150

/*
 * 1 TLS record is 16k, and cURL will accept no less than 16k.
 * On slow links, too large a buffer can make cURL think it's transmitted
 * more data than it has, and can cause false timeouts.
 */
#define DEFAULT_UPLOAD_BUFFER_BYTES 16384L

/**
 * Try to set a cURL share option and complain with a warning if it fails
 */
#define curlSetShareOpt(share, opt, ...)                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        CURLSHcode err;                                                                                                \
        if ((err = curl_share_setopt((share), (opt), (__VA_ARGS__))) != CURLSHE_OK)                                    \
        {                                                                                                              \
            icLogWarn(LOG_TAG,                                                                                         \
                      "curl_share_setopt(" #share ", %s, %s) "                                                         \
                      "failed at %s(%d): %s",                                                                          \
                      #opt,                                                                                            \
                      #__VA_ARGS__,                                                                                    \
                      __FILE__,                                                                                        \
                      __LINE__,                                                                                        \
                      curl_share_strerror(err));                                                                       \
        }                                                                                                              \
    } while (0)

static pthread_mutex_t shareMtx[CURL_LOCK_DATA_LAST];
static CURLSH *curlShare;

static void cleanupCurlShare(void);

static void lockCurlShare(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr)
{
    mutexLock(&shareMtx[data]);
}

static void unlockCurlShare(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr)
{
    mutexUnlock(&shareMtx[data]);
}

static void configureCurlShare(CURLSH *share)
{
    if (share == NULL)
    {
        return;
    }

    if (CURL_AT_LEAST_VERSION(7, 23, 0) == true)
    {
        curlSetShareOpt(curlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    }

    if (CURL_AT_LEAST_VERSION(7, 57, 0) == true)
    {
        curlSetShareOpt(curlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    }
}

static void setupCurlShare()
{
    /*
     * Don't evaluate the enable property here so curlShare is ready
     * whenever the URL_HELPER_PROP_REUSE_ENABLE property is set.
     */

    for (int i = 0; i < ARRAY_LENGTH(shareMtx); i++)
    {
        mutexInitWithType(&shareMtx[i], PTHREAD_MUTEX_ERRORCHECK);
    }

    curlShare = curl_share_init();

    CURLSHcode rc;
    bool ok = true;
    if ((rc = curl_share_setopt(curlShare, CURLSHOPT_LOCKFUNC, lockCurlShare)) != CURLSHE_OK)
    {
        icLogError(
            LOG_TAG, "%s: Could not set lock function: '%s'. Sharing disabled.", __func__, curl_share_strerror(rc));
        ok = false;
    }

    if ((rc = curl_share_setopt(curlShare, CURLSHOPT_UNLOCKFUNC, unlockCurlShare)) != CURLSHE_OK)
    {
        icLogError(
            LOG_TAG, "%s: Could not set unlock function: '%s'. Sharing disabled.", __func__, curl_share_strerror(rc));
        ok = false;
    }

    if (ok == true)
    {
        /* This configuration is best effort. Any or all of it may fail without causing a problem. */
        configureCurlShare(curlShare);
        atexit(cleanupCurlShare);
    }
    else
    {
        cleanupCurlShare();
    }
}

#define SHARE_CLEANUP_WAIT_TIMEOUT_MILLIS (5000)

/* Called automatically on library unload/exit */
static void cleanupCurlShare(void)
{
    icLogTrace(LOG_TAG, "%s", __func__);

    pthread_mutex_t mtx = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
    pthread_cond_t cond;
    initTimedWaitCond(&cond);
    scoped_timeTracker *tracker = timeTrackerCreate();
    timeTrackerStartWithUnit(tracker, SHARE_CLEANUP_WAIT_TIMEOUT_MILLIS, TIME_TRACKER_MILLIS);

    CURLSHcode cleanupCode = curl_share_cleanup(curlShare);
    while (cleanupCode == CURLSHE_IN_USE && !timeTrackerExpired(tracker))
    {
        icLogWarn(LOG_TAG, "curl_share_cleanup() returned CURLSHE_IN_USE; retrying");
        incrementalCondTimedWaitMillis(&cond, &mtx, timeTrackerTimeUntilExpiration(tracker, TIME_TRACKER_MILLIS));
        cleanupCode = curl_share_cleanup(curlShare);
    }

    if (timeTrackerExpired(tracker))
    {
        icLogError(LOG_TAG, "Timed out waiting for curl_share_cleanup to succeed- last returned %d", cleanupCode);
    }
    else
    {
        icLogDebug(LOG_TAG, "curl_share_cleanup() returned %d", cleanupCode);
    }

    curlShare = NULL;
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mtx);
}

typedef struct
{
    char *ptr;
    size_t len;
} ResponseBuffer;

typedef struct fileDownloadData
{
    FILE *fout;
    size_t size;
} fileDownloadData;

/**
 * Internal function for running the cURL request. When mTLS is enabled,
 * this will try again without mTLS automatically when the client certificate is unusable.
 * @param curl The cURL context used by the cURL library
 * @param reportOnFailure If true then report that a network connectivity issue may have
 *                        occurred to the system reporting component.
 * @return the HTTP response status code
 * @note the return value may be < 200 if no response is received or the connection is interrupted.
 */
static long performRequest(CURL *curl, bool reportOnFailure);
static inline CURL *urlHelperCreateCurl(void);
static inline void urlHelperDestroyCurl(CURL *ctx);

/**
 * Auto initializer: called by pthread_once
 */
static void urlHelperInit(void)
{
    mutexInitWithType(&cancelMtx, PTHREAD_MUTEX_ERRORCHECK);
    cancelUrls = stringHashMapCreate();
    curl_global_init(CURL_GLOBAL_ALL);
    atexit(curl_global_cleanup);
    setupCurlShare();
}

static int curlDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *debugCtx);

static bool initResponseBuffer(ResponseBuffer *buff)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;
    buff->len = 0;
    buff->ptr = malloc(buff->len + 1);
    if (buff->ptr == NULL)
    {
        icLogError(LOG_TAG, "failed to allocate response buffer");
    }
    else
    {
        buff->ptr[0] = '\0';
        result = true;
    }

    return result;
}

/* implement the 'curl_write_callback' prototype
typedef size_t (*curl_write_callback)(char *buffer,
                                      size_t size,
                                      size_t nitems,
                                      void *outstream);
*/
static size_t writefunc(char *ptr, size_t size, size_t nmemb, void *outstream)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    ResponseBuffer *buff = (ResponseBuffer *) outstream;
    size_t new_len = buff->len + size * nmemb;
    buff->ptr = realloc(buff->ptr, new_len + 1);
    if (buff->ptr == NULL)
    {
        icLogError(LOG_TAG, "failed to reallocate response buffer");
    }
    else
    {
        memcpy(buff->ptr + buff->len, ptr, size * nmemb);
        buff->ptr[new_len] = '\0';
        buff->len = new_len;
    }

    return size * nmemb;
}

/*
 * Core logic for performing HTTP Request. Allows other callers in the library to presupply a curl context, if
 * additional options are needed (such as in the case of multipart)
 */
static char *urlHelperPerformRequestInternal(CURL *curl,
                                             const char *url,
                                             long *httpCode,
                                             const char *postData,
                                             icLinkedList *headerStrings,
                                             const char *username,
                                             const char *password,
                                             uint32_t timeoutSecs,
                                             const mtlsCertInfo *certInfo,
                                             sslVerify verifyFlag,
                                             bool allowCellular)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    char *result = NULL;

    if (curl)
    {
        ResponseBuffer buff;
        ResponseBuffer *pBuff = &buff;
        if (initResponseBuffer(&buff) == true)
        {
            // apply standard options
            applyStandardCurlOptions(curl, url, timeoutSecs, certInfo, verifyFlag, allowCellular);

            icLogTrace(LOG_TAG, "%s : Done setting standard curl options applying additional options", __FUNCTION__);
            // set additional options
            curlSetopt(curl, CURLOPT_URL, url);
            curlSetopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curlSetopt(curl, CURLOPT_WRITEDATA, pBuff);

            // apply POST data if supplied
            if (postData != NULL)
            {
                curlSetopt(curl, CURLOPT_POSTFIELDS, postData);
                icLogTrace(LOG_TAG, "%s : Applied POST data", __FUNCTION__);
            }

            // apply credentials if supplied
            char *userpass = NULL;
            if (username != NULL && password != NULL)
            {
                userpass = (char *) malloc(strlen(username) + 1 + strlen(password) + 1);
                if (userpass == NULL)
                {
                    icLogError(LOG_TAG, "Failed to allocate userpass");
                    free(buff.ptr);
                    return NULL;
                }
                sprintf(userpass, "%s:%s", username, password);
                curlSetopt(curl, CURLOPT_USERPWD, userpass);
                icLogTrace(LOG_TAG, "%s : Applied user credentials", __FUNCTION__);
            }

            // apply HTTP headers if supplied
            struct curl_slist *header = NULL;
            if (linkedListCount(headerStrings) > 0)
            {
                // add each string
                //
                icLinkedListIterator *loop = linkedListIteratorCreate(headerStrings);
                while (linkedListIteratorHasNext(loop) == true)
                {
                    char *string = (char *) linkedListIteratorGetNext(loop);
                    if (string != NULL)
                    {
                        header = curl_slist_append(header, string);
                    }
                }
                linkedListIteratorDestroy(loop);
                curlSetopt(curl, CURLOPT_HTTPHEADER, header);
                icLogTrace(LOG_TAG, "%s : Applied HTTP headers", __FUNCTION__);
            }

            applyUserAgentCurlOptionIfAvailable(curl);

            icLogDebug(LOG_TAG, "%s : Going to perform curl request", __FUNCTION__);

            *httpCode = performRequest(curl, allowCellular);

            if (header != NULL)
            {
                curl_slist_free_all(header);
            }
            result = buff.ptr;

            // cleanup
            if (userpass != NULL)
            {
                free(userpass);
            }
        }
    }
    return result;
}

static long performRequest(CURL *curl, bool reportOnFailure)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    long httpCode = 0;
    if (curl == NULL)
    {
        return 0;
    }

    icLogTrace(LOG_TAG, "%s : curl_easy_perform()", __FUNCTION__);

    CURLcode curlcode = curl_easy_perform(curl);

    /* This code is only emitted when mTLS is enabled and the client cert is invalid */
    if (curlcode == CURLE_SSL_CERTPROBLEM)
    {
        curlSetopt(curl, CURLOPT_SSLCERT, NULL);
        icLogWarn(LOG_TAG, "cURL could not use mTLS certificate; attempting unsigned request");
        curlcode = curl_easy_perform(curl);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    icLogDebug(LOG_TAG, "curl info response httpcode : %ld", httpCode);

    if (curlcode != CURLE_OK)
    {
        icLogDebug(LOG_TAG,
                   "Error performing HTTP request. HTTP status: [%ld]; Error code: [%d][%s]",
                   httpCode,
                   curlcode,
                   curl_easy_strerror(curlcode));

        if (reportOnFailure)
        {
            switch (curlcode)
            {
                case CURLE_COULDNT_CONNECT:
                case CURLE_COULDNT_RESOLVE_HOST:
                case CURLE_COULDNT_RESOLVE_PROXY:
                case CURLE_BAD_DOWNLOAD_RESUME:
                case CURLE_INTERFACE_FAILED:
                case CURLE_GOT_NOTHING:
                case CURLE_NO_CONNECTION_AVAILABLE:
                case CURLE_OPERATION_TIMEDOUT:
                case CURLE_PARTIAL_FILE:
                case CURLE_READ_ERROR:
                case CURLE_RECV_ERROR:
                case CURLE_SEND_ERROR:
                case CURLE_SEND_FAIL_REWIND:
                case CURLE_SSL_CONNECT_ERROR:
                case CURLE_UPLOAD_FAILED:
                case CURLE_WRITE_ERROR:
                    icLogDebug(LOG_TAG, "Network connectivity concerns.");
                    break;
                default:
                    break;
            }
        }
    }

    return httpCode;
}

/*
 * Execute a request to a web server and return the resulting body.
 * Caller should free the return string (if not NULL)
 *
 * @param requestUrl - the URL for the request that can contain variables
 * @param httpCode - the result code from the server for the HTTP request
 * @param postData - text to use for an HTTP POST operation or NULL to use a standard HTTP GET
 * @param variableMap - the string map of variable names to values used to substitute in the requestUri
 * @param username - the username to use for basic authentication or NULL for none
 * @param password - the password to use for basic authentication or NULL for none
 * @param timeoutSecs - number of seconds to timeout (0 means no timeout set)
 * @param verifyFlag  - SSL verification flag
 * @param allowCellular - A flag to indicate if this request should be sent out over cell iff it cannot be sent
 *                        over bband
 *
 * @returns the body of the response.  Caller frees.
 */
char *urlHelperExecuteVariableRequest(char *requestUrl,
                                      icStringHashMap *variableMap,
                                      long *httpCode,
                                      char *postData,
                                      const char *username,
                                      const char *password,
                                      uint32_t timeoutSecs,
                                      sslVerify verifyFlag,
                                      bool allowCellular)
{
    char *result = NULL;

    if (requestUrl == NULL || variableMap == NULL)
    {
        icLogError(LOG_TAG, "executeVariableRequest: invalid args");
        return NULL;
    }

    char *updatedUri = requestUrl;
    char *updatedPostData = postData;

    // for each variable in the variable map, search and replace all occurrences in the requestUri and the postData (if
    // it was provided)
    icStringHashMapIterator *iterator = stringHashMapIteratorCreate(variableMap);
    while (stringHashMapIteratorHasNext(iterator))
    {
        char *key;
        char *value;
        stringHashMapIteratorGetNext(iterator, &key, &value);

        char *tmp = stringReplace(updatedUri, key, value);
        if (updatedUri != requestUrl)
        {
            free(updatedUri);
        }
        updatedUri = tmp;

        if (postData != NULL)
        {
            tmp = stringReplace(updatedPostData, key, value);
            if (updatedPostData != postData)
            {
                free(updatedPostData);
            }
            updatedPostData = tmp;
        }
    }
    stringHashMapIteratorDestroy(iterator);

    result = urlHelperExecuteRequest(
        updatedUri, httpCode, updatedPostData, username, password, timeoutSecs, verifyFlag, allowCellular);

    if (updatedUri != requestUrl)
    {
        free(updatedUri);
    }
    if (postData != NULL && updatedPostData != postData)
    {
        free(updatedPostData);
    }

    return result;
}

/*
 * Execute a simple request to the provided URL and return the body. If username and password
 * are provided, basic authentication will be used.
 * Caller should free the return string (if not NULL)
 *
 * @param url - the URL to retrieve
 * @param httpCode - the result code from the server for the HTTP request
 * @param postData - text to use for an HTTP POST operation or NULL to use a standard HTTP GET
 * @param username - the username to use for basic authentication or NULL for none
 * @param password - the password to use for basic authentication or NULL for none
 * @param timeoutSecs - number of seconds to timeout (0 means no timeout set)
 * @param verifyFlag  - SSL verification flag
 * @param allowCellular - A flag to indicate if this request should be sent out over cell iff it cannot be sent
 *                        over bband
 *
 * @returns the body of the response.  Caller frees.
 */
char *urlHelperExecuteRequest(const char *url,
                              long *httpCode,
                              const char *postData,
                              const char *username,
                              const char *password,
                              uint32_t timeoutSecs,
                              sslVerify verifyFlag,
                              bool allowCellular)
{
    return urlHelperExecuteRequestHeaders(
        url, httpCode, postData, NULL, username, password, timeoutSecs, verifyFlag, allowCellular);
}

/*
 * same as urlHelperExecuteRequest, but allow for assigning HTTP headers in the request.
 * For ex:  Accept: application/json
 *
 * @param headers - list of strings that define header values.  ex: "Content-Type: text/xml; charset=utf-8".  ignored if
 * NULL
 */
char *urlHelperExecuteRequestHeaders(const char *url,
                                     long *httpCode,
                                     const char *postData,
                                     icLinkedList *headerStrings,
                                     const char *username,
                                     const char *password,
                                     uint32_t timeoutSecs,
                                     sslVerify verifyFlag,
                                     bool allowCellular)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    CURL *ctx = urlHelperCreateCurl();
    char *result = urlHelperPerformRequestInternal(
        ctx, url, httpCode, postData, headerStrings, username, password, timeoutSecs, NULL, verifyFlag, allowCellular);
    urlHelperDestroyCurl(ctx);

    return result;
}


char *urlHelperExecuteMTLSRequest(const char *url,
                                  long *httpCode,
                                  const char *postData,
                                  const char *username,
                                  const char *password,
                                  uint32_t timeoutSecs,
                                  const mtlsCertInfo *certInfo,
                                  bool allowCellular)
{
    return urlHelperExecuteMTLSRequestHeaders(
        url, httpCode, postData, NULL, username, password, timeoutSecs, certInfo, allowCellular);
}

char *urlHelperExecuteMTLSRequestHeaders(const char *url,
                                         long *httpCode,
                                         const char *postData,
                                         icLinkedList *headerStrings,
                                         const char *username,
                                         const char *password,
                                         uint32_t timeoutSecs,
                                         const mtlsCertInfo *certInfo,
                                         bool allowCellular)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    CURL *ctx = urlHelperCreateCurl();
    char *result = urlHelperPerformRequestInternal(ctx,
                                                   url,
                                                   httpCode,
                                                   postData,
                                                   headerStrings,
                                                   username,
                                                   password,
                                                   timeoutSecs,
                                                   certInfo,
                                                   SSL_VERIFY_BOTH,
                                                   allowCellular);
    urlHelperDestroyCurl(ctx);

    return result;
}

/*
 * same as urlHelperExecuteMultipartRequestHeaders, but without headers
 */
char *urlHelperExecuteMultipartRequest(const char *url,
                                       long *httpCode,
                                       icLinkedList *plainParts,
                                       icLinkedList *fileInfo,
                                       const char *username,
                                       const char *password,
                                       uint32_t timeoutSecs,
                                       sslVerify verifyFlag,
                                       bool allowCellular)
{
    return urlHelperExecuteMultipartRequestHeaders(
        url, httpCode, plainParts, fileInfo, NULL, username, password, timeoutSecs, verifyFlag, allowCellular);
}

/*
 * Performs a multipart POST request using the information passed. It is up to the caller to free all memory passed.
 *
 * @param url               The url to perform the POST request to
 * @param httpCode          A pointer to the httpCode returned by the request.
 * @param plainParts        A list of MimePartInfo types containing key/value string part information.
 * @param fileInfo          A list of MimeFileInfo types containing file information for local files. Each entry in
 *                           the list will be a separate part in the HTTP request, with the file data being the
 *                           part's body.
 * @param headerStrings     A list of header strings for the request
 * @param username          A username to provide the server for authentication
 * @param password          A password to provide the server for authentication
 * @param timeoutSecs       Number of seconds to wait before timeout
 * @param verifyFlag        SSL verification flag
 * @param allowCellular     A flag to indicate if this request should be sent out over cell iff it cannot be sent
 *                          over bband
 * @return The body of the response from the server.
 */
char *urlHelperExecuteMultipartRequestHeaders(const char *url,
                                              long *httpCode,
                                              icLinkedList *plainParts,
                                              icLinkedList *fileInfo,
                                              icLinkedList *headerStrings,
                                              const char *username,
                                              const char *password,
                                              uint32_t timeoutSecs,
                                              sslVerify verifyFlag,
                                              bool allowCellular)
{
    char *result = NULL;

    // We are going to construct the curl context so we can fill it with our multipart data.
    CURL *ctx = urlHelperCreateCurl();
    if (ctx)
    {
        // Init mime
        curl_mime *requestBody = curl_mime_init(ctx);

        if (plainParts != NULL)
        {
            // Iterate over parts and add each one to the mime part
            icLinkedListIterator *partIter = linkedListIteratorCreate(plainParts);
            while (linkedListIteratorHasNext(partIter))
            {
                MimePartInfo *partInfo = (MimePartInfo *) linkedListIteratorGetNext(partIter);

                curl_mimepart *part = curl_mime_addpart(requestBody);
                CURLcode mimeCode = curl_mime_name(part, partInfo->partName);

                if (mimeCode == CURLE_OK)
                {
                    mimeCode = curl_mime_data(part, partInfo->partData, partInfo->dataLength);
                }

                if (mimeCode != CURLE_OK)
                {
                    icLogWarn("Unable to set MIME part '%s': %s", partInfo->partName, curl_easy_strerror(mimeCode));
                }
            }

            linkedListIteratorDestroy(partIter);
        }

        if (fileInfo != NULL)
        {
            // Iterate over the file parts and add each one to the mime part
            icLinkedListIterator *partIter = linkedListIteratorCreate(fileInfo);
            while (linkedListIteratorHasNext(partIter))
            {
                MimeFileInfo *file = (MimeFileInfo *) linkedListIteratorGetNext(partIter);

                // Make sure they supplied the bare mimimum information
                if (file->partName != NULL && file->localFilePath != NULL)
                {
                    curl_mimepart *part = curl_mime_addpart(requestBody);

                    CURLcode mimeCode = curl_mime_name(part, file->partName);

                    if (mimeCode == CURLE_OK)
                    {
                        mimeCode = curl_mime_filedata(part, file->localFilePath);
                    }

                    // Add a content-type if it was specified
                    if (file->contentType != NULL && mimeCode == CURLE_OK)
                    {
                        mimeCode = curl_mime_type(part, file->contentType);
                    }

                    // See if the caller wants to have a custom remote filename
                    if (file->remoteFileName != NULL && mimeCode == CURLE_OK)
                    {
                        mimeCode = curl_mime_filename(part, file->remoteFileName);
                    }

                    if (mimeCode != CURLE_OK)
                    {
                        icLogWarn(
                            LOG_TAG, "Can't set MIME part '%s': %s", file->partName, curl_easy_strerror(mimeCode));
                    }
                }
            }

            linkedListIteratorDestroy(partIter);
        }

        curlSetopt(ctx, CURLOPT_MIMEPOST, requestBody);
        // Since multipart can be a large request, setting this bit will cause libcurl to handle exepct: 100 scenarios
        // properly.
        curlSetopt(ctx, CURLOPT_FAILONERROR, 1L);

        // Perform the operation and get the result
        result = urlHelperPerformRequestInternal(
            ctx, url, httpCode, NULL, headerStrings, username, password, timeoutSecs, NULL, verifyFlag, allowCellular);

        // Cleanup
        curl_mime_free(requestBody);
        urlHelperDestroyCurl(ctx);
    }

    return result;
}

static size_t download_func(void *ptr, size_t size, size_t nmemb, void *stream)
{
    fileDownloadData *data = stream;
    size_t written;

    written = fwrite(ptr, size, nmemb, data->fout);

    /* fwrite returns the number of "items" (as in the number of 'nmemb').
     * Thus to get the correct number of bytes we must multiply by the size.
     */
    data->size += written * size;

    return written;
}

size_t urlHelperDownloadFile(const char *url,
                             long *httpCode,
                             const char *username,
                             const char *password,
                             uint32_t timeoutSecs,
                             sslVerify verifyFlag,
                             bool allowCellular,
                             const char *pathname)
{
    CURL *curl = urlHelperCreateCurl();
    fileDownloadData data;
    fileDownloadData *pData = &data;

    data.size = 0;

    if (curl)
    {
        data.fout = fopen(pathname, "w");
        if (data.fout != NULL)
        {
            // apply standard options
            applyStandardCurlOptions(curl, url, timeoutSecs, NULL, verifyFlag, allowCellular);

            // set additional options
            curlSetopt(curl, CURLOPT_URL, url);
            curlSetopt(curl, CURLOPT_WRITEFUNCTION, download_func);
            curlSetopt(curl, CURLOPT_WRITEDATA, pData);

            // apply credentials if supplied
            char *userpass = NULL;
            if (username != NULL && password != NULL)
            {
                userpass = (char *) malloc(strlen(username) + 1 + strlen(password) + 1);
                if (userpass == NULL)
                {
                    icLogError(LOG_TAG, "Failed to allocate userpass");
                    urlHelperDestroyCurl(curl);
                    return 0;
                }
                sprintf(userpass, "%s:%s", username, password);
                curlSetopt(curl, CURLOPT_USERPWD, userpass);
            }

            *httpCode = performRequest(curl, allowCellular);

            free(userpass);

            fflush(data.fout);
            fclose(data.fout);
        }

        urlHelperDestroyCurl(curl);
    }

    return data.size;
}

/**
 * Helper routine to upload a file using PUT
 *
 * @param url The URL pointing to data to download
 * @param httpCode The returned HTTP response code
 * @param username The username to authorize against
 * @param password The password to use in authorization
 * @param timeoutSecs The number of seconds before timing out.
 * @param verifyFlag SSL verification turned on or off.
 * @param allowCellular Allow this download over a cellular connection
 * @param filename The path and file name to upload
 * @return body of the response
 */
char *urlHelperUploadFile(const char *url,
                          long *httpCode,
                          const char *username,
                          const char *password,
                          uint32_t timeoutSecs,
                          sslVerify verifyFlag,
                          bool allowCellular,
                          const char *fileName)
{
    char *result = NULL;

    // We are going to construct the curl context so we can fill it with our upload file data
    CURL *ctx = urlHelperCreateCurl();
    if (ctx)
    {
        // Verify the file
        if (fileName != NULL)
        {
            struct stat file_info;
            FILE *file;

            errno = 0;
            int rc = stat(fileName, &file_info);
            if (rc == 0)
            {
                // coverity[result_independent_of_operands] length of off_t and long are implementation defined
                if (file_info.st_size <= LONG_MAX)
                {
                    // open the file
                    //
                    file = fopen(fileName, "rb");

                    if (file != NULL)
                    {
                        // Set the operation to PUT
                        //
                        curlSetopt(ctx, CURLOPT_UPLOAD, 1L);

                        // Set the upload data stream to the file we want to upload
                        //
                        curlSetopt(ctx, CURLOPT_READDATA, file);

                        // Add the file size.
                        //
                        curlSetopt(ctx, CURLOPT_INFILESIZE, (long) file_info.st_size);

                        curlSetopt(ctx, CURLOPT_FAILONERROR, 1L);

                        // Perform the operation and get the result
                        result = urlHelperPerformRequestInternal(ctx,
                                                                 url,
                                                                 httpCode,
                                                                 NULL,
                                                                 NULL,
                                                                 username,
                                                                 password,
                                                                 timeoutSecs,
                                                                 NULL,
                                                                 verifyFlag,
                                                                 allowCellular);

                        // clean up the open file
                        //
                        fclose(file);
                    }
                }
                else
                {
                    icLogError(LOG_TAG, "file '%s' too large (2 GiB limit)", fileName);
                }
            }
            else
            {
                AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
                icLogWarn(LOG_TAG, "cannot stat '%s': %s", fileName, errStr);
            }
        }
        // Cleanup
        urlHelperDestroyCurl(ctx);
    }

    return result;
}

static int onCurlXferInfo(void *userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    int action = 0;

    LOCK_SCOPE(cancelMtx);
    if (stringHashMapCount(cancelUrls) > 0 && stringHashMapDelete(cancelUrls, userData, NULL) == true)
    {
        /* Any nonzero will abort */
        action = 1;
    }

    return action;
}

void urlHelperCancel(const char *url)
{
    pthread_once(&initOnce, urlHelperInit);
    LOCK_SCOPE(cancelMtx);
    stringHashMapPutCopy(cancelUrls, url, NULL);
}

/*
 * apply the user agent option if we have a user agent defined
 */
void applyUserAgentCurlOptionIfAvailable(CURL *context)
{
    // apply userAgent if supplied.
    // Note: The UserAgent string is set from USER_AGENT_STRING macro from the header file config_url_helper.h
    // generated from the template file config_url_helper.h.in by the CMakelists.txt
    char *userAgent = USER_AGENT_STRING;
    if (userAgent != NULL)
    {
        curlSetopt(context, CURLOPT_USERAGENT, userAgent);
    }
}

/*
 * apply standard options to a Curl Context.
 * if the 'url' is not null, this will add it to the context.
 * additionally, if the 'verifyFlag' includes VERIFY_HOST or VERIFY_BOTH, then the
 * 'url' will be checked for IP Addresses, and if so remove VERIFY_HOST from the mix
 */
void applyStandardCurlOptions(CURL *context,
                              const char *url,
                              uint32_t timeoutSecs,
                              const mtlsCertInfo *certInfo,
                              sslVerify verifyFlag,
                              bool allowCellular)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    pthread_once(&initOnce, urlHelperInit);

    // apply the 'verify' settings based on the flag
    //
    if (verifyFlag == SSL_VERIFY_HOST || verifyFlag == SSL_VERIFY_BOTH)
    {
        // before applying the VERIFY_HOST, check the url to see if it is a hostname or IP address
        //
        if (url != NULL && urlHelperCanVerifyHost(url) == false)
        {
            icLogInfo(LOG_TAG, "Disabling SSL_VERIFY_HOST, url %s appears to be an IP address", url);
            curlSetopt(context, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        else
        {
            // 2 is necessary to enable host verify.
            // 1 is a deprecated argument that used to be used for debugging but now does nothing.
            curlSetopt(context, CURLOPT_SSL_VERIFYHOST, 2L);
        }
    }
    else
    {
        curlSetopt(context, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (verifyFlag == SSL_VERIFY_PEER || verifyFlag == SSL_VERIFY_BOTH)
    {
        curlSetopt(context, CURLOPT_SSL_VERIFYPEER, 1L);
    }
    else
    {
        curlSetopt(context, CURLOPT_SSL_VERIFYPEER, 0L);
    }

    /* Prior to 7.62.0, UPLOAD_BUFFERSIZE was 16KiB, and was fixed. */
#if CURL_AT_LEAST_VERSION(7, 62, 0)
    /*
     * Slow links can cause false "Operation Too Slow" errors if upload buffers
     * are too large.  To avoid this, the upload buffer size is limited to ensure
     * that the progress callback is called with ulnow numbers that more closely
     * match what was actually written to the underlying socket.
     */
    curlSetopt(context, CURLOPT_UPLOAD_BUFFERSIZE, DEFAULT_UPLOAD_BUFFER_BYTES);
#endif

    // set the input URL
    //
    if (url != NULL)
    {
        curlSetopt(context, CURLOPT_URL, url);
    }

    if (curlShare != NULL)
    {
        curlSetopt(context, CURLOPT_SHARE, curlShare);
    }

/* Option did not exist before 7.65.0 */
#if CURL_AT_LEAST_VERSION(7, 65, 0)
    /* TODO: allow request to override */
    curlSetopt(context, CURLOPT_MAXAGE_CONN, DEFAULT_CONN_IDLE_SECS);
#endif

    // follow any redirection (302?)
    //
    curlSetopt(context, CURLOPT_FOLLOWLOCATION, 1L);

    // bail if there's an error
    //
    curlSetopt(context, CURLOPT_FAILONERROR, 1L);

    // prevent curl from calling SIGABRT if we are trying to communicate with
    // a device that won't let us negotiate SSL or login properly
    //
    curlSetopt(context, CURLOPT_NOSIGNAL, 1);

    // disable DNS caching
    //
    curlSetopt(context, CURLOPT_DNS_CACHE_TIMEOUT, 0);

    if (isIcLogPriorityTrace() == true)
    {
        // enable verbose output
        curlSetopt(context, CURLOPT_VERBOSE, 1L);
        curlSetopt(context, CURLOPT_DEBUGFUNCTION, curlDebugCallback);
    }
    else
    {
        // disable verbose output
        curlSetopt(context, CURLOPT_VERBOSE, 0L);
    }

    curlSetopt(context, CURLOPT_NOPROGRESS, 0);
    curlSetopt(context, CURLOPT_XFERINFOFUNCTION, onCurlXferInfo);
    curlSetopt(context, CURLOPT_XFERINFODATA, url);

    if (timeoutSecs > 0)
    {
        uint32_t connectTimeout = (allowCellular) ? CELLULAR_CONNECT_TIMEOUT : CONNECT_TIMEOUT;

        if (connectTimeout > timeoutSecs)
        {
            connectTimeout = timeoutSecs;
        }

        // When using the minimum speed limit operation timeout should be unlimited.
        // Transfers that fail to receive or send the low speed limit, in bytes per second,
        // for an entire low speed time period will be aborted.
        //
        curlSetopt(context, CURLOPT_TIMEOUT, 0);
        curlSetopt(context, CURLOPT_LOW_SPEED_LIMIT, DEFAULT_LOW_BYTES_PER_SEC);
        curlSetopt(context, CURLOPT_LOW_SPEED_TIME, timeoutSecs);

        // set the 'socket connect' timeout
        //
        curlSetopt(context, CURLOPT_CONNECTTIMEOUT, connectTimeout);
    }
    icLogDebug(LOG_TAG, "%s : Done applying standard curl options", __FUNCTION__);
}

/*
 * Internal function to create URL encoded String
 * Returns queryString or form-Data based on isQueryString value sent by caller.
 */
static char *urlHelperCreateStringData(const icStringHashMap *keyValuePairs, bool isQueryString)
{
    char *retVal = NULL;
    if (keyValuePairs != NULL && stringHashMapCount((icStringHashMap *) keyValuePairs) > 0)
    {
        CURL *ctx = curl_easy_init();
        icStringBuffer *buffer = stringBufferCreate(0);
        if (isQueryString)
        {
            stringBufferAppend(buffer, "?");
        }
        icStringHashMapIterator *iterator = stringHashMapIteratorCreate((icStringHashMap *) keyValuePairs);
        while (stringHashMapIteratorHasNext(iterator) == true)
        {
            char *key;
            char *value;
            stringHashMapIteratorGetNext(iterator, &key, &value);

            char *escapedKey = curl_easy_escape(ctx, key, strlen(key));
            char *escapedValue = curl_easy_escape(ctx, value, strlen(value));
            stringBufferAppend(buffer, escapedKey);
            stringBufferAppend(buffer, "=");
            stringBufferAppend(buffer, escapedValue);
            if (stringHashMapIteratorHasNext(iterator) == true)
            {
                stringBufferAppend(buffer, "&");
            }

            curl_free(escapedKey);
            escapedKey = NULL;
            curl_free(escapedValue);
            escapedValue = NULL;
        }
        stringHashMapIteratorDestroy(iterator);

        retVal = stringBufferToString(buffer);

        stringBufferDestroy(buffer);
        curl_easy_cleanup(ctx);
    }

    return retVal;
}

/**
 * Creates a query string based on provided keyValuePairs in the form of
 * "?key1=value1&key2=value2&..."
 * Note the keys and values will be url encoded.
 *
 * @param keyValuePairs - map of string key/value pairs to use for the query string
 * @return a valid url query string starting with '?' with url encoded members, or NULL on error.
 */
char *urlHelperCreateQueryString(const icStringHashMap *keyValuePairs)
{
    char *queryString = urlHelperCreateStringData(keyValuePairs, true);
    return queryString;
}

/**
 * Creates a URL encoded form-data based on provided keyValuePairs in the form of
 * "key1=value1&key2=value2&..."
 * Note the keys and values will be url encoded.
 *
 * @param keyValuePairs - map of string key/value pairs to use for the query string
 * @return a valid url URL encoded form-data, or NULL on error.
 */
char *urlHelperCreateUrlEncodedForm(const icStringHashMap *keyValuePairs)
{
    char *formData = urlHelperCreateStringData(keyValuePairs, false);
    return formData;
}

/*
 * pull the hostname from the url string.
 * caller must free the string.
 */
static char *extractHostFromUrl(const char *urlStr)
{
    // sanity check
    //
    if (stringIsEmpty(urlStr) == true)
    {
        return NULL;
    }

    // in a perfect world, we could leverage "curl_url_get", but that requires libcurl 7.62
    // and we can't do that just yet (due to RDK using 7.60)
    //
    // therefore we'll do this via string manipulation by extracting the characters
    // between the // and the /
    //

    // find the leading '//'
    //
    char *start = strstr(urlStr, "//");
    if (start == NULL)
    {
        // bad url
        //
        return NULL;
    }
    start += 2;

    // find the next '/'
    //
    char *end = strchr(start, '/');
    if (end == NULL)
    {
        // format is probably "https://hostname"
        //
        return strdup(start);
    }

    // calculate the difference between start & end so we create the return string
    //
    int len = 0;
    char *ptr = start;
    while (ptr != end && ptr != NULL)
    {
        len++;
        ptr++;
    }
    char *retVal = calloc(len + 1, sizeof(char));
    strncpy(retVal, start, len);
    return retVal;
}

/*
 * returns if VERIFY_HOST is possible on the supplied url string.  this is a simple check to
 * handle "IP Address" based url strings as those cannot be used in a VERIFY_HOST situation.
 */
bool urlHelperCanVerifyHost(const char *urlStr)
{
    // this needs to handle a variety of scenarios:
    //    https://hostname/
    //    https://hostname:port/
    //    https://ipv4/
    //    https://ipv4:port/
    //    https://ipv6/
    //    https://ipv6:port/
    //

    // first, extract the host from the url
    //
    char *hostname = extractHostFromUrl(urlStr);
    if (hostname == NULL)
    {
        return false;
    }

    // ignore the optional ":port" and see if this is an IP address.
    // the easy one is ipv6 because it will have more then 1 colon char in the hostname
    //
    icLogTrace(LOG_TAG, "checking if %s is an ip address", hostname);
    char *firstColon = strchr(hostname, ':');
    if (firstColon != NULL)
    {
        // it could just be the optional port, sp look for a second colon
        //
        char *endColon = strrchr(hostname, ':');
        if (endColon != firstColon)
        {
            // got more then 1 colon, so assume IPv6
            //
            icLogDebug(LOG_TAG, "it appears %s is an IPv6 address; unable to use SSL_VERIFY_HOST", hostname);
            free(hostname);
            return false;
        }
    }

    // if we got here, it's not IPv6, so run a simple regex check for [0-9]* and a '.'
    //
    int rc = 0;
    regex_t exp;

    // simple regex check to see if the url starts with 'https://' and numbers.  ex: 'https://12.'
    // compile the pattern into an regular expression
    //
    if ((rc = regcomp(&exp, "^[0-9]*\\.", REG_ICASE | REG_NOSUB)) != 0)
    {
        // log the reason the regex didn't compile
        //
        char buffer[256];
        regerror(rc, &exp, buffer, sizeof(buffer));
        icLogError(LOG_TAG, "unable to parse regex pattern %s", buffer);
        free(hostname);
        return false;
    }

    // do the comparison
    //
    bool retVal = true;
    if (regexec(&exp, hostname, 0, NULL, 0) == 0)
    {
        // matched
        //
        icLogDebug(LOG_TAG, "it appears %s is an IPv4 address; unable to use SSL_VERIFY_HOST", hostname);
        retVal = false;
    }

    // cleanup
    //
    regfree(&exp);
    free(hostname);
    return retVal;
}

static inline CURL *urlHelperCreateCurl(void)
{
    pthread_once(&initOnce, urlHelperInit);
    return curl_easy_init();
}

static inline void urlHelperDestroyCurl(CURL *ctx)
{
    curl_easy_cleanup(ctx);
}

static const char curlInfoTypes[CURLINFO_END][3] = {"* ", "< ", "> ", "{ ", "} ", "{ ", "} "};

static int curlDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *debugCtx)
{
    switch (type)
    {
        case CURLINFO_TEXT:
        case CURLINFO_HEADER_IN:
        case CURLINFO_HEADER_OUT:
            /* cURL data will have a linefeed that icLog* will also write */
            icLogTrace(LOG_TAG, "cURL: %s%.*s", curlInfoTypes[type], (int) size - 1, data);
            break;

        default:
            break;
    }

    return 0;
}

MimeFileInfo *createMimeFileInfo()
{
    MimeFileInfo *retVal = (MimeFileInfo *) calloc(1, sizeof(MimeFileInfo));

    return retVal;
}

void destroyMimeFileInfo(MimeFileInfo *fileInfo)
{
    if (fileInfo == NULL)
    {
        return;
    }

    if (fileInfo->partName != NULL)
    {
        free(fileInfo->partName);
    }
    if (fileInfo->contentType != NULL)
    {
        free(fileInfo->contentType);
    }
    if (fileInfo->localFilePath != NULL)
    {
        free(fileInfo->localFilePath);
    }
    if (fileInfo->remoteFileName != NULL)
    {
        free(fileInfo->remoteFileName);
    }

    free(fileInfo);
}

MimePartInfo *createMimePartInfo()
{
    MimePartInfo *retVal = (MimePartInfo *) calloc(1, sizeof(MimePartInfo));

    return retVal;
}

void destroyMimePartInfo(MimePartInfo *partInfo)
{
    if (partInfo == NULL)
    {
        return;
    }

    if (partInfo->partName != NULL)
    {
        free(partInfo->partName);
    }
    if (partInfo->partData != NULL)
    {
        free(partInfo->partData);
    }

    free(partInfo);
}

/*
 * Appends given data as MIME file info to
 * provided MIME file info list
 * @param fileInfoList The list where file info is to be appended
 * @param partName The name of the part for this file
 * @param remoteFileName The name to use for the file on the remote side (Optional)
 * @param localFilePath The path to the local file
 * @param contentType The content type of the file (ex. "text/plain", "application/x-tar-gz", etc.) (Optional)
 * @return true if all the input params were non-NULL, false otherwise
 */
bool appendMimeFileInfoToList(icLinkedList *fileInfoList,
                              const char *partName,
                              const char *remoteFileName,
                              const char *localFilePath,
                              const char *contentType)
{
    if (fileInfoList == NULL || partName == NULL || localFilePath == NULL)
    {
        return false;
    }

    MimeFileInfo *mimeFileInfo = createMimeFileInfo();
    mimeFileInfo->partName = strdup(partName);
    mimeFileInfo->remoteFileName = strdupOpt(remoteFileName);
    mimeFileInfo->localFilePath = strdup(localFilePath);
    mimeFileInfo->contentType = strdupOpt(contentType);
    linkedListAppend(fileInfoList, mimeFileInfo);

    return true;
}

/*
 * Appends given data as MIME part info to
 * provided MIME properties list
 * @param fileInfoList The list where file info is to be appended
 * @param partName The name of the part for this file
 * @param partData A string representation of data to be the body of a part
 * @return true if all the input params were non-NULL, false otherwise
 */
bool appendMimePartInfoToList(icLinkedList *props, const char *partName, const char *partData)
{
    if (props == NULL || partName == NULL || partData == NULL)
    {
        return false;
    }

    MimePartInfo *mimePartInfo = createMimePartInfo();
    mimePartInfo->partName = strdup(partName);
    mimePartInfo->partData = strdup(partData);
    mimePartInfo->dataLength = strlen(mimePartInfo->partData);
    linkedListAppend(props, mimePartInfo);

    return true;
}

mtlsCertInfo *createMtlsCertInfo()
{
    mtlsCertInfo *info = (mtlsCertInfo *) calloc(1, sizeof(mtlsCertInfo));

    return info;
}

void destroyMtlsCertificates(mtlsCertInfo *certInfo)
{
    if (certInfo == NULL)
    {
        return;
    }

    if (certInfo->cert != NULL)
    {
        free(certInfo->cert);
    }
    if (certInfo->key != NULL)
    {
        free(certInfo->key);
    }

    free(certInfo);
}
