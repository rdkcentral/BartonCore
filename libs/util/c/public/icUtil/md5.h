/*-----------------------------------------------
 * md5.h
 *
 * Helper functions for performing
 * MD5 checksum operations
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IC_MD5_H
#define IC_MD5_H

/*
 * Perform an MD5 checksum on the provided buffer.
 * Caller is responsible for freeing the result.
 */
char *icMd5sum(const char *src);

#endif  // IC_MD5_H