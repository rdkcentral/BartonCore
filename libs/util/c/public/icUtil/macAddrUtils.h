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
 * macAddrUtils.h
 *
 * MAC Address Util Functions
 *
 * Author: jgleason
 *-----------------------------------------------*/

#ifndef IC_MACADDRUTILS_H
#define IC_MACADDRUTILS_H

#include <netinet/if_ether.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif

#define MAC_ADDR_BYTES             12
#define MAC_ADDR_WITH_COLONS_BYTES 17

/*
 * enumerated list of possible return codes from MAC address utility
 */
typedef enum
{
    MAC_ADDR_CODE_SUCCESS = 0,
    MAC_ADDR_CODE_GENERIC_ERROR,        // A generic error occurred
    MAC_ADDR_CODE_ARP_FILE_OPEN_ERROR,  // The ARP file could not be found or could not be opened
    MAC_ADDR_CODE_ARP_FILE_NO_HWA,      // The ARP file did not contain the hwa field
    MAC_ADDR_CODE_ARP_FILE_NO_IP_MATCH, // The IP address was not found in the ARP file

    /* the final entry is just for looping over the enum */
    MAC_ADDR_CODE_LAST
} macAddrCode;

/*
 * gets the hardware address (MAC address) of the device
 * using the Address Resolution Protocol (ARP)
 *
 * The ARP allows a host to find the MAC address of a node
 * with an IP address on the same physical network
 */
macAddrCode lookupMacAddressByIpAddress(char *ipAddress, char *macAddress);

/*
 * parse the source MAC address string, and populate a string version with
 * the :'s removed and leading 0's filled in
 *  e.g. '0:e:8f:e9:93:f9' is converted to '000e8fe993f9'
 */
bool macAddrToUUID(char *destinationUUID, const char *sourceMacAddress);

/*
 * convert a MAC address string to an array of bytes.  the supplied
 * destArray should have at least 6 elements.  can handle the input
 * string with or without colon chars, just need to be told.
 */
bool macAddrToBytes(const char *macAddress, uint8_t *destArray, bool hasColonChars);

/*
 * compare 2 MAC address byte arrays.  like typical compare functions,
 * returns -1 if 'left' is less, 1 if 'left' is more, 0 if they are equal
 */
int8_t compareMacAddrs(uint8_t *left, uint8_t *right);

/**
 * Set an ARP cache entry for an IP address.
 * The entry will be flagged as if the address were discovered normally by ARP.
 * @param ipAddr The IP Address to add to the MAC table. Must be an IPv4 address (dotted quad).
 * @param macAddr The 6 byte Ethernet MAC address.
 * @param devname The network device name, up to 15 characters (may be NULL)
 * @return true on success. Any errors are logged as warnings.
 */
bool setMacAddressForIP(const char *ipAddr, const uint8_t macAddr[ETHER_ADDR_LEN], const char *devname);

#endif // IC_MACADDRUTILS_H
