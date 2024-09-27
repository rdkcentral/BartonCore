//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2020 Comcast Corporation
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

#ifndef ZILKER_CRYPTOPRIVATE_H
#define ZILKER_CRYPTOPRIVATE_H

/**
 * This is a private header for crypto modules that need to share
 * definitions for publicly opaque types. Be sure to exclude this from all public headers.
 */

#include <openssl/x509.h>

struct X509Cert
{
    X509 *cert;
};

#endif // ZILKER_CRYPTOPRIVATE_H
