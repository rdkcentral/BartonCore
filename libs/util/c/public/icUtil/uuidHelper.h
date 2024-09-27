//------------------------------ tabstop = 4 ----------------------------------
// 
// Copyright (C) 2020 Comcast
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
// Created by mkoch201 on 8/6/20.
//

#ifndef ZILKER_UUIDHELPER_H
#define ZILKER_UUIDHELPER_H

/**
 * Generate a new 128 bit uuid and return it in string form (xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx)
 * @return the uuid string, caller must free
 */
char *generateUUIDString(void);

#endif //ZILKER_UUIDHELPER_H
