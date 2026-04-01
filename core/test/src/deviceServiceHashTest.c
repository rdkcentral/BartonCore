//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>
#include <glib.h>

#include "../../src/deviceServiceHash.h"

static char *tmpDir = NULL;

/* ---- fread / ferror wrappers for fault injection ---- */

extern size_t __real_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int __real_ferror(FILE *stream);

static bool faultInjectionActive = false;

static int faultInjectionSetup(void **state)
{
    faultInjectionActive = true;

    return 0;
}

static int faultInjectionTeardown(void **state)
{
    faultInjectionActive = false;

    return 0;
}

size_t __wrap_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (faultInjectionActive)
    {
        bool passthrough = mock_type(bool);

        if (!passthrough)
        {
            return mock_type(size_t);
        }
    }

    return __real_fread(ptr, size, nmemb, stream);
}

int __wrap_ferror(FILE *stream)
{
    if (faultInjectionActive)
    {
        return mock_type(int);
    }

    return __real_ferror(stream);
}

static int groupSetup(void **state)
{
    tmpDir = g_dir_make_tmp("deviceServiceHashTest-XXXXXX", NULL);
    assert_non_null(tmpDir);

    return 0;
}

static int groupTeardown(void **state)
{
    if (tmpDir != NULL)
    {
        // Clean up any leftover files
        g_autoptr(GDir) dir = g_dir_open(tmpDir, 0, NULL);

        if (dir != NULL)
        {
            const char *name;

            while ((name = g_dir_read_name(dir)) != NULL)
            {
                g_autofree char *path = g_build_filename(tmpDir, name, NULL);
                unlink(path);
            }
        }

        rmdir(tmpDir);
        g_free(tmpDir);
    }

    return 0;
}

static char *writeTestFile(const char *name, const void *data, size_t len)
{
    char *path = g_build_filename(tmpDir, name, NULL);
    assert_true(g_file_set_contents(path, data, len, NULL));

    return path;
}

/*
 * Compute expected MD5 hex string for arbitrary data using GChecksum directly.
 */
static char *expectedMd5(const void *data, size_t len)
{
    GChecksum *cksum = g_checksum_new(G_CHECKSUM_MD5);
    g_checksum_update(cksum, data, len);
    char *hex = g_strdup(g_checksum_get_string(cksum));
    g_checksum_free(cksum);

    return hex;
}

/* ------------------------------------------------------------------ */

static void test_knownDigest(void **state)
{
    // MD5("") == d41d8cd98f00b204e9800998ecf8427e
    g_autofree char *path = writeTestFile("empty", "", 0);
    g_autofree char *md5 = deviceServiceHashComputeFileMd5HexString(path);

    assert_non_null(md5);
    assert_string_equal(md5, "d41d8cd98f00b204e9800998ecf8427e");
}

static void test_helloWorld(void **state)
{
    const char *content = "Hello, World!";
    g_autofree char *path = writeTestFile("hello", content, strlen(content));
    g_autofree char *md5 = deviceServiceHashComputeFileMd5HexString(path);
    g_autofree char *expected = expectedMd5(content, strlen(content));

    assert_non_null(md5);
    assert_string_equal(md5, expected);
}

static void test_binaryContent(void **state)
{
    // Include null bytes to ensure binary mode works
    const unsigned char data[] = {0x00, 0x01, 0x02, 0xff, 0xfe, 0x00, 0x80};
    g_autofree char *path = writeTestFile("binary", data, sizeof(data));
    g_autofree char *md5 = deviceServiceHashComputeFileMd5HexString(path);
    g_autofree char *expected = expectedMd5(data, sizeof(data));

    assert_non_null(md5);
    assert_string_equal(md5, expected);
}

static void test_largeFile(void **state)
{
    // Exceed the 4096 byte internal buffer to exercise multi-iteration reads
    const size_t size = 4096 * 3 + 42;
    g_autofree char *data = g_malloc(size);
    memset(data, 'A', size);
    g_autofree char *path = writeTestFile("large", data, size);
    g_autofree char *md5 = deviceServiceHashComputeFileMd5HexString(path);
    g_autofree char *expected = expectedMd5(data, size);

    assert_non_null(md5);
    assert_string_equal(md5, expected);
}

static void test_nonexistentFile(void **state)
{
    char *md5 = deviceServiceHashComputeFileMd5HexString("/tmp/no-such-file-deviceServiceHashTest");

    assert_null(md5);
}

static void test_deterministicAcrossReads(void **state)
{
    const char *content = "deterministic";
    g_autofree char *path = writeTestFile("det", content, strlen(content));
    g_autofree char *md5a = deviceServiceHashComputeFileMd5HexString(path);
    g_autofree char *md5b = deviceServiceHashComputeFileMd5HexString(path);

    assert_non_null(md5a);
    assert_non_null(md5b);
    assert_string_equal(md5a, md5b);
}

static void test_midReadFerror(void **state)
{
    // Create a file large enough for multiple fread iterations (buffer is 4096)
    const size_t size = 4096 * 3;
    g_autofree char *data = g_malloc(size);
    memset(data, 'B', size);
    g_autofree char *path = writeTestFile("ferror", data, size);

    // First fread: passthrough (reads real data)
    will_return(__wrap_fread, true);
    // Second fread: inject fault, return 0 bytes
    will_return(__wrap_fread, false);
    will_return(__wrap_fread, 0);
    // ferror: report the error
    will_return(__wrap_ferror, 1);

    char *md5 = deviceServiceHashComputeFileMd5HexString(path);

    assert_null(md5);
}

static void test_partialReadThenFerror(void **state)
{
    // Create a file large enough for multiple fread iterations (buffer is 4096)
    const size_t size = 4096 * 3;
    g_autofree char *data = g_malloc(size);
    memset(data, 'C', size);
    g_autofree char *path = writeTestFile("partial", data, size);

    // First fread: passthrough (reads real data)
    will_return(__wrap_fread, true);
    // Second fread: simulate partial read returning >0 bytes despite error
    will_return(__wrap_fread, false);
    will_return(__wrap_fread, 2000);
    // Third fread: return 0 to terminate the loop
    will_return(__wrap_fread, false);
    will_return(__wrap_fread, 0);
    // ferror: report the error
    will_return(__wrap_ferror, 1);

    char *md5 = deviceServiceHashComputeFileMd5HexString(path);

    assert_null(md5);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_knownDigest),
        cmocka_unit_test(test_helloWorld),
        cmocka_unit_test(test_binaryContent),
        cmocka_unit_test(test_largeFile),
        cmocka_unit_test(test_nonexistentFile),
        cmocka_unit_test(test_deterministicAcrossReads),
        cmocka_unit_test_setup_teardown(test_midReadFerror, faultInjectionSetup, faultInjectionTeardown),
        cmocka_unit_test_setup_teardown(test_partialReadThenFerror, faultInjectionSetup, faultInjectionTeardown),
    };

    return cmocka_run_group_tests(tests, groupSetup, groupTeardown);
}
