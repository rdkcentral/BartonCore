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
inotify-based file wait utility for integration tests.

Uses Linux inotify to efficiently wait for a file to be written (via rename)
without polling or sleeping.
"""

import ctypes
import ctypes.util
import os
import select
import struct
import time
from pathlib import Path

# inotify event masks (from <sys/inotify.h>)
IN_CLOSE_WRITE = 0x00000008
IN_MOVED_TO = 0x00000080
IN_CREATE = 0x00000100

_WATCH_MASK = IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE

# struct inotify_event { int wd; uint32_t mask; uint32_t cookie; uint32_t len; char name[]; }
_EVENT_HEADER_SIZE = struct.calcsize("iIII")

_libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)


def _check_errno(result, func, args):
    if result == -1:
        errno = ctypes.get_errno()
        raise OSError(errno, os.strerror(errno))

    return result


_libc.inotify_init1.restype = ctypes.c_int
_libc.inotify_init1.errcheck = _check_errno
_libc.inotify_add_watch.restype = ctypes.c_int
_libc.inotify_add_watch.errcheck = _check_errno

_IN_CLOEXEC = 0x80000


def wait_for_file_content(file_path, expected_content, timeout):
    """Wait for *file_path* to contain *expected_content* using inotify.

    Watches the parent directory for filesystem events on the target file:

    - ``IN_MOVED_TO`` — covers the download pattern (write to .tmp, then
      ``rename()`` to the final path)
    - ``IN_CLOSE_WRITE`` — covers direct writes (file opened, written, closed)
    - ``IN_CREATE`` — covers file creation from scratch

    Returns True if the content matched within *timeout* seconds, False
    otherwise.
    """
    path = Path(file_path)
    parent = str(path.parent)
    target_name = path.name

    # Check if already correct before setting up inotify
    if path.exists() and path.read_bytes() == expected_content:
        return True

    fd = _libc.inotify_init1(_IN_CLOEXEC)

    try:
        _libc.inotify_add_watch(
            fd, parent.encode(), _WATCH_MASK
        )

        start = time.monotonic()
        remaining = timeout

        while remaining > 0:
            ready, _, _ = select.select([fd], [], [], remaining)

            if not ready:
                break

            buf = os.read(fd, 4096)
            offset = 0

            while offset < len(buf):
                _, mask, _, name_len = struct.unpack_from("iIII", buf, offset)
                offset += _EVENT_HEADER_SIZE
                name = buf[offset : offset + name_len].rstrip(b"\x00").decode()
                offset += name_len

                if name == target_name and (mask & _WATCH_MASK):
                    if path.exists() and path.read_bytes() == expected_content:
                        return True

            remaining = timeout - (time.monotonic() - start)
    finally:
        os.close(fd)

    # Final check in case the event arrived just before timeout
    return path.exists() and path.read_bytes() == expected_content
