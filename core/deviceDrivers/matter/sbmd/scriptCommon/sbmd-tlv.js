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
 * SBMD TLV Utilities
 *
 * Provides Sbmd.Tlv for encoding and decoding Matter TLV data.
 *
 * Requires: sbmd-namespace.js, sbmd-base64.js
 *   (Sbmd.Base64 and Sbmd._internal.Utf8 must exist)
 */

(function (Sbmd) {
    'use strict';

    var Utf8 = Sbmd._internal.Utf8;
    var Base64 = Sbmd.Base64;

    // Matter TLV element types (from Matter spec)
    var TLV_TYPE = {
        SIGNED_INT: 0x00, // 0x00-0x03 based on size
        UNSIGNED_INT: 0x04, // 0x04-0x07 based on size
        BOOL_FALSE: 0x08,
        BOOL_TRUE: 0x09,
        FLOAT: 0x0a,
        DOUBLE: 0x0b,
        UTF8_STRING: 0x0c, // 0x0C-0x0F based on length encoding
        OCTET_STRING: 0x10, // 0x10-0x13 based on length encoding
        NULL: 0x14,
        STRUCT: 0x15,
        ARRAY: 0x16,
        LIST: 0x17,
        END_CONTAINER: 0x18
    };

    // Control byte masks
    var CONTROL_ELEMENT_TYPE_MASK = 0x1f;
    var CONTROL_TAG_FORM_MASK = 0xe0;

    // Tag forms
    var TAG_ANONYMOUS = 0x00;
    var TAG_CONTEXT = 0x20;
    var TAG_COMMON_PROFILE_2 = 0x40;
    var TAG_COMMON_PROFILE_4 = 0x60;
    var TAG_IMPLICIT_PROFILE_2 = 0x80;
    var TAG_IMPLICIT_PROFILE_4 = 0xa0;
    var TAG_FULLY_QUALIFIED_6 = 0xc0;
    var TAG_FULLY_QUALIFIED_8 = 0xe0;

    // -----------------------------------------------------------------------
    // TLV Reader
    // -----------------------------------------------------------------------

    /**
     * TLV Reader - reads TLV encoded data
     * @param {Uint8Array|Array} bytes - TLV encoded bytes
     */
    function TlvReader(bytes) {
        this.bytes = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
        this.offset = 0;
    }

    TlvReader.prototype.hasMore = function () {
        return this.offset < this.bytes.length;
    };

    TlvReader.prototype.readByte = function () {
        if (this.offset >= this.bytes.length) {
            throw new Error('Unexpected end of TLV data');
        }

        return this.bytes[this.offset++];
    };

    TlvReader.prototype.readBytes = function (count) {
        if (this.offset + count > this.bytes.length) {
            throw new Error('Unexpected end of TLV data');
        }

        // Manual copy into a new ArrayBuffer (Uint8Array.slice not available in mquickjs)
        var buf = new ArrayBuffer(count);
        var result = new Uint8Array(buf);

        for (var i = 0; i < count; i++) {
            result[i] = this.bytes[this.offset + i];
        }

        this.offset += count;

        return result;
    };

    TlvReader.prototype.readUint = function (size) {
        // For values up to 4 bytes, use Number with bitwise operations.
        if (size <= 4) {
            var value = 0;

            for (var i = 0; i < size; i++) {
                value = value | (this.readByte() << (i * 8));
            }

            // Handle unsigned values that exceed 31-bit range
            if (size === 4 && value < 0) {
                value = value >>> 0; // Convert to unsigned
            }

            return value;
        }

        // For 8-byte unsigned integers: read as two 32-bit halves and combine.
        // Values that exceed Number safe integer range (53 bits) will be approximate.
        var low = this.readUint(4);
        var high = this.readUint(4);

        return high * 4294967296 + low;
    };

    TlvReader.prototype.readInt = function (size) {
        // For sizes up to 4 bytes, use existing 32-bit logic.
        if (size <= 4) {
            var value = this.readUint(size);
            // Sign extend if necessary. Use Math.pow rather than "1 << n": JS
            // bitwise shifts are modulo 32, so "1 << 32" (size == 4) would wrap
            // to 1 and corrupt int32 sign extension.
            var signBit = Math.pow(2, size * 8 - 1);

            if (value >= signBit) {
                // Negative number - sign extend
                value = value - Math.pow(2, size * 8);
            }

            return value;
        }

        // For 8-byte signed integers: read as two 32-bit halves with sign extension.
        var low = this.readUint(4);
        var high = this.readUint(4);
        // Treat high as signed 32-bit for sign extension
        var signedHigh = high | 0;

        return signedHigh * 4294967296 + low;
    };

    TlvReader.prototype.readLength = function (sizeIndicator) {
        if (sizeIndicator === 0) return this.readByte();
        if (sizeIndicator === 1) return this.readUint(2);
        if (sizeIndicator === 2) return this.readUint(4);
        throw new Error('Invalid length indicator: ' + sizeIndicator);
    };

    // Detect host endianness once at load time (no DataView needed)
    var _isHostLE = (function () {
        var buf = new ArrayBuffer(2);
        new Uint8Array(buf)[0] = 1;

        return new Uint16Array(buf)[0] === 1;
    })();

    // Helper: decode float32/float64 from little-endian bytes, regardless of host endianness
    function decodeFloatLE(bytes, isDouble) {
        // Use DataView if available
        if (typeof DataView !== 'undefined') {
            var dv = new DataView(bytes.buffer, bytes.byteOffset || 0, bytes.length);

            return isDouble ? dv.getFloat64(0, true) : dv.getFloat32(0, true);
        }

        if (_isHostLE) {
            // Data is LE, host is LE — direct view if aligned, copy otherwise
            var align = isDouble ? 8 : 4;

            if (bytes.byteOffset % align === 0) {
                return isDouble
                    ? new Float64Array(bytes.buffer, bytes.byteOffset, 1)[0]
                    : new Float32Array(bytes.buffer, bytes.byteOffset, 1)[0];
            }

            var arr = new Uint8Array(align);

            for (var i = 0; i < align; i++) arr[i] = bytes[i];

            return isDouble ? new Float64Array(arr.buffer)[0] : new Float32Array(arr.buffer)[0];
        }

        // BE host: reverse bytes
        var len = isDouble ? 8 : 4;
        var rev = new Uint8Array(len);

        for (var i = 0; i < len; i++) rev[i] = bytes[len - 1 - i];

        return isDouble ? new Float64Array(rev.buffer)[0] : new Float32Array(rev.buffer)[0];
    }

    /**
     * Read a single TLV element
     * @returns {{tag: number|null, value: any, type: number}}
     */
    TlvReader.prototype.readElement = function () {
        var control = this.readByte();
        var elementType = control & CONTROL_ELEMENT_TYPE_MASK;
        var tagForm = control & CONTROL_TAG_FORM_MASK;

        // Read tag if present
        var tag = null;

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
        var value;
        var baseType = elementType;

        if (baseType >= 0x00 && baseType <= 0x03) {
            // Signed integer (1, 2, 4, 8 bytes)
            var intSize = 1 << (baseType - 0x00);
            value = this.readInt(intSize);
        } else if (baseType >= 0x04 && baseType <= 0x07) {
            // Unsigned integer (1, 2, 4, 8 bytes)
            var uintSize = 1 << (baseType - 0x04);
            value = this.readUint(uintSize);
        } else if (baseType === TLV_TYPE.BOOL_FALSE) {
            value = false;
        } else if (baseType === TLV_TYPE.BOOL_TRUE) {
            value = true;
        } else if (baseType === TLV_TYPE.FLOAT) {
            var floatBytes = this.readBytes(4);
            value = decodeFloatLE(floatBytes, false);
        } else if (baseType === TLV_TYPE.DOUBLE) {
            var doubleBytes = this.readBytes(8);
            value = decodeFloatLE(doubleBytes, true);
        } else if (baseType >= 0x0c && baseType <= 0x0f) {
            // UTF-8 string
            var strLen = this.readLength(baseType - 0x0c);
            var strBytes = this.readBytes(strLen);
            value = Utf8.decode(strBytes);
        } else if (baseType >= 0x10 && baseType <= 0x13) {
            // Octet string
            var octLen = this.readLength(baseType - 0x10);
            value = this.readBytes(octLen);
        } else if (baseType === TLV_TYPE.NULL) {
            value = null;
        } else if (baseType === TLV_TYPE.STRUCT) {
            value = this.readContainer('struct');
        } else if (baseType === TLV_TYPE.ARRAY) {
            value = this.readContainer('array');
        } else if (baseType === TLV_TYPE.LIST) {
            value = this.readContainer('list');
        } else if (baseType === TLV_TYPE.END_CONTAINER) {
            return {type: 'end'};
        } else {
            throw new Error('Unknown TLV element type: 0x' + baseType.toString(16));
        }

        return {tag: tag, value: value, type: elementType};
    };

    TlvReader.prototype.readContainer = function (containerType) {
        var elements = [];

        while (this.hasMore()) {
            var element = this.readElement();

            if (element.type === 'end') break;

            elements.push(element);
        }

        if (containerType === 'array') {
            // Arrays contain anonymous elements
            return elements.map(function (e) {
                return e.value;
            });
        } else if (containerType === 'struct') {
            // Structs have context-tagged elements, convert to object
            var obj = {};

            for (var i = 0; i < elements.length; i++) {
                var e = elements[i];

                if (e.tag !== null) {
                    obj[e.tag] = e.value;
                }
            }

            return obj;
        }

        return elements;
    };

    // -----------------------------------------------------------------------
    // TLV Writer
    // -----------------------------------------------------------------------

    /**
     * TLV Writer - writes TLV encoded data
     */
    function TlvWriter() {
        this.bytes = [];
    }

    TlvWriter.prototype.writeByte = function (b) {
        this.bytes.push(b & 0xff);
    };

    TlvWriter.prototype.writeBytes = function (bytes) {
        for (var i = 0; i < bytes.length; i++) {
            this.bytes.push(bytes[i] & 0xff);
        }
    };

    TlvWriter.prototype.writeUint = function (value, size) {
        // For 1-4 byte integers, use Number-based bitwise operations.
        if (size <= 4) {
            for (var i = 0; i < size; i++) {
                this.bytes.push((value >> (i * 8)) & 0xff);
            }

            return;
        }

        // For 8-byte integers, split into two 32-bit halves.
        var high = Math.floor(value / 4294967296);
        var low = value - high * 4294967296;

        for (var i = 0; i < 4; i++) {
            this.bytes.push((low >> (i * 8)) & 0xff);
        }

        for (var i = 0; i < 4; i++) {
            this.bytes.push((high >> (i * 8)) & 0xff);
        }
    };

    TlvWriter.prototype.writeLength = function (len) {
        if (len <= 0xff) {
            return 0; // length encoded as 1 byte in writeElement
        } else if (len <= 0xffff) {
            return 1;
        }

        return 2;
    };

    /**
     * Write a TLV element
     * @param {number|null} tag - Context tag (or null for anonymous)
     * @param {any} value - Value to encode
     * @param {string} type - Matter type name (optional, for explicit typing)
     */
    TlvWriter.prototype.writeElement = function (tag, value, type) {
        var tagForm = tag === null ? TAG_ANONYMOUS : TAG_CONTEXT;

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
            if (typeof value === 'number' && value === Math.floor(value)) {
                // Determine size needed
                if (type && type.substring(0, 3) === 'int') {
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

                var f64 = new Float64Array(1);
                f64[0] = value;
                this.writeBytes(new Uint8Array(f64.buffer));
            }

            return;
        }

        if (typeof value === 'string') {
            var strBytes = Utf8.encode(value);
            var strLenSize = strBytes.length <= 0xff ? 0 : strBytes.length <= 0xffff ? 1 : 2;
            this.writeByte(tagForm | (TLV_TYPE.UTF8_STRING + strLenSize));

            if (tag !== null) this.writeByte(tag);

            if (strLenSize === 0) this.writeByte(strBytes.length);
            else if (strLenSize === 1) this.writeUint(strBytes.length, 2);
            else this.writeUint(strBytes.length, 4);

            this.writeBytes(strBytes);

            return;
        }

        if (value instanceof Uint8Array || (Array.isArray(value) && type === 'octstr')) {
            var octBytes = value instanceof Uint8Array ? value : new Uint8Array(value);
            var octLenSize = octBytes.length <= 0xff ? 0 : octBytes.length <= 0xffff ? 1 : 2;
            this.writeByte(tagForm | (TLV_TYPE.OCTET_STRING + octLenSize));

            if (tag !== null) this.writeByte(tag);

            if (octLenSize === 0) this.writeByte(octBytes.length);
            else if (octLenSize === 1) this.writeUint(octBytes.length, 2);
            else this.writeUint(octBytes.length, 4);

            this.writeBytes(octBytes);

            return;
        }

        if (Array.isArray(value)) {
            this.writeByte(tagForm | TLV_TYPE.ARRAY);

            if (tag !== null) this.writeByte(tag);

            for (var i = 0; i < value.length; i++) {
                this.writeElement(null, value[i]);
            }

            this.writeByte(TLV_TYPE.END_CONTAINER);

            return;
        }

        if (typeof value === 'object') {
            this.writeByte(tagForm | TLV_TYPE.STRUCT);

            if (tag !== null) this.writeByte(tag);

            var keys = Object.keys(value);

            for (var i = 0; i < keys.length; i++) {
                var k = keys[i];
                var fieldTag = parseInt(k, 10);

                if (!isNaN(fieldTag)) {
                    this.writeElement(fieldTag, value[k]);
                }
            }

            this.writeByte(TLV_TYPE.END_CONTAINER);

            return;
        }

        throw new Error('Unsupported value type: ' + typeof value);
    };

    TlvWriter.prototype.writeSignedInt = function (tagForm, tag, value, type) {
        var size;

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
        if (size <= 4) {
            for (var i = 0; i < size; i++) {
                this.bytes.push((value >> (i * 8)) & 0xff);
            }
        } else {
            // 8-byte signed: split into two 32-bit halves
            var high = Math.floor(value / 4294967296);
            var low = value - high * 4294967296;

            for (var i = 0; i < 4; i++) {
                this.bytes.push((low >> (i * 8)) & 0xff);
            }

            for (var i = 0; i < 4; i++) {
                this.bytes.push((high >> (i * 8)) & 0xff);
            }
        }
    };

    TlvWriter.prototype.writeUnsignedInt = function (tagForm, tag, value, type) {
        var size;

        if (
            type === 'uint8' ||
            type === 'enum8' ||
            type === 'percent' ||
            (value >= 0 && value <= 0xff && !type)
        ) {
            size = 1;
            this.writeByte(tagForm | 0x04);
        } else if (
            type === 'uint16' ||
            type === 'enum16' ||
            type === 'percent100ths' ||
            (value >= 0 && value <= 0xffff && !type)
        ) {
            size = 2;
            this.writeByte(tagForm | 0x05);
        } else if (type === 'uint32' || (value >= 0 && value <= 0xffffffff && !type)) {
            size = 4;
            this.writeByte(tagForm | 0x06);
        } else {
            size = 8;
            this.writeByte(tagForm | 0x07);
        }

        if (tag !== null) this.writeByte(tag);

        this.writeUint(value, size);
    };

    TlvWriter.prototype.toBytes = function () {
        return new Uint8Array(this.bytes);
    };

    TlvWriter.prototype.toBase64 = function () {
        return Base64.encode(this.toBytes());
    };

    // -----------------------------------------------------------------------
    // Public Tlv API
    // -----------------------------------------------------------------------

    Sbmd.Tlv = {
        /**
         * Decode a base64 TLV string to a JavaScript value
         * @param {string} base64 - Base64 encoded TLV data
         * @returns {any} Decoded value
         */
        decode: function (base64) {
            // A missing or empty payload (e.g. an attribute report that omits the
            // TLV data) decodes to null rather than throwing.
            if (base64 === undefined || base64 === null || base64 === '') {
                return null;
            }

            var bytes = Base64.decode(base64);
            var reader = new TlvReader(bytes);

            if (!reader.hasMore()) {
                return null;
            }

            var element = reader.readElement();

            return element.value;
        },

        /**
         * Encode a JavaScript value to base64 TLV.
         *
         * Accepts either a number or a string.  For integer/enum types, a
         * string value is parsed using parseInt with the given base (default
         * 10).  For type 'string', the value is encoded as a TLV UTF-8
         * string (coerced via String() if not already a string).
         *
         * Range checking is performed for all non-string types: the value
         * must be a whole number within the type's bounds (e.g. int16:
         * -32768..32767, uint8: 0..255, enum8: 0..255).  Returns null when
         * the value cannot be parsed or is out of range.
         *
         * @param {any} value - Value to encode (number or parseable string)
         * @param {string} type - Matter type name (e.g. 'int16', 'uint8', 'enum8', 'string')
         * @param {number} [base=10] - Radix for string-to-integer parsing (invalid with type 'string')
         * @returns {string|null} Base64 encoded TLV, or null on parse/range error
         * @throws {Error} If type is omitted (undefined)
         * @throws {Error} If base is provided when type is 'string'
         */
        encode: function (value, type, base) {
            if (type === undefined) {
                throw new Error('Sbmd.Tlv.encode: type argument is required');
            }

            if (type === 'string') {
                if (base !== undefined) {
                    throw new Error('Sbmd.Tlv.encode: base cannot be used with string type');
                }

                if (typeof value !== 'string') {
                    value = String(value);
                }

                var writer = new TlvWriter();
                writer.writeElement(null, value);

                return writer.toBase64();
            }

            if (base === undefined) {
                base = 10;
            }

            // Type-specific ranges (min, max inclusive)
            var integerRanges = {
                'int8': [-128, 127],
                'int16': [-32768, 32767],
                'int32': [-2147483648, 2147483647],
                'uint8': [0, 0xff],
                'uint16': [0, 0xffff],
                'uint32': [0, 0xffffffff],
                'enum8': [0, 0xff],
                'enum16': [0, 0xffff],
                'percent': [0, 0xff],
                'percent100ths': [0, 0xffff],
                'bitmap8': [0, 0xff],
                'bitmap16': [0, 0xffff],
                'bitmap32': [0, 0xffffffff]
            };

            var range = integerRanges[type];

            // If value is a string and the type is an integer kind, parse it
            if (typeof value === 'string' && range) {
                var trimmed = value.trim();
                var integerPattern;

                if (trimmed.length === 0) {
                    return null;
                }

                switch (base) {
                    case 2:
                        integerPattern = /^[-+]?[01]+$/;
                        break;
                    case 8:
                        integerPattern = /^[-+]?[0-7]+$/;
                        break;
                    case 10:
                        integerPattern = /^[-+]?[0-9]+$/;
                        break;
                    case 16:
                        integerPattern = /^[-+]?[0-9a-fA-F]+$/;
                        break;
                    default:
                        return null;
                }

                if (!integerPattern.test(trimmed)) {
                    return null;
                }

                var parsed = parseInt(trimmed, base);

                if (isNaN(parsed)) {
                    return null;
                }

                value = parsed;
            }

            // Range-check numeric values when a type with known bounds is given
            if (typeof value === 'number' && range) {
                if (value !== Math.floor(value) || value < range[0] || value > range[1]) {
                    return null;
                }
            }

            var writer = new TlvWriter();
            writer.writeElement(null, value, type);

            return writer.toBase64();
        },

        /**
         * Encode a struct with named fields using a schema
         * @param {Object} value - Object with field values
         * @param {Object} schema - Schema mapping field names to {tag, type}
         * @returns {string} Base64 encoded TLV
         */
        encodeStruct: function (value, schema) {
            var writer = new TlvWriter();
            writer.writeByte(TLV_TYPE.STRUCT);
            var names = Object.keys(schema);

            for (var i = 0; i < names.length; i++) {
                var name = names[i];
                var fieldInfo = schema[name];

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
        emptyStruct: function () {
            return Base64.encode(new Uint8Array([TLV_TYPE.STRUCT, TLV_TYPE.END_CONTAINER]));
        },

        TYPE: TLV_TYPE
    };
})(globalThis.Sbmd);
