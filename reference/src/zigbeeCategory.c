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

#include "zigbeeCategory.h"

#include "barton-core-client.h"
#include "barton-core-reference-io.h"
#include "barton-core-zigbee-energy-scan-result.h"

// The zigbeeSubsystem API lives in Barton's private API tree; the reference app
// is wired up (in CMake, guarded by BCORE_ZIGBEE) to reach it so we can exercise
// the parts of the subsystem that the public client API does not expose.
#include "subsystems/zigbee/zigbeeAttributeTypes.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"

#include <cjson/cJSON.h>
#include <icTypes/icLinkedList.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Small argument parsing helpers
// ---------------------------------------------------------------------------

// EUI64 / device-id addresses are hex without a "0x" prefix (matching the
// zigbee device id format used everywhere else).  An optional "0x" prefix is
// tolerated and ignored (g_ascii_strtoull skips it in base 16).
static uint64_t parseEui64(const gchar *arg)
{
    return (uint64_t) g_ascii_strtoull(arg, NULL, 16);
}

static uint64_t parseNumeric(const gchar *arg)
{
    // Accept either 0x-prefixed hex or plain decimal.
    return (uint64_t) g_ascii_strtoull(arg, NULL, 0);
}

static gboolean parseBool(const gchar *arg)
{
    return g_ascii_strcasecmp(arg, "true") == 0 || g_ascii_strcasecmp(arg, "1") == 0;
}

// Interpret a server/client selector.  Defaults to the server side of the
// cluster (which is the common case) unless "client" is explicitly requested.
static bool parseToServer(const gchar *arg)
{
    return g_ascii_strcasecmp(arg, "client") != 0 && g_ascii_strcasecmp(arg, "false") != 0;
}

// Parse a hex string (e.g. "01ff0a") into a newly allocated byte buffer.  On
// success returns the buffer (caller frees) and sets *outLen; returns NULL for
// a NULL/empty/odd-length/invalid string.
static uint8_t *parseHexPayload(const gchar *hex, uint16_t *outLen)
{
    *outLen = 0;
    if (hex == NULL)
    {
        return NULL;
    }

    size_t len = strlen(hex);
    if (len == 0 || (len % 2) != 0)
    {
        return NULL;
    }

    uint8_t *buf = (uint8_t *) malloc(len / 2);
    for (size_t i = 0; i < len / 2; i++)
    {
        gint hi = g_ascii_xdigit_value(hex[2 * i]);
        gint lo = g_ascii_xdigit_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
        {
            free(buf);
            return NULL;
        }
        buf[i] = (uint8_t) ((hi << 4) | lo);
    }

    *outLen = (uint16_t) (len / 2);
    return buf;
}

// ===========================================================================
// Set 1: BartonCore public API commands (Zigbee related)
// ===========================================================================

static bool changeChannelFunc(BCoreClient *client, gint argc, gchar **argv)
{
    guint8 channel = (guint8) g_ascii_strtoull(argv[0], NULL, 10);
    gboolean dryRun = (argc == 2) && parseBool(argv[1]);

    g_autoptr(GError) error = NULL;
    guint8 result = b_core_client_change_zigbee_channel(client, channel, dryRun, &error);

    if (error != NULL)
    {
        emitError("Failed to change zigbee channel: %s\n", error->message);
        return false;
    }

    emitOutput("Zigbee channel change %s: resulting channel %u\n", dryRun ? "(dry run)" : "applied", result);
    return true;
}

static bool zigbeeTestFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) argc;
    (void) argv;

    g_autofree gchar *result = b_core_client_zigbee_test(client);
    if (result != NULL)
    {
        emitOutput("Zigbee test results: %s\n", result);
        return true;
    }

    emitError("Zigbee test failed\n");
    return false;
}

static bool energyScanFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) argc;

    GList *channels = NULL;
    g_auto(GStrv) parts = g_strsplit(argv[0], ",", -1);
    for (gchar **it = parts; it != NULL && *it != NULL; it++)
    {
        if ((*it)[0] == '\0')
        {
            continue;
        }
        guint channel = (guint) g_ascii_strtoull(*it, NULL, 10);
        channels = g_list_append(channels, GUINT_TO_POINTER(channel));
    }

    if (channels == NULL)
    {
        emitError("No valid channels provided\n");
        return false;
    }

    guint32 durationMs = (guint32) g_ascii_strtoull(argv[1], NULL, 10);
    guint32 scanCount = (guint32) g_ascii_strtoull(argv[2], NULL, 10);

    GList *results = b_core_client_zigbee_energy_scan(client, channels, durationMs, scanCount);
    g_list_free(channels);

    if (results == NULL)
    {
        emitError("Energy scan failed or returned no results\n");
        return false;
    }

    emitOutput("Energy scan results:\n");
    for (GList *it = results; it != NULL; it = it->next)
    {
        BCoreZigbeeEnergyScanResult *result = (BCoreZigbeeEnergyScanResult *) it->data;

        guint channel = 0;
        gint maxRssi = 0;
        gint minRssi = 0;
        gint avgRssi = 0;
        guint score = 0;
        g_object_get(result,
                     B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_CHANNEL],
                     &channel,
                     B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MAX],
                     &maxRssi,
                     B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_MIN],
                     &minRssi,
                     B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_RSSI_AVG],
                     &avgRssi,
                     B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROPERTY_NAMES[B_CORE_ZIGBEE_ENERGY_SCAN_RESULT_PROP_SCORE],
                     &score,
                     NULL);

        emitOutput("\tchannel %u: maxRssi=%d minRssi=%d avgRssi=%d score=%u\n",
                   channel,
                   maxRssi,
                   minRssi,
                   avgRssi,
                   score);
    }

    g_list_free_full(results, g_object_unref);
    return true;
}

// ===========================================================================
// Set 2: zigbeeSubsystem API commands not covered by the public client API
// ===========================================================================

static bool localEui64Func(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    uint64_t eui64 = getLocalEui64();
    char *id = zigbeeSubsystemEui64ToId(eui64);
    emitOutput("Local EUI64: %s\n", id != NULL ? id : "<unknown>");
    free(id);

    return eui64 != 0;
}

static bool firmwareVersionFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    gchar *version = zigbeeSubsystemGetFirmwareVersion();
    if (version != NULL)
    {
        emitOutput("Zigbee firmware version: %s\n", version);
        g_free(version);
        return true;
    }

    emitError("Failed to retrieve zigbee firmware version\n");
    return false;
}

static bool systemStatusFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    ZhalSystemStatus status;
    memset(&status, 0, sizeof(status));

    if (zigbeeSubsystemGetSystemStatus(&status) != 0)
    {
        emitError("Failed to retrieve zigbee system status\n");
        return false;
    }

    emitOutput("Zigbee system status:\n");
    emitOutput("\tnetworkIsUp: %s\n", status.networkIsUp ? "true" : "false");
    emitOutput("\tnetworkIsOpenForJoin: %s\n", status.networkIsOpenForJoin ? "true" : "false");
    emitOutput("\teui64: %016" PRIx64 "\n", (uint64_t) status.eui64);
    emitOutput("\tchannel: %u\n", status.channel);
    emitOutput("\tpanId: 0x%04x\n", status.panId);
    emitOutput("\tversion: %s\n", status.version);

    return true;
}

static bool countersFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    cJSON *counters = zigbeeSubsystemGetAndClearCounters();
    if (counters == NULL)
    {
        emitError("Failed to retrieve zigbee counters\n");
        return false;
    }

    char *json = cJSON_Print(counters);
    emitOutput("Zigbee counters: %s\n", json != NULL ? json : "{}");
    free(json);
    cJSON_Delete(counters);

    return true;
}

static bool networkMapFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    icLinkedList *networkMap = zigbeeSubsystemGetNetworkMap();
    if (networkMap == NULL)
    {
        emitError("Failed to retrieve zigbee network map\n");
        return false;
    }

    emitOutput("Zigbee network map (%d entries):\n", linkedListCount(networkMap));
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(networkMap);
    while (linkedListIteratorHasNext(iter))
    {
        ZigbeeSubsystemNetworkMapEntry *entry = (ZigbeeSubsystemNetworkMapEntry *) linkedListIteratorGetNext(iter);
        if (entry != NULL)
        {
            emitOutput("\taddress=%016" PRIx64 " nextCloserHop=%016" PRIx64 " lqi=%d nodeId=0x%04x\n",
                       (uint64_t) entry->address,
                       (uint64_t) entry->nextCloserHop,
                       entry->lqi,
                       entry->nodeId);
        }
    }

    linkedListDestroy(networkMap, free);
    return true;
}

static bool discoverStartFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    if (zigbeeSubsystemStartDiscoveringDevices() == 0)
    {
        emitOutput("Zigbee device discovery started\n");
        return true;
    }

    emitError("Failed to start zigbee device discovery\n");
    return false;
}

static bool discoverStopFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    if (zigbeeSubsystemStopDiscoveringDevices() == 0)
    {
        emitOutput("Zigbee device discovery stopped\n");
        return true;
    }

    emitError("Failed to stop zigbee device discovery\n");
    return false;
}

static bool requestLeaveFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;

    uint64_t eui64 = parseEui64(argv[0]);
    gboolean withRejoin = (argc >= 2) && parseBool(argv[1]);
    gboolean isEndDevice = (argc >= 3) && parseBool(argv[2]);

    if (zigbeeSubsystemRequestDeviceLeave(eui64, withRejoin, isEndDevice))
    {
        emitOutput("Requested device %016" PRIx64 " leave the network\n", eui64);
        return true;
    }

    emitError("Failed to request device leave\n");
    return false;
}

static bool setAddressesFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;
    (void) argv;

    if (zigbeeSubsystemSetAddresses() == 0)
    {
        emitOutput("Zigbee device addresses configured\n");
        return true;
    }

    emitError("Failed to configure zigbee device addresses\n");
    return false;
}

static bool rejectUnknownFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    gboolean doReject = parseBool(argv[0]);
    zigbeeSubsystemSetRejectUnknownDevices(doReject);
    emitOutput("Reject unknown devices set to %s\n", doReject ? "true" : "false");
    return true;
}

static bool readNumberFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    bool toServer = parseToServer(argv[3]);
    uint16_t attributeId = (uint16_t) parseNumeric(argv[4]);

    uint64_t value = 0;
    if (zigbeeSubsystemReadNumber(eui64, endpointId, clusterId, toServer, attributeId, &value) == 0)
    {
        emitOutput("Read number attribute 0x%04x = %" PRIu64 " (0x%" PRIx64 ")\n", attributeId, value, value);
        return true;
    }

    emitError("Failed to read number attribute\n");
    return false;
}

static bool readStringFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    bool toServer = parseToServer(argv[3]);
    uint16_t attributeId = (uint16_t) parseNumeric(argv[4]);

    char *value = NULL;
    if (zigbeeSubsystemReadString(eui64, endpointId, clusterId, toServer, attributeId, &value) == 0 && value != NULL)
    {
        emitOutput("Read string attribute 0x%04x = \"%s\"\n", attributeId, value);
        free(value);
        return true;
    }

    free(value);
    emitError("Failed to read string attribute\n");
    return false;
}

static bool writeNumberFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    bool toServer = parseToServer(argv[3]);
    uint16_t attributeId = (uint16_t) parseNumeric(argv[4]);
    ZigbeeAttributeType attributeType = (ZigbeeAttributeType) parseNumeric(argv[5]);
    uint64_t value = parseNumeric(argv[6]);
    uint8_t numBytes = (uint8_t) g_ascii_strtoull(argv[7], NULL, 10);

    if (zigbeeSubsystemWriteNumber(eui64, endpointId, clusterId, toServer, attributeId, attributeType, value, numBytes) ==
        0)
    {
        emitOutput("Wrote number attribute 0x%04x = %" PRIu64 "\n", attributeId, value);
        return true;
    }

    emitError("Failed to write number attribute\n");
    return false;
}

static bool bindingGetFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);

    icLinkedList *bindings = zigbeeSubsystemBindingGet(eui64);
    if (bindings == NULL)
    {
        emitError("Failed to retrieve binding table\n");
        return false;
    }

    emitOutput("Binding table for %016" PRIx64 " (%d entries):\n", eui64, linkedListCount(bindings));
    scoped_icLinkedListIterator *iter = linkedListIteratorCreate(bindings);
    while (linkedListIteratorHasNext(iter))
    {
        ZhalBindingTableEntry *entry = (ZhalBindingTableEntry *) linkedListIteratorGetNext(iter);
        if (entry == NULL)
        {
            continue;
        }

        emitOutput("\tsource=%016" PRIx64 " sourceEndpoint=%u clusterId=0x%04x ",
                   (uint64_t) entry->sourceAddress,
                   entry->sourceEndpoint,
                   entry->clusterId);

        if (entry->destinationAddressMode == ZHAL_DESTINATION_ADDRESS_MODE_GROUP)
        {
            emitOutput("-> group 0x%04x\n", entry->destination.groupAddress.groupId);
        }
        else
        {
            emitOutput("-> device %016" PRIx64 " endpoint %u\n",
                       (uint64_t) entry->destination.extendedAddress.eui64,
                       entry->destination.extendedAddress.endpoint);
        }
    }

    linkedListDestroy(bindings, free);
    return true;
}

static bool bindingSetFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);

    if (zigbeeSubsystemBindingSet(eui64, endpointId, clusterId) == 0)
    {
        emitOutput("Binding set for cluster 0x%04x\n", clusterId);
        return true;
    }

    emitError("Failed to set binding\n");
    return false;
}

static bool bindingClearFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);

    if (zigbeeSubsystemBindingClear(eui64, endpointId, clusterId) == 0)
    {
        emitOutput("Binding cleared for cluster 0x%04x\n", clusterId);
        return true;
    }

    emitError("Failed to clear binding\n");
    return false;
}

static bool discoverAttributesFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    bool toServer = parseToServer(argv[3]);

    ZhalAttributeInfo *infos = NULL;
    uint16_t numInfos = 0;
    if (zigbeeSubsystemDiscoverAttributes(eui64, endpointId, clusterId, toServer, &infos, &numInfos) != 0)
    {
        emitError("Failed to discover attributes\n");
        return false;
    }

    emitOutput("Discovered %u attributes on cluster 0x%04x:\n", numInfos, clusterId);
    for (uint16_t i = 0; i < numInfos; i++)
    {
        emitOutput("\tattribute 0x%04x type 0x%02x\n", infos[i].id, infos[i].type);
    }

    free(infos);
    return true;
}

static bool deviceDetailsFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);

    IcDiscoveredDeviceDetails *details = zigbeeSubsystemDiscoverDeviceDetails(eui64);
    if (details == NULL)
    {
        emitError("Failed to discover device details\n");
        return false;
    }

    cJSON *json = icDiscoveredDeviceDetailsToJson(details);
    char *jsonStr = (json != NULL) ? cJSON_Print(json) : NULL;
    emitOutput("Device details for %016" PRIx64 ": %s\n", eui64, jsonStr != NULL ? jsonStr : "<none>");

    free(jsonStr);
    cJSON_Delete(json);
    freeIcDiscoveredDeviceDetails(details);

    return true;
}

static bool sendCommandFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    bool toServer = parseToServer(argv[3]);
    uint8_t commandId = (uint8_t) parseNumeric(argv[4]);

    uint16_t payloadLen = 0;
    uint8_t *payload = NULL;
    if (argc == 6)
    {
        payload = parseHexPayload(argv[5], &payloadLen);
        if (payload == NULL)
        {
            emitError("Invalid hex payload '%s'\n", argv[5]);
            return false;
        }
    }

    int rc = zigbeeSubsystemSendCommand(eui64, endpointId, clusterId, toServer, commandId, payload, payloadLen);
    free(payload);

    if (rc == 0)
    {
        emitOutput("Sent command 0x%02x to cluster 0x%04x\n", commandId, clusterId);
        return true;
    }

    emitError("Failed to send command (rc=%d)\n", rc);
    return false;
}

static bool sendViaApsAckFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    uint8_t sequenceNum = (uint8_t) parseNumeric(argv[3]);

    uint16_t payloadLen = 0;
    uint8_t *payload = NULL;
    if (argc == 5)
    {
        payload = parseHexPayload(argv[4], &payloadLen);
        if (payload == NULL)
        {
            emitError("Invalid hex payload '%s'\n", argv[4]);
            return false;
        }
    }

    int rc = zigbeeSubsystemSendViaApsAck(eui64, endpointId, clusterId, sequenceNum, payload, payloadLen);
    free(payload);

    if (rc == 0)
    {
        emitOutput("Sent APS-acked frame to cluster 0x%04x (seq %u)\n", clusterId, sequenceNum);
        return true;
    }

    emitError("Failed to send APS-acked frame (rc=%d)\n", rc);
    return false;
}

static bool setReportingFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);

    ZhalAttributeReportingConfig config;
    memset(&config, 0, sizeof(config));
    config.attributeInfo.id = (uint16_t) parseNumeric(argv[3]);
    config.attributeInfo.type = (uint8_t) parseNumeric(argv[4]);
    config.minInterval = (uint16_t) parseNumeric(argv[5]);
    config.maxInterval = (uint16_t) parseNumeric(argv[6]);
    config.reportableChange = parseNumeric(argv[7]);

    if (zigbeeSubsystemAttributesSetReporting(eui64, endpointId, clusterId, &config, 1) == 0)
    {
        emitOutput("Configured reporting for attribute 0x%04x on cluster 0x%04x (min %u, max %u)\n",
                   config.attributeInfo.id,
                   clusterId,
                   config.minInterval,
                   config.maxInterval);
        return true;
    }

    emitError("Failed to configure attribute reporting\n");
    return false;
}

static bool getEndpointIdsFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);

    uint8_t *endpointIds = NULL;
    uint8_t numEndpointIds = 0;
    if (zigbeeSubsystemGetEndpointIds(eui64, &endpointIds, &numEndpointIds) != 0)
    {
        emitError("Failed to get endpoint ids\n");
        return false;
    }

    emitOutput("Endpoints for %016" PRIx64 " (%u):", eui64, numEndpointIds);
    for (uint8_t i = 0; i < numEndpointIds; i++)
    {
        emitOutput(" %u", endpointIds[i]);
    }
    emitOutput("\n");

    free(endpointIds);
    return true;
}

static bool bindingClearTargetFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);
    uint8_t endpointId = (uint8_t) g_ascii_strtoull(argv[1], NULL, 10);
    uint16_t clusterId = (uint16_t) parseNumeric(argv[2]);
    uint64_t targetEui64 = parseEui64(argv[3]);
    uint8_t targetEndpointId = (uint8_t) g_ascii_strtoull(argv[4], NULL, 10);

    if (zigbeeSubsystemBindingClearTarget(eui64, endpointId, clusterId, targetEui64, targetEndpointId) == 0)
    {
        emitOutput("Cleared binding on %016" PRIx64 " cluster 0x%04x -> target %016" PRIx64 "\n",
                   eui64,
                   clusterId,
                   targetEui64);
        return true;
    }

    emitError("Failed to clear target binding\n");
    return false;
}

static bool removeDeviceAddressFunc(BCoreClient *client, gint argc, gchar **argv)
{
    (void) client;
    (void) argc;

    uint64_t eui64 = parseEui64(argv[0]);

    if (zigbeeSubsystemRemoveDeviceAddress(eui64) == 0)
    {
        emitOutput("Removed device address %016" PRIx64 "\n", eui64);
        return true;
    }

    emitError("Failed to remove device address\n");
    return false;
}

Category *buildZigbeeCategory(void)
{
    Category *cat = categoryCreate("Zigbee", "Zigbee related commands");

    // -----------------------------------------------------------------------
    // Set 1: BartonCore public API (Zigbee related)
    // -----------------------------------------------------------------------
    Command *command = commandCreate("zigbeeChangeChannel",
                                     "zcc",
                                     "<channel> [dryRun]",
                                     "Change the zigbee channel (channel 0 lets device service pick the best). "
                                     "Pass 'true' for a dry run",
                                     1,
                                     2,
                                     changeChannelFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeTest",
                            "zt",
                            NULL,
                            "Perform a Zigbee test and print the results. NOTE: some implementations require a special test device nearby.",
                            0,
                            0,
                            zigbeeTestFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeEnergyScan",
                            "zes",
                            "<channels> <durationMs> <count>",
                            "Perform an energy scan on a comma separated list of channels (e.g. 11,15,20)",
                            3,
                            3,
                            energyScanFunc);
    categoryAddCommand(cat, command);

    // -----------------------------------------------------------------------
    // Set 2: zigbeeSubsystem API not covered by the public client API
    // -----------------------------------------------------------------------
    command = commandCreate("zigbeeLocalEui64",
                            "zle",
                            NULL,
                            "Print our local Zigbee EUI64",
                            0,
                            0,
                            localEui64Func);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeFirmwareVersion",
                            "zfv",
                            NULL,
                            "Print the Zigbee module firmware version",
                            0,
                            0,
                            firmwareVersionFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeSystemStatus",
                            "zss",
                            NULL,
                            "Print the Zigbee coordinator system status",
                            0,
                            0,
                            systemStatusFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeCounters",
                            "zct",
                            NULL,
                            "Retrieve (and clear) the Zigbee stack counters",
                            0,
                            0,
                            countersFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeNetworkMap",
                            "znm",
                            NULL,
                            "Print the Zigbee network map",
                            0,
                            0,
                            networkMapFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeDiscoverStart",
                            "zds",
                            NULL,
                            "Start discovering (pairing) Zigbee devices",
                            0,
                            0,
                            discoverStartFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeDiscoverStop",
                            "zdx",
                            NULL,
                            "Stop discovering Zigbee devices",
                            0,
                            0,
                            discoverStopFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeRequestLeave",
                            "zrl",
                            "<eui64> [withRejoin] [isEndDevice]",
                            "Request that a Zigbee device leave the network",
                            1,
                            3,
                            requestLeaveFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeSetAddresses",
                            "zsa",
                            NULL,
                            "Push the full list of paired Zigbee device addresses to the stack",
                            0,
                            0,
                            setAddressesFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeRemoveDeviceAddress",
                            "zrda",
                            "<eui64>",
                            "Remove a single Zigbee device address from those allowed on our network",
                            1,
                            1,
                            removeDeviceAddressFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeRejectUnknown",
                            "zru",
                            "<true|false>",
                            "Set whether unknown devices are rejected",
                            1,
                            1,
                            rejectUnknownFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeReadNumber",
                            "zrn",
                            "<eui64> <endpoint> <clusterId> <server|client> <attributeId>",
                            "Read a numeric attribute from a Zigbee device",
                            5,
                            5,
                            readNumberFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeReadString",
                            "zrs",
                            "<eui64> <endpoint> <clusterId> <server|client> <attributeId>",
                            "Read a string attribute from a Zigbee device",
                            5,
                            5,
                            readStringFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeWriteNumber",
                            "zwn",
                            "<eui64> <endpoint> <clusterId> <server|client> <attributeId> <attrType> <value> <numBytes>",
                            "Write a numeric attribute to a Zigbee device (attrType is a ZCL data type, e.g. 0x20)",
                            8,
                            8,
                            writeNumberFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeSendCommand",
                            "zsc",
                            "<eui64> <endpoint> <clusterId> <server|client> <commandId> [hexPayload]",
                            "Send a ZCL cluster command (e.g. OnOff on=0x01/off=0x00/toggle=0x02 on cluster 0x0006)",
                            5,
                            6,
                            sendCommandFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeSendViaApsAck",
                            "zva",
                            "<eui64> <endpoint> <clusterId> <sequenceNum> [hexPayload]",
                            "Send a raw payload to a Zigbee device using an APS-acked frame",
                            4,
                            5,
                            sendViaApsAckFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeBindingGet",
                            "zbg",
                            "<eui64>",
                            "Retrieve the binding table for a Zigbee device",
                            1,
                            1,
                            bindingGetFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeBindingSet",
                            "zbs",
                            "<eui64> <endpoint> <clusterId>",
                            "Create a binding from a Zigbee device cluster to us",
                            3,
                            3,
                            bindingSetFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeBindingClear",
                            "zbc",
                            "<eui64> <endpoint> <clusterId>",
                            "Clear a binding from a Zigbee device cluster to us",
                            3,
                            3,
                            bindingClearFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeBindingClearTarget",
                            "zbct",
                            "<eui64> <endpoint> <clusterId> <targetEui64> <targetEndpoint>",
                            "Clear a binding on a Zigbee device to an arbitrary target (not necessarily us)",
                            5,
                            5,
                            bindingClearTargetFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeDiscoverAttributes",
                            "zda",
                            "<eui64> <endpoint> <clusterId> <server|client>",
                            "Discover the attributes available on a Zigbee cluster",
                            4,
                            4,
                            discoverAttributesFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeSetReporting",
                            "zsr",
                            "<eui64> <endpoint> <clusterId> <attributeId> <attrType> <minInterval> <maxInterval> "
                            "<reportableChange>",
                            "Configure reporting for a single attribute (intervals in seconds)",
                            8,
                            8,
                            setReportingFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeGetEndpointIds",
                            "zei",
                            "<eui64>",
                            "Retrieve the endpoint IDs from a Zigbee device",
                            1,
                            1,
                            getEndpointIdsFunc);
    categoryAddCommand(cat, command);

    command = commandCreate("zigbeeDeviceDetails",
                            "zdd",
                            "<eui64>",
                            "Discover and print the full device details for a Zigbee device",
                            1,
                            1,
                            deviceDetailsFunc);
    categoryAddCommand(cat, command);

    return cat;
}
