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
 * icFifoBuffer.c
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
 * Author: jelderton - 6/18/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

#include "icTypes/icFifoBuffer.h"
#include <icLog/logging.h>


#define LOG_TAG             "FIFO"
#define DEFAULT_BUFFER_SIZE 1024

/*
 * internal structure
 * our linear memory should look like:
 *
 *  top        readPos                       writePos      size
 *  |          |                             |             |
 *  v          v                             v             v
 *  :::::::::::------------------------------...............
 *  |<- used -><----- data-in-buffer ------->|<-- unused -->
 *
 *  as data is 'pushed', it will start at 'writePos' and occupy the 'unused' area
 *  as data is 'pulled', it will start at 'readPos' and move the pointer as data is consumed
 */
struct _icFifoBuff
{
    void *top;         // top of allocated space
    uint32_t readPos;  // the 'read' position
    uint32_t writePos; // the 'append' position
    uint32_t size;     // end of the 'allocated memory'
    uint32_t chunkSize;
};


/*
 * private function declarations
 */
static uint32_t ensureCapacity(icFifoBuff *buffer, uint32_t needSize);
static void compact(icFifoBuff *buffer, uint32_t newSize);


/*
 * Create a new FIFO buffer
 *
 * @param initialSize - number of bytes to pre-allocate for the buffer.  If <= 0, then the default of 1024 will be used
 */
icFifoBuff *fifoBuffCreate(uint32_t initialSize)
{
    // first allocate the struct (no size and all pointers at 0)
    //
    icFifoBuff *retVal = (icFifoBuff *) malloc(sizeof(icFifoBuff));
    memset(retVal, 0, sizeof(icFifoBuff));

    // make sure 'size' is a decent amount
    //
    if (initialSize < 64)
    {
        initialSize = DEFAULT_BUFFER_SIZE;
    }

    // now allocate memory for the buffer
    //
    retVal->chunkSize = initialSize;
    ensureCapacity(retVal, initialSize);

    return retVal;
}

/*
 * Deep clone a FIFO buffer
 *
 * @param orig - original buffer to make a copy of
 */
icFifoBuff *fifoBuffClone(icFifoBuff *orig)
{
    // for simplicity, just copy everything
    //
    icFifoBuff *copy = fifoBuffCreate(orig->size);
    copy->readPos = orig->readPos;
    copy->writePos = orig->writePos;
    copy->chunkSize = orig->chunkSize;
    memcpy(copy->top, orig->top, orig->size);

    return copy;
}

/*
 * Destroy a FIFO buffer and free any used memory
 *
 * @param buffer - the buffer to destroy
 */
void fifoBuffDestroy(icFifoBuff *buffer)
{
    if (buffer != NULL)
    {
        // free buffer & struct
        //
        if (buffer->top != NULL)
        {
            free(buffer->top);
        }
        buffer->readPos = 0;
        buffer->writePos = 0;
        buffer->size = 0;
        free(buffer);
    }
}

/*
 * Reset the buffer, but not release the allocated memory (i.e clear content)
 * Primarily used when re-purposing the buffer.
 *
 * @param buffer - the buffer to wipe
 */
void fifoBuffClear(icFifoBuff *buffer)
{
    // simply set our pointers to the beginning
    //
    buffer->readPos = 0;
    buffer->writePos = 0;
}


/*
 * Return amount of free space available for 'fifoBuffPush'.
 * Note that if more space is required it will be automatically
 * allocated during the 'fifoBuffPush'.
 *
 * @param buffer - the buffer to get size info from
 */
uint32_t fifoBuffGetPushAvailable(icFifoBuff *buffer)
{
    // remember our picture:
    //
    //   top        readPos                       writePos      size
    //   |          |                             |             |
    //   v          v                             v             v
    //   :::::::::::------------------------------...............
    //   |<- used -><----- data-in-buffer ------->|<-- unused -->
    //
    // simply return the 'unused' amount
    //
    return (buffer->size - buffer->writePos);
}

/*
 * Append bytes to the end of the buffer.
 * Internally ensures there is enough capacity before copying
 * data from 'src' into the buffer.
 *
 * @param buffer - the buffer to push to
 * @param src - the memory to append to the end of the buffer
 * @param srcSize - number of bytes from 'src' to copy into the buffer
 */
void fifoBuffPush(icFifoBuff *buffer, void *src, uint32_t srcSize)
{
    // ensure we have enough space to take on 'srcSize' bytes
    //
    ensureCapacity(buffer, srcSize);

    // remember our picture:
    //
    //   top        readPos                       writePos      size
    //   |          |                             |             |
    //   v          v                             v             v
    //   :::::::::::------------------------------...............
    //   |<- used -><----- data-in-buffer ------->|<-- unused -->
    //
    // copy into the 'unused' area and update our 'writePos'
    //
    void *ptr = buffer->top + buffer->writePos;
    memcpy(ptr, src, srcSize);
    buffer->writePos += srcSize;
}

/*
 * append a singly byte to the end of the buffer.
 * generally used when building a large string and
 * need to ensure it's NULL terminated.
 */
void fifoBuffPushByte(icFifoBuff *buffer, char byte)
{
    fifoBuffPush(buffer, (void *) &byte, 1);
}

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
void *fifoBuffPushPointer(icFifoBuff *buffer, uint32_t numBytesNeeded)
{
    // first ensure we have enough room
    //
    ensureCapacity(buffer, numBytesNeeded);

    // return the pointer
    //
    return buffer->top + buffer->writePos;
}

/*
 * Update internal structure after a call to 'fifoBuffPushPointer' is complete.
 *
 * @param buffer - the buffer to append to
 * @param numBytesAdded - amount of memory appended
 */
void fifoBuffAfterPushPointer(icFifoBuff *buffer, uint32_t numBytesAdded)
{
    // the 'writePos' did not change from the fifoBuffPushPointer() call
    // so simply move that counter
    //
    buffer->writePos += numBytesAdded;
}


/*
 * Return total number of bytes available for 'fifoBuffPull'
 * (i.e. bytes added via 'fifoBuffPush')
 *
 * @param buffer - the buffer to get size info from
 */
uint32_t fifoBuffGetPullAvailable(icFifoBuff *buffer)
{
    // remember our picture:
    //
    //   top        readPos                       writePos      size
    //   |          |                             |             |
    //   v          v                             v             v
    //   :::::::::::------------------------------...............
    //   |<- used -><----- data-in-buffer ------->|<-- unused -->
    //
    // return the 'data-in-buffer' amount
    //
    return (buffer->writePos - buffer->readPos);
}

/*
 * Extract up to 'numBytes' from the buffer and place into 'dest'
 * Once extracted, that memory will no longer be accessible from the buffer.
 *
 * @param buffer - the buffer to adjust
 * @param dest - location to move the data into
 * @param numBytes - size of 'memory'
 * @return number of bytes actually copied to 'dest'
 */
uint32_t fifoBuffPull(icFifoBuff *buffer, void *dest, uint32_t numBytes)
{
    // make sure there is data
    //
    uint32_t avail = fifoBuffGetPullAvailable(buffer);
    if (avail < numBytes)
    {
        // do not have as much data as we expected
        //
        return 0;
    }

    // remember our picture:
    //
    //   top        readPos                       writePos      size
    //   |          |                             |             |
    //   v          v                             v             v
    //   :::::::::::------------------------------...............
    //   |<- used -><----- data-in-buffer ------->|<-- unused -->
    //
    // copy the 'data-in-buffer' area to 'dest' and update our 'readPos'
    //
    void *ptr = buffer->top + buffer->readPos;
    memcpy(dest, ptr, numBytes);
    buffer->readPos += numBytes;
    return numBytes;
}

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
void *fifoBuffPullPointer(icFifoBuff *buffer, uint32_t numBytesNeeded)
{
    // ensure we have this much available
    //
    uint32_t avail = fifoBuffGetPullAvailable(buffer);
    if (avail >= numBytesNeeded)
    {
        return buffer->top + buffer->readPos;
    }

    return NULL;
}

/*
 * Inform 'buffer' that the 'fifoBuffPullPointer' is complete and
 * exactly how many bytes were extracted.
 */
void fifoBuffAfterPullPointer(icFifoBuff *buffer, uint32_t numBytesPulled)
{
    buffer->readPos += numBytesPulled;
}


/*
 * return if the 'used' space is large enough that we need to perform a 'compact()' operation
 */
static bool needsCompress(icFifoBuff *buffer)
{
    // for now, keep this simple.  if our 'used' area is more then half
    // of what we have allocated, then perform a compression so that
    // we can reclaim space without reallocating the memory
    //
    if (buffer->size > 0 && buffer->readPos > (buffer->size / 2))
    {
        return true;
    }
    return false;
}

/*
 * ensure buffer has enough space to accommodate 'size'
 * returns the amount of available space
 */
static uint32_t ensureCapacity(icFifoBuff *buffer, uint32_t needSize)
{
    // remember our picture:
    //
    //   top        readPos                       writePos      size
    //   |          |                             |             |
    //   v          v                             v             v
    //   :::::::::::------------------------------...............
    //   |<- used -><----- data-in-buffer ------->|<-- unused -->
    //
    // see how much space is 'unused', so we can determine
    // if we need to allocate more room or not
    //
    uint32_t avail = fifoBuffGetPushAvailable(buffer);
    int lacking = (needSize - avail);
    if (lacking > 0)
    {
        // not enough room, so need to reallocate the buffer.
        // before we do that, see if we need to compact
        //
        if (needsCompress(buffer) == true)
        {
            //            icLogDebug(LOG_TAG, "compacting FIFO buffer");
            compact(buffer, needSize);
            return fifoBuffGetPushAvailable(buffer);
        }

        // need more space, so reallocate our 'top'
        //
        uint32_t newLen = buffer->size + lacking + buffer->chunkSize;
        void *tmp = realloc(buffer->top, newLen * sizeof(void));
        if (tmp == NULL)
        {
            // allocation failed, have to make do with that we have
            //
            icLogWarn(LOG_TAG, "unable to resize FIFO buffer - %s, only allowing %d bytes", strerror(errno), avail);
            return avail;
        }

        // update length and the buffer
        //
        buffer->size = newLen;
        buffer->top = tmp;

        // return the amount of space now available within inputBuffer
        // (from position to end)
        //
        return fifoBuffGetPushAvailable(buffer);
    }

    // no reallocation needed, return the amount of space inputBuffer has
    //
    return avail;
}

/*
 * should ONLY be called if needsCompress() == true
 */
static void compact(icFifoBuff *buffer, uint32_t newSize)
{
    // remember our picture:
    //
    //   top        readPos                       writePos      size
    //   |          |                             |             |
    //   v          v                             v             v
    //   :::::::::::------------------------------...............
    //   |<- used -><----- data-in-buffer ------->|<-- unused -->
    //
    // since our needsCompress() was 'true' we can assume that the 'used' area
    // is larger then our 'data-in-buffer' area
    //
    void *ptr = buffer->top + buffer->readPos;
    uint32_t ptrLen = buffer->writePos - buffer->readPos;
    memcpy(buffer->top, ptr, ptrLen);
    buffer->readPos = 0;
    buffer->writePos = ptrLen;
}
