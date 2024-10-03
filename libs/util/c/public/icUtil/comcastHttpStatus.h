//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------

/**
 * defining the IANA HTTP Status codes. This is based on the CSV found at
 * https://www.iana.org/assignments/http-status-codes/http-status-codes-1.csv
 **/

#ifndef COMCASTHTTPSTATUS_H
#define COMCASTHTTPSTATUS_H

#define HTTP_STATUS_CONTINUE                        100
#define HTTP_STATUS_SWITCHING_PROTOCOLS             101
#define HTTP_STATUS_PROCESSING                      102
#define HTTP_STATUS_EARLY_HINTS                     103
#define HTTP_STATUS_OK                              200
#define HTTP_STATUS_CREATED                         201
#define HTTP_STATUS_ACCEPTED                        202
#define HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION   203
#define HTTP_STATUS_NO_CONTENT                      204
#define HTTP_STATUS_RESET_CONTENT                   205
#define HTTP_STATUS_PARTIAL_CONTENT                 206
#define HTTP_STATUS_MULTI_STATUS                    207
#define HTTP_STATUS_ALREADY_REPORTED                208
#define HTTP_STATUS_IM_USED                         226
#define HTTP_STATUS_MULTIPLE_CHOICES                300
#define HTTP_STATUS_MOVED_PERMANENTLY               301
#define HTTP_STATUS_FOUND                           302
#define HTTP_STATUS_SEE_OTHER                       303
#define HTTP_STATUS_NOT_MODIFIED                    304
#define HTTP_STATUS_USE_PROXY                       305
#define HTTP_STATUS_(UNUSED)                        306
#define HTTP_STATUS_TEMPORARY_REDIRECT              307
#define HTTP_STATUS_PERMANENT_REDIRECT              308
#define HTTP_STATUS_BAD_REQUEST                     400
#define HTTP_STATUS_UNAUTHORIZED                    401
#define HTTP_STATUS_PAYMENT_REQUIRED                402
#define HTTP_STATUS_FORBIDDEN                       403
#define HTTP_STATUS_NOT_FOUND                       404
#define HTTP_STATUS_METHOD_NOT_ALLOWED              405
#define HTTP_STATUS_NOT_ACCEPTABLE                  406
#define HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED   407
#define HTTP_STATUS_REQUEST_TIMEOUT                 408
#define HTTP_STATUS_CONFLICT                        409
#define HTTP_STATUS_GONE                            410
#define HTTP_STATUS_LENGTH_REQUIRED                 411
#define HTTP_STATUS_PRECONDITION_FAILED             412
#define HTTP_STATUS_PAYLOAD_TOO_LARGE               413
#define HTTP_STATUS_URI_TOO_LONG                    414
#define HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE          415
#define HTTP_STATUS_RANGE_NOT_SATISFIABLE           416
#define HTTP_STATUS_EXPECTATION_FAILED              417
#define HTTP_STATUS_MISDIRECTED_REQUEST             421
#define HTTP_STATUS_UNPROCESSABLE_ENTITY            422
#define HTTP_STATUS_LOCKED                          423
#define HTTP_STATUS_FAILED_DEPENDENCY               424
#define HTTP_STATUS_TOO_EARLY                       425
#define HTTP_STATUS_UPGRADE_REQUIRED                426
#define HTTP_STATUS_PRECONDITION_REQUIRED           428
#define HTTP_STATUS_TOO_MANY_REQUESTS               429
#define HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE 431
#define HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS   451
#define HTTP_STATUS_INTERNAL_SERVER_ERROR           500
#define HTTP_STATUS_NOT_IMPLEMENTED                 501
#define HTTP_STATUS_BAD_GATEWAY                     502
#define HTTP_STATUS_SERVICE_UNAVAILABLE             503
#define HTTP_STATUS_GATEWAY_TIMEOUT                 504
#define HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED      505
#define HTTP_STATUS_VARIANT_ALSO_NEGOTIATES         506
#define HTTP_STATUS_INSUFFICIENT_STORAGE            507
#define HTTP_STATUS_LOOP_DETECTED                   508
#define HTTP_STATUS_NOT_EXTENDED                    510
#define HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED 511

#define HTTP_STATUS_IS_INFORMATIONAL(_stat_code)    ((_stat_code >= 100) && (_stat_code < 200))
#define HTTP_STATUS_IS_SUCCESSFUL(_stat_code)       ((_stat_code >= 200) && (_stat_code < 300))
#define HTTP_STATUS_IS_REDIRECTION(_stat_code)      ((_stat_code >= 300) && (_stat_code < 400))
#define HTTP_STATUS_IS_CLIENT_ERROR(_stat_code)     ((_stat_code >= 400) && (_stat_code < 500))
#define HTTP_STATUS_IS_SERVER_ERROR(_stat_code)     ((_stat_code >= 500) && (_stat_code < 600))
#define HTTP_STATUS_IS_ERROR(_stat_code)            (_stat_code >= 400)

typedef enum
{
    HTTP_STATUS_IS_INFORMATIONAL,
    HTTP_STATUS_IS_SUCCESSFUL,
    HTTP_STATUS_IS_REDIRECTION,
    HTTP_STATUS_IS_CLIENT_ERROR,
    HTTP_STATUS_IS_SERVER_ERROR,
    HTTP_STATUS_IS_ERROR
} httpStatusClassifier;

/*
 * simple inline function to classify an http status code according to the httpStatusClassifier enum
 */
static inline httpStatusClassifier getHttpStatusClassifier(int stat_code)
{
    httpStatusClassifier retval = HTTP_STATUS_IS_ERROR;
    if ((stat_code >= 100) && (stat_code < 200))
    {
        retval = HTTP_STATUS_IS_INFORMATIONAL;
    }
    else if ((stat_code >= 200) && (stat_code < 300))
    {
        retval = HTTP_STATUS_IS_SUCCESSFUL;
    }
    else if ((stat_code >= 300) && (stat_code < 400))
    {
        retval = HTTP_STATUS_IS_REDIRECTION;
    }
    else if ((stat_code >= 400) && (stat_code < 500))
    {
        retval = HTTP_STATUS_IS_CLIENT_ERROR;
    }
    else if ((stat_code >= 500) && (stat_code < 600))
    {
        retval = HTTP_STATUS_IS_SERVER_ERROR;
    }
    return retval;
}

#endif /* COMCASTHTTPSTATUS_H */
