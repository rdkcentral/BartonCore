//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
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
// Small std::string-friendly XML helpers shared by the ONVIF SOAP client and WS-Discovery. Find,
// text, and attribute lookups delegate to bartoncommon's xmlHelper (which matches by libxml2 local
// name, ignoring namespace prefix -- exactly what ONVIF's prefix-variant responses need); only the
// collect-all-by-local-name helper, which bartoncommon lacks, is implemented here.
//

#pragma once

#include <libxml/tree.h>
#include <string>
#include <vector>

namespace barton
{
    namespace onvif
    {

        // Recursively find the first element whose local name matches (namespace ignored), or nullptr.
        xmlNode *OnvifXmlFindFirstByLocalName(xmlNode *node, const char *localName);

        // Recursively collect every element whose local name matches (namespace ignored).
        void OnvifXmlCollectByLocalName(xmlNode *node, const char *localName, std::vector<xmlNode *> &out);

        // The concatenated text content of an element (empty string if node is nullptr).
        std::string OnvifXmlElementText(xmlNode *node);

        // The value of an attribute on an element (empty string if absent).
        std::string OnvifXmlElementAttr(xmlNode *node, const char *attrName);

        // Convenience: text of the first descendant with the given local name, relative to root.
        std::string OnvifXmlFindText(xmlNode *root, const char *localName);

    } // namespace onvif
} // namespace barton
