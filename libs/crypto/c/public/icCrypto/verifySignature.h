//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2019 Comcast Corporation
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

#ifndef ZILKER_VERIFYSIGNATURE_H
#define ZILKER_VERIFYSIGNATURE_H

/*
 * validate the signature of a file.  used during upgrade situations to
 * ensure the packaged file was un-touched and good-to-go for use
 *
 * @param keyFilename - the public key to use for the validation
 * @param baseFilename - the filename the signature will be validated against
 * @param signatureFilename - the .sig file (to accompany the baseFilename)
 */
bool verifySignature(char *keyFilename, char *baseFilename, char *signatureFilename);

#endif // ZILKER_VERIFYSIGNATURE_H
