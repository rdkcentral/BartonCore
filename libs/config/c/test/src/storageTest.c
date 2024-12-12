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
// Created by tlea on 4/4/18.
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <icConfig/simpleProtectConfig.h>
#include <icConfig/storage.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <jsonHelper/jsonHelper.h>

static char *tempDir = NULL;

static void test_storage_save_load_simple(void **state)
{
    assert_true(storageSave("namespace1", "key1", "value1"));

    char *value = NULL;
    assert_true(storageLoad("namespace1", "key1", &value));
    assert_non_null(value);
    assert_string_equal("value1", value);

    free(value);

    (void) state;
}

static void test_storage_overwrite_value(void **state)
{
    assert_true(storageSave("namespace2", "key2", "value2"));

    char *value = NULL;
    assert_true(storageLoad("namespace2", "key2", &value));
    assert_non_null(value);
    assert_string_equal("value2", value);

    free(value);

    value = NULL;
    assert_true(storageSave("namespace2", "key2", "new_value2"));
    assert_true(storageLoad("namespace2", "key2", &value));
    assert_non_null(value);
    assert_string_equal("new_value2", value);

    free(value);

    (void) state;
}

static void test_storage_delete_value(void **state)
{
    assert_true(storageSave("namespace1", "key3", "value3"));
    assert_true(storageDelete("namespace1", "key3"));

    char *value = NULL;
    assert_false(storageLoad("namespace1", "key3", &value));
    assert_null(value);

    free(value);

    (void) state;
}

static void test_storage_get_keys(void **state)
{
    assert_true(storageSave("namespace3", "key4a", "value4"));
    assert_true(storageSave("namespace3", "key4b", "value4"));
    assert_true(storageSave("namespace3", "key4c", "value4"));

    AUTO_CLEAN(free_generic__auto) char *origFile = stringBuilder("%s/%s/namespace3/key4c", tempDir, getStorageDir());
    AUTO_CLEAN(free_generic__auto) char *bakFile = stringBuilder("%s.bak", origFile);
    assert_true(copyFileByPath(origFile, bakFile));

    icLinkedList *keys = storageGetKeys("namespace3");
    assert_non_null(keys);
    assert_int_equal(linkedListCount(keys), 3);

    icLinkedListIterator *it = linkedListIteratorCreate(keys);
    bool foundKey4a = false;
    bool foundKey4b = false;
    bool foundKey4c = false;
    while (linkedListIteratorHasNext(it))
    {
        char *key = (char *) linkedListIteratorGetNext(it);
        if (strcmp(key, "key4a") == 0)
            foundKey4a = true;
        if (strcmp(key, "key4b") == 0)
            foundKey4b = true;
        if (strcmp(key, "key4c") == 0)
            foundKey4c = true;
    }
    linkedListIteratorDestroy(it);
    linkedListDestroy(keys, NULL);

    assert_true(foundKey4a && foundKey4b && foundKey4c);

    (void) state;
}

// if the main file for a storage item is missing, but there is a .bak, it should still appear in keys
static void test_storage_get_keys_with_bak_only(void **state)
{
    storageDeleteNamespace("namespace3"); // clean up anything previous
    assert_true(storageSave("namespace3", "key5a", "value5"));
    assert_true(storageSave("namespace3", "key5b", "value5"));
    assert_true(storageSave("namespace3", "key5c", "value5"));

    const char *storageDir = getStorageDir();
    char buf1[255];
    char buf2[255];
    sprintf(buf1, "%s/%s/namespace3/key5b", tempDir, storageDir);
    sprintf(buf2, "%s/%s/namespace3/key5b.bak", tempDir, storageDir);
    assert_int_equal(rename(buf1, buf2), 0);

    icLinkedList *keys = storageGetKeys("namespace3");
    assert_non_null(keys);
    assert_int_equal(linkedListCount(keys), 3);

    icLinkedListIterator *it = linkedListIteratorCreate(keys);
    bool foundKey5a = false;
    bool foundKey5b = false;
    bool foundKey5c = false;
    while (linkedListIteratorHasNext(it))
    {
        char *key = (char *) linkedListIteratorGetNext(it);
        if (strcmp(key, "key5a") == 0)
            foundKey5a = true;
        if (strcmp(key, "key5b") == 0)
            foundKey5b = true;
        if (strcmp(key, "key5c") == 0)
            foundKey5c = true;
    }
    linkedListIteratorDestroy(it);
    linkedListDestroy(keys, NULL);

    assert_true(foundKey5a && foundKey5b && foundKey5c);

    (void) state;
}

static void test_storage_delete_namespace(void **state)
{
    assert_true(storageSave("namespace1", "key5", "value5"));
    assert_true(storageDeleteNamespace("namespace1"));

    char *value = NULL;
    assert_false(storageLoad("namespace1", "key5", &value));
    assert_null(value);

    icLinkedList *keys = storageGetKeys("namespace1");
    assert_null(keys);

    linkedListDestroy(keys, NULL);
    free(value);

    (void) state;
}

static void test_storage_namespace_safety(void **state)
{
    assert_true(storageSave("namespace1", "key6", "value6"));
    assert_true(storageSave("namespace2", "key6", "other_value6"));

    char *value1 = NULL;
    assert_true(storageLoad("namespace1", "key6", &value1));
    assert_non_null(value1);

    char *value2 = NULL;
    assert_true(storageLoad("namespace2", "key6", &value2));
    assert_non_null(value2);

    assert_string_not_equal(value1, value2);

    free(value1);
    free(value2);

    (void) state;
}

static void test_storage_namespace_restore(void **state)
{
    /*
     * Create our junk namespace and populate with several key/values.
     * These should be destroyed on the restore.
     */
    assert_true(storageSave("namespace10", "key1", "value1"));
    assert_true(storageSave("namespace10", "key2", "value2"));
    assert_true(storageSave("namespace10", "key3", "value3"));

    char *restoreDir = strdup("/tmp/storageRestoreTest_XXXXXX");
    assert_non_null(mkdtemp(restoreDir));

    char cmd[1024]; // Stupid large as we don't really care.

    /*
     * Create our temporary configuration restore point and contents.
     */
    snprintf(cmd, 1024, "mkdir -p %s/%s/namespace10", restoreDir, getStorageDir());
    assert_int_equal(system(cmd), 0);

    snprintf(cmd, 1024, "echo -n \"value4\" > %s/%s/namespace10/key4", restoreDir, getStorageDir());
    assert_int_equal(system(cmd), 0);

    snprintf(cmd, 1024, "echo -n \"value5\" > %s/%s/namespace10/key5", restoreDir, getStorageDir());
    assert_int_equal(system(cmd), 0);

    assert_true(storageRestoreNamespace("namespace10", restoreDir) == STORAGE_RESTORE_ERROR_NONE);

    /*
     * Get all the restored keys and make sure they are
     * the restored configuration.
     */
    icLinkedList *list = storageGetKeys("namespace10");
    assert_non_null(list);
    assert_int_equal(linkedListCount(list), 2);

    icLinkedListIterator *iterator = linkedListIteratorCreate(list);
    while (linkedListIteratorHasNext(iterator))
    {
        const char *key = linkedListIteratorGetNext(iterator);
        char *value = NULL;

        assert_true(storageLoad("namespace10", key, &value));
        assert_non_null(value);

        if (strcmp(key, "key4") == 0)
        {
            assert_string_equal(value, "value4");
        }
        else if (strcmp(key, "key5") == 0)
        {
            assert_string_equal(value, "value5");
        }
        else
        {
            fail_msg("Invalid key found [%s]", key);
        }

        free(value);
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(list, NULL);

    /*
     * Cleanup the temporary restore point.
     */
    snprintf(cmd, 1024, "rm -rf %s", restoreDir);
    assert_int_equal(system(cmd), 0);

    /*
     * Destroy our junk namespace.
     */
    assert_true(storageDeleteNamespace("namespace10"));

    free(restoreDir);

    (void) state;
}

static void test_simple_protect(void **state)
{
    // encrypt a string using the 'simple' way
    //
    char *encoded = simpleProtectConfigData("protect", "data I wish to protect");
    assert_non_null(encoded);

    // decrypt what was encrpyted
    char *original = simpleUnprotectConfigData("protect", encoded);
    int compare = strcmp(original, "data I wish to protect");
    assert_true(compare == 0);

    // cleanup
    free(encoded);
    free(original);
}

static void test_load_valid_json(void **state)
{
    const char *validJson = "{\"json\":true}";
    AUTO_CLEAN(cJSON_Delete__auto) cJSON *fixture = cJSON_Parse(validJson);
    assert_true(storageSave("namespace1", "validJSON", validJson));

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *value = storageLoadJSON("namespace1", "validJSON");
    assert_non_null(value);
    assert_true(cJSON_Compare(fixture, value, true));
}

static void test_load_invalid_json(void **state)
{
    const char *badJSON = "{json:false}";
    assert_true(storageSave("namespace1", "badJSON", badJSON));

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *value = storageLoadJSON("namespace1", "badJSON");
    assert_null(value);
}

static void test_load_valid_xml(void **state)
{
    const char *validXML = "<?xml version=\"1.0\" encoding=\"utf-8\"?><testElement/>";
    assert_true(storageSave("namespace1", "validXML", validXML));

    xmlDoc *parsed = storageLoadXML("namespace1", "validXML", NULL, 0);
    assert_non_null(parsed);
    assert_non_null(parsed->children);
    assert_int_equal(parsed->children->type, XML_ELEMENT_NODE);
    assert_string_equal(parsed->children->name, "testElement");

    xmlFreeDoc(parsed);
}

static void test_load_invalid_xml(void **state)
{
    const char *badXML = "<notgood>";
    assert_true(storageSave("namespace1", "badXML", badXML));
    assert_null(storageLoadXML("namespace1", "badXML", NULL, 0));
}


static void test_load_backup(void **state)
{
    AUTO_CLEAN(free_generic__auto) char *mainPath = stringBuilder("%s/%s/namespace1/key1", tempDir, getStorageDir());

    storageSave("namespace1", "key1", "key1data");

    /* Ovewrite to make a backup - this will be "lost" */
    storageSave("namespace1", "key1", "key1data2");
    assert_int_equal(unlink(mainPath), 0);

    AUTO_CLEAN(free_generic__auto) char *key1data = NULL;
    assert_true(storageLoad("namespace1", "key1", &key1data));

    assert_int_equal(access(mainPath, F_OK), 0);
    assert_string_equal(key1data, "key1data");
}

static bool testParser(const char *data, void *parseCtx)
{
    return (bool) parseCtx;
}

static void test_parse_bad_failover(void **state)
{
    scoped_generic char *mainPath = stringBuilder("%s/%s/namespace1/key1", tempDir, getStorageDir());
    scoped_generic char *badPath = stringBuilder("%s/%s/namespace1/key1.bad", tempDir, getStorageDir());
    scoped_generic char *backupPath = stringBuilder("%s/%s/namespace1/key1.bak", tempDir, getStorageDir());

    storageSave("namespace1", "key1", "key1data");

    StorageCallbacks angryCb = {.parse = testParser, .parserCtx = (void *) false};
    StorageCallbacks recoverCb = {.parse = testParser, .parserCtx = (void *) true};

    assert_true(deleteFile(backupPath));

    assert_false(storageParse("namespace1", "key1", &angryCb));
    assert_int_not_equal(access(mainPath, F_OK), 0);
    assert_int_equal(access(badPath, F_OK), 0);

    assert_true(storageParseBad("namespace1", "key1", &recoverCb));

    scoped_generic char *key1Data = NULL;
    assert_true(storageLoad("namespace1", "key1", &key1Data));
    assert_string_equal(key1Data, "key1data");

    /* Recovery is meant to keep a pristine copy of the bad file for analysis (the main can be updated many times) */
    assert_int_equal(access(badPath, F_OK), 0);
}

int runStorageTest()
{
    tempDir = strdup("/tmp/storageTest_XXXXXX");
    mkdtemp(tempDir);
    storageSetConfigPath(tempDir);

    const struct CMUnitTest tests[] = {cmocka_unit_test(test_storage_save_load_simple),
                                       cmocka_unit_test(test_storage_overwrite_value),
                                       cmocka_unit_test(test_storage_delete_value),
                                       cmocka_unit_test(test_storage_get_keys),
                                       cmocka_unit_test(test_storage_get_keys_with_bak_only),
                                       cmocka_unit_test(test_storage_delete_namespace),
                                       cmocka_unit_test(test_storage_namespace_safety),
                                       cmocka_unit_test(test_storage_namespace_restore),
                                       cmocka_unit_test(test_simple_protect),
                                       cmocka_unit_test(test_load_valid_json),
                                       cmocka_unit_test(test_load_invalid_json),
                                       cmocka_unit_test(test_load_valid_xml),
                                       cmocka_unit_test(test_load_invalid_xml),
                                       cmocka_unit_test(test_load_backup),
                                       cmocka_unit_test(test_parse_bad_failover)};

    int rc = cmocka_run_group_tests(tests, NULL, NULL);

    // clean up our stuff
    storageDeleteNamespace("namespace1");
    storageDeleteNamespace("namespace2");
    storageDeleteNamespace("namespace3");
    storageDeleteNamespace("protect");
    const char *storageDir = getStorageDir();
    char buf[255];
    sprintf(buf, "%s/%s", tempDir, storageDir);
    assert_int_equal(rmdir(buf), 0);
    assert_int_equal(rmdir(tempDir), 0);
    free(tempDir);

    return rc;
}
