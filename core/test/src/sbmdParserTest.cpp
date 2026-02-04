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
 * Created by Thomas Lea on 10/22/2025
 */

#include "deviceDrivers/matter/sbmd/SbmdParser.h"

extern "C" {
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>

static void test_sbmdParserReportingSection(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
scriptType: "JavaScript"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
reporting:
  minSecs: 5
  maxSecs: 7200
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    // Verify basic metadata
    assert_string_equal(spec->name.c_str(), "Test Device");
    assert_string_equal(spec->schemaVersion.c_str(), "1.0");
    assert_string_equal(spec->driverVersion.c_str(), "1.0");
    assert_string_equal(spec->scriptType.c_str(), "JavaScript");

    // Verify reporting section
    assert_int_equal((int)spec->reporting.minSecs, 5);
    assert_int_equal((int)spec->reporting.maxSecs, 7200);
}

static void test_sbmdParserReportingOptional(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
    - "67"
  revision: 1
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    // Verify reporting defaults to 0 when not present
    assert_int_equal((int)spec->reporting.minSecs, 0);
    assert_int_equal((int)spec->reporting.maxSecs, 0);
}

static void test_sbmdParserEndpointWithStringIds(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceType: "0x0043"
  revision: 1
resources: []
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 3
    resources: []
  - id: "main"
    profile: "control"
    profileVersion: 2
    resources: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    // Verify endpoints were parsed
    assert_int_equal((int) spec->endpoints.size(), 2);

    // Verify first endpoint
    assert_string_equal(spec->endpoints[0].id.c_str(), "1");
    assert_string_equal(spec->endpoints[0].profile.c_str(), "sensor");
    assert_int_equal((int) spec->endpoints[0].profileVersion, 3);

    // Verify second endpoint with string id
    assert_string_equal(spec->endpoints[1].id.c_str(), "main");
    assert_string_equal(spec->endpoints[1].profile.c_str(), "control");
    assert_int_equal((int) spec->endpoints[1].profileVersion, 2);
}

static void test_sbmdParserWaterLeakSensorFile(void **state)
{
    (void) state;

    // Use absolute path defined by CMake
    const char *filePath = SBMD_SPEC_DIR "water-leak-detector.sbmd";

    auto spec = barton::SbmdParser::ParseFile(filePath);
    assert_non_null(spec.get());

    // Verify basic metadata
    assert_string_equal(spec->name.c_str(), "Water Leak Detector");

    // Verify reporting section from the actual file
    assert_int_equal((int)spec->reporting.minSecs, 1);
    assert_int_equal((int)spec->reporting.maxSecs, 3600);

    // Verify endpoints with string IDs
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_string_equal(spec->endpoints[0].id.c_str(), "1");
}

static void test_sbmdParserResourceIdFields(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources:
  - id: "rootResource"
    type: "boolean"
    modes: ["read"]
    cachingPolicy: "always"
    mapper:
      read:
        attribute:
          clusterId: "0x0001"
          attributeId: "0x0002"
          name: "TestAttr"
          type: "bool"
        script: "return value;"
endpoints:
  - id: "ep1"
    profile: "sensor"
    profileVersion: 3
    resources:
      - id: "endpointResource"
        type: "boolean"
        modes: ["read", "write"]
        cachingPolicy: "always"
        mapper:
          read:
            attribute:
              clusterId: "0x0003"
              attributeId: "0x0004"
              name: "TestAttr2"
              type: "bool"
            script: "return value;"
          write:
            attribute:
              clusterId: "0x0003"
              attributeId: "0x0004"
              name: "TestAttr2"
              type: "bool"
            script: "return value;"
      - id: "executeResource"
        type: "function"
        modes: []
        mapper:
          execute:
            command:
              clusterId: "0x0005"
              commandId: "0x0006"
              name: "TestCmd"
            script: "return {};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    // Verify root resource has resourceId but no resourceEndpointId
    assert_int_equal((int) spec->resources.size(), 1);
    assert_string_equal(spec->resources[0].id.c_str(), "rootResource");
    assert_false(spec->resources[0].resourceEndpointId.has_value());

    assert_true(spec->resources[0].mapper.hasRead);
    assert_true(spec->resources[0].mapper.readAttribute.has_value());
    assert_string_equal(spec->resources[0].mapper.readAttribute->resourceId.c_str(), "rootResource");
    assert_false(spec->resources[0].mapper.readAttribute->resourceEndpointId.has_value());

    // Verify endpoint resources have both resourceId and resourceEndpointId
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_string_equal(spec->endpoints[0].id.c_str(), "ep1");
    assert_int_equal((int) spec->endpoints[0].resources.size(), 2);

    // First endpoint resource with read and write
    auto &epResource1 = spec->endpoints[0].resources[0];
    assert_string_equal(epResource1.id.c_str(), "endpointResource");
    assert_true(epResource1.resourceEndpointId.has_value());
    assert_string_equal(epResource1.resourceEndpointId.value().c_str(), "ep1");

    assert_true(epResource1.mapper.hasRead);
    assert_true(epResource1.mapper.readAttribute.has_value());
    assert_string_equal(epResource1.mapper.readAttribute->resourceId.c_str(), "endpointResource");
    assert_true(epResource1.mapper.readAttribute->resourceEndpointId.has_value());
    assert_string_equal(epResource1.mapper.readAttribute->resourceEndpointId.value().c_str(), "ep1");

    assert_true(epResource1.mapper.hasWrite);
    assert_true(epResource1.mapper.writeAttribute.has_value());
    assert_string_equal(epResource1.mapper.writeAttribute->resourceId.c_str(), "endpointResource");
    assert_true(epResource1.mapper.writeAttribute->resourceEndpointId.has_value());
    assert_string_equal(epResource1.mapper.writeAttribute->resourceEndpointId.value().c_str(), "ep1");

    // Second endpoint resource with execute
    auto &epResource2 = spec->endpoints[0].resources[1];
    assert_string_equal(epResource2.id.c_str(), "executeResource");
    assert_true(epResource2.resourceEndpointId.has_value());
    assert_string_equal(epResource2.resourceEndpointId.value().c_str(), "ep1");

    assert_true(epResource2.mapper.hasExecute);
    assert_true(epResource2.mapper.executeCommand.has_value());
    assert_string_equal(epResource2.mapper.executeCommand->resourceId.c_str(), "executeResource");
    assert_true(epResource2.mapper.executeCommand->resourceEndpointId.has_value());
    assert_string_equal(epResource2.mapper.executeCommand->resourceEndpointId.value().c_str(), "ep1");
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sbmdParserReportingSection),
        cmocka_unit_test(test_sbmdParserReportingOptional),
        cmocka_unit_test(test_sbmdParserEndpointWithStringIds),
        cmocka_unit_test(test_sbmdParserWaterLeakSensorFile),
        cmocka_unit_test(test_sbmdParserResourceIdFields),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

} // extern "C"
