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

/*
 * Created by Thomas Lea on 9/10/24.
 */

/*
 * AccessRestriction implementation for Barton
 */

#pragma once

#include <access/AccessRestrictionProvider.h>
#include <access/AccessControl.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/EventLogging.h>

namespace barton
{

    class AccessRestrictionProvider : public chip::Access::AccessRestrictionProvider
    {
    public:
        AccessRestrictionProvider() : chip::Access::AccessRestrictionProvider() {}

        ~AccessRestrictionProvider() {}

    protected:
        CHIP_ERROR DoRequestFabricRestrictionReview(const chip::FabricIndex fabricIndex,
                                                    uint64_t token,
                                                    const std::vector<Entry> &arl)
        {
            // this example simply removes all restrictions and will generate AccessRestrictionEntryChanged events
            chip::Access::GetAccessControl().GetAccessRestrictionProvider()->SetEntries(fabricIndex,
                                                                                        std::vector<Entry> {});

            chip::app::Clusters::AccessControl::Events::FabricRestrictionReviewUpdate::Type event {
                .token = token, .fabricIndex = fabricIndex};
            chip::EventNumber eventNumber;
            ReturnErrorOnFailure(chip::app::LogEvent(event, chip::kRootEndpointId, eventNumber));

            return CHIP_NO_ERROR;
        }
    };

} // namespace barton
