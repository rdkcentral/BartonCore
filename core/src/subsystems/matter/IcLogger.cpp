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
