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
// clang-format off  // setjmp.h must precede cmocka.h
#include <cmocka.h>
// clang-format on
#include <stdio.h>
#include <string.h>

static void test_sbmdParserReportingSection(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
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
    assert_string_equal(spec->schemaVersion.c_str(), "2.0");
    assert_string_equal(spec->driverVersion.c_str(), "1.0");
    assert_string_equal(spec->scriptType.c_str(), "JavaScript");

    // Verify matterMeta deviceTypes - this catches schema errors like using 'deviceType' instead of 'deviceTypes'
    assert_int_equal((int) spec->matterMeta.deviceTypes.size(), 1);
    assert_int_equal((int) spec->matterMeta.deviceTypes[0], 0x0043);

    // Verify reporting section
    assert_int_equal((int) spec->reporting.minSecs, 5);
    assert_int_equal((int) spec->reporting.maxSecs, 7200);
}

static void test_sbmdParserReportingOptional(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
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
    assert_int_equal((int) spec->reporting.minSecs, 0);
    assert_int_equal((int) spec->reporting.maxSecs, 0);
}

static void test_sbmdParserEndpointWithStringIds(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
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
    assert_int_equal((int) spec->reporting.minSecs, 1);
    assert_int_equal((int) spec->reporting.maxSecs, 3600);

    // Verify endpoints
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_string_equal(spec->endpoints[0].id.c_str(), "1");
    assert_string_equal(spec->endpoints[0].profile.c_str(), "doorLock");

    // Verify locked resource uses read mapper (LockState attribute)
    assert_true(spec->endpoints[0].resources.size() >= 1);
    auto &locked = spec->endpoints[0].resources[0];
    assert_string_equal(locked.id.c_str(), "locked");
    assert_true(locked.mapper.hasRead);
    assert_true(locked.mapper.readAttribute.has_value());
    assert_int_equal((int) locked.mapper.readAttribute->clusterId, 0x0101);
    assert_int_equal((int) locked.mapper.readAttribute->attributeId, 0x0000);
    assert_string_equal(locked.mapper.readAttribute->name.c_str(), "LockState");
    assert_false(locked.mapper.readScript.empty());
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

    // Verify endpoints and core light resources
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_string_equal(spec->endpoints[0].id.c_str(), "1");
    assert_string_equal(spec->endpoints[0].profile.c_str(), "light");
    assert_int_equal((int) spec->endpoints[0].resources.size(), 2);

    // isOn resource maps to OnOff cluster - uses script-only approach
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

    // currentLevel resource maps to LevelControl cluster - uses script-only approach
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
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: requiredAttr
      attribute:
        clusterId: "0x0001"
        attributeId: "0x0002"
        name: "TestAttr"
        type: "bool"
    - name: optionalAttr
      attribute:
        clusterId: "0x0003"
        attributeId: "0x0004"
        name: "TestAttr2"
        type: "bool"
    - name: epRequiredAttr
      attribute:
        clusterId: "0x0005"
        attributeId: "0x0006"
        name: "TestAttr3"
        type: "bool"
    - name: epOptionalAttr
      attribute:
        clusterId: "0x0007"
        attributeId: "0x0008"
        name: "TestAttr4"
        type: "bool"
resources:
  - id: "requiredResource"
    type: "boolean"
    modes: ["read"]
    prerequisites:
      - alias: requiredAttr
    mapper:
      read:
        alias: requiredAttr
        script: "return value;"
  - id: "optionalResource"
    type: "boolean"
    optional: true
    modes: ["read"]
    prerequisites:
      - alias: optionalAttr
    mapper:
      read:
        alias: optionalAttr
        script: "return value;"
endpoints:
  - id: "ep1"
    profile: "sensor"
    profileVersion: 3
    resources:
      - id: "epRequired"
        type: "boolean"
        modes: ["read"]
        prerequisites:
          - alias: epRequiredAttr
        mapper:
          read:
            alias: epRequiredAttr
            script: "return value;"
      - id: "epOptional"
        type: "boolean"
        optional: true
        modes: ["read"]
        prerequisites:
          - alias: epOptionalAttr
        mapper:
          read:
            alias: epOptionalAttr
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
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: testAttr
      attribute:
        clusterId: "0x0001"
        attributeId: "0x0002"
        name: "TestAttr"
        type: "bool"
    - name: testAttr2
      attribute:
        clusterId: "0x0003"
        attributeId: "0x0004"
        name: "TestAttr2"
        type: "bool"
resources:
  - id: "rootResource"
    type: "boolean"
    modes: ["read"]
    prerequisites:
      - alias: testAttr
    mapper:
      read:
        alias: testAttr
        script: "return value;"
endpoints:
  - id: "ep1"
    profile: "sensor"
    profileVersion: 3
    resources:
      - id: "endpointResource"
        type: "boolean"
        modes: ["read", "write"]
        prerequisites:
          - alias: testAttr2
        mapper:
          read:
            alias: testAttr2
            script: "return value;"
          write:
            script: "return value;"
      - id: "executeResource"
        type: "function"
        modes: []
        prerequisites: none
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
schemaVersion: "2.0"
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

    // Specs without schemaVersion should now be rejected
    const char *emptyYaml = R"(
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(emptyYaml);
    assert_null(spec.get());
}

static void test_sbmdParserWrongSchemaVersion(void **state)
{
    (void) state;

    // Wrong major version (1.x) — rejected regardless of minor
    const char *yaml1 = R"(
schemaVersion: "1.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml1);
    assert_null(spec.get());

    // Wrong major version (3.x) — rejected regardless of minor
    const char *yaml2 = R"(
schemaVersion: "3.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources: []
endpoints: []
)";

    spec = barton::SbmdParser::ParseString(yaml2);
    assert_null(spec.get());

    // Correct major but spec minor is newer than the parser supports — rejected
    // (a spec written for schema 2.1 cannot be loaded by a 2.0 parser)
    const char *yaml3 = R"(
schemaVersion: "2.1"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources: []
endpoints: []
)";

    spec = barton::SbmdParser::ParseString(yaml3);
    assert_null(spec.get());

    // Trailing component — "2.0.1" must be rejected (sscanf would ignore ".1" without the %n check)
    const char *yaml4 = R"(
schemaVersion: "2.0.1"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources: []
endpoints: []
)";

    spec = barton::SbmdParser::ParseString(yaml4);
    assert_null(spec.get());

    // Negative minor — "2.-1" must be rejected (specMinor = -1 would otherwise satisfy specMinor <= 0)
    const char *yaml5 = R"(
schemaVersion: "2.-1"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
resources: []
endpoints: []
)";

    spec = barton::SbmdParser::ParseString(yaml5);
    assert_null(spec.get());
}
{
    (void) state;

    // bartonMeta should be a map, not a scalar
    const char *yaml = R"(
schemaVersion: "2.0"
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
schemaVersion: "2.0"
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
schemaVersion: "2.0"
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

static void test_sbmdParserReadMapperRejectsEventAlias(void **state)
{
    (void) state;

    // A read mapper must reference an attribute alias; referencing an event alias should fail
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: lockOp
      event:
        clusterId: "0x0101"
        eventId: "0x0002"
        name: "LockOperation"
resources:
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    prerequisites:
      - alias: lockOp
    mapper:
      read:
        alias: lockOp
        script: "return value;"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail: read mapper alias must be an attribute alias, not an event alias
    assert_null(spec.get());
}

static void test_sbmdParserReadMapperRejectsBothAliasAndCommand(void **state)
{
    (void) state;

    // A read mapper must have exactly one of 'alias' or 'command', not both
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: testAttr
      attribute:
        clusterId: "0x0001"
        attributeId: "0x0002"
        name: "TestAttr"
        type: "bool"
resources:
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    prerequisites:
      - alias: testAttr
    mapper:
      read:
        alias: testAttr
        command:
          clusterId: "0x0001"
          commandId: "0x0000"
          name: "TestCommand"
        script: "return value;"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail: read mapper cannot have both 'alias' and 'command'
    assert_null(spec.get());
}

static void test_sbmdParserMapperWithNeitherAttributeNorCommand(void **state)
{
    (void) state;

    // A read mapper must have either 'alias' or 'command'
    const char *yaml = R"(
schemaVersion: "2.0"
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
    prerequisites: none
    mapper:
      read:
        script: "return value;"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail when read mapper has neither alias nor command
    assert_null(spec.get());
}

static void test_sbmdParserReadMapperMissingScript(void **state)
{
    (void) state;

    // Read mapper must have a non-empty script
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: testAttr
      attribute:
        clusterId: "0x0001"
        attributeId: "0x0002"
        name: "TestAttr"
        type: "bool"
resources:
  - id: "testResource"
    type: "boolean"
    modes: ["read"]
    prerequisites:
      - alias: testAttr
    mapper:
      read:
        alias: testAttr
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
schemaVersion: "2.0"
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
schemaVersion: "2.0"
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
schemaVersion: "2.0"
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
schemaVersion: "2.0"
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

static void test_sbmdParserAliasRejectsBothAttributeAndEvent(void **state)
{
    (void) state;

    // An alias must have exactly one of 'attribute' or 'event', not both
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: badAlias
      attribute:
        clusterId: "0x0001"
        attributeId: "0x0000"
        name: "TestAttr"
        type: "bool"
      event:
        clusterId: "0x0001"
        eventId: "0x0000"
        name: "TestEvent"
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail: alias cannot have both attribute and event
    assert_null(spec.get());
}

static void test_sbmdParserDuplicateAliasName(void **state)
{
    (void) state;

    // Two aliases with the same name make FindAlias() ambiguous and must be rejected
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: stateValue
      attribute:
        clusterId: "0x0045"
        attributeId: "0x0000"
        name: "StateValue"
        type: "bool"
    - name: stateValue
      attribute:
        clusterId: "0x0046"
        attributeId: "0x0001"
        name: "OtherValue"
        type: "uint8"
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail: duplicate alias name
    assert_null(spec.get());
}

static void test_sbmdParserAliasEmptyName(void **state)
{
    (void) state;

    // An alias with an empty string for 'name' must be rejected
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: ""
      attribute:
        clusterId: "0x0045"
        attributeId: "0x0000"
        name: "StateValue"
        type: "bool"
resources: []
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail: alias name must not be empty
    assert_null(spec.get());
}

static void test_sbmdParserEmptyPrerequisitesList(void **state)
{
    (void) state;

    // An empty sequence is not a valid opt-out; 'prerequisites: none' must be used instead
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 2
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: stateValue
      attribute:
        clusterId: "0x0045"
        attributeId: "0x0000"
        name: "StateValue"
        type: "bool"
resources:
  - id: r.state
    type: SENSOR.BOOLEAN
    prerequisites: []
    mapper:
      read:
        alias: stateValue
        script: "return attr"
endpoints: []
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    // Parser should fail: empty prerequisites sequence must be rejected
    assert_null(spec.get());
}

static void test_sbmdParserNonexistentFile(void **state)
{
    (void) state;

    auto spec = barton::SbmdParser::ParseFile("/nonexistent/path/to/file.sbmd");
    assert_null(spec.get());
}

// ============================================================================
// Prerequisites Tests
// ============================================================================

static void test_prerequisiteFromReadMapper(void **state)
{
    (void) state;

    // Renamed intent: prerequisite from attribute alias resolves clusterId + attributeId
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: humidity
      attribute:
        clusterId: "0x0405"
        attributeId: "0x0000"
        name: "MeasuredValue"
        type: "uint16"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "humidity"
        type: "com.icontrol.humidity"
        modes: ["read"]
        prerequisites:
          - alias: humidity
        mapper:
          read:
            alias: humidity
            script: "return {output: 'test'};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());
    assert_int_equal((int) spec->endpoints.size(), 1);
    assert_int_equal((int) spec->endpoints[0].resources.size(), 1);

    auto &resource = spec->endpoints[0].resources[0];
    assert_int_equal((int) resource.prerequisites.size(), 1);
    // Alias resolved at parse time: clusterId and attributeId populated, no mapperRef
    assert_int_equal((int) resource.prerequisites[0].clusterId, 0x0405);
    assert_int_equal((int) resource.prerequisites[0].attributeIds.size(), 1);
    assert_int_equal((int) resource.prerequisites[0].attributeIds[0], 0x0000);

    // Verify mapper also resolved alias
    assert_true(resource.mapper.hasRead);
    assert_true(resource.mapper.readAttribute.has_value());
    assert_int_equal((int) resource.mapper.readAttribute->clusterId, 0x0405);
    assert_int_equal((int) resource.mapper.readAttribute->attributeId, 0x0000);
    assert_string_equal(resource.mapper.readAttribute->name.c_str(), "MeasuredValue");
}

static void test_prerequisiteFromEventMapper(void **state)
{
    (void) state;

    // Renamed intent: prerequisite from event alias resolves clusterId only (no attribute check)
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x000a"
  revision: 1
  aliases:
    - name: lockOp
      event:
        clusterId: "0x0101"
        eventId: "0x0002"
        name: "LockOperation"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "locked"
        type: "boolean"
        modes: ["read", "dynamic", "emitEvents"]
        prerequisites:
          - alias: lockOp
        mapper:
          event:
            alias: lockOp
            script: "return {output: 'true'};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());
    assert_int_equal((int) spec->endpoints[0].resources.size(), 1);

    auto &resource = spec->endpoints[0].resources[0];
    assert_int_equal((int) resource.prerequisites.size(), 1);
    // Event alias: clusterId resolved, attributeIds empty (cluster-only check)
    assert_int_equal((int) resource.prerequisites[0].clusterId, 0x0101);
    assert_int_equal((int) resource.prerequisites[0].attributeIds.size(), 0);

    // Verify mapper event resolved
    assert_true(resource.mapper.event.has_value());
    assert_int_equal((int) resource.mapper.event->clusterId, 0x0101);
    assert_int_equal((int) resource.mapper.event->eventId, 0x0002);
    assert_string_equal(resource.mapper.event->name.c_str(), "LockOperation");
}

static void test_prerequisiteAttributeAliasResolvesClusterAndAttribute(void **state)
{
    (void) state;

    // An attribute alias used as a prerequisite resolves both clusterId and attributeId
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: humidityCluster
      attribute:
        clusterId: "0x0405"
        attributeId: "0x0000"
        name: "MeasuredValue"
        type: "uint16"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "humidity"
        type: "com.icontrol.humidity"
        modes: ["read"]
        prerequisites:
          - alias: humidityCluster
        mapper:
          read:
            alias: humidityCluster
            script: "return {output: 'test'};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    auto &resource = spec->endpoints[0].resources[0];
    assert_int_equal((int) resource.prerequisites.size(), 1);
    assert_int_equal((int) resource.prerequisites[0].clusterId, 0x0405);
    // Attribute alias always resolves attributeId
    assert_int_equal((int) resource.prerequisites[0].attributeIds.size(), 1);
    assert_int_equal((int) resource.prerequisites[0].attributeIds[0], 0x0000);
}

static void test_prerequisiteAliasIndependentOfMapperAlias(void **state)
{
    (void) state;

    // The prerequisite alias and the mapper alias may differ; each is resolved independently
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: measuredValue
      attribute:
        clusterId: "0x0405"
        attributeId: "0x0000"
        name: "MeasuredValue"
        type: "uint16"
    - name: tolerance
      attribute:
        clusterId: "0x0405"
        attributeId: "0x0003"
        name: "Tolerance"
        type: "uint16"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "humidityTolerance"
        type: "com.icontrol.humidity"
        modes: ["read"]
        prerequisites:
          - alias: tolerance
        mapper:
          read:
            alias: measuredValue
            script: "return {output: 'test'};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    auto &resource = spec->endpoints[0].resources[0];
    assert_int_equal((int) resource.prerequisites.size(), 1);
    assert_int_equal((int) resource.prerequisites[0].clusterId, 0x0405);
    assert_int_equal((int) resource.prerequisites[0].attributeIds.size(), 1);
    assert_int_equal((int) resource.prerequisites[0].attributeIds[0], 0x0003);
}

static void test_prerequisiteNone(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: stateValue
      attribute:
        clusterId: "0x0045"
        attributeId: "0x0000"
        name: "StateValue"
        type: "bool"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "faulted"
        type: "boolean"
        modes: ["read"]
        prerequisites: none
        mapper:
          read:
            alias: stateValue
            script: "return {output: 'true'};"
)";

    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_non_null(spec.get());

    auto &resource = spec->endpoints[0].resources[0];
    // prerequisites: none -> empty vector, always register
    assert_int_equal((int) resource.prerequisites.size(), 0);
}

static void test_prerequisiteMissingOnReadMapper(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: stateValue
      attribute:
        clusterId: "0x0045"
        attributeId: "0x0000"
        name: "StateValue"
        type: "bool"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "faulted"
        type: "boolean"
        modes: ["read"]
        mapper:
          read:
            alias: stateValue
            script: "return {output: 'true'};"
)";

    // Must fail: read mapper present but no prerequisites declared
    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_prerequisiteMissingOnEventMapper(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x000a"
  revision: 1
  aliases:
    - name: lockOp
      event:
        clusterId: "0x0101"
        eventId: "0x0002"
        name: "LockOperation"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "locked"
        type: "boolean"
        modes: ["read", "dynamic", "emitEvents"]
        mapper:
          event:
            alias: lockOp
            script: "return {output: 'true'};"
)";

    // Must fail: event mapper present but no prerequisites declared
    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_prerequisiteNotRequiredForWriteMapper(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "setLevel"
        type: "com.icontrol.lightLevel"
        optional: true
        modes: ["write"]
        mapper:
          write:
            script: "return SbmdUtils.Response.invoke(0x0008, 0x0004);"
)";

    // Must fail: prerequisites is required on every resource
    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_prerequisiteNotRequiredForExecuteMapper(void **state)
{
    (void) state;

    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x000a"
  revision: 1
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "lock"
        type: "function"
        mapper:
          execute:
            script: "return SbmdUtils.Response.invoke(0x0101, 0x0000);"
)";

    // Must fail: prerequisites is required on every resource
    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_prerequisiteEntryUnknownKey(void **state)
{
    (void) state;

    // A prerequisite entry with an unexpected key must be rejected (additionalProperties: false)
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: humidity
      attribute:
        clusterId: "0x0405"
        attributeId: "0x0000"
        name: "MeasuredValue"
        type: "uint16"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "humidity"
        type: "com.icontrol.humidity"
        modes: ["read"]
        prerequisites:
          - alias: humidity
            typo: unexpected
        mapper:
          read:
            alias: humidity
            script: "return {output: 'test'};"
)";

    // Must fail: prerequisite entry has unexpected key 'typo'
    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_prerequisiteInvalidBothForms(void **state)
{
    (void) state;

    // A prerequisite entry that references a nonexistent alias should fail
    const char *yaml = R"(
schemaVersion: "2.0"
driverVersion: "1.0"
name: "Test Device"
bartonMeta:
  deviceClass: "sensor"
  deviceClassVersion: 1
matterMeta:
  deviceTypes:
    - "0x0043"
  revision: 1
  aliases:
    - name: humidity
      attribute:
        clusterId: "0x0405"
        attributeId: "0x0000"
        name: "MeasuredValue"
        type: "uint16"
endpoints:
  - id: "1"
    profile: "sensor"
    profileVersion: 1
    resources:
      - id: "humidity"
        type: "com.icontrol.humidity"
        modes: ["read"]
        prerequisites:
          - alias: nonExistentAlias
        mapper:
          read:
            alias: humidity
            script: "return {output: 'test'};"
)";

    // Must fail: prerequisite references unknown alias
    auto spec = barton::SbmdParser::ParseString(yaml);
    assert_null(spec.get());
}

static void test_sbmdParserEmptyString(void **state)
{
    (void) state;

    auto spec = barton::SbmdParser::ParseString("");
    // Empty string creates a null YAML node; missing schemaVersion causes parse failure
    assert_null(spec.get());
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
        cmocka_unit_test(test_sbmdParserWrongSchemaVersion),
        cmocka_unit_test(test_sbmdParserInvalidBartonMetaType),
        cmocka_unit_test(test_sbmdParserInvalidMatterMetaType),
        cmocka_unit_test(test_sbmdParserInvalidReportingType),
        cmocka_unit_test(test_sbmdParserReadMapperRejectsEventAlias),
        cmocka_unit_test(test_sbmdParserReadMapperRejectsBothAliasAndCommand),
        cmocka_unit_test(test_sbmdParserMapperWithNeitherAttributeNorCommand),
        cmocka_unit_test(test_sbmdParserReadMapperMissingScript),
        cmocka_unit_test(test_sbmdParserWriteMapperMissingScript),
        cmocka_unit_test(test_sbmdParserExecuteMapperMissingScript),
        cmocka_unit_test(test_sbmdParserInvalidResourceType),
        cmocka_unit_test(test_sbmdParserInvalidEndpointType),
        cmocka_unit_test(test_sbmdParserAliasRejectsBothAttributeAndEvent),
        cmocka_unit_test(test_sbmdParserDuplicateAliasName),
        cmocka_unit_test(test_sbmdParserAliasEmptyName),
        cmocka_unit_test(test_sbmdParserEmptyPrerequisitesList),
        cmocka_unit_test(test_sbmdParserNonexistentFile),
        cmocka_unit_test(test_sbmdParserEmptyString),
        // Prerequisites tests
        cmocka_unit_test(test_prerequisiteFromReadMapper),
        cmocka_unit_test(test_prerequisiteFromEventMapper),
        cmocka_unit_test(test_prerequisiteAttributeAliasResolvesClusterAndAttribute),
        cmocka_unit_test(test_prerequisiteAliasIndependentOfMapperAlias),
        cmocka_unit_test(test_prerequisiteNone),
        cmocka_unit_test(test_prerequisiteMissingOnReadMapper),
        cmocka_unit_test(test_prerequisiteMissingOnEventMapper),
        cmocka_unit_test(test_prerequisiteNotRequiredForWriteMapper),
        cmocka_unit_test(test_prerequisiteNotRequiredForExecuteMapper),
        cmocka_unit_test(test_prerequisiteEntryUnknownKey),
        cmocka_unit_test(test_prerequisiteInvalidBothForms),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

} // extern "C"
