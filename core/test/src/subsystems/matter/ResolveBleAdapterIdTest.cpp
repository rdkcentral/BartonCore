//------------------------------ tabstop = 4 ----------------------------------
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
//------------------------------ tabstop = 4 ----------------------------------

#include "subsystems/matter/MatterCommon.h"
#include <gtest/gtest.h>

extern "C" {
#include "MockPropertyProvider.h"
#include "barton-core-properties.h"
#include "deviceServiceConfiguration.h"
#include "provider/barton-core-property-provider.h"
}

using barton::Subsystem::Matter::ResolveBleAdapterId;

namespace
{
    class ResolveBleAdapterIdTest : public ::testing::Test
    {
    protected:
        void SetUp() override { MockPropertyProviderInit(); }

        void TearDown() override { MockPropertyProviderReset(); }

        static void SetAdapterId(const char *value)
        {
            g_autoptr(BCorePropertyProvider) provider = deviceServiceConfigurationGetPropertyProvider();
            b_core_property_provider_set_property_string(provider, B_CORE_BARTON_BLE_ADAPTER_ID, value);
        }
    };

    TEST_F(ResolveBleAdapterIdTest, DefaultsToZeroWhenUnset)
    {
        EXPECT_EQ(ResolveBleAdapterId(), 0u);
    }

    TEST_F(ResolveBleAdapterIdTest, UsesValidValue)
    {
        SetAdapterId("1");
        EXPECT_EQ(ResolveBleAdapterId(), 1u);

        SetAdapterId("9999");
        EXPECT_EQ(ResolveBleAdapterId(), 9999u);
    }

    TEST_F(ResolveBleAdapterIdTest, RejectsOutOfRangeAndMalformedValues)
    {
        for (const char *value : {"-1", "10000", "4294967295", "4294967296", "abc", ""})
        {
            SetAdapterId(value);
            EXPECT_EQ(ResolveBleAdapterId(), 0u) << "value: '" << value << "'";
        }
    }
} // namespace
