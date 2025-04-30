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

/*-----------------------------------------------
 * xmlHelper.c
 *
 * Helper routines to improve the interaction with libxml2.
 *
 * Author: gfaulkner, jelderton
 *-----------------------------------------------*/

#include <inttypes.h>
#include <libxml/parser.h>
#include <stdint.h>
#include <string.h>

#include <icUtil/stringUtils.h>
#include <xmlHelper/xmlHelper.h>

/*
 * read the contents of an XML Node, and interpret as an integer
 */
int32_t getXmlNodeContentsAsInt(xmlNodePtr node, int32_t defValue)
{
    int32_t retVal = defValue;

    // get contents from node
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlNodeGetContent(node);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            stringToInt32(contents, &retVal);
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * read the contents of an XML Node, and interpret as an unsigned integer
 */
uint32_t getXmlNodeContentsAsUnsignedInt(xmlNodePtr node, uint32_t defValue)
{
    uint32_t retVal = defValue;

    // get contents from node
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlNodeGetContent(node);

        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            stringToUint32(contents, &retVal);
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * read the contents of an XML Node, and interpret as an unsigned long long
 */
uint64_t getXmlNodeContentsAsUnsignedLongLong(xmlNodePtr node, uint64_t defValue)
{
    uint64_t retVal = defValue;

    // get contents from node
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlNodeGetContent(node);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            stringToUint64(contents, &retVal);
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * read the contents of an XML Node, and interpret as a boolean
 * returns 1 for TRUE, 0 for FALSE (or defValue if node is missing/empty)
 */
bool getXmlNodeContentsAsBoolean(xmlNodePtr node, bool defValue)
{
    bool retVal = defValue;

    // get contents from node
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlNodeGetContent(node);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            if (strcasecmp(contents, "true") == 0 || strcmp(contents, "1") == 0)
            {
                retVal = true;
            }
            else
            {
                retVal = false;
            }
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * inverse of 'getAsBoolean' since the value is interpreted more then the others
 */
void setXmlNodeContentsAsBoolean(xmlNodePtr node, bool value)
{
    char temp[6];

    // make string representation
    //
    if (value == false)
    {
        sprintf(temp, "false");
    }
    else
    {
        sprintf(temp, "true");
    }

    // apply to the node
    //
    xmlNodeSetContent(node, BAD_CAST temp);
}

/*
 * read the contents of an XML Node as a string
 * if the return is not NULL, the caller must free() the memory
 */
char *getXmlNodeContentsAsString(xmlNodePtr node, char *defValue)
{
    char *retVal = NULL;

    // get contents from node
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlNodeGetContent(node);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            retVal = strdup(contents);
            xmlFree(contents);
        }
    }

    // return string read from node
    //
    if (retVal != NULL)
    {
        return retVal;
    }

    // return the default value, but duplicate it
    // so the caller is forced to free the result
    // regardless of successful XML node or not
    //
    if (defValue == NULL)
    {
        return NULL;
    }
    return strdup(defValue);
}

/*
 * helper function to create a new node, add it
 * to the 'parentNode', and set the 'contents' (if not empty)
 * ex:
 *   <resend>true</resend>
 *
 * returns the newly created node
 */
xmlNodePtr appendNewStringNode(xmlNodePtr parentNode, const char *nodeName, const char *contents)
{
    // create the new node
    //
    xmlNodePtr tmp = NULL;

    // if 'contents' are not emptu. set those into this new node
    //
    if (contents != NULL)
    {

        tmp = xmlNewTextChild(parentNode, NULL, BAD_CAST nodeName, BAD_CAST contents);
    }
    else
    {
        // add empty node to the parent
        //
        tmp = xmlNewNode(NULL, BAD_CAST nodeName);
        xmlAddChild(parentNode, tmp);
    }


    return tmp;
}

/*
 * helper function to find a node named 'search' as a child of 'base'
 * return NULL if not able to be located
 */
xmlNodePtr findChildNode(xmlNodePtr base, const char *search, bool recurse)
{
    // loop through the children of 'base'
    //
    xmlNodePtr loopNode, currNode;
    loopNode = base->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        // see if <internal-server-error>
        //
        if (strcmp((const char *) currNode->name, search) == 0)
        {
            // found match
            //
            return currNode;
        }
        else if (recurse == true)
        {
            // ask this node to recursively look
            //
            xmlNodePtr maybe = findChildNode(currNode, search, recurse);
            if (maybe != NULL)
            {
                return maybe;
            }
        }
    }

    return NULL;
}

uint32_t getXmlNodeLineNumber(xmlNodePtr node)
{
    uint32_t lineNo = 0;

    if (node != NULL)
    {
        long res = xmlGetLineNo(node);

        if (res > 0)
        {
            lineNo = res;
        }
    }

    return lineNo;
}

/*
 * read the contents of an attribute, and interpret as an integer
 */
int32_t getXmlNodeAttributeAsInt(xmlNodePtr node, const char *attributeName, int32_t defValue)
{
    int32_t retVal = defValue;

    // search for this attribute from 'node'
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlGetProp(node, BAD_CAST attributeName);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            stringToInt32(contents, &retVal);
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * set an integer attribute to the supplied node
 */
void setXmlNodeAttributeAsInt(xmlNodePtr node, const char *attributeName, int32_t value)
{
    // max value would be 2147483647 + possible minus sign + null char + extra to be certain
    //
    char temp[13];
    sprintf(temp, "%" PRIi32, value);

    // apply the attribute to the node
    //
    xmlSetProp(node, BAD_CAST attributeName, BAD_CAST temp);
}

/*
 * read the contents of an attribute, and interpret as an unsigned integer
 */
uint32_t getXmlNodeAttributeAsUnsignedInt(xmlNodePtr node, const char *attributeName, uint32_t defValue)
{
    uint32_t retVal = defValue;

    // search for this attribute from 'node'
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlGetProp(node, BAD_CAST attributeName);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            stringToUint32(contents, &retVal);
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * read the contents of an attribute, and interpret as an unsigned long long
 */
uint64_t getXmlNodeAttributeAsUnsignedLongLong(xmlNodePtr node, const char *attributeName, uint64_t defValue)
{
    uint64_t retVal = defValue;

    // search for this attribute from 'node'
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlGetProp(node, BAD_CAST attributeName);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            stringToUint64(contents, &retVal);
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * look for and read the contents of an attribute from an XML Node as a boolean
 * returns 1 for TRUE, 0 for FALSE (or defValue if node is missing/empty)
 */
bool getXmlNodeAttributeAsBoolean(xmlNodePtr node, const char *attributeName, bool defValue)
{
    bool retVal = defValue;

    // search for this attribute from 'node'
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlGetProp(node, BAD_CAST attributeName);
        if (contents != NULL)
        {
            // convert and release memory returned from libxml2
            //
            if (strcasecmp(contents, "true") == 0)
            {
                retVal = true;
            }
            else
            {
                retVal = false;
            }
            xmlFree(contents);
        }
    }

    return retVal;
}

/*
 * inverse of 'getAsBoolean' since the value is interpreted more then the others
 */
void setXmlNodeAttributeAsBoolean(xmlNodePtr node, const char *attributeName, bool value)
{
    char temp[6];

    // make string representation
    //
    if (value == false)
    {
        sprintf(temp, "false");
    }
    else
    {
        sprintf(temp, "true");
    }

    // apply the attribute to the node
    //
    xmlSetProp(node, BAD_CAST attributeName, BAD_CAST temp);
}

/*
 * look for and read the contents of an attribute from an XML Node as a string
 * if the return is not NULL, the caller must free() the memory
 */
char *getXmlNodeAttributeAsString(xmlNodePtr node, const char *attributeName, char *defValue)
{
    char *retVal = NULL;

    // search for this attribute from 'node'
    //
    if (node != NULL)
    {
        char *contents = (char *) xmlGetProp(node, BAD_CAST attributeName);
        if (contents != NULL)
        {
            // duplicate the string
            //
            retVal = strdup(contents);
            xmlFree(contents);
        }
    }

    // return attribute string read from node if valid
    //
    if (retVal != NULL)
    {
        return retVal;
    }

    // return the default value, but duplicate it
    // so the caller is forced to free the result
    // regardless of successful XML node or not
    //
    if (defValue == NULL)
    {
        return NULL;
    }
    return strdup(defValue);
}
