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
 * Created by Christian Leithner on 2/8/24
 */

#include "dbus/common/types.hpp"
#include "dbus/dbus.h"
#include "otbr/dbus/client/thread_api_dbus.hpp"
#include "otbr/dbus/common/error.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <vector>

extern "C" {
#include "glib.h"
#include "icLog/logging.h"
}

#include "OpenThreadClient.hpp"
#include <stdbool.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmt(fmt) "%s: " fmt, __func__
#define LOG_TAG     "openThreadClient"

using namespace otbr::DBus;

namespace
{
    // Note: I've seen approx 7 seconds on a standard Ubuntu desktop environment for a new network, about 18 seconds for
    // creating and replacing an existing network. This should just "go away" if we implement a proper main loop.
    constexpr int ATTACH_WAIT_SECONDS = 25;
} // namespace

namespace barton
{

    std::vector<uint8_t> OpenThreadClient::CreateNetwork(const std::string &networkName, uint8_t desiredChannel)
    {
        icDebug();

        std::vector<uint8_t> retVal;

        g_return_val_if_fail(ready, retVal);
        g_return_val_if_fail(networkName.size() <= 16, retVal);

        uint32_t channel = UINT32_MAX;

        if (desiredChannel >= 11 && desiredChannel <= 26)
        {
            channel = 1 << desiredChannel;
        }

        ClientError threadApiCallError = ClientError::ERROR_NONE;
        ClientError threadApiAttachError = ClientError::ERROR_NONE;
        std::atomic_bool threadApiAttachCallbackCalled(false);


        threadApiCallError = threadApiBus->Attach(
            networkName,
            UINT16_MAX,
            UINT64_MAX,
            {},
            {},
            channel,
            [&threadApiAttachError, &threadApiAttachCallbackCalled, &networkName](ClientError error) {
                if (error == ClientError::ERROR_NONE)
                {
                    icDebug("Successfully created network %s", networkName.c_str());
                }
                else
                {
                    icWarn("Failed to create network. Error = %d", (int) error);
                }

                threadApiAttachError = error;
                threadApiAttachCallbackCalled = true;
            });

        if (threadApiCallError == ClientError::ERROR_NONE)
        {
            using namespace std::chrono;

            auto totalTime = seconds(ATTACH_WAIT_SECONDS);
            milliseconds timer = duration_cast<milliseconds>(totalTime);
            auto current = steady_clock::now();

            // Block on a reply until timeout.
            // FIXME: This is jank, we really want some kinda main loop for this. Async responses and any property
            // change signals will pile up until we do some kind of processing like below. Synchronous calls will not
            // process the queue except for the specific reply they want. Note: Consequence of ThreadApiBus using async
            // call (dbus_connection_send_with_reply_and_block). Most operations are synchronous (backed by
            // dbus_connection_send_with_reply_and_block) and don't need this.
            while (timer.count() > 0 && dbus_connection_read_write_dispatch(dbusConnection.get(), timer.count()) &&
                   !threadApiAttachCallbackCalled)
            {
                // Processed an incoming message
                auto next = steady_clock::now();
                timer = timer - duration_cast<milliseconds>(next - current);
                current = next;
            }

            if (threadApiAttachCallbackCalled && threadApiAttachError == ClientError::ERROR_NONE)
            {
                threadApiCallError = threadApiBus->GetActiveDatasetTlvs(retVal);
                if (threadApiCallError != ClientError::ERROR_NONE)
                {
                    icError("Failed to get active dataset tlvs. Error = %d", (int) threadApiCallError);
                }

                // Silabs OTBR isn't functional simply by adding the active dataset. It also needs a soft reset
                // so it brings up its thread / network stack.
                threadApiCallError = threadApiBus->Reset();
                if (threadApiCallError != ClientError::ERROR_NONE)
                {
                    icError("Failed to reset OTBR. Error = %d", (int) threadApiCallError);
                }
            }
            else if (timer.count() <= 0)
            {
                icError("Timed out waiting for create network response.");
            }
            else
            {
                icError("Network create operation failed. Error=%d", (int) threadApiAttachError);
            }
        }
        else
        {
            icError("Failed to create network. Error = %d", (int) threadApiCallError);
        }

        return retVal;
    }

    bool OpenThreadClient::RestoreNetwork(const std::vector<uint8_t> &networkDataTlvs)
    {
        icDebug();

        g_return_val_if_fail(ready, false);
        g_return_val_if_fail(!networkDataTlvs.empty(), false);

        bool retVal = false;

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->SetActiveDatasetTlvs(networkDataTlvs)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully restored thread network");
            // Silabs OTBR isn't functional simply by adding the active dataset. It also needs a soft reset
            // so it brings up its thread / network stack.
            if ((error = threadApiBus->Reset()) == ClientError::ERROR_NONE)
            {
                icDebug("Successfully reset OTBR");
                retVal = true;
            }
            else
            {
                icError("Failed to reset OTBR. Error = %d", (int) error);
            }
        }
        else
        {
            icError("Failed to restore thread network. Error = %d", (int) error);
        }

        return retVal;
    }

    std::vector<uint8_t> OpenThreadClient::GetActiveDatasetTlvs()
    {
        icDebug();

        std::vector<uint8_t> retVal;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->GetActiveDatasetTlvs(retVal)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully fetched active network dataset tlvs");
        }
        else
        {
            icError("Failed to get active network dataset tlvs. Error = %d", (int) error);
        }

        return retVal;
    }

    uint16_t OpenThreadClient::GetChannel()
    {
        icDebug();

        uint16_t retVal = UINT16_MAX;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->GetChannel(retVal)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully fetched channel: %" PRIu16, retVal);
        }
        else
        {
            icError("Failed to get channel. Error = %d", (int) error);
        }

        return retVal;
    }

    uint16_t OpenThreadClient::GetPanId()
    {
        icDebug();

        uint16_t retVal = UINT16_MAX;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->GetPanId(retVal)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully fetched pan id: %" PRIx16, retVal);
        }
        else
        {
            icError("Failed to get pan id. Error = %d", (int) error);
        }

        return retVal;
    }

    uint64_t OpenThreadClient::GetExtPanId()
    {
        icDebug();

        uint64_t retVal = UINT64_MAX;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->GetExtPanId(retVal)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully fetched extended pan id: %" PRIx64, retVal);
        }
        else
        {
            icError("Failed to get extended pan id. Error = %d", (int) error);
        }

        return retVal;
    }

    std::vector<uint8_t> OpenThreadClient::GetNetworkKey()
    {
        icDebug();

        std::vector<uint8_t> retVal;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->GetNetworkKey(retVal)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully fetched network key");
        }
        else
        {
            icError("Failed to get network key. Error = %d", (int) error);
        }

        return retVal;
    }

    std::string OpenThreadClient::GetNetworkName()
    {
        icDebug();

        std::string retVal;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->GetNetworkName(retVal)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully fetched network name: %s", retVal.c_str());
        }
        else
        {
            icError("Failed to get network name. Error = %d", (int) error);
        }

        return retVal;
    }

    OpenThreadClient::DeviceRole OpenThreadClient::GetDeviceRole()
    {
        icDebug();

        DeviceRole retVal = DEVICE_ROLE_UNKNOWN;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        otbr::DBus::DeviceRole otbrDeviceRole;
        if ((error = threadApiBus->GetDeviceRole(otbrDeviceRole)) == ClientError::ERROR_NONE)
        {
            retVal = TranslateOTBRDeviceRole(otbrDeviceRole);
            icDebug("Successfully fetched device role: %d", (int) retVal);
        }
        else
        {
            icError("Failed to get device role. Error = %d", (int) error);
        }

        return retVal;
    }

    bool OpenThreadClient::IsNat64Enabled()
    {
        icDebug();

        bool retVal = false;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        Nat64ComponentState state;
        if ((error = threadApiBus->GetNat64State(state)) == ClientError::ERROR_NONE)
        {
            retVal = (state.mTranslatorState.compare("active") == 0);
            icDebug("Successfully fetched NAT64 enabled: %s", retVal ? "true" : "false");
        }
        else
        {
            icError("Failed to get NAT64 enabled. Error = %d", (int) error);
        }

        return retVal;
    }

    bool OpenThreadClient::SetNat64Enabled(bool enable)
    {
        icDebug();

        bool retVal = false;

        g_return_val_if_fail(ready, retVal);

        ClientError error = ClientError::ERROR_NONE;
        if ((error = threadApiBus->SetNat64Enabled(enable)) == ClientError::ERROR_NONE)
        {
            icDebug("Successfully set NAT64 enabled: %s", enable ? "true" : "false");
            retVal = true;
        }
        else
        {
            icError("Failed to set NAT64 enabled. Error = %d", (int) error);
        }

        return retVal;
    }

    bool barton::OpenThreadClient::ActivateEphemeralKeyMode(std::string &ephemeralKey)
    {
        uint32_t lifetime = 0; // 0 means use the default lifetime
        DBusMessage *message;
        DBusMessage *reply;
        DBusError error;
        bool result = false;

        icDebug();

        g_return_val_if_fail(ready, false);

        // NOTE: the current dbus client api does not have a definition for this method, so we have
        //  to call it manually.  We should ditch this once the client api is updated.

        dbus_error_init(&error);

        message = dbus_message_new_method_call((OTBR_DBUS_SERVER_PREFIX + threadApiBus->GetInterfaceName()).c_str(),
                                               (OTBR_DBUS_OBJECT_PREFIX + threadApiBus->GetInterfaceName()).c_str(),
                                               OTBR_DBUS_THREAD_INTERFACE,
                                               "ActivateEphemeralKeyMode");

        if (!message)
        {
            icError("Failed to create DBus message");
            return false;
        }

        dbus_message_append_args(message, DBUS_TYPE_UINT32, &lifetime, DBUS_TYPE_INVALID);

        reply =
            dbus_connection_send_with_reply_and_block(dbusConnection.get(), message, DBUS_TIMEOUT_USE_DEFAULT, &error);

        dbus_message_unref(message);

        if (dbus_error_is_set(&error))
        {
            icError("DBus error: %s", error.message);
            dbus_error_free(&error);
            return false;
        }

        if (!reply)
        {
            icError("No reply received");
            return false;
        }

        const char *epskc;
        if (!dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &epskc, DBUS_TYPE_INVALID))
        {
            icError("Failed to get reply arguments: %s", error.message);
            dbus_error_free(&error);
        }
        else
        {
            ephemeralKey = epskc;
            result = true;
        }

        dbus_message_unref(reply);

        return result;
    }

    void OpenThreadClient::Connect()
    {
        icDebug();

        std::unique_ptr<DBusError, std::function<void(DBusError *)>> dbusError(new DBusError(), [](DBusError *error) {
            dbus_error_free(error);
            delete error;
        });
        dbus_error_init(dbusError.get());

        // Force a new connection to the system bus socket, even if one exists.
        // FIXME: Generally this is not recommended but at this time it may be the case that matter subsystem also
        // manages a dbusconnection to the system bus socket and I don't know what kind of issues it would cause if we
        // each had refs to the dbusconnection but different methods of processing data. Until that's investigated, this
        // at least gets us our own sandbox to play in.
        dbusConnection = std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>>(
            dbus_bus_get_private(DBUS_BUS_SYSTEM, dbusError.get()), ReleaseDBusConnection);

        if (dbus_error_is_set(dbusError.get()))
        {
            icError("DBus error creating connection: %s", dbusError->message);
            return;
        }

        threadApiBus = std::unique_ptr<ThreadApiDBus>(new ThreadApiDBus(dbusConnection.get()));

        if (threadApiBus)
        {
            ready = true;
        }
}

    void OpenThreadClient::ReleaseDBusConnection(DBusConnection *connection)
    {
        if (connection)
        {
            dbus_connection_close(connection);
            dbus_connection_unref(connection);
        }
    }

    OpenThreadClient::DeviceRole OpenThreadClient::TranslateOTBRDeviceRole(otbr::DBus::DeviceRole role)
    {
        OpenThreadClient::DeviceRole retVal = DEVICE_ROLE_UNKNOWN;

        switch (role)
        {
            case OTBR_DEVICE_ROLE_DISABLED:
            {
                retVal = DEVICE_ROLE_DISABLED;
                break;
            }
            case OTBR_DEVICE_ROLE_DETACHED:
            {
                retVal = DEVICE_ROLE_DETACHED;
                break;
            }
            case OTBR_DEVICE_ROLE_CHILD:
            {
                retVal = DEVICE_ROLE_CHILD;
                break;
            }
            case OTBR_DEVICE_ROLE_ROUTER:
            {
                retVal = DEVICE_ROLE_ROUTER;
                break;
            }
            case OTBR_DEVICE_ROLE_LEADER:
            {
                retVal = DEVICE_ROLE_LEADER;
                break;
            }
        }

        return retVal;
    }

} // namespace barton
