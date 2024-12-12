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
// Created by einfochips
//

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>
#include <icTypes/sbrm.h>
#include <icUtil/fileUtils.h>
#include <icUtil/md5.h>
#include <icUtil/stringUtils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char templateTempDir[255] = "/tmp/testDirXXXXXX";
static char *tmpDir = NULL;
static char *sourcePath = NULL;
static char *destPath = NULL;
static const char *textToBeWritten = "Test CopyFile Functionality";
static const char *srcFileChecksum = "5102fded5245de1595a5a5caaef642ce";
static int sizeOfString = 0;
static char *getFileMd5(const char *path);

typedef enum
{
    pass,
    fail,
    real
} testCaseResult;

void reloadTestStringSize()
{
    sizeOfString = strlen(textToBeWritten);
}

int __real_fsync(int __fd);
int __wrap_fsync(int __fd)
{
    testCaseResult value = mock();
    switch (value)
    {
        case pass:
        {
            return 0;
        };
        case fail:
        {
            return -1;
        };
        case real:
        {
            int real = __real_fsync(__fd);
            return real;
        }
    }
    return 0;
}

int __real_fflush(FILE *__stream);
int __wrap_fflush(FILE *__stream)
{
    testCaseResult value = mock();
    switch (value)
    {
        case pass:
        {
            return 0;
        };
        case fail:
        {
            return -1;
        };
        case real:
        {
            int real = __real_fflush(__stream);
            return real;
        }
    }
    return 0;
}

int __real_feof(FILE *fp);
int __wrap_feof(FILE *fp)
{
    testCaseResult value = mock();
    switch (value)
    {
        case pass:
        {
            return sizeOfString != 0 ? 0 : 1;
        }
        case fail:
        {
            return 1;
        }
        case real:
        {
            int real = __real_feof(fp);
            return real;
        }
    }
    return 0;
}

size_t __real_fread(void *__ptr, size_t __size, size_t __nmemb, FILE *__stream);
size_t __wrap_fread(void *__ptr, size_t __size, size_t __nmemb, FILE *__stream)
{
    testCaseResult value = mock();
    switch (value)
    {
        case pass:
        {
            return sizeOfString;
        };
        case fail:
        {
            return 0;
        };
        case real:
        {
            ssize_t real = __real_fread(__ptr, __size, __nmemb, __stream);
            return real;
        }
    }
    return 0;
}

size_t __real_fwrite(const void *__ptr, size_t __size, size_t __count, FILE *__stream);
size_t __wrap_fwrite(const void *__ptr, size_t __size, size_t __count, FILE *__stream)
{
    testCaseResult value = mock();
    switch (value)
    {
        case pass:
        {
            return sizeOfString--;
        };
        case fail:
        {
            return 0;
        };
        case real:
        {
            ssize_t real = __real_fwrite(__ptr, __size, __count, __stream);
            return real;
        }
    }
    return 0;
}

FILE *__real_fopen(const char *__pathname, const char *__mode);
FILE *__wrap_fopen(const char *__pathname, const char *__mode)
{
    testCaseResult value = mock();
    switch (value)
    {
        case fail:
        {
            return NULL;
        };
        case pass:
        case real:
        {
            FILE *real = __real_fopen(__pathname, __mode);
            return real;
        }
    }
    return 0;
}

int __real_ferror(FILE *file);
int __wrap_ferror(FILE *file)
{
    testCaseResult value = mock();

    switch (value)
    {
        case fail:
            return 1;

        case pass:
            return 0;

        case real:
            return __real_ferror(file);

        default:
            fail_msg("unhandled ferror mock return %d", value);
            return 1;
    }
}

/*
 * read the contents of a local file
 * and produce an md5sum of the local file.  the caller is
 * responsible for releasing the returned string
 */
static char *getFileMd5(const char *path)
{
    char *retVal = NULL;
    AUTO_CLEAN(free_generic__auto) char *content = readFileContents(path);
    if (content != NULL)
    {
        retVal = icMd5sum(content);
    }

    return retVal;
}

static void writeContentsToTestFile(testCaseResult value)
{
    if (value == pass)
    {
        reloadTestStringSize();
    }

    will_return(__wrap_fopen, value);
    will_return(__wrap_fsync, value);
    will_return(__wrap_fflush, value);
    will_return(__wrap_fwrite, value);
    writeContentsToFileName(sourcePath, textToBeWritten);
}

static int test_testSetup(void **state)
{
    remove(destPath);
    // write content to source file
    writeContentsToTestFile(pass);
    (void) state;
    return 0;
}

static int test_testSetup_real(void **state)
{
    // write content to source file
    writeContentsToTestFile(real);
    (void) state;
    return 0;
}

/*
// *************************
// Tests
// *************************
*/

static void test_copyFileByPath_fopen_fail(void **state)
{
    bool result = false;

    will_return_always(__wrap_fopen, fail);

    // copying file
    result = copyFileByPath(sourcePath, destPath);
    assert_false(doesFileExist(destPath));
    assert_false(result);
}

static void test_copyFileByPath_feof_ok(void **state)
{
    bool result = false;

    will_return_always(__wrap_fopen, pass);
    will_return(__wrap_feof, pass);
    will_return(__wrap_fread, pass);
    will_return(__wrap_fwrite, pass);
    will_return(__wrap_feof, fail);
    will_return_always(__wrap_ferror, pass);
    will_return_always(__wrap_fsync, pass);

    // copying file
    result = copyFileByPath(sourcePath, destPath);
    assert_true(doesFileExist(destPath));
    assert_true(result);
}

static void test_copyFileByPath_fread_fail(void **state)
{
    bool result = false;

    will_return_count(__wrap_fopen, pass, 2);
    will_return_always(__wrap_feof, pass);
    will_return_always(__wrap_ferror, fail);
    will_return_always(__wrap_fread, fail);
    will_return(__wrap_fflush, pass);

    // copying file
    result = copyFileByPath(sourcePath, destPath);
    assert_false(doesFileExist(destPath));
    assert_false(result);
}

static void test_copyFileByPath_fwrite_fail(void **state)
{
    bool result = false;

    will_return_count(__wrap_fopen, pass, 2);
    will_return_always(__wrap_feof, pass);
    will_return_always(__wrap_fread, pass);
    will_return(__wrap_ferror, pass);
    will_return(__wrap_fwrite, fail);
    will_return(__wrap_fflush, pass);

    // copying file
    result = copyFileByPath(sourcePath, destPath);
    assert_false(doesFileExist(destPath));
    assert_false(result);
}

static void test_copyFileByPath_fsync_fail(void **state)
{
    bool result = false;

    will_return_count(__wrap_fopen, pass, 2);
    will_return_always(__wrap_feof, pass);
    will_return_always(__wrap_fread, pass);
    will_return_always(__wrap_fwrite, pass);
    will_return_always(__wrap_ferror, pass);
    will_return_always(__wrap_fsync, fail);
    will_return(__wrap_fflush, pass);

    // copying file
    result = copyFileByPath(sourcePath, destPath);
    assert_false(doesFileExist(destPath));
    assert_false(result);
}

static void test_copyFileByPath_real(void **state)
{
    bool result = false;

    will_return_always(__wrap_fopen, real);
    will_return_always(__wrap_feof, real);
    will_return_always(__wrap_ferror, real);
    will_return_always(__wrap_fwrite, real);
    will_return_always(__wrap_fread, real);
    will_return_always(__wrap_fsync, real);

    // pass case
    // copying file
    result = copyFileByPath(sourcePath, destPath);
    assert_true(doesFileExist(destPath));
    assert_true(result);

    // get Md5 checksum for destination file
    AUTO_CLEAN(free_generic__auto) char *destFileChecksum = getFileMd5(destPath);

    result = false;

    // verify md5 checksum of both source and destination file
    if (stringCompare(srcFileChecksum, destFileChecksum, true) == 0)
    {
        result = true;
    }
    assert_true(result);
}

static void test_readFileContentsWithTrim(void **state)
{
    (void) state;

    will_return_always(__wrap_fopen, real);
    will_return_always(__wrap_fread, real);

    static const char *simpleFilePath = RESOURCES_DIR "/simpleFile";
    static const char *simpleFileTrimmedPath = RESOURCES_DIR "/simpleFileTrimmed";

    // This should be the matching contents for both files once read in.
    static const char *simpleFileContents = "apples\n"
                                            "bananas\n"
                                            "cucumbers";

    scoped_generic char *fileContents = readFileContentsWithTrim(simpleFilePath);
    assert_non_null(fileContents);
    assert_string_equal(simpleFileContents, fileContents);

    free(fileContents);
    fileContents = readFileContentsWithTrim(simpleFileTrimmedPath);
    assert_non_null(fileContents);
    assert_string_equal(simpleFileContents, fileContents);
}

static void test_copyBufferSizedFile(void **state)
{
    will_return_always(__wrap_fopen, real);
    will_return_always(__wrap_feof, real);
    will_return_count(__wrap_fwrite, real, 8);
    will_return_always(__wrap_ferror, real);
    will_return_always(__wrap_fread, real);
    will_return_always(__wrap_fsync, real);

    assert_true(copyFileByPath(RESOURCES_DIR "/000d6f000c3fe4a0.bak", destPath));
    assert_true(doesFileExist(destPath));

    scoped_generic char *destFileChecksum = getFileMd5(destPath);
    assert_int_equal(stringCompare("97e726557a11ee42a8a248c4f3c30d7c", destFileChecksum, true), 0);
}

static void test_copyZeroSizeFile(void **state)
{
    will_return_always(__wrap_fopen, real);
    will_return_always(__wrap_feof, real);
    will_return_always(__wrap_ferror, real);
    will_return_always(__wrap_fread, real);
    will_return_always(__wrap_fsync, real);

    assert_true(copyFileByPath(RESOURCES_DIR "/empty", destPath));
    assert_true(doesFileExist(destPath));
    assert_false(doesNonEmptyFileExist(destPath));
}


int main(int argc, const char **argv)
{
    // creating a temp directory
    tmpDir = mkdtemp(templateTempDir);
    assert_non_null(tmpDir);

    // source and destination path
    sourcePath = stringBuilder("%s/%s", tmpDir, "testFile");
    destPath = stringBuilder("%s/%s", tmpDir, "copyFile");

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_copyFileByPath_fopen_fail, test_testSetup),
        cmocka_unit_test_setup(test_copyFileByPath_feof_ok, test_testSetup),
        cmocka_unit_test_setup(test_copyFileByPath_fread_fail, test_testSetup),
        cmocka_unit_test_setup(test_copyFileByPath_fwrite_fail, test_testSetup),
        cmocka_unit_test_setup(test_copyFileByPath_fsync_fail, test_testSetup),
        cmocka_unit_test_setup(test_copyFileByPath_real, test_testSetup_real),
        cmocka_unit_test_setup(test_copyBufferSizedFile, test_testSetup_real),
        cmocka_unit_test_setup(test_copyZeroSizeFile, test_testSetup_real),
        cmocka_unit_test(test_readFileContentsWithTrim),
    };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    /*
     * gcov will call these atexit as part of its reporting, so make sure
     * all wrapped I/O functions stay in real mode for the rest of the program.
     */
    will_return_always(__wrap_fread, real);
    will_return_always(__wrap_fwrite, real);
    will_return_always(__wrap_fopen, real);
    will_return_always(__wrap_feof, real);
    will_return_always(__wrap_fsync, real);
    will_return_always(__wrap_fflush, real);

    // deleting Temp directory
    deleteDirectory(tmpDir);

    free(sourcePath);
    free(destPath);

    return retval;
}
