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

#include "ThreadBorderRouterManagementDelegate.h"
#include "lib/core/CHIPError.h"
#include "lib/support/Span.h"
#include "subsystems/thread/threadSubsystem.h"
#include <app/server/Server.h>
#include <cstdint>

extern "C" {
#include "icLog/logging.h"
}

#define logFmt(fmt) "%s: " fmt, __func__
#define LOG_TAG     "tbrmd"

using namespace barton;
using namespace chip;
using namespace chip::DeviceLayer;

namespace
{
    constexpr char kFailsafeActiveDatasetConfigured[] = "g/fs/tbradc";
}

CHIP_ERROR ThreadBorderRouterManagementDelegate::Init(AttributeChangeCallback *attributeChangeCallback)
{
    icDebug();

#ifndef BARTON_CONFIG_THREAD
    return CHIP_ERROR_NOT_IMPLEMENTED;
#else
    mAttributeChangeCallback = attributeChangeCallback;

    return chip::DeviceLayer::PlatformMgrImpl().AddEventHandler(OnMatterPlatformEventHandler,
                                                           reinterpret_cast<intptr_t>(this));
#endif
}

void ThreadBorderRouterManagementDelegate::GetBorderRouterName(chip::MutableCharSpan &borderRouterName)
{
    icDebug();

#ifdef BARTON_CONFIG_THREAD
    g_autoptr(ThreadNetworkInfo) info = threadNetworkInfoCreate();
    if (threadSubsystemGetNetworkInfo(info) && info->networkName != NULL)
    {
        CopyCharSpanToMutableCharSpan(CharSpan::fromCharString(info->networkName), borderRouterName);
    }
#endif
}

CHIP_ERROR ThreadBorderRouterManagementDelegate::GetBorderAgentId(chip::MutableByteSpan &borderAgentId)
{
    icDebug();

#ifndef BARTON_CONFIG_THREAD
    return CHIP_ERROR_NOT_IMPLEMENTED;
#else
    CHIP_ERROR err = CHIP_ERROR_READ_FAILED;

    GArray *borderAgentIdArray = nullptr;
    if (threadSubsystemGetBorderAgentId(&borderAgentIdArray) && borderAgentIdArray != nullptr)
    {
        borderAgentId = MutableByteSpan(reinterpret_cast<uint8_t *>(borderAgentIdArray->data), borderAgentIdArray->len);
        err          = CHIP_NO_ERROR;
    }

    g_array_unref(borderAgentIdArray);

    return err;
#endif
}

uint16_t ThreadBorderRouterManagementDelegate::GetThreadVersion()
{
    icDebug();

    uint16_t version = 0;
#ifdef BARTON_CONFIG_THREAD
    version = threadSubsystemGetThreadVersion();
#endif
    return version;
}

bool ThreadBorderRouterManagementDelegate::GetInterfaceEnabled()
{
    icDebug();

#ifndef BARTON_CONFIG_THREAD
    return false;
#else
    return threadSubsystemIsThreadInterfaceUp();
#endif
}

CHIP_ERROR ThreadBorderRouterManagementDelegate::GetDataset(chip::Thread::OperationalDataset &dataset, DatasetType type)
{
    icDebug();

#ifndef BARTON_CONFIG_THREAD
    return CHIP_ERROR_NOT_IMPLEMENTED;
#else
    GArray *datasetTlvs = nullptr;
    bool success        = false;
    if (type == DatasetType::kActive)
    {
        success = threadSubsystemGetActiveDatasetTlvs(&datasetTlvs);
    }
    else
    {
         success = threadSubsystemGetPendingDatasetTlvs(&datasetTlvs);
    }

    if (!success || datasetTlvs == nullptr)
    {
        return CHIP_ERROR_INTERNAL;
    }
    else
    {
        return dataset.Init(ByteSpan(reinterpret_cast<uint8_t*>(datasetTlvs->data), datasetTlvs->len));
    }
#endif
}

void ThreadBorderRouterManagementDelegate::SetActiveDataset(const chip::Thread::OperationalDataset &activeDataset,
                                                            uint32_t sequenceNum,
                                                            ActivateDatasetCallback *callback)
{
    icDebug();

#ifdef BARTON_CONFIG_THREAD
    // This function will never be invoked when there is an Active Dataset already configured.

    // first save off a marker indicating that there is no active dataset configured
    // next tell the stack to attach to the provided dataset

    CHIP_ERROR err = SaveActiveDatasetConfigured(false);
    if (err == CHIP_NO_ERROR)
    {
        err = DeviceLayer::ThreadStackMgrImpl().AttachToThreadNetwork(activeDataset, nullptr);
    }
    if (err != CHIP_NO_ERROR)
    {
        callback->OnActivateDatasetComplete(sequenceNum, err);
        return;
    }
    mSequenceNum              = sequenceNum;
    mActivateDatasetCallback  = callback;
#endif
}

CHIP_ERROR ThreadBorderRouterManagementDelegate::CommitActiveDataset()
{
    icDebug();

#ifndef BARTON_CONFIG_THREAD
    return CHIP_ERROR_NOT_IMPLEMENTED;
#else
    return SaveActiveDatasetConfigured(DeviceLayer::ThreadStackMgrImpl().IsThreadAttached());
#endif
}

CHIP_ERROR
ThreadBorderRouterManagementDelegate::SetPendingDataset(const chip::Thread::OperationalDataset &pendingDataset)
{
    icDebug();

    return CHIP_ERROR_NOT_IMPLEMENTED;
}

void ThreadBorderRouterManagementDelegate::OnMatterPlatformEventHandler(const DeviceLayer::ChipDeviceEvent *event,
                                                                        intptr_t arg)
{
    icDebug();

    ThreadBorderRouterManagementDelegate *delegate = reinterpret_cast<ThreadBorderRouterManagementDelegate *>(arg);
    if (delegate)
    {
        if ((event->Type == DeviceLayer::DeviceEventType::kThreadConnectivityChange) &&
            (event->ThreadConnectivityChange.Result == DeviceLayer::kConnectivity_Established) &&
            delegate->mActivateDatasetCallback)
        {
            delegate->mActivateDatasetCallback->OnActivateDatasetComplete(delegate->mSequenceNum, CHIP_NO_ERROR);
            delegate->mActivateDatasetCallback = nullptr;
        }
    }
}

CHIP_ERROR ThreadBorderRouterManagementDelegate::SaveActiveDatasetConfigured(bool configured)
{
    icDebug();

    auto &storage = Server::GetInstance().GetPersistentStorage();
    return storage.SyncSetKeyValue(kFailsafeActiveDatasetConfigured, &configured, sizeof(bool));
}
