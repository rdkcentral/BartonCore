// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Raiyan Chowdhury on 3/4/2026.
//

#define LOG_TAG     "IdentifyCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "Identify.hpp"
#include "app/ConcreteAttributePath.h"
#include "lib/core/CHIPError.h"

using namespace chip::app::Clusters::Identify;


namespace barton
{
    void Identify::OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                     const chip::app::ConcreteAttributePath &path)
    {
        using namespace chip::app;

        if (path.mEndpointId != endpointId || path.mClusterId != Clusters::Identify::Id)
        {
            return;
        }

        switch (path.mAttributeId)
        {
            case Attributes::IdentifyTime::Id:
            {
                using TypeInfo = Attributes::IdentifyTime::TypeInfo;

                TypeInfo::DecodableType value;
                CHIP_ERROR error = cache->Get<TypeInfo>(path, value);
                if (error == CHIP_NO_ERROR)
                {
                    static_cast<Identify::EventHandler *>(eventHandler)->IdentifyTimeChanged(deviceId, value);
                }
                else
                {
                    icError("Failed to read IdentifyTime attribute for device %s: %s", deviceId.c_str(),
                            error.AsString());
                }
                break;
            }

            default:
                break;
        }
    }
} // namespace barton
