//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2024 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mkoch201 on 4/18/19.
//

#ifndef ZILKER_RESOURCEMODES_H
#define ZILKER_RESOURCEMODES_H

// Resource can be read
#define RESOURCE_MODE_READABLE        (1u << 0u)

// Resource can be written
#define RESOURCE_MODE_WRITEABLE       (1u << 1u)

// Resource can be executed
#define RESOURCE_MODE_EXECUTABLE      (1u << 2u)

// Resource can change dynamically
#define RESOURCE_MODE_DYNAMIC_CAPABLE (1u << 3u)

// Resource dynamic behavior is enabled
#define RESOURCE_MODE_DYNAMIC         (1u << 4u)

// Resources emit events upon change if set (this mode bit can be changed via API to start/stop resource changed events)
#define RESOURCE_MODE_EMIT_EVENTS     (1u << 5u)

// Resources that are non-critical and are ok to save upon the next save operation.
//  (changes to these resources could be lost under error conditions))
#define RESOURCE_MODE_LAZY_SAVE_NEXT  (1u << 6u)

// Resources start shouldn't be stored in clear text (user names, passwords, etc)
#define RESOURCE_MODE_SENSITIVE       (1u << 7u)

// Convenience mask combinations
#define RESOURCE_MODE_READWRITEABLE   (RESOURCE_MODE_READABLE | RESOURCE_MODE_WRITEABLE)

#endif // ZILKER_RESOURCEMODES_H
