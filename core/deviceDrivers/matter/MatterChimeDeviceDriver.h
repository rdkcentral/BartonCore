//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
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
 * Created by Thomas Lea on 11/15/22.
 */

#pragma once

#include "MatterDeviceDriver.h"
#include "clusters/ComcastChime.h"
#include "clusters/LevelControl.h"
#include "clusters/WifiNetworkDiagnostics.h"
#include "lib/core/DataModelTypes.h"

extern "C" {
#include <glib.h>
#ifdef HAVE_GLIB_POLYFILL
#include <glib-polyfill.h>
#endif
}

namespace zilker
{
    class MatterChimeDeviceDriver : public MatterDeviceDriver
    {
    public:
        MatterChimeDeviceDriver();

        bool ClaimDevice(DiscoveredDeviceDetails *details) override;

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

        void ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             icDeviceResource *resource,
                             const char *arg,
                             char **response,
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

        bool shouldPlaySound(icDeviceResource *resource, cJSON *json);

        static uint8_t getLinkScore(int8_t rssi);

        std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) override;

        class ChimeClusterEventHandler : public ComcastChime::EventHandler
        {
        public:
            void CommandCompleted(void *context, bool success) override
            {
                deviceDriver->OnDeviceWorkCompleted(context, success);
            };

            void AudioAssetsChanged(const std::string &deviceUuid,
                                    std::unique_ptr<std::string> assets,
                                    void *asyncContext) override;

            void AudioAssetsReadComplete(const std::string &deviceUuid,
                                         std::unique_ptr<std::string> assets,
                                         void *asyncContext) override;

            MatterChimeDeviceDriver *deviceDriver;
        } chimeClusterEventHandler;

        class LevelControlClusterEventHandler : public LevelControl::EventHandler
        {
        public:
            void CommandCompleted(void *context, bool success) override
            {
                deviceDriver->OnDeviceWorkCompleted(context, success);
            };

            void CurrentLevelChanged(const std::string &deviceUuid, uint8_t level, void *asyncContext) override;
            void CurrentLevelReadComplete(const std::string &deviceUuid, uint8_t level, void *asyncContext) override;
            MatterChimeDeviceDriver *deviceDriver;
        } levelControlClusterEventHandler;

        class WifiNetworkDiagnosticsEventHandler : public WifiNetworkDiagnostics::EventHandler
        {
        public:
            void CommandCompleted(void *context, bool success) override
            {
                deviceDriver->OnDeviceWorkCompleted(context, success);
            };

            void RssiChanged(const std::string &deviceUuid, int8_t *rssi, void *asyncContext) override;
            void RssiReadComplete(const std::string &deviceUuid, int8_t *rssi, void *asyncContext) override;
            MatterChimeDeviceDriver *deviceDriver;
        } wifiDiagnosticsClusterEventHandler;

        /**
         * @brief Send PlayUrl command
         *
         * @param promises
         * @param deviceId
         * @param arg URI to send with the command
         * @param exchangeMgr
         * @param sessionHandle
         */
        void PlaySound(std::forward_list<std::promise<bool>> &promises,
                       const std::string &deviceId,
                       const char *uriStr,
                       chip::Messaging::ExchangeManager &exchangeMgr,
                       const chip::SessionHandle &sessionHandle);

        /**
         * @brief Send StopPlay command
         *
         * @param promises
         * @param deviceId
         * @param exchangeMgr
         * @param sessionHandle
         */
        void StopSound(std::forward_list<std::promise<bool>> &promises,
                       const std::string &deviceId,
                       chip::Messaging::ExchangeManager &exchangeMgr,
                       const chip::SessionHandle &sessionHandle);

        bool shouldPlaySound(const char *deviceUuid, GUri *uri);

        bool IsSilenced(const char *deviceUuid);

        std::string CreateChimeLabel(icDevice *device);
    };
} // namespace zilker
