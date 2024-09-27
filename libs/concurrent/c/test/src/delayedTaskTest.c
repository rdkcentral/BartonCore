//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2018 Comcast
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
// Created by mkoch201 on 2/8/19.
//

#include <unistd.h>
#include <icConcurrent/delayedTask.h>
#include <icLog/logging.h>

#define LOG_TAG "delayedTaskTest"

static void delayCallback(void * arg)
{
    const char *str = (char *)arg;
    icLogDebug(LOG_TAG, "Delay callback called: %s", str);
}

/*
 * main
 */
int main(int argc, const char **argv)
{
    icLogDebug(LOG_TAG, "Scheduling start...");
    scheduleDelayTask(100, DELAY_MILLIS, delayCallback, "100 Millisecond delay");
    scheduleDelayTask(1, DELAY_SECS, delayCallback, "1 second delay");
    scheduleDelayTask(5, DELAY_SECS, delayCallback, "5 second delay");
    scheduleDelayTask(10, DELAY_SECS, delayCallback, "10 second delay");

    sleep(12);
}
