#!/bin/bash
# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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
# Created by Kevin Funderburg on 12/17/2024
#

# This script is used to setup VSC related settings for the project after the container
# has been created.

# set -x
set -e

# This environment file is specifically to set up the Python environment for VSC.
# https://code.visualstudio.com/docs/python/environments#_environment-variable-definitions-file
PYTHON_ENV_FILE=".devcontainer/vsc-python.env"

# Ensure the env file exists (VS Code reads it via python.envFile setting).
# LD_PRELOAD is no longer written here — see sitecustomize.py below for the
# ASAN fix. The env file is kept for any future Python-specific env vars.
touch "$PYTHON_ENV_FILE"

# Install a sitecustomize.py that sets ASAN_OPTIONS at Python interpreter startup.
# This guarantees the option is set regardless of how the process is spawned (VS Code
# extensions, terminals, subprocess, etc.). Without this, processes that load
# ASAN-instrumented libBartonCore.so via GObject introspection crash because ASAN
# kills them when the runtime isn't first in the link order.
# NOTE: We append to the existing stdlib sitecustomize.py (Ubuntu's apport hook)
# because Python only loads one, and the stdlib location takes precedence over
# dist-packages.
SITE_CUSTOMIZE="$(python3 -c "import sysconfig; print(sysconfig.get_path('stdlib'))")/sitecustomize.py"
if ! grep -q "ASAN_OPTIONS" "$SITE_CUSTOMIZE" 2>/dev/null; then
    sudo tee -a "$SITE_CUSTOMIZE" > /dev/null << 'PYEOF'

# BartonCore: ensure ASAN link-order check is disabled for GObject introspection
import os as _os
_os.environ.setdefault("ASAN_OPTIONS", "verify_asan_link_order=0")
del _os
PYEOF
fi

# Install matter.js dependencies for virtual test devices.
# The package.json pins the exact version (currently v0.16.10).
MATTERJS_DIR="testing/mocks/devices/matterjs"
if [ -f "$MATTERJS_DIR/package.json" ]; then
    npm --prefix "$MATTERJS_DIR" install
fi
