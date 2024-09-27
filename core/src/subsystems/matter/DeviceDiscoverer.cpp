//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2023 Comcast Corporation
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

//
// Created by tlea on 11/4/22.
//

#define LOG_TAG     "MatterDiscoverer"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>
}

#include "DeviceDiscoverer.h"
#include "Matter.h"
#include "MatterCommon.h"
#include "ReadPrepareParamsBuilder.hpp"
#include "controller-clusters/zap-generated/CHIPClusters.h"
#include "platform/PlatformManager.h"
#include "protocols/interaction_model/StatusCode.h"
#include <assert.h>

namespace zilker
{
    struct EndpointDiscoveryContext
    {
        // must be first since this could end up in OnAttrReadFailureResponse and cast directly to DeviceDiscoverer
        DeviceDiscoverer *discoverer;
        chip::Messaging::ExchangeManager *exchangeMgr;
        chip::EndpointId endpointId;
        chip::OperationalDeviceProxy *device;
        std::unique_ptr<DescriptorClusterData> data;
    };

    static void OnAttrReadFailureResponse(void *context, CHIP_ERROR error);
    static void
    OnDTLReadResponse(void *context,
                      const chip::app::DataModel::DecodableList<
                          chip::app::Clusters::Descriptor::Structs::DeviceTypeStruct::DecodableType> &value);
    static void OnSLReadResponse(void *context, const app::DataModel::DecodableList<ClusterId> &responseList);
    static void OnCLReadResponse(void *context, const app::DataModel::DecodableList<ClusterId> &responseList);
    static void OnPLReadResponse(void *context, const app::DataModel::DecodableList<EndpointId> &responseList);
    static void AddEndpointDataToResultIfComplete(EndpointDiscoveryContext *ctx);

    std::future<bool> DeviceDiscoverer::Start()
    {
        details = std::make_shared<DiscoveredDeviceDetails>();

        chip::DeviceLayer::PlatformMgr().ScheduleWork(DiscoverWorkFuncCb, reinterpret_cast<intptr_t>(this));

        return promise.get_future();
    }

    // data has been added to our result set.  If we are done, update our status
    void DeviceDiscoverer::ResultsUpdated()
    {
        if (details)
        {
            // Since 'macAddress', an optional attribute is already being added, managing
            // networkType optional attribute is not required.
            if (details->vendorName && details->productName && details->hardwareVersion && details->softwareVersion &&
                !details->softwareVersionString.empty() && details->serialNumber.HasValue() &&
                details->macAddress.HasValue())
            {
                primaryAttributesRead = true;

                if (!discoveringEndpoints.empty() &&
                    discoveringEndpoints.size() == details->endpointDescriptorData.size())
                {
                    SetCompleted(true);
                }
            }
        }
    }

    /** This is the beginning of the async chain that has a happy path ending at completion of reading our basic
     *  attributes.  Failures along the async path terminate the chain.  Successes trigger the next async step
     */
    void DeviceDiscoverer::DiscoverWorkFuncCb(intptr_t arg)
    {
        auto *instance = reinterpret_cast<DeviceDiscoverer *>(arg);

        // Step 1:  Connect to device
        Matter &matter = Matter::GetInstance();
        if (matter.GetCommissioner().GetConnectedDevice(instance->nodeId,
                                                        &instance->onDeviceConnectedCallback,
                                                        &instance->onDeviceConnectionFailureCallback) != CHIP_NO_ERROR)
        {
            icError("Failed to start device connection");
            instance->SetCompleted(false);
        }
    }

    void DeviceDiscoverer::OnDone(chip::app::ReadClient *apReadClient)
    {
        icDebug();
    }

    void DeviceDiscoverer::OnAttributeData(const chip::app::ConcreteDataAttributePath &path,
                                           chip::TLV::TLVReader *data,
                                           const chip::app::StatusIB &status)
    {
        icDebug("Cluster %x, attribute %x", path.mClusterId, path.mAttributeId);

        CHIP_ERROR error = status.ToChipError();
        if (CHIP_NO_ERROR != error)
        {
            // special handling of the SerialNumber attribute which is optional (dont fail if its unsupported)
            if (error.IsIMStatus() && chip::app::StatusIB(error).mStatus ==
                chip::Protocols::InteractionModel::Status::UnsupportedAttribute &&
                path.mClusterId == app::Clusters::BasicInformation::Id &&
                path.mAttributeId == app::Clusters::BasicInformation::Attributes::SerialNumber::Id)
            {
                GetDetails()->serialNumber.Emplace("");
                ResultsUpdated();
                return;
            }

            icError("Response Failure for cluster %x, attribute %x: %s",
                    path.mClusterId,
                    path.mAttributeId,
                    chip::ErrorStr(error));
            SetCompleted(false);
            return;
        }

        if (data == nullptr)
        {
            icError("Response Failure No data for cluster %x, attribute %x", path.mClusterId, path.mAttributeId);
            SetCompleted(false);
            return;
        }

        switch (path.mClusterId)
        {
            case app::Clusters::BasicInformation::Id:
            {
                switch (path.mAttributeId)
                {
                    case app::Clusters::BasicInformation::Attributes::VendorName::Id:
                    {
                        using TypeInfo = app::Clusters::BasicInformation::Attributes::VendorName::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}
                        {
                            GetDetails()->vendorName = new std::string(v.begin(), v.size());
                            ResultsUpdated();
                        }
                        break;
                    }

                    case app::Clusters::BasicInformation::Attributes::ProductName::Id:
                    {
                        using TypeInfo = app::Clusters::BasicInformation::Attributes::ProductName::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}
                        {
                            GetDetails()->productName = new std::string(v.begin(), v.size());
                            ResultsUpdated();
                        }
                        break;
                    }

                    case app::Clusters::BasicInformation::Attributes::HardwareVersion::Id:
                    {
                        using TypeInfo = app::Clusters::BasicInformation::Attributes::HardwareVersion::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}
                        {
                            GetDetails()->hardwareVersion = new std::string(std::to_string(v));
                            ResultsUpdated();
                        }
                        break;
                    }

                    case app::Clusters::BasicInformation::Attributes::SoftwareVersion::Id:
                    {
                        using TypeInfo = app::Clusters::BasicInformation::Attributes::SoftwareVersion::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}
                        {
                            GetDetails()->softwareVersion = new std::string(Matter::VersionToString(v));
                            ResultsUpdated();
                        }
                        break;
                    }

                    case app::Clusters::BasicInformation::Attributes::SoftwareVersionString::Id:
                    {
                        using TypeInfo = app::Clusters::BasicInformation::Attributes::SoftwareVersionString::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}
                        {
                            GetDetails()->softwareVersionString = std::string(v.begin(), v.size());
                            ResultsUpdated();
                        }
                        break;
                    }

                    case app::Clusters::BasicInformation::Attributes::SerialNumber::Id:
                    {
                        using TypeInfo = app::Clusters::BasicInformation::Attributes::SerialNumber::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}
                        {
                            GetDetails()->serialNumber.Emplace(std::string(v.begin(), v.size()));
                            ResultsUpdated();
                        }
                        break;
                    }
                }
                break;
            }
            case app::Clusters::GeneralDiagnostics::Id:
            {
                switch (path.mAttributeId)
                {
                    case app::Clusters::GeneralDiagnostics::Attributes::NetworkInterfaces::Id:
                    {
                        using TypeInfo = app::Clusters::GeneralDiagnostics::Attributes::NetworkInterfaces::TypeInfo;
                        TypeInfo::DecodableType v;
                        error = app::DataModel::Decode(*data, v);
                        if (error != CHIP_NO_ERROR) {}

                        auto session = device->GetSecureSession().Value()->AsSecureSession();
                        char nodeIpv6Addr[INET6_ADDRSTRLEN] = {};
                        session->GetPeerAddress().GetIPAddress().ToString(nodeIpv6Addr);
                        auto interfaceInfo = NetworkUtils::ExtractOperationalInterfaceInfo(v,
                                                                                           nodeIpv6Addr);
                        if (interfaceInfo.HasValue())
                        {
                            GetDetails()->macAddress.Emplace(interfaceInfo.Value().macAddress);
                            GetDetails()->networkType.Emplace(interfaceInfo.Value().networkType);
                        }else
                        {
                            // Failed to find the hw address. Don't fail discovery if unsupported
                            GetDetails()->macAddress.Emplace("");
                            GetDetails()->networkType.Emplace("");
                        }

                        ResultsUpdated();
                        break;
                    }
                    // other attributes of diagnostic cluster
                }
            }
        // other clusters...
        }

        if (error != CHIP_NO_ERROR)
        {
            icError("Failed to decode attribute: %s", chip::ErrorStr(error));
        }
        else if (error == CHIP_NO_ERROR &&
                 primaryAttributesRead &&
                 discoveringEndpoints.find(0) == discoveringEndpoints.end())
        {
            //we are done reading the basic cluster and have not yet started endpoint discovery
            error = DiscoverEndpoint(*device->GetExchangeManager(), device, 0);
            if (error != CHIP_NO_ERROR)
            {
                icError("Failed to start endpoint discovery: %s", chip::ErrorStr(error));
            }
        }

        if (error != CHIP_NO_ERROR)
        {
            SetCompleted(false);
        }
    }

    void DeviceDiscoverer::OnDeviceConnectedFn(void *context,
                                               chip::Messaging::ExchangeManager &exchangeMgr,
                                               const chip::SessionHandle &sessionHandle)
    {
        icDebug();

        auto *self = reinterpret_cast<DeviceDiscoverer *>(context);
        self->device = new chip::OperationalDeviceProxy(&exchangeMgr, sessionHandle);

        constexpr EndpointId endpoint = chip::kRootEndpointId;
        /*
         * Read what we consider 'primary' attributes from the device.  This includes manufacturer, model,
         * hardware version, firmware version, etc.  All devices are required to have these attributes per
         * Matter spec (R1.0 11.1.6.3):
            app::Clusters::BasicInformation::Attributes::VendorName
            app::Clusters::BasicInformation::Attributes::ProductName
            app::Clusters::BasicInformation::Attributes::HardwareVersion
            app::Clusters::BasicInformation::Attributes::SoftwareVersion
            app::Clusters::BasicInformation::Attributes::SoftwareVersionString

           This attribute is optional:
            app::Clusters::BasicInformation::Attributes::SerialNumber

           Operational mac address from GeneralDiagnostics
           app::Clusters::GeneralDiagnostics::Attributes::NetworkInterfaces
         */

        ReadPrepareParamsBuilder builder(endpoint, app::Clusters::BasicInformation::Id);

        builder.AddAttribute(chip::app::Clusters::BasicInformation::Attributes::VendorName::Id)
            .AddAttribute(chip::app::Clusters::BasicInformation::Attributes::ProductName::Id)
            .AddAttribute(chip::app::Clusters::BasicInformation::Attributes::HardwareVersion::Id)
            .AddAttribute(chip::app::Clusters::BasicInformation::Attributes::SoftwareVersion::Id)
            .AddAttribute(chip::app::Clusters::BasicInformation::Attributes::SoftwareVersionString::Id)
            .AddAttribute(chip::app::Clusters::BasicInformation::Attributes::SerialNumber::Id)
            .AddAttribute(chip::app::Clusters::GeneralDiagnostics::Id,
                          chip::app::Clusters::GeneralDiagnostics::Attributes::NetworkInterfaces::Id);

        auto params = builder.Build(sessionHandle);

        assert(params != nullptr);

        self->readClient = std::make_unique<chip::app::ReadClient>(chip::app::InteractionModelEngine::GetInstance(),
                                                                   &exchangeMgr,
                                                                   self->bufferedReadCallback,
                                                                   chip::app::ReadClient::InteractionType::Read);

        CHIP_ERROR err = self->readClient->SendRequest(params->Get());

        if (CHIP_NO_ERROR != err)
        {
            icError("Failed to send read request: %s", chip::ErrorStr(err));
            self->SetCompleted(false);
        }
    }

    /**
     * Note: this runs under the stack thread
     */
    CHIP_ERROR DeviceDiscoverer::DiscoverEndpoint(chip::Messaging::ExchangeManager &exchangeMgr,
                                                  chip::OperationalDeviceProxy *device,
                                                  chip::EndpointId endpointId)
    {
        CHIP_ERROR error = CHIP_NO_ERROR;

        icDebug("endpoint %" PRIu16, endpointId);

        if (discoveringEndpoints.find(endpointId) != discoveringEndpoints.end())
        {
            icInfo("Already processed endpoint %" PRIu16, endpointId);
            return CHIP_NO_ERROR;
        }

        {
            using namespace chip::app::Clusters::Descriptor;

            chip::Controller::DescriptorCluster cluster(exchangeMgr, device->GetSecureSession().Value(), endpointId);

            // TODO use smart pointer instead since we are hading off...
            auto context = new EndpointDiscoveryContext;
            context->exchangeMgr = &exchangeMgr;
            context->discoverer = this;
            context->endpointId = endpointId;
            context->device = device;
            context->data = std::make_unique<DescriptorClusterData>(endpointId);

            error = cluster.ReadAttribute<app::Clusters::Descriptor::Attributes::DeviceTypeList::TypeInfo>(
                context, OnDTLReadResponse, OnAttrReadFailureResponse);
            if (error != CHIP_NO_ERROR)
            {
                icError("VendorName ReadAttribute failed with error: %s", error.AsString());
            }

            if (error == CHIP_NO_ERROR)
            {
                error = cluster.ReadAttribute<app::Clusters::Descriptor::Attributes::ServerList::TypeInfo>(
                    context, OnSLReadResponse, OnAttrReadFailureResponse);
                if (error != CHIP_NO_ERROR)
                {
                    icError("VendorName ReadAttribute failed with error: %s", error.AsString());
                }
            }

            if (error == CHIP_NO_ERROR)
            {
                error = cluster.ReadAttribute<app::Clusters::Descriptor::Attributes::ClientList::TypeInfo>(
                    context, OnCLReadResponse, OnAttrReadFailureResponse);
                if (error != CHIP_NO_ERROR)
                {
                    icError("VendorName ReadAttribute failed with error: %s", error.AsString());
                }
            }

            if (error == CHIP_NO_ERROR)
            {
                error = cluster.ReadAttribute<app::Clusters::Descriptor::Attributes::PartsList::TypeInfo>(
                    context, OnPLReadResponse, OnAttrReadFailureResponse);
                if (error != CHIP_NO_ERROR)
                {
                    icError("VendorName ReadAttribute failed with error: %s", error.AsString());
                }
            }

            if (error == CHIP_NO_ERROR)
            {
                discoveringEndpoints.emplace(endpointId);
            }
            else
            {
                // clean up.  if this crashes, you were warned to use smart pointers!
                delete context;
            }
        }

        return error;
    }

    void
    DeviceDiscoverer::OnDeviceConnectionFailureFn(void *context, const chip::ScopedNodeId &peerId, CHIP_ERROR error)
    {
        icDebug();

        auto *self = reinterpret_cast<DeviceDiscoverer *>(context);
        self->SetCompleted(false);
    }


    /**** Begin Read Callback Handlers *****/

    static void OnAttrReadFailureResponse(void *context, CHIP_ERROR error)
    {
        icError("Failed to read basic attribute for the device. error %" CHIP_ERROR_FORMAT, error.Format());
        auto *deviceDiscoverer = reinterpret_cast<DeviceDiscoverer *>(context);
        deviceDiscoverer->SetCompleted(false);
    }

    static void OnDTLReadResponse(void *context,
                                  const chip::app::DataModel::DecodableList<
                                      chip::app::Clusters::Descriptor::Structs::DeviceTypeStruct::DecodableType> &value)
    {
        auto *endpointDiscoveryContext = static_cast<EndpointDiscoveryContext *>(context);
        auto *deviceDiscoverer = endpointDiscoveryContext->discoverer;
        chip::EndpointId endpointId = endpointDiscoveryContext->endpointId;

        endpointDiscoveryContext->data->deviceTypes = new std::vector<chip::DeviceTypeId>();
        auto iter = value.begin();
        while (iter.Next())
        {
            endpointDiscoveryContext->data->deviceTypes->emplace_back(iter.GetValue().deviceType);
        }

        AddEndpointDataToResultIfComplete(endpointDiscoveryContext);
    }

    static void OnSLReadResponse(void *context, const app::DataModel::DecodableList<ClusterId> &responseList)
    {
        auto *endpointDiscoveryContext = static_cast<EndpointDiscoveryContext *>(context);
        auto *deviceDiscoverer = endpointDiscoveryContext->discoverer;
        chip::EndpointId endpointId = endpointDiscoveryContext->endpointId;

        endpointDiscoveryContext->data->serverList = new std::vector<chip::ClusterId>();
        auto iter = responseList.begin();
        while (iter.Next())
        {
            endpointDiscoveryContext->data->serverList->emplace_back(iter.GetValue());
        }

        AddEndpointDataToResultIfComplete(endpointDiscoveryContext);
    }

    static void OnCLReadResponse(void *context, const app::DataModel::DecodableList<ClusterId> &responseList)
    {
        auto *endpointDiscoveryContext = static_cast<EndpointDiscoveryContext *>(context);
        auto *deviceDiscoverer = endpointDiscoveryContext->discoverer;
        chip::EndpointId endpointId = endpointDiscoveryContext->endpointId;

        endpointDiscoveryContext->data->clientList = new std::vector<chip::ClusterId>();
        auto iter = responseList.begin();
        while (iter.Next())
        {
            endpointDiscoveryContext->data->clientList->emplace_back(iter.GetValue());
        }

        AddEndpointDataToResultIfComplete(endpointDiscoveryContext);
    }

    static void OnPLReadResponse(void *context, const app::DataModel::DecodableList<EndpointId> &responseList)
    {
        auto *endpointDiscoveryContext = static_cast<EndpointDiscoveryContext *>(context);
        auto *deviceDiscoverer = endpointDiscoveryContext->discoverer;
        chip::EndpointId endpointId = endpointDiscoveryContext->endpointId;

        endpointDiscoveryContext->data->partsList = new std::vector<chip::EndpointId>();
        auto iter = responseList.begin();
        while (iter.Next())
        {
            endpointDiscoveryContext->data->partsList->emplace_back(iter.GetValue());

            // this 'recursively' starts the next endpoint discovery
            deviceDiscoverer->DiscoverEndpoint(
                *endpointDiscoveryContext->exchangeMgr, endpointDiscoveryContext->device, iter.GetValue());
        }

        AddEndpointDataToResultIfComplete(endpointDiscoveryContext);
    }

    static void AddEndpointDataToResultIfComplete(EndpointDiscoveryContext *ctx)
    {
        if (ctx->data->deviceTypes && ctx->data->serverList && ctx->data->clientList && ctx->data->partsList)
        {
            ctx->discoverer->GetDetails()->endpointDescriptorData[ctx->endpointId] = std::move(ctx->data);
            ctx->discoverer->ResultsUpdated();
        }
    }

} // namespace zilker
