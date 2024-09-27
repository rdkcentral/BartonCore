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
// any other form of intellectual property whatsoever.
//
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 11/7/22.
//

#include "matter/subscriptions/SubscribeInteraction.h"
#define LOG_TAG     "MatterCluster"
#define logFmt(fmt) "(%s): " fmt, __func__

extern "C" {
#include <icLog/logging.h>
}

#include "MatterCluster.h"
#include "app/InteractionModelEngine.h"

namespace zilker
{
    bool MatterCluster::SendCommand(chip::app::CommandSender *commandSender,
                                    const chip::SessionHandle &sessionHandle,
                                    void *context)
    {
        mtx.lock();
        commandContext = context;
        mtx.unlock();

        CHIP_ERROR error = commandSender->SendCommandRequest(sessionHandle);

        if (error == CHIP_NO_ERROR)
        {
            return true; // we are awaiting async now
        }
        else
        {
            icError("Failed to send command");

            mtx.lock();
            commandContext = nullptr;
            mtx.unlock();

            // no async will occur
            delete commandSender;
            return false;
        }
    }

    bool MatterCluster::SendWriteRequest(chip::app::WriteClient *writeClient,
                                         const chip::SessionHandle &sessionHandle,
                                         void *context)
    {
        mtx.lock();
        writeRequestContext = context;
        mtx.unlock();

        // We're using the default MRP timeouts here, but could pass one ourselves if we want to
        CHIP_ERROR error = writeClient->SendWriteRequest(sessionHandle);

        if (error == CHIP_NO_ERROR)
        {
            return true; // we are awaiting async now
        }
        else
        {
            icError("Failed to send write request");

            mtx.lock();
            writeRequestContext = nullptr;
            mtx.unlock();

            // no async will occur
            delete writeClient;
            return false;
        }
    }

    void MatterCluster::OnResponse(chip::app::CommandSender *apCommandSender,
                                   const chip::app::ConcreteCommandPath &aPath,
                                   const chip::app::StatusIB &aStatusIB,
                                   chip::TLV::TLVReader *apData)
    {
        icDebug();

        mtx.lock();
        void *localCtx = commandContext;
        commandContext = nullptr;
        mtx.unlock();

        eventHandler->CommandCompleted(localCtx, true);
    }

    void MatterCluster::OnError(const chip::app::CommandSender *apCommandSender, CHIP_ERROR aError)
    {
        icDebug();

        mtx.lock();
        void *localCtx = commandContext;
        commandContext = nullptr;
        mtx.unlock();

        eventHandler->CommandCompleted(localCtx, false);
    }

    void MatterCluster::OnResponse(const chip::app::WriteClient *apWriteClient,
                                   const chip::app::ConcreteDataAttributePath &path,
                                   chip::app::StatusIB status)
    {
        icDebug();

        mtx.lock();
        void *localCtx = writeRequestContext;
        writeRequestContext = nullptr;
        mtx.unlock();

        eventHandler->WriteRequestCompleted(localCtx, true);
    }

    void MatterCluster::OnError(const chip::app::WriteClient *apCommandSender, CHIP_ERROR aError)
    {
        icDebug();

        mtx.lock();
        void *localCtx = writeRequestContext;
        writeRequestContext = nullptr;
        mtx.unlock();

        eventHandler->WriteRequestCompleted(localCtx, false);
    }

    void MatterCluster::SetClusterStateCacheRef(std::shared_ptr<chip::app::ClusterStateCache> &clusterStateCache)
    {
        clusterStateCacheRef = clusterStateCache;
    }
} // namespace zilker
