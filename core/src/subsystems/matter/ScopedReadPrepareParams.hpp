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

#pragma once

#include "app/AttributePathParams.h"
#include "app/EventPathParams.h"
#include "app/ReadPrepareParams.h"
#include "lib/core/DataModelTypes.h"
#include "transport/Session.h"
#include <memory>
#include <vector>

namespace barton
{
    class ScopedReadPrepareParams final
    {
    public:
        ScopedReadPrepareParams(const ScopedReadPrepareParams &other) = delete;
        ScopedReadPrepareParams(const std::vector<chip::app::AttributePathParams> &attributes,
                                const std::vector<chip::app::EventPathParams> &events,
                                const chip::SessionHandle &handle) : params(handle)
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
} // namespace barton
