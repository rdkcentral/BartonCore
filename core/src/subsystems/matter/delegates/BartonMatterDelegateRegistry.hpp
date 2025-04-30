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
// Created by Christian Leithner on 3/31/2025.
//

#pragma once

#include "BartonOperationalCredentialsDelegate.hpp"
#include "glib.h"

namespace barton
{

    /**
    * This class provides delegates to the Matter subsystem. It abstracts different implementations
    * of Matter delegates. Implementations should self register via this class' registration
    * methods.
    */
    class BartonMatterDelegateRegistry final
    {
        public:
            static BartonMatterDelegateRegistry &Instance()
            {
                static BartonMatterDelegateRegistry instance;
                return instance;
            }

            template <typename T>
            typename std::enable_if<std::is_base_of<BartonOperationalCredentialsDelegate, T>::value, bool>::type
            RegisterBartonOperationalCredentialDelegate(std::shared_ptr<T> delegate)
            {
                g_return_val_if_fail(delegate, false);
                g_return_val_if_fail(!operationalCredentialsDelegate, false);

                operationalCredentialsDelegate = std::static_pointer_cast<BartonOperationalCredentialsDelegate>(delegate);
                return true;
            }

            std::shared_ptr<BartonOperationalCredentialsDelegate> GetBartonOperationalCredentialDelegate()
            {
                return operationalCredentialsDelegate;
            }

        private:
            BartonMatterDelegateRegistry() = default;
            ~BartonMatterDelegateRegistry() = default;

            std::shared_ptr<BartonOperationalCredentialsDelegate> operationalCredentialsDelegate;


    };
}
