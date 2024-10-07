//------------------------------ tabstop = 4 ----------------------------------
//
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
// any other form of intellectual property whatsoever.my
//
// Comcast Corporation retains all ownership rights.
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

namespace zilker {

class AccessRestrictionProvider : public chip::Access::AccessRestrictionProvider
{
public:
    AccessRestrictionProvider() : chip::Access::AccessRestrictionProvider() {}

    ~AccessRestrictionProvider() {}

protected:
    CHIP_ERROR DoRequestFabricRestrictionReview(const chip::FabricIndex fabricIndex, uint64_t token, const std::vector<Entry> & arl)
    {
        // this example simply removes all restrictions and will generate AccessRestrictionEntryChanged events
        chip::Access::GetAccessControl().GetAccessRestrictionProvider()->SetEntries(fabricIndex, std::vector<Entry>{});

        chip::app::Clusters::AccessControl::Events::FabricRestrictionReviewUpdate::Type event{ .token       = token,
                                                                                               .fabricIndex = fabricIndex };
        chip::EventNumber eventNumber;
        ReturnErrorOnFailure(chip::app::LogEvent(event, chip::kRootEndpointId, eventNumber));

        return CHIP_NO_ERROR;
    }
};

} // namespace zilker
