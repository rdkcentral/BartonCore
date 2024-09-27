//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2022 Comcast Cable
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
// Comcast Cable retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * repeatingTaskPolicyPrivate.h
 *
 * Private functions that are implemented in repeatingTaskPolicy.c
 *
 * Author: jelderton - 6/16/22
 *-----------------------------------------------*/

#ifndef ZILKER_REPEATINGTASKPOLICYPRIVATE_H
#define ZILKER_REPEATINGTASKPOLICYPRIVATE_H

#include <icConcurrent/repeatingTask.h>

/*
 * Given a task policy created via createIncrementalRepeatingTaskPolicy,
 * this returns a deep clone, but updates the current delay amount.
 * Solely used by changeRepeatingTask
 */
RepeatingTaskPolicy *cloneAndChangeIncrementalRepeatingTaskPolicy(RepeatingTaskPolicy *orig,
                                                                  uint32_t delayAmount, delayUnits units);

#endif //ZILKER_REPEATINGTASKPOLICYPRIVATE_H
