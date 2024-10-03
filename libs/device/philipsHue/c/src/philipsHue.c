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

/*-----------------------------------------------
 * philipsHue.c
 *
 * Author: tlea
 *-----------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <icLog/logging.h>
#include <philipsHue/philipsHue.h>
#include <pthread.h>
#include <ssdp/ssdp.h>
#include <stdlib.h>
#include <unistd.h>

#define LOG_TAG                  "PhueLib"

#define MONITOR_INTERVAL_SECS    5

#define RECOVERY_TIMEOUT_SECONDS 10

static void localBridgeDiscoveredCallback(SsdpDevice *device);

static pthread_mutex_t discoverMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    pthread_t thread;
    bool running;
    char *macAddress;
    char *ipAddress;
    char *username;
    icHashMap *lights; // last known state
    PhilipsHueLightChangedCallback lightChangedCallback;
    PhilipsHueIpAddressChangedCallback ipChangedCallback;
} BridgeMonitoringInfo;

static icHashMap *monitors = NULL;
static pthread_mutex_t monitorsMutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t ssdpHandle = 0;

static PhilipsHueBridgeDiscoverCallback clientBridgeDiscoveredCallback = NULL;

bool philipsHueStartDiscoveringBridges(PhilipsHueBridgeDiscoverCallback callback)
{
    bool result = false;

    pthread_mutex_lock(&discoverMutex);
    if (clientBridgeDiscoveredCallback == NULL)
    {
        clientBridgeDiscoveredCallback = callback;
        ssdpHandle = ssdpDiscoverStart(PHILIPSHUE, localBridgeDiscoveredCallback);
        if (ssdpHandle != 0)
        {
            result = true;
        }

        if (result == false)
        {
            clientBridgeDiscoveredCallback = NULL;
        }
    }
    pthread_mutex_unlock(&discoverMutex);

    return result;
}

bool philipsHueStopDiscoveringBridges()
{
    ssdpDiscoverStop(ssdpHandle);
    ssdpHandle = 0;
    pthread_mutex_lock(&discoverMutex);
    clientBridgeDiscoveredCallback = NULL;
    pthread_mutex_unlock(&discoverMutex);
    return true;
}

struct MemoryBlock
{
    char *memory;
    size_t size;
};

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryBlock *mem = (struct MemoryBlock *) userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL)
    {
        icLogError(LOG_TAG, "Not enough memory in %s (realloc returned NULL) . . .", __func__);
        return (size_t) -1;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static bool createUser(const char *ipAddress, char **username)
{
    CURL *curl;
    CURLcode res;

    *username = NULL;

    curl = curl_easy_init();
    if (curl)
    {
        struct MemoryBlock responseBlock;
        responseBlock.memory = malloc(1);
        responseBlock.size = 0;

        char url[128];
        sprintf(url, "http://%s/api", ipAddress);

        char *body = strdup("{\"devicetype\":\"icontrol#fcore\"}");

        struct curl_slist *header = NULL;

        header = curl_slist_append(header, "Accept: application/json");
        header = curl_slist_append(header, "Content-Type: application/json");
        header = curl_slist_append(header, "charsets: utf-8");

        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &responseBlock);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(responseBlock.memory);
            free(body);
            curl_slist_free_all(header);
            return false;
        }

        curl_easy_cleanup(curl);

        icLogDebug(LOG_TAG, "got response %s", responseBlock.memory);

        cJSON *responseObj = cJSON_Parse(responseBlock.memory);
        if (responseObj != NULL)
        {
            cJSON *tmp = cJSON_GetArrayItem(responseObj, 0);

            if (tmp != NULL)
            {
                cJSON *successObj = cJSON_GetObjectItem(tmp, "success");
                if (successObj != NULL)
                {
                    cJSON *userObj = cJSON_GetObjectItem(successObj, "username");
                    if (userObj != NULL)
                    {
                        *username = strdup(userObj->valuestring);
                    }
                }
            }

            cJSON_Delete(responseObj);
        }

        free(responseBlock.memory);
        free(body);
        curl_slist_free_all(header);
    }

    if (*username != NULL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void localBridgeDiscoveredCallback(SsdpDevice *device)
{
    icLogInfo(LOG_TAG, "Bridge found: ip=%s, st=%s, url=%s", device->ipAddress, device->upnpST, device->upnpURL);

    char *username = NULL;

    // we will stop trying if discovery stops
    int count = 0;
    while (clientBridgeDiscoveredCallback != NULL && count < 10)
    {
        if (createUser(device->ipAddress, &username) == true)
        {
            break;
        }

        // the user probably hasnt pushed the link button yet... wait a little and try again
        usleep(1000000);
        count++;
    }

    pthread_mutex_lock(&discoverMutex);
    if (username != NULL && clientBridgeDiscoveredCallback != NULL)
    {
        clientBridgeDiscoveredCallback(device->macAddress, device->ipAddress, username);
    }
    pthread_mutex_unlock(&discoverMutex);

    if (username != NULL)
    {
        free(username);
    }
}

icLinkedList *philipsHueGetLights(const char *ipAddress, const char *username)
{
    icLinkedList *result = NULL;

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        struct MemoryBlock responseBlock;
        responseBlock.memory = malloc(1);
        responseBlock.size = 0;

        char url[128];
        sprintf(url, "http://%s/api/%s/lights", ipAddress, username);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &responseBlock);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(responseBlock.memory);
            return NULL;
        }

        curl_easy_cleanup(curl);

        //        icLogDebug(LOG_TAG, "got response %s", responseBlock.memory);

        cJSON *responseObj = cJSON_Parse(responseBlock.memory);

        if (responseObj != NULL)
        {
            result = linkedListCreate();
            cJSON *lightObj = responseObj->child;
            while (lightObj != NULL)
            {
                PhilipsHueLight *light = (PhilipsHueLight *) calloc(1, sizeof(PhilipsHueLight));

                light->id = strdup(lightObj->string);

                cJSON *state = cJSON_GetObjectItem(lightObj, "state");
                if (state != NULL)
                {
                    cJSON *on = cJSON_GetObjectItem(state, "on");
                    if (on != NULL && on->valueint == 1)
                    {
                        light->isOn = true;
                    }
                }

                linkedListAppend(result, light);

                lightObj = lightObj->next;
            }

            cJSON_Delete(responseObj);
        }

        free(responseBlock.memory);
    }

    return result;
}

bool philipsHueSetLight(const char *ipAddress, const char *username, const char *lightId, bool on)
{
    bool result = false;
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        char url[128];
        sprintf(url, "http://%s/api/%s/lights/%s/state", ipAddress, username, lightId);

        char *body;
        if (on == true)
        {
            body = strdup("{\"on\":true}");
        }
        else
        {
            body = strdup("{\"on\":false}");
        }

        struct curl_slist *header = NULL;

        header = curl_slist_append(header, "Accept: application/json");
        header = curl_slist_append(header, "Content-Type: application/json");
        header = curl_slist_append(header, "charsets: utf-8");

        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(body);
            curl_slist_free_all(header);
            return false;
        }
        else
        {
            result = true;
        }

        curl_easy_cleanup(curl);

        free(body);
        curl_slist_free_all(header);
    }

    return result;
}

void philipsHueLightDestroy(PhilipsHueLight *light)
{
    if (light->id != NULL)
    {
        free(light->id);
        light->id = NULL;
    }

    free(light);
}

static void lightFreeFunc(void *key, void *value)
{
    free(key);
    philipsHueLightDestroy((PhilipsHueLight *) value);
}

static void destroyBridgeMonitoringInfo(BridgeMonitoringInfo *info)
{
    if (info->macAddress != NULL)
    {
        free(info->macAddress);
        info->macAddress = NULL;
    }
    if (info->ipAddress != NULL)
    {
        free(info->ipAddress);
        info->ipAddress = NULL;
    }
    if (info->username != NULL)
    {
        free(info->username);
        info->username = NULL;
    }

    hashMapDestroy(info->lights, lightFreeFunc);

    free(info);
}

/*
 * thread to monitor a single philip hue ipAddress (the bridge)
 */
static void *bridgeMonitoringThreadProc(void *arg)
{
    BridgeMonitoringInfo *info = (BridgeMonitoringInfo *) arg;
    while (info->running == true)
    {
        // load the list of all known light objects (from the hue hub)
        //
        icLinkedList *lights = philipsHueGetLights(info->ipAddress, info->username);
        if (lights != NULL)
        {
            // loop through the list of light devices so we can update status and state
            //
            icLinkedListIterator *iterator = linkedListIteratorCreate(lights);
            while (linkedListIteratorHasNext(iterator))
            {
                PhilipsHueLight *light = (PhilipsHueLight *) linkedListIteratorGetNext(iterator);

                PhilipsHueLight *previousLight =
                    (PhilipsHueLight *) hashMapGet(info->lights, light->id, (uint16_t) (strlen(light->id) + 1));
                if (previousLight == NULL)
                {
                    // this is the first time we have seen this light, just save it;
                    // stealing the object from the temp list and placing it into the hash
                    hashMapPut(info->lights, strdup(light->id), (uint16_t) (strlen(light->id) + 1), light);
                }
                else
                {
                    if (previousLight->isOn != light->isOn)
                    {
                        // the state has changed, invoke the callback
                        info->lightChangedCallback(info->macAddress, light->id, light->isOn);
                        previousLight->isOn = light->isOn;
                    }

                    // not stealing 'light', so free it
                    philipsHueLightDestroy(light);
                }
            }

            // cleanup, note we don't delete the elements from 'lights' as each
            // element should have been stolen or released
            //
            linkedListIteratorDestroy(iterator);
            linkedListDestroy(lights, standardDoNotFreeFunc);
        }
        else
        {
            // we failed to get any lights from the device.  That most likely means its IP address changed and we have
            // to find it again
            icLogInfo(LOG_TAG,
                      "bridgeMonitoringThreadProc: didn't get any lights from %s, its ip address probably changed from "
                      "%s... attempting recovery",
                      info->macAddress,
                      info->ipAddress);
            char *recoveredIpAddress = NULL;
            if (ssdpRecoverIpAddress(PHILIPSHUE, info->macAddress, &recoveredIpAddress, RECOVERY_TIMEOUT_SECONDS) ==
                true)
            {
                icLogInfo(LOG_TAG, "bridgeMonitoringThreadProc: found %s at %s", info->macAddress, recoveredIpAddress);
                info->ipChangedCallback(info->macAddress, recoveredIpAddress);
                free(info->ipAddress);
                info->ipAddress = recoveredIpAddress;
            }
        }

        usleep(MONITOR_INTERVAL_SECS * 1000000);
    }

    return NULL;
}

bool philipsHueStartMonitoring(const char *macAddress,
                               const char *ipAddress,
                               const char *username,
                               PhilipsHueLightChangedCallback lightChangedCallback,
                               PhilipsHueIpAddressChangedCallback ipAddressChangedCallback)
{
    bool result = true;

    icLogInfo(LOG_TAG, "Monitoring of the bridge %s at %s starting", macAddress, ipAddress);

    pthread_mutex_lock(&monitorsMutex);
    if (monitors == NULL)
    {
        // create hash of 'ipAddresses' to monitor
        //
        monitors = hashMapCreate();
    }
    else
    {
        // skip if this ipAddress already being monitored
        //
        if (hashMapGet(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1)) != NULL)
        {
            icLogError(LOG_TAG, "duplicate attempt to watch bridge at %s ignored", ipAddress);
            result = false;
        }
    }

    if (result == true)
    {
        BridgeMonitoringInfo *info = (BridgeMonitoringInfo *) calloc(1, sizeof(BridgeMonitoringInfo));
        info->macAddress = strdup(macAddress);
        info->ipAddress = strdup(ipAddress);
        info->username = strdup(username);
        info->lightChangedCallback = lightChangedCallback;
        info->ipChangedCallback = ipAddressChangedCallback;
        info->lights = hashMapCreate();
        info->running = true;

        hashMapPut(monitors, info->ipAddress, (uint16_t) (strlen(ipAddress) + 1), info);
        pthread_create(&info->thread, NULL, bridgeMonitoringThreadProc, info);
    }
    pthread_mutex_unlock(&monitorsMutex);

    return result;
}

bool philipsHueStopMonitoring(const char *ipAddress)
{
    bool result = false;

    icLogInfo(LOG_TAG, "Monitoring of the bridge at %s stopping", ipAddress);

    if (ipAddress == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&monitorsMutex);
    if (monitors != NULL)
    {
        BridgeMonitoringInfo *info =
            (BridgeMonitoringInfo *) hashMapGet(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1));
        if (info != NULL)
        {
            info->running = false;
            pthread_join(info->thread, NULL);
            hashMapDelete(
                monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1), standardDoNotFreeHashMapFunc);
            destroyBridgeMonitoringInfo(info);
        }

        if (hashMapCount(monitors) == 0)
        {
            hashMapDestroy(monitors, standardDoNotFreeHashMapFunc);
            monitors = NULL;
        }
    }
    pthread_mutex_unlock(&monitorsMutex);

    return result;
}
