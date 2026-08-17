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

#include "OnvifXml.h"

// xmlHelper is a C library that also pulls in libxml2's parser; include libxml2 here (as C++) first
// so those headers are processed before the extern "C" block below gives xmlHelper C linkage.
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdlib>

extern "C" {
#include <xmlHelper/xmlHelper.h>
}

namespace barton
{
    namespace onvif
    {

        xmlNode *OnvifXmlFindFirstByLocalName(xmlNode *node, const char *localName)
        {
            // findChildNode compares libxml2's local element name (namespace prefix ignored) and
            // recurses; it searches descendants, which is how every ONVIF caller uses this (the
            // passed node is always a container, never itself the sought element).
            return node != nullptr ? findChildNode(node, localName, true) : nullptr;
        }

        void OnvifXmlCollectByLocalName(xmlNode *node, const char *localName, std::vector<xmlNode *> &out)
        {
            // xmlHelper has no collect-all primitive, so this one stays custom.
            for (xmlNode *cur = node; cur != nullptr; cur = cur->next)
            {
                if (cur->type == XML_ELEMENT_NODE && cur->name != nullptr &&
                    xmlStrcmp(cur->name, reinterpret_cast<const xmlChar *>(localName)) == 0)
                {
                    out.push_back(cur);
                }

                OnvifXmlCollectByLocalName(cur->children, localName, out);
            }
        }

        std::string OnvifXmlElementText(xmlNode *node)
        {
            char *content = getXmlNodeContentsAsString(node, nullptr);
            std::string result = content != nullptr ? content : "";
            free(content);

            return result;
        }

        std::string OnvifXmlElementAttr(xmlNode *node, const char *attrName)
        {
            char *value = getXmlNodeAttributeAsString(node, attrName, nullptr);
            std::string result = value != nullptr ? value : "";
            free(value);

            return result;
        }

        std::string OnvifXmlFindText(xmlNode *root, const char *localName)
        {
            return OnvifXmlElementText(OnvifXmlFindFirstByLocalName(root, localName));
        }

    } // namespace onvif
} // namespace barton
