// ------------------------------ tabstop = 4 ----------------------------------
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
// ------------------------------ tabstop = 4 ----------------------------------

/**
 * SBMD Result Builder
 *
 * Provides the Sbmd.result() builder for constructing handler return values.
 * Loaded after sbmd-utils.js, which creates the Sbmd namespace.
 *
 * Usage:
 *   Sbmd.result()
 *       .dataModel.updateResource("1", "isOn", "true")
 *       .log("updated isOn")
 *       .success()
 *
 * Non-terminal methods return the builder. Terminal methods return the raw
 * {ops, terminal} object — further chaining is impossible because the raw
 * object has no builder methods.
 *
 * If a caller stores a reference to the builder and attempts to add
 * operations after a terminal has been set, the builder throws.
 */

(function(globalThis)
{
    'use strict';

    function ResultBuilder()
    {
        this._ops = [];
        this._terminal = null;
        this._sealed = false;
    }

    ResultBuilder.prototype._addOp = function(op)
    {
        if (this._sealed)
        {
            throw new Error('Cannot add operations after a terminal');
        }

        this._ops.push(op);

        return this;
    };

    ResultBuilder.prototype._setTerminal = function(terminal)
    {
        if (this._sealed)
        {
            throw new Error('Cannot add operations after a terminal');
        }

        this._terminal = terminal;
        this._sealed = true;

        return { ops: this._ops, terminal: this._terminal };
    };

    ResultBuilder.prototype.log = function(message)
    {
        return this._addOp({ op: 'log', message: message });
    };

    ResultBuilder.prototype.success = function(value)
    {
        var terminal = { op: 'success' };

        if (value !== undefined)
        {
            terminal.value = value;
        }

        return this._setTerminal(terminal);
    };

    ResultBuilder.prototype.error = function(message)
    {
        return this._setTerminal({ op: 'error', message: message });
    };

    /**
     * dataModel namespace — resource and metadata operations.
     * Accessed as builder.dataModel.updateResource(...) etc.
     * Each method returns the builder for further chaining.
     */
    Object.defineProperty(ResultBuilder.prototype, 'dataModel', {
        get: function()
        {
            var builder = this;

            return {
                /**
                 * Update a Barton resource value.
                 * 2-arg: updateResource(resource, value) — uses trigger endpoint
                 * 3-arg: updateResource(endpoint, resource, value)
                 * 4-arg: updateResource(endpoint, resource, value, options)
                 */
                updateResource: function(a, b, c, d)
                {
                    var op;

                    if (c === undefined)
                    {
                        op = { op: 'updateResource', resource: a, value: b };
                    }
                    else
                    {
                        op = { op: 'updateResource', endpoint: a, resource: b, value: c };

                        if (d !== undefined)
                        {
                            op.metadata = JSON.stringify(d);
                        }
                    }

                    return builder._addOp(op);
                },

                /**
                 * Set metadata on a resource.
                 * @param {string} endpoint - Endpoint ID
                 * @param {string} resource - Resource ID
                 * @param {string} key - Metadata key
                 * @param {string} value - Metadata value
                 */
                setMetadata: function(endpoint, resource, key, value)
                {
                    return builder._addOp({
                        op: 'setMetadata',
                        endpoint: endpoint,
                        resource: resource,
                        key: key,
                        value: value
                    });
                }
            };
        }
    });

    /**
     * storage namespace — persistent and transient data operations.
     */
    Object.defineProperty(ResultBuilder.prototype, 'storage', {
        get: function()
        {
            var builder = this;

            return {
                setPersistentData: function(key, value)
                {
                    return builder._addOp({
                        op: 'setPersistentData',
                        key: key,
                        value: value
                    });
                },

                setTransientData: function(key, value, ttlSecs)
                {
                    return builder._addOp({
                        op: 'setTransientData',
                        key: key,
                        value: value,
                        ttlSecs: ttlSecs
                    });
                }
            };
        }
    });

    /**
     * device namespace — Matter device command and attribute operations.
     * sendCommand and writeAttribute are terminals (they trigger a Matter command/write).
     * requestCommand and readAttribute are deferred terminals (park the operation).
     */
    Object.defineProperty(ResultBuilder.prototype, 'device', {
        get: function()
        {
            var builder = this;

            return {
                /**
                 * Terminal: send a Matter invoke command.
                 * @param {number} clusterId
                 * @param {number} commandId
                 * @param {string} [tlvBase64] - Optional TLV payload
                 * @param {Object} [options] - endpointId, timedInvokeTimeoutMs
                 */
                sendCommand: function(clusterId, commandId, tlvBase64, options)
                {
                    var t = {
                        op: 'sendCommand',
                        clusterId: clusterId,
                        commandId: commandId
                    };

                    if (tlvBase64 !== undefined)
                    {
                        t.tlvBase64 = tlvBase64;
                    }

                    if (options !== undefined)
                    {
                        t.options = options;
                    }

                    return builder._setTerminal(t);
                },

                /**
                 * Terminal: write a Matter attribute.
                 * @param {number} clusterId
                 * @param {number} attributeId
                 * @param {string} tlvBase64
                 * @param {Object} [options] - endpointId
                 */
                writeAttribute: function(clusterId, attributeId, tlvBase64, options)
                {
                    var t = {
                        op: 'writeAttribute',
                        clusterId: clusterId,
                        attributeId: attributeId,
                        tlvBase64: tlvBase64
                    };

                    if (options !== undefined)
                    {
                        t.options = options;
                    }

                    return builder._setTerminal(t);
                },

                /**
                 * Deferred terminal: request a Matter command and wait for a response.
                 * @param {number} clusterId
                 * @param {number} commandId
                 * @param {Object} deferred - { responseCommandId, onResponse, onError, timeoutMs }
                 * @param {string} [tlvBase64]
                 * @param {Object} [options]
                 */
                requestCommand: function(clusterId, commandId, deferred, tlvBase64, options)
                {
                    var t = {
                        op: 'requestCommand',
                        clusterId: clusterId,
                        commandId: commandId,
                        deferred: deferred
                    };

                    if (tlvBase64 !== undefined)
                    {
                        t.tlvBase64 = tlvBase64;
                    }

                    if (options !== undefined)
                    {
                        t.options = options;
                    }

                    return builder._setTerminal(t);
                },

                /**
                 * Deferred terminal: read a Matter attribute and wait for the response.
                 * @param {number} clusterId
                 * @param {number} attributeId
                 * @param {Object} deferred - { onResponse, onError, timeoutMs }
                 * @param {Object} [options]
                 */
                readAttribute: function(clusterId, attributeId, deferred, options)
                {
                    var t = {
                        op: 'readAttribute',
                        clusterId: clusterId,
                        attributeId: attributeId,
                        deferred: deferred
                    };

                    if (options !== undefined)
                    {
                        t.options = options;
                    }

                    return builder._setTerminal(t);
                }
            };
        }
    });

    function createResultBuilder()
    {
        return new ResultBuilder();
    }

    // Attach result builder to existing Sbmd namespace
    globalThis.Sbmd.result = createResultBuilder;

})(globalThis);
