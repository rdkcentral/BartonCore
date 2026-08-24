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
# A test-only mock ONVIF camera. It answers WS-Discovery probes over UDP and serves canned ONVIF
# SOAP responses over HTTP, using only the Python standard library. The ONVIF driver is pointed at
# the UDP responder by unicast via the "onvif.discovery.address" property (see the ONVIF driver's
# discovery test seam), which avoids relying on multicast reachability in CI/container networks.
#
# It also runs a real GStreamer RTSP server that publishes a dummy H.264 test pattern, so the RTSP
# media URL returned by GetStreamUri points at a live stream that can be validated end-to-end.
#

import base64
import http.server
import logging
import socket
import threading
import uuid as uuid_module

import gi
import pytest

gi.require_version("Gst", "1.0")
gi.require_version("GstRtspServer", "1.0")
from gi.repository import GLib, Gst, GstRtspServer  # noqa: E402

logger = logging.getLogger(__name__)

# The RTSP media factory that publishes a dummy live H.264 test pattern for the mock camera.
# Constrained-baseline matches the reference app's MSE player codec string (avc1.42e01f).
_RTSP_LAUNCH = (
    "( videotestsrc is-live=true ! video/x-raw,width=320,height=240,framerate=15/1 "
    "! x264enc tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline "
    "! rtph264pay name=pay0 pt=96 )"
)

# A minimal 1x1 JPEG served for GetSnapshotUri so the snapshot fetch path can be validated.
_SNAPSHOT_JPEG = base64.b64decode(
    "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0a"
    "HBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/wAALCAABAAEBAREA/8QAFAABAAAAAAAA"
    "AAAAAAAAAAAACf/EABQQAQAAAAAAAAAAAAAAAAAAAAD/2gAIAQEAAT8Af//Z"
)


def _soap_envelope(body: str) -> str:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>'
        '<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope">'
        "<s:Body>" + body + "</s:Body></s:Envelope>"
    )


class _OnvifSoapHandler(http.server.BaseHTTPRequestHandler):
    """Serves canned SOAP responses; the operation is chosen by matching the request body."""

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8", "ignore")

        if "GetDeviceInformation" in body:
            payload = _soap_envelope(
                '<tds:GetDeviceInformationResponse xmlns:tds="http://www.onvif.org/ver10/device/wsdl">'
                "<tds:Manufacturer>MockCorp</tds:Manufacturer><tds:Model>MockCam</tds:Model>"
                "<tds:FirmwareVersion>9.9.9</tds:FirmwareVersion><tds:SerialNumber>SN-MOCK</tds:SerialNumber>"
                "<tds:HardwareId>HW-MOCK</tds:HardwareId></tds:GetDeviceInformationResponse>"
            )
        elif "GetProfiles" in body:
            payload = _soap_envelope(
                '<trt:GetProfilesResponse xmlns:trt="http://www.onvif.org/ver10/media/wsdl">'
                '<trt:Profiles token="profile0"/></trt:GetProfilesResponse>'
            )
        elif "GetStreamUri" in body:
            payload = _soap_envelope(
                '<trt:GetStreamUriResponse xmlns:trt="http://www.onvif.org/ver10/media/wsdl" '
                'xmlns:tt="http://www.onvif.org/ver10/schema"><trt:MediaUri>'
                "<tt:Uri>" + self.server.rtsp_uri + "</tt:Uri></trt:MediaUri></trt:GetStreamUriResponse>"
            )
        elif "GetSnapshotUri" in body:
            payload = _soap_envelope(
                '<trt:GetSnapshotUriResponse xmlns:trt="http://www.onvif.org/ver10/media/wsdl" '
                'xmlns:tt="http://www.onvif.org/ver10/schema"><trt:MediaUri>'
                "<tt:Uri>" + self.server.snapshot_uri + "</tt:Uri></trt:MediaUri></trt:GetSnapshotUriResponse>"
            )
        else:
            self.send_response(400)
            self.end_headers()
            return

        data = payload.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/soap+xml; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        # Serve the canned snapshot JPEG referenced by GetSnapshotUri; 404 anything else.
        if self.path.startswith("/snapshot"):
            self.send_response(200)
            self.send_header("Content-Type", "image/jpeg")
            self.send_header("Content-Length", str(len(_SNAPSHOT_JPEG)))
            self.end_headers()
            self.wfile.write(_SNAPSHOT_JPEG)
            return

        self.send_response(404)
        self.end_headers()

    def log_message(self, fmt, *args):
        logger.debug("onvif-mock-http: " + fmt, *args)


class OnvifCameraServer:
    """A mock ONVIF camera: a UDP WS-Discovery responder plus an HTTP SOAP endpoint."""

    def __init__(self):
        # A unique id per instance keeps repeated runs from colliding on a persisted device id.
        self.device_uuid = str(uuid_module.uuid4())

        # HTTP SOAP endpoint.
        self._http = http.server.HTTPServer(("127.0.0.1", 0), _OnvifSoapHandler)
        http_port = self._http.server_address[1]
        self.service_url = f"http://127.0.0.1:{http_port}/onvif/device_service"

        # Live RTSP server publishing a dummy H.264 test pattern at a per-device mount point.
        self._rtsp_server, rtsp_port = self._build_rtsp_server()
        self.rtsp_uri = f"rtsp://127.0.0.1:{rtsp_port}/{self.device_uuid}"
        self.snapshot_uri = f"http://127.0.0.1:{http_port}/snapshot.jpg"
        self._http.rtsp_uri = self.rtsp_uri
        self._http.snapshot_uri = self.snapshot_uri

        # UDP WS-Discovery responder (unicast target for the driver's probe).
        self._udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._udp.bind(("127.0.0.1", 0))
        discovery_port = self._udp.getsockname()[1]
        self.discovery_address = f"127.0.0.1:{discovery_port}"

        self._running = False
        self._http_thread = None
        self._udp_thread = None
        self._loop = GLib.MainLoop()
        self._loop_thread = None

    def _build_rtsp_server(self):
        # Gst.init is idempotent, so calling it per instance is safe.
        Gst.init(None)
        server = GstRtspServer.RTSPServer()
        server.set_service("0")  # let the OS pick a free port
        factory = GstRtspServer.RTSPMediaFactory()
        factory.set_launch(_RTSP_LAUNCH)
        factory.set_shared(True)
        server.get_mount_points().add_factory(f"/{self.device_uuid}", factory)
        # attach registers the server's sources on the default main context; the loop dispatches
        # them once started. get_bound_port is valid after attach.
        server.attach(None)
        return server, server.get_bound_port()

    def _probe_match(self) -> bytes:
        xml = (
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<e:Envelope xmlns:e="http://www.w3.org/2003/05/soap-envelope" '
            'xmlns:w="http://schemas.xmlsoap.org/ws/2004/08/addressing" '
            'xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery">'
            "<e:Body><d:ProbeMatches><d:ProbeMatch>"
            "<w:EndpointReference><w:Address>urn:uuid:" + self.device_uuid + "</w:Address></w:EndpointReference>"
            "<d:Types>dn:NetworkVideoTransmitter</d:Types>"
            "<d:XAddrs>" + self.service_url + "</d:XAddrs>"
            "</d:ProbeMatch></d:ProbeMatches></e:Body></e:Envelope>"
        )
        return xml.encode("utf-8")

    def _udp_loop(self):
        self._udp.settimeout(0.5)
        while self._running:
            try:
                data, addr = self._udp.recvfrom(8192)
            except socket.timeout:
                continue
            except OSError:
                break

            if b"Probe" in data:
                self._udp.sendto(self._probe_match(), addr)

    def start(self):
        self._running = True
        self._http_thread = threading.Thread(target=self._http.serve_forever, daemon=True)
        self._http_thread.start()
        self._udp_thread = threading.Thread(target=self._udp_loop, daemon=True)
        self._udp_thread.start()
        # The RTSP server needs a running GLib main loop to accept connections and stream media.
        self._loop_thread = threading.Thread(target=self._loop.run, daemon=True)
        self._loop_thread.start()

    def stop(self):
        self._running = False
        self._http.shutdown()
        self._http.server_close()
        self._udp.close()
        self._loop.quit()

        for thread in (self._http_thread, self._udp_thread, self._loop_thread):
            if thread is not None:
                thread.join(timeout=5)


@pytest.fixture
def onvif_camera():
    """Start a mock ONVIF camera for the duration of a test."""
    server = OnvifCameraServer()
    server.start()
    try:
        yield server
    finally:
        server.stop()
