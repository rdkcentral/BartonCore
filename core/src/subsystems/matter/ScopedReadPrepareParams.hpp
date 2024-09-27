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

#include "app/AttributePathParams.h"
#include "app/EventPathParams.h"
#include "app/ReadPrepareParams.h"
#include "lib/core/DataModelTypes.h"
#include "transport/Session.h"
#include <memory>
#include <vector>

namespace zilker
{
    class ScopedReadPrepareParams final
    {
    public:
        ScopedReadPrepareParams(const ScopedReadPrepareParams &other) = delete;
        ScopedReadPrepareParams(const std::vector<chip::app::AttributePathParams> &attributes,
                                const std::vector<chip::app::EventPathParams> &events,
                                const chip::SessionHandle &handle) :
            params(handle)
        {
            if (attributes.size() > 0)
            {
                params.mAttributePathParamsListSize = attributes.size();
                params.mpAttributePathParamsList = new chip::app::AttributePathParams[attributes.size()];

                for (std::vector<chip::app::AttributePathParams>::size_type i = 0; i < attributes.size(); i++)
                {
                    params.mpAttributePathParamsList[i] = attributes[i];
                }
            }

            if (events.size() > 0)
            {
                params.mEventPathParamsListSize = events.size();
                params.mpEventPathParamsList = new chip::app::EventPathParams[events.size()];

                for (std::vector<chip::app::EventPathParams>::size_type i = 0; i < events.size(); i++)
                {
                    params.mpEventPathParamsList[i] = events[i];
                }
            }
        }

        chip::app::ReadPrepareParams &Get() { return params; }

        ~ScopedReadPrepareParams()
        {
            if (params.mAttributePathParamsListSize != 0)
            {
                delete[] params.mpAttributePathParamsList;
            }

            if (params.mEventPathParamsListSize != 0)
            {
                delete[] params.mpEventPathParamsList;
            }
        }

    private:
        chip::app::ReadPrepareParams params;
    };
} // namespace zilker