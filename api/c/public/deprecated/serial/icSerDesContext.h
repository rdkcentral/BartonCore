//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2019 Comcast
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
// Comcast retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
//
// Created by mdeleo739 on 6/10/19.
//

#ifndef ZILKER_ICSERDESCONTEXT_H
#define ZILKER_ICSERDESCONTEXT_H

#include <stdbool.h>
#include <icTypes/icStringHashMap.h>

typedef struct {
    icStringHashMap *props;
} icSerDesContext;

icSerDesContext *serDesCreateContext(void);

void serDesDestroyContext(icSerDesContext *context);

bool serDesSetContextValue(icSerDesContext *context, const char *key, const char *value);

bool serDesHasContextValue(const icSerDesContext *context, const char *key);

const char *serDesGetContextValue(const icSerDesContext *context, const char *key);

#endif //ZILKER_ICSERDESCONTEXT_H
