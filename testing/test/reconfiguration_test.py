# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

#
# Verifies public query APIs gate a device that is being reconfigured instead of serving stale data.
#
# Reconfiguration has no public trigger, and the startup path can't be re-run in-process (the Matter
# SDK can't be re-initialized), so the test calls the library's exported
# deviceServiceReconfigureDevice() directly: it marks the device pending synchronously, which is all
# the gate checks, while the deferred work never runs and is cancelled at teardown or on device removal.
#

import ctypes
import time

import pytest
from gi.repository import BCore, GLib
from testing.utils.barton_utils import commission_device, resource_uri

# Far beyond the test's lifetime, so the scheduled reconfiguration never actually runs.
DEFERRED_RECONFIGURATION_DELAY_SECS = 3600

# Well below the 60 s the removal path waits when it cannot cancel and must wait for the task instead.
REMOVAL_CANCEL_BOUND_SECS = 30

pytestmark = [
    pytest.mark.requires_matterjs,
]


def _barton_library():
    """Return a handle to the libBartonCore instance already loaded by the GI bindings."""
    with open("/proc/self/maps", encoding="utf-8") as maps:
        for line in maps:
            if "libBartonCore.so" in line:
                return ctypes.CDLL(line.split()[-1])

    raise RuntimeError("libBartonCore is not loaded")


def _begin_reconfiguration(uuid):
    """Mark a device as pending reconfiguration without letting the deferred work run."""
    lib = _barton_library()
    # Hand-written binding: keep in sync with the declaration in api/c/public/private/deviceServicePrivate.h.
    # deviceServiceReconfigureDevice(deviceUuid, delaySeconds, completedCallback, allowAsap)
    lib.deviceServiceReconfigureDevice.argtypes = [
        ctypes.c_char_p,
        ctypes.c_uint32,
        ctypes.c_void_p,
        ctypes.c_bool,
    ]
    lib.deviceServiceReconfigureDevice.restype = None
    lib.deviceServiceReconfigureDevice(
        uuid.encode(), DEFERRED_RECONFIGURATION_DELAY_SECS, None, False
    )


def _is_reconfiguration_pending(uuid):
    lib = _barton_library()
    # Hand-written binding: keep in sync with the declaration in api/c/public/private/deviceServicePrivate.h.
    lib.deviceServiceIsReconfigurationPending.argtypes = [ctypes.c_char_p]
    lib.deviceServiceIsReconfigurationPending.restype = ctypes.c_bool

    return lib.deviceServiceIsReconfigurationPending(uuid.encode())


def test_query_during_reconfiguration_returns_error(
    default_environment, matter_temperature_sensor
):
    """Public query APIs must gate a device that is being reconfigured, not return stale data."""
    client = default_environment.get_client()

    device = commission_device(
        default_environment, matter_temperature_sensor, "environmentalSensor"
    )
    uuid = device.props.uuid

    # Before reconfiguration the device is served normally.
    assert client.get_device_by_id(uuid) is not None

    _begin_reconfiguration(uuid)

    # A single-device lookup must now fail rather than return stale data.
    with pytest.raises(GLib.Error) as excinfo:
        client.get_device_by_id(uuid)
    assert excinfo.value.matches(
        BCore.Client.error_quark(), BCore.ServiceStateError.DEVICE_RECONFIGURING
    )

    # URI-based calls resolve the owning device from the URI and are gated the same way.
    with pytest.raises(GLib.Error) as excinfo:
        client.read_resource(resource_uri(device, "temperature", endpoint_id="1"))
    assert excinfo.value.matches(
        BCore.Client.error_quark(), BCore.ServiceStateError.DEVICE_RECONFIGURING
    )

    # List getters still include the device: it exists, it is only busy.
    all_uuids = [d.props.uuid for d in client.get_devices()]
    assert uuid in all_uuids

    class_uuids = [
        d.props.uuid for d in client.get_devices_by_device_class("environmentalSensor")
    ]
    assert uuid in class_uuids


def test_remove_device_cancels_pending_reconfiguration(
    default_environment, matter_temperature_sensor
):
    """Removing a device whose reconfiguration has not started cancels it instead of waiting for it."""
    client = default_environment.get_client()

    device = commission_device(
        default_environment, matter_temperature_sensor, "environmentalSensor"
    )
    uuid = device.props.uuid

    _begin_reconfiguration(uuid)
    assert _is_reconfiguration_pending(uuid)

    start = time.monotonic()
    assert client.remove_device(uuid)
    assert time.monotonic() - start < REMOVAL_CANCEL_BOUND_SECS

    assert not _is_reconfiguration_pending(uuid)
    # Gone, not busy: the lookup reports "not found" without a reconfiguring error.
    assert client.get_device_by_id(uuid) is None
