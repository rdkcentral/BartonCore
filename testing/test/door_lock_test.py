# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
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


import logging

import pytest
from testing.environment.default_environment_orchestrator import (
    DefaultEnvironmentOrchestrator,
)
from testing.utils.barton_utils import (
    assert_device_has_common_resources,
    commission_device,
    resource_update_listener,
    resource_uri,
    wait_for_resource_value,
)

logger = logging.getLogger(__name__)

pytestmark = pytest.mark.requires_matterjs


@pytest.fixture
def fast_commfail_environment():
    """Like default_environment but with barton.commFail.monitorIntervalSecs=1.

    This property must be set before start_client() because it is read by
    deviceServiceStart() immediately after deviceServiceCommFailInit(). It
    shortens the watchdog check interval from 60 s to 1 s, allowing a 3 s
    commFailOverrideSeconds timeout to be detected promptly. Only
    test_locked_resource_seeded_on_synchronize requires this.
    """
    env = DefaultEnvironmentOrchestrator()
    env._barton_client_params.get_property_provider().set_property_string(
        "barton.commFail.monitorIntervalSecs", "1"
    )
    env.start_client()
    env.wait_for_client_to_be_ready()
    try:
        yield env
    finally:
        env._cleanup()


def _commission_door_lock(default_environment, matter_door_lock):
    """Helper to commission the door lock and return the device object."""
    return commission_device(default_environment, matter_door_lock, "doorLock")


def test_commission_door_lock(default_environment, matter_door_lock):
    """Commission a virtual door lock and verify it appears as a doorLock device."""
    lock = _commission_door_lock(default_environment, matter_door_lock)

    assert_device_has_common_resources(
        default_environment.get_client(),
        lock,
        [
            "firmwareVersionString",
            "macAddress",
            "networkType",
            "serialNumber",
        ],
    )


def test_lock_unlock_via_barton(default_environment, matter_door_lock):
    """Lock and unlock the door lock via Barton resource writes, verify via side-band."""
    lock = _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "locked")

    # Unlock via Barton — execute the "unlock" function resource
    client.execute_resource(resource_uri(lock, "unlock", endpoint_id=1), "", "")
    wait_for_resource_value(resource_updated_queue, "false")

    # Verify via side-band
    state = matter_door_lock.sideband.get_state()
    assert state["lockState"] == "unlocked"

    # Lock via Barton — execute the "lock" function resource
    client.execute_resource(resource_uri(lock, "lock", endpoint_id=1), "", "")
    wait_for_resource_value(resource_updated_queue, "true", timeout=10)

    # Verify via side-band
    state = matter_door_lock.sideband.get_state()
    assert state["lockState"] == "locked"


def test_sideband_unlock_triggers_barton_update(
    default_environment, matter_door_lock
):
    """Simulate a manual unlock via side-band and verify Barton receives the update."""
    _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "locked")

    # The door lock starts in locked state. Simulate manual unlock via side-band.
    result = matter_door_lock.sideband.send("unlock")
    assert result["lockState"] == "unlocked"

    # Barton should receive a resource update for the locked resource
    wait_for_resource_value(resource_updated_queue, "false", timeout=10)


def test_sideband_lock_triggers_barton_update(
    default_environment, matter_door_lock
):
    """Simulate a manual lock via side-band and verify Barton receives the update."""
    _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource_updated_queue = resource_update_listener(client, "locked")

    # First unlock the device via side-band so we can test locking
    matter_door_lock.sideband.send("unlock")
    wait_for_resource_value(resource_updated_queue, "false")

    # Simulate manual lock via side-band
    result = matter_door_lock.sideband.send("lock")
    assert result["lockState"] == "locked"

    # Barton should receive a resource update for the locked resource
    wait_for_resource_value(resource_updated_queue, "true")


def test_locked_resource_seeded_on_commission(default_environment, matter_door_lock):
    """Verify that the locked resource is seeded with the correct initial value at commission.

    The virtual door lock starts in the locked state. The seedFrom mapper runs inside
    DoRegisterResources (before DEVICE_ADDED fires), so the value is baked directly into
    createEndpointResource. Verify by reading the resource value directly after commission.
    """

    lock = _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    resource = client.get_resource_by_uri(resource_uri(lock, "locked", endpoint_id=1))
    assert resource is not None, "locked resource not found after commission"
    assert resource.props.value == "true", (
        f"Expected locked resource to be seeded to 'true' at commission time, "
        f"got '{resource.props.value}'"
    )


def test_locked_resource_seeded_on_synchronize(
    fast_commfail_environment, matter_door_lock
):
    """Verify that the locked resource is re-seeded at synchronize time.

    Simulates a real-world scenario: Barton loses communication with the
    device, the device state changes while Barton is in comm-fail, and Barton
    learns about the new state when the device comes back online.

    goOffline abruptly kills all Matter sessions via initiateForceClose()
    without sending any Matter messages (no SessionClose, no StatusReport).

    Two device metadata properties are used to keep the test fast:

    commFailOverrideSeconds=3: shortens the comm-fail watchdog timeout from
    its default (~56 min) to 3 s.  Writing this metadata also immediately
    reprograms the running watchdog timer (deviceServiceCommFailSetDeviceTimeoutSecs
    is called from setMetadata() in deviceService.c).  The fast_commfail_environment
    fixture sets barton.commFail.monitorIntervalSecs=1 so the watchdog thread
    checks every second rather than every 60 s; without that, the 3 s timeout
    would expire undetected until the next 60 s check.  Together these two
    settings cause the communicationFailure resource to become "true" within
    ~4 s of goOffline.

    matterLivenessTimeoutOverrideMs=1: after the device comes back online and
    comm-fail is confirmed, the ReadClient's subscription is still logically
    active from Barton's perspective (the liveness timer has ~14 s remaining).
    Setting this property causes MatterDeviceDriver to call
    ReadClient::OverrideLivenessTimeout(1ms) via ScheduleLambda so the
    liveness timer fires on the next Matter event-loop tick. This triggers
    DefaultResubscribePolicy to open a new CASE session (ForceCASE=true).
    When the priming report arrives with LockState=Unlocked, the watchdog pet
    fires communicationRestored → synchronizeDevice → SeedInitialResourceValues
    which reads Unlocked from the cache and writes "false".

    Both properties are set via the standard b_core_client_write_metadata API.
    """

    lock = _commission_door_lock(fast_commfail_environment, matter_door_lock)
    client = fast_commfail_environment.get_client()

    # Shorten the comm-fail watchdog so it fires in ~3 s instead of ~56 min.
    metadata_base = f"/{lock.props.uuid}/m"
    client.write_metadata(f"{metadata_base}/commFailOverrideSeconds", "3")

    commfail_queue = resource_update_listener(client, "communicationFailure")

    # Abruptly kill all Matter sessions without sending any Matter messages.
    # The ServerNode stays running so Barton can reconnect once asked to.
    matter_door_lock.sideband.send("goOffline")

    # Wait for Barton to detect comm-fail via the watchdog (~3 s).
    wait_for_resource_value(commfail_queue, "true", timeout=10)

    seed_queue = resource_update_listener(client, "locked")

    # Update the device's lock state attribute before Barton reconnects.
    matter_door_lock.sideband.send("comeOnline", {"lockState": "unlocked"})

    # The ReadClient's liveness timer still has ~14 s remaining at this point.
    # Setting matterLivenessTimeoutOverrideMs=1 causes MatterDeviceDriver to
    # apply ReadClient::OverrideLivenessTimeout(1ms) via ScheduleLambda, so
    # the liveness fires immediately, triggering DefaultResubscribePolicy to
    # open a new CASE session.  Since the device is now online, CASE succeeds,
    # the priming report delivers LockState=Unlocked, and synchronizeDevice
    # seeds locked="false".
    client.write_metadata(f"{metadata_base}/matterLivenessTimeoutOverrideMs", "1")

    wait_for_resource_value(seed_queue, "false", timeout=15)


def test_locked_resource_updated_by_event(default_environment, matter_door_lock):
    """Verify that the locked resource updates when a LockOperation event is received.

    Confirm the initial seeded value via direct read (the seedFrom mapper runs inside
    DoRegisterResources and bakes the value in without emitting RESOURCE_UPDATED), then
    trigger sideband unlock and verify the resource transitions to "false" via the
    LockOperation event. Then lock and verify "true".
    """
    lock = _commission_door_lock(default_environment, matter_door_lock)
    client = default_environment.get_client()

    # Confirm initial seed via direct read — no RESOURCE_UPDATED fires for this.
    resource = client.get_resource_by_uri(resource_uri(lock, "locked", endpoint_id=1))
    assert resource is not None, "locked resource not found after commission"
    assert resource.props.value == "true", (
        f"Expected locked resource to be seeded to 'true' at commission time, "
        f"got '{resource.props.value}'"
    )

    # From here, use event-driven updates via RESOURCE_UPDATED.
    resource_updated_queue = resource_update_listener(client, "locked")

    # Trigger sideband unlock — DoorLockDevice.js emits LockOperation (Unlock) event
    result = matter_door_lock.sideband.send("unlock")
    assert result["lockState"] == "unlocked"

    # Barton should receive a resource update driven by the LockOperation event
    wait_for_resource_value(resource_updated_queue, "false", timeout=10)

    # Trigger sideband lock — DoorLockDevice.js emits LockOperation (Lock) event
    result = matter_door_lock.sideband.send("lock")
    assert result["lockState"] == "locked"

    # Barton should receive a resource update driven by the LockOperation event
    wait_for_resource_value(resource_updated_queue, "true", timeout=10)
