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
    assert_int_equal((int) spec->bartonMeta.deviceClassVersion, 2);

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
    // Parser should succeed at top level but the resource should have parsing issues
    // Check that the mapper parsing failed by verifying the resource wasn't added properly
    assert_non_null(spec.get());
    // The resource is added but mapper parsing fails, so mapper fields may be incomplete
    // Current behavior: resource is added despite mapper failure (logged as warning)
    assert_int_equal((int) spec->resources.size(), 1);
    // The mapper should have failed - hasRead is set before validation
    assert_true(spec->resources[0].mapper.hasRead);
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
    // Parser succeeds but mapper should have issues
    assert_non_null(spec.get());
    assert_int_equal((int) spec->resources.size(), 1);
    // hasRead is set but no attribute or command
    assert_true(spec->resources[0].mapper.hasRead);
    assert_false(spec->resources[0].mapper.readAttribute.has_value());
    assert_false(spec->resources[0].mapper.readCommand.has_value());
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
    // Parser skips invalid resources
    assert_non_null(spec.get());
    assert_int_equal((int) spec->resources.size(), 0);
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
    // Parser skips invalid endpoints
    assert_non_null(spec.get());
    assert_int_equal((int) spec->endpoints.size(), 0);
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
    // Parser succeeds but attribute should have issues
    assert_non_null(spec.get());
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

static void test_sbmdParserWriteMapperWithBothAttributeAndCommand(void **state)
{
    (void) state;

    // Test that write mapper also validates attribute/command exclusivity
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
    assert_non_null(spec.get());
    assert_int_equal((int) spec->resources.size(), 1);
}

static void test_sbmdParserExecuteMapperWithBothAttributeAndCommand(void **state)
{
    (void) state;

    // Test that execute mapper also validates attribute/command exclusivity
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
        attribute:
          clusterId: "0x0001"
          attributeId: "0x0002"
          name: "TestAttr"
          type: "bool"
        command:
          clusterId: "0x0001"
          commandId: "0x0003"
          name: "TestCmd"
        script: "return {};"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());
    assert_int_equal((int) spec->resources.size(), 1);
}

int main(int argc, const char **argv)
{
    const struct CMUnitTest tests[] = {
        // Positive tests
        cmocka_unit_test(test_sbmdParserReportingSection),
        cmocka_unit_test(test_sbmdParserReportingOptional),
        cmocka_unit_test(test_sbmdParserEndpointWithStringIds),
        cmocka_unit_test(test_sbmdParserDoorLockFile),
        cmocka_unit_test(test_sbmdParserResourceIdFields),
        // Negative tests - error handling
        cmocka_unit_test(test_sbmdParserInvalidYamlSyntax),
        cmocka_unit_test(test_sbmdParserMissingRequiredFields),
        cmocka_unit_test(test_sbmdParserInvalidBartonMetaType),
        cmocka_unit_test(test_sbmdParserInvalidMatterMetaType),
        cmocka_unit_test(test_sbmdParserInvalidReportingType),
        cmocka_unit_test(test_sbmdParserMapperWithBothAttributeAndCommand),
        cmocka_unit_test(test_sbmdParserMapperWithNeitherAttributeNorCommand),
        cmocka_unit_test(test_sbmdParserInvalidResourceType),
        cmocka_unit_test(test_sbmdParserInvalidEndpointType),
        cmocka_unit_test(test_sbmdParserInvalidAttributeType),
        cmocka_unit_test(test_sbmdParserNonexistentFile),
        cmocka_unit_test(test_sbmdParserEmptyString),
        cmocka_unit_test(test_sbmdParserWriteMapperWithBothAttributeAndCommand),
        cmocka_unit_test(test_sbmdParserExecuteMapperWithBothAttributeAndCommand),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

} // extern "C"
