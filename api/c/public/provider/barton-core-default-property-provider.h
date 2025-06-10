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

/*
 * Created by Christian Leithner on 8/29/2024.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * An empty default implementation of the property provider interface. Only exists so that
 * device service can use one if a client fails to provide an implementation.
 */
#define B_CORE_DEFAULT_PROPERTY_PROVIDER_TYPE (b_core_default_property_provider_get_type())
G_DECLARE_FINAL_TYPE(BCoreDefaultPropertyProvider,
                     b_core_default_property_provider,
                     B_CORE,
                     DEFAULT_PROPERTY_PROVIDER,
                     GObject);

/**
 * b_core_default_property_provider_new:
 *
 * @brief Creates a new instance of the default property provider.
 *
 * Returns: (transfer full): A new instance of the default property provider.
 */
BCoreDefaultPropertyProvider *b_core_default_property_provider_new(void);

G_END_DECLS
