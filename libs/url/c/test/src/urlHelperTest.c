//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2011. iControl Networks
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


//
// Created by Christian Leithner on 3/7/19.
//

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include <commMgr/commService_ipc.h>
#include <commMgr/commService_pojo.h>
#include <icLog/logging.h>
#include <icSystem/runtimeAttributes.h>
#include <icTypes/icStringBuffer.h>
#include <propsMgr/paths.h>
#include <urlHelper/urlHelper.h>

#define TEST_LOG_TAG    "urlHelperTest"

#define DEFAULT_TIMEOUT 10

/*
 * This test is meant to be done with an instance of zilker running already (as it makes IPC requests). It is largely
 * meant to be a means for testing by inspection, not necessarily pass or fail.
 */
static void test_urlHelperExecuteMultipartRequestHeaders(void **state)
{
    char *url = NULL;
    long httpCode = -1;
    icLinkedList *plainParts = NULL;
    icLinkedList *fileParts = NULL;
    icLinkedList *headerStrings = NULL;
    char *username = NULL;
    char *password = NULL;
    uint32_t timeoutSecs = DEFAULT_TIMEOUT;
    sslVerify verifyFlag = getSslVerifyProperty(SSL_VERIFY_HTTP_FOR_SERVER);
    // For testing ca certificates
    // sslVerify verifyFlag = SSL_VERIFY_BOTH;
    bool allowCellular = false;
    char *result = NULL;

    // Simple test for multipart http post. Values are largely improvised

    // Request some basic values from comm. The rmaBackupConfig struct isn't really made for this, but it conveniently
    //  has all the information we need from comm
    rmaBackupConfig *backupConfig = create_rmaBackupConfig();
    commService_request_GET_RMA_BACKUP_INFO(true, backupConfig);

    url = "https://pds.icontrol.com/";
    username = backupConfig->username;
    password = backupConfig->password;

    int len = snprintf(NULL, 0, "%lu", backupConfig->premiseId);
    char *premiseId = malloc(len + 1);
    snprintf(premiseId, len + 1, "%lu", backupConfig->premiseId);
    char *cpeId = (char *) getSystemCpeId();

    // Construct our parts (just fake parts, tester should step through and inspect if they want to verify the multipart
    //  body is constructed correctly
    plainParts = linkedListCreate();
    MimePartInfo *partInfo = createMimePartInfo();
    partInfo->partName = strdup("cpeId");
    partInfo->partData = strdup(cpeId);
    partInfo->dataLength = strlen(partInfo->partData);
    linkedListAppend(plainParts, partInfo);
    partInfo = createMimePartInfo();
    partInfo->partName = strdup("premiseId");
    partInfo->partData = strdup(premiseId);
    partInfo->dataLength = strlen(partInfo->partData);
    linkedListAppend(plainParts, partInfo);
    partInfo = createMimePartInfo();
    partInfo->partName = strdup("rescue");
    partInfo->partData = strdup("true");
    partInfo->dataLength = strlen(partInfo->partData);
    linkedListAppend(plainParts, partInfo);

    // Use macAddress as a dummy file to use as a file part.
    fileParts = linkedListCreate();
    MimeFileInfo *macFileInfo = createMimeFileInfo();

    char *dynamicConfigPath = getDynamicConfigPath();
    char *macAddrFilename = "/macAddress";
    len = strlen(dynamicConfigPath) + strlen(macAddrFilename);
    char *pathToMacFile = malloc(len + 1);
    snprintf(pathToMacFile, len + 1, "%s%s", dynamicConfigPath, macAddrFilename);
    macFileInfo->partName = strdup("MacAddressFile");
    macFileInfo->localFilePath = pathToMacFile;
    macFileInfo->remoteFileName = strdup("fakeFile");
    macFileInfo->contentType = strdup("text/plain");
    linkedListAppend(fileParts, macFileInfo);

    // Execute the request
    result = urlHelperExecuteMultipartRequestHeaders(url,
                                                     &httpCode,
                                                     plainParts,
                                                     fileParts,
                                                     headerStrings,
                                                     username,
                                                     password,
                                                     timeoutSecs,
                                                     verifyFlag,
                                                     allowCellular);

    // Check response
    assert_int_not_equal(httpCode, -1);
    assert_non_null(result);

    // cleanup
    destroy_rmaBackupConfig(backupConfig);
    linkedListDestroy(plainParts, (linkedListItemFreeFunc) destroyMimePartInfo);
    linkedListDestroy(fileParts, (linkedListItemFreeFunc) destroyMimeFileInfo);
    free(premiseId);
    free(dynamicConfigPath);
    free(result);

    (void) state;
}

static void test_urlHelperCreateQueryString(void **state)
{
    (void) state;

    // This test is a bit weird. We can't do exact assert_string_equals
    // for the result as the order of string hash map members will
    // not be deterministic prior to runtime. Best we can do without
    // reimplementing the exact logic we are using for urlHelperCreateQueryString
    // (which would be circular) is assert_non_null and hand inspect results
    // for confidence.

    // Bad input
    char *queryString = urlHelperCreateQueryString(NULL);
    assert_null(queryString);

    icStringHashMap *kvPairs = stringHashMapCreate();
    queryString = urlHelperCreateQueryString(kvPairs);
    assert_null(queryString);

    // No escape needed
    stringHashMapPutCopy(kvPairs, "fruit", "apple");
    stringHashMapPutCopy(kvPairs, "meat", "steak");
    stringHashMapPutCopy(kvPairs, "side", "rice");
    queryString = urlHelperCreateQueryString(kvPairs);

    assert_non_null(queryString);
    stringHashMapClear(kvPairs);

    // Escape needed
    stringHashMapPutCopy(kvPairs, "fruit", "apples and bananas");
    stringHashMapPutCopy(kvPairs, "meat+dairy", "steak+eggs");
    stringHashMapPutCopy(kvPairs, "side", "rice");
    queryString = urlHelperCreateQueryString(kvPairs);
    assert_non_null(queryString);

    stringHashMapDestroy(kvPairs, NULL);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_urlHelperExecuteMultipartRequestHeaders),
                                       cmocka_unit_test(test_urlHelperCreateQueryString)};

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    return retval;
}