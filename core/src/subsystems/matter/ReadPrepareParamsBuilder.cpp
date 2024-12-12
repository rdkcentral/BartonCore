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

#include "ReadPrepareParamsBuilder.hpp"
#include "ScopedReadPrepareParams.hpp"
#include "app/AttributePathParams.h"
#include "app/EventPathParams.h"
#include "app/ReadPrepareParams.h"
#include <memory>

extern "C" {
#define LOG_TAG "Matter"
#include "icLog/logging.h"
#define logFmt(fmt) "(%s):" fmt, __func__
}

using namespace zilker;

std::unique_ptr<ScopedReadPrepareParams> ReadPrepareParamsBuilder::Build(const chip::SessionHandle &sessionHandle)
{
    if (attributes.empty() && events.empty())
    {
        return nullptr;
    }

    auto params = std::make_unique<ScopedReadPrepareParams>(attributes, events, sessionHandle);
    params->Get().mMaxIntervalCeilingSeconds = reportCeilSecs;
    params->Get().mMinIntervalFloorSeconds = reportFloorSecs;

    return params;
}

ReadPrepareParamsBuilder &ReadPrepareParamsBuilder::AddAttribute(chip::EndpointId endpointId,
                                                                 chip::ClusterId clusterId,
                                                                 chip::AttributeId attributeId,
                                                                 uint16_t floor,
                                                                 uint16_t ceil)
{
    if (UpdateReportIterval(floor, ceil))
    {
        attributes.push_back({endpointId, clusterId, attributeId});
    }
    else
    {
        icWarn("Ignoring cluster %#" PRIx32 " attribute %#" PRIx32 " on endpoint %#" PRIx8 ": Constraint ceil (%" PRIu16
               ") >= nonzero floor (%" PRIu16 ") not met",
               clusterId,
               attributeId,
               endpointId,
               ceil,
               floor);
    }

    return *this;
}

ReadPrepareParamsBuilder &ReadPrepareParamsBuilder::AddEvent(chip::EndpointId endpointId,
                                                             chip::ClusterId clusterId,
                                                             chip::EventId eventId,
                                                             uint16_t floor,
                                                             uint16_t ceil)
{
    if (UpdateReportIterval(floor, ceil))
    {
        events.push_back({endpointId, clusterId, eventId});
    }
    else
    {
        icWarn("Ignoring cluster %#" PRIx32 " event %#" PRIx32 " on endpoint %#" PRIx8 ": Constraint ceil (%" PRIu16
               ") >= nonzero floor (%" PRIu16 ") not met",
               clusterId,
               eventId,
               endpointId,
               ceil,
               floor);
    }

    return *this;
}
