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

#include "deviceDrivers/OnvifDeviceDriver.h"
#include "OnvifSoapClient.h"
#include "OnvifWsDiscovery.h"

#include <atomic>
#include <chrono>
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
// Back off between retries when a discovery probe fails fast (bad socket/destination/args) so the
// worker loop cannot hot-spin; a normal probe consumes ONVIF_DISCOVERY_TIMEOUT_MS and never backs off.
#define ONVIF_DISCOVERY_RETRY_BACKOFF_MS 5000
#define ONVIF_DISCOVERY_RETRY_SLICE_MS   250
// Test seam: when set to "host:port", discovery probes that address by unicast instead of the
// 239.255.255.250 multicast group (which does not reliably traverse container/CI networks).
#define ONVIF_DISCOVERY_ADDRESS_PROPERTY "onvif.discovery.address"
// PRIMITIVE / INTERIM discovery-time ONVIF credentials -- NOT A FINAL DESIGN. See the "AUTH MODEL"
// header above: this extends that stopgap to the discovery/identification phase and is expected to be
// reworked when Barton gains a device-configuration/onboarding mechanism.
//
// Why this exists: some cameras (observed: Reolink E1 Pro) require WS-Security on GetDeviceInformation,
// so an anonymous identification returns 401 and the camera cannot be identified (manufacturer/model
// empty) -- which breaks vendor claim/ownership. When set, these credentials are used to retry
// identification so such cameras are correctly identified at discovery.
//
// KNOWN LIMITATION (intentional, interim): this introduces a SECOND credential surface that duplicates
// the per-device ep/onvif username/password resources used for runtime (polling, media/snapshot URL
// retrieval). Discovery runs before the device -- and therefore its ep/onvif resources -- exists, so
// there is no per-device resource to read yet; hence a global property seam. These are single, static,
// global credentials (one pair for all cameras), write-once via properties, with no per-device scoping,
// rotation, expiry, or secure provisioning. A finished design should provision a single credential
// source consumed by both discovery and runtime (e.g. persist the discovery credentials onto the device
// at add time, or source both from the onboarding subsystem) and remove this global seam.
#define ONVIF_DISCOVERY_USERNAME_PROPERTY "onvif.discovery.username"
#define ONVIF_DISCOVERY_PASSWORD_PROPERTY "onvif.discovery.password"

namespace
{

    // A discovered uuid becomes part of a JSON springboard payload and a resource URI path. The
    // endpoint reference is network-controlled and the parser accepts arbitrary values, so reject any
    // identifier that could inject quotes, backslashes, path separators, or control characters.
    // Read a CPE/property-provider string property, returning "" when unset. Used for the interim
    // discovery-time ONVIF credential seam (see ONVIF_DISCOVERY_USERNAME/PASSWORD_PROPERTY).
    std::string ReadDiscoveryProperty(const char *propertyName)
    {
        std::string value;
        BCorePropertyProvider *provider = deviceServiceConfigurationGetPropertyProvider();

        if (provider != nullptr)
        {
            gchar *raw = b_core_property_provider_get_property_as_string(provider, propertyName, nullptr);

            if (raw != nullptr)
            {
                value = raw;
                g_free(raw);
            }

            g_object_unref(provider);
        }

        return value;
    }

    bool IsSafeDeviceUuid(const std::string &uuid)
    {
        if (uuid.empty())
        {
            return false;
        }

        // Reject dot-segments: the uuid becomes a single URI path segment, so "."/".." could produce a
        // traversal ("/../ep/...") after path normalization.
        if (uuid == "." || uuid == "..")
        {
            return false;
        }

        for (char c : uuid)
        {
            unsigned char uc = static_cast<unsigned char>(c);
            bool safe = (uc >= '0' && uc <= '9') || (uc >= 'a' && uc <= 'z') || uc == '-' || uc == '.' || uc == '_';

            if (!safe)
            {
                return false;
            }
        }

        return true;
    }

    // Return the offset of the first character that terminates a URL authority (path, query, or
    // fragment) at or after authStart, per RFC 3986. A query or fragment can precede any '/', so the
    // authority must not be scanned for '/' alone or userinfo could survive credential redaction.
    size_t UrlAuthorityEnd(const std::string &url, size_t authStart)
    {
        size_t end = std::string::npos;

        for (char delim : {'/', '?', '#'})
        {
            size_t pos = url.find(delim, authStart);

            if (pos != std::string::npos && (end == std::string::npos || pos < end))
            {
                end = pos;
            }
        }

        return end;
    }

    // Remove any "user:pass@" userinfo from an http(s) URL authority so a forged XAddr cannot smuggle
    // credentials into the cached service URL (or its logs).
    std::string StripUrlUserinfo(const std::string &url)
    {
        size_t schemeEnd = url.find("://");

        if (schemeEnd == std::string::npos)
        {
            return url;
        }

        size_t authStart = schemeEnd + 3;
        size_t authEnd = UrlAuthorityEnd(url, authStart);
        std::string authority =
            url.substr(authStart, authEnd == std::string::npos ? std::string::npos : authEnd - authStart);
        size_t at = authority.rfind('@');

        if (at == std::string::npos)
        {
            return url;
        }

        return url.substr(0, authStart) + authority.substr(at + 1) +
               (authEnd == std::string::npos ? std::string() : url.substr(authEnd));
    }

    // Return the host of an http(s)/rtsp URL (without userinfo or port; brackets kept for IPv6).
    std::string UrlHost(const std::string &url)
    {
        size_t schemeEnd = url.find("://");

        if (schemeEnd == std::string::npos)
        {
            return "";
        }

        size_t authStart = schemeEnd + 3;
        size_t authEnd = UrlAuthorityEnd(url, authStart);
        std::string authority =
            url.substr(authStart, authEnd == std::string::npos ? std::string::npos : authEnd - authStart);
        size_t at = authority.rfind('@');

        if (at != std::string::npos)
        {
            authority = authority.substr(at + 1);
        }

        if (!authority.empty() && authority.front() == '[')
        {
            size_t close = authority.find(']');

            return close == std::string::npos ? authority : authority.substr(0, close + 1);
        }

        size_t colon = authority.rfind(':');

        return colon == std::string::npos ? authority : authority.substr(0, colon);
    }

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
// Base construction and the vendor-specialization claim registry.
// ============================================================================================

OnvifDriver::OnvifDriver()
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

namespace
{
    // Guards the specialization registry. A function-local static avoids static-init-order issues
    // between translation units (a specialization registers from its own TU's init).
    std::mutex &SpecializationMutex()
    {
        static std::mutex m;

        return m;
    }

    std::vector<OnvifDriver *> &Specializations()
    {
        static std::vector<OnvifDriver *> list;

        return list;
    }
} // namespace

void OnvifDriver::RegisterSpecialization(OnvifDriver *self)
{
    if (self == nullptr)
    {
        return;
    }

    self->isSpecialization = true;

    std::lock_guard<std::mutex> lock(SpecializationMutex());
    Specializations().push_back(self);
}

bool OnvifDriver::AnySpecializationClaims(const std::string &manufacturer, const std::string &model)
{
    std::lock_guard<std::mutex> lock(SpecializationMutex());

    for (OnvifDriver *spec : Specializations())
    {
        if (spec->ClaimsCamera(manufacturer, model))
        {
            return true;
        }
    }

    return false;
}

bool OnvifDriver::ShouldReportCamera(const std::string &manufacturer, const std::string &model) const
{
    // A specialization reports only cameras it claims; the generic driver reports only cameras no
    // registered specialization claims. This makes ownership deterministic instead of a discovery race.
    if (isSpecialization)
    {
        return ClaimsCamera(manufacturer, model);
    }

    return !AnySpecializationClaims(manufacturer, model);
}

// ============================================================================================
// Registration and lifecycle. The driver self-registers when this translation unit is loaded. The
// DeviceDriver C-callback thunks it wires up are forward-declared here and defined near the bottom.
// ============================================================================================

static bool discoverDevices(void *ctx, const char *deviceClass);
static void stopDiscoveringDevices(void *ctx, const char *deviceClass);
static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);
static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);
static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);
static void synchronizeDevice(void *ctx, icDevice *device);
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

// Wire the DeviceDriver C callbacks to the dispatch thunks. Shared by the ONVIF registration and by
// vendor specializations so they reuse the same (virtual-dispatching) thunks.
namespace barton
{
    namespace onvif
    {
        void OnvifWireDriverCallbacks(DeviceDriver *driver)
        {
            driver->startup = startup;
            driver->shutdown = shutdown;
            driver->destroy = destroyDriver;
            driver->discoverDevices = discoverDevices;
            driver->stopDiscoveringDevices = stopDiscoveringDevices;
            driver->configureDevice = configureDevice;
            driver->registerResources = registerResources;
            driver->fetchInitialResourceValues = fetchInitialResourceValues;
            driver->synchronizeDevice = synchronizeDevice;
            driver->executeResource = executeResource;
            driver->writeResource = writeResource;
            driver->deviceRemoved = deviceRemoved;
            driver->getDeviceClassVersion = getDeviceClassVersion;
        }
    } // namespace onvif
} // namespace barton

// Registration entry point. Called from deviceDriverManagerInitialize under BARTON_CONFIG_ONVIF (an
// explicit reference, unlike a self-registering constructor) so the driver object is pulled from the
// static archive and its registration actually runs for BartonCoreStatic consumers.
extern "C" void onvifDeviceDriverInitialize(void)
{
    // Idempotent: deviceDriverManagerInitialize() may run more than once, and the manager retains every
    // registered pointer, so register exactly one OnvifDriver for the process lifetime.
    static std::once_flag registerFlag;
    std::call_once(registerFlag, [] {
        icLogDebug(LOG_TAG, "registering ONVIF camera device driver");

        OnvifDriver *instance = new OnvifDriver();
        DeviceDriver *driver = instance->GetDriver();

        OnvifWireDriverCallbacks(driver);

        deviceDriverManagerRegisterDriver(driver);
    });
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
    // Prefer the driver-owned discovered URL (immutable within this session) over the persisted
    // metadata, which is client-writable and could be tampered with to redirect authenticated calls.
    DiscoveredCamera cam;

    if (LookupDiscovered(uuid, cam))
    {
        return cam.serviceUrl;
    }

    char *meta = getMetadata(uuid.c_str(), nullptr, ONVIF_METADATA_SERVICE_URL);

    if (meta != nullptr)
    {
        std::string url = StripUrlUserinfo(meta);
        free(meta);

        // Device metadata is client-writable, so re-validate the scheme on every read (not just at
        // discovery) before using it as an authenticated SOAP target.
        if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0)
        {
            return url;
        }

        icLogWarn(LOG_TAG, "ignoring non-HTTP persisted ONVIF service URL for %s", uuid.c_str());
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

bool OnvifDriver::ReadAuthRequired(const std::string &uuid)
{
    // Prefer the persisted authRequired resource (written at configuration from the discovery-derived
    // value) so the decision survives a restart, when the in-memory discovery cache is empty. Default
    // to requiring credentials if neither source resolves.
    icDeviceResource *res = deviceServiceGetResourceById(uuid.c_str(), ONVIF_ENDPOINT_ID, ONVIF_RESOURCE_AUTH_REQUIRED);

    if (res != nullptr)
    {
        bool required = (res->value == nullptr) || strcmp(res->value, "false") != 0;
        resourceDestroy(res);

        return required;
    }

    DiscoveredCamera cam;

    return LookupDiscovered(uuid, cam) ? cam.authRequired : true;
}

bool OnvifDriver::StartDiscovery(const char *deviceClass)
{
    if (deviceClass == nullptr || strcmp(deviceClass, CAMERA_DC) != 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(discoveryThreadMutex);

    // Record the intent to discover. If a worker already holds the latch (including one draining a
    // pending stop), it observes this and runs another session before exiting, so a start that races a
    // stop is not lost. The worker releases discoveryRunning under this same mutex, so the CAS below and
    // that release cannot interleave into a lost start.
    discoverDesired.store(true);

    bool expected = false;

    if (!discoveryRunning.compare_exchange_strong(expected, true))
    {
        // A worker already holds the latch; it will (re)discover because discoverDesired is now true.
        return true;
    }

    // The previous worker released discoveryRunning under this mutex before exiting, so this join reaps
    // a finished thread rather than blocking, and reaping before reassigning avoids move-assigning over
    // a joinable handle (which would call std::terminate).
    if (discoveryThread.joinable())
    {
        discoveryThread.join();
    }

    discoveryThread = std::thread(&OnvifDriver::DiscoveryWorker, this);

    return true;
}

void OnvifDriver::StopDiscovery()
{
    // Contract: this returns immediately. Clear the desire to discover; the worker observes it, aborts
    // any in-flight session's reporting, and exits (releasing the latch under discoveryThreadMutex). The
    // thread is reaped by the next StartDiscovery or by Shutdown rather than joined here, which would
    // otherwise block for the full in-flight probe/SOAP timeout.
    discoverDesired.store(false);
}

void OnvifDriver::DiscoveryWorker()
{
    // Run discovery sessions until discovery is no longer desired. Looping here (rather than exiting
    // after a single session) lets a start that arrived while a stop was draining be honored: the worker
    // keeps the latch and runs again instead of releasing it and losing the start.
    while (true)
    {
        bool probeOk = false;

        try
        {
            probeOk = RunDiscoveryProbe();
        }
        catch (const std::exception &e)
        {
            icLogError(LOG_TAG, "discovery worker error: %s", e.what());
        }
        catch (...)
        {
            icLogError(LOG_TAG, "discovery worker error");
        }

        // Decide to run again or exit atomically with releasing the latch, under the same mutex
        // StartDiscovery uses: a start that set discoverDesired just now either keeps this worker
        // looping (CAS would have failed) or wins the CAS after we release here -- never both, never
        // neither.
        {
            std::lock_guard<std::mutex> lock(discoveryThreadMutex);

            if (!discoverDesired.load())
            {
                discoveryRunning.store(false);

                return;
            }
        }

        // A probe that failed fast returns immediately; without a delay the loop would hot-spin (a
        // pegged core, a log line per iteration, and repeated socket() calls under fd exhaustion) for
        // the whole discovery window. Back off before retrying -- a normal probe consumes its full
        // timeout and never reaches here -- sleeping in slices so a stop during the backoff is prompt.
        if (!probeOk)
        {
            for (int waited = 0; waited < ONVIF_DISCOVERY_RETRY_BACKOFF_MS && discoverDesired.load();
                 waited += ONVIF_DISCOVERY_RETRY_SLICE_MS)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(ONVIF_DISCOVERY_RETRY_SLICE_MS));
            }
        }
    }
}

bool OnvifDriver::RunDiscoveryProbe()
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

    // A non-empty error means the probe failed fast (bad socket/destination/args) rather than ran its
    // full budget; return it so the worker loop backs off instead of hot-spinning on a fast failure.
    bool probeSucceeded = error.empty();

    if (!error.empty())
    {
        icLogWarn(LOG_TAG, "WS-Discovery probe error: %s", error.c_str());
    }

    for (const OnvifProbeMatch &match : matches)
    {
        if (!discoverDesired.load())
        {
            break;
        }

        std::string uuid = OnvifDeviceUuidFromEndpointReference(match.endpointReference);

        if (uuid.empty() || match.xaddrs.empty())
        {
            continue;
        }

        // Reject a network-controlled endpoint reference that would not yield a safe identifier before
        // it is concatenated into the springboard JSON / resource URIs.
        if (!IsSafeDeviceUuid(uuid))
        {
            icLogWarn(LOG_TAG, "skipping ONVIF camera with unsafe endpoint reference");
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
        cam.serviceUrl = StripUrlUserinfo(match.xaddrs.front());

        // The XAddr comes from an unauthenticated ProbeMatch; accept only http(s) URLs before caching
        // it as the libcurl POST target so a forged response cannot redirect SOAP calls elsewhere.
        if (cam.serviceUrl.rfind("http://", 0) != 0 && cam.serviceUrl.rfind("https://", 0) != 0)
        {
            icLogWarn(LOG_TAG, "skipping ONVIF camera with non-HTTP service URL");
            continue;
        }

        // Anonymous device information. Whether the camera answers an unauthenticated GetDeviceInformation
        // also tells us if it requires credentials at all; a camera that responds anonymously does not, so
        // the operator is not forced to invent throwaway credentials just to stream an open camera.
        OnvifSoapClient client(cam.serviceUrl);
        OnvifDeviceInfo info;

        if (client.GetDeviceInformation(OnvifCredentials {}, info, nullptr))
        {
            cam.manufacturer = info.manufacturer;
            cam.model = info.model;
            cam.firmwareVersion = info.firmwareVersion;
            cam.authRequired = false; // answered without credentials
        }
        else
        {
            // The camera rejected an anonymous GetDeviceInformation (e.g. Reolink E1 Pro requires
            // WS-Security). Retry identification with the interim discovery credentials so the camera is
            // still identified (manufacturer/model) and can be claimed by its vendor specialization.
            std::string discoveryUser = ReadDiscoveryProperty(ONVIF_DISCOVERY_USERNAME_PROPERTY);
            std::string discoveryPass = ReadDiscoveryProperty(ONVIF_DISCOVERY_PASSWORD_PROPERTY);

            if (!discoveryUser.empty() && !discoveryPass.empty())
            {
                OnvifCredentials discoveryCreds {discoveryUser, discoveryPass};
                OnvifDeviceInfo authedInfo;

                if (client.GetDeviceInformation(discoveryCreds, authedInfo, nullptr))
                {
                    cam.manufacturer = authedInfo.manufacturer;
                    cam.model = authedInfo.model;
                    cam.firmwareVersion = authedInfo.firmwareVersion;
                    cam.authRequired = true; // required credentials to identify
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            discovered[uuid] = cam;
        }

        // Ownership gate: a specialization reports only cameras it claims; the generic driver yields a
        // camera that a registered specialization claims. Decided from the (best-effort) manufacturer/
        // model obtained above. See the interim claim model in OnvifDeviceDriver.h.
        if (!ShouldReportCamera(cam.manufacturer, cam.model))
        {
            icLogDebug(LOG_TAG, "yielding ONVIF camera %s to a vendor specialization", uuid.c_str());

            std::lock_guard<std::mutex> lock(stateMutex);
            discovered.erase(uuid);

            continue;
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

    return probeSucceeded;
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

bool OnvifDriver::FetchInitialResourceValues(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    if (device == nullptr || device->uuid == nullptr)
    {
        return false;
    }

    // Seed the resources the driver (not the platform) supplies values for, so a reconfiguration -- which
    // recreates the device's resources -- preserves them instead of resetting them. The credentials are
    // client-written and would otherwise be lost; authRequired is discovery-derived and the discovery
    // cache is empty during reconfiguration, so ReadAuthRequired prefers the persisted value. On first
    // configuration ReadCredentials returns empty (seeded NULL) and ReadAuthRequired uses the freshly
    // discovered value.
    OnvifCredentials creds = ReadCredentials(device->uuid);

    initialResourceValuesPutEndpointValue(initialResourceValues,
                                          ONVIF_ENDPOINT_ID,
                                          ONVIF_RESOURCE_USERNAME,
                                          creds.username.empty() ? nullptr : creds.username.c_str());
    initialResourceValuesPutEndpointValue(initialResourceValues,
                                          ONVIF_ENDPOINT_ID,
                                          ONVIF_RESOURCE_PASSWORD,
                                          creds.password.empty() ? nullptr : creds.password.c_str());
    initialResourceValuesPutEndpointValue(initialResourceValues,
                                          ONVIF_ENDPOINT_ID,
                                          ONVIF_RESOURCE_AUTH_REQUIRED,
                                          ReadAuthRequired(device->uuid) ? "true" : "false");

    return true;
}

bool OnvifDriver::RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icDeviceEndpoint *cameraEp = findEndpointById(device, CAMERA_SESSION_ENDPOINT_ID);
    icDeviceEndpoint *onvifEp = findEndpointById(device, ONVIF_ENDPOINT_ID);

    if (cameraEp == nullptr || onvifEp == nullptr)
    {
        icLogError(LOG_TAG, "registerResources: endpoints missing for %s", device->uuid);

        return false;
    }

    // Abstract camera session lifecycle (springboard executes). Track every creation so a partial
    // failure fails registration rather than persisting a half-configured camera.
    bool allOk = true;
    allOk = (createEndpointResource(cameraEp,
                                    CAMERA_SESSION_FUNCTION_CREATE_SESSION,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EXECUTABLE,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    allOk = (createEndpointResource(cameraEp,
                                    CAMERA_SESSION_FUNCTION_STREAM,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EXECUTABLE,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    allOk = (createEndpointResource(cameraEp,
                                    CAMERA_SESSION_FUNCTION_TAKE_PICTURE,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EXECUTABLE,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    allOk = (createEndpointResource(cameraEp,
                                    CAMERA_SESSION_FUNCTION_DESTROY_SESSION,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EXECUTABLE,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;

    // ONVIF protocol endpoint: on-demand URL retrieval + event delivery + credentials.
    allOk = (createEndpointResource(onvifEp,
                                    ONVIF_FUNCTION_GET_MEDIA_URL,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EXECUTABLE,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    allOk = (createEndpointResource(onvifEp,
                                    ONVIF_RESOURCE_MEDIA_URL,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    allOk = (createEndpointResource(onvifEp,
                                    ONVIF_FUNCTION_GET_SNAPSHOT_URL,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EXECUTABLE,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    allOk = (createEndpointResource(onvifEp,
                                    ONVIF_RESOURCE_SNAPSHOT_URL,
                                    NULL,
                                    RESOURCE_TYPE_STRING,
                                    RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_NEVER) != nullptr) &&
            allOk;
    // authRequired (non-secret) tells the client whether credentials must be applied to the media/
    // snapshot URLs, and the write-only credentials are client-supplied. All three are seeded in
    // fetchInitialResourceValues (from the derived value and any persisted values) and created here via
    // createEndpointResourceIfAvailable so a reconfiguration preserves them instead of resetting them.
    allOk = (createEndpointResourceIfAvailable(onvifEp,
                                               ONVIF_RESOURCE_AUTH_REQUIRED,
                                               initialResourceValues,
                                               RESOURCE_TYPE_BOOLEAN,
                                               RESOURCE_MODE_READABLE,
                                               CACHING_POLICY_ALWAYS) != nullptr) &&
            allOk;
    allOk = (createEndpointResourceIfAvailable(onvifEp,
                                               ONVIF_RESOURCE_USERNAME,
                                               initialResourceValues,
                                               RESOURCE_TYPE_USER_ID,
                                               RESOURCE_MODE_WRITEABLE | RESOURCE_MODE_SENSITIVE,
                                               CACHING_POLICY_ALWAYS) != nullptr) &&
            allOk;
    allOk = (createEndpointResourceIfAvailable(onvifEp,
                                               ONVIF_RESOURCE_PASSWORD,
                                               initialResourceValues,
                                               RESOURCE_TYPE_PASSWORD,
                                               RESOURCE_MODE_WRITEABLE | RESOURCE_MODE_SENSITIVE,
                                               CACHING_POLICY_ALWAYS) != nullptr) &&
            allOk;

    if (!allOk)
    {
        icLogError(LOG_TAG, "registerResources: a resource failed to create for %s", device->uuid);
    }

    return allOk;
}

bool OnvifDriver::FetchAndEmitUrl(const std::string &uuid, bool snapshot)
{
    std::string serviceUrl = ReadServiceUrl(uuid);
    OnvifCredentials creds = ReadCredentials(uuid);

    if (serviceUrl.empty())
    {
        icLogError(LOG_TAG, "no ONVIF service URL for %s", uuid.c_str());

        return false;
    }

    // A camera that requires authentication needs both credentials before we contact it; without them
    // the execute fails rather than issuing a request the camera will reject. A camera that answered
    // anonymously during discovery (authRequired false) is queried anonymously when no credentials are
    // configured, so an open camera streams without the operator inventing throwaway credentials. Read
    // the persisted authRequired resource (not the volatile discovery cache) so this holds after a restart.
    bool authRequired = ReadAuthRequired(uuid);

    if (authRequired && (creds.username.empty() || creds.password.empty()))
    {
        icLogError(LOG_TAG,
                   "missing ONVIF credentials for %s; refusing %s",
                   uuid.c_str(),
                   snapshot ? "getSnapshotUrl" : "getMediaUrl");

        return false;
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

        // The URL is camera-controlled; only emit an rtsp:// media URL or an http(s):// snapshot URL so
        // a malformed/compromised camera cannot make the client fetch a file:// or other-scheme target.
        bool validScheme =
            snapshot ? (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) : (url.rfind("rtsp://", 0) == 0);

        if (!validScheme)
        {
            icLogError(LOG_TAG,
                       "%s for %s returned an unexpected URL scheme; not emitting",
                       snapshot ? "getSnapshotUrl" : "getMediaUrl",
                       uuid.c_str());

            return;
        }

        // Pin the URL to the discovered camera's host so a compromised camera cannot redirect the
        // client (and the credentials it applies) to a different host. This is host pinning only: the
        // port is intentionally not compared (the media/RTSP port legitimately differs from the HTTP
        // service port), so a same-host/different-port URL is accepted. Reject an empty parsed host so
        // a malformed URL (e.g. rtsp:///path) cannot pass by matching an empty host.
        std::string urlHost = UrlHost(url);

        if (urlHost.empty() || urlHost != UrlHost(serviceUrl))
        {
            icLogError(LOG_TAG,
                       "%s for %s returned a URL on a different host than the camera; not emitting",
                       snapshot ? "getSnapshotUrl" : "getMediaUrl",
                       uuid.c_str());

            return;
        }

        // Defensive: also strip any userinfo from the emitted value so the mediaUrl/snapshotUrl event
        // stays credential-free even if a future parse path does not.
        EmitResourceUpdate(uuid,
                           ONVIF_ENDPOINT_ID,
                           snapshot ? ONVIF_RESOURCE_SNAPSHOT_URL : ONVIF_RESOURCE_MEDIA_URL,
                           StripUrlUserinfo(url));
    }).detach();

    return true;
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
            return FetchAndEmitUrl(uuid, false);
        }

        if (strcmp(id, ONVIF_FUNCTION_GET_SNAPSHOT_URL) == 0)
        {
            return FetchAndEmitUrl(uuid, true);
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

    // StopDiscovery only signals; join here so the thread is reaped before the driver is destroyed (a
    // joinable std::thread destructor calls std::terminate). Move the handle out under the mutex and
    // join without holding it, since the worker needs the same mutex for its own exit decision. This may
    // block until the in-flight probe finishes, which is acceptable on shutdown.
    std::thread toJoin;
    {
        std::lock_guard<std::mutex> lock(discoveryThreadMutex);
        toJoin = std::move(discoveryThread);
    }

    if (toJoin.joinable())
    {
        toJoin.join();
    }
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

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    return static_cast<OnvifDriver *>(ctx)->RegisterResources(device, initialResourceValues);
}

// Seed the driver-supplied resource values (credentials and the derived authRequired) so the device
// service recreates them with their current values during a reconfiguration rather than resetting them.
static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    return static_cast<OnvifDriver *>(ctx)->FetchInitialResourceValues(device, initialResourceValues);
}

// No cached device state to refresh. The device service invokes this unconditionally when an ONVIF
// device's reconfiguration fails, so a real (no-op) callback must exist to avoid a null dereference.
static void synchronizeDevice(void *, icDevice *) {}

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

static void startup(void *ctx)
{
    static_cast<OnvifDriver *>(ctx)->Startup();
}

static bool getDeviceClassVersion(void *, const char *, uint8_t *version)
{
    if (version != nullptr)
    {
        *version = ONVIF_DEVICE_CLASS_VERSION;
    }

    return true;
}

#endif // BARTON_CONFIG_ONVIF
