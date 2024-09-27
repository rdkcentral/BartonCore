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

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "ScopedReadPrepareParams.hpp"
#include "app/AttributePathParams.h"
#include "app/EventPathParams.h"
#include "app/ReadPrepareParams.h"
#include "lib/core/DataModelTypes.h"
#include "transport/Session.h"

namespace zilker
{
    class ReadPrepareParamsBuilder
    {

    public:
        ReadPrepareParamsBuilder(chip::EndpointId defaultEndpointId, chip::ClusterId defaultClusterId) :
            defaultEndpoint(defaultEndpointId), defaultCluster(defaultClusterId) {};

        ReadPrepareParamsBuilder &AddAttribute(chip::AttributeId attributeId)
        {
            return AddAttribute(defaultEndpoint, defaultCluster, attributeId);
        }

        ReadPrepareParamsBuilder &AddAttribute(chip::ClusterId clusterId, chip::AttributeId attributeId)
        {
            return AddAttribute(defaultEndpoint, clusterId, attributeId);
        }

        ReadPrepareParamsBuilder &AddAttribute(chip::EndpointId endpointId, chip::ClusterId clusterId, chip::AttributeId attributeId)
        {
            return AddAttribute(endpointId, clusterId, attributeId, 0, 0);
        }

        ReadPrepareParamsBuilder &AddAttribute(chip::AttributeId attributeId, uint16_t floor, uint16_t ceil)
        {
            return AddAttribute(defaultEndpoint, defaultCluster, attributeId, floor, ceil);
        }

        ReadPrepareParamsBuilder &AddAttribute(chip::EndpointId endpointId, chip::ClusterId clusterId,
                                               chip::AttributeId attributeId, uint16_t floor, uint16_t ceil);

        ReadPrepareParamsBuilder &AddEvent(chip::EventId eventId)
        {
            return AddEvent(defaultCluster, eventId);
        }

        ReadPrepareParamsBuilder &AddEvent(chip::ClusterId clusterId, chip::EventId eventId)
        {
            return AddEvent(defaultEndpoint, clusterId, eventId);
        }

        ReadPrepareParamsBuilder &AddEvent(chip::EndpointId endpointId, chip::ClusterId clusterId, chip::EventId eventId)
        {
            return AddEvent(endpointId, clusterId, eventId, 0, 0);
        }

        ReadPrepareParamsBuilder &AddEvent(chip::EventId eventId, uint16_t floor, uint16_t ceil)
        {
            return AddEvent(defaultEndpoint, defaultCluster, eventId, floor, ceil);
        }

        ReadPrepareParamsBuilder &AddEvent(chip::EndpointId endpointId, chip::ClusterId clusterId, chip::EventId eventId,
                                           uint16_t floor, uint16_t ceil);

        std::unique_ptr<ScopedReadPrepareParams> Build(const chip::SessionHandle &sessionHandle);

    private:
        bool UpdateReportIterval(uint16_t floor, uint16_t ceil)
        {
            /*
             * For simple reads, the floor/ceiling are unused; 0 for unspecifed is valid,
             * as is any other pair that meets the constraints described in Matter 1.1
             * 8.5.3.2.
             * When used for subscriptions, floor must be <= ceil, however a zero floor
             * is excluded from valid due to its ambiguity between "unspecified" and
             * "as fast as events occur". At least "as fast as events occur" is implicitly
             * contradicted by requirements for a min interval timer to expire between event
             * transmissions (Matter 1.1 8.5); and a 0 value represents no interval at all,
             * so 1s is considered the minimum possible value. The publisher is ultimately
             * responsible for selecting a valid interval irrespective of subscriber request;
             * it is not necessary to clamp/enforce values fall within any other range here.
             */

            bool valid = ((floor == 0 && ceil == 0) || (ceil >= floor && floor != 0));

            if (valid)
            {
                reportFloorSecs = std::min(reportFloorSecs, floor);
                reportCeilSecs = std::max(reportCeilSecs, ceil);
            }

            return valid;
        }

        std::vector<chip::app::EventPathParams> events;
        std::vector<chip::app::AttributePathParams> attributes;
        chip::EndpointId defaultEndpoint;
        chip::ClusterId defaultCluster;
        uint16_t reportFloorSecs = UINT8_MAX;
        uint16_t reportCeilSecs;
    };
} // namespace zilker