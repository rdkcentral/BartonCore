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

#include "app/InteractionModelEngine.h"
#include "subsystems/matter/MatterCommon.h"
#include <app/server/Server.h>
#include <queue>
#include <thread>
#include <transport/SessionMessageDelegate.h>
#include <unordered_set>

extern "C" {
#include "deviceCommunicationWatchdog.h"
}

using namespace chip;

/**
 * @brief A simple class that passes all messages to the default ExchangeManager
 *        and pets the deviceService watchdog upon receiving a message from an
 *        authenticated session originating from an operational peer.
 * @note All device 'pet' operations are queued and run on a separate thread.
 *       The calling context _is_ the Matter main loop, and can't risk any
 *       slow or reentrant operations on the main loop stack.
 */

class SessionMessageHandler : public chip::SessionMessageDelegate
{
public:
    SessionMessageHandler() = default;

    virtual ~SessionMessageHandler()
    {
        if (queue != nullptr)
        {
            g_async_queue_push(queue, const_cast<char *>(stop));
            petThread.join();
            g_async_queue_unref(queue);
        }
    }

private:
    std::thread petThread;
    GAsyncQueue *queue;
    const char *stop = "";

    void Petter()
    {
        char *deviceId = (char *) g_async_queue_pop(queue);

        while (deviceId != stop)
        {
            deviceCommunicationWatchdogPetDevice(deviceId);
            free(deviceId);

            deviceId = (char *) g_async_queue_pop(queue);
        }
    }

    /* Called on the Matter device event loop */
    void OnMessageReceived(const PacketHeader &packetHeader,
                           const PayloadHeader &payloadHeader,
                           const SessionHandle &session,
                           DuplicateMessage isDuplicate,
                           System::PacketBufferHandle &&msgBuf) override
    {
        SessionMessageDelegate *exchangeMgr =
            static_cast<SessionMessageDelegate *>(app::InteractionModelEngine::GetInstance()->GetExchangeManager());

        if (session->GetPeer().IsOperational() && !session->IsUnauthenticatedSession())
        {
            auto deviceId = barton::Subsystem::Matter::NodeIdToUuid(session->GetPeer().GetNodeId());

            if (queue == nullptr)
            {
                queue = g_async_queue_new_full(free);
                petThread = std::thread(&SessionMessageHandler::Petter, this);
            }

            g_async_queue_push(queue, strdup(deviceId.c_str()));
        }

        exchangeMgr->OnMessageReceived(packetHeader, payloadHeader, session, isDuplicate, std::move(msgBuf));
    }
};
