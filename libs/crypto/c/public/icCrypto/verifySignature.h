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
