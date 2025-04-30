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
# Created by Christian Leithner on 12/16/2024.
#

import sys
import os

def main(template_path, output_path, replacement_values):
    with open(template_path, "r") as template_file:
        content = template_file.read()

    for key, value in replacement_values.items():
        replacement_key = "$({key})".format(key=key)
        if not value.isnumeric():
            value = '"{value}"'.format(value=value)
        content = content.replace(replacement_key, value)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, "w") as output_file:
        output_file.write(content)


if __name__ == "__main__":
    template_path = sys.argv[1]
    output_path = sys.argv[2]
    replacements = dict()
    for i in range(3, len(sys.argv)):
        split = sys.argv[i].split("=")
        replacements[split[0]] = split[1]

    main(template_path, output_path, replacements)
