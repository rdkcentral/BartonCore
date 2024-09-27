//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
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
 * icFifoBuffer.h
 *
 * Simplistic FIFO (First In - First Out) Buffer that
 * dynamically increases memory required to store an
 * undetermined amount of information.
 *
 * Similar to a queue, but intended for chunks of
 * contiguous memory (such as a stream).
 *
 * As data is realized, it can be appended to this buffer,
 * then read from the front.  Once enough data has been
 * read (pulled), it will attempt to cleanup the internal
 * storage to prevent an ever-growing buffer.
 *
 * Primarily used as a temporary memory storage area
 * when the buffering data between two different
 * components or threads.  Could be thought of as the
 * inner portion of a pipe.
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_FIFOBUFFER_H
#define IC_FIFOBUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct _icFifoBuff icFifoBuff;

/*
 * Create a new FIFO buffer
 *
 * @param initialSize - number of bytes to pre-allocate for the buffer.  If <= 0, then the default of 1024 will be used
 */
icFifoBuff *fifoBuffCreate(uint32_t initialSize);

/*
 * Deep clone a FIFO buffer
 *
 * @param orig - original buffer to make a copy of
 */
icFifoBuff *fifoBuffClone(icFifoBuff *orig);

/*
 * Destroy a FIFO buffer and free any used memory
 *
 * @param buffer - the buffer to destroy
 */
void fifoBuffDestroy(icFifoBuff *buffer);

/*
 * Reset the buffer, but not release the allocated memory (i.e clear content)
 * Primarily used when re-purposing the buffer.
 *
 * @param buffer - the buffer to wipe
 */
void fifoBuffClear(icFifoBuff *buffer);


/*
 * Return amount of free space available for 'fifoBuffPush'.
 * Note that if more space is required it will be automatically
 * allocated during the 'fifoBuffPush'.
 *
 * @param buffer - the buffer to get size info from
 */
uint32_t fifoBuffGetPushAvailable(icFifoBuff *buffer);

/*
 * Append bytes to the end of the buffer.
 * Internally ensures there is enough capacity before copying
 * data from 'src' into the buffer.
 *
 * @param buffer - the buffer to push to
 * @param src - the memory to append to the end of the buffer
 * @param srcSize - number of bytes from 'src' to copy into the buffer
 */
void fifoBuffPush(icFifoBuff *buffer, void *src, uint32_t srcSize);

/*
 * append a singly byte to the end of the buffer.
 * generally used when building a large string and
 * need to ensure it's NULL terminated.
 */
void fifoBuffPushByte(icFifoBuff *buffer, char byte);

/*
 * Returns a pointer to the internal buffer to allow a caller to
 * directly append bytes without needing an intermediate chunk of memory.
 * Primarily used in situations such as read() where a direct injection
 * into the buffer is more efficient then 'fifoBuffPush'.
 *
 * Once complete, the caller **MUST** follow up with a call to
 * fifoBuffAfterPushPointer() so that the internal structure can properly
 * reflect the newly appended data.
 *
 * @param buffer - the buffer to directly push to
 * @param numBytesNeeded - amount of memory that will be appended
 * @return the pointer to start directly appending at
 * @see fifoBuffAfterPushPointer()
 */
void *fifoBuffPushPointer(icFifoBuff *buffer, uint32_t numBytesNeeded);

/*
 * Update internal structure after a call to 'fifoBuffPushPointer' is complete.
 *
 * @param buffer - the buffer to append to
 * @param numBytesAdded - amount of memory appended
 */
void fifoBuffAfterPushPointer(icFifoBuff *buffer, uint32_t numBytesAdded);


/*
 * Return total number of bytes available for 'fifoBuffPull'
 * (i.e. bytes added via 'fifoBuffPush')
 *
 * @param buffer - the buffer to get size info from
 */
uint32_t fifoBuffGetPullAvailable(icFifoBuff *buffer);

/*
 * Extract up to 'numBytes' from the buffer and place into 'dest'
 * Once extracted, that memory will no longer be accessible from the buffer.
 *
 * @param buffer - the buffer to adjust
 * @param dest - location to move the data into
 * @param numBytes - size of 'memory'
 * @return number of bytes actually copied to 'dest'
 */
uint32_t fifoBuffPull(icFifoBuff *buffer, void *dest, uint32_t numBytes);

/*
 * Returns a pointer to the internal buffer to allow a caller to
 * directly read bytes without needing an intermediate chunk of memory.
 *
 * Primarily used when an operation needs a pointer to read from
 * (such as zlib decompressing a buffer) vs copying from this buffer
 * into another.
 *
 * It is up to the caller to ensure they do not read more then
 * 'numBytesNeeded' and when done **MUST** make a subsequent call to
 * fifoBuffAfterPullPointer() to update the internal structure.
 *
 * @param buffer - the buffer to read from
 * @param numBytesNeeded - amount of memory that will be appended
 * @return the pointer to start directly reading from
 * @see fifoBuffAfterPullPointer()
 */
void *fifoBuffPullPointer(icFifoBuff *buffer, uint32_t numBytesNeeded);

/*
 * Inform 'buffer' that the 'fifoBuffPullPointer' is complete and
 * exactly how many bytes were extracted.
 */
void fifoBuffAfterPullPointer(icFifoBuff *buffer, uint32_t numBytesPulled);

#endif // IC_FIFOBUFFER_H
