// ------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2025 Comcast Cable Communications Management, LLC
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
// ------------------------------ tabstop = 4 ----------------------------------

//
// Created by Raiyan Chowdhury on 3/28/2025.
//

#pragma once

#include <glib-object.h>
#include <provider/device-service-property-provider.h>

G_BEGIN_DECLS

/*
 * Mock implementation of the device service property provider interface.
 * This mock allows one to simulate property storage and retrieval for testing purposes.
 */
#define B_DEVICE_SERVICE_MOCK_PROPERTY_PROVIDER_TYPE (b_device_service_mock_property_provider_get_type())
G_DECLARE_FINAL_TYPE(BDeviceServiceMockPropertyProvider,
                     b_device_service_mock_property_provider,
                     B_DEVICE_SERVICE,
                     MOCK_PROPERTY_PROVIDER,
                     GObject);

/**
 * b_device_service_mock_property_provider_new:
 *
 * @brief Creates a new instance of the mock property provider.
 *
 * Returns: (transfer full): A new instance of the mock property provider.
 */
BDeviceServiceMockPropertyProvider *b_device_service_mock_property_provider_new(void);

G_END_DECLS
