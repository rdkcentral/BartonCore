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


"""
Integration tests for lazy SBMD driver activation.

At startup SBMD drivers are loaded inactive (claim-stub only). A driver is
activated when a device first binds to it. These tests verify that behavior
through the activation observability metrics:

  - sbmd.driver.active.count : set-on-change gauge of currently-active drivers
  - sbmd.driver.activation   : counter incremented on each driver activation
"""

import contextlib
import json
import threading
import time
from pathlib import Path

import pytest
from testing.mocks.devices.matter.matter_temperature_sensor import (
    MatterTemperatureSensor,
)
from testing.utils.barton_utils import commission_device, resource_uri

_DEFERRED_SPEC = (
    Path(__file__).resolve().parent.parent.parent
    / "testing"
    / "resources"
    / "sbmd-specs"
    / "deferred-command-test.sbmd.js"
)

pytestmark = [
    pytest.mark.requires_matterjs,
]


def _gauge_sum(metrics, name):
    metric = metrics.get(name)

    if metric is None:
        return None

    return sum(dp["value"] for dp in metric.get("dataPoints", []))


def _metrics(client):
    return json.loads(client.get_telemetry()).get("metrics", {})


def _wait_for_device_count(client, device_class, count, timeout=20.0):
    deadline = time.time() + timeout

    while time.time() < deadline:
        devices = client.get_devices_by_device_class(device_class)

        if len(devices) >= count:
            return devices

        time.sleep(0.25)

    return client.get_devices_by_device_class(device_class)


def test_drivers_inactive_at_startup(default_environment):
    """
    With no devices commissioned, no driver is activated. The factory publishes
    the true active-driver count at startup, so the gauge is present and reports
    zero (a non-zero value would indicate eager startup activation).
    """

    client = default_environment.get_client()
    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    active = _gauge_sum(metrics, "sbmd.driver.active.count")
    assert (
        active == 0
    ), f"Expected 0 active SBMD drivers at startup, gauge reported {active}"


def test_commissioning_activates_driver(
    default_environment, matter_temperature_sensor
):
    """
    Commissioning a device that claims an SBMD driver activates that driver:
    the activation counter increments and the active-driver gauge rises above
    zero.
    """
    commission_device(
        default_environment,
        matter_temperature_sensor,
        "environmentalSensor",
    )
    client = default_environment.get_client()
    telemetry = json.loads(client.get_telemetry())
    metrics = telemetry.get("metrics", {})

    activation = _gauge_sum(metrics, "sbmd.driver.activation")
    assert activation is not None and activation >= 1, (
        "sbmd.driver.activation counter should be >= 1 after commissioning"
    )

    active = _gauge_sum(metrics, "sbmd.driver.active.count")
    assert active is not None and active >= 1, (
        "sbmd.driver.active.count gauge should be >= 1 after commissioning"
    )

    # Each activation records one observation on the activation-duration histogram.
    duration = metrics.get("sbmd.driver.activation.duration_ms")
    assert (
        duration is not None
    ), "sbmd.driver.activation.duration_ms not found in telemetry after commissioning"
    duration_dps = duration.get("dataPoints", [])
    assert (
        sum(dp["count"] for dp in duration_dps) >= 1
    ), "sbmd.driver.activation.duration_ms should have >= 1 observation after commissioning"
    assert (
        sum(dp["sum"] for dp in duration_dps) > 0
    ), "sbmd.driver.activation.duration_ms sum should be positive (activation takes non-zero time)"


def test_removal_deactivates_driver(
    default_environment, matter_temperature_sensor
):
    """
    Removing the last device bound to a driver deactivates it: the deactivation
    counter increments and the active-driver gauge returns to zero.
    """
    device = commission_device(
        default_environment,
        matter_temperature_sensor,
        "environmentalSensor",
    )
    client = default_environment.get_client()

    assert (_gauge_sum(_metrics(client), "sbmd.driver.active.count") or 0) >= 1

    assert client.remove_device(device.props.uuid), "remove_device failed"

    # Deactivation runs asynchronously on the Matter thread after fabric teardown.
    active = None
    deactivation = None
    deadline = time.time() + 20

    while time.time() < deadline:
        metrics = _metrics(client)
        active = _gauge_sum(metrics, "sbmd.driver.active.count")
        deactivation = _gauge_sum(metrics, "sbmd.driver.deactivation")

        if (deactivation or 0) >= 1 and (active or 0) == 0:
            break

        time.sleep(0.5)

    assert (deactivation or 0) >= 1, (
        "sbmd.driver.deactivation counter should be >= 1 after last device removed"
    )
    assert (active or 0) == 0, (
        "sbmd.driver.active.count gauge should return to 0 after last device removed"
    )


def test_driver_stays_active_until_last_device_removed(
    default_environment, matter_temperature_sensor
):
    """
    With two devices bound to the same driver, removing one leaves the driver
    active (no deactivation, gauge unchanged); only removing the last device
    deactivates it.
    """
    client = default_environment.get_client()

    # First sensor via the fixture.
    client.commission_device(
        matter_temperature_sensor.get_commissioning_code(), 100
    )
    default_environment.wait_for_device_added()

    # Second sensor of the same class claims the same SBMD driver.
    second = MatterTemperatureSensor()
    second.start()

    try:
        client.commission_device(second.get_commissioning_code(), 100)
        devices = _wait_for_device_count(client, "environmentalSensor", 2)
        assert len(devices) == 2, (
            f"expected 2 environmental sensors, got {len(devices)}"
        )
        uuids = [d.props.uuid for d in devices]

        # One driver, two devices: exactly one active driver.
        assert (_gauge_sum(_metrics(client), "sbmd.driver.active.count") or 0) == 1
        deactivations_before = (
            _gauge_sum(_metrics(client), "sbmd.driver.deactivation") or 0
        )

        # Remove the first device; the driver must stay active (second still bound).
        assert client.remove_device(uuids[0]), "remove_device failed"

        # Allow any (incorrect) deactivation time to occur, then confirm it did not.
        time.sleep(4)
        metrics = _metrics(client)
        assert (_gauge_sum(metrics, "sbmd.driver.deactivation") or 0) == deactivations_before, (
            "driver deactivated while a second device was still bound"
        )
        assert (_gauge_sum(metrics, "sbmd.driver.active.count") or 0) == 1, (
            "active-driver gauge changed while a second device was still bound"
        )

        # Remove the last device; now the driver deactivates.
        assert client.remove_device(uuids[1]), "remove_device failed"

        active = None
        deactivation = None
        deadline = time.time() + 20

        while time.time() < deadline:
            metrics = _metrics(client)
            active = _gauge_sum(metrics, "sbmd.driver.active.count")
            deactivation = _gauge_sum(metrics, "sbmd.driver.deactivation")

            if (deactivation or 0) > deactivations_before and (active or 0) == 0:
                break

            time.sleep(0.5)

        assert (deactivation or 0) > deactivations_before, (
            "driver did not deactivate after its last device was removed"
        )
        assert (active or 0) == 0, (
            "sbmd.driver.active.count gauge should return to 0 after last device removed"
        )
    finally:
        second._cleanup()


def test_deferred_op_settled_when_last_device_removed(
    default_environment, matter_deferred_cmd_test_device
):
    """
    Removing the last device while a deferred operation is pending must settle that
    operation (its parking promise resolves so the blocked caller returns) and
    deactivate the driver, and a late command response must be handled safely.
    """
    device = commission_device(
        default_environment, matter_deferred_cmd_test_device, "deferredCmdTest"
    )
    client = default_environment.get_client()
    uuid = device.props.uuid

    assert (_gauge_sum(_metrics(client), "sbmd.driver.active.count") or 0) >= 1
    deactivations_before = _gauge_sum(_metrics(client), "sbmd.driver.deactivation") or 0

    # Withhold the device's Toggle response so the deferred op stays pending.
    matter_deferred_cmd_test_device.sideband.send("armToggleHang")

    # execute_resource blocks on the parked deferred op, so run it off-thread.
    exec_result = {}

    def run_toggle():
        try:
            # execute_resource returns (ok, response); a cancelled deferred op must settle as a failure.
            ok, _ = client.execute_resource(
                resource_uri(device, "toggle", endpoint_id=1), ""
            )
            exec_result["ok"] = ok
        except Exception as e:  # noqa: BLE001 - record whatever the blocked call raises
            exec_result["error"] = e

    worker = threading.Thread(target=run_toggle, daemon=True)
    worker.start()

    # Wait until the deferred op is actually in flight before removing the device.
    deadline = time.time() + 10

    while time.time() < deadline:
        if (_gauge_sum(_metrics(client), "sbmd.deferred.in_flight") or 0) >= 1:
            break

        time.sleep(0.05)

    assert (
        _gauge_sum(_metrics(client), "sbmd.deferred.in_flight") or 0
    ) >= 1, "deferred operation never parked"

    # Remove the last device while the op is pending.
    assert client.remove_device(uuid), "remove_device failed"

    # The parked op must be settled — the blocked execute_resource must return, not hang.
    worker.join(timeout=20)
    assert (
        not worker.is_alive()
    ), "execute_resource did not return after device removal; deferred op left unresolved"
    assert (
        exec_result.get("error") is None
    ), f"execute_resource raised unexpectedly: {exec_result.get('error')}"
    assert (
        exec_result.get("ok") is False
    ), "a cancelled deferred op must settle as a failure (ok=False), not success"

    # The driver deactivates once its last device is gone.
    active = None
    deactivation = None
    deadline = time.time() + 20

    while time.time() < deadline:
        metrics = _metrics(client)
        active = _gauge_sum(metrics, "sbmd.driver.active.count")
        deactivation = _gauge_sum(metrics, "sbmd.driver.deactivation")

        if (deactivation or 0) > deactivations_before and (active or 0) == 0:
            break

        time.sleep(0.5)

    assert (
        deactivation or 0
    ) > deactivations_before, (
        "driver did not deactivate after its last device was removed"
    )
    assert (
        active or 0
    ) == 0, (
        "sbmd.driver.active.count gauge should return to 0 after last device removed"
    )

    # Release the withheld response; the now-late callback must be handled safely (Barton alive).
    matter_deferred_cmd_test_device.sideband.send("releaseToggle")
    time.sleep(1)
    assert json.loads(client.get_telemetry()) is not None


# Upper bound on how long a commissioning attempt may take to resolve. A successful bind fires
# device-added in a few seconds; this generous margin keeps the spec patched until a rejected
# attempt has definitively given up, so the bind is never evaluated against the restored spec.
_COMMISSION_RESOLVE_TIMEOUT = 30


@contextlib.contextmanager
def _patched_spec(old, new):
    """Replace the first occurrence of old with new in the deferred test spec on disk,
    restoring the original content afterward even on failure."""
    original = _DEFERRED_SPEC.read_text()
    assert old in original, f"expected {old!r} in {_DEFERRED_SPEC}"

    try:
        _DEFERRED_SPEC.write_text(original.replace(old, new, 1))
        yield
    finally:
        _DEFERRED_SPEC.write_text(original)


def _assert_bind_rejected_on_version_change(default_environment, device, old, new):
    """With Barton already running (drivers constructed from the original spec), change a
    version in the spec on disk and verify commissioning is rejected: the on-demand activation
    re-reads the changed spec, the version no longer matches the cached value, and the bind
    fails so the device is never added and the activation is rolled back."""
    client = default_environment.get_client()
    # Ensure Barton is up and drivers were constructed from the original spec before patching.
    client.get_telemetry()

    appeared = False

    with _patched_spec(old, new):
        try:
            client.commission_device(device.get_commissioning_code(), 100)
        except (
            Exception
        ):  # noqa: BLE001 - commissioning is expected to fail; the add is what we assert
            pass

        # Commissioning runs on a detached background thread, so the bind can be attempted
        # well after commission_device() returns. Keep the mismatched spec on disk until the
        # attempt has definitively resolved so the bind is always evaluated against it: a
        # successful bind would fire device-added (returns immediately), while a rejected bind
        # never does and the wait times out. Restoring only happens on leaving this block.
        try:
            default_environment.wait_for_device_added(
                timeout=_COMMISSION_RESOLVE_TIMEOUT
            )
            appeared = True
        except AssertionError:
            # No device-added within the window; confirm none of the class slipped in regardless.
            appeared = bool(client.get_devices_by_device_class("deferredCmdTest"))

    assert (
        not appeared
    ), "device must not be commissioned when the re-read spec version no longer matches"
    # A rejected bind rolls the activation back, so no driver is left active.
    assert (
        _gauge_sum(_metrics(client), "sbmd.driver.active.count") or 0
    ) == 0, "a rejected bind must roll the activation back (no active driver)"


def test_bind_rejected_when_device_class_version_changes(
    default_environment, matter_deferred_cmd_test_device
):
    _assert_bind_rejected_on_version_change(
        default_environment,
        matter_deferred_cmd_test_device,
        "deviceClassVersion: 1",
        "deviceClassVersion: 2",
    )


def test_bind_rejected_when_profile_version_changes(
    default_environment, matter_deferred_cmd_test_device
):
    _assert_bind_rejected_on_version_change(
        default_environment,
        matter_deferred_cmd_test_device,
        "profileVersion: 1",
        "profileVersion: 2",
    )
