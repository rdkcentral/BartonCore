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
 * Created by Christian Leithner on 9/3/2024.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "glib/gprintf.h"
#include "glib/gtypes.h"
#include "glibconfig.h"
#include "inttypes.h"
#include "provider/barton-core-default-property-provider.h"
#include "provider/barton-core-property-provider.h"
#include <cmocka.h>
#include <stdbool.h>

// OOB - out of bounds
static gchar *INT8_MAX_STR_OOB = NULL;
static gchar *INT8_MIN_STR_OOB = NULL;
static gchar *INT16_MAX_STR_OOB = NULL;
static gchar *INT16_MIN_STR_OOB = NULL;
static gchar *INT32_MAX_STR_OOB = NULL;
static gchar *INT32_MIN_STR_OOB = NULL;
static gchar *INT64_MAX_STR_OOB = NULL;
static gchar *INT64_MIN_STR_OOB = NULL;
static gchar *UINT8_MAX_STR_OOB = NULL;
static gchar *UINT16_MAX_STR_OOB = NULL;
static gchar *UINT32_MAX_STR_OOB = NULL;
static gchar *UINT64_MAX_STR_OOB = NULL;

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    function_called();
    check_expected(name);
    *value = g_strdup(mock_type(char *));
    return mock_type(bool);
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    function_called();
    check_expected(name);
    check_expected(value);
    return mock_type(bool);
}

static void
mockPropertyChangedHandler(GObject *source, gchar *property_name, gchar *old_property_value, gchar *new_property_value);

static void registerSignalHandlers(BCorePropertyProvider *provider)
{
    g_signal_connect(provider,
                     B_CORE_PROPERTY_PROVIDER_SIGNAL_PROPERTY_CHANGED,
                     G_CALLBACK(mockPropertyChangedHandler),
                     NULL);
}

static int setup(void **state)
{
    *state = b_core_default_property_provider_new();
    registerSignalHandlers(*state);

    INT8_MAX_STR_OOB = g_strdup_printf("%" PRIi16, G_MAXINT8 + 1);
    INT8_MIN_STR_OOB = g_strdup_printf("%" PRIi16, G_MININT8 - 1);
    INT16_MAX_STR_OOB = g_strdup_printf("%" PRIi32, G_MAXINT16 + 1);
    INT16_MIN_STR_OOB = g_strdup_printf("%" PRIi32, G_MININT16 - 1);
    INT32_MAX_STR_OOB = g_strdup_printf("%" PRIi64, ((gint64) G_MAXINT32) + 1);
    INT32_MIN_STR_OOB = g_strdup_printf("%" PRIi64, ((gint64) G_MININT32) - 1);
    INT64_MAX_STR_OOB = g_strdup("9223372036854775808");
    INT64_MIN_STR_OOB = g_strdup("-9223372036854775809");
    UINT8_MAX_STR_OOB = g_strdup_printf("%" PRIu16, G_MAXUINT8 + 1);
    UINT16_MAX_STR_OOB = g_strdup_printf("%" PRIu32, G_MAXUINT16 + 1);
    UINT32_MAX_STR_OOB = g_strdup_printf("%" PRIu64, ((gint64) G_MAXUINT32) + 1);
    UINT64_MAX_STR_OOB = g_strdup("18446744073709551616");

    return 0;
}

static int teardown(void **state)
{
    g_clear_object((BCoreDefaultPropertyProvider **) state);

    g_free(INT8_MAX_STR_OOB);
    g_free(INT8_MIN_STR_OOB);
    g_free(INT16_MAX_STR_OOB);
    g_free(INT16_MIN_STR_OOB);
    g_free(INT32_MAX_STR_OOB);
    g_free(INT32_MIN_STR_OOB);
    g_free(INT64_MAX_STR_OOB);
    g_free(INT64_MIN_STR_OOB);
    g_free(UINT8_MAX_STR_OOB);
    g_free(UINT16_MAX_STR_OOB);
    g_free(UINT32_MAX_STR_OOB);
    g_free(UINT64_MAX_STR_OOB);

    return 0;
}

static void test_b_core_property_provider_get_property(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "bar";
    GError *error = NULL;

    gchar *valueTest = b_core_property_provider_get_property(NULL, name, &error);
    assert_null(valueTest);

    valueTest =
        b_core_property_provider_get_property(B_CORE_PROPERTY_PROVIDER(provider), NULL, &error);
    assert_null(valueTest);
    assert_non_null(error);
    assert_int_equal(error->code, B_CORE_PROPERTY_PROVIDER_ERROR_ARG);
    g_clear_error(&error);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest =
        b_core_property_provider_get_property(B_CORE_PROPERTY_PROVIDER(provider), name, &error);
    assert_null(valueTest);
    assert_non_null(error);
    assert_int_equal(error->code, B_CORE_PROPERTY_PROVIDER_ERROR_NOT_FOUND);
    g_clear_error(&error);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest =
        b_core_property_provider_get_property(B_CORE_PROPERTY_PROVIDER(provider), name, &error);
    assert_non_null(valueTest);
    assert_string_equal(valueTest, value);
    assert_null(error);
    g_clear_pointer(&valueTest, g_free);
}

static void test_b_core_property_provider_get_property_as_string(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "bar";
    gchar *defaultValue = "baz";

    gchar *valueTest = b_core_property_provider_get_property_as_string(NULL, name, defaultValue);
    assert_null(valueTest);

    valueTest = b_core_property_provider_get_property_as_string(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_non_null(valueTest);
    assert_string_equal(valueTest, defaultValue);
    g_clear_pointer(&valueTest, g_free);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_string(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_non_null(valueTest);
    assert_string_equal(valueTest, defaultValue);
    g_clear_pointer(&valueTest, g_free);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_string(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_non_null(valueTest);
    assert_string_equal(valueTest, value);
    g_clear_pointer(&valueTest, g_free);
}

static void test_b_core_property_provider_get_property_as_bool(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "true";
    gboolean defaultValue = false;

    gboolean valueTest = b_core_property_provider_get_property_as_bool(NULL, name, defaultValue);
    assert_true(valueTest == defaultValue);

    valueTest = b_core_property_provider_get_property_as_bool(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_true(valueTest == defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_bool(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_true(valueTest == defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_bool(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_true(valueTest);

    value = "True";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_bool(B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_true(valueTest);

    value = "false";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_bool(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_false(valueTest);

    value = "bar";
    defaultValue = true;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_bool(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_false(valueTest);
}

static void test_b_core_property_provider_get_property_as_int8(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    gint8 defaultValue = 0;

    gint8 valueTest = b_core_property_provider_get_property_as_int8(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, -1);

    value = INT8_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = INT8_MIN_STR_OOB;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_int16(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    gint16 defaultValue = 0;

    gint16 valueTest = b_core_property_provider_get_property_as_int16(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, -1);

    value = INT16_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = INT16_MIN_STR_OOB;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_int32(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    gint32 defaultValue = 0;

    gint32 valueTest = b_core_property_provider_get_property_as_int32(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, -1);

    value = INT32_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = INT32_MIN_STR_OOB;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_int64(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    gint64 defaultValue = 0;

    gint64 valueTest = b_core_property_provider_get_property_as_int64(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, -1);

    value = INT64_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = INT64_MIN_STR_OOB;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_int64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_uint8(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    guint8 defaultValue = 0;

    guint8 valueTest = b_core_property_provider_get_property_as_uint8(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_uint8(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_uint8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = UINT8_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint8(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_uint16(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    guint16 defaultValue = 0;

    guint16 valueTest = b_core_property_provider_get_property_as_uint16(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_uint16(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_uint16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = UINT16_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint16(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_uint32(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    guint32 defaultValue = 0;

    guint32 valueTest = b_core_property_provider_get_property_as_uint32(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_uint32(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_uint32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = UINT32_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint32(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_get_property_as_uint64(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "42";
    guint64 defaultValue = 0;

    guint64 valueTest = b_core_property_provider_get_property_as_uint64(NULL, name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    valueTest = b_core_property_provider_get_property_as_uint64(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    valueTest = b_core_property_provider_get_property_as_uint64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, 42);

    value = "-1";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = UINT64_MAX_STR_OOB;
    defaultValue = 10;
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);

    value = "bar";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, value);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    valueTest = b_core_property_provider_get_property_as_uint64(
        B_CORE_PROPERTY_PROVIDER(provider), name, defaultValue);
    assert_int_equal(valueTest, defaultValue);
}

static void test_b_core_property_provider_set_property(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "bar";
    gchar *oldValue = NULL;
    GError *error = NULL;

    gboolean success = b_core_property_provider_set_property(NULL, name, value, &error);
    assert_false(success);

    success = b_core_property_provider_set_property(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, value, &error);
    assert_false(success);
    assert_non_null(error);
    assert_int_equal(error->code, B_CORE_PROPERTY_PROVIDER_ERROR_ARG);
    g_clear_error(&error);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_value(__wrap_deviceServiceSetSystemProperty, value, NULL);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_value(mockPropertyChangedHandler, old_property_value, NULL);
    expect_value(mockPropertyChangedHandler, new_property_value, NULL);

    success = b_core_property_provider_set_property(
        B_CORE_PROPERTY_PROVIDER(provider), name, NULL, &error);
    assert_true(success);
    assert_null(error);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_value(mockPropertyChangedHandler, old_property_value, NULL);
    expect_string(mockPropertyChangedHandler, new_property_value, value);

    success = b_core_property_provider_set_property(
        B_CORE_PROPERTY_PROVIDER(provider), name, value, &error);
    assert_true(success);
    assert_null(error);

    oldValue = "baz";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, oldValue);
    will_return(__wrap_deviceServiceGetSystemProperty, true);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_string(mockPropertyChangedHandler, old_property_value, oldValue);
    expect_string(mockPropertyChangedHandler, new_property_value, value);

    success = b_core_property_provider_set_property(
        B_CORE_PROPERTY_PROVIDER(provider), name, value, &error);
    assert_true(success);
    assert_null(error);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, false);

    success = b_core_property_provider_set_property(
        B_CORE_PROPERTY_PROVIDER(provider), name, value, &error);
    assert_false(success);
    assert_non_null(error);
    assert_int_equal(error->code, B_CORE_PROPERTY_PROVIDER_ERROR_INTERNAL);
    g_clear_error(&error);
}

static void test_b_core_property_provider_set_property_as_string(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "bar";

    gboolean success = b_core_property_provider_set_property_string(NULL, name, value);
    assert_false(success);

    success = b_core_property_provider_set_property_string(
        B_CORE_PROPERTY_PROVIDER(provider), NULL, value);
    assert_false(success);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_value(__wrap_deviceServiceSetSystemProperty, value, NULL);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_value(mockPropertyChangedHandler, old_property_value, NULL);
    expect_value(mockPropertyChangedHandler, new_property_value, NULL);

    success = b_core_property_provider_set_property_string(
        B_CORE_PROPERTY_PROVIDER(provider), name, NULL);
    assert_true(success);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_value(mockPropertyChangedHandler, old_property_value, NULL);
    expect_string(mockPropertyChangedHandler, new_property_value, value);

    success = b_core_property_provider_set_property_string(
        B_CORE_PROPERTY_PROVIDER(provider), name, value);
    assert_true(success);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, false);

    success = b_core_property_provider_set_property_string(
        B_CORE_PROPERTY_PROVIDER(provider), name, value);
    assert_false(success);
}

static void test_b_core_property_provider_set_property_as_bool(void **state)
{
    BCoreDefaultPropertyProvider *provider = *state;

    gchar *name = "foo";
    gchar *value = "true";

    gboolean success = b_core_property_provider_set_property_bool(NULL, name, true);
    assert_false(success);

    success =
        b_core_property_provider_set_property_bool(B_CORE_PROPERTY_PROVIDER(provider), NULL, true);
    assert_false(success);

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_value(mockPropertyChangedHandler, old_property_value, NULL);
    expect_string(mockPropertyChangedHandler, new_property_value, value);

    success =
        b_core_property_provider_set_property_bool(B_CORE_PROPERTY_PROVIDER(provider), name, true);
    assert_true(success);

    value = "false";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, true);

    expect_function_call(mockPropertyChangedHandler);
    expect_string(mockPropertyChangedHandler, property_name, name);
    expect_value(mockPropertyChangedHandler, old_property_value, NULL);
    expect_string(mockPropertyChangedHandler, new_property_value, value);

    success =
        b_core_property_provider_set_property_bool(B_CORE_PROPERTY_PROVIDER(provider), name, false);
    assert_true(success);

    value = "false";
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    expect_string(__wrap_deviceServiceGetSystemProperty, name, name);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);
    will_return(__wrap_deviceServiceGetSystemProperty, false);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, name, name);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, value);
    will_return(__wrap_deviceServiceSetSystemProperty, false);

    success =
        b_core_property_provider_set_property_bool(B_CORE_PROPERTY_PROVIDER(provider), name, false);
    assert_false(success);
}

static void
mockPropertyChangedHandler(GObject *source, gchar *property_name, gchar *old_property_value, gchar *new_property_value)
{
    function_called();

    check_expected(property_name);
    check_expected(old_property_value);
    check_expected(new_property_value);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_b_core_property_provider_get_property),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_string),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_bool),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_int8),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_int16),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_int32),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_int64),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_uint8),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_uint16),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_uint32),
        cmocka_unit_test(test_b_core_property_provider_get_property_as_uint64),
        cmocka_unit_test(test_b_core_property_provider_set_property),
        cmocka_unit_test(test_b_core_property_provider_set_property_as_string),
        cmocka_unit_test(test_b_core_property_provider_set_property_as_bool),
    };

    int retval = cmocka_run_group_tests(tests, setup, teardown);

    return retval;
}
