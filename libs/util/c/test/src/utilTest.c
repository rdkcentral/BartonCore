//
// Created by John Elderton on 6/18/15.
//


#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include <zlog.h>
#include "encodeTest.h"
#include "macAddrTest.h"
#include "parsePropTest.h"
#include "versionTest.h"
#include <icLog/logging.h>

/*
 *
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  utilTest <-a|-t>\n");
    fprintf(stderr, "    -a : run all tests\n");
    fprintf(stderr, "    -p : run propFile test\n");
    fprintf(stderr, "    -e : run encode/decode tests\n");
    fprintf(stderr, "    -m : run mac address tests\n");
    fprintf(stderr, "    -v : run version tests\n");
    fprintf(stderr, "\n");
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    int c;
    bool doPropFile = false;
    bool doEncode = false;
    bool doMacAddr = false;
    bool doVersion = false;

    while ((c = getopt(argc, argv, "aehpmiv")) != -1)
    {
        switch (c)
        {
            case 'a':
                doPropFile = true;
                doEncode = true;
                doMacAddr = true;
                doVersion = true;
                break;

            case 'p':
                doPropFile = true;
                break;

            case 'e':
                doEncode = true;
                break;

            case 'm':
                doMacAddr = true;
                break;

            case 'v':
                doVersion = true;
                break;

            case 'h':
            default:
            {
                printUsage();
                return 1;
            }
        }
    }

    bool didSomething = false;
    if (doPropFile == true)
    {
        printf("\n\nRunning PropFile Test:\n");
        if (runParsePropFileTests() == false)
        {
            printf("PropFile Test FAILED!\n");
            return 1;
        }
        printf("PropFile Test SUCCESS!\n");
        didSomething = true;
    }
    if (doEncode == true)
    {
        printf("\n\nRunning Encode/Decode Test:\n");
        if (runEncodeTests() == false)
        {
            printf("Encode/Decode Test FAILED!\n");
            return 1;
        }
        printf("Encode/Decode Test SUCCESS!\n");
        didSomething = true;
    }
    if (doMacAddr == true)
    {
        printf("\n\nRunning MAC Address Test:\n");
        if (runMacAddrTests() == false)
        {
            printf("MAC Address Test FAILED!\n");
            return 1;
        }
        printf("MAC Address Test SUCCESS!\n");
        didSomething = true;
    }
    if (doVersion == true)
    {
        printf("\n\nRunning Version Compare Test:\n");
        if (runVersionTests() == false)
        {
            printf("Version Compare Test FAILED!\n");
            return 1;
        }
        printf("Version Compare Test SUCCESS!\n");
        didSomething = true;
    }

    if (didSomething == false)
    {
        fprintf(stderr, "no options provided, use -h option for help\n");
        return 1;
    }

    // no errors?
    //
    return 0;
}
