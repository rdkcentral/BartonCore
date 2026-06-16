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

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    AttrPair *pairs;
    int count;
} AttrSet;

static AttrSet attrSetFromVa(va_list ap)
{
    AttrSet set = {NULL, 0};
    int cap = 4;
    set.pairs = malloc(sizeof(AttrPair) * cap);

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

        if (set.count >= cap)
        {
            cap *= 2;
            set.pairs = realloc(set.pairs, sizeof(AttrPair) * cap);
        }

        set.pairs[set.count].key = strdup(key);
        set.pairs[set.count].value = strdup(val);
        set.count++;
    }

    return set;
}

static void attrSetFree(AttrSet *set)
{
    for (int i = 0; i < set->count; i++)
    {
        free(set->pairs[i].key);
        free(set->pairs[i].value);
    }

    free(set->pairs);
    set->pairs = NULL;
    set->count = 0;
}

static bool attrSetEqual(const AttrSet *a, const AttrSet *b)
{
    if (a->count != b->count)
    {
        return false;
    }

    for (int i = 0; i < a->count; i++)
    {
        if (strcmp(a->pairs[i].key, b->pairs[i].key) != 0 ||
            strcmp(a->pairs[i].value, b->pairs[i].value) != 0)
        {
            return false;
        }
    }

    return true;
}

static AttrSet attrSetClone(const AttrSet *src)
{
    AttrSet dst = {NULL, src->count};

    if (src->count > 0)
    {
        dst.pairs = malloc(sizeof(AttrPair) * src->count);

        for (int i = 0; i < src->count; i++)
        {
            dst.pairs[i].key = strdup(src->pairs[i].key);
            dst.pairs[i].value = strdup(src->pairs[i].value);
        }
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
static const double kHistogramBounds[] = {
    0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000};
static const int kNumBounds = sizeof(kHistogramBounds) / sizeof(kHistogramBounds[0]);

typedef struct HistogramDataPoint
{
    AttrSet attrs;
    uint64_t count;
    double sum;
    double min;
    double max;
    uint64_t buckets[sizeof(kHistogramBounds) / sizeof(kHistogramBounds[0]) + 1];
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
    int refCount;
    struct InstrumentBase *next; /* linked list in global registry */
} InstrumentBase;

struct ObservabilityCounter
{
    InstrumentBase base;
    pthread_mutex_t lock;
    CounterDataPoint *dataPoints;
    int dataPointCount;
    int dataPointCapacity;
};

struct ObservabilityGauge
{
    InstrumentBase base;
    pthread_mutex_t lock;
    GaugeDataPoint *dataPoints;
    int dataPointCount;
    int dataPointCapacity;
};

struct ObservabilityHistogram
{
    InstrumentBase base;
    pthread_mutex_t lock;
    HistogramDataPoint *dataPoints;
    int dataPointCount;
    int dataPointCapacity;
};

/* ------------------------------------------------------------------ */
/* Global registry                                                    */
/* ------------------------------------------------------------------ */

static pthread_mutex_t registryLock = PTHREAD_MUTEX_INITIALIZER;
static InstrumentBase *registryHead = NULL;
static bool initialized = false;

static void registryAdd(InstrumentBase *inst)
{
    pthread_mutex_lock(&registryLock);
    inst->next = registryHead;
    registryHead = inst;
    pthread_mutex_unlock(&registryLock);
}

static void registryRemove(InstrumentBase *inst)
{
    pthread_mutex_lock(&registryLock);
    InstrumentBase **pp = &registryHead;

    while (*pp)
    {
        if (*pp == inst)
        {
            *pp = inst->next;
            break;
        }

        pp = &(*pp)->next;
    }

    pthread_mutex_unlock(&registryLock);
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

    /* Release all instruments still in the registry */
    while (registryHead)
    {
        InstrumentBase *inst = registryHead;
        registryHead = inst->next;
        inst->next = NULL;

        /* We don't free here — instruments may still be referenced.
         * Just detach from registry. */
    }

    pthread_mutex_unlock(&registryLock);
}

/* ------------------------------------------------------------------ */
/* Counter                                                            */
/* ------------------------------------------------------------------ */

static CounterDataPoint *counterFindOrAdd(ObservabilityCounter *counter, const AttrSet *attrs)
{
    for (int i = 0; i < counter->dataPointCount; i++)
    {
        if (attrSetEqual(&counter->dataPoints[i].attrs, attrs))
        {
            return &counter->dataPoints[i];
        }
    }

    if (counter->dataPointCount >= counter->dataPointCapacity)
    {
        counter->dataPointCapacity = counter->dataPointCapacity == 0 ? 4 : counter->dataPointCapacity * 2;
        counter->dataPoints = realloc(counter->dataPoints, sizeof(CounterDataPoint) * counter->dataPointCapacity);
    }

    CounterDataPoint *dp = &counter->dataPoints[counter->dataPointCount++];
    dp->attrs = attrSetClone(attrs);
    dp->value = 0;

    return dp;
}

ObservabilityCounter *observabilityCounterCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityCounter *c = calloc(1, sizeof(ObservabilityCounter));
    c->base.type = INSTRUMENT_COUNTER;
    c->base.name = strdup(name);
    c->base.description = description ? strdup(description) : strdup("");
    c->base.unit = unit ? strdup(unit) : strdup("1");
    c->base.refCount = 1;
    pthread_mutex_init(&c->lock, NULL);

    registryAdd(&c->base);

    return c;
}

void observabilityCounterAdd(ObservabilityCounter *counter, uint64_t value)
{
    if (counter == NULL)
    {
        return;
    }

    AttrSet empty = {NULL, 0};
    pthread_mutex_lock(&counter->lock);
    CounterDataPoint *dp = counterFindOrAdd(counter, &empty);
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
    CounterDataPoint *dp = counterFindOrAdd(counter, &attrs);
    dp->value += value;
    pthread_mutex_unlock(&counter->lock);

    attrSetFree(&attrs);
}

void observabilityCounterRelease(ObservabilityCounter *counter)
{
    if (counter == NULL)
    {
        return;
    }

    int remaining = __sync_sub_and_fetch(&counter->base.refCount, 1);

    if (remaining <= 0)
    {
        registryRemove(&counter->base);
        pthread_mutex_destroy(&counter->lock);

        for (int i = 0; i < counter->dataPointCount; i++)
        {
            attrSetFree(&counter->dataPoints[i].attrs);
        }

        free(counter->dataPoints);
        free(counter->base.name);
        free(counter->base.description);
        free(counter->base.unit);
        free(counter);
    }
}

/* ------------------------------------------------------------------ */
/* Gauge                                                              */
/* ------------------------------------------------------------------ */

static GaugeDataPoint *gaugeFindOrAdd(ObservabilityGauge *gauge, const AttrSet *attrs)
{
    for (int i = 0; i < gauge->dataPointCount; i++)
    {
        if (attrSetEqual(&gauge->dataPoints[i].attrs, attrs))
        {
            return &gauge->dataPoints[i];
        }
    }

    if (gauge->dataPointCount >= gauge->dataPointCapacity)
    {
        gauge->dataPointCapacity = gauge->dataPointCapacity == 0 ? 4 : gauge->dataPointCapacity * 2;
        gauge->dataPoints = realloc(gauge->dataPoints, sizeof(GaugeDataPoint) * gauge->dataPointCapacity);
    }

    GaugeDataPoint *dp = &gauge->dataPoints[gauge->dataPointCount++];
    dp->attrs = attrSetClone(attrs);
    dp->value = 0;

    return dp;
}

ObservabilityGauge *observabilityGaugeCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityGauge *g = calloc(1, sizeof(ObservabilityGauge));
    g->base.type = INSTRUMENT_GAUGE;
    g->base.name = strdup(name);
    g->base.description = description ? strdup(description) : strdup("");
    g->base.unit = unit ? strdup(unit) : strdup("1");
    g->base.refCount = 1;
    pthread_mutex_init(&g->lock, NULL);

    registryAdd(&g->base);

    return g;
}

void observabilityGaugeRecord(ObservabilityGauge *gauge, int64_t value)
{
    if (gauge == NULL)
    {
        return;
    }

    AttrSet empty = {NULL, 0};
    pthread_mutex_lock(&gauge->lock);
    GaugeDataPoint *dp = gaugeFindOrAdd(gauge, &empty);
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
    GaugeDataPoint *dp = gaugeFindOrAdd(gauge, &attrs);
    dp->value = value;
    pthread_mutex_unlock(&gauge->lock);

    attrSetFree(&attrs);
}

void observabilityGaugeRelease(ObservabilityGauge *gauge)
{
    if (gauge == NULL)
    {
        return;
    }

    int remaining = __sync_sub_and_fetch(&gauge->base.refCount, 1);

    if (remaining <= 0)
    {
        registryRemove(&gauge->base);
        pthread_mutex_destroy(&gauge->lock);

        for (int i = 0; i < gauge->dataPointCount; i++)
        {
            attrSetFree(&gauge->dataPoints[i].attrs);
        }

        free(gauge->dataPoints);
        free(gauge->base.name);
        free(gauge->base.description);
        free(gauge->base.unit);
        free(gauge);
    }
}

/* ------------------------------------------------------------------ */
/* Histogram                                                          */
/* ------------------------------------------------------------------ */

static HistogramDataPoint *histogramFindOrAdd(ObservabilityHistogram *h, const AttrSet *attrs)
{
    for (int i = 0; i < h->dataPointCount; i++)
    {
        if (attrSetEqual(&h->dataPoints[i].attrs, attrs))
        {
            return &h->dataPoints[i];
        }
    }

    if (h->dataPointCount >= h->dataPointCapacity)
    {
        h->dataPointCapacity = h->dataPointCapacity == 0 ? 4 : h->dataPointCapacity * 2;
        h->dataPoints = realloc(h->dataPoints, sizeof(HistogramDataPoint) * h->dataPointCapacity);
    }

    HistogramDataPoint *dp = &h->dataPoints[h->dataPointCount++];
    dp->attrs = attrSetClone(attrs);
    dp->count = 0;
    dp->sum = 0;
    dp->min = 0;
    dp->max = 0;
    memset(dp->buckets, 0, sizeof(dp->buckets));

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
    int bucket = kNumBounds; /* overflow bucket */

    for (int i = 0; i < kNumBounds; i++)
    {
        if (value <= kHistogramBounds[i])
        {
            bucket = i;
            break;
        }
    }

    dp->buckets[bucket]++;
}

ObservabilityHistogram *observabilityHistogramCreate(const char *name, const char *description, const char *unit)
{
    ObservabilityHistogram *h = calloc(1, sizeof(ObservabilityHistogram));
    h->base.type = INSTRUMENT_HISTOGRAM;
    h->base.name = strdup(name);
    h->base.description = description ? strdup(description) : strdup("");
    h->base.unit = unit ? strdup(unit) : strdup("1");
    h->base.refCount = 1;
    pthread_mutex_init(&h->lock, NULL);

    registryAdd(&h->base);

    return h;
}

void observabilityHistogramRecord(ObservabilityHistogram *histogram, double value)
{
    if (histogram == NULL)
    {
        return;
    }

    AttrSet empty = {NULL, 0};
    pthread_mutex_lock(&histogram->lock);
    HistogramDataPoint *dp = histogramFindOrAdd(histogram, &empty);
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
    HistogramDataPoint *dp = histogramFindOrAdd(histogram, &attrs);
    histogramRecordValue(dp, value);
    pthread_mutex_unlock(&histogram->lock);

    attrSetFree(&attrs);
}

void observabilityHistogramRelease(ObservabilityHistogram *histogram)
{
    if (histogram == NULL)
    {
        return;
    }

    int remaining = __sync_sub_and_fetch(&histogram->base.refCount, 1);

    if (remaining <= 0)
    {
        registryRemove(&histogram->base);
        pthread_mutex_destroy(&histogram->lock);

        for (int i = 0; i < histogram->dataPointCount; i++)
        {
            attrSetFree(&histogram->dataPoints[i].attrs);
        }

        free(histogram->dataPoints);
        free(histogram->base.name);
        free(histogram->base.description);
        free(histogram->base.unit);
        free(histogram);
    }
}

/* ------------------------------------------------------------------ */
/* JSON dump                                                          */
/* ------------------------------------------------------------------ */

static cJSON *attrSetToJson(const AttrSet *attrs)
{
    if (attrs->count == 0)
    {
        return NULL;
    }

    cJSON *obj = cJSON_CreateObject();

    for (int i = 0; i < attrs->count; i++)
    {
        cJSON_AddStringToObject(obj, attrs->pairs[i].key, attrs->pairs[i].value);
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

    for (int i = 0; i < c->dataPointCount; i++)
    {
        cJSON *dp = cJSON_CreateObject();
        cJSON_AddNumberToObject(dp, "value", (double) c->dataPoints[i].value);

        cJSON *attrs = attrSetToJson(&c->dataPoints[i].attrs);

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

    for (int i = 0; i < g->dataPointCount; i++)
    {
        cJSON *dp = cJSON_CreateObject();
        cJSON_AddNumberToObject(dp, "value", (double) g->dataPoints[i].value);

        cJSON *attrs = attrSetToJson(&g->dataPoints[i].attrs);

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

    for (int i = 0; i < h->dataPointCount; i++)
    {
        HistogramDataPoint *hdp = &h->dataPoints[i];
        cJSON *dp = cJSON_CreateObject();
        cJSON_AddNumberToObject(dp, "count", (double) hdp->count);
        cJSON_AddNumberToObject(dp, "sum", hdp->sum);
        cJSON_AddNumberToObject(dp, "min", hdp->min);
        cJSON_AddNumberToObject(dp, "max", hdp->max);

        cJSON *buckets = cJSON_CreateArray();

        for (int b = 0; b <= kNumBounds; b++)
        {
            cJSON *bucket = cJSON_CreateObject();

            if (b < kNumBounds)
            {
                cJSON_AddNumberToObject(bucket, "le", kHistogramBounds[b]);
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

    for (InstrumentBase *inst = registryHead; inst != NULL; inst = inst->next)
    {
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
