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
 * uriDispatcher.c
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
 * Author: jelderton -  9/1/16.
 *-----------------------------------------------*/

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>
#include <urlHelper/uriDispatcher.h>

/*
 * internal notes:
 *  - 'head' of 'trie' is not null, and not really used other then to hold children
 *  - can only have 1 variable child per node
 *  - can only have 1 handler per unique path
 *  - store 'variable' child as the first child in the list
 */
#define LOG_TAG         "uriDistpatch"
#define HEAD_LABEL      "_HEAD_"
#define URL_SEPARATOR   "/"
#define VARIABLE_BEGIN  '['
#define VARIABLE_END    ']'

/*
 * variable types
 */
typedef enum
{
    VAR_NONE,           // node is NOT a variable node
    VAR_BASIC,          // node is a variable,
    VAR_DIRECTIVE       // node is a variable that should be formatted with custom directive
} trieVarType;

/*
 * Information about a registered variable directive
 */
typedef struct
{
    char                            *directiveName; // the directive name as it appears in the uri, does NOT include #
    UriDispatcherDirectiveHandler   handler;        // callback to handle the directive
    void                            *context;       // callback context
} variableDirective;

/*
 * single node within the Trie
 */
typedef struct _trieNode
{
    char                *label;       // name of the node (or variable name)
    trieVarType         varType;      // if not NONE, use label as the variable name
    variableDirective   *directive;   // if varType is VAR_DIRECTIVE, the directive used to process the variable
    icLinkedList        *children;    // contains trieNode objects
    handleUriPath       handler;      // optional, function to call if this is the end of the path
    char                *handlerDesc; // optional description of the handler
} trieNode;

/*
 * head of the Trie (what we expose)
 */
struct _uriDispatcher
{
    trieNode      *head;                // head of the Trie
    uint16_t      size;                 // total number of nodes within this Trie
    icHashMap     *variableDirectives;  // Map of directive name -> variableDirective
};

/*
 * private functions
 */
static trieNode *createNode(char *name);
static trieNode *findChildTrieNode(trieNode *node, char *name);
static trieNode *getNodeVarChild(trieNode *node);

/*
 * create a URI dispatcher
 */
uriDispatcher *uriDispatcherCreate()
{
    // create the Trie and add an empty head node.  note that
    // don't increment the size as the head really doesn't get counted
    //
    uriDispatcher *retVal = (uriDispatcher *)calloc(sizeof(uriDispatcher), 1);
    retVal->head = createNode(HEAD_LABEL);
    retVal->variableDirectives = hashMapCreate();
    return retVal;
}

static void destroyNode(trieNode *node)
{
    free(node->label);
    free(node->handlerDesc);
    linkedListDestroy(node->children, (linkedListItemFreeFunc)destroyNode);
    free(node);
}

static void destroyDirective(variableDirective *directive)
{
    if (directive != NULL)
    {
        free(directive->directiveName);
        free(directive);
    }
}

static void destroyDirectiveEntry(void *key, void *value)
{
    // Key is pointer into the structure, so leave it alone
    (void)key;
    destroyDirective((variableDirective *)value);
}

/*
 * destroy a URI dispatcher
 */
void uriDispatcherDestroy(uriDispatcher *dispatcher)
{
    destroyNode(dispatcher->head);
    hashMapDestroy(dispatcher->variableDirectives, destroyDirectiveEntry);
    free(dispatcher);
}

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
uriDispatchAddResult registerUriHandler(uriDispatcher *dispatcher, const char *uriTemplate, const char *description, handleUriPath handler)
{
    // sanity check
    //
    if (dispatcher == NULL || uriTemplate == NULL || handler == NULL)
    {
        icLogWarn(LOG_TAG, "unable to 'register uri' to dispatcher; bad input args");
        return URI_DISPATCH_INVALID;
    }

    // create a string copy of 'uriTemplate' since strtok() doesn't like constants
    //
    char *ctx;
    char *copy = strdup(uriTemplate);
    char *tok = strtok_r(copy, URL_SEPARATOR, &ctx);

    // loop through each token, finding the corresponding child-node
    // in the trie, inserting where needed
    //
    uriDispatchAddResult retVal = URI_DISPATCH_INVALID;
    trieNode *node = dispatcher->head;
    while (tok != NULL)
    {
        // check for valid token
        //
        size_t tokLen = strlen(tok);
        if (tokLen <= 0)
        {
            icLogWarn(LOG_TAG, "unable to 'register uri %s' to dispatcher; found empty token", uriTemplate);
            retVal = URI_DISPATCH_INVALID;
            node = NULL;
            break;
        }

        // see if this token is a variable (denoted with [] brackets)
        //
        if ((tok[0] == VARIABLE_BEGIN) && (tok[tokLen-1] == VARIABLE_END))
        {
            // extract variable
            //
            char varName[64];
            strncpy(varName, tok+1, tokLen-2);
            varName[tokLen-2] = '\0';
            trieVarType variableKind = VAR_BASIC;
            variableDirective *directive = NULL;

            // see if this variable has a registered parsing directive
            //
            char *hash = 0;
            if ((hash = strchr((const char *)varName, '#')) != NULL)
            {
                // Lookup the directive
                directive = hashMapGet(dispatcher->variableDirectives, hash + 1,
                                       strlen(hash + 1));
                if (directive != NULL)
                {
                    // Track the type of variable
                    variableKind = VAR_DIRECTIVE;
                    // terminate the variable name at the #, so the it doesn't include the directive
                    *hash = '\0';
                }
                else
                {
                    icLogWarn(LOG_TAG, "Failed to find directive %s", hash+1);
                    retVal = URI_DISPATCH_UNKNOWN_DIRECTIVE;
                    node = NULL;
                    break;
                }
            }

            // see if this node already has a variable node
            //
            trieNode *varNode = getNodeVarChild(node);
            if (varNode != NULL)
            {
                // see if the variable names match
                //
                if (stringCompare(varName, varNode->label, true) == 0)
                {
                    // same var in the same place..good to go
                    //
                    node = varNode;
                }
                else
                {
                    // variable names are different, so log the error
                    //
                    icLogWarn(LOG_TAG, "unable to 'register uri %s' to dispatcher; var %s conflicts with %s", uriTemplate, varName, varNode->label);
                    retVal = URI_DISPATCH_DUP_VAR;
                    node = NULL;
                    break;
                }

                if (varNode->directive != directive)
                {
                    // directives are different, so log the error
                    //
                    icLogWarn(LOG_TAG,
                              "unable to 'register uri %s' to dispatcher; var %s directive %s conflicts with %s",
                              uriTemplate, varName, directive != NULL ? directive->directiveName : "(null)",
                              varNode->directive != NULL ? varNode->directive->directiveName : "(null)");
                    retVal = URI_DISPATCH_DUP_VAR;
                    node = NULL;
                    break;
                }
            }
            else
            {
                // no variable assigned to this node, so create it
                //
                varNode = createNode(varName);
                varNode->varType = variableKind;
                varNode->directive = directive;

                // add to the 'front' of the children list
                //
                linkedListPrepend(node->children, varNode);
                node = varNode;
            }
        }
        else
        {
            // not a 'variable', so find the child of 'node' that has
            // the same name as 'token'
            //
            trieNode *match = findChildTrieNode(node, tok);
            if (match != NULL)
            {
                // found a match, so move down the tree
                //
                node = match;
            }
            else
            {
                // not there, so create the child
                //
                match = createNode(tok);
                linkedListAppend(node->children, match);
                node = match;
            }
        }

        // move to the next token
        //
        tok = strtok_r(NULL, URL_SEPARATOR, &ctx);
    }

    // should be pointing to the last node in the trie path
    //
    if (node != NULL)
    {
        // see if we can apply the 'handler'
        //
        if (node->handler != NULL)
        {
            // duplicate handler
            //
            icLogWarn(LOG_TAG, "unable to 'register uri %s' to dispatcher; node %s already has a handler", uriTemplate, node->label);
            retVal = URI_DISPATCH_DUP_HANDLER;
        }
        else
        {
            // all good, save the handler
            //
            node->handler = handler;
            if (description != NULL)
            {
                node->handlerDesc = strdup(description);
            }
            retVal = URI_DISPATCH_ADD_OK;
        }
    }

    // cleanup
    //
    free(copy);
    return retVal;
}

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
uriHandlerContainer *locateUriHandler(uriDispatcher *dispatcher, const char *uri, icStringHashMap *valuesMap)
{
    // sanity check
    //
    if (dispatcher == NULL || uri == NULL)
    {
        icLogWarn(LOG_TAG, "unable to 'locate uri' in dispatcher; bad input args");
        return NULL;
    }

    // create a string copy of 'uri' since strtok() doesn't like constants
    //
    char *ctx;
    char *copy = strdup(uri);
    char *tok = strtok_r(copy, URL_SEPARATOR, &ctx);
    uriHandlerContainer *retVal = NULL;

    // walk the trie structure using tokens from the uri
    //
    trieNode *node = dispatcher->head;
    while (tok != NULL)
    {
        // check for valid token
        //
        if (strlen(tok) == 0)
        {
            icLogWarn(LOG_TAG, "unable to 'locate uri %s' in dispatcher; found empty token", uri);
            node = NULL;
            break;
        }

        // find the child of 'node' that has the same name as 'token'
        //
        trieNode *match = findChildTrieNode(node, tok);
        if (match != NULL)
        {
            // found a match, so move down the tree
            //
            node = match;
        }
        else
        {
            // not there, see if this node has a 'variable node'
            // (making this token part of that wildcard)
            //
            if ((match = getNodeVarChild(node)) != NULL)
            {
                // got a variable node, so use this token as the wildcard.
                //
                if (match->varType == VAR_BASIC)
                {
                    // put the token as-is into the variable map
                    //
                    stringHashMapPut(valuesMap, strdup(match->label), strdup(tok));
                }
                else if (match->varType == VAR_DIRECTIVE)
                {
                    // Lookup custom directive
                    variableDirective *directive = match->directive;
                    if (directive != NULL)
                    {
                        char *value = NULL;
                        if (directive->handler(tok, &value, directive->context) == true)
                        {
                            stringHashMapPut(valuesMap, strdup(match->label), value);
                        }
                        else
                        {
                            stringHashMapPut(valuesMap, strdup(match->label), strdup(tok));
                        }
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Failed to find directive for variable %s", match->label);
                        stringHashMapPut(valuesMap, strdup(match->label), strdup(tok));
                    }
                }

                node = match;
            }
            else
            {
                // no need to continue.  missing segment in our trie
                //
                icLogWarn(LOG_TAG, "unable to 'locate uri %s' in dispatcher; missing node with label %s", uri, tok);
                node = NULL;
                break;
            }
        }

        // move to the next token
        //
        tok = strtok_r(NULL, URL_SEPARATOR, &ctx);
    }

    // should be pointing to the last node in the trie path
    //
    if (node != NULL && node->handler != NULL)
    {
        // create the return object to include the function
        // the the description (if set)
        //
        retVal = (uriHandlerContainer *)calloc(sizeof(uriHandlerContainer), 1);
        retVal->handler = node->handler;
        if (node->handlerDesc != NULL)
        {
            retVal->description = strdup(node->handlerDesc);
        }
    }

    // cleanup and return
    //
    free(copy);
    return retVal;
}

/*
 * destroy a uriHandlerContainer (from locateUriHandler)
 */
void uriHandlerContainerDestroy(uriHandlerContainer *container)
{
    if (container != NULL)
    {
        if (container->description != NULL)
        {
            free(container->description);
        }
        free(container);
    }
}

/*
 * Register a directive
 * @param dispatcher the dispatcher to register for
 * @param directiveName the directive name.  This should NOT include the # which signifies a directive
 * @param handler the handler callback
 * @param context the callback context
 * @return true if successfully registered
 */
bool uriDispatcherRegisterDirective(uriDispatcher *dispatcher, const char *directiveName,
                                    UriDispatcherDirectiveHandler handler, void *context)
{
    bool retVal = false;

    if (dispatcher != NULL && directiveName != NULL && handler != NULL)
    {
        variableDirective *directive = (variableDirective *)calloc(1, sizeof(variableDirective));
        directive->directiveName = strdup(directiveName);
        directive->handler = handler;
        directive->context = context;
        retVal = hashMapPut(dispatcher->variableDirectives, directive->directiveName,
                            strlen(directive->directiveName), directive);
    }

    return retVal;
}

/*
 * create a node and prime with 'name' (if not NULL)
 */
static trieNode *createNode(char *name)
{
    // allocate memory
    //
    trieNode *retVal = (trieNode *)calloc(sizeof(trieNode), 1);
    retVal->children = linkedListCreate();

    // apply name and default the varType
    //
    if (name != NULL)
    {
        retVal->label = strdup(name);
    }
    retVal->varType = VAR_NONE;

    return retVal;
}

/*
 * 'linkedListCompareFunc' implementation to search a list
 * of 'trieNode' objects, looking for a matching 'name'
 */
static bool searchListByName(void *searchVal, void *item)
{
    char *searchName = (char *)searchVal;
    trieNode *node = (trieNode *)item;

    // see if this node has the name we're looking for
    //
    if (stringCompare(searchName, node->label, true) == 0)
    {
        // match
        //
        return true;
    }
    return false;
}

/*
 * return the child of 'node' that has a label == name
 */
static trieNode *findChildTrieNode(trieNode *node, char *name)
{
    // loop through the children of 'node' looking for 'name'
    //
    return (trieNode *)linkedListFind(node->children, name, searchListByName);
}

/*
 * returns the 'variable node' child (if set)
 */
static trieNode *getNodeVarChild(trieNode *node)
{
    // see if the first child of 'node' is a variable node
    //
    trieNode *first = (trieNode *)linkedListGetElementAt(node->children, 0);
    if (first != NULL && first->varType != VAR_NONE)
    {
        // got a variable at the head of the children list
        //
        return first;
    }

    return NULL;
}
