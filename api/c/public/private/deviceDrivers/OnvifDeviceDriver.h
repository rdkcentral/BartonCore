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
// OnvifDriver — the native ONVIF camera driver object that lives behind the C DeviceDriver struct.
//
// This class is exposed in a header (rather than kept file-local) so vendor specializations can extend
// it by C++ inheritance, reusing ONVIF discovery, the ep/camera lifecycle, the ep/onvif endpoint, and
// media/snapshot URL resolution while overriding the virtual hooks below. A specialization may live in
// an external client that links this library and registers itself via RegisterSpecialization().
//
// CLAIM MODEL — INTERIM. Ownership of a discovered camera is decided by ShouldReportCamera(): a
// registered specialization reports only cameras it ClaimsCamera()s, and the generic driver reports
// only cameras no specialization claims. This surgical, manufacturer-based yield is a short-term
// exception; the intended long-term model is a single discovery owned by an ONVIF subsystem that
// dispatches to the correct driver.
//

#pragma once

#include "onvif/OnvifSoapClient.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// device-driver.h pulls in deviceDescriptors.h → libxml2's parser.h, which on this platform drags in
// ICU C++ template headers. Include parser.h here as C++ (outside the extern "C" block) so its include
// guard keeps those templates from being re-processed with C linkage below.
#include <libxml/parser.h>

extern "C" {
#include "device-driver/device-driver.h"
#include "device/icDevice.h"
#include "device/icDeviceResource.h"
#include "device/icInitialResourceValues.h"
}

namespace barton
{
    namespace onvif
    {

        // Per-camera information captured during WS-Discovery and consulted at configuration time.
        struct DiscoveredCamera
        {
            std::string serviceUrl;
            std::string manufacturer;
            std::string model;
            std::string firmwareVersion;
            // Derived during discovery: false when the camera answered an anonymous GetDeviceInformation,
            // meaning it does not require credentials. Defaults to true (require credentials) until proven.
            bool authRequired = true;
        };

        class OnvifDriver
        {
        public:
            OnvifDriver();
            virtual ~OnvifDriver() = default;

            DeviceDriver *GetDriver() { return &driver; }

            bool StartDiscovery(const char *deviceClass);
            void StopDiscovery();

            // Lifecycle hooks. Virtual so a vendor specialization can extend behavior (e.g. add sensor
            // endpoints, run a detection poll loop) while reusing the ONVIF baseline.
            virtual void Startup() {}

            virtual bool ConfigureDevice(icDevice *device);
            virtual bool FetchInitialResourceValues(icDevice *device, icInitialResourceValues *initialResourceValues);
            virtual bool RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues);
            virtual bool ExecuteResource(icDeviceResource *resource, const char *arg, char **response);
            virtual void DeviceRemoved(icDevice *device);
            virtual void Shutdown();

            DeviceDriver driver {};

        protected:
            // Claim hook: a specialization returns true for a camera identity it manages. The generic
            // base returns false (it reports cameras that no specialization claims — see ShouldReportCamera).
            virtual bool ClaimsCamera(const std::string &manufacturer, const std::string &model) const
            {
                (void) manufacturer;
                (void) model;

                return false;
            }

            // Register `self` as a vendor specialization consulted during discovery. Called by a
            // specialization's constructor.
            static void RegisterSpecialization(OnvifDriver *self);

            // Decide whether THIS driver should report a camera with the given identity during discovery:
            // a specialization reports only cameras it claims; the generic reports only cameras no
            // registered specialization claims. This makes ownership deterministic rather than a race.
            bool ShouldReportCamera(const std::string &manufacturer, const std::string &model) const;

            bool LookupDiscovered(const std::string &uuid, DiscoveredCamera &out);
            OnvifCredentials ReadCredentials(const std::string &uuid);
            std::string ReadServiceUrl(const std::string &uuid);
            bool ReadAuthRequired(const std::string &uuid);

        private:
            void DiscoveryWorker();
            bool RunDiscoveryProbe();
            bool FetchAndEmitUrl(const std::string &uuid, bool snapshot);

            static bool AnySpecializationClaims(const std::string &manufacturer, const std::string &model);

            // True once RegisterSpecialization has tagged this instance as a vendor specialization.
            bool isSpecialization = false;

            std::mutex stateMutex;
            std::unordered_map<std::string, DiscoveredCamera> discovered;
            std::mutex discoveryThreadMutex;
            std::thread discoveryThread;
            std::atomic<bool> discoveryRunning {false};
            std::atomic<bool> discoverDesired {false};
        };

        // Wire the DeviceDriver C callbacks to the OnvifDriver dispatch thunks. Shared by the ONVIF
        // registration and by vendor specializations so they reuse the same (virtual-dispatching) thunks.
        void OnvifWireDriverCallbacks(DeviceDriver *driver);

    } // namespace onvif
} // namespace barton
