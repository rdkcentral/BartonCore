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
// Native C++ device driver for ONVIF/RTSP IP cameras. It proves out Barton's protocol-agnostic
// camera data model with a second technology: it creates the abstract "camera" endpoint (the same
// session springboard the Matter WebRTC driver exposes) plus a protocol-specific "onvif" endpoint,
// and drives real cameras discovered via ONVIF WS-Discovery.
//
// The driver is a C++ object living behind the C DeviceDriver struct: its instance pointer is stored
// in callbackContext and each C callback is an extern "C" thunk that dispatches to the object.
//
// ============================================================================================
// AUTH MODEL — PRIMITIVE / INTERIM. READ BEFORE EXTENDING.
// --------------------------------------------------------------------------------------------
// Authentication in this driver version is intentionally minimal and is NOT a finished design. A
// camera has a single static username/password pair, written out-of-band by the client into the
// sensitive `username`/`password` resources on ep/onvif, and used for WS-UsernameToken digest auth
// on demand. There are no per-stream tokens, no rotation, no expiry, no separate media accounts,
// no TLS/cert handling, and no configuration-driven credential provisioning. This is a deliberate
// stopgap so the data model can be proven end-to-end without a configuration subsystem that does
// not yet exist. When Barton gains a device-configuration/onboarding mechanism, this auth model
// will very likely need to be reworked. See the change design (D5a) for the full rationale.
// ============================================================================================
//

#include "OnvifSoapClient.h"
#include "OnvifWsDiscovery.h"

#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// deviceDescriptors.h (pulled in transitively below) includes libxml2's parser.h, which on this
// platform drags in ICU C++ template headers. Include it here — as C++, using libxml2's own linkage
// guards — so those templates are processed before the extern "C" block guards them out.
#include <libxml/parser.h>

extern "C" {
#include "device-driver/device-driver-manager.h"
#include "device-driver/device-driver.h"
#include "device/deviceModelHelper.h"
#include "device/icDevice.h"
#include "device/icDeviceResource.h"
#include "device/icInitialResourceValues.h"
#include "deviceService.h"
#include "deviceService/resourceModes.h"
#include "deviceServiceConfiguration.h"
#include "provider/barton-core-property-provider.h"
#include <commonDeviceDefs.h>
#include <deviceServicePrivate.h>
#include <glib.h>
#include <icLog/logging.h>
#include <icTypes/icStringHashMap.h>
#include <resourceTypes.h>
}

#ifdef BARTON_CONFIG_ONVIF

using namespace barton::onvif;

#define LOG_TAG                          "onvifDD"
#define DEVICE_DRIVER_NAME               "onvifCameraDeviceDriver"
#define ONVIF_DEVICE_CLASS_VERSION       1
#define ONVIF_METADATA_SERVICE_URL       "onvifServiceUrl"
#define ONVIF_DISCOVERY_TIMEOUT_MS       3000
// Test seam: when set to "host:port", discovery probes that address by unicast instead of the
// 239.255.255.250 multicast group (which does not reliably traverse container/CI networks).
#define ONVIF_DISCOVERY_ADDRESS_PROPERTY "onvif.discovery.address"

namespace
{

    // Per-camera information captured during WS-Discovery and consulted at configuration time.
    struct DiscoveredCamera
    {
        std::string serviceUrl;
        std::string manufacturer;
        std::string model;
        std::string firmwareVersion;
    };

    class OnvifDriver
    {
    public:
        DeviceDriver *GetDriver() { return &driver; }

        OnvifDriver()
        {
            driver.driverName = strdup(DEVICE_DRIVER_NAME);
            driver.supportedDeviceClasses = linkedListCreate();
            linkedListAppend(driver.supportedDeviceClasses, strdup(CAMERA_DC));
            driver.callbackContext = this;
            // The driver vouches for cameras it discovers via ONVIF WS-Discovery, so they are accepted
            // without a device descriptor (this also lets discovery start before a descriptor list loads).
            driver.neverReject = true;
            driver.customCommFail = true; // no comm-fail monitoring in this version (Non-goal)
        }

        bool StartDiscovery(const char *deviceClass);
        void StopDiscovery();
        bool ConfigureDevice(icDevice *device);
        bool RegisterResources(icDevice *device);
        bool ExecuteResource(icDeviceResource *resource, const char *arg, char **response);
        void DeviceRemoved(icDevice *device);
        void Shutdown();

        DeviceDriver driver {};

    private:
        void DiscoveryWorker();
        bool LookupDiscovered(const std::string &uuid, DiscoveredCamera &out);
        OnvifCredentials ReadCredentials(const std::string &uuid);
        std::string ReadServiceUrl(const std::string &uuid);
        void FetchAndEmitUrl(const std::string &uuid, bool snapshot);

        std::mutex stateMutex;
        std::unordered_map<std::string, DiscoveredCamera> discovered;
        std::atomic<bool> discoveryActive {false};
        std::thread discoveryThread;
    };

    // updateResource() is safe to call from a driver worker thread (the same pattern the Zigbee driver
    // uses from its receive threads). It emits the resource-updated event that carries the URL to clients.
    void EmitResourceUpdate(const std::string &uuid,
                            const std::string &endpointId,
                            const std::string &resourceId,
                            const std::string &value)
    {
        updateResource(uuid.c_str(), endpointId.c_str(), resourceId.c_str(), value.c_str(), nullptr);
    }

    std::string StreamInfoJson(const std::string &uuid, const char *entryResource)
    {
        return std::string("{\"protocol\":\"") + ONVIF_PROTOCOL_NAME + "\",\"entryPoint\":\"/" + uuid + "/ep/" +
               ONVIF_ENDPOINT_ID + "/r/" + entryResource + "\"}";
    }

} // namespace

// ============================================================================================
// Registration and lifecycle. The driver self-registers when this translation unit is loaded. The
// DeviceDriver C-callback thunks it wires up are forward-declared here and defined near the bottom.
// ============================================================================================

static bool discoverDevices(void *ctx, const char *deviceClass);
static void stopDiscoveringDevices(void *ctx, const char *deviceClass);
static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);
static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);
static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);
static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);
static void deviceRemoved(void *ctx, icDevice *device);
static void shutdown(void *ctx);
static void startup(void *ctx);
static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version);

// The DeviceDriver's destroy callback. ctx is the driver's callbackContext, which onvifDriverRegister
// sets to the OnvifDriver instance — so every thunk (this one included) receives the OnvifDriver, not
// a bare DeviceDriver. The object is heap-allocated with `new`, so it must be released with `delete`;
// this callback keeps deviceDriverManager from free()-ing a new-allocated block (an alloc/dealloc
// mismatch that otherwise trips AddressSanitizer at shutdown).
static void destroyDriver(void *ctx)
{
    auto *self = static_cast<OnvifDriver *>(ctx);

    self->Shutdown(); // stop the discovery thread before the object is destroyed
    free(self->GetDriver()->driverName);
    linkedListDestroy(self->GetDriver()->supportedDeviceClasses, free);
    delete self;
}

__attribute__((constructor)) static void onvifDriverRegister(void)
{
    icLogDebug(LOG_TAG, "registering ONVIF camera device driver");

    OnvifDriver *instance = new OnvifDriver();
    DeviceDriver *driver = instance->GetDriver();

    driver->startup = startup;
    driver->shutdown = shutdown;
    driver->destroy = destroyDriver;
    driver->discoverDevices = discoverDevices;
    driver->stopDiscoveringDevices = stopDiscoveringDevices;
    driver->configureDevice = configureDevice;
    driver->registerResources = registerResources;
    driver->executeResource = executeResource;
    driver->writeResource = writeResource;
    driver->deviceRemoved = deviceRemoved;
    driver->getDeviceClassVersion = getDeviceClassVersion;

    deviceDriverManagerRegisterDriver(driver);
}

bool OnvifDriver::LookupDiscovered(const std::string &uuid, DiscoveredCamera &out)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    auto it = discovered.find(uuid);

    if (it == discovered.end())
    {
        return false;
    }

    out = it->second;

    return true;
}

std::string OnvifDriver::ReadServiceUrl(const std::string &uuid)
{
    // Prefer the persisted metadata (survives restarts); fall back to the in-memory discovery cache.
    char *meta = getMetadata(uuid.c_str(), nullptr, ONVIF_METADATA_SERVICE_URL);

    if (meta != nullptr)
    {
        std::string url(meta);
        free(meta);

        return url;
    }

    DiscoveredCamera cam;

    if (LookupDiscovered(uuid, cam))
    {
        return cam.serviceUrl;
    }

    return "";
}

OnvifCredentials OnvifDriver::ReadCredentials(const std::string &uuid)
{
    OnvifCredentials creds;

    icDeviceResource *user = deviceServiceGetResourceById(uuid.c_str(), ONVIF_ENDPOINT_ID, ONVIF_RESOURCE_USERNAME);

    if (user != nullptr)
    {
        if (user->value != nullptr)
        {
            creds.username = user->value;
        }
        resourceDestroy(user);
    }

    icDeviceResource *pass = deviceServiceGetResourceById(uuid.c_str(), ONVIF_ENDPOINT_ID, ONVIF_RESOURCE_PASSWORD);

    if (pass != nullptr)
    {
        if (pass->value != nullptr)
        {
            creds.password = pass->value;
        }
        resourceDestroy(pass);
    }

    return creds;
}

bool OnvifDriver::StartDiscovery(const char *deviceClass)
{
    if (deviceClass == nullptr || strcmp(deviceClass, CAMERA_DC) != 0)
    {
        return false;
    }

    bool expected = false;

    if (!discoveryActive.compare_exchange_strong(expected, true))
    {
        // Already discovering.
        return true;
    }

    if (discoveryThread.joinable())
    {
        discoveryThread.join();
    }
    discoveryThread = std::thread(&OnvifDriver::DiscoveryWorker, this);

    return true;
}

void OnvifDriver::StopDiscovery()
{
    discoveryActive.store(false);

    if (discoveryThread.joinable())
    {
        discoveryThread.join();
    }
}

void OnvifDriver::DiscoveryWorker()
{
    OnvifWsDiscovery discovery;

    // Test seam: allow a unicast discovery target via a property (see ONVIF_DISCOVERY_ADDRESS_PROPERTY).
    BCorePropertyProvider *provider = deviceServiceConfigurationGetPropertyProvider();

    if (provider != nullptr)
    {
        gchar *addr =
            b_core_property_provider_get_property_as_string(provider, ONVIF_DISCOVERY_ADDRESS_PROPERTY, nullptr);

        if (addr != nullptr)
        {
            std::string value(addr);
            g_free(addr);
            size_t colon = value.find(':');

            if (colon != std::string::npos)
            {
                std::string host = value.substr(0, colon);
                int port = atoi(value.substr(colon + 1).c_str());

                if (!host.empty() && port > 0)
                {
                    icLogInfo(LOG_TAG, "using unicast discovery target %s:%d", host.c_str(), port);
                    discovery.SetDestination(host, port);
                }
            }
        }

        g_object_unref(provider);
    }

    std::string error;
    std::vector<OnvifProbeMatch> matches = discovery.Probe(ONVIF_DISCOVERY_TIMEOUT_MS, &error);

    if (!error.empty())
    {
        icLogWarn(LOG_TAG, "WS-Discovery probe error: %s", error.c_str());
    }

    for (const OnvifProbeMatch &match : matches)
    {
        if (!discoveryActive.load())
        {
            break;
        }

        std::string uuid = OnvifDeviceUuidFromEndpointReference(match.endpointReference);

        if (uuid.empty() || match.xaddrs.empty())
        {
            continue;
        }

        // Discovery must be idempotent: WS-Discovery ProbeMatches are received on every discovery
        // run (and cameras may answer a single probe more than once). If the device is already in
        // the database, re-reporting it via deviceServiceDeviceFound would fail to re-create the
        // existing device entry ("Failed to create device entry" / "device discovery failed").
        // Skip devices we already know about so repeat discoveries are harmless no-ops.
        if (deviceServiceIsDeviceKnown(uuid.c_str()))
        {
            icLogDebug(LOG_TAG, "ONVIF camera %s already known; skipping re-report", uuid.c_str());
            continue;
        }

        DiscoveredCamera cam;
        cam.serviceUrl = match.xaddrs.front();

        // Anonymous device information (no credentials required for most cameras).
        OnvifSoapClient client(cam.serviceUrl);
        OnvifDeviceInfo info;

        if (client.GetDeviceInformation(OnvifCredentials {}, info, nullptr))
        {
            cam.manufacturer = info.manufacturer;
            cam.model = info.model;
            cam.firmwareVersion = info.firmwareVersion;
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            discovered[uuid] = cam;
        }

        DeviceFoundDetails details {};
        details.deviceDriver = &driver;
        details.deviceClass = CAMERA_DC;
        details.deviceClassVersion = ONVIF_DEVICE_CLASS_VERSION;
        details.deviceUuid = uuid.c_str();
        details.manufacturer = cam.manufacturer.empty() ? "ONVIF" : cam.manufacturer.c_str();
        details.model = cam.model.empty() ? "Camera" : cam.model.c_str();
        details.hardwareVersion = "1";
        details.firmwareVersion = cam.firmwareVersion.empty() ? "1" : cam.firmwareVersion.c_str();

        details.endpointProfileMap = stringHashMapCreate();
        stringHashMapPut(
            details.endpointProfileMap, strdup(CAMERA_SESSION_ENDPOINT_ID), strdup(CAMERA_SESSION_PROFILE));
        stringHashMapPut(details.endpointProfileMap, strdup(ONVIF_ENDPOINT_ID), strdup(ONVIF_PROFILE));

        icLogInfo(LOG_TAG, "ONVIF camera found: uuid=%s service=%s", uuid.c_str(), cam.serviceUrl.c_str());

        bool accepted = deviceServiceDeviceFound(&details, driver.neverReject);

        stringHashMapDestroy(details.endpointProfileMap, NULL);

        if (!accepted)
        {
            // The device service rejected the camera; drop the entry we speculatively cached so a
            // later probe starts clean instead of reusing stale discovery state.
            icLogWarn(LOG_TAG, "deviceServiceDeviceFound rejected uuid=%s; dropping cached discovery", uuid.c_str());

            std::lock_guard<std::mutex> lock(stateMutex);
            discovered.erase(uuid);
        }
    }

    discoveryActive.store(false);
}

bool OnvifDriver::ConfigureDevice(icDevice *device)
{
    if (device == nullptr || device->uuid == nullptr)
    {
        return false;
    }

    DiscoveredCamera cam;

    if (LookupDiscovered(device->uuid, cam))
    {
        // Fresh discovery: persist the service URL so on-demand SOAP calls survive restarts.
        createDeviceMetadata(device, ONVIF_METADATA_SERVICE_URL, cam.serviceUrl.c_str());
    }
    else
    {
        // Reconfiguration (e.g. after a service restart): the discovery cache is empty but the
        // service URL was persisted at first configuration, so recreate the endpoints idempotently.
        icLogInfo(
            LOG_TAG, "configureDevice: %s not in discovery cache; reconfiguring from persisted state", device->uuid);
    }

    createEndpoint(device, CAMERA_SESSION_ENDPOINT_ID, CAMERA_SESSION_PROFILE, true);
    createEndpoint(device, ONVIF_ENDPOINT_ID, ONVIF_PROFILE, true);

    return true;
}

static icDeviceEndpoint *findEndpointById(icDevice *device, const char *endpointId)
{
    icLinkedListIterator *it = linkedListIteratorCreate(device->endpoints);
    icDeviceEndpoint *found = nullptr;

    while (linkedListIteratorHasNext(it))
    {
        icDeviceEndpoint *ep = (icDeviceEndpoint *) linkedListIteratorGetNext(it);

        if (ep != nullptr && ep->id != nullptr && strcmp(ep->id, endpointId) == 0)
        {
            found = ep;
            break;
        }
    }

    linkedListIteratorDestroy(it);

    return found;
}

bool OnvifDriver::RegisterResources(icDevice *device)
{
    icDeviceEndpoint *cameraEp = findEndpointById(device, CAMERA_SESSION_ENDPOINT_ID);
    icDeviceEndpoint *onvifEp = findEndpointById(device, ONVIF_ENDPOINT_ID);

    if (cameraEp == nullptr || onvifEp == nullptr)
    {
        icLogError(LOG_TAG, "registerResources: endpoints missing for %s", device->uuid);

        return false;
    }

    // Abstract camera session lifecycle (springboard executes).
    createEndpointResource(cameraEp,
                           CAMERA_SESSION_FUNCTION_CREATE_SESSION,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EXECUTABLE,
                           CACHING_POLICY_NEVER);
    createEndpointResource(cameraEp,
                           CAMERA_SESSION_FUNCTION_STREAM,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EXECUTABLE,
                           CACHING_POLICY_NEVER);
    createEndpointResource(cameraEp,
                           CAMERA_SESSION_FUNCTION_TAKE_PICTURE,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EXECUTABLE,
                           CACHING_POLICY_NEVER);
    createEndpointResource(cameraEp,
                           CAMERA_SESSION_FUNCTION_DESTROY_SESSION,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EXECUTABLE,
                           CACHING_POLICY_NEVER);

    // ONVIF protocol endpoint: on-demand URL retrieval + event delivery + credentials.
    createEndpointResource(onvifEp,
                           ONVIF_FUNCTION_GET_MEDIA_URL,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EXECUTABLE,
                           CACHING_POLICY_NEVER);
    createEndpointResource(
        onvifEp, ONVIF_RESOURCE_MEDIA_URL, NULL, RESOURCE_TYPE_STRING, RESOURCE_MODE_EMIT_EVENTS, CACHING_POLICY_NEVER);
    createEndpointResource(onvifEp,
                           ONVIF_FUNCTION_GET_SNAPSHOT_URL,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EXECUTABLE,
                           CACHING_POLICY_NEVER);
    createEndpointResource(onvifEp,
                           ONVIF_RESOURCE_SNAPSHOT_URL,
                           NULL,
                           RESOURCE_TYPE_STRING,
                           RESOURCE_MODE_EMIT_EVENTS,
                           CACHING_POLICY_NEVER);
    // Non-secret signal telling the client that credentials must be applied to the media/snapshot URLs.
    createEndpointResource(onvifEp,
                           ONVIF_RESOURCE_AUTH_REQUIRED,
                           "true",
                           RESOURCE_TYPE_BOOLEAN,
                           RESOURCE_MODE_READABLE,
                           CACHING_POLICY_ALWAYS);
    // Credentials: write-only sensitive resources (encrypted at rest, redacted in logs).
    createEndpointResource(onvifEp,
                           ONVIF_RESOURCE_USERNAME,
                           NULL,
                           RESOURCE_TYPE_USER_ID,
                           RESOURCE_MODE_WRITEABLE | RESOURCE_MODE_SENSITIVE,
                           CACHING_POLICY_ALWAYS);
    createEndpointResource(onvifEp,
                           ONVIF_RESOURCE_PASSWORD,
                           NULL,
                           RESOURCE_TYPE_PASSWORD,
                           RESOURCE_MODE_WRITEABLE | RESOURCE_MODE_SENSITIVE,
                           CACHING_POLICY_ALWAYS);

    return true;
}

void OnvifDriver::FetchAndEmitUrl(const std::string &uuid, bool snapshot)
{
    std::string serviceUrl = ReadServiceUrl(uuid);
    OnvifCredentials creds = ReadCredentials(uuid);

    if (serviceUrl.empty())
    {
        icLogError(LOG_TAG, "no ONVIF service URL for %s", uuid.c_str());

        return;
    }

    // The SOAP round trip is blocking network I/O, so run it on a detached worker thread and emit the
    // resulting resource-update event directly (the emit path is thread-safe, matching the Zigbee
    // driver's asynchronous resource-update pattern).
    std::thread([uuid, serviceUrl, creds, snapshot]() {
        OnvifSoapClient client(serviceUrl);
        std::string error;
        std::vector<std::string> profiles;

        if (!client.GetProfiles(creds, profiles, &error) || profiles.empty())
        {
            icLogError(LOG_TAG, "GetProfiles failed for %s: %s", uuid.c_str(), error.c_str());

            return;
        }

        std::string url;
        bool ok = snapshot ? client.GetSnapshotUri(creds, profiles.front(), url, &error)
                           : client.GetStreamUri(creds, profiles.front(), url, &error);

        if (!ok)
        {
            icLogError(LOG_TAG,
                       "%s failed for %s: %s",
                       snapshot ? "GetSnapshotUri" : "GetStreamUri",
                       uuid.c_str(),
                       error.c_str());

            return;
        }

        EmitResourceUpdate(
            uuid, ONVIF_ENDPOINT_ID, snapshot ? ONVIF_RESOURCE_SNAPSHOT_URL : ONVIF_RESOURCE_MEDIA_URL, url);
    }).detach();
}

bool OnvifDriver::ExecuteResource(icDeviceResource *resource, const char *arg, char **response)
{
    (void) arg; // sessionId is accepted but ignored: ONVIF is stateless (see design D3).

    if (resource == nullptr || resource->id == nullptr || resource->endpointId == nullptr ||
        resource->deviceUuid == nullptr)
    {
        return false;
    }

    std::string uuid = resource->deviceUuid;
    const char *id = resource->id;

    if (strcmp(resource->endpointId, CAMERA_SESSION_ENDPOINT_ID) == 0)
    {
        if (strcmp(id, CAMERA_SESSION_FUNCTION_CREATE_SESSION) == 0)
        {
            // Stateless: return a fixed correlation id so the client contract is satisfied.
            if (response != nullptr)
            {
                *response = strdup("1");
            }

            return true;
        }

        if (strcmp(id, CAMERA_SESSION_FUNCTION_STREAM) == 0)
        {
            if (response != nullptr)
            {
                *response = strdup(StreamInfoJson(uuid, ONVIF_FUNCTION_GET_MEDIA_URL).c_str());
            }

            return true;
        }

        if (strcmp(id, CAMERA_SESSION_FUNCTION_TAKE_PICTURE) == 0)
        {
            if (response != nullptr)
            {
                *response = strdup(StreamInfoJson(uuid, ONVIF_FUNCTION_GET_SNAPSHOT_URL).c_str());
            }

            return true;
        }

        if (strcmp(id, CAMERA_SESSION_FUNCTION_DESTROY_SESSION) == 0)
        {
            // Nothing to tear down for a stateless protocol.
            return true;
        }
    }
    else if (strcmp(resource->endpointId, ONVIF_ENDPOINT_ID) == 0)
    {
        if (strcmp(id, ONVIF_FUNCTION_GET_MEDIA_URL) == 0)
        {
            FetchAndEmitUrl(uuid, false);

            return true;
        }

        if (strcmp(id, ONVIF_FUNCTION_GET_SNAPSHOT_URL) == 0)
        {
            FetchAndEmitUrl(uuid, true);

            return true;
        }
    }

    icLogWarn(LOG_TAG, "unhandled execute of %s on %s", id, resource->endpointId);

    return false;
}

void OnvifDriver::DeviceRemoved(icDevice *device)
{
    if (device == nullptr || device->uuid == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    discovered.erase(device->uuid);
}

void OnvifDriver::Shutdown()
{
    StopDiscovery();
}

// ============================================================================================
// extern "C" thunks — dispatch each DeviceDriver callback to the OnvifDriver instance in ctx.
// ============================================================================================

static bool discoverDevices(void *ctx, const char *deviceClass)
{
    return static_cast<OnvifDriver *>(ctx)->StartDiscovery(deviceClass);
}

static void stopDiscoveringDevices(void *ctx, const char *)
{
    static_cast<OnvifDriver *>(ctx)->StopDiscovery();
}

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *)
{
    return static_cast<OnvifDriver *>(ctx)->ConfigureDevice(device);
}

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *)
{
    return static_cast<OnvifDriver *>(ctx)->RegisterResources(device);
}

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response)
{
    return static_cast<OnvifDriver *>(ctx)->ExecuteResource(resource, arg, response);
}

static bool writeResource(void *ctx, icDeviceResource *resource, const char *, const char *newValue)
{
    (void) ctx;

    // Accept writes only to our credential resources (username/password). Per the device service
    // write contract, the driver must call updateResource() to actually persist the new value;
    // the service does not store it on our behalf. Without this, ReadCredentials() would
    // later read back an empty value and the WS-UsernameToken digest would be computed over
    // an empty password, causing the camera to reject every request with HTTP 401.
    if (resource != nullptr && resource->endpointId != nullptr && resource->id != nullptr &&
        resource->deviceUuid != nullptr && strcmp(resource->endpointId, ONVIF_ENDPOINT_ID) == 0 &&
        (strcmp(resource->id, ONVIF_RESOURCE_USERNAME) == 0 || strcmp(resource->id, ONVIF_RESOURCE_PASSWORD) == 0))
    {
        updateResource(resource->deviceUuid, resource->endpointId, resource->id, newValue, nullptr);
        return true;
    }

    return false;
}

static void deviceRemoved(void *ctx, icDevice *device)
{
    static_cast<OnvifDriver *>(ctx)->DeviceRemoved(device);
}

static void shutdown(void *ctx)
{
    static_cast<OnvifDriver *>(ctx)->Shutdown();
}

static void startup(void *) {}

static bool getDeviceClassVersion(void *, const char *, uint8_t *version)
{
    if (version != nullptr)
    {
        *version = ONVIF_DEVICE_CLASS_VERSION;
    }

    return true;
}

#endif // BARTON_CONFIG_ONVIF
