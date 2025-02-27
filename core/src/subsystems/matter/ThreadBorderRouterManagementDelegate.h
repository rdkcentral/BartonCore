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
 * Created by Thomas Lea on 11/6/24.
 */

#pragma once

#include <app/clusters/thread-border-router-management-server/thread-border-router-management-server.h>
#include <lib/support/Span.h>

namespace barton
{

    class ThreadBorderRouterManagementDelegate final
        : public chip::app::Clusters::ThreadBorderRouterManagement::Delegate
    {
        CHIP_ERROR Init(AttributeChangeCallback *attributeChangeCallback) override;

        /**
         * @brief Return whether or not this border router supports changing the PAN.
         *
         * @note This is disruptive to dual-stack configurations with Zigbee since Zigbee
         *       does not handle channel changing as well as Thread, so we indicate not
         *       supported.
         */
        bool GetPanChangeSupported() override { return false; }

        void GetBorderRouterName(chip::MutableCharSpan &borderRouterName) override;

        CHIP_ERROR GetBorderAgentId(chip::MutableByteSpan &borderAgentId) override;

        uint16_t GetThreadVersion() override;

        bool GetInterfaceEnabled() override;

        CHIP_ERROR GetDataset(chip::Thread::OperationalDataset &dataset, DatasetType type) override;

        void SetActiveDataset(const chip::Thread::OperationalDataset &activeDataset,
                              uint32_t sequenceNum,
                              ActivateDatasetCallback *callback) override;

        CHIP_ERROR CommitActiveDataset() override;

        CHIP_ERROR RevertActiveDataset() override { return CHIP_ERROR_NOT_IMPLEMENTED; }

        CHIP_ERROR SetPendingDataset(const chip::Thread::OperationalDataset &pendingDataset) override;

    private:
        AttributeChangeCallback *mAttributeChangeCallback;
        ActivateDatasetCallback *mActivateDatasetCallback = nullptr;
        uint32_t mSequenceNum                             = 0;
        static void OnMatterPlatformEventHandler(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg);
        CHIP_ERROR SaveActiveDatasetConfigured(bool configured);
    };

} // namespace barton
