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
Integration tests for the device descriptor download and update lifecycle.
Verifies that the descriptor handler downloads allowlists and denylists
by observing HTTP requests made to the mock descriptor server.
"""

import logging

from testing.environment.descriptor_environment_orchestrator import DescriptorEnvironmentOrchestrator
from testing.helpers.inotify_file_wait import wait_for_file_content

logger = logging.getLogger(__name__)

_REPROCESSING_TIMEOUT = DescriptorEnvironmentOrchestrator.REPROCESSING_DELAY_SECS + 1


def test_initial_allowlist_download(descriptor_environment):
    """After startup, the descriptor handler should download the allowlist."""
    env = descriptor_environment
    server = env.descriptor_server

    assert env.wait_for_ready_for_pairing(timeout=10), "Timed out waiting for READY_FOR_PAIRING"

    count = server.get_request_count("AllowList.xml")
    logger.info("AllowList.xml request count after startup: %d", count)
    assert count >= 1, "Allowlist was never requested"


def test_denylist_download(descriptor_environment):
    """After startup, the descriptor handler should download the denylist."""
    env = descriptor_environment
    server = env.descriptor_server

    assert env.wait_for_ready_for_pairing(timeout=10), "Timed out waiting for READY_FOR_PAIRING"

    count = server.get_request_count("DenyList.xml")
    logger.info("DenyList.xml request count after startup: %d", count)
    assert count >= 1, "Denylist was never requested"


def test_update_detection_on_url_change(descriptor_environment):
    """Changing the allowlist URL at runtime should trigger a re-download."""
    env = descriptor_environment
    server = env.descriptor_server

    assert env.wait_for_ready_for_pairing(timeout=10), "Timed out waiting for READY_FOR_PAIRING"

    # Create a modified allowlist with different content
    server.add_file("AllowList_v2.xml", server.allowlist_content + b"\n<!-- modified -->")

    # Change the URL property to trigger a re-download via the property provider
    new_url = server.url_for("AllowList_v2.xml")
    logger.info("Changing allowlist URL to %s", new_url)
    env.get_property_provider().set_property_string(
        "deviceDescriptor.whitelist.url.override", new_url
    )

    assert server.wait_for_request_count("AllowList_v2.xml", 1, timeout=_REPROCESSING_TIMEOUT), \
        "New allowlist URL was never requested after URL change"
    logger.info(
        "AllowList_v2.xml downloaded after URL change (count: %d)",
        server.get_request_count("AllowList_v2.xml"),
    )


def test_no_redownload_when_unchanged(descriptor_environment):
    """When the file hasn't changed, re-triggering should not cause another download."""
    env = descriptor_environment
    server = env.descriptor_server

    assert env.wait_for_ready_for_pairing(timeout=10), "Timed out waiting for READY_FOR_PAIRING"

    initial_request_count = server.get_request_count("AllowList.xml")
    logger.info(
        "AllowList.xml request count before re-check: %d", initial_request_count
    )

    # Re-set the same URL to trigger a re-check via the property provider
    logger.info("Re-setting same allowlist URL to trigger re-check")
    env.get_property_provider().set_property_string(
        "deviceDescriptor.whitelist.url.override", server.allowlist_url
    )

    # Wait for an extra request that should never come — the wait should time out
    did_increase = server.wait_for_request_count(
        "AllowList.xml", initial_request_count + 1, timeout=_REPROCESSING_TIMEOUT
    )
    assert not did_increase, "AllowList was re-downloaded despite being unchanged"


def test_redownload_on_local_file_tampering(descriptor_environment):
    """Modifying the local AllowList.xml should cause a re-download when a re-check is triggered."""
    env = descriptor_environment
    server = env.descriptor_server

    assert env.wait_for_ready_for_pairing(timeout=10), "Timed out waiting for READY_FOR_PAIRING"

    initial_request_count = server.get_request_count("AllowList.xml")
    logger.info(
        "AllowList.xml request count before tampering: %d", initial_request_count
    )

    # Tamper with the locally downloaded AllowList.xml so its MD5 no longer matches
    local_path = env.local_allowlist_path
    assert local_path.exists(), f"Local AllowList.xml not found at {local_path}"
    original_content = local_path.read_bytes()
    local_path.write_bytes(original_content + b"\n<!-- tampered -->")
    logger.info("Tampered local AllowList.xml at %s", local_path)

    # Re-set the same URL to trigger a re-check — the handler will compute
    # an MD5 mismatch between the stored value and the tampered local file
    logger.info("Re-setting same allowlist URL to trigger MD5 mismatch detection")
    env.get_property_provider().set_property_string(
        "deviceDescriptor.whitelist.url.override", server.allowlist_url
    )

    assert server.wait_for_request_count("AllowList.xml", initial_request_count + 1, timeout=_REPROCESSING_TIMEOUT), \
        "AllowList was not re-downloaded after local file was tampered with"
    logger.info(
        "AllowList.xml re-downloaded after tampering (count: %d)",
        server.get_request_count("AllowList.xml"),
    )

    # Verify the local file was restored to the original served content.
    assert wait_for_file_content(
        local_path, server.allowlist_content, timeout=_REPROCESSING_TIMEOUT
    ), "Local AllowList.xml was not restored to the original served content after re-download"
