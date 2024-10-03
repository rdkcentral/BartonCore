//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
#include <icLog/logging.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zhal/zhal.h>

#define LOG_TAG "zhalImplTest"

#define TEST_BLOB                                                                                                      \
    "IgAAAAIAAABOjZwaUSXLWvS16Guy1wavAAAAAAAAAAAAAAAAAAAAAAAAslEDEwIBAABtXeon4QFSYGQQ0gQAVWatGwBaaWdCZWVBbGxpYW5jZTA5" \
    "APj/BwAAAgg="
#define TEST_EUI64                       0x001BAD66550004D2

#define CLUSTER_ID_BASIC                 0x0000
#define ATTRIBUTE_ID_APPLICATION_VERSION 0x0001
#define ATTRIBUTE_ID_HARDWARE_VERSION    0x0003
#define ATTRIBUTE_ID_MANUFACTURER_NAME   0x0004
#define ATTRIBUTE_ID_MODEL_IDENTIFIER    0x0005

#define TEST_TARGET_EUI64                0x000d6f0003c04a7d

/*
 * Created by Thomas Lea on 3/14/16.
 */


static void startup(void *ctx)
{
    icLogDebug(LOG_TAG, "startup callback");
}

static void deviceJoined(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceJoined callback: %016" PRIx64, eui64);
    zhalNetworkDisableJoin();
}

static void deviceLeft(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceLeft callback: %016" PRIx64, eui64);
}

static void deviceRejoined(void *ctx, uint64_t eui64, bool isSecure)
{
    icLogDebug(LOG_TAG, "deviceRejoined callback: %016" PRIx64, eui64);
}

static void attributeReportReceived(void *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG,
               "attributeReportReceived callback: %016" PRIx64 " ep %d, cluster %04x",
               report->eui64,
               report->sourceEndpoint,
               report->clusterId);
}

static void clusterCommandReceived(void *ctx, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG,
               "clusterCommandReceived callback: %016" PRIx64 " ep %d, profileId %04x, cluster %04x",
               command->eui64,
               command->sourceEndpoint,
               command->profileId,
               command->clusterId);
}

static void otaUpgradeMessageSent(void *ctx, OtaUpgradeEvent *otaEvent)
{
    icLogDebug(LOG_TAG, "%s callback: %016" PRIx64, __func__, otaEvent->eui64);
}
static void otaUpgradeMessageReceived(void *ctx, OtaUpgradeEvent *otaEvent)
{
    icLogDebug(LOG_TAG, "%s callback: %016" PRIx64, __func__, otaEvent->eui64);
}

static void deviceCommunicationSucceeded(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceCommunicationSucceeded callback: %016" PRIx64, eui64);
}
static void deviceCommunicationFailed(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "deviceCommunicationFailed callback: %016" PRIx64, eui64);
}

static void networkConfigChanged(void *ctx, char *networkConfigData)
{
    icLogDebug(LOG_TAG, "networkConfigChanged callback: networkConfigData=%s", networkConfigData);
}

static void networkHealthProblem(void *ctx)
{
    icLogDebug(LOG_TAG, "networkHealthProblem callback");
}
static void networkHealthProblemRestored(void *ctx)
{
    icLogDebug(LOG_TAG, "networkHealthProblemRestored callback");
}

void testGetSystemStatus()
{
    zhalSystemStatus status;
    zhalGetSystemStatus(&status);

    char *keybuf = (char *) malloc(sizeof(status.networkKey) * 2 + 1);

    // print it in reverse so it can by copy/pasted
    int pos = 0;
    for (int i = sizeof(status.networkKey) - 1; i >= 0; i--)
    {
        sprintf(keybuf + 2 * (pos++), "%02x", status.networkKey[i]);
    }
    keybuf[sizeof(status.networkKey) * 2] = '\0';

    icLogDebug(LOG_TAG,
               "Got System Status: networkIsUp=%d, networkIsOpenForJoin=%d, eui64=%016" PRIx64
               ", originalEui64=%016" PRIx64 ", channel=%d, panId=%04x, networkKey=%s",
               status.networkIsUp,
               status.networkIsOpenForJoin,
               status.eui64,
               status.originalEui64,
               status.channel,
               status.panId,
               keybuf);

    free(keybuf);
}

int main(int argc, char **argv)
{
    zhalCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.startup = startup;
    callbacks.deviceLeft = deviceLeft;
    callbacks.deviceJoined = deviceJoined;
    callbacks.deviceRejoined = deviceRejoined;
    callbacks.attributeReportReceived = attributeReportReceived;
    callbacks.clusterCommandReceived = clusterCommandReceived;
    callbacks.deviceOtaUpgradeMessageSent = otaUpgradeMessageSent;
    callbacks.deviceOtaUpgradeMessageReceived = otaUpgradeMessageReceived;
    callbacks.deviceCommunicationSucceeded = deviceCommunicationSucceeded;
    callbacks.deviceCommunicationFailed = deviceCommunicationFailed;
    callbacks.networkConfigChanged = networkConfigChanged;
    callbacks.networkHealthProblem = networkHealthProblem;
    callbacks.networkHealthProblemRestored = networkHealthProblemRestored;

    zhalInit("127.0.0.1", 18443, &callbacks, NULL, NULL);

    //    zhalNetworkInit(TEST_EUI64, NULL, TEST_BLOB);

    testGetSystemStatus();

    //    zhalNetworkEnableJoin();

    /*
    uint8_t *endpointIds;
    uint8_t numEndpointIds;
    if(zhalGetEndpointIds(TEST_TARGET_EUI64, &endpointIds, &numEndpointIds) == 0)
    {
        icLogDebug(LOG_TAG, "Found %d endpoints", numEndpointIds);
        for(uint8_t i = 0; i < numEndpointIds; i++)
        {
            icLogDebug(LOG_TAG, "Got endpoint id %d", endpointIds[i]);

            zhalEndpointInfo info;
            if(zhalGetEndpointInfo(TEST_TARGET_EUI64, endpointIds[i], &info) == 0)
            {
                icLogDebug(LOG_TAG, "EndpointInfo:");
                icLogDebug(LOG_TAG, "\tendpointId: %d", info.endpointId);
                icLogDebug(LOG_TAG, "\tappProfileId: %04x", info.appProfileId);
                icLogDebug(LOG_TAG, "\tappDeviceId: %04x", info.appDeviceId);
                icLogDebug(LOG_TAG, "\tappDeviceVersion: %d", info.appDeviceVersion);
                icLogDebug(LOG_TAG, "\tnumServerClusterIds: %d", info.numServerClusterIds);
                for(uint8_t j = 0; j < info.numServerClusterIds; j++)
                {
                    icLogDebug(LOG_TAG, "\t                   : %04x", info.serverClusterIds[j]);
                }
                icLogDebug(LOG_TAG, "\tnumClientClusterIds: %d", info.numClientClusterIds);
                for(uint8_t j = 0; j < info.numClientClusterIds; j++)
                {
                    icLogDebug(LOG_TAG, "\t                   : %04x", info.clientClusterIds[j]);
                }
            }
            else
            {
                icLogError(LOG_TAG, "failed to get endpoint info for %d", i);
            }
        }
    }
     */

    // read some basic cluster attributes
    uint16_t attributeIds[] = {
        ATTRIBUTE_ID_HARDWARE_VERSION, ATTRIBUTE_ID_MANUFACTURER_NAME, ATTRIBUTE_ID_MODEL_IDENTIFIER};
    zhalAttributeData attributeData[3];
    if (zhalAttributesRead(TEST_TARGET_EUI64, 1, CLUSTER_ID_BASIC, true, attributeIds, 3, attributeData) == 0)
    {
        for (int i = 0; i < 3; i++)
        {
            switch (attributeData[i].attributeInfo.id)
            {
                case ATTRIBUTE_ID_HARDWARE_VERSION:
                    icLogDebug(LOG_TAG,
                               "Hardware Version: %d, type=%d, len=%d",
                               attributeData[i].data[0],
                               attributeData[i].attributeInfo.type,
                               attributeData[i].dataLen);
                    break;

                case ATTRIBUTE_ID_MANUFACTURER_NAME:
                {
                    char *str = (char *) calloc(1, attributeData[i].data[0]);
                    memcpy(str, attributeData[i].data + 1, attributeData[i].data[0]);
                    icLogDebug(LOG_TAG,
                               "Manufacturer: %s, type=%d, len=%d",
                               str,
                               attributeData[i].attributeInfo.type,
                               attributeData[i].dataLen);
                    free(str);
                    break;
                }

                case ATTRIBUTE_ID_MODEL_IDENTIFIER:
                {
                    char *str = (char *) calloc(1, attributeData[i].data[0]);
                    memcpy(str, attributeData[i].data + 1, attributeData[i].data[0]);
                    icLogDebug(LOG_TAG,
                               "Model: %s, type=%d, len=%d",
                               str,
                               attributeData[i].attributeInfo.type,
                               attributeData[i].dataLen);
                    free(str);
                    break;
                }

                default:
                    icLogError(LOG_TAG, "unexpected attribute id returned %d", attributeData[i].attributeInfo.id);
                    break;
            }

            if (attributeData[i].data != NULL)
            {
                free(attributeData[i].data);
            }
        }
    }
    else
    {
        icLogError(LOG_TAG, "failed to read attributes");
    }

    //    zhalBindingSet(TEST_TARGET_EUI64, 1, 6);


    /*
    zhalAttributeReportingConfig configs[1];
    configs->attributeInfo.id = 0;
    configs->attributeInfo.type = 0x10;
    configs->minInterval = 1;
    configs->maxInterval = 1620;
    configs->reportableChange = 1;
    zhalAttributesSetReporting(TEST_TARGET_EUI64, 1, 6, configs, 1);
     */

    while (1)
        sleep(10);

    zhalTerm();

    return 0;
}
