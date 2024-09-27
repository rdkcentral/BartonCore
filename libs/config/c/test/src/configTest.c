//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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

/*-----------------------------------------------
 * configTest.c
 *
 * test utility for libicConfig.  also serves as an
 * example for protecting portions of the file.
 *
 * Author: jelderton -  8/30/16.
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <icLog/logging.h>
#include <icUtil//fileUtils.h>
#include <icUtil/stringUtils.h>
#include <icConfig/backupUtils.h>
#include "tests.h"

#define CONFIG_TESTS_DIR    "/tmp/cfgTest"
#define CONFIG_TEST_VALID_XML_FILENAME      CONFIG_TESTS_DIR "/valid.xml"
#define CONFIG_TEST_INVALID_XML_FILENAME    CONFIG_TESTS_DIR "/bad.xml"
#define CONFIG_TEST_VALID_JSON_FILENAME     CONFIG_TESTS_DIR "/valid.json"
#define CONFIG_TEST_INVALID_JSON_FILENAME   CONFIG_TESTS_DIR "/bad.json"

static int runConfigTest();

/*
 *
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  configTest <-a|-t>\n");
    fprintf(stderr, "    -a : run all tests\n");
    fprintf(stderr, "    -c : run config test\n");
    fprintf(stderr, "    -l : run legacy-config test\n");
    fprintf(stderr, "    -s : run storage test\n");
    fprintf(stderr, "\n");
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    int c;
    bool doConfig = false;
    bool doLegacy = false;
    bool doStorage = false;

    while ((c = getopt(argc, argv, "aclsh")) != -1)
    {
        switch (c)
        {
            case 'a':
                doConfig = true;
                doLegacy = true;
                doStorage = true;
                break;

            case 'c':
                doConfig = true;
                break;

            case 'l':
                doLegacy = true;
                break;

            case 's':
                doStorage = true;
                break;


            case 'h':
            default:
            {
                printUsage();
                return 1;
            }
        }
    }

    bool didSomething = false;
    if (doConfig == true)
    {
        printf("\n\nRunning Config Tests:\n");
        if (obfuscateTest() == false)
        {
            printf("  Obfuscate test FAILED\n");
            return 1;
        }
        if (testProtectConfig() == false)
        {
            printf("  Protect Test FAILED\n");
            return 1;
        }
        if (runConfigTest() != 0)
        {
            printf("  Config Test FAILED\n");
            return 1;
        }
        printf("  Config Test SUCCESS!\n");
        didSomething = true;
    }
    if (doLegacy == true)
    {
        printf("\n\nRunning Legacy Config Test:\n");
        if (testLegacyProtectConfig() == false)
        {
            printf("  Legacy Config Test FAILED\n");
            return 1;
        }
        printf("  Legacy Config Test SUCCESS!\n");
        didSomething = true;
    }
    if (doStorage == true)
    {
        printf("\n\nRunning Storage Test:\n");
        if (runStorageTest() != 0)
        {
            printf("  Storage Test FAILED\n");
            return 1;
        }
        printf("  Legacy Config Test SUCCESS!\n");
        didSomething = true;
    }

    if (didSomething == false)
    {
        fprintf(stderr, "no options provided, use -h option for help\n");
        return 1;
    }

    // no errors?
    //
    return 0;
}

static int config_test_setup(void **state)
{
    // create our temp dir, and 4 files
    mkdir_p(CONFIG_TESTS_DIR, 0777);

    // create valid XML
    const char *validXmlContent = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><testConfig><rootNode></rootNode></testConfig>";
    writeContentsToFileName(CONFIG_TEST_VALID_XML_FILENAME, validXmlContent);

    // create bad XML
    const char *badXmlContent = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><testConfig><lots of garbage here";
    writeContentsToFileName(CONFIG_TEST_INVALID_XML_FILENAME, badXmlContent);

    // create valid JSON
    const char *validJsonContent = "{ \"a\": \"1234\", \"b\": \"9932\" }";
    writeContentsToFileName(CONFIG_TEST_VALID_JSON_FILENAME, validJsonContent);

    // create bad JSON
    const char *badJsonContent = "{ \"a\": \"1234\", garbage goes here";
    writeContentsToFileName(CONFIG_TEST_INVALID_JSON_FILENAME, badJsonContent);
    return 0;
}

static int config_test_teardown(void **state)
{
    // destroy our temp dir and files
    deleteDirectory(CONFIG_TESTS_DIR);
    return 0;
}

static void test_config_load_xml_valid(void **state)
{
    // ask backupUtil to validate our XML files.  it should chose the valid one (ORIG)
    fileToRead x = chooseValidFileToRead(CONFIG_TEST_VALID_XML_FILENAME,
                                         CONFIG_TEST_INVALID_XML_FILENAME,
                                         CONFIG_FILE_FORMAT_XML);
    assert_int_equal(x, ORIGINAL_FILE);
}

static void test_config_load_xml_invalid(void **state)
{
    // ask backupUtil to validate our XML files.  it should chose the valid one (BACK)
    fileToRead x = chooseValidFileToRead(CONFIG_TEST_INVALID_XML_FILENAME,
                                         CONFIG_TEST_VALID_XML_FILENAME,
                                         CONFIG_FILE_FORMAT_XML);
    assert_int_equal(x, BACKUP_FILE);
}

static void test_config_load_json_valid(void **state)
{
    // ask backupUtil to validate our JSON files.  it should chose the valid one (ORIG)
    fileToRead x = chooseValidFileToRead(CONFIG_TEST_VALID_JSON_FILENAME,
                                         CONFIG_TEST_INVALID_JSON_FILENAME,
                                         CONFIG_FILE_FORMAT_JSON);
    assert_int_equal(x, ORIGINAL_FILE);
}

static void test_config_load_json_invalid(void **state)
{
    // ask backupUtil to validate our JSON files.  it should chose the valid one (BACK)
    fileToRead x = chooseValidFileToRead(CONFIG_TEST_INVALID_JSON_FILENAME,
                                         CONFIG_TEST_VALID_JSON_FILENAME,
                                         CONFIG_FILE_FORMAT_JSON);
    assert_int_equal(x, BACKUP_FILE);
}

/*
 * cmocka style for the "config" tests
 */
static int runConfigTest()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(test_config_load_xml_valid),
        cmocka_unit_test(test_config_load_xml_invalid),
        cmocka_unit_test(test_config_load_json_valid),
        cmocka_unit_test(test_config_load_json_invalid)
    };

    int rc = cmocka_run_group_tests(tests, config_test_setup, config_test_teardown);
    return rc;
}

