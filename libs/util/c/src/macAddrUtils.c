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
 * macAddrUtils.c
 *
 * Utilities for finding the MAC address of a device
 *
 * Author: jgleason
 *-----------------------------------------------*/

#include <arpa/inet.h>
#include <errno.h>
#include <net/if_arp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <icUtil/macAddrUtils.h>
#include <icUtil/stringUtils.h>
#include <icUtil/systemCommandUtils.h>

#ifndef CONFIG_OS_DARWIN
#define PROC_ARP_FILE "/proc/net/arp"
#endif

#define LOG_TAG   "macUtil"
#define DELIMETER ":"

#ifdef CONFIG_OS_DARWIN
/*-------------------------------------------------
 * removeChar : remove a character from the string
 *--------------------------------------------------*/
static void removeChar(char *s, char chr)
{
    int i, j = 0;
    /* 'i' moves through all of the string, 's' */
    for (i = 0; s[i] != '\0'; i++)
    {
        if (s[i] != chr)
        {
            /* 'j' only moves through 's' when 'chr' is not encountered */
            s[j++] = s[i];
        }
    }
    /* null-terminate the updated string */
    s[j] = '\0';
}
#endif

/*----------------------------------------------------------------------
 * lookupMacAddressByIpAddress - gets the hardware address (MAC address) of the device
 * using the Address Resolution Protocol (ARP)
 *
 * The ARP allows a host to find the MAC address of a node
 * with an IP address on the same physical network
 *-----------------------------------------------------------------------*/
macAddrCode lookupMacAddressByIpAddress(char *ipAddress, char *macAddress)
{
    FILE *fp;
    char line[200];
    macAddrCode code = MAC_ADDR_CODE_ARP_FILE_NO_IP_MATCH;

    // first ping the device to get it into the arp table.  some environments require
    // this as we need a certain amount of network traffic to update the ARP table
    //
    icLogTrace(LOG_TAG, "pinging device %s to prime ARP table", ipAddress);
    executeSystemCommand("ping", "-c", "1", "-w", "1", ipAddress);
    executeSystemCommand("ping", "-c", "1", "-w", "1", ipAddress);

#ifdef CONFIG_OS_DARWIN
    /*
     * on mac platform, use 'arp -a'
     *  example output format:
     *    foo.com (10.0.6.178) at 0:e:8f:e9:93:f9 on en4 ifscope [ethernet]
     */

    /* open the command for reading. */
    fp = popen("/usr/sbin/arp -a", "r");
    if (fp == NULL)
    {
        icLogWarn(LOG_TAG, "unable to open ARP table");
        return MAC_ADDR_CODE_ARP_FILE_OPEN_ERROR;
    }

    while (fgets(line, sizeof(line) - 1, fp) != NULL)
    {
        char *ctx;
        strtok_r(line, " ", &ctx);                // discard...
        char *token2 = strtok_r(NULL, " ", &ctx); // ip address wrapped in parenthesis
        strtok_r(NULL, " ", &ctx);                // the 'at' word... discard
        char *token4 = strtok_r(NULL, " ", &ctx); // the mac address

        removeChar(token2, '(');
        removeChar(token2, ')');

        if (strcmp(token2, ipAddress) == 0)
        {
            strcpy(macAddress, token4);
            icLogTrace(LOG_TAG, "found macAddress = %s for ip = %s\n", token4, ipAddress);
            code = MAC_ADDR_CODE_SUCCESS;
            break;
        }
    }

    /* close */
    pclose(fp);

#else

    /*
     * on linux platform, use /proc/net/arp to map the IP address to the MAC address
     *  example file format:
     *    IP address       HW type     Flags       HW address            Mask     Device
     *    10.0.6.1         0x1         0x2         00:25:59:3e:46:c4     *        eth0
     */

    /* open the file for reading. */
    fp = fopen(PROC_ARP_FILE, "r");
    if (fp == NULL)
    {
        /* unable to open ARP file */
        return MAC_ADDR_CODE_ARP_FILE_OPEN_ERROR;
    }

    /* read the output a line at a time - look for IP match. */
    if (fgets(line, sizeof(line), fp) != (char *) NULL)
    {
        for (; fgets(line, sizeof(line), fp);)
        {
            char ip[256];
            char hwa[256];
            char mask[256];
            char dev[256];
            int type, flags;
            int num = sscanf(line, "%s 0x%x 0x%x %255s %255s %255s\n", ip, &type, &flags, hwa, mask, dev);
            if (num < 4)
            {
                /* ARP file does not have MAC address */
                code = MAC_ADDR_CODE_ARP_FILE_NO_IP_MATCH;
                icLogWarn(LOG_TAG, "no macAddress for IP %s", ipAddress);
                break;
            }
            if (strcmp(ip, ipAddress) == 0)
            {
                /* we found our address */
                strcpy(macAddress, hwa);
                icLogTrace(LOG_TAG, "found macAddress = %s for ip = %s\n", hwa, ipAddress);
                code = MAC_ADDR_CODE_SUCCESS;
                break;
            }
        }
    }
    /* close */
    fclose(fp);
#endif
    return code;
}

/*-------------------------------------------------------
 * ensureTwoChars - prepend with '0' when necessary
 *
 * takes a token input which should be 2 characters, and
 * prepends it with a '0' if it is one character
 * useful when converting the MAC address to a UUID
 *  e.g. '0:e:8f:e9:93:f9' is converted to '000e8fe993f9'
 *--------------------------------------------------------*/
static void ensureTwoChars(char *output, const char *input)
{
    /* if input is only 1 character */
    if (strlen(input) == 1)
    { /* then prepend a '0' to it */
        sprintf(output, "%s%s", "0", input);
    }
    else
    {
        sprintf(output, "%s", input);
    }
    return;
}

/*----------------------------------------------------------------------
 * macAddrToUUID - remove the colons in MAC address to make UUID string
 *
 * parse the source MAC address string, and populate a string version with
 * the :'s removed and leading 0's filled in
 *  e.g. '0:e:8f:e9:93:f9' is converted to '000e8fe993f9'
 *-----------------------------------------------------------------------*/
bool macAddrToUUID(char *destinationUUID, const char *sourceMacAddress)
{
    bool retVal = false;

    /* tokenize on colons (:) and populate the values */
    if (sourceMacAddress != NULL && destinationUUID != NULL && strlen(sourceMacAddress) != 0)
    {
        /* zero out the target string */
        memset(destinationUUID, 0, MAC_ADDR_BYTES);

        /* make the string end with the delimiter to allow */
        /* a token to represent the last characters */
        char *copy = (char *) malloc(strlen(sourceMacAddress) + strlen(DELIMETER) + 1);
        sprintf(copy, "%s%s", sourceMacAddress, DELIMETER);

        /* loop through the MAC address string, copying the tokens to the UUID string*/
        char *ctx;
        char *token = NULL;
        for (token = strtok_r(copy, DELIMETER, &ctx); token != NULL; token = strtok_r(NULL, DELIMETER, &ctx))
        {
            /* prepend a '0' if it is missing */
            ensureTwoChars(destinationUUID + strlen(destinationUUID), token);
        }
        /* update return value */
        retVal = true;

        /* mem cleanup */
        free(copy);
    }

    return retVal;
}

/*
 * convert a MAC address string to an array of bytes.  the supplied
 * destArray should have at least 6 elements.  can handle the input
 * string with or without colon chars, just need to be told.
 */
bool macAddrToBytes(const char *macAddress, uint8_t *destArray, bool hasColonChars)
{
    // sanity check
    //
    if (macAddress == NULL || destArray == NULL)
    {
        return false;
    }

    if (hasColonChars == true)
    {
        // convert a mac address string (with colon chars).  we'll use strtok
        // since it's safer then sscanf, but that requires a non-const array
        //
        int offset = 0;
        char *copy = strdup(macAddress);
        char *tok = NULL;
        char *ctx;
        for (tok = strtok_r(copy, DELIMETER, &ctx); tok != NULL; tok = strtok_r(NULL, DELIMETER, &ctx), offset++)
        {
            // convert the token to byte, and assign to the array at 'offset'
            //
            destArray[offset] = (uint8_t) strtol(tok, NULL, 16);
        }
        free(copy);
    }
    else
    {
        // require the string to have 12 characters to represent the 6 bytes
        //
        if (strlen(macAddress) < 12)
        {
            return false;
        }

        int offset = 0;
        char *ptr = (char *) macAddress;
        char tok[3];
        while (offset < 6)
        {
            // grab the next 2 chars
            //
            tok[0] = *ptr;
            ptr++;
            tok[1] = *ptr;
            ptr++;
            tok[2] = '\0';

            // convert to byte, and assign to the array at 'offset'
            //
            destArray[offset] = (uint8_t) strtol(tok, NULL, 16);
            offset++;
        }
    }

    return true;
}

/*
 * compare 2 MAC address byte arrays.  like typical compare functions,
 * returns -1 if 'left' is less, 1 if 'left' is more, 0 if they are equal
 */
int8_t compareMacAddrs(uint8_t *left, uint8_t *right)
{
    if (left == NULL && right == NULL)
    {
        // same
        return 0;
    }
    else if (left == NULL)
    {
        // right is greater
        return 1;
    }
    else if (right == NULL)
    {
        // left is greater
        return -1;
    }

    // bot are not-null, compare each byte one-at-a-time
    //
    int index = 0;
    while (index < 6)
    {
        if (left[index] < right[index])
        {
            return -1;
        }
        else if (left[index] > right[index])
        {
            return 1;
        }

        // go to next byte
        //
        index++;
    }

    // must be the same
    return 0;
}

bool setMacAddressForIP(const char *ipAddr, const uint8_t macAddr[ETHER_ADDR_LEN], const char *devname)
{
    bool ok = false;

    if (ipAddr == NULL)
    {
        return false;
    }

#ifdef __linux__
    int rc = 0;
    struct arpreq req;
    struct sockaddr_in *tmp = (struct sockaddr_in *) &req.arp_pa;

    memset(&req, 0, sizeof(req));

    tmp->sin_port = 0;
    tmp->sin_family = AF_INET;
    int valid = inet_pton(tmp->sin_family, ipAddr, &tmp->sin_addr);

    if (valid != 1)
    {
        icLogError(LOG_TAG, "'%s' is not a valid IPv4 address", ipAddr);
        return false;
    }

    int arpsock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_IP);

    if (arpsock < 0)
    {
        icLogWarn(LOG_TAG, "Unable to open ARP request socket");
        return ok;
    }

    safeStringCopy(req.arp_dev, sizeof(req.arp_dev), devname);

    req.arp_flags = ATF_COM;
    req.arp_ha.sa_family = ARPHRD_ETHER;
    memcpy(req.arp_ha.sa_data, macAddr, ETHER_ADDR_LEN);

    /* Always delete first to ensure lookups are cancelled */
    ioctl(arpsock, SIOCDARP, &req);
    rc = ioctl(arpsock, SIOCSARP, &req);
    if (rc == 0 || errno == EEXIST)
    {
        ok = true;
    }
    else
    {
        char *err = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "Unable to add ARP record: [%d](%s)", errno, err);
        free(err);
    }

    close(arpsock);

#endif
    return ok;
}
