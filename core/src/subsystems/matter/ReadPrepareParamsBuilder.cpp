// Copyright (C) 2024 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.

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

ReadPrepareParamsBuilder &ReadPrepareParamsBuilder::AddAttribute(chip::EndpointId endpointId, chip::ClusterId clusterId,
                                                                 chip::AttributeId attributeId, uint16_t floor, uint16_t ceil)
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

ReadPrepareParamsBuilder &ReadPrepareParamsBuilder::AddEvent(chip::EndpointId endpointId, chip::ClusterId clusterId,
                                                             chip::EventId eventId, uint16_t floor, uint16_t ceil)
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
