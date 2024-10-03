//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2021 Comcast Corporation
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
// Comcast Corporation retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

//
// Created by tlea on 12/7/21.
//

#ifndef ZILKER_THREADSUBSYSTEM_H
#define ZILKER_THREADSUBSYSTEM_H

extern "C" {
#include "subsystems/thread/threadNetworkInfo.h"
}

#include <stdbool.h>

#define THREAD_SUBSYSTEM_NAME "thread"

/**
 * Retrieve current thread network information.
 *
 * @param info a pointer to an allocated ThreadNetworkInfo struct that will receive the information
 *
 * @return true on success
 */
bool threadSubsystemGetNetworkInfo(ThreadNetworkInfo *info);

#endif // ZILKER_THREADSUBSYSTEM_H
