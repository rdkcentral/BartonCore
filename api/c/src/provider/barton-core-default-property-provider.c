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

#include "provider/barton-core-default-property-provider.h"
#include "provider/barton-core-property-provider.h"

struct _BCoreDefaultPropertyProvider
{
    GObject parent_instance;
};

static void b_core_property_provider_interface_init(BCorePropertyProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE(BCoreDefaultPropertyProvider,
                        b_core_default_property_provider,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(B_CORE_PROPERTY_PROVIDER_TYPE,
                                              b_core_default_property_provider_init))

static void b_core_property_provider_interface_init(BCorePropertyProviderInterface *iface)
{
    iface->get_property = NULL;
}

static void b_core_default_property_provider_init(BCoreDefaultPropertyProvider *self)
{
    // Nothing to do here
}

static void b_core_default_property_provider_class_init(BCoreDefaultPropertyProviderClass *klass)
{
    // Nothing to do here
}

BCoreDefaultPropertyProvider *b_core_default_property_provider_new(void)
{
    return g_object_new(B_CORE_DEFAULT_PROPERTY_PROVIDER_TYPE, NULL);
}
