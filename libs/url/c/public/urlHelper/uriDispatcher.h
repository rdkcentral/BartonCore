//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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
 * uriDispatcher.h
 *
 * Use a "trie" data-structure as a mechanism to breakdown URI paths,
 * providing a way to process URIs that can have wildcard variables and
 * callback-handlers.  Primarily created to support REST calls, which is
 * why this is tailored for those URIs.  Some example URIs that this can
 * process include (wildcards are depicted with surrounding [] brackets):
 *
 *  1 - /icontrol/sites/[siteId]/network/cameras/[cameraId]
 *  2 - /icontrol/sites/[siteId]/network/rules/[ruleId]
 *  3 - /icontrol/sites/[siteId]/network/zdif/discover
 *
 * Before this can process URIs, it first must be created and primed with
 * callback-handlers for the various templates (the examples above).  As
 * these handlers are inserted into the structure, it will build an internal
 * trie.  Using our examples above, the trie would look something like:
 *
 *         icontrol
 *            |
 *          sites
 *            |
 *         [*var*]
 *            |
 *         network
 *          / | \
 *         /  |  \
 *       cam  |  zdif
 *       /   rules \
 *      /     |     \
 *   [*var*]  |  discover
 *            |
 *         [*var*]
 *
 * limitations and conventions:
 *  - each node of the tree can have 0-n children
 *  - each node can only have 1 "variable" child
 *  - each node can be a callback-handler notification
 *  - if a node has a "variable" child, will be stored as the first child (for optimization)
 *
 * Author: jelderton -  9/2/16.
 *-----------------------------------------------*/

#ifndef FLEXCORE_URI_DISPATCHER_H
#define FLEXCORE_URI_DISPATCHER_H

#include <icTypes/icStringHashMap.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * function signature to process the URI.
 *
 * @param fullUri - the original URI as supplied (ex: /icontrol/sites/1234/network/cameras/1111)
 * @param nodePath - all 'variable' nodes in key=value format (ex: siteId=1234, cameraId=1111)
 * @param arg - optional object to be interpreted by the handler (ex: rest parser information)
 */
typedef void (*handleUriPath)(char *fullUri, icStringHashMap *variables, void *arg);

/*
 * abstract object that represents our dispatcher
 */
typedef struct _uriDispatcher uriDispatcher;

/*
 * container of a 'function handler' and a 'description'
 */
typedef struct _uriHandlerContainer
{
    handleUriPath handler;
    char *description;
} uriHandlerContainer;


/*
 * return codes
 */
typedef enum
{
    URI_DISPATCH_ADD_OK,
    URI_DISPATCH_INVALID,           // input args are invalid
    URI_DISPATCH_DUP_VAR,           // conflict with a variable at the same segment (with a different name or directive)
    URI_DISPATCH_DUP_HANDLER,       // conflict with a callback already registered
    URI_DISPATCH_UNKNOWN_DIRECTIVE, // directive found that could not be resolved
} uriDispatchAddResult;

/*
 * create a URI dispatcher
 */
uriDispatcher *uriDispatcherCreate();

/*
 * destroy a URI dispatcher
 */
void uriDispatcherDestroy(uriDispatcher *dispatcher);

/*
 * register a new handler for a URI template into the supplied 'dispatcher'.
 * uses the same convention as our examples above, were wildcard variables
 * are surrounded by [] brackets.  returns the success or reason for failure.
 *
 * @param dispatcher - object to add the handler to
 * @param uriTemplate - ex:  /icontrol/sites/[siteId]/network/cameras/[cameraId]
 * @param description - label for this handler (for debugging/logging)
 * @param handler - callback to register for this template
 */
uriDispatchAddResult
registerUriHandler(uriDispatcher *dispatcher, const char *uriTemplate, const char *description, handleUriPath handler);

/*
 * process a URI and return the handler/description that should be used.
 * will return NULL if no match was located.  allows caller to
 * then call the handler as they see fit (threaded, in/out arg, etc).
 * caller must free the returned object, see 'uriHandlerContainerDestroy()'
 *
 * @param dispatcher - object to find the handler from
 * @param uri - string to parse/process
 * @param valuesMap - hashmap to insert variable names/values into.  used as 'variable' arg to handler.
 *                    caller should free contents of the hash when destroying it
 */
uriHandlerContainer *locateUriHandler(uriDispatcher *dispatcher, const char *uri, icStringHashMap *valuesMap);

/*
 * destroy a uriHandlerContainer (from locateUriHandler)
 */
void uriHandlerContainerDestroy(uriHandlerContainer *container);

/*
 * Callback function for a registered directive
 */
typedef bool (*UriDispatcherDirectiveHandler)(char *input, char **output, void *context);

/*
 * Register a directive.  Directives are used to manipulate a variable in a uriTemplate.
 * ex: icontrol/sites/[siteId]/network/lights/[lightId#stripPremise]
 * The stripPremise directive can be registered to accept the passed lightId and strip off the premise prefix.
 *
 * @param dispatcher the dispatcher to register for
 * @param directiveName the directive name.  This should NOT include the # which signifies a directive
 * @param handler the handler callback
 * @param context the callback context
 * @return true if successfully registered
 */
bool uriDispatcherRegisterDirective(uriDispatcher *dispatcher,
                                    const char *directiveName,
                                    UriDispatcherDirectiveHandler handler,
                                    void *context);


#endif // FLEXCORE_URI_DISPATCHER_H
