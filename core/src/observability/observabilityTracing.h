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

#ifndef OBSERVABILITY_TRACING_H
#define OBSERVABILITY_TRACING_H

#include <stdint.h>

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque span handle */
typedef struct ObservabilitySpan ObservabilitySpan;

/** Opaque span context handle for propagation */
typedef struct ObservabilitySpanContext ObservabilitySpanContext;

#ifdef BARTON_CONFIG_OBSERVABILITY

/**
 * Start a new root span with the given name.
 * @param name  Span name (e.g., "device.discovery")
 * @return Span handle, or NULL on failure. Caller owns one reference; use g_autoptr or observabilitySpanRelease().
 */
ObservabilitySpan *observabilitySpanStart(const char *name);

/**
 * Start a new child span with a parent context.
 * @param name    Span name
 * @param parent  Parent span context (NULL creates a root span)
 * @return Span handle, or NULL on failure. Caller owns one reference; use g_autoptr or observabilitySpanRelease().
 */
ObservabilitySpan *observabilitySpanStartWithParent(const char *name, const ObservabilitySpanContext *parent);

/**
 * Start a new root span with an OTel span link to an existing context.
 * The returned span is NOT a child of @p linked; it starts its own trace but carries a link reference.
 * @param name    Span name
 * @param linked  Span context to link (NULL creates a plain root span with no link)
 * @return Span handle, or NULL on failure. Caller owns one reference; use g_autoptr or observabilitySpanRelease().
 */
ObservabilitySpan *observabilitySpanStartWithLink(const char *name, const ObservabilitySpanContext *linked);

/**
 * Release a span reference. Ends the span and frees when the last reference is dropped.
 * @param span  Span to release (NULL is safe no-op)
 */
void observabilitySpanRelease(ObservabilitySpan *span);

/**
 * Set a string attribute on an active span.
 * @param span   Span to annotate (NULL is safe no-op)
 * @param key    Attribute key
 * @param value  Attribute value
 */
void observabilitySpanSetAttribute(ObservabilitySpan *span, const char *key, const char *value);

/**
 * Set an integer attribute on an active span.
 * @param span   Span to annotate (NULL is safe no-op)
 * @param key    Attribute key
 * @param value  Attribute value
 */
void observabilitySpanSetAttributeInt(ObservabilitySpan *span, const char *key, int64_t value);

/**
 * Mark a span as errored with a message.
 * @param span     Span to mark (NULL is safe no-op)
 * @param message  Error description
 */
void observabilitySpanSetError(ObservabilitySpan *span, const char *message);

/**
 * Extract the span context for propagation to other threads/functions.
 * @param span  Source span (NULL returns NULL)
 * @return Context handle. Caller owns one reference; use g_autoptr or observabilitySpanContextRelease().
 */
ObservabilitySpanContext *observabilitySpanGetContext(ObservabilitySpan *span);

/**
 * Acquire an additional reference on a span context.
 * @param ctx  Context to ref (NULL is safe no-op)
 */
void observabilitySpanContextRef(ObservabilitySpanContext *ctx);

/**
 * Release a span context reference. Frees when the last reference is dropped.
 * @param ctx  Context to release (NULL is safe no-op)
 */
void observabilitySpanContextRelease(ObservabilitySpanContext *ctx);

/**
 * Set the current thread's span context. Refs the new context and releases any previous one.
 * @param ctx  Context to set as current (NULL clears the current context)
 */
void observabilitySpanContextSetCurrent(ObservabilitySpanContext *ctx);

/**
 * Get the current thread's span context (borrowed reference — caller must NOT release).
 * @return Current context, or NULL if none is set.
 */
ObservabilitySpanContext *observabilitySpanContextGetCurrent(void);

#else /* !BARTON_CONFIG_OBSERVABILITY */

static inline ObservabilitySpan *observabilitySpanStart(const char *name)
{
    (void) name;
    return (ObservabilitySpan *) 0;
}
static inline ObservabilitySpan *observabilitySpanStartWithParent(const char *name,
                                                                  const ObservabilitySpanContext *parent)
{
    (void) name;
    (void) parent;
    return (ObservabilitySpan *) 0;
}
static inline ObservabilitySpan *observabilitySpanStartWithLink(const char *name,
                                                                const ObservabilitySpanContext *linked)
{
    (void) name;
    (void) linked;
    return (ObservabilitySpan *) 0;
}
static inline void observabilitySpanRelease(ObservabilitySpan *span)
{
    (void) span;
}
static inline void observabilitySpanSetAttribute(ObservabilitySpan *span, const char *key, const char *value)
{
    (void) span;
    (void) key;
    (void) value;
}
static inline void observabilitySpanSetAttributeInt(ObservabilitySpan *span, const char *key, int64_t value)
{
    (void) span;
    (void) key;
    (void) value;
}
static inline void observabilitySpanSetError(ObservabilitySpan *span, const char *message)
{
    (void) span;
    (void) message;
}
static inline ObservabilitySpanContext *observabilitySpanGetContext(ObservabilitySpan *span)
{
    (void) span;
    return (ObservabilitySpanContext *) 0;
}
static inline void observabilitySpanContextRef(ObservabilitySpanContext *ctx)
{
    (void) ctx;
}
static inline void observabilitySpanContextRelease(ObservabilitySpanContext *ctx)
{
    (void) ctx;
}
static inline void observabilitySpanContextSetCurrent(ObservabilitySpanContext *ctx)
{
    (void) ctx;
}
static inline ObservabilitySpanContext *observabilitySpanContextGetCurrent(void)
{
    return (ObservabilitySpanContext *) 0;
}

#endif /* BARTON_CONFIG_OBSERVABILITY */

#ifdef __cplusplus
}
#endif

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilitySpan, observabilitySpanRelease)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(ObservabilitySpanContext, observabilitySpanContextRelease)

#endif /* OBSERVABILITY_TRACING_H */
