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
 * SBMD Utilities Bundle
 *
 * Provides general-purpose utilities for SBMD scripts including:
 * - Base64 encoding/decoding
 * - TLV encoding/decoding for Matter types
 * - Helper functions for constructing invoke/write responses
 *
 * This bundle is always loaded into the QuickJS context for SBMD scripts,
 * providing a consistent interface regardless of whether matter.js is used.
 */

(function(globalThis) {
    'use strict';

    // Matter TLV element types (from Matter spec)
    const TLV_TYPE = {
        SIGNED_INT: 0x00,      // 0x00-0x03 based on size
        UNSIGNED_INT: 0x04,    // 0x04-0x07 based on size
        BOOL_FALSE: 0x08,
        BOOL_TRUE: 0x09,
        FLOAT: 0x0A,
        DOUBLE: 0x0B,
        UTF8_STRING: 0x0C,     // 0x0C-0x0F based on length encoding
        OCTET_STRING: 0x10,    // 0x10-0x13 based on length encoding
        NULL: 0x14,
        STRUCT: 0x15,
        ARRAY: 0x16,
        LIST: 0x17,
        END_CONTAINER: 0x18
    };

    // Control byte masks
    const CONTROL_ELEMENT_TYPE_MASK = 0x1F;
    const CONTROL_TAG_FORM_MASK = 0xE0;

    // Tag forms
    const TAG_ANONYMOUS = 0x00;
    const TAG_CONTEXT = 0x20;
    const TAG_COMMON_PROFILE_2 = 0x40;
    const TAG_COMMON_PROFILE_4 = 0x60;
    const TAG_IMPLICIT_PROFILE_2 = 0x80;
    const TAG_IMPLICIT_PROFILE_4 = 0xA0;
    const TAG_FULLY_QUALIFIED_6 = 0xC0;
    const TAG_FULLY_QUALIFIED_8 = 0xE0;

    /**
     * UTF-8 encoding/decoding utilities
     * Needed because String.fromCharCode treats bytes as UCS-2 code units,
     * not UTF-8 bytes. These utilities properly handle multi-byte UTF-8 sequences.
     */
    const Utf8 = {
        /**
         * Decode UTF-8 bytes to a JavaScript string
         * @param {Uint8Array} bytes - UTF-8 encoded bytes
         * @returns {string} Decoded string
         */
        decode: function(bytes) {
            let result = '';
            let i = 0;
            while (i < bytes.length) {
                const b0 = bytes[i];
                if (b0 < 0x80) {
                    // 1-byte sequence (ASCII)
                    result += String.fromCharCode(b0);
                    i += 1;
                } else if ((b0 & 0xE0) === 0xC0) {
                    // 2-byte sequence
                    const b1 = bytes[i + 1];
                    result += String.fromCharCode(((b0 & 0x1F) << 6) | (b1 & 0x3F));
                    i += 2;
                } else if ((b0 & 0xF0) === 0xE0) {
                    // 3-byte sequence
                    const b1 = bytes[i + 1];
                    const b2 = bytes[i + 2];
                    result += String.fromCharCode(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
                    i += 3;
                } else if ((b0 & 0xF8) === 0xF0) {
                    // 4-byte sequence (surrogate pair needed)
                    const b1 = bytes[i + 1];
                    const b2 = bytes[i + 2];
                    const b3 = bytes[i + 3];
                    const codePoint = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
                    // Convert to surrogate pair
                    const adjusted = codePoint - 0x10000;
                    result += String.fromCharCode(0xD800 + (adjusted >> 10), 0xDC00 + (adjusted & 0x3FF));
                    i += 4;
                } else {
                    // Invalid UTF-8, skip byte
                    result += '\uFFFD';
                    i += 1;
                }
            }
            return result;
        },

        /**
         * Encode a JavaScript string to UTF-8 bytes
         * @param {string} str - String to encode
         * @returns {Uint8Array} UTF-8 encoded bytes
         */
        encode: function(str) {
            const bytes = [];
            for (let i = 0; i < str.length; i++) {
                let codePoint = str.charCodeAt(i);
                // Handle surrogate pairs
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < str.length) {
                    const next = str.charCodeAt(i + 1);
                    if (next >= 0xDC00 && next <= 0xDFFF) {
                        codePoint = 0x10000 + ((codePoint & 0x3FF) << 10) + (next & 0x3FF);
                        i++;
                    }
                }

                if (codePoint < 0x80) {
                    bytes.push(codePoint);
                } else if (codePoint < 0x800) {
                    bytes.push(0xC0 | (codePoint >> 6));
                    bytes.push(0x80 | (codePoint & 0x3F));
                } else if (codePoint < 0x10000) {
                    bytes.push(0xE0 | (codePoint >> 12));
                    bytes.push(0x80 | ((codePoint >> 6) & 0x3F));
                    bytes.push(0x80 | (codePoint & 0x3F));
                } else {
                    bytes.push(0xF0 | (codePoint >> 18));
                    bytes.push(0x80 | ((codePoint >> 12) & 0x3F));
                    bytes.push(0x80 | ((codePoint >> 6) & 0x3F));
                    bytes.push(0x80 | (codePoint & 0x3F));
                }
            }
            return new Uint8Array(bytes);
        }
    };

    /**
     * Base64 encoding/decoding utilities
     */
    const Base64 = {
        /**
         * Decode a base64 string to a Uint8Array
         * @param {string} base64 - Base64 encoded string
         * @returns {Uint8Array} Decoded bytes
         */
        decode: function(base64) {
            // Handle padding
            const padding = base64.endsWith('==') ? 2 : base64.endsWith('=') ? 1 : 0;
            const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
            const bytes = [];

            for (let i = 0; i < base64.length; i += 4) {
                const c0 = chars.indexOf(base64[i]);
                const c1 = chars.indexOf(base64[i + 1]);
                const c2 = base64[i + 2] === '=' ? 0 : chars.indexOf(base64[i + 2]);
                const c3 = base64[i + 3] === '=' ? 0 : chars.indexOf(base64[i + 3]);

                bytes.push((c0 << 2) | (c1 >> 4));
                if (base64[i + 2] !== '=') {
                    bytes.push(((c1 & 0x0F) << 4) | (c2 >> 2));
                }
                if (base64[i + 3] !== '=') {
                    bytes.push(((c2 & 0x03) << 6) | c3);
                }
            }

            return new Uint8Array(bytes);
        },

        /**
         * Encode a Uint8Array to a base64 string
         * @param {Uint8Array} bytes - Bytes to encode
         * @returns {string} Base64 encoded string
         */
        encode: function(bytes) {
            const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
            let result = '';

            for (let i = 0; i < bytes.length; i += 3) {
                const b0 = bytes[i];
                const b1 = i + 1 < bytes.length ? bytes[i + 1] : 0;
                const b2 = i + 2 < bytes.length ? bytes[i + 2] : 0;

                result += chars[b0 >> 2];
                result += chars[((b0 & 0x03) << 4) | (b1 >> 4)];
                result += i + 1 < bytes.length ? chars[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
                result += i + 2 < bytes.length ? chars[b2 & 0x3F] : '=';
            }

            return result;
        }
    };

    /**
     * TLV Reader - reads TLV encoded data
     */
    class TlvReader {
        constructor(bytes) {
            this.bytes = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
            this.offset = 0;
        }

        hasMore() {
            return this.offset < this.bytes.length;
        }

        readByte() {
            if (this.offset >= this.bytes.length) {
                throw new Error('Unexpected end of TLV data');
            }
            return this.bytes[this.offset++];
        }

        readBytes(count) {
            if (this.offset + count > this.bytes.length) {
                throw new Error('Unexpected end of TLV data');
            }
            const result = this.bytes.slice(this.offset, this.offset + count);
            this.offset += count;
            return result;
        }

        readUint(size) {
            // For values up to 4 bytes, use Number with bitwise operations as before.
            if (size <= 4) {
                let value = 0;
                for (let i = 0; i < size; i++) {
                    value |= this.readByte() << (i * 8);
                }
                // Handle unsigned values that exceed 31-bit range
                if (size === 4 && value < 0) {
                    value = value >>> 0;  // Convert to unsigned
                }
                return value;
            }

            // For values larger than 4 bytes, use BigInt to avoid 32-bit overflow.
            let value = 0n;
            for (let i = 0; i < size; i++) {
                const byte = this.readByte();
                value |= BigInt(byte) << (BigInt(i) * 8n);
            }
            return value;
        }

        readInt(size) {
            let value = this.readUint(size);

            // If value is a Number (sizes up to 4 bytes), use existing 32-bit logic.
            if (typeof value === 'number') {
                // Sign extend if necessary
                const signBit = 1 << (size * 8 - 1);
                if (value & signBit) {
                    // Negative number - sign extend
                    value = value - (1 << (size * 8));
                }
                return value;
            }

            // For BigInt values (sizes greater than 4 bytes), perform sign extension using BigInt.
            const bitCount = BigInt(size * 8);
            const signBit = 1n << (bitCount - 1n);
            if ((value & signBit) !== 0n) {
                // Negative number - sign extend
                value = value - (1n << bitCount);
            }
            return value;
        }

        readLength(sizeIndicator) {
            if (sizeIndicator === 0) return this.readByte();
            if (sizeIndicator === 1) return this.readUint(2);
            if (sizeIndicator === 2) return this.readUint(4);
            throw new Error('Invalid length indicator: ' + sizeIndicator);
        }

        /**
         * Read a single TLV element
         * @returns {{tag: number|null, value: any, type: number}}
         */
        readElement() {
            const control = this.readByte();
            const elementType = control & CONTROL_ELEMENT_TYPE_MASK;
            const tagForm = control & CONTROL_TAG_FORM_MASK;

            // Read tag if present
            let tag = null;
            if (tagForm === TAG_CONTEXT) {
                tag = this.readByte();
            } else if (tagForm === TAG_COMMON_PROFILE_2 || tagForm === TAG_IMPLICIT_PROFILE_2) {
                tag = this.readUint(2);
            } else if (tagForm === TAG_COMMON_PROFILE_4 || tagForm === TAG_IMPLICIT_PROFILE_4) {
                tag = this.readUint(4);
            } else if (tagForm === TAG_FULLY_QUALIFIED_6) {
                this.readUint(2); // vendor
                this.readUint(2); // profile
                tag = this.readUint(2);
            } else if (tagForm === TAG_FULLY_QUALIFIED_8) {
                this.readUint(2); // vendor
                this.readUint(2); // profile
                tag = this.readUint(4);
            }

            // Read value based on element type
            let value;
            const baseType = elementType;

            if (baseType >= 0x00 && baseType <= 0x03) {
                // Signed integer (1, 2, 4, 8 bytes)
                const size = 1 << (baseType - 0x00);
                value = this.readInt(size);
            } else if (baseType >= 0x04 && baseType <= 0x07) {
                // Unsigned integer (1, 2, 4, 8 bytes)
                const size = 1 << (baseType - 0x04);
                value = this.readUint(size);
            } else if (baseType === TLV_TYPE.BOOL_FALSE) {
                value = false;
            } else if (baseType === TLV_TYPE.BOOL_TRUE) {
                value = true;
            } else if (baseType === TLV_TYPE.FLOAT) {
                const bytes = this.readBytes(4);
                const view = new DataView(bytes.buffer, bytes.byteOffset, 4);
                value = view.getFloat32(0, true);
            } else if (baseType === TLV_TYPE.DOUBLE) {
                const bytes = this.readBytes(8);
                const view = new DataView(bytes.buffer, bytes.byteOffset, 8);
                value = view.getFloat64(0, true);
            } else if (baseType >= 0x0C && baseType <= 0x0F) {
                // UTF-8 string
                const len = this.readLength(baseType - 0x0C);
                const bytes = this.readBytes(len);
                value = Utf8.decode(bytes);
            } else if (baseType >= 0x10 && baseType <= 0x13) {
                // Octet string
                const len = this.readLength(baseType - 0x10);
                value = this.readBytes(len);
            } else if (baseType === TLV_TYPE.NULL) {
                value = null;
            } else if (baseType === TLV_TYPE.STRUCT) {
                value = this.readContainer('struct');
            } else if (baseType === TLV_TYPE.ARRAY) {
                value = this.readContainer('array');
            } else if (baseType === TLV_TYPE.LIST) {
                value = this.readContainer('list');
            } else if (baseType === TLV_TYPE.END_CONTAINER) {
                return { type: 'end' };
            } else {
                throw new Error('Unknown TLV element type: 0x' + baseType.toString(16));
            }

            return { tag, value, type: elementType };
        }

        readContainer(containerType) {
            const elements = [];
            while (this.hasMore()) {
                const element = this.readElement();
                if (element.type === 'end') break;
                elements.push(element);
            }

            if (containerType === 'array') {
                // Arrays contain anonymous elements
                return elements.map(e => e.value);
            } else if (containerType === 'struct') {
                // Structs have context-tagged elements, convert to object
                const obj = {};
                for (const e of elements) {
                    if (e.tag !== null) {
                        obj[e.tag] = e.value;
                    }
                }
                return obj;
            }
            return elements;
        }
    }

    /**
     * TLV Writer - writes TLV encoded data
     */
    class TlvWriter {
        constructor() {
            this.bytes = [];
        }

        writeByte(b) {
            this.bytes.push(b & 0xFF);
        }

        writeBytes(bytes) {
            for (let i = 0; i < bytes.length; i++) {
                this.bytes.push(bytes[i] & 0xFF);
            }
        }

        writeUint(value, size) {
            // For 1–4 byte integers, use the existing Number-based bitwise operations.
            if (size <= 4) {
                for (let i = 0; i < size; i++) {
                    this.bytes.push((value >> (i * 8)) & 0xFF);
                }
                return;
            }

            // For larger integers (e.g., 8-byte / 64-bit), use BigInt to avoid 32-bit shift limits.
            let v = (typeof value === 'bigint') ? value : BigInt(value);
            for (let i = 0; i < size; i++) {
                const byte = Number((v >> BigInt(i * 8)) & 0xFFn);
                this.bytes.push(byte & 0xFF);
            }
        }

        writeLength(len) {
            if (len <= 0xFF) {
                return 0; // length encoded as 1 byte in writeElement
            } else if (len <= 0xFFFF) {
                return 1;
            }
            return 2;
        }

        /**
         * Write a TLV element
         * @param {number|null} tag - Context tag (or null for anonymous)
         * @param {any} value - Value to encode
         * @param {string} type - Matter type name (optional, for explicit typing)
         */
        writeElement(tag, value, type) {
            const tagForm = tag === null ? TAG_ANONYMOUS : TAG_CONTEXT;

            if (value === null || value === undefined) {
                this.writeByte(tagForm | TLV_TYPE.NULL);
                if (tag !== null) this.writeByte(tag);
                return;
            }

            if (typeof value === 'boolean') {
                this.writeByte(tagForm | (value ? TLV_TYPE.BOOL_TRUE : TLV_TYPE.BOOL_FALSE));
                if (tag !== null) this.writeByte(tag);
                return;
            }

            if (typeof value === 'number') {
                if (Number.isInteger(value)) {
                    // Determine size needed
                    if (type && type.startsWith('int')) {
                        // Signed integer
                        this.writeSignedInt(tagForm, tag, value, type);
                    } else {
                        // Default to unsigned
                        this.writeUnsignedInt(tagForm, tag, value, type);
                    }
                } else {
                    // Float - use double for precision
                    this.writeByte(tagForm | TLV_TYPE.DOUBLE);
                    if (tag !== null) this.writeByte(tag);
                    const buf = new ArrayBuffer(8);
                    new DataView(buf).setFloat64(0, value, true);
                    this.writeBytes(new Uint8Array(buf));
                }
                return;
            }

            if (typeof value === 'string') {
                const bytes = Utf8.encode(value);
                const lenSize = bytes.length <= 0xFF ? 0 : bytes.length <= 0xFFFF ? 1 : 2;
                this.writeByte(tagForm | (TLV_TYPE.UTF8_STRING + lenSize));
                if (tag !== null) this.writeByte(tag);
                if (lenSize === 0) this.writeByte(bytes.length);
                else if (lenSize === 1) this.writeUint(bytes.length, 2);
                else this.writeUint(bytes.length, 4);
                this.writeBytes(bytes);
                return;
            }

            if (value instanceof Uint8Array || Array.isArray(value) && type === 'octstr') {
                const bytes = value instanceof Uint8Array ? value : new Uint8Array(value);
                const lenSize = bytes.length <= 0xFF ? 0 : bytes.length <= 0xFFFF ? 1 : 2;
                this.writeByte(tagForm | (TLV_TYPE.OCTET_STRING + lenSize));
                if (tag !== null) this.writeByte(tag);
                if (lenSize === 0) this.writeByte(bytes.length);
                else if (lenSize === 1) this.writeUint(bytes.length, 2);
                else this.writeUint(bytes.length, 4);
                this.writeBytes(bytes);
                return;
            }

            if (Array.isArray(value)) {
                this.writeByte(tagForm | TLV_TYPE.ARRAY);
                if (tag !== null) this.writeByte(tag);
                for (const item of value) {
                    this.writeElement(null, item);
                }
                this.writeByte(TLV_TYPE.END_CONTAINER);
                return;
            }

            if (typeof value === 'object') {
                this.writeByte(tagForm | TLV_TYPE.STRUCT);
                if (tag !== null) this.writeByte(tag);
                for (const [k, v] of Object.entries(value)) {
                    const fieldTag = parseInt(k, 10);
                    if (!isNaN(fieldTag)) {
                        this.writeElement(fieldTag, v);
                    }
                }
                this.writeByte(TLV_TYPE.END_CONTAINER);
                return;
            }

            throw new Error('Unsupported value type: ' + typeof value);
        }

        writeSignedInt(tagForm, tag, value, type) {
            let size;
            if (type === 'int8' || (value >= -128 && value <= 127)) {
                size = 1;
                this.writeByte(tagForm | 0x00);
            } else if (type === 'int16' || (value >= -32768 && value <= 32767)) {
                size = 2;
                this.writeByte(tagForm | 0x01);
            } else if (type === 'int32' || (value >= -2147483648 && value <= 2147483647)) {
                size = 4;
                this.writeByte(tagForm | 0x02);
            } else {
                size = 8;
                this.writeByte(tagForm | 0x03);
            }
            if (tag !== null) this.writeByte(tag);
            // Write in little-endian
            for (let i = 0; i < size; i++) {
                this.bytes.push((value >> (i * 8)) & 0xFF);
            }
        }

        writeUnsignedInt(tagForm, tag, value, type) {
            let size;
            if (type === 'uint8' || type === 'enum8' || type === 'percent' ||
                (value >= 0 && value <= 0xFF && !type)) {
                size = 1;
                this.writeByte(tagForm | 0x04);
            } else if (type === 'uint16' || type === 'enum16' || type === 'percent100ths' ||
                       (value >= 0 && value <= 0xFFFF && !type)) {
                size = 2;
                this.writeByte(tagForm | 0x05);
            } else if (type === 'uint32' || (value >= 0 && value <= 0xFFFFFFFF && !type)) {
                size = 4;
                this.writeByte(tagForm | 0x06);
            } else {
                size = 8;
                this.writeByte(tagForm | 0x07);
            }
            if (tag !== null) this.writeByte(tag);
            this.writeUint(value, size);
        }

        toBytes() {
            return new Uint8Array(this.bytes);
        }

        toBase64() {
            return Base64.encode(this.toBytes());
        }
    }

    /**
     * TLV utilities namespace
     */
    const Tlv = {
        /**
         * Decode a base64 TLV string to a JavaScript value
         * @param {string} base64 - Base64 encoded TLV data
         * @returns {any} Decoded value
         */
        decode: function(base64) {
            const bytes = Base64.decode(base64);
            const reader = new TlvReader(bytes);
            if (!reader.hasMore()) {
                return null;
            }
            const element = reader.readElement();
            return element.value;
        },

        /**
         * Decode base64 TLV to the underlying structure representation
         * This returns the decoded structure with context tags as numeric keys
         * @param {string} base64 - Base64 encoded TLV data
         * @returns {any} Decoded structure with context tags
         */
        decodeStruct: function(base64) {
            return this.decode(base64);
        },

        /**
         * Encode a JavaScript value to base64 TLV
         * @param {any} value - Value to encode
         * @param {string} [type] - Optional Matter type hint
         * @returns {string} Base64 encoded TLV
         */
        encode: function(value, type) {
            const writer = new TlvWriter();
            writer.writeElement(null, value, type);
            return writer.toBase64();
        },

        /**
         * Encode a struct with named fields using a schema
         * @param {Object} value - Object with field values
         * @param {Object} schema - Schema mapping field names to {tag, type}
         * @returns {string} Base64 encoded TLV
         */
        encodeStruct: function(value, schema) {
            const writer = new TlvWriter();
            writer.writeByte(TLV_TYPE.STRUCT);
            for (const [name, fieldInfo] of Object.entries(schema)) {
                if (value[name] !== undefined) {
                    writer.writeElement(fieldInfo.tag, value[name], fieldInfo.type);
                }
            }
            writer.writeByte(TLV_TYPE.END_CONTAINER);
            return writer.toBase64();
        },

        /**
         * Create an empty struct (STRUCT followed by END_CONTAINER)
         * @returns {string} Base64 encoded empty TLV struct
         */
        emptyStruct: function() {
            return Base64.encode(new Uint8Array([TLV_TYPE.STRUCT, TLV_TYPE.END_CONTAINER]));
        }
    };

    /**
     * Response helpers for SBMD scripts
     */
    const Response = {
        /**
         * Create an invoke (command) response
         * @param {number} clusterId - Matter cluster ID
         * @param {number} commandId - Matter command ID
         * @param {string} [tlvBase64] - Optional base64 TLV payload (defaults to empty struct)
         * @param {Object} [options] - Optional settings: endpointId, timedInvokeTimeoutMs
         * @returns {Object} Invoke response object
         */
        invoke: function(clusterId, commandId, tlvBase64, options) {
            const result = {
                invoke: {
                    clusterId: clusterId,
                    commandId: commandId
                }
            };

            if (tlvBase64) {
                result.invoke.tlvBase64 = tlvBase64;
            }

            if (options) {
                if (options.endpointId !== undefined) {
                    result.invoke.endpointId = options.endpointId;
                }
                if (options.timedInvokeTimeoutMs !== undefined) {
                    result.invoke.timedInvokeTimeoutMs = options.timedInvokeTimeoutMs;
                }
            }

            return result;
        },

        /**
         * Create a write (attribute) response
         * @param {number} clusterId - Matter cluster ID
         * @param {number} attributeId - Matter attribute ID
         * @param {string} tlvBase64 - Base64 TLV payload
         * @param {Object} [options] - Optional settings: endpointId
         * @returns {Object} Write response object
         */
        write: function(clusterId, attributeId, tlvBase64, options) {
            const result = {
                write: {
                    clusterId: clusterId,
                    attributeId: attributeId,
                    tlvBase64: tlvBase64
                }
            };

            if (options && options.endpointId !== undefined) {
                result.write.endpointId = options.endpointId;
            }

            return result;
        }
    };

    // Export the SbmdUtils object
    globalThis.SbmdUtils = Object.freeze({
        Base64: Object.freeze(Base64),
        Tlv: Object.freeze(Tlv),
        Response: Object.freeze(Response),
        TLV_TYPE: Object.freeze(TLV_TYPE)
    });

})(globalThis);
