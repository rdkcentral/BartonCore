#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "icUtil/stringUtils.h"
#include "subsystemManager.c"
#include <cmocka.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool __wrap_deviceServiceGetSystemProperty(const char *name, char **value)
{
    function_called();
    *value = mock_type(char *);

    return (*value != NULL);
}

bool __wrap_deviceServiceSetSystemProperty(const char *name, const char *value)
{
    function_called();
    check_expected(value);

    return mock_type(bool);
}

static bool myInitialize(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback)
{
    return true;
}

static bool myMigrate(uint16_t oldVersion, uint16_t newVersion)
{
    function_called();
    check_expected(oldVersion);
    check_expected(newVersion);

    return mock_type(bool);
}

void test_subsystem_migration(void **state)
{
    (void) state;
    // A little whacky but I don't want self-registering subsystems interfering with test fixtures.
    unregisterSubsystems();

    static const char *subsystemName = "mySubsystem";

    scoped_generic Subsystem *mySubsystem = calloc(1, sizeof(Subsystem));
    mySubsystem->initialize = myInitialize;
    mySubsystem->migrate = myMigrate;
    mySubsystem->name = subsystemName;
    subsystemManagerRegister(mySubsystem);

    const char *zero = "0";
    const char *one = "1";
    const char *two = "2";
    const char *oneHundo = "100";

    will_return_always(__wrap_deviceServiceSetSystemProperty, true);

    // Case: No saved version, subsystem version is 0 (unspecified). Save 0 to properties
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, zero);

    subsystemManagerInitialize(NULL);

    // Case : No saved version, subsystem version is 0 (specified). Save 0 to properties
    mySubsystem->version = 0;

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, zero);

    subsystemManagerInitialize(NULL);

    // Case : No saved version, subsystem version is 1. Save 1 to properties.
    mySubsystem->version = 1;

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, NULL);

    expect_function_call(myMigrate);
    will_return(myMigrate, true);
    expect_value(myMigrate, oldVersion, 0);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, one);

    subsystemManagerInitialize(NULL);

    // Case : Saved version 1, subsystem version is 1. Save 1 to properties.
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, one);
    subsystemManagerInitialize(NULL);

    // Case : Saved version 1, subsystem version is 2. Save 2 to properties.
    mySubsystem->version = 2;

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(myMigrate);
    will_return(myMigrate, true);
    expect_value(myMigrate, oldVersion, 1);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, two);
    subsystemManagerInitialize(NULL);

    // Case : Saved version 1, subsystem version is 2. Migration routine fails, don't save.
    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(myMigrate);
    will_return(myMigrate, false);
    expect_value(myMigrate, oldVersion, 1);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    subsystemManagerInitialize(NULL);

    // Case : Saved version 1, subsystem version is 100. Save 100 to properties.
    mySubsystem->version = 100;

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(one));

    expect_function_call(myMigrate);
    will_return(myMigrate, true);
    expect_value(myMigrate, oldVersion, 1);
    expect_value(myMigrate, newVersion, mySubsystem->version);

    expect_function_call(__wrap_deviceServiceSetSystemProperty);
    expect_string(__wrap_deviceServiceSetSystemProperty, value, oneHundo);

    subsystemManagerInitialize(NULL);

    // Case : Saved version 2, subsystem version is 1. Don't save.
    // This is considered time travel.
    mySubsystem->version = 1;

    expect_function_call(__wrap_deviceServiceGetSystemProperty);
    will_return(__wrap_deviceServiceGetSystemProperty, strdup(two));

    subsystemManagerInitialize(NULL);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest test[] = {
        cmocka_unit_test(test_subsystem_migration),
    };

    return cmocka_run_group_tests(test, NULL, NULL);
}
