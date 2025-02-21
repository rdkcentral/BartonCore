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

#
# Created by Christian Leithner on 1/23/2025.
#

from __future__ import annotations
import os
import select
import socket
import threading
from typing import Final, override
import logging

import pytest

from testing.mocks.zhal.requests.request_deserializer import RequestDeserializer
from testing.mocks.zhal.requests.request_handler import RequestHandler


UDP_READ_MAX_SIZE: Final[int] = 65535
READ_TIMEOUT_SECOND: Final[int] = 5
REQUEST_RECEIVED_RESULT: Final[bytes] = '{"resultCode": 0}'.encode()

logger = logging.getLogger(__name__)


class RequestReceiver:
    """
    RequestReceiver listens for requests from the Barton library runtime.
    RequestReceiver is not thread-safe.

    Attributes:
        _port (int): The ipc port to listen on.
        _request_handler (RequestHandler): The mock handler for requests.
        _running (bool): A flag to indicate if the thread is running.
        _reciever_thread (ReceiverThread): The thread that listens for requests.
        _reciever_thread_event (threading.Event): An event condition for the receiver thread to know to stop
        _interrupt_fd_write (int): The file descriptor to write to to interrupt the receiver thread.
    """

    _port: int
    _request_handler: RequestHandler
    _reciever_thread: ReceiverThread
    _reciever_thread_event: threading.Event
    _running: bool
    _interrupt_fd_write: int

    def __init__(self, port: int, request_handler: RequestHandler):
        self._port = port
        self._request_handler = request_handler
        self._running = False
        self._reciever_thread_event = threading.Event()

    @property
    def port(self):
        return self._port

    @property
    def running(self):
        return self._running

    def start(self):
        if self._running:
            logger.error("RequestReceiver already running.")
            return
        else:
            self._interrupt_fd_write, read_fd = os.pipe()
            self._reciever_thread_event.clear()

            self._reciever_thread = self.ReceiverThread(
                self._port, self._reciever_thread_event, read_fd, self._request_handler
            )
            self._reciever_thread.start()
            self._running = True

    def stop(self):
        if not self._running:
            logger.error("RequestReceiver not running.")
            return
        else:
            self._reciever_thread_event.set()
            os.write(self._interrupt_fd_write, b"1")
            self._reciever_thread.join()
            self._running = False

    class ReceiverThread(threading.Thread):
        """
        ReceiverThread is a thread that listens for requests from the Barton library runtime.

        Attributes:
            _port (int): The ipc port to listen on.
            _thread_event (threading.Event): An event condition used to signal this thread that it should stop.
            _interrupt_fd_read (int): The file descriptor to read from to interrupt the receiver thread.
        """

        _port: int
        _thread_event: threading.Event
        _request_handler: RequestHandler
        _interrupt_fd_read: int
        _request_deserializer: RequestDeserializer = RequestDeserializer()

        def __init__(
            self,
            port: int,
            thread_event: threading.Event,
            interrupt_fd_read: int,
            request_handler: RequestHandler,
        ):
            self._port = port
            self._thread_event = thread_event
            self._interrupt_fd_read = interrupt_fd_read
            self._request_handler = request_handler

        @property
        def port(self):
            return self._port

        @override
        def run(self):
            logger.info(f"ReceiverThread listening on port {self._port}")

            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.bind(("localhost", self._port))
                sock.setblocking(False)

                while not self._thread_event.is_set():
                    try:
                        ready_fds, _, _ = select.select(
                            [self._interrupt_fd_read, sock], [], [], READ_TIMEOUT_SECOND
                        )

                        if sock in ready_fds:
                            data, _ = sock.recvfrom(UDP_READ_MAX_SIZE)
                            logger.debug(f"Received message: {data.decode()}")
                            request = self._request_deserializer.deserialize(data)

                            if request:
                                sock.send(REQUEST_RECEIVED_RESULT)
                                response = request.process(self._request_handler)
                                if response:
                                    response.send()
                                else:
                                    raise ValueError("Request process failed.")
                            else:
                                raise ValueError("Request deserialization failed.")

                    except socket.error as e:
                        logger.exception("Socket error while waiting for requests.")


@pytest.fixture
def request_receiver(request):
    request_receiver = RequestReceiver(
        request.param["port"], request.param["request_handler"]
    )
    try:
        yield request_receiver
    finally:
        pass
