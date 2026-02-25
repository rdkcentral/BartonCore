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

    // Verify matterMeta deviceTypes - this catches schema errors like using 'deviceType' instead of 'deviceTypes'
    assert_int_equal((int) spec->matterMeta.deviceTypes.size(), 1);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x0043);

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

    // Verify matterMeta deviceTypes - ensures multiple device types are parsed correctly
    assert_int_equal((int) spec->matterMeta.deviceTypes.size(), 2);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x0043);
    assert_int_equal((int) spec->matterMeta.deviceTypes[1], 67);

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
  deviceTypes:
    - "0x0043"
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

    // Verify matterMeta deviceTypes
    assert_int_equal((int) spec->matterMeta.deviceTypes.size(), 1);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x0043);

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

static void test_sbmdParserDoorLockFile(void **state)
{
    (void) state;

    // Use absolute path defined by CMake
    const char *filePath = SBMD_SPEC_DIR "door-lock.sbmd";

    auto spec = barton::SbmdParser::ParseFile(filePath);
    assert_non_null(spec.get());

    // Verify basic metadata
    assert_string_equal(spec->name.c_str(), "Door Lock");
    assert_string_equal(spec->bartonMeta.deviceClass.c_str(), "doorLock");
    assert_int_equal((int) spec->bartonMeta.deviceClassVersion, 3);

    // Verify matterMeta deviceTypes - 0x000a is 10 (door lock device type)
    assert_int_equal((int) spec->matterMeta.deviceTypes.size(), 1);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x000a);

    // Verify reporting section from the actual file
    assert_int_equal((int)spec->reporting.minSecs, 1);
    assert_int_equal((int)spec->reporting.maxSecs, 3600);

    // Verify endpoints
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_string_equal(spec->endpoints[0].id.c_str(), "1");
    assert_string_equal(spec->endpoints[0].profile.c_str(), "doorLock");

    // Verify locked resource uses event mapper (LockOperation)
    assert_true(spec->endpoints[0].resources.size() >= 1);
    auto &locked = spec->endpoints[0].resources[0];
    assert_string_equal(locked.id.c_str(), "locked");
    assert_false(locked.mapper.hasRead);
    assert_true(locked.mapper.event.has_value());
    assert_int_equal((int) locked.mapper.event->clusterId, 0x0101);
    assert_int_equal((int) locked.mapper.event->eventId, 0x0002);
    assert_string_equal(locked.mapper.event->name.c_str(), "LockOperation");
    assert_false(locked.mapper.eventScript.empty());
    // Event should have endpoint ID set
    assert_true(locked.mapper.event->resourceEndpointId.has_value());
    assert_string_equal(locked.mapper.event->resourceEndpointId.value().c_str(), "1");
    assert_string_equal(locked.mapper.event->resourceId.c_str(), "locked");
}

static void test_sbmdParserLightFile(void **state)
{
    (void) state;

    // Use absolute path defined by CMake
    const char *filePath = SBMD_SPEC_DIR "light.sbmd";

    auto spec = barton::SbmdParser::ParseFile(filePath);
    assert_non_null(spec.get());

    // Verify basic metadata
    assert_string_equal(spec->name.c_str(), "Light");
    assert_string_equal(spec->bartonMeta.deviceClass.c_str(), "light");
    assert_int_equal((int) spec->bartonMeta.deviceClassVersion, 0);

    // Verify matterMeta contains at least On/Off Light and Dimmable Light
    assert_true(spec->matterMeta.deviceTypes.size() >= 3);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x0100);
    assert_int_equal((int) spec->matterMeta.deviceTypes[2], 0x0101);

    // Verify reporting section
    assert_int_equal((int) spec->reporting.minSecs, 1);
    assert_int_equal((int) spec->reporting.maxSecs, 3600);

    // Verify identifySeconds device-level resource exists
    assert_true(spec->resources.size() >= 1);
    assert_string_equal(spec->resources[0].id.c_str(), "identifySeconds");
    assert_true(spec->resources[0].mapper.hasRead);
    assert_true(spec->resources[0].mapper.hasWrite);
    assert_true(spec->resources[0].mapper.readAttribute.has_value());
    // Write uses script-only approach
    assert_false(spec->resources[0].mapper.writeScript.empty());

    // Verify endpoints and core light resources
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_string_equal(spec->endpoints[0].id.c_str(), "1");
    assert_string_equal(spec->endpoints[0].profile.c_str(), "light");
    assert_int_equal((int) spec->endpoints[0].resources.size(), 2);

    // isOn resource maps to OnOff cluster - uses MatterJs script-only approach
    auto &isOn = spec->endpoints[0].resources[0];
    assert_string_equal(isOn.id.c_str(), "isOn");
    assert_false(isOn.optional);
    assert_true(isOn.mapper.hasRead);
    assert_true(isOn.mapper.hasWrite);
    assert_true(isOn.mapper.readAttribute.has_value());
    assert_int_equal((int) isOn.mapper.readAttribute->clusterId, 0x0006);
    assert_int_equal((int) isOn.mapper.readAttribute->attributeId, 0x0000);
    // Write uses script-only approach
    assert_false(isOn.mapper.writeScript.empty());

    // currentLevel resource maps to LevelControl cluster - uses MatterJs script-only approach
    auto &currentLevel = spec->endpoints[0].resources[1];
    assert_string_equal(currentLevel.id.c_str(), "currentLevel");
    assert_true(currentLevel.optional);
    assert_true(currentLevel.mapper.hasRead);
    assert_true(currentLevel.mapper.hasWrite);
    assert_true(currentLevel.mapper.readAttribute.has_value());
    assert_int_equal((int) currentLevel.mapper.readAttribute->clusterId, 0x0008);
    assert_int_equal((int) currentLevel.mapper.readAttribute->attributeId, 0x0000);
    // Write uses script-only approach
    assert_false(currentLevel.mapper.writeScript.empty());
}

static void test_sbmdParserOptionalResource(void **state)
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
  - id: "requiredResource"
    type: "boolean"
    modes: ["read"]
    mapper:
      read:
        attribute:
          clusterId: "0x0001"
          attributeId: "0x0002"
          name: "TestAttr"
          type: "bool"
        script: "return value;"
  - id: "optionalResource"
    type: "boolean"
    optional: true
    modes: ["read"]
    mapper:
      read:
        attribute:
          clusterId: "0x0003"
          attributeId: "0x0004"
          name: "TestAttr2"
          type: "bool"
        script: "return value;"
endpoints:
  - id: "ep1"
    profile: "sensor"
    profileVersion: 3
    resources:
      - id: "epRequired"
        type: "boolean"
        modes: ["read"]
        mapper:
          read:
            attribute:
              clusterId: "0x0005"
              attributeId: "0x0006"
              name: "TestAttr3"
              type: "bool"
            script: "return value;"
      - id: "epOptional"
        type: "boolean"
        optional: true
        modes: ["read"]
        mapper:
          read:
            attribute:
              clusterId: "0x0007"
              attributeId: "0x0008"
              name: "TestAttr4"
              type: "bool"
            script: "return value;"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    // Verify top-level resources
    assert_int_equal((int) spec->resources.size(), 2);
    assert_string_equal(spec->resources[0].id.c_str(), "requiredResource");
    assert_false(spec->resources[0].optional);
    assert_string_equal(spec->resources[1].id.c_str(), "optionalResource");
    assert_true(spec->resources[1].optional);

    // Verify endpoint resources
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_int_equal((int) spec->endpoints[0].resources.size(), 2);
    assert_string_equal(spec->endpoints[0].resources[0].id.c_str(), "epRequired");
    assert_false(spec->endpoints[0].resources[0].optional);
    assert_string_equal(spec->endpoints[0].resources[1].id.c_str(), "epOptional");
    assert_true(spec->endpoints[0].resources[1].optional);
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
        mapper:
          read:
            attribute:
              clusterId: "0x0003"
              attributeId: "0x0004"
              name: "TestAttr2"
              type: "bool"
            script: "return value;"
          write:
            script: "return value;"
      - id: "executeResource"
        type: "function"
        modes: []
        mapper:
          execute:
            script: "return {};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    // Verify matterMeta deviceTypes
    assert_int_equal((int) spec->matterMeta.deviceTypes.size(), 1);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x0043);

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
    assert_false(epResource1.mapper.writeScript.empty());

    // Second endpoint resource with execute
    auto &epResource2 = spec->endpoints[0].resources[1];
    assert_string_equal(epResource2.id.c_str(), "executeResource");
    assert_true(epResource2.resourceEndpointId.has_value());
    assert_string_equal(epResource2.resourceEndpointId.value().c_str(), "ep1");

    assert_true(epResource2.mapper.hasExecute);
    assert_false(epResource2.mapper.executeScript.empty());
}

// ============================================================================
// Negative Test Cases - Error Handling
// ============================================================================

static void test_sbmdParserInvalidYamlSyntax(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "1.0"
name: "Test Device"
  invalid indentation here: "this is broken YAML"
bartonMeta:
  deviceClass: "sensor"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserMissingRequiredFields(void **state)
{
    (void) state;

    // Empty YAML should parse but result in empty spec (no validation currently)
    // This test documents the current behavior
    const char *emptyYaml = R"(
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(emptyYaml);
    // Parser currently allows missing required fields - spec is created but fields are empty
    assert_non_null(spec.get());
    assert_true(spec->schemaVersion.empty());
    assert_true(spec->driverVersion.empty());
    assert_true(spec->name.empty());
}

static void test_sbmdParserInvalidBartonMetaType(void **state)
{
    (void) state;

    // bartonMeta should be a map, not a scalar
    const char *yaml = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta: "this should be a map"
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserInvalidMatterMetaType(void **state)
{
    (void) state;

    // matterMeta should be a map, not a scalar
    const char *yaml = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta: "this should be a map"
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserInvalidReportingType(void **state)
{
    (void) state;

    // reporting should be a map, not a scalar
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
reporting: "this should be a map"
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserMapperWithBothAttributeAndCommand(void **state)
{
    (void) state;

    // A mapper cannot have both attribute and command
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
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    mapper:
      read:
        attribute:
          clusterId: "0x0001"
          attributeId: "0x0002"
          name: "TestAttr"
          type: "bool"
        command:
          clusterId: "0x0001"
          commandId: "0x0003"
          name: "TestCmd"
        script: "return value;"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail when read mapper has both attribute and command
    assert_null(spec.get());
}

static void test_sbmdParserMapperWithNeitherAttributeNorCommand(void **state)
{
    (void) state;

    // A mapper must have either attribute or command
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
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    mapper:
      read:
        script: "return value;"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail when read mapper has neither attribute nor command
    assert_null(spec.get());
}

static void test_sbmdParserReadMapperMissingScript(void **state)
{
    (void) state;

    // Read mapper must have a non-empty script
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
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    mapper:
      read:
        attribute:
          clusterId: "0x0001"
          attributeId: "0x0002"
          name: "TestAttr"
          type: "bool"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserWriteMapperMissingScript(void **state)
{
    (void) state;

    // Write mapper must have a non-empty script (write is script-only now)
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
  - id: "testResource"
    type: "boolean"
    modes: ["write"]
    mapper:
      write:
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserExecuteMapperMissingScript(void **state)
{
    (void) state;

    // Execute mapper must have a non-empty script (execute is script-only now)
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
  - id: "testResource"
    type: "function"
    modes: []
    mapper:
      execute:
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserInvalidResourceType(void **state)
{
    (void) state;

    // resource should be a map, not a sequence item that's a scalar
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
  - "this should be a map, not a string"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail on invalid resource type
    assert_null(spec.get());
}

static void test_sbmdParserInvalidEndpointType(void **state)
{
    (void) state;

    // endpoint should be a map, not a scalar
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
resources: []
endpoints:
  - "this should be a map, not a string"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail on invalid endpoint type
    assert_null(spec.get());
}

static void test_sbmdParserInvalidAttributeType(void **state)
{
    (void) state;

    // attribute should be a map, not a scalar
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
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    mapper:
      read:
        attribute: "this should be a map"
        script: "return value;"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail on invalid attribute type
    assert_null(spec.get());
}

static void test_sbmdParserNonexistentFile(void **state)
{
    (void) state;

    auto spec = barton::SbmdParser::ParseFile("/nonexistent/path/to/file.sbmd");
    assert_null(spec.get());
}

static void test_sbmdParserEmptyString(void **state)
{
    (void) state;

    auto spec = barton::SbmdParser::ParseString("");
    // Empty string creates a null YAML node, which results in empty spec
    assert_non_null(spec.get());
    assert_true(spec->schemaVersion.empty());
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        // Positive tests
        cmocka_unit_test(test_sbmdParserReportingSection),
        cmocka_unit_test(test_sbmdParserReportingOptional),
        cmocka_unit_test(test_sbmdParserEndpointWithStringIds),
        cmocka_unit_test(test_sbmdParserDoorLockFile),
        cmocka_unit_test(test_sbmdParserLightFile),
        cmocka_unit_test(test_sbmdParserOptionalResource),
        cmocka_unit_test(test_sbmdParserResourceIdFields),
        // Negative tests - error handling
        cmocka_unit_test(test_sbmdParserInvalidYamlSyntax),
        cmocka_unit_test(test_sbmdParserMissingRequiredFields),
        cmocka_unit_test(test_sbmdParserInvalidBartonMetaType),
        cmocka_unit_test(test_sbmdParserInvalidMatterMetaType),
        cmocka_unit_test(test_sbmdParserInvalidReportingType),
        cmocka_unit_test(test_sbmdParserMapperWithBothAttributeAndCommand),
        cmocka_unit_test(test_sbmdParserMapperWithNeitherAttributeNorCommand),
        cmocka_unit_test(test_sbmdParserReadMapperMissingScript),
        cmocka_unit_test(test_sbmdParserWriteMapperMissingScript),
        cmocka_unit_test(test_sbmdParserExecuteMapperMissingScript),
        cmocka_unit_test(test_sbmdParserInvalidResourceType),
        cmocka_unit_test(test_sbmdParserInvalidEndpointType),
        cmocka_unit_test(test_sbmdParserInvalidAttributeType),
        cmocka_unit_test(test_sbmdParserNonexistentFile),
        cmocka_unit_test(test_sbmdParserEmptyString),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

} // extern "C"
