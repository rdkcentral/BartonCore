//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2016 iControl Networks, Inc.
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
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------

/*-----------------------------------------------
 * legacyProtectConfig.h
 *
 * the legacy (Java) configuration decrypt ported to C
 * mainly used for migration from older firmware.
 *
 * Author: kgandhi
 *-----------------------------------------------*/

#include <stdbool.h>

// allow enable/disable support of legacy encryption
#ifdef CONFIG_LIB_LEGACY_ENCRYPTION

#ifndef _PROTECTCONFIG_H_
#define _PROTECTCONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Support decrypting strings that were encrypted with the legacy Java product
 * caller must free "out" on success
 *
 * @param *str Encrypt Base64 string, terminated with '\0'
 * @param **out pointer to decrypt buffer, terminated with '\0' (*out[*len], won't affect content) malloc() during decryption
 * @param *len length of out, set to 0 if failed
 *
 * @return SUCCESS or FAILURE
 */
bool decryptConfigString(const char *str, unsigned char **out, size_t * len);

/*check if the compiler is of C++ */
#ifdef __cplusplus
}
#endif
#endif // _PROTECTCONFIG_H_

#endif // CONFIG_LIB_LEGACY_ENCRYPTION
