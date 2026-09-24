//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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
// Unit tests for the ONVIF device driver's DeviceDriver wiring. The device-service C API the driver
// depends on is replaced with Fake Function Framework (FFF) fakes so the driver links standalone.
// The driver self-registers via an __attribute__((constructor)) when this translation unit loads,
// so the fake registration call captures the DeviceDriver for the tests to exercise.
//

#include <gtest/gtest.h>

#include <fff/fff.h>

#include <cstdlib>
#include <string>

// device-driver.h transitively pulls in libxml2/ICU C++ template headers; include libxml2's parser
// here (as C++) so those templates are processed before the extern "C" block below guards them out.
#include <libxml/parser.h>

extern "C" {
#include "device-driver/device-driver-manager.h"
#include "device-driver/device-driver.h"
#include "device/deviceModelHelper.h"
#include "device/icDevice.h"
#include "device/icDeviceResource.h"
#include "deviceServiceConfiguration.h"
#include "provider/barton-core-property-provider.h"
#include <cjson/cJSON.h>
#include <commonDeviceDefs.h>
#include <deviceServicePrivate.h>
#include <icTypes/icLinkedList.h>
}

DEFINE_FFF_GLOBALS;

// Fakes for the device-service C API the driver references (so it links without the full service).
FAKE_VALUE_FUNC(bool, deviceDriverManagerRegisterDriver, DeviceDriver *);
FAKE_VALUE_FUNC(icDeviceEndpoint *, createEndpoint, icDevice *, const char *, const char *, bool);
FAKE_VALUE_FUNC(icDeviceResource *,
                createEndpointResource,
                icDeviceEndpoint *,
                const char *,
                const char *,
                const char *,
                uint8_t,
                ResourceCachingPolicy);
FAKE_VALUE_FUNC(icDeviceMetadata *, createDeviceMetadata, icDevice *, const char *, const char *);
FAKE_VALUE_FUNC(bool, deviceServiceDeviceFound, DeviceFoundDetails *, bool);
FAKE_VALUE_FUNC(bool, deviceServiceIsDeviceKnown, const char *);
FAKE_VOID_FUNC(updateResource, const char *, const char *, const char *, const char *, cJSON *);
FAKE_VALUE_FUNC(char *, getMetadata, const char *, const char *, const char *);
FAKE_VALUE_FUNC(icDeviceResource *, deviceServiceGetResourceById, const char *, const char *, const char *);
FAKE_VOID_FUNC(resourceDestroy, icDeviceResource *);
FAKE_VALUE_FUNC(BCorePropertyProvider *, deviceServiceConfigurationGetPropertyProvider);
FAKE_VALUE_FUNC(gchar *,
                b_core_property_provider_get_property_as_string,
                BCorePropertyProvider *,
                const gchar *,
                const gchar *);

namespace
{

    // The single DeviceDriver the constructor registered when this TU loaded.
    DeviceDriver *RegisteredDriver()
    {
        return deviceDriverManagerRegisterDriver_fake.arg0_val;
    }

    // Build a minimal executable resource for a device/endpoint/id (the fields the driver reads).
    struct FakeResource
    {
        icDeviceResource resource {};
        std::string uuid;
        std::string endpointId;
        std::string id;

        FakeResource(const char *deviceUuid, const char *endpoint, const char *resourceId) :
            uuid(deviceUuid), endpointId(endpoint), id(resourceId)
        {
            resource.deviceUuid = uuid.data();
            resource.endpointId = endpointId.data();
            resource.id = id.data();
        }
    };

} // namespace

// A single test: the shared driver is created once at load, so registration, the springboard
// executes, and the destroy path are all exercised in order within one deterministic test.
TEST(OnvifDeviceDriver, RegistersWiresContractAndDestroysCleanly)
{
    DeviceDriver *driver = RegisteredDriver();
    ASSERT_NE(driver, nullptr);

    // Registration contract: named, vouches for its devices, and wires up the callbacks.
    EXPECT_STREQ(driver->driverName, "onvifCameraDeviceDriver");
    EXPECT_TRUE(driver->neverReject);
    ASSERT_NE(driver->callbackContext, nullptr);
    ASSERT_NE(driver->executeResource, nullptr);
    ASSERT_NE(driver->destroy, nullptr);
    ASSERT_NE(driver->supportedDeviceClasses, nullptr);
    EXPECT_STREQ((const char *) linkedListGetElementAt(driver->supportedDeviceClasses, 0), CAMERA_DC);

    void *ctx = driver->callbackContext;

    // stream springboards to the ONVIF media entry point.
    {
        FakeResource stream("dev1", CAMERA_SESSION_ENDPOINT_ID, CAMERA_SESSION_FUNCTION_STREAM);
        char *response = nullptr;
        EXPECT_TRUE(driver->executeResource(ctx, &stream.resource, "", &response));
        ASSERT_NE(response, nullptr);
        EXPECT_STREQ(response, "{\"protocol\":\"onvif\",\"entryPoint\":\"/dev1/ep/onvif/r/getMediaUrl\"}");
        free(response);
    }

    // takePicture springboards to the ONVIF snapshot entry point.
    {
        FakeResource picture("dev1", CAMERA_SESSION_ENDPOINT_ID, CAMERA_SESSION_FUNCTION_TAKE_PICTURE);
        char *response = nullptr;
        EXPECT_TRUE(driver->executeResource(ctx, &picture.resource, "", &response));
        ASSERT_NE(response, nullptr);
        EXPECT_STREQ(response, "{\"protocol\":\"onvif\",\"entryPoint\":\"/dev1/ep/onvif/r/getSnapshotUrl\"}");
        free(response);
    }

    // createSession returns a correlation id; destroySession is a no-op success (stateless protocol).
    {
        FakeResource create("dev1", CAMERA_SESSION_ENDPOINT_ID, CAMERA_SESSION_FUNCTION_CREATE_SESSION);
        char *response = nullptr;
        EXPECT_TRUE(driver->executeResource(ctx, &create.resource, "", &response));
        ASSERT_NE(response, nullptr);
        EXPECT_STREQ(response, "1");
        free(response);

        FakeResource destroySession("dev1", CAMERA_SESSION_ENDPOINT_ID, CAMERA_SESSION_FUNCTION_DESTROY_SESSION);
        EXPECT_TRUE(driver->executeResource(ctx, &destroySession.resource, "", nullptr));
    }

    // An unknown resource is rejected.
    {
        FakeResource unknown("dev1", CAMERA_SESSION_ENDPOINT_ID, "bogus");
        char *response = nullptr;
        EXPECT_FALSE(driver->executeResource(ctx, &unknown.resource, "", &response));
    }

    // Destroy: ctx is the OnvifDriver instance (callbackContext), not a bare DeviceDriver. The object
    // was allocated with `new`, so destroy() must `delete` it; a free()/delete mismatch here would
    // trip AddressSanitizer. A clean run proves the destroy contract and the ctx handling.
    driver->destroy(ctx);
    SUCCEED();
}
