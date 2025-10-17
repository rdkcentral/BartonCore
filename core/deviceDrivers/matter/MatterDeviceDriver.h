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
 * Created by Thomas Lea on 11/15/21.
 */

#pragma once

#include "app/AttributePathParams.h"
#include "app/ClusterStateCache.h"
#include "app/EventPathParams.h"
#include "app/InteractionModelEngine.h"
#include "app/OperationalSessionSetup.h"
#include "app/ReadClient.h"
#include "app/ReadPrepareParams.h"
#include "clusters/BasicInformation.hpp"
#include "clusters/GeneralDiagnostics.h"
#include "clusters/MatterCluster.h"
#include "clusters/OTARequestor.h"
#include "lib/core/CHIPCallback.h"
#include "lib/core/DataModelTypes.h"
#include "subscriptions/SubscribeInteraction.h"
#include "subsystems/matter/DiscoveredDeviceDetails.h"
#include "subsystems/matter/Matter.h"
#include <condition_variable>
#include <forward_list>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#define MATTER_ASYNC_DEVICE_TIMEOUT_SECS             15
/*
 * The synchronize operation gets a longer timeout in order to give Matter time to reestablish a CASE session
 * with the device.
 */
#define MATTER_ASYNC_SYNCHRONIZE_DEVICE_TIMEOUT_SECS 90

#define MATTER_RECONFIGURATION_DELAY_SECS            60

#define CLUSTER_SERVERS_MAP_DEVICE_ID_IDX            0
#define CLUSTER_SERVERS_MAP_ENDPOINT_ID_IDX          1
#define CLUSTER_SERVERS_MAP_CLUSTER_ID_IDX           2

#include <libxml/parser.h>

extern "C" {
#include "device/icDeviceResource.h"
#include "glib.h"
#include "icUtil/stringUtils.h"
#include <commonDeviceDefs.h>
#include <device-driver/device-driver.h>
#include <deviceServiceCommFail.h>
#include <deviceServicePrivate.h>
#include <icTypes/icStringHashMap.h>
};

namespace barton
{
    using namespace chip::app::Clusters;
    using namespace chip::app::DataModel;
    using ClusterKey = std::tuple<std::string, chip::EndpointId, chip::ClusterId>;
    using OtaSoftwareUpdateRequestorStateTransitionEvent =
        chip::app::Clusters::OtaSoftwareUpdateRequestor::Events::StateTransition::DecodableType;

    /**
     * This is the base class to Matter device drivers.  It provides the common interface to drivers and also provides
     * the mapping from the synchronous model expected by device service to the asynchronous model provided by the
     * Matter stack.  For example, the C driver interface function "configureDevice" calls the driver's ConfigureDevice
     * method, which is asynchronous.  This mapping takes care of blocking for a timeout and awaiting a final success
     * or failure from the asynchronous invocation chain provided by the concrete driver class.
     */
    class MatterDeviceDriver
    {
    public:
        MatterDeviceDriver(const char *driverName, const char *deviceClass, const uint8_t dcVersion);

        virtual ~MatterDeviceDriver();

        virtual bool ClaimDevice(DiscoveredDeviceDetails *details) = 0;
        DeviceDriver *GetDriver() { return &driver; }
        uint8_t GetDeviceClassVersion() const { return deviceClassVersion; }
        virtual icStringHashMap *GetEndpointToProfileMap(const DiscoveredDeviceDetails *details);
        const char *GetDeviceClass() const;

        virtual bool DeviceRemoved(icDevice *device);

        /**
         * @brief Safely a run callable on the Matter stack, waiting for it to finish.
         * This will schedule a runnable on the Matter event loop, abandoning the task
         * if it can't be scheduled.
         * @param work The function to run on the stack
         * @note if work is too large (e.g., too many lambda captures), a
         *       static_assert will abort compilation. When this happens, aggregate
         *       captures to reduce the functor size to fit on the event loop.
         */
        void RunOnMatterSync(std::function<void(void)> work);

        // Synchronous DeviceDriver interface entrypoints. These methods can only interact with the Matter
        // stack if they are run in a protected stack context with chip::DeviceLayer::SystemLayer().ScheduleLambda,
        // chip::DeviceLayer::PlatformMgr().ScheduleWork, RunOnMatterSync, or while holding LockChipStack().
        // These rules extend to interactions with the Matter class and cluster classes. See std::promise to synchronize
        // Matter tasks with the calling thread.
        virtual void Startup();
        virtual bool DevicePersisted(icDevice *device);

        /**
         * @brief ConnectAndExecute will invoke this upon successful connection.
         *        Perform device interactions in this callback, which may be a
         *        lambda expression or ordinary function. Execution must be as fast
         *        as possible, as this is called on the device event loop and must not block.
         *
         * @param promises Move/emplace promises into this container to indicate work
         *                 has been or will be completed. Set the value to indicate success/failure.
         *                 Upon completion, invoke set_value() on created promises.
         *                 Note that if no promises are in the container, ConnectAndExecute
         *                 will indicate success and true.
         *
         * @param deviceId The device uuid for this work
         * @param exchangeMgr A valid exchange manager for the connected device. Do not store.
         * @param sessionHandle A valid session handle for the connected device. Do not store.
         */
        using connect_work_cb = std::function<void(std::forward_list<std::promise<bool>> &promises,
                                                   const std::string &deviceId,
                                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                                   const chip::SessionHandle &sessionHandle)>;

        /**
         * @brief Called by synchronous entry points to do arbitrary work in the
         *        Matter SDK's device event loop thread context.
         *
         * @param deviceId UUID to connect to
         * @param work Callable to invoke upon successful connection. This is guaranteed to
         *             not be called unless the device is actually connected.
         * @see connect_work_cb for callback declaration
         * @param timeoutSeconds Give up on the entire operation after this many seconds
         * @return true When all work has successfully completed
         * @return false
         */
        bool ConnectAndExecute(const std::string &deviceId, connect_work_cb &&work, uint16_t timeoutSeconds);

        /**
         * @brief Used by ConnectAndExecute to abort a connection attempt. This guarantees
         *        successCb/failCb have either been called already or never will be.
         *
         * @param successCb
         * @param failCb
         */
        void AbortDeviceConnectionAttempt(chip::Callback::Callback<chip::OnDeviceConnected> &successCb,
                                          chip::Callback::Callback<chip::OnDeviceConnectionFailure> &failCb);


        // The following methods are to be called via work functions passed to
        // ConnectAndExecute. To promise asynchronous work, add to the promises list.
        // Never remove an object from the list. To perform asynchronous work without
        // caring about whether it finishes or not, simply do not make a corresponding promise.

        void Shutdown();

        /*
         * Configures the device using the common driver - configures common matter clusters
         * such as the OTARequestor cluster for base functionality.
         */
        void ConfigureDevice(std::forward_list<std::promise<bool>> &promises,
                             const std::string &deviceId,
                             DeviceDescriptor *deviceDescriptor,
                             chip::Messaging::ExchangeManager &exchangeMgr,
                             const chip::SessionHandle &sessionHandle);

        void PopulateInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                           const std::string &deviceId,
                                           icInitialResourceValues *initialResourceValues,
                                           chip::Messaging::ExchangeManager &exchangeMgr,
                                           const chip::SessionHandle &sessionHandle);

        bool CreateResources(icDevice *device, icInitialResourceValues *initialResourceValues);

        virtual void SynchronizeDevice(std::forward_list<std::promise<bool>> &promises,
                                       const std::string &deviceId,
                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                       const chip::SessionHandle &sessionHandle);

        virtual void ReadResource(std::forward_list<std::promise<bool>> &promises,
                                  const std::string &deviceId,
                                  icDeviceResource *resource,
                                  char **value,
                                  chip::Messaging::ExchangeManager &exchangeMgr,
                                  const chip::SessionHandle &sessionHandle);

        /**
         * @brief Write a new value to a resource.
         *
         * @param promises
         * @param deviceId
         * @param resource
         * @param previousValue
         * @param newValue
         * @param exchangeMgr
         * @param sessionHandle
         * @return true if the base driver should update the resource when the device-specific driver's operations
         *         succeed
         * @return false if the base driver should not update the resource; note that this does not preclude the
         *         resource from being updated asynchronously later
         */
        virtual bool WriteResource(std::forward_list<std::promise<bool>> &promises,
                                   const std::string &deviceId,
                                   icDeviceResource *resource,
                                   const char *previousValue,
                                   const char *newValue,
                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle);

        virtual void ExecuteResource(std::forward_list<std::promise<bool>> &promises,
                                     const std::string &deviceId,
                                     icDeviceResource *resource,
                                     const char *arg,
                                     char **response,
                                     chip::Messaging::ExchangeManager &exchangeMgr,
                                     const chip::SessionHandle &sessionHandle);

        bool ProcessDeviceDescriptor(const icDevice *device, const DeviceDescriptor *dd);

    protected:
        DeviceDriver driver;

        /**
         * @brief Get a server cluster on a given endpoint
         *
         * @param deviceUuid
         * @param endpointId
         * @param clusterId
         * @return MatterCluster* The cluster or nullptr if not supported. The caller does not own this object.
         */
        MatterCluster *
        GetServerById(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId);

        /**
         * @brief Get a cluster server anywhere on the device
         * This scans the device's discovery details and returns the first available instance.
         * @see GetServerById
         */
        MatterCluster *GetAnyServerById(std::string const &deviceUuid, chip::ClusterId clusterId);

        /**
         * @brief Locate an endpoint that has a cluster server by ID
         *
         * @param deviceUuid
         * @param clusterId
         * @return std::vector<chip::EndpointId> A list of endpoint IDs that have this cluster (may be empty)
         */
        std::vector<chip::EndpointId> FindServerEndpoints(std::string const &deviceUuid, chip::ClusterId clusterId);


        /**
         * @brief Validate that a server cluster is actually present on a device's endpoint
         *
         * @param deviceUuid
         * @param endpointId Any valid endpoint ID
         * @param clusterId
         * @return
         */
        bool IsServerPresent(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId);

        /**
         * @brief Call to ensure a driver operation is considered failed, e.g.,
         *        when no promises have been made. This does nothing
         *        when promises have been stored in the Matter SDK for a pending
         *        async callback.
         *
         * @param promises
         */
        void FailOperation(std::forward_list<std::promise<bool>> &promises)
        {
            std::promise<bool> operationOK;
            operationOK.set_value(false);

            // ConnectAndExecute will wait for the first failure, or the entire
            // set to become ready with 'true' values, whichever comes first.
            // Putting 'failed' states in the very beginning ensures nothing
            // will be waited on for no reason.
            promises.push_front(std::move(operationOK));
        }

        /**
         * @brief Call this when passing a context to the Matter SDK
         *        that is not transferred to the SDK itself.
         *
         * @see FinalizeStoredContext
         * @see DisassociateStoredContext
         * @param context A driver owned object that Matter will store
         */
        void AssociateStoredContext(void *context)
        {
            std::lock_guard<std::mutex> lock(storedContextsMtx);
            storedContexts.insert(context);
        }

        /**
         * @brief Perform work accessing a previously associated context
         *        if and only if it hasn't been invalidated.
         *
         * @see DisassociateStoredContext
         * @see AbandonDeviceWork
         * @param context Private data previously given to Matter
         * @return true if the context is valid. Call DisassociateStoredContext
         *              when finished with the object.
         */
        inline void FinalizeStoredContext(void *context, std::function<void(void *context)> &&work)
        {
            std::lock_guard<std::mutex> lock(storedContextsMtx);
            auto pos = storedContexts.find(context);

            if (pos != storedContexts.end())
            {
                work(*pos);

                DisassociateStoredContextLocked(*pos);
            }
        }

        /**
         * @brief Call upon abanoning device work. This will fulfill the
         *        promise with a false (failed) value and invalidate context
         *        rendering any FinalizeStoredContext with pointers to this promise
         *        a noop.
         *
         * @param promise
         */
        void AbandonDeviceWork(std::promise<bool> &promise)
        {
            promise.set_value(false);
            DisassociateStoredContext(&promise);
        }

        /**
         * @brief Invalidate a context pointer previously associated with
         *        AssociateStoredContext
         *
         * @param context
         */
        void DisassociateStoredContext(void *context)
        {
            std::lock_guard<std::mutex> lock(storedContextsMtx);
            DisassociateStoredContextLocked(context);
        }

        /**
         * @brief Convenience method to complete device work where
         *        a promise was provided to the Matter SDK as context.
         *
         * @param context A pointer to a previously issued promise
         * @param success The device work disposition from SDK/cluster callbacks
         * @see HasStoredContext to look up if the callback context is still valid
         * @see DisassociateStoredContext to prevent access to data on callback (i.e., upon release)
         */
        void OnDeviceWorkCompleted(void *context, bool success)
        {
            FinalizeStoredContext(context, [&success](void *context) {
                auto promise = static_cast<std::promise<bool> *>(context);
                promise->set_value(success);
            });
        }

        // Asynchronous DeviceDriver interface entrypoints

        /**
         * @brief Implement this to perform device specific configuration,
         *        like to issue configuration commands, etc.
         *
         * @param promises Add a promise for each asynchronous task
         * @param deviceId
         * @param deviceDescriptor
         * @param exchangeMgr
         * @param sessionHandle
         */
        virtual void DoConfigureDevice(std::forward_list<std::promise<bool>> &promises,
                                       const std::string &deviceId,
                                       const DeviceDescriptor *deviceDescriptor,
                                       chip::Messaging::ExchangeManager &exchangeMgr,
                                       const chip::SessionHandle &sessionHandle) {};

        virtual void FetchInitialResourceValues(std::forward_list<std::promise<bool>> &promises,
                                                const std::string &deviceId,
                                                icInitialResourceValues *initialResourceValues,
                                                chip::Messaging::ExchangeManager &exchangeMgr,
                                                const chip::SessionHandle &sessionHandle);

        virtual bool RegisterResources(icDevice *device, icInitialResourceValues *initialResourceValues);

        virtual void SetTamperedEndpointResource(std::string const &deviceId, bool tampered);

        /**
         * @brief Return a list of clusters that we wish to subscribe to on this device type.
         *
         * @param deviceId
         * @return std::vector<MatterCluster *>
         */
        virtual std::vector<MatterCluster *> GetClustersToSubscribeTo(const std::string &deviceId);

        /**
         * @brief Make a cluster instance. Implementations simply create an instance of a supported cluster
         * implementation.
         *
         * @param deviceUuid
         * @return MatterCluster* A heap allocated cluster for this device/endpoint/cluster
         */
        virtual std::unique_ptr<MatterCluster>
        MakeCluster(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId) = 0;

        /**
         * @brief Perform some work on each cluster server for a device.
         *        This method must only be run in a protected stack context.
         *
         * @param deviceUuid
         * @param work
         */
        void ForEachServer(const std::string &deviceUuid, std::function<void(MatterCluster *server)> &&work)
        {
            for (const auto &kv : clusterServers)
            {
                MatterCluster *server = kv.second.get();
                if (server->GetDeviceId() == deviceUuid)
                {
                    work(server);
                }
            }
        }

    private:
        // These are for unit test fixtures.
        friend class MockMatterDeviceDriver;
        friend class MatterConfigureSubscriptionSpecsTest;

        uint8_t deviceClassVersion;

        /*
         * Increment this when changing the resource model common
         * to all device classes (e.g., adding a new device resource in this file)
         */
        static constexpr uint8_t deviceModelVersion = 3;

        std::atomic<uint32_t> commFailTimeoutSecs;

        virtual inline uint32_t GetCommFailTimeoutSecs(const char *deviceUuid) { return commFailTimeoutSecs; }

        class DriverSubscribeEventHandler : public SubscribeInteraction::EventHandler
        {
        public:
            DriverSubscribeEventHandler(MatterDeviceDriver &outer, const std::string &deviceUuid) :
                driver(outer), deviceId(deviceUuid)
            {
            }

            void OnSubscriptionEstablished(chip::SubscriptionId aSubscriptionId,
                                           std::promise<bool> &subscriptionPromise,
                                           std::shared_ptr<chip::app::ClusterStateCache> &clusterStateCache) override
            {
                driver.ForEachServer(this->deviceId, [&clusterStateCache](MatterCluster *server) {
                    server->SetClusterStateCacheRef(clusterStateCache);
                });
                driver.OnDeviceWorkCompleted(&subscriptionPromise, true);
            }

            void AbandonSubscription(std::promise<bool> &subscriptionPromise) override
            {
                driver.AbandonDeviceWork(subscriptionPromise);
            }

            void OnAttributeChanged(chip::app::ClusterStateCache *cache,
                                    const chip::app::ConcreteAttributePath &path,
                                    const std::string deviceId) override
            {
                auto server = driver.GetServerById(deviceId, path.mEndpointId, path.mClusterId);
                if (server)
                {
                    server->OnAttributeChanged(cache, path);
                }
            }

            void OnEventData(const chip::app::EventHeader &aEventHeader,
                             chip::TLV::TLVReader *apData,
                             const chip::app::StatusIB *apStatus,
                             const std::string deviceId,
                             SubscribeInteraction &outerSubscriber) override
            {
                auto eventPath = aEventHeader.mPath;
                auto server = driver.GetServerById(deviceId, eventPath.mEndpointId, eventPath.mClusterId);
                if (server)
                {
                    server->OnEventDataReceived(outerSubscriber, aEventHeader, apData, apStatus);
                }
            }

        private:
            MatterDeviceDriver &driver;
            std::string deviceId;
        };

        /* key, value pair of deviceId, collection of SubscribeInteractions */
        std::map<std::string, std::unique_ptr<SubscribeInteraction>> subscriptionMap;

        class OtaRequestorEventHandler : public OTARequestor::EventHandler
        {
        public:
            OtaRequestorEventHandler(MatterDeviceDriver &outer) : driver(outer) {};
            void CommandCompleted(void *context, bool success) override
            {
                driver.OnDeviceWorkCompleted(context, success);
            };

            void WriteRequestCompleted(void *context, bool success) override
            {
                driver.OnDeviceWorkCompleted(context, success);
            };

            void OnStateTransition(OTARequestor &source,
                                   OtaSoftwareUpdateRequestor::OTAUpdateStateEnum oldState,
                                   OtaSoftwareUpdateRequestor::OTAUpdateStateEnum newState,
                                   OtaSoftwareUpdateRequestor::OTAChangeReasonEnum reason,
                                   Nullable<uint32_t> targetSoftwareVersion,
                                   SubscribeInteraction &subscriber) override;

            void OnDownloadError(OTARequestor &source,
                                 uint32_t softwareVersion,
                                 uint64_t bytesDownloaded,
                                 Nullable<uint8_t> percentDownloaded,
                                 Nullable<int64_t> platformCode,
                                 SubscribeInteraction &subscriber) override;

            void OnVersionApplied(OTARequestor &source,
                                  uint32_t softwareVersion,
                                  uint16_t productId,
                                  SubscribeInteraction &subscriber) override;

        private:
            MatterDeviceDriver &driver;

        } otaRequestorEventHandler;

        class BasicInfoEventHandler : public barton::BasicInformation::EventHandler
        {
            void OnSoftwareVersionChanged(BasicInformation &source, uint32_t softwareVersion) override
            {
                updateResource(source.GetDeviceId().c_str(),
                               NULL,
                               COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                               Matter::VersionToString(softwareVersion).c_str(),
                               NULL);
            }

            void OnSoftwareVersionStringChanged(BasicInformation &source,
                                                const std::string &softwareVersionString) override
            {
                updateResource(source.GetDeviceId().c_str(),
                               NULL,
                               COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION_STRING,
                               softwareVersionString.c_str(),
                               NULL);
            }

            void OnSerialNumberChanged(BasicInformation &source, const std::string &serialNumber) override
            {
                scoped_icDeviceResource *current = deviceServiceGetResourceById(
                    source.GetDeviceId().c_str(), NULL, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER);
                const char *currentSerialNumber = nullptr;

                if (current != nullptr)
                {
                    currentSerialNumber = current->value;
                }

                // "Changed" doesn't necessarily mean the value changed, only that the
                // volatile cluster cache received a new value.
                if (g_strcmp0(currentSerialNumber, serialNumber.c_str()) == 0)
                {
                    return;
                }

                icLogWarn("MatterDeviceDriver",
                          "Serial number changed for device %s from %s to %s",
                          source.GetDeviceId().c_str(),
                          stringCoalesce(currentSerialNumber),
                          serialNumber.c_str());

                updateResource(source.GetDeviceId().c_str(),
                               NULL,
                               COMMON_DEVICE_RESOURCE_SERIAL_NUMBER,
                               serialNumber.c_str(),
                               NULL);
            }
        } basicInfoEventHandler;

        class generalDiagnosticsEventHandler : public barton::GeneralDiagnostics::EventHandler
        {
        public:
            generalDiagnosticsEventHandler(MatterDeviceDriver &outer) : driver(outer) {};

            void OnNetworkInterfacesChanged(GeneralDiagnostics &source,
                                            NetworkUtils::NetworkInterfaceInfo info) override
            {
                updateResource(source.GetDeviceId().c_str(),
                               NULL,
                               COMMON_DEVICE_RESOURCE_MAC_ADDRESS,
                               info.macAddress.c_str(),
                               NULL);

                updateResource(source.GetDeviceId().c_str(),
                               NULL,
                               COMMON_DEVICE_RESOURCE_NETWORK_TYPE,
                               info.networkType.c_str(),
                               NULL);
            }

            void OnHardwareFaultsChanged(
                GeneralDiagnostics &source,
                const std::vector<chip::app::Clusters::GeneralDiagnostics::HardwareFaultEnum> &currentFaults) override
            {
                driver.SetTamperedEndpointResource(
                    source.GetDeviceId(),
                    std::find(currentFaults.begin(), currentFaults.end(), HardwareFaultEnum::kTamperDetected) !=
                        currentFaults.end());
            }

        private:
            MatterDeviceDriver &driver;
        } generalDiagnosticsEventHandler;

        std::map<std::tuple<std::string, chip::EndpointId, chip::ClusterId>, std::unique_ptr<MatterCluster>>
            clusterServers;

        std::unordered_set<void *> storedContexts;
        std::mutex storedContextsMtx;

        /* TODO: drivers may want to configure this with a protected mutator */
        uint32_t fastLivenessCheckMillis = 30 * 1000;

        /**
         * @brief Called during device configuration, this kicks off the process of
         *        initiating a subscribe interaction(s) with the device.
         *
         * @param promises
         * @param deviceId
         * @param deviceDescriptor
         * @param exchangeMgr
         * @param sessionHandle
         */
        void ConfigureSubscription(std::forward_list<std::promise<bool>> &promises,
                                   const std::string &deviceId,
                                   DeviceDescriptor *deviceDescriptor,
                                   chip::Messaging::ExchangeManager &exchangeMgr,
                                   const chip::SessionHandle &sessionHandle);

        /**
         * @brief Return a list of common Matter clusters required for base functionality that we wish to subscribe to.
         *
         * @param deviceId
         * @return std::vector<MatterCluster *>
         */
        virtual std::vector<MatterCluster *> GetCommonClustersToSubscribeTo(const std::string &deviceId);

        SubscriptionIntervalSecs CalculateFinalSubscriptionIntervalSecs(const std::string &deviceId);

        /**
         * This object can be used to track the status of a ReadAttribute request and,
         * if the request is successfully fulfilled, store the data that was read.
         */
        template<typename T>
        struct AttributeReadRequestContext
        {
            AttributeReadRequestContext(MatterDeviceDriver *d,
                                        std::promise<bool> *r,
                                        T *a,
                                        const std::string &deviceUuid) :
                driver(d), readComplete(r), attributeData(a), deviceId(deviceUuid)
            {
            }
            MatterDeviceDriver *driver;
            std::promise<bool> *readComplete;
            T *attributeData;
            const std::string &deviceId;
        };

        void DisassociateStoredContextLocked(void *context) { storedContexts.erase(context); }

        /**
         * @brief Look up a cluster server by device/endpoint ID
         *
         * @param deviceUuid
         * @param endpointId Specify kInvalidEndpointId to find all endpoint IDs with the requested clusterID.
         *                   Otherwise, the specified endpoint will be searched for the requested cluster.
         * @param clusterId
         * @return std::vector<chip::EndpointId> A list of endpoint IDs that have this cluster (empty when not
         * supported)
         */
        std::vector<chip::EndpointId>
        FindServerEndpoints(std::string const &deviceUuid, chip::EndpointId endpointId, chip::ClusterId clusterId);

        /**
         * @brief Set "this" node as the default OTA provider location on a new device
         *
         * @param deviceId DeviceService uuid
         * @param deviceDescriptor
         * @param exchangeMgr
         * @param sessionHandle
         * @return true on success or when the device has no OTA requester to configure
         * @return false on failure
         */
        bool ConfigureOTARequestorCluster(std::forward_list<std::promise<bool>> &promises,
                                          const std::string &deviceId,
                                          DeviceDescriptor *deviceDescriptor,
                                          const chip::Messaging::ExchangeManager &exchangeMgr,
                                          const chip::SessionHandle &sessionHandle);

        /**
         * @brief Record a device reported it failed to update
         *
         * @param nodeId
         */
        void
        DeviceFirmwareUpdateFailed(OTARequestor &source, uint32_t softwareVersion, SubscribeInteraction &subscriber);

        void ProcessDeviceDescriptorMetadata(const icDevice *device, const icStringHashMap *metadata);

        // The following methods are to be called via work functions passed to
        // ConnectAndExecute. To promise asynchronous work, add to the promises list.

        bool SendRemoveFabric(std::forward_list<std::promise<bool>> &promises,
                              const std::string &deviceId,
                              const chip::FabricIndex fabricIndex,
                              chip::Messaging::ExchangeManager &exchangeMgr,
                              const chip::SessionHandle &sessionHandle);

        void ReadCurrentFabricIndex(std::forward_list<std::promise<bool>> &promises,
                                    const std::string &deviceId,
                                    chip::FabricIndex &fabricIndex,
                                    chip::Messaging::ExchangeManager &exchangeMgr,
                                    const chip::SessionHandle &sessionHandle);
    };
} // namespace barton
