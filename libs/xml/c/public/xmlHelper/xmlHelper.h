//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * xmlHelper.h
 *
 * Helper routines to improve the interaction with libxml2.
 *
 * Author: gfaulkner, jelderton
 *-----------------------------------------------*/

#ifndef IC_XMLHELPER_H
#define IC_XMLHELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/*-------------------------------*
 *
 *  XML Node Operations
 *
 *-------------------------------*/

/**
 * @brief Get the line number of the supplied node.
 * @param node
 * @return uint32_t the line number on success, or 0 on failure
 */
uint32_t getXmlNodeLineNumber(xmlNodePtr node);

/*
 * read the contents of an XML Node, and interpret as an integer
 */
int32_t getXmlNodeContentsAsInt(xmlNodePtr node, int32_t defValue);

/*
 * read the contents of an XML Node, and interpret as an unsigned integer
 */
uint32_t getXmlNodeContentsAsUnsignedInt(xmlNodePtr node, uint32_t defValue);

/*
 * read the contents of an XML Node, and interpret as an unsigned long long
 */
uint64_t getXmlNodeContentsAsUnsignedLongLong(xmlNodePtr node, uint64_t defValue);

/*
 * read the contents of an XML Node, and interpret as a boolean
 * returns 1 for TRUE, 0 for FALSE (or defValue if node is missing/empty)
 */
bool getXmlNodeContentsAsBoolean(xmlNodePtr node, bool defValue);

/*
 * inverse of 'getAsBoolean' since the value is interpreted more then the others
 */
void setXmlNodeContentsAsBoolean(xmlNodePtr node, bool value);

/*
 * read the contents of an XML Node as a string
 * if the return is not NULL, the caller must free() the memory
 */
char *getXmlNodeContentsAsString(xmlNodePtr node, char *defValue);

/*
 * helper function to create a new node, add it
 * to the 'parentNode', and set the 'contents' (if not empty)
 * ex:
 *   <resend>true</resend>
 *
 * returns the newly created node
 */
xmlNodePtr appendNewStringNode(xmlNodePtr parentNode, const char *nodeName, const char *contents);

/*
 * helper function to find a node named 'search' as a child of 'base'
 * return NULL if not able to be located
 */
xmlNodePtr findChildNode(xmlNodePtr base, const char *search, bool recurse);


/*-------------------------------*
 *
 *  XML Attribute Operations
 *
 *-------------------------------*/

/*
 * read the contents of an attribute, and interpret as an integer
 */
int32_t getXmlNodeAttributeAsInt(xmlNodePtr node, const char *attributeName, int32_t defValue);

/*
 * set an integer attribute to the supplied node
 */
void setXmlNodeAttributeAsInt(xmlNodePtr node, const char *attributeName, int32_t value);

/*
 * read the contents of an attribute, and interpret as an unsigned integer
 */
uint32_t getXmlNodeAttributeAsUnsignedInt(xmlNodePtr node, const char *attributeName, uint32_t defValue);

/*
 * read the contents of an attribute, and interpret as an unsigned long long
 */
uint64_t getXmlNodeAttributeAsUnsignedLongLong(xmlNodePtr node, const char *attributeName, uint64_t defValue);

/*
 * look for and read the contents of an attribute from an XML Node as a boolean
 * returns 1 for TRUE, 0 for FALSE (or defValue if node is missing/empty)
 */
bool getXmlNodeAttributeAsBoolean(xmlNodePtr node, const char *attributeName, bool defValue);

/*
 * inverse of 'getAsBoolean' since the value is interpreted more then the others
 */
void setXmlNodeAttributeAsBoolean(xmlNodePtr node, const char *attributeName, bool value);

/*
 * look for and read the contents of an attribute from an XML Node as a string
 * if the return is not NULL, the caller must free() the memory
 */
char *getXmlNodeAttributeAsString(xmlNodePtr node, const char *attributeName, char *defValue);


#endif // IC_XMLHELPER_H

