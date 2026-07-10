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
 * SafeJSValue is an engine-neutral, owning wrapper around a JSValue that keeps
 * the value alive for the wrapper's lifetime. It hides the two very different
 * memory models the SBMD JS engines use behind a single, hard-to-misuse type:
 *
 *   - mquickjs uses a moving/compacting GC with a root list (JS_AddGCRef /
 *     JS_DeleteGCRef), not reference counting. A held value stays alive only
 *     while it is rooted, and the collector may relocate it, updating the
 *     rooted slot in place. SafeJSValue owns a GC root, so Get() always reads
 *     the GC-updated slot and is never a stale snapshot.
 *
 *   - quickjs uses reference counting (JS_DupValue / JS_FreeValue) and never
 *     relocates objects. SafeJSValue owns a reference, freed on destruction.
 *
 * The contract is uniform across both engines:
 *   - Construction takes ownership of a strong reference that keeps the value
 *     alive until the wrapper is destroyed or moved from.
 *   - AddObject(key) creates a child object, attaches it to *this, and returns
 *     a SafeJSValue that independently keeps the child alive. The returned
 *     wrapper is always current (never a stale snapshot).
 *   - Get() / operator JSValue always yields a currently-valid JSValue.
 *
 * SafeJSValue is non-copyable (a JS reference is single-owner) but movable, so
 * it can be stored as a member of, and relocated between, other types without
 * any ordering hazard: there is no "populate before attaching" or "root before
 * moving" sequence a caller can get wrong. A default-constructed or moved-from
 * wrapper owns nothing (HasValue() == false) and is safe to destroy or assign
 * over.
 *
 * All operations require the caller to hold the shared runtime mutex (e.g.
 * MQuickJsRuntime::GetMutex()), since they operate on the shared context.
 */

#pragma once

#include <cstddef> // IWYU pragma: keep — mquickjs.h below uses size_t but does not include it
#include <cstdint>

#if defined(BCORE_USE_MQUICKJS)
extern "C" {
#include <mquickjs/mquickjs.h>
}
#elif defined(BCORE_USE_QUICKJS)
#include <quickjs/quickjs.h>
#else
#error "SafeJSValue requires BCORE_USE_MQUICKJS or BCORE_USE_QUICKJS to be defined"
#endif

namespace barton
{
    class SafeJSValue
    {
    public:
        /*
         * Construct an empty wrapper that owns no value (HasValue() == false). Useful for members
         * that are populated later via move-assignment.
         */
        SafeJSValue() = default;

        /*
         * Take ownership of a strong reference to v that keeps it alive for this wrapper's lifetime.
         *
         * ctx - the shared JS context; the caller must hold the runtime mutex.
         * v   - the JSValue to hold.
         */
        SafeJSValue(JSContext *ctx, JSValue v)
        {
#if defined(BCORE_USE_MQUICKJS)
            JS_AddGCRef(ctx, &ref); // register a GC root (resets ref.val)
            ref.val = v;            // assign AFTER AddGCRef
            this->ctx = ctx;
#elif defined(BCORE_USE_QUICKJS)
            this->ctx = ctx;
            value = JS_DupValue(ctx, v); // own a reference
#endif
        }

        /*
         * Release the owned reference (GC root on mquickjs, refcount on quickjs). A no-op for an
         * empty or moved-from wrapper.
         */
        ~SafeJSValue() { Reset(); }

        /*
         * Non-copyable: a JS reference is single-owner.
         */
        SafeJSValue(const SafeJSValue &) = delete;
        SafeJSValue &operator=(const SafeJSValue &) = delete;

        /*
         * Movable: ownership transfers to the destination and the source becomes empty. On mquickjs
         * this re-registers a fresh root at the destination's address (the GC list tracks node
         * addresses, so the root cannot simply be memcpy'd); on quickjs the reference is transferred
         * with no additional dup/free. Either way there is no caller-visible ordering to get wrong.
         */
        SafeJSValue(SafeJSValue &&other) noexcept { MoveFrom(other); }

        SafeJSValue &operator=(SafeJSValue &&other) noexcept
        {
            if (this != &other)
            {
                Reset();
                MoveFrom(other);
            }

            return *this;
        }

        /*
         * Abandon ownership without releasing the underlying reference: the wrapper becomes empty
         * (HasValue() == false) and its destructor becomes a no-op, but the GC root (mquickjs) or
         * reference (quickjs) is intentionally leaked.
         *
         * Use only when the reference cannot be released safely — e.g. the owning context is being
         * torn down and may already be gone, so calling JS_DeleteGCRef / JS_FreeValue would be
         * unsafe. Leaking is the lesser evil in that narrow case.
         */
        void Detach() { ctx = nullptr; }

        /*
         * Report whether this wrapper owns a value.
         *
         * Returns false for a default-constructed or moved-from wrapper, true otherwise.
         */
        bool HasValue() const { return ctx != nullptr; }

        /*
         * Return the underlying JSValue, always current.
         *
         * Returns JS_UNDEFINED for an empty or moved-from wrapper.
         */
        JSValue Get() const
        {
#if defined(BCORE_USE_MQUICKJS)
            return ctx != nullptr ? ref.val : JS_UNDEFINED;
#elif defined(BCORE_USE_QUICKJS)
            return value;
#endif
        }

        /*
         * Implicit conversion to JSValue for interop with the raw JS_* API. Returns the same value
         * as Get().
         */
        operator JSValue() const { return Get(); }

        /*
         * Create a child object, attach it to *this under `key`, and return a SafeJSValue that
         * independently keeps the child alive. The returned wrapper is always current.
         *
         * key - the property name under which the new child object is attached to *this.
         *
         * Returns an owning SafeJSValue viewing the newly created and attached child object.
         */
        SafeJSValue AddObject(const char *key)
        {
            JS_SetPropertyStr(ctx, Get(), key, JS_NewObject(ctx)); // create + attach in one step

#if defined(BCORE_USE_MQUICKJS)
            // *this is rooted, so Get() is current. Re-read the child to obtain a current handle,
            // then root it in its own slot so the returned wrapper is never a stale snapshot.
            return SafeJSValue(ctx, JS_GetPropertyStr(ctx, Get(), key));
#elif defined(BCORE_USE_QUICKJS)
            // JS_GetPropertyStr returns an owned (+1) reference; adopt it without an extra dup.
            return SafeJSValue(ctx, JS_GetPropertyStr(ctx, Get(), key), Adopt {});
#endif
        }

        /*
         * Set property `key` on this object to a new uint32 JS value.
         *
         * key - the property name to assign.
         * v   - the unsigned 32-bit value to store.
         */
        void SetUint32(const char *key, uint32_t v) { JS_SetPropertyStr(ctx, Get(), key, JS_NewUint32(ctx, v)); }

        /*
         * Set property `key` on this object to a new int32 JS value.
         *
         * key - the property name to assign.
         * v   - the signed 32-bit value to store.
         */
        void SetInt32(const char *key, int32_t v) { JS_SetPropertyStr(ctx, Get(), key, JS_NewInt32(ctx, v)); }

        /*
         * Set property `key` on this object to a new JS string.
         *
         * key - the property name to assign.
         * v   - the null-terminated C string to store (copied into a JS string).
         */
        void SetString(const char *key, const char *v) { JS_SetPropertyStr(ctx, Get(), key, JS_NewString(ctx, v)); }

        /*
         * Set property `key` on this object to JS null.
         *
         * key - the property name to assign.
         */
        void SetNull(const char *key) { JS_SetPropertyStr(ctx, Get(), key, JS_NULL); }

        /*
         * Set property `key` on this object to an already-constructed JSValue (e.g. a caller-provided
         * handlerContext).
         *
         * key - the property name to assign.
         * v   - the existing JSValue to attach.
         */
        void SetValue(const char *key, JSValue v) { JS_SetPropertyStr(ctx, Get(), key, v); }

    private:
#if defined(BCORE_USE_QUICKJS)
        struct Adopt
        {
        };

        /*
         * Adopt an already-owned (+1) reference without an additional dup; used by AddObject on
         * quickjs where JS_GetPropertyStr returns ownership.
         */
        SafeJSValue(JSContext *ctx, JSValue v, Adopt) : ctx(ctx), value(v) {}
#endif

        /*
         * Release the owned reference and return to the empty state.
         */
        void Reset()
        {
            if (ctx == nullptr)
            {
                return;
            }

#if defined(BCORE_USE_MQUICKJS)
            JS_DeleteGCRef(ctx, &ref);
#elif defined(BCORE_USE_QUICKJS)
            JS_FreeValue(ctx, value);
#endif
            ctx = nullptr;
        }

        /*
         * Transfer ownership from `other` into *this (which must be empty), leaving `other` empty.
         */
        void MoveFrom(SafeJSValue &other)
        {
            if (other.ctx == nullptr)
            {
                return;
            }

            ctx = other.ctx;

#if defined(BCORE_USE_MQUICKJS)
            JS_AddGCRef(ctx, &ref);  // link our own node (resets ref.val)
            ref.val = other.ref.val; // copy the GC-current value from the still-rooted source
            JS_DeleteGCRef(other.ctx, &other.ref);
#elif defined(BCORE_USE_QUICKJS)
            value = other.value; // transfer the reference; no dup/free
#endif
            other.ctx = nullptr;
        }

        JSContext *ctx = nullptr;
#if defined(BCORE_USE_MQUICKJS)
        JSGCRef ref {};
#elif defined(BCORE_USE_QUICKJS)
        JSValue value = JS_UNDEFINED;
#endif
    };

} // namespace barton
