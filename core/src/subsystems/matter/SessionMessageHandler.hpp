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
            auto deviceId = zilker::Subsystem::Matter::NodeIdToUuid(session->GetPeer().GetNodeId());

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