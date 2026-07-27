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

/*
 * Unit tests for the SBMD JavaScript TLV utilities (Sbmd.Tlv) defined in
 * scriptCommon/sbmd-tlv.js. These run against the real mquickjs runtime with
 * the scriptCommon bundle loaded, exercising the full public API: decode,
 * encode, encodeStruct, emptyStruct, and TYPE.
 *
 * Test vectors are built from raw TLV bytes via Sbmd.Base64.encode(...) so the
 * control-byte/value encoding is explicit. Values are read back as strings
 * (String(...) / JSON.stringify(...)) because the installed mquickjs header
 * exposes only JS_ToInt32 (no 64-bit / float extractors), and strings compare
 * uniformly across every numeric range and type.
 */

#include "deviceDrivers/matter/sbmd/mquickjs/MQuickJsRuntime.h"
#include "deviceDrivers/matter/sbmd/mquickjs/SbmdBundleLoader.h"

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include <mquickjs/mquickjs.h>
}

using namespace barton;

namespace
{
    class SbmdTlvTest : public ::testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(MQuickJsRuntime::Initialize(256 * 1024));
            auto *ctx = MQuickJsRuntime::GetSharedContext();
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(SbmdBundleLoader::LoadBundle(ctx));
        }

        static void TearDownTestSuite() { MQuickJsRuntime::Shutdown(); }

        /*
         * Evaluate a JS expression (which must yield a string) and return it.
         * Takes and releases the runtime mutex. Returns "<exception>" if the
         * expression throws.
         */
        std::string Eval(const std::string &js)
        {
            std::lock_guard<std::mutex> lock(MQuickJsRuntime::GetMutex());
            auto *ctx = MQuickJsRuntime::GetSharedContext();

            JSValue result = JS_Eval(ctx, js.c_str(), js.size(), "<test>", JS_EVAL_RETVAL);

            if (JS_IsException(result))
            {
                MQuickJsRuntime::CheckAndClearPendingException(ctx);
                return "<exception>";
            }

            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, result, &buf);

            return str ? std::string(str) : std::string("<null>");
        }

        /*
         * Decode raw TLV bytes (a JS array literal like "[0x02, 0xFF]") and
         * return String(decodedValue).
         */
        std::string DecodeStr(const std::string &bytes)
        {
            return Eval("String(Sbmd.Tlv.decode(Sbmd.Base64.encode(new Uint8Array(" + bytes + "))))");
        }

        /*
         * Decode raw TLV bytes and return JSON.stringify(decodedValue). Used for
         * container types (struct/array).
         */
        std::string DecodeJson(const std::string &bytes)
        {
            return Eval("JSON.stringify(Sbmd.Tlv.decode(Sbmd.Base64.encode(new Uint8Array(" + bytes + "))))");
        }
    };

    // ========================================================================
    // decode: signed integers (readInt) -- includes the int32 sign-extension
    // regression. Before the Math.pow fix, "1 << 32" wrapped to 1 and int32
    // negatives decoded as large positive numbers.
    // ========================================================================

    TEST_F(SbmdTlvTest, DecodeSignedInt8)
    {
        EXPECT_EQ("-1", DecodeStr("[0x00, 0xFF]"));
        EXPECT_EQ("-128", DecodeStr("[0x00, 0x80]"));
        EXPECT_EQ("127", DecodeStr("[0x00, 0x7F]"));
        EXPECT_EQ("0", DecodeStr("[0x00, 0x00]"));
    }

    TEST_F(SbmdTlvTest, DecodeSignedInt16)
    {
        EXPECT_EQ("-1", DecodeStr("[0x01, 0xFF, 0xFF]"));
        EXPECT_EQ("-32768", DecodeStr("[0x01, 0x00, 0x80]"));
        EXPECT_EQ("32767", DecodeStr("[0x01, 0xFF, 0x7F]"));
    }

    TEST_F(SbmdTlvTest, DecodeSignedInt32Regression)
    {
        EXPECT_EQ("-1", DecodeStr("[0x02, 0xFF, 0xFF, 0xFF, 0xFF]"));
        EXPECT_EQ("-2147483648", DecodeStr("[0x02, 0x00, 0x00, 0x00, 0x80]"));
        EXPECT_EQ("2147483647", DecodeStr("[0x02, 0xFF, 0xFF, 0xFF, 0x7F]"));
        // -100000 == 0xFFFE7960, little-endian 0x60 0x79 0xFE 0xFF
        EXPECT_EQ("-100000", DecodeStr("[0x02, 0x60, 0x79, 0xFE, 0xFF]"));
    }

    TEST_F(SbmdTlvTest, DecodeSignedInt64)
    {
        EXPECT_EQ("-1", DecodeStr("[0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]"));
        EXPECT_EQ("1", DecodeStr("[0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]"));
    }

    // ========================================================================
    // decode: unsigned integers (readUint)
    // ========================================================================

    TEST_F(SbmdTlvTest, DecodeUnsignedInt)
    {
        EXPECT_EQ("255", DecodeStr("[0x04, 0xFF]"));
        EXPECT_EQ("65535", DecodeStr("[0x05, 0xFF, 0xFF]"));
        // uint32 values above 2^31 must stay positive (readUint's ">>> 0")
        EXPECT_EQ("4294967295", DecodeStr("[0x06, 0xFF, 0xFF, 0xFF, 0xFF]"));
        EXPECT_EQ("2147483648", DecodeStr("[0x06, 0x00, 0x00, 0x00, 0x80]"));
        // uint64 read as two 32-bit halves: 0x0000000100000000
        EXPECT_EQ("4294967296", DecodeStr("[0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00]"));
    }

    // ========================================================================
    // decode: bool, null, float, double
    // ========================================================================

    TEST_F(SbmdTlvTest, DecodeBoolAndNull)
    {
        EXPECT_EQ("true", DecodeStr("[0x09]"));
        EXPECT_EQ("false", DecodeStr("[0x08]"));
        EXPECT_EQ("null", DecodeStr("[0x14]"));
        // Missing / empty payloads decode to null (not throw).
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.decode())"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.decode(''))"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.decode(null))"));
    }

    TEST_F(SbmdTlvTest, DecodeFloatAndDouble)
    {
        // float32(1.5) == 0x3FC00000, little-endian
        EXPECT_EQ("1.5", DecodeStr("[0x0A, 0x00, 0x00, 0xC0, 0x3F]"));
        // float64(2.25) == 0x4002000000000000, little-endian
        EXPECT_EQ("2.25", DecodeStr("[0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x40]"));
    }

    // ========================================================================
    // decode: UTF-8 string and octet string
    // ========================================================================

    TEST_F(SbmdTlvTest, DecodeUtf8String)
    {
        // UTF8 string, 1-byte length: 'H'(0x48) 'i'(0x69)
        EXPECT_EQ("Hi", DecodeStr("[0x0C, 0x02, 0x48, 0x69]"));
        EXPECT_EQ("", DecodeStr("[0x0C, 0x00]"));
    }

    TEST_F(SbmdTlvTest, DecodeOctetString)
    {
        // Octet string, 1-byte length, bytes 0xAB 0xCD -> Uint8Array
        EXPECT_EQ("2:171,205",
                  Eval("(function () {"
                       "  var b = Sbmd.Tlv.decode(Sbmd.Base64.encode(new Uint8Array([0x10, 0x02, 0xAB, 0xCD])));"
                       "  return b.length + ':' + b[0] + ',' + b[1];"
                       "})()"));
    }

    // ========================================================================
    // decode: containers (struct, array)
    // ========================================================================

    TEST_F(SbmdTlvTest, DecodeStruct)
    {
        // struct { ctx-tag 1: uint8 5, ctx-tag 2: bool true }
        // 0x15 struct, 0x24 (ctx|uint8) tag 0x01 val 0x05, 0x29 (ctx|bool-true) tag 0x02, 0x18 end
        EXPECT_EQ("{\"1\":5,\"2\":true}", DecodeJson("[0x15, 0x24, 0x01, 0x05, 0x29, 0x02, 0x18]"));
    }

    TEST_F(SbmdTlvTest, DecodeArray)
    {
        // array [ uint8 1, uint8 2 ]
        EXPECT_EQ("[1,2]", DecodeJson("[0x16, 0x04, 0x01, 0x04, 0x02, 0x18]"));
    }

    // ========================================================================
    // encode: round-trips through decode (writer + reader)
    // ========================================================================

    TEST_F(SbmdTlvTest, EncodeRoundTripIntegers)
    {
        EXPECT_EQ("255", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode(255, 'uint8')))"));
        EXPECT_EQ("65535", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode(65535, 'uint16')))"));
        EXPECT_EQ("4294967295", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode(4294967295, 'uint32')))"));
        // int32 negatives that require the 4-byte encoding (regression guard).
        EXPECT_EQ("-2147483648", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode(-2147483648, 'int32')))"));
        EXPECT_EQ("-100000", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode(-100000, 'int32')))"));
    }

    TEST_F(SbmdTlvTest, EncodeRoundTripString)
    {
        EXPECT_EQ("hello", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode('hello', 'string')))"));
    }

    // ========================================================================
    // encode: string parsing (radix) and range checking
    // ========================================================================

    TEST_F(SbmdTlvTest, EncodeParsesStrings)
    {
        EXPECT_EQ("42", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode('42', 'uint8')))"));
        EXPECT_EQ("255", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode('ff', 'uint8', 16)))"));
        EXPECT_EQ("5", Eval("String(Sbmd.Tlv.decode(Sbmd.Tlv.encode('101', 'uint8', 2)))"));
    }

    TEST_F(SbmdTlvTest, EncodeRejectsOutOfRangeOrUnparseable)
    {
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.encode(256, 'uint8'))"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.encode(-1, 'uint8'))"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.encode(40000, 'int16'))"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.encode('xyz', 'uint8'))"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.encode('', 'uint8'))"));
        EXPECT_EQ("null", Eval("String(Sbmd.Tlv.encode(1.5, 'uint8'))"));
    }

    TEST_F(SbmdTlvTest, EncodeArgumentErrors)
    {
        // Missing type argument throws.
        EXPECT_EQ("threw",
                  Eval("(function () { try { Sbmd.Tlv.encode(5); return 'no-throw'; }"
                       "               catch (e) { return 'threw'; } })()"));
        // base with string type throws.
        EXPECT_EQ("threw",
                  Eval("(function () { try { Sbmd.Tlv.encode('x', 'string', 16); return 'no-throw'; }"
                       "               catch (e) { return 'threw'; } })()"));
    }

    // ========================================================================
    // encodeStruct / emptyStruct
    // ========================================================================

    TEST_F(SbmdTlvTest, EncodeStructRoundTrip)
    {
        EXPECT_EQ("{\"0\":-5,\"1\":1000}",
                  Eval("JSON.stringify(Sbmd.Tlv.decode(Sbmd.Tlv.encodeStruct("
                       "  { x: -5, y: 1000 },"
                       "  { x: { tag: 0, type: 'int8' }, y: { tag: 1, type: 'uint16' } })))"));
    }

    TEST_F(SbmdTlvTest, EncodeStructOctetString)
    {
        // octstr field round-trips as a Uint8Array (mirrors door-lock PINCode).
        EXPECT_EQ("3:1,2,3",
                  Eval("(function () {"
                       "  var s = Sbmd.Tlv.decode(Sbmd.Tlv.encodeStruct("
                       "    { pin: new Uint8Array([1, 2, 3]) },"
                       "    { pin: { tag: 0, type: 'octstr' } }));"
                       "  var b = s[0];"
                       "  return b.length + ':' + b[0] + ',' + b[1] + ',' + b[2];"
                       "})()"));
    }

    TEST_F(SbmdTlvTest, EmptyStruct)
    {
        EXPECT_EQ("{}", Eval("JSON.stringify(Sbmd.Tlv.decode(Sbmd.Tlv.emptyStruct()))"));
    }

    // ========================================================================
    // TYPE constants are exposed
    // ========================================================================

    TEST_F(SbmdTlvTest, ExposesTypeConstants)
    {
        EXPECT_EQ("true",
                  Eval("String(Sbmd.Tlv.TYPE.STRUCT === 0x15 &&"
                       "       Sbmd.Tlv.TYPE.NULL === 0x14 &&"
                       "       Sbmd.Tlv.TYPE.BOOL_TRUE === 0x09)"));
    }
} // namespace
