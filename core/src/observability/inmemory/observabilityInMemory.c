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

/*
 * In-memory observability backend.
 *
 * Backs all metric instruments with thread-safe in-process data structures.
 * Counters use atomic uint64, gauges use atomic int64, histograms use a
 * mutex-protected fixed-bucket distribution.
 *
 * Instruments are registered in a global list so observabilityDumpJson()
 * can enumerate and serialize them.
 */

#include "observability/observability.h"
#include "observability/observabilityMetrics.h"

#include <cjson/cJSON.h>

#include <glib.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Attribute key-value pair                                           */
/* ------------------------------------------------------------------ */

typedef struct AttrPair
{
    char *key;
    char *value;
} AttrPair;

typedef struct AttrSet
{
    GSList *pairs;
} AttrSet;

static AttrSet attrSetFromVa(va_list ap)
{
    AttrSet set = {NULL};

    while (true)
    {
        const char *key = va_arg(ap, const char *);

        if (key == NULL)
        {
            break;
        }

        const char *val = va_arg(ap, const char *);

        if (val == NULL)
        {
            break;
        }

        AttrPair *pair = g_new(AttrPair, 1);
        pair->key = g_strdup(key);
        pair->value = g_strdup(val);
        set.pairs = g_slist_append(set.pairs, pair);
    }

    return set;
}

static void attrSetFree(AttrSet *set)
{
    for (GSList *node = set->pairs; node != NULL; node = node->next)
    {
        AttrPair *pair = node->data;
        g_free(pair->key);
        g_free(pair->value);
        g_free(pair);
    }

    g_slist_free(set->pairs);
    set->pairs = NULL;
}

static bool attrSetEqual(const AttrSet *a, const AttrSet *b)
{
    GSList *nodeA = a->pairs;
    GSList *nodeB = b->pairs;

    while (nodeA != NULL && nodeB != NULL)
    {
        AttrPair *pairA = nodeA->data;
        AttrPair *pairB = nodeB->data;

        if (g_strcmp0(pairA->key, pairB->key) != 0 || g_strcmp0(pairA->value, pairB->value) != 0)
        {
            return false;
        }

        nodeA = nodeA->next;
        nodeB = nodeB->next;
    }

    return nodeA == NULL && nodeB == NULL;
}

static AttrSet attrSetClone(const AttrSet *src)
{
    AttrSet dst = {NULL};

    for (GSList *node = src->pairs; node != NULL; node = node->next)
    {
        AttrPair *srcPair = node->data;
        AttrPair *dstPair = g_new(AttrPair, 1);
        dstPair->key = g_strdup(srcPair->key);
        dstPair->value = g_strdup(srcPair->value);
        dst.pairs = g_slist_append(dst.pairs, dstPair);
    }

    return dst;
}

/* ------------------------------------------------------------------ */
/* Keyed data point — value associated with an attribute set          */
/* ------------------------------------------------------------------ */

typedef struct CounterDataPoint
{
    AttrSet attrs;
    uint64_t value;
} CounterDataPoint;

typedef struct GaugeDataPoint
{
    AttrSet attrs;
    int64_t value;
} GaugeDataPoint;

/* ------------------------------------------------------------------ */
/* Histogram buckets                                                  */
/* ------------------------------------------------------------------ */

/* Default bucket boundaries matching OpenTelemetry SDK defaults */
static const double histogramBounds[] = {0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000};

enum
{
    numBounds = G_N_ELEMENTS(histogramBounds)
};

typedef struct HistogramDataPoint
{
    AttrSet attrs;
    uint64_t count;
    double sum;
    double min;
    double max;
    uint64_t buckets[numBounds + 1];
} HistogramDataPoint;

/* ------------------------------------------------------------------ */
/* Instrument types                                                   */
/* ------------------------------------------------------------------ */

typedef enum
{
    INSTRUMENT_COUNTER,
    INSTRUMENT_GAUGE,
    INSTRUMENT_HISTOGRAM
} InstrumentType;

typedef struct InstrumentBase
{
    InstrumentType type;
    char *name;
    char *description;
    char *unit;
} InstrumentBase;

struct ObservabilityCounter
{
    InstrumentBase base;
    pthread_mutex_t lock;
    GSList *dataPoints;
};

struct ObservabilityGauge
{
    InstrumentBase base;
    pthread_mutex_t lock;
    GSList *dataPoints;
};

struct ObservabilityHistogram
{
    InstrumentBase base;
    pthread_mutex_t lock;
    GSList *dataPoints;
};

/* ------------------------------------------------------------------ */
/* Global registry                                                    */
/* ------------------------------------------------------------------ */

static pthread_mutex_t registryLock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static GSList *registry = NULL;
static bool initialized = false;

static void registryAdd(InstrumentBase *inst)
{
    pthread_mutex_lock(&registryLock);
    registry = g_slist_prepend(registry, inst);
    pthread_mutex_unlock(&registryLock);
}

static void registryRemove(InstrumentBase *inst)
{
    pthread_mutex_lock(&registryLock);
    registry = g_slist_remove(registry, inst);
    pthread_mutex_unlock(&registryLock);
}

static void initMutex(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

/* ------------------------------------------------------------------ */
/* Init / Shutdown                                                    */
/* ------------------------------------------------------------------ */

int observabilityInit(void)
{
    pthread_mutex_lock(&registryLock);
    initialized = true;
    pthread_mutex_unlock(&registryLock);

    return 0;
}

void observabilityShutdown(void)
{
    pthread_mutex_lock(&registryLock);
    initialized = false;

    /* Detach all instruments from the registry — does not free them;
     * instruments may still be referenced and must be released individually. */
    g_slist_free(registry);
    registry = NULL;

    pthread_mutex_unlock(&registryLock);
}

/* ------------------------------------------------------------------ */
/* Counter                                                            */
/* ------------------------------------------------------------------ */

/* Returns the data point for the given attribute set, lazily allocating
 * a new slot if no existing data point matches. Each unique attribute
 * set gets its own data point. */
static CounterDataPoint *counterGetOrCreateDataPoint(ObservabilityCounter *counter, const AttrSet *attrs)
{
    for (GSList *node = counter->dataPoints; node != NULL; node = node->next)
    {
        CounterDataPoint *dp = node->data;

        if (attrSetEqual(&dp->attrs, attrs))
        {
            return dp;
        }
    }

    CounterDataPoint *dp = g_new0(CounterDataPoint, 1);
    dp->attrs = attrSetClone(attrs);
    counter->dataPoints = g_slist_append(counter->dataPoints, dp);

    return dp;
}

ObservabilityCounter *observabilityCounterCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityCounter *c = g_atomic_rc_box_new0(ObservabilityCounter);
    c->base.type = INSTRUMENT_COUNTER;
    c->base.name = g_strdup(name);
    c->base.description = g_strdup(description != NULL ? description : "");
    c->base.unit = g_strdup(unit != NULL ? unit : "1");
    initMutex(&c->lock);

    registryAdd(&c->base);

    return c;
}

void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value)
{
    if (counter == NULL)
    {
        return;
    }

    AttrSet empty = {NULL};
    pthread_mutex_lock(&counter->lock);
    CounterDataPoint *dp = counterGetOrCreateDataPoint(counter, &empty);
    dp->value += value;
    pthread_mutex_unlock(&counter->lock);
}

void observabilityCounterAddWithAttrs(ObservabilityCounter *counter, uint64_t value, ...)
{
    if (counter == NULL)
    {
        return;
    }

    va_list ap;
    va_start(ap, value);
    AttrSet attrs = attrSetFromVa(ap);
    va_end(ap);

    pthread_mutex_lock(&counter->lock);
    CounterDataPoint *dp = counterGetOrCreateDataPoint(counter, &attrs);
    dp->value += value;
    pthread_mutex_unlock(&counter->lock);

    attrSetFree(&attrs);
}

static void counterDestroy(gpointer data)
{
    ObservabilityCounter *counter = data;
    registryRemove(&counter->base);
    pthread_mutex_destroy(&counter->lock);

    for (GSList *node = counter->dataPoints; node != NULL; node = node->next)
    {
        CounterDataPoint *dp = node->data;
        attrSetFree(&dp->attrs);
        g_free(dp);
    }

    g_slist_free(counter->dataPoints);
    g_free(counter->base.name);
    g_free(counter->base.description);
    g_free(counter->base.unit);
}

void observabilityCounterRelease(ObservabilityCounter *counter)
{
    if (counter == NULL)
    {
        return;
    }

    g_atomic_rc_box_release_full(counter, counterDestroy);
}

/* ------------------------------------------------------------------ */
/* Gauge                                                              */
/* ------------------------------------------------------------------ */

/* Returns the data point for the given attribute set, lazily allocating
 * a new slot if no existing data point matches. Each unique attribute
 * set gets its own data point. */
static GaugeDataPoint *gaugeGetOrCreateDataPoint(ObservabilityGauge *gauge, const AttrSet *attrs)
{
    for (GSList *node = gauge->dataPoints; node != NULL; node = node->next)
    {
        GaugeDataPoint *dp = node->data;

        if (attrSetEqual(&dp->attrs, attrs))
        {
            return dp;
        }
    }

    GaugeDataPoint *dp = g_new0(GaugeDataPoint, 1);
    dp->attrs = attrSetClone(attrs);
    gauge->dataPoints = g_slist_append(gauge->dataPoints, dp);

    return dp;
}

ObservabilityGauge *observabilityGaugeCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityGauge *g = g_atomic_rc_box_new0(ObservabilityGauge);
    g->base.type = INSTRUMENT_GAUGE;
    g->base.name = g_strdup(name);
    g->base.description = g_strdup(description != NULL ? description : "");
    g->base.unit = g_strdup(unit != NULL ? unit : "1");
    initMutex(&g->lock);

    registryAdd(&g->base);

    return g;
}

void observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value)
{
    if (gauge == NULL)
    {
        return;
    }

    AttrSet empty = {NULL};
    pthread_mutex_lock(&gauge->lock);
    GaugeDataPoint *dp = gaugeGetOrCreateDataPoint(gauge, &empty);
    dp->value = value;
    pthread_mutex_unlock(&gauge->lock);
}

void observabilityGaugeRecordWithAttrs(ObservabilityGauge *gauge, int64_t value, ...)
{
    if (gauge == NULL)
    {
        return;
    }

    va_list ap;
    va_start(ap, value);
    AttrSet attrs = attrSetFromVa(ap);
    va_end(ap);

    pthread_mutex_lock(&gauge->lock);
    GaugeDataPoint *dp = gaugeGetOrCreateDataPoint(gauge, &attrs);
    dp->value = value;
    pthread_mutex_unlock(&gauge->lock);

    attrSetFree(&attrs);
}

static void gaugeDestroy(gpointer data)
{
    ObservabilityGauge *gauge = data;
    registryRemove(&gauge->base);
    pthread_mutex_destroy(&gauge->lock);

    for (GSList *node = gauge->dataPoints; node != NULL; node = node->next)
    {
        GaugeDataPoint *dp = node->data;
        attrSetFree(&dp->attrs);
        g_free(dp);
    }

    g_slist_free(gauge->dataPoints);
    g_free(gauge->base.name);
    g_free(gauge->base.description);
    g_free(gauge->base.unit);
}

void observabilityGaugeRelease(ObservabilityGauge *gauge)
{
    if (gauge == NULL)
    {
        return;
    }

    g_atomic_rc_box_release_full(gauge, gaugeDestroy);
}

/* ------------------------------------------------------------------ */
/* Histogram                                                          */
/* ------------------------------------------------------------------ */

/* Returns the data point for the given attribute set, lazily allocating
 * a new slot if no existing data point matches. Each unique attribute
 * set gets its own data point. */
static HistogramDataPoint *histogramGetOrCreateDataPoint(ObservabilityHistogram *h, const AttrSet *attrs)
{
    for (GSList *node = h->dataPoints; node != NULL; node = node->next)
    {
        HistogramDataPoint *dp = node->data;

        if (attrSetEqual(&dp->attrs, attrs))
        {
            return dp;
        }
    }

    HistogramDataPoint *dp = g_new0(HistogramDataPoint, 1);
    dp->attrs = attrSetClone(attrs);
    h->dataPoints = g_slist_append(h->dataPoints, dp);

    return dp;
}

static void histogramRecordValue(HistogramDataPoint *dp, double value)
{
    dp->count++;
    dp->sum += value;

    if (dp->count == 1)
    {
        dp->min = value;
        dp->max = value;
    }
    else
    {
        if (value < dp->min)
        {
            dp->min = value;
        }

        if (value > dp->max)
        {
            dp->max = value;
        }
    }

    /* Find bucket: first boundary where value <= bound */
    int bucket = numBounds; /* overflow bucket */

    for (int i = 0; i < numBounds; i++)
    {
        if (value <= histogramBounds[i])
        {
            bucket = i;
            break;
        }
    }

    dp->buckets[bucket]++;
}

ObservabilityHistogram *observabilityHistogramCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityHistogram *h = g_atomic_rc_box_new0(ObservabilityHistogram);
    h->base.type = INSTRUMENT_HISTOGRAM;
    h->base.name = g_strdup(name);
    h->base.description = g_strdup(description != NULL ? description : "");
    h->base.unit = g_strdup(unit != NULL ? unit : "1");
    initMutex(&h->lock);

    registryAdd(&h->base);

    return h;
}

void observabilityHistogramRecord(ObservabilityHistogram *histogram, double value)
{
    if (histogram == NULL)
    {
        return;
    }

    AttrSet empty = {NULL};
    pthread_mutex_lock(&histogram->lock);
    HistogramDataPoint *dp = histogramGetOrCreateDataPoint(histogram, &empty);
    histogramRecordValue(dp, value);
    pthread_mutex_unlock(&histogram->lock);
}

void observabilityHistogramRecordWithAttrs(ObservabilityHistogram *histogram, double value, ...)
{
    if (histogram == NULL)
    {
        return;
    }

    va_list ap;
    va_start(ap, value);
    AttrSet attrs = attrSetFromVa(ap);
    va_end(ap);

    pthread_mutex_lock(&histogram->lock);
    HistogramDataPoint *dp = histogramGetOrCreateDataPoint(histogram, &attrs);
    histogramRecordValue(dp, value);
    pthread_mutex_unlock(&histogram->lock);

    attrSetFree(&attrs);
}

static void histogramDestroy(gpointer data)
{
    ObservabilityHistogram *histogram = data;
    registryRemove(&histogram->base);
    pthread_mutex_destroy(&histogram->lock);

    for (GSList *node = histogram->dataPoints; node != NULL; node = node->next)
    {
        HistogramDataPoint *dp = node->data;
        attrSetFree(&dp->attrs);
        g_free(dp);
    }

    g_slist_free(histogram->dataPoints);
    g_free(histogram->base.name);
    g_free(histogram->base.description);
    g_free(histogram->base.unit);
}

void observabilityHistogramRelease(ObservabilityHistogram *histogram)
{
    if (histogram == NULL)
    {
        return;
    }

    g_atomic_rc_box_release_full(histogram, histogramDestroy);
}

/* ------------------------------------------------------------------ */
/* JSON dump                                                          */
/* ------------------------------------------------------------------ */

static cJSON *attrSetToJson(const AttrSet *attrs)
{
    if (attrs->pairs == NULL)
    {
        return NULL;
    }

    cJSON *obj = cJSON_CreateObject();

    for (GSList *node = attrs->pairs; node != NULL; node = node->next)
    {
        AttrPair *pair = node->data;
        cJSON_AddStringToObject(obj, pair->key, pair->value);
    }

    return obj;
}

static cJSON *counterToJson(ObservabilityCounter *c)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "type", "counter");
    cJSON_AddStringToObject(obj, "description", c->base.description);
    cJSON_AddStringToObject(obj, "unit", c->base.unit);

    cJSON *dataPoints = cJSON_CreateArray();

    pthread_mutex_lock(&c->lock);

    for (GSList *node = c->dataPoints; node != NULL; node = node->next)
    {
        CounterDataPoint *cdp = node->data;
        cJSON *dp = cJSON_CreateObject();
        cJSON_AddNumberToObject(dp, "value", (double) cdp->value);

        cJSON *attrs = attrSetToJson(&cdp->attrs);

        if (attrs)
        {
            cJSON_AddItemToObject(dp, "attributes", attrs);
        }

        cJSON_AddItemToArray(dataPoints, dp);
    }

    pthread_mutex_unlock(&c->lock);

    cJSON_AddItemToObject(obj, "dataPoints", dataPoints);

    return obj;
}

static cJSON *gaugeToJson(ObservabilityGauge *g)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "type", "gauge");
    cJSON_AddStringToObject(obj, "description", g->base.description);
    cJSON_AddStringToObject(obj, "unit", g->base.unit);

    cJSON *dataPoints = cJSON_CreateArray();

    pthread_mutex_lock(&g->lock);

    for (GSList *node = g->dataPoints; node != NULL; node = node->next)
    {
        GaugeDataPoint *gdp = node->data;
        cJSON *dp = cJSON_CreateObject();
        cJSON_AddNumberToObject(dp, "value", (double) gdp->value);

        cJSON *attrs = attrSetToJson(&gdp->attrs);

        if (attrs)
        {
            cJSON_AddItemToObject(dp, "attributes", attrs);
        }

        cJSON_AddItemToArray(dataPoints, dp);
    }

    pthread_mutex_unlock(&g->lock);

    cJSON_AddItemToObject(obj, "dataPoints", dataPoints);

    return obj;
}

static cJSON *histogramToJson(ObservabilityHistogram *h)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "type", "histogram");
    cJSON_AddStringToObject(obj, "description", h->base.description);
    cJSON_AddStringToObject(obj, "unit", h->base.unit);

    cJSON *dataPoints = cJSON_CreateArray();

    pthread_mutex_lock(&h->lock);

    for (GSList *node = h->dataPoints; node != NULL; node = node->next)
    {
        HistogramDataPoint *hdp = node->data;
        cJSON *dp = cJSON_CreateObject();
        cJSON_AddNumberToObject(dp, "count", (double) hdp->count);
        cJSON_AddNumberToObject(dp, "sum", hdp->sum);
        cJSON_AddNumberToObject(dp, "min", hdp->min);
        cJSON_AddNumberToObject(dp, "max", hdp->max);

        cJSON *buckets = cJSON_CreateArray();

        for (int b = 0; b <= numBounds; b++)
        {
            cJSON *bucket = cJSON_CreateObject();

            if (b < numBounds)
            {
                cJSON_AddNumberToObject(bucket, "le", histogramBounds[b]);
            }
            else
            {
                cJSON_AddStringToObject(bucket, "le", "+Inf");
            }

            cJSON_AddNumberToObject(bucket, "count", (double) hdp->buckets[b]);
            cJSON_AddItemToArray(buckets, bucket);
        }

        cJSON_AddItemToObject(dp, "buckets", buckets);

        cJSON *attrs = attrSetToJson(&hdp->attrs);

        if (attrs)
        {
            cJSON_AddItemToObject(dp, "attributes", attrs);
        }

        cJSON_AddItemToArray(dataPoints, dp);
    }

    pthread_mutex_unlock(&h->lock);

    cJSON_AddItemToObject(obj, "dataPoints", dataPoints);

    return obj;
}

char *observabilityDumpJson(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *metrics = cJSON_CreateObject();

    pthread_mutex_lock(&registryLock);

    for (GSList *node = registry; node != NULL; node = node->next)
    {
        InstrumentBase *inst = node->data;
        cJSON *metric = NULL;

        switch (inst->type)
        {
            case INSTRUMENT_COUNTER:
                metric = counterToJson((ObservabilityCounter *) inst);
                break;

            case INSTRUMENT_GAUGE:
                metric = gaugeToJson((ObservabilityGauge *) inst);
                break;

            case INSTRUMENT_HISTOGRAM:
                metric = histogramToJson((ObservabilityHistogram *) inst);
                break;
        }

        if (metric)
        {
            cJSON_AddItemToObject(metrics, inst->name, metric);
        }
    }

    pthread_mutex_unlock(&registryLock);

    cJSON_AddItemToObject(root, "metrics", metrics);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}
