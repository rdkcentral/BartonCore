//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2022 Comcast Corporation
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
//
//------------------------------ tabstop = 4 ----------------------------------

/*
 * Created by Thomas Lea on 4/4/2022..
 */

#pragma once

#include "MatterDeviceDriver.h"
#include "clusters/DoorLock.h"

namespace zilker
{
    class MatterDoorLockDeviceDriver : public MatterDeviceDriver,
                                       DoorLock::EventHandler
    {
    public:
        MatterDoorLockDeviceDriver();

        bool ClaimDevice(DiscoveredDeviceDetails *details) override;

        // DoorLock cluster callbacks
        void CommandCompleted(void *context, bool success) override { OnDeviceWorkCompleted(context, success); };
        void WriteRequestCompleted(void *context, bool success) override { OnDeviceWorkCompleted(context, success); };
        void LockStateChanged(std::string &deviceUuid,
                              chip::app::Clusters::DoorLock::DlLockState lockState,
                              void *asyncContext) override;
        void LockStateReadComplete(std::string &deviceUuid,
                                   chip::app::Clusters::DoorLock::DlLockState lockState,
                                   void *asyncContext) override;

    protected:
        std::vector<MatterCluster *> GetClustersToSubscribeTo(const std::string &deviceId) override;

        void SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                               const std::string &deviceId,
                               chip::Messaging::ExchangeManager &exchangeMgr,
                               const chip::SessionHandle &sessionHandle) override;

        void FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                        const std::string &deviceId,
                                        icInitialResourceValues *initialResourceValues,
                                        chip::Messaging::ExchangeManager &exchangeMgr,
                                        const chip::SessionHandle &sessionHandle) override;
        bool RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues) override;

        void ReadResource(std::forward_list<std::promise<bool>> &promises,
                          const std::string &deviceId,
                          icDeviceResource *resource,
                          char **value,
                          chip::Messaging::ExchangeManager &exchangeMgr,
                          const chip::SessionHandle &sessionHandle) override;

        bool WriteResource(std::forward_list<std::promise<bool>> &promises,
                           const std::string &deviceId,
                           icDeviceResource *resource,
                           const char *previousValue,
                           const char *newValue,
                           chip::Messaging::ExchangeManager &exchangeMgr,
                           const chip::SessionHandle &sessionHandle) override;

    private:
        static bool registeredWithFactory;

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) override;
    };
} // namespace zilker
