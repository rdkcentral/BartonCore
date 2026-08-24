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
# End-to-end test of the native ONVIF camera driver through the public BCoreClient API against a
# mock ONVIF camera: discover the camera, provide credentials, and drive the abstract camera
# endpoint's stream/takePicture springboards to obtain a media (RTSP) URL and a snapshot (JPEG) URL.
#

import json
import logging
import time
import urllib.request

import gi

from testing.utils.barton_utils import (
    resource_update_listener,
    resource_uri,
)

gi.require_version("Gst", "1.0")
from gi.repository import Gst  # noqa: E402

logger = logging.getLogger(__name__)


def _probe_rtsp_buffers(url, seconds=8):
    """Open the RTSP URL and count the raw RTP buffers rtspsrc delivers to fakesink within the time budget.

    The pipeline is ``rtspsrc ! fakesink`` with no depayloader, so these are undepayloaded RTP
    buffers; the count is only used to prove that media is actually flowing over the connection.
    """
    Gst.init(None)
    counter = {"n": 0}
    pipeline = Gst.parse_launch(f"rtspsrc location={url} latency=100 ! fakesink name=sink")
    sink = pipeline.get_by_name("sink")
    sink.set_property("signal-handoffs", True)
    sink.connect("handoff", lambda s, b, p: counter.__setitem__("n", counter["n"] + 1))

    pipeline.set_state(Gst.State.PLAYING)
    try:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline and counter["n"] == 0:
            time.sleep(0.2)
    finally:
        pipeline.set_state(Gst.State.NULL)

    return counter["n"]


def _discover_camera(environment):
    client = environment.get_client()
    client.discover_start(["camera"], [], 10)
    environment.wait_for_device_added(timeout=15)

    cameras = client.get_devices_by_device_class("camera")
    assert len(cameras) >= 1, "Expected the mock ONVIF camera to be discovered"
    return cameras[-1]


def test_onvif_camera_discovered(onvif_environment, onvif_camera):
    camera = _discover_camera(onvif_environment)
    assert camera.props.uuid == onvif_camera.device_uuid


def test_onvif_stream_returns_media_url(onvif_environment, onvif_camera):
    client = onvif_environment.get_client()
    camera = _discover_camera(onvif_environment)

    # Provide credentials (interim primitive auth model).
    assert client.write_resource(resource_uri(camera, "username", endpoint_id="onvif"), "admin")
    assert client.write_resource(resource_uri(camera, "password", endpoint_id="onvif"), "secret")

    media_queue = resource_update_listener(client, "mediaUrl")

    # The abstract stream execute springboards to the ONVIF media entry point.
    ok, info = client.execute_resource(resource_uri(camera, "stream", endpoint_id="camera"), "")
    assert ok
    stream_info = json.loads(info)
    assert stream_info["protocol"] == "onvif"
    assert stream_info["entryPoint"] == resource_uri(camera, "getMediaUrl", endpoint_id="onvif")

    # Follow the entry point: getMediaUrl retrieves the RTSP URL and delivers it as a mediaUrl event.
    ok, _ = client.execute_resource(stream_info["entryPoint"], "")
    assert ok, "getMediaUrl entry-point execute failed"

    media_url = media_queue.get(timeout=10)
    assert media_url == onvif_camera.rtsp_uri
    # The delivered URL must be credential-free.
    assert "@" not in media_url


def test_onvif_take_picture_returns_snapshot_url(onvif_environment, onvif_camera):
    client = onvif_environment.get_client()
    camera = _discover_camera(onvif_environment)

    assert client.write_resource(resource_uri(camera, "username", endpoint_id="onvif"), "admin")
    assert client.write_resource(resource_uri(camera, "password", endpoint_id="onvif"), "secret")

    snapshot_queue = resource_update_listener(client, "snapshotUrl")

    # The abstract takePicture execute springboards to the ONVIF snapshot entry point.
    ok, info = client.execute_resource(resource_uri(camera, "takePicture", endpoint_id="camera"), "")
    assert ok
    snapshot_info = json.loads(info)
    assert snapshot_info["protocol"] == "onvif"
    assert snapshot_info["entryPoint"] == resource_uri(camera, "getSnapshotUrl", endpoint_id="onvif")

    ok, _ = client.execute_resource(snapshot_info["entryPoint"], "")
    assert ok, "getSnapshotUrl entry-point execute failed"

    snapshot_url = snapshot_queue.get(timeout=10)
    assert snapshot_url == onvif_camera.snapshot_uri


def test_onvif_auth_required_is_readable_and_non_secret(onvif_environment, onvif_camera):
    client = onvif_environment.get_client()
    camera = _discover_camera(onvif_environment)

    assert client.read_resource(resource_uri(camera, "authRequired", endpoint_id="onvif")) == "true"


def test_onvif_stream_media_flows(onvif_environment, onvif_camera):
    client = onvif_environment.get_client()
    camera = _discover_camera(onvif_environment)

    assert client.write_resource(resource_uri(camera, "username", endpoint_id="onvif"), "admin")
    assert client.write_resource(resource_uri(camera, "password", endpoint_id="onvif"), "secret")

    media_queue = resource_update_listener(client, "mediaUrl")

    ok, info = client.execute_resource(resource_uri(camera, "stream", endpoint_id="camera"), "")
    assert ok
    ok, _ = client.execute_resource(json.loads(info)["entryPoint"], "")
    assert ok, "getMediaUrl entry-point execute failed"

    media_url = media_queue.get(timeout=10)
    assert media_url == onvif_camera.rtsp_uri

    # Open the driver-reported RTSP URL against the mock's live server and confirm media flows.
    buffers = _probe_rtsp_buffers(media_url)
    assert buffers > 0, "Expected to receive RTSP media buffers from the mock camera"


def test_onvif_snapshot_fetch_returns_jpeg(onvif_environment, onvif_camera):
    client = onvif_environment.get_client()
    camera = _discover_camera(onvif_environment)

    assert client.write_resource(resource_uri(camera, "username", endpoint_id="onvif"), "admin")
    assert client.write_resource(resource_uri(camera, "password", endpoint_id="onvif"), "secret")

    snapshot_queue = resource_update_listener(client, "snapshotUrl")

    ok, info = client.execute_resource(resource_uri(camera, "takePicture", endpoint_id="camera"), "")
    assert ok
    ok, _ = client.execute_resource(json.loads(info)["entryPoint"], "")
    assert ok, "getSnapshotUrl entry-point execute failed"

    snapshot_url = snapshot_queue.get(timeout=10)

    # Fetch the driver-reported snapshot URL from the mock and confirm it returns JPEG bytes.
    with urllib.request.urlopen(snapshot_url, timeout=10) as response:
        body = response.read()

    assert body[:2] == b"\xff\xd8", "Expected a JPEG (SOI marker) from the snapshot URL"
