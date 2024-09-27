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
// Created by mkoch201 on 4/18/19.
//

#ifndef ZILKER_RESOURCEMODES_H
#define ZILKER_RESOURCEMODES_H

//Resource can be read
#define RESOURCE_MODE_READABLE              (1u << 0u)

//Resource can be written
#define RESOURCE_MODE_WRITEABLE             (1u << 1u)

//Resource can be executed
#define RESOURCE_MODE_EXECUTABLE            (1u << 2u)

//Resource can change dynamically
#define RESOURCE_MODE_DYNAMIC_CAPABLE       (1u << 3u)

//Resource dynamic behavior is enabled
#define RESOURCE_MODE_DYNAMIC               (1u << 4u)

//Resources emit events upon change if set (this mode bit can be changed via API to start/stop resource changed events)
#define RESOURCE_MODE_EMIT_EVENTS           (1u << 5u)

//Resources that are non-critical and are ok to save upon the next save operation.
// (changes to these resources could be lost under error conditions))
#define RESOURCE_MODE_LAZY_SAVE_NEXT        (1u << 6u)

//Resources start shouldn't be stored in clear text (user names, passwords, etc)
#define RESOURCE_MODE_SENSITIVE             (1u << 7u)

//Convenience mask combinations
#define RESOURCE_MODE_READWRITEABLE         (RESOURCE_MODE_READABLE | RESOURCE_MODE_WRITEABLE)

#endif //ZILKER_RESOURCEMODES_H
