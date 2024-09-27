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

#include "IcLogger.hpp"
#include "cinttypes"
#include "lib/support/logging/Constants.h"

extern "C" {
#include "icLog/logging.h"
#include "icTypes/sbrm.h"
}

#define LOG_TAG     "MatterSDK"
#define logFmt(fmt) fmt

using namespace zilker;

void IcLogger::OnLogMessage(const char *module, uint8_t category, const char *msg, va_list args)
{
    scoped_generic char *formattedMsg = FormatMatterLog(msg, args);

    if (formattedMsg == nullptr)
    {
        icError("Failed to format string '%s'!", msg);
        return;
    }

    switch (category)
    {
        case chip::Logging::kLogCategory_Error:
            icError("[%s] %s", module, formattedMsg);
            break;

        case chip::Logging::kLogCategory_Progress:
            icDebug("[%s] %s", module, formattedMsg);
            break;

        case chip::Logging::kLogCategory_Detail:
            // Fallthrough: LogCategory_Detail is mostly ultra-fine info like
            // raw message bytes: trace them.
        case chip::Logging::kLogCategory_Max:
            icTrace("[%s] %s", module, formattedMsg);
            break;

        case chip::Logging::kLogCategory_None:
            break;

        default:
            icError("[%s] Invalid log category %" PRIu8 "; message: %s", module, category, formattedMsg);
            break;
    }
}