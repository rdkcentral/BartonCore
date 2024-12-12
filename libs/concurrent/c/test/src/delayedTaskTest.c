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
//
// Created by mkoch201 on 2/8/19.
//

#include <icConcurrent/delayedTask.h>
#include <icLog/logging.h>
#include <unistd.h>

#define LOG_TAG "delayedTaskTest"

static void delayCallback(void *arg)
{
    const char *str = (char *) arg;
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
