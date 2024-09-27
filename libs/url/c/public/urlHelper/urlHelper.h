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
 * Created by Thomas Lea on 8/13/15.
 */

#ifndef FLEXCORE_URLHELPER_H
#define FLEXCORE_URLHELPER_H

#include <stdint.h>
#include <curl/curl.h>

#include <icTypes/icStringHashMap.h>
#include <icTypes/icLinkedList.h>

/*
 * list of verify categories
 */
typedef enum
{
    SSL_VERIFY_CATEGORY_FIRST = 0,
    SSL_VERIFY_HTTP_FOR_SERVER = SSL_VERIFY_CATEGORY_FIRST,
    SSL_VERIFY_HTTP_FOR_DEVICE,
    SSL_VERIFY_CATEGORY_LAST = SSL_VERIFY_HTTP_FOR_DEVICE
} sslVerifyCategory;

/*
 * enumeration of possible verify values
 */
typedef enum
{
    SSL_VERIFY_INVALID = -1,
    SSL_VERIFY_NONE,
    SSL_VERIFY_HOST,
    SSL_VERIFY_PEER,
    SSL_VERIFY_BOTH,
} sslVerify;

/**
 * Try to set a cURL option and complain with an error if it fails
 */
#define curlSetopt(curl, opt, ...)                                              \
do                                                                              \
{                                                                               \
    CURLcode err;                                                               \
    if ((err = curl_easy_setopt((curl), (opt), (__VA_ARGS__))) != CURLE_OK)     \
    {                                                                           \
        icLogError(LOG_TAG, "curl_easy_setopt(curl, %s, %s) "                   \
                            "failed at %s(%d): %s",                             \
                   #opt,                                                        \
                   #__VA_ARGS__,                                                \
                   __FILE__,                                                    \
                   __LINE__,                                                    \
                   curl_easy_strerror(err));                                    \
    }                                                                           \
} while (0)

//A struct for encapsulating information about file data for multipart http posts
typedef struct
{
    char *partName;         //The name of the part for this file
    char *localFilePath;    //The path to the local file
    char *remoteFileName;   //The name to use for the file on the remote side
    char *contentType;      //The content type of the file (ex. "text/plain", "application/x-tar-gz", etc.)
} MimeFileInfo;

MimeFileInfo *createMimeFileInfo();
void destroyMimeFileInfo(MimeFileInfo *fileInfo);

//A struct for encapsulating information about regular part data for multipart http posts
typedef struct
{
    char *partName;         //The name of a part
    char *partData;         //A string representation of data to be the body of a part
    size_t dataLength;      //The length of the data block (curl expects size_t)
} MimePartInfo;

MimePartInfo *createMimePartInfo();
void destroyMimePartInfo(MimePartInfo *partInfo);

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
                              const char *contentType);

/*
 * Appends given data as MIME part info to
 * provided MIME properties list
 * @param fileInfoList The list where file info is to be appended
 * @param partName The name of the part for this file
 * @param partData A string representation of data to be the body of a part
 * @return true if all the input params were non-NULL, false otherwise
 */
bool appendMimePartInfoToList(icLinkedList *props,
                              const char *partName,
                              const char *partData);


typedef enum {
    CERT_TYPE_PEM,
    CERT_TYPE_P12,
    CERT_TYPE_DER,
    CERT_TYPE_ENG
} mtlsCertType;

static const char *mtlsCertTypeStrings[] = {
        "PEM", // CERT_TYPE_PEM
        "P12", // CERT_TYPE_P12
        "DER", // CERT_TYPE_DER
        "ENG"  // CERT_TYPE_ENG
};

typedef struct
{
    char *cert;
    mtlsCertType certType;
    char *key;
    mtlsCertType keyType;
} mtlsCertInfo;

mtlsCertInfo *createMtlsCertInfo();

void destroyMtlsCertificates(mtlsCertInfo *certInfo);

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
char* urlHelperExecuteVariableRequest(char* requestUrl,
                                      icStringHashMap* variableMap,
                                      long* httpCode,
                                      char* postData,
                                      const char* username,
                                      const char* password,
                                      uint32_t timeoutSecs,
                                      sslVerify verifyFlag,
                                      bool allowCellular);

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
char* urlHelperExecuteRequest(const char* url,
                              long* httpCode,
                              const char* postData,
                              const char* username,
                              const char* password,
                              uint32_t timeoutSecs,
                              sslVerify verifyFlag,
                              bool allowCellular);

/*
 * same as urlHelperExecuteRequest, but allow for assigning HTTP headers in the request.
 * For ex:  Accept: application/json
 *
 * @param headers - list of strings that define header values.  ex: "Content-Type: text/xml; charset=utf-8".  ignored if NULL
 */
char* urlHelperExecuteRequestHeaders(const char* url,
                                     long* httpCode,
                                     const char* postData,
                                     icLinkedList* headerStrings,
                                     const char* username,
                                     const char* password,
                                     uint32_t timeoutSecs,
                                     sslVerify verifyFlag,
                                     bool allowCellular);

/*
 * same as urlHelperExecuteMultipartRequestHeaders, but without headers
 */
char *urlHelperExecuteMultipartRequest(const char* url,
                                       long* httpCode,
                                       icLinkedList* plainParts,
                                       icLinkedList* fileInfo,
                                       const char* username,
                                       const char* password,
                                       uint32_t timeoutSecs,
                                       sslVerify verifyFlag,
                                       bool allowCellular);

/*
 * same as urlHelperExecuteMTLSRequestHeaders, but without headers
 */
char *urlHelperExecuteMTLSRequest(const char* url,
                                         long* httpCode,
                                         const char* postData,
                                         const char* username,
                                         const char* password,
                                         uint32_t timeoutSecs,
                                         const mtlsCertInfo *certInfo,
                                         bool allowCellular);

/*
 * Execute a request to the provided URL and return the body. If username and password
 * are provided, basic authentication will be used.
 * Caller should free the return string (if not NULL)
 *
 * @param url - the URL to retrieve
 * @param httpCode - the result code from the server for the HTTP request
 * @param postData - text to use for an HTTP POST operation or NULL to use a standard HTTP GET
 * @param headerStrings - - list of strings that define header values.  ex: "Content-Type: text/xml; charset=utf-8".  ignored if NULL
 * @param username - the username to use for basic authentication or NULL for none
 * @param password - the password to use for basic authentication or NULL for none
 * @param timeoutSecs - number of seconds to timeout (0 means no timeout set)
 * @param certInfo - object containing certificate information - ignored if NULL
 * @param allowCellular - A flag to indicate if this request should be sent out over cell iff it cannot be sent
 *                        over bband
 *
 * @returns the body of the response.  Caller frees.
 */
char *urlHelperExecuteMTLSRequestHeaders(const char* url,
                                  long* httpCode,
                                  const char* postData,
                                  icLinkedList* headerStrings,
                                  const char* username,
                                  const char* password,
                                  uint32_t timeoutSecs,
                                  const mtlsCertInfo *certInfo,
                                  bool allowCellular);

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
char *urlHelperExecuteMultipartRequestHeaders(const char* url,
                                              long* httpCode,
                                              icLinkedList* plainParts,
                                              icLinkedList* fileInfo,
                                              icLinkedList* headerStrings,
                                              const char* username,
                                              const char* password,
                                              uint32_t timeoutSecs,
                                              sslVerify verifyFlag,
                                              bool allowCellular);

/**
 * Helper routine to download a file into a specified
 * location.
 *
 * @note: the pathname provided will be created and/or zeroed out in
 * nearly all situations. Callers are responsible for not accidentally
 * overwriting existing files if they do not mean to.
 *
 * @param url The URL pointing to data to download
 * @param httpCode The returned HTTP response code
 * @param username The username to authorize against
 * @param password The password to use in authorization
 * @param timeoutSecs The number of seconds before timing out.
 * @param verifyFlag SSL verification turned on or off.
 * @param allowCellular Allow this download over a cellular connection
 * @param pathname The path and file name to store the file into.
 * @return The number of bytes written into the file.
 */
size_t urlHelperDownloadFile(const char* url,
                             long* httpCode,
                             const char* username,
                             const char* password,
                             uint32_t timeoutSecs,
                             sslVerify verifyFlag,
                             bool allowCellular,
                             const char* pathname);

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
char* urlHelperUploadFile(const char* url,
                          long *httpCode,
                          const char* username,
                          const char* password,
                          uint32_t timeoutSecs,
                          sslVerify verifyFlag,
                          bool allowCellular,
                          const char* fileName);

/*
 * apply standard options to a Curl Context.
 * if the 'url' is not null, this will add it to the context.
 * additionally, if the 'verifyFlag' includes VERIFY_HOST or VERIFY_BOTH, then the
 * 'url' will be checked for IP Addresses, and if so remove VERIFY_HOST from the mix
 */
void applyStandardCurlOptions(CURL *context, const char *url, uint32_t timeoutSecs, const mtlsCertInfo *certInfo,
                              sslVerify verifyFlag, bool allowCellular);

/*
 * apply the user agent option if we have a user agent defined
 */
void applyUserAgentCurlOptionIfAvailable(CURL *context);

/*
 * returns if VERIFY_HOST is possible on the supplied url string.  this is a simple check to
 * handle "IP Address" based url strings as those cannot be used in a VERIFY_HOST situation.
 */
bool urlHelperCanVerifyHost(const char *urlStr);

/**
 * Cancel a transfer by URL. If the transfer is not active,
 * the next request will be aborted immediately.
 * @param enabled
 */
void urlHelperCancel(const char *url);

/**
 * Creates a query string based on provided keyValuePairs in the form of
 * "?key1=value1&key2=value2&..."
 * Note the keys and values will be url encoded.
 *
 * @param keyValuePairs - map of string key/value pairs to use for the query string
 * @return a valid url query string starting with '?' with url encoded members, or NULL on error.
 */
char *urlHelperCreateQueryString(const icStringHashMap *keyValuePairs);

/**
 * Creates a URL encoded form-data based on provided keyValuePairs in the form of
 * "key1=value1&key2=value2&..."
 * Note the keys and values will be url encoded.
 *
 * @param keyValuePairs - map of string key/value pairs to use for the query string
 * @return a valid url URL encoded form-data, or NULL on error.
 */
char *urlHelperCreateUrlEncodedForm(const icStringHashMap *keyValuePairs);


#endif //FLEXCORE_URLHELPER_H
