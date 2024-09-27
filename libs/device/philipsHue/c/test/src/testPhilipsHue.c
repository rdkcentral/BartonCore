#include <philipsHue/philipsHue.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

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

    if(lights != NULL)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(lights);
        while(linkedListIteratorHasNext(iterator))
        {
            PhilipsHueLight *light = (PhilipsHueLight*)linkedListIteratorGetNext(iterator);

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

    if(strcmp(lightId, "3") == 0)
    {
        pthread_cond_signal(&monitoringCond);
    }

    pthread_mutex_unlock(&monitoringMtx);
}

static void lightIpAddressChangedCallback(const char *macAddress, char *newIpAddress)
{
    printf("lightIpAddressChanged: %s is now at %s\n", macAddress, newIpAddress);
}

//Start monitoring lights.  Exit when light id "3" changes state
void testMonitoring(const char *macAddress, const char *ipAddress, const char *username)
{
    pthread_mutex_lock(&monitoringMtx);

    philipsHueStartMonitoring(macAddress, ipAddress, username, lightChangedCallback, lightIpAddressChangedCallback);

    pthread_cond_wait(&monitoringCond,&monitoringMtx);

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
