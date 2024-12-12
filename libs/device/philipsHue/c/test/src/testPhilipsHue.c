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

#include <philipsHue/philipsHue.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void bridgeFoundCallback(const char *macAddress, const char *ipAddress, const char *username)
{
    printf("bridge found: %s, %s: user: %s\n", macAddress, ipAddress, username);
}

static void testBridgeDiscovery()
{
    philipsHueStartDiscoveringBridges(bridgeFoundCallback);
    usleep(5000000);
    philipsHueStopDiscoveringBridges();
}

static void testGetLights(const char *ipAddress, const char *username)
{
    icLinkedList *lights = philipsHueGetLights(ipAddress, username);

    if (lights != NULL)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(lights);
        while (linkedListIteratorHasNext(iterator))
        {
            PhilipsHueLight *light = (PhilipsHueLight *) linkedListIteratorGetNext(iterator);

            printf("got light id %s, ison = %s\n", light->id, (light->isOn == true) ? "true" : "false");
        }
        linkedListIteratorDestroy(iterator);

        linkedListDestroy(lights, NULL);
    }
}

pthread_cond_t monitoringCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t monitoringMtx = PTHREAD_MUTEX_INITIALIZER;

static void lightChangedCallback(const char *macAddress, const char *lightId, bool isOn)
{
    pthread_mutex_lock(&monitoringMtx);
    printf("lightChanged: %s.%s is now %s\n", macAddress, lightId, (isOn == true) ? "on" : "off");

    if (strcmp(lightId, "3") == 0)
    {
        pthread_cond_signal(&monitoringCond);
    }

    pthread_mutex_unlock(&monitoringMtx);
}

static void lightIpAddressChangedCallback(const char *macAddress, char *newIpAddress)
{
    printf("lightIpAddressChanged: %s is now at %s\n", macAddress, newIpAddress);
}

// Start monitoring lights.  Exit when light id "3" changes state
void testMonitoring(const char *macAddress, const char *ipAddress, const char *username)
{
    pthread_mutex_lock(&monitoringMtx);

    philipsHueStartMonitoring(macAddress, ipAddress, username, lightChangedCallback, lightIpAddressChangedCallback);

    pthread_cond_wait(&monitoringCond, &monitoringMtx);

    pthread_mutex_unlock(&monitoringMtx);
}

int main(int argc, char **argv)
{
    testMonitoring("themac", "172.16.12.116", "25a242962b832472cc0cafa27f6e75b");

    //    testGetLights("172.16.12.116", "25a242962b832472cc0cafa27f6e75b");
    //    philipsHueSetLight("172.16.12.116", "25a242962b832472cc0cafa27f6e75b", "1", true);
    //    philipsHueSetLight("172.16.12.116", "25a242962b832472cc0cafa27f6e75b", "1", false);


    return 0;
}
