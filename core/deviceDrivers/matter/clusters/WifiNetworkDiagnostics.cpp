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

//
// Created by tlea on 8/9/2023
//

#define LOG_TAG     "WifiNetworkDiagnosticsCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

#include "glib-2.0/glib.h"

extern "C" {
#include "deviceServiceConfiguration.h"
#include "deviceServiceProps.h"
#include "provider/barton-core-property-provider.h"
#include <icLog/logging.h>
}

#include <memory>
#include "WifiNetworkDiagnostics.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include "app-common/zap-generated/ids/Clusters.h"
#include "app/BufferedReadCallback.h"
#include "app/InteractionModelEngine.h"

using namespace barton;

bool WifiNetworkDiagnostics::GetRssi(void *context,
                                     chip::Messaging::ExchangeManager &exchangeMgr,
                                     const chip::SessionHandle &sessionHandle)
{
    icDebug();

    using namespace chip::app::Clusters::WiFiNetworkDiagnostics;
    using TypeInfo = Attributes::Rssi::TypeInfo;

    // Per Matter 1.4 11.15.6, the RSSI attribute is not reportable, so its value is not populated in the cache via
    // subscription reports. Therefore, we must read it on-demand.
    // This context pointer is passed to the read attribute callback, which will be responsible for deleting it. However,
    // if the ReadAttribute request fails to be sent, then we must delete it here.
    auto rssiReadContext = new OnDemandReadContext(context, eventHandler, deviceId);

    chip::Controller::ClusterBase cluster(exchangeMgr, sessionHandle, endpointId);

    CHIP_ERROR error = cluster.ReadAttribute<TypeInfo>(
        rssiReadContext,
        [](void *context,
           TypeInfo::DecodableArgType rssiValue) { /*read response success callback*/
                                                   auto rssiReadContext = static_cast<OnDemandReadContext *>(context);
                                                   const int8_t *nullableRssiValue =
                                                       rssiValue.IsNull() ? nullptr : &rssiValue.Value();
                                                   static_cast<WifiNetworkDiagnostics::EventHandler *>(
                                                       rssiReadContext->eventHandler)
                                                       ->RssiReadComplete(rssiReadContext->deviceId,
                                                                          nullableRssiValue,
                                                                          true,
                                                                          rssiReadContext->baseReadContext);
                                                   delete rssiReadContext;
        },
        [](void *context,
           CHIP_ERROR error) { /*read response failure callback*/
                               auto rssiReadContext = static_cast<OnDemandReadContext *>(context);
                               icError("Failed to read RSSI attribute from device %s with error: %s",
                                       rssiReadContext->deviceId.c_str(),
                                       error.AsString());
                               static_cast<WifiNetworkDiagnostics::EventHandler *>(rssiReadContext->eventHandler)
                                   ->RssiReadComplete(
                                       rssiReadContext->deviceId, nullptr, false, rssiReadContext->baseReadContext);
                               delete rssiReadContext;
        });

    if (error != CHIP_NO_ERROR)
    {
        icError("Rssi ReadAttribute command failed with error: %s", error.AsString());
        delete rssiReadContext;
        return false;
    }

    return true;
};
