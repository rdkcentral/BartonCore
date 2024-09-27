/*-----------------------------------------------
 * tea.c
 *
 * TEA encrypt/decrypt functions that are carryover
 * from QNX days.  No idea where this code originally
 * came from, but we do know it works.
 *
 * Author: unknown
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <icUtil/tea.h>
#include <icLog/logging.h>

#define LOG_TAG "TEACrypto"

/**
 * convert the string key to the internal binary format; caller must free result
 */
static unsigned long *convertKey(char* pKey)
{
    int i = 0;
    int p = 0;
    int pKeySize = strlen(pKey);
    char pKeyCpy[128];
    unsigned long mask;
    unsigned long *teaKey = NULL;

    if (pKeySize >= sizeof(pKeyCpy))
    {
        icLogError(LOG_TAG, "%s: key is longer than maximum %zu", __FUNCTION__, sizeof(pKeyCpy) - 1);
        return NULL;
    }

    teaKey = calloc(KEY_SIZE, sizeof(unsigned long));
    strcpy(pKeyCpy, pKey);
    /*first clear out the key we are setting*/
    memset(&mask,0,sizeof(mask));
    /*convert the passed key to unsigned long and store appropriatly in teaKey*/
    i = 0;
    for(p = 0; p < KEY_SIZE; p++)
    {

        mask = pKeyCpy[i];
        mask <<= 24;
        teaKey[p] |= mask;

        i++; if(i > pKeySize){ i = 0; }

        mask = pKeyCpy[i];
        mask <<= 16;
        teaKey[p] |= mask;

        i++; if(i > pKeySize){ i = 0; }

        mask = pKeyCpy[i];
        mask <<= 8;
        teaKey[p] |= mask;

        i++; if(i > pKeySize){ i = 0; }

        mask = pKeyCpy[i];
        mask <<= 0;
        teaKey[p] |= mask;

        i++; if(i > pKeySize){ i = 0; }
    }
    return teaKey;
}


static void encipher (uint32_t* v, uint32_t* k)
{
    uint32_t v0=v[0], v1=v[1], sum=0, i;           /* set up */
    uint32_t delta=0x9e3779b9;                     /* a key schedule constant */
    uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
    for (i=0; i < 32; i++) {                       /* basic cycle start */
        sum += delta;
        v0 += ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        v1 += ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);  
    }                                              /* end cycle */
    v[0]=v0; v[1]=v1;
}

static void decipher (uint32_t* v, uint32_t* k)
{
    uint32_t v0=v[0], v1=v[1], sum=0xC6EF3720, i;  /* set up */
    uint32_t delta=0x9e3779b9;                     /* a key schedule constant */
    uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
    for (i=0; i<32; i++) {                         /* basic cycle start */
        v1 -= ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
        v0 -= ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        sum -= delta;                                   
    }                                              /* end cycle */
    v[0]=v0; v[1]=v1;
}

static int int2Byte(int input, unsigned char* result)
{
    result[0] = (unsigned char) (input >> 24);
    result[1] = (unsigned char) (input >> 16);
    result[2] = (unsigned char) (input >> 8);
    result[3] = (unsigned char) (input);
    return 0;
}

static int Byte2Int(unsigned long* output, unsigned char *buff)
{
    unsigned char* byte = buff;
//    output[0] = 0xffffffff & ((byte[0]<<24)|(byte[1]<<16)|(byte[2]<<8)|(byte[3]));
    output[0] = ((uint32_t) (byte[0] << 24u)|(uint32_t) (byte[1]<<16U)|(uint32_t) (byte[2]<<8U)|(byte[3]));
    return 0;
}

#define MAX_ENCRYPT_SIZE 16384

static int prepareData(unsigned char* data, int* dataSize)
{
    static unsigned char* prepData[MAX_ENCRYPT_SIZE] = {0};
    int offset = 0;
    
    if(*dataSize < 8){ 
        offset = 8 - *dataSize;
    }
    else if(*dataSize%8 != 0){
        offset = 8 -  *dataSize%8;
    }
    
    if(offset != 0)
    {
        //data length is not a multiple of 8
        memcpy(prepData,data,*dataSize);
        *dataSize += offset;
        memcpy(data,prepData,*dataSize);    
    }
    
    return 0;
}

static int testData(unsigned char* data, char *keyStr, int dataSize)
{
    unsigned char testData[dataSize];
    memset(testData,0, sizeof(testData));
    memcpy(testData,data,dataSize);
    teaDecrypt(testData,keyStr,dataSize);
    return 0;
}

int teaEncrypt(unsigned char* data, char *keyStrIn, int dataSize)
{
    int index = 0;
    unsigned long data64Bit[2] = {0};
    
    prepareData(data,&dataSize);

    char *keyStr = malloc((16*strlen(keyStrIn))+2);
    strcpy(keyStr,keyStrIn);
    while (strlen(keyStr) < 16)
    {
        strcat(keyStr,keyStrIn);
    }

    unsigned long *key = convertKey(keyStr);

    do
    {
        Byte2Int(&data64Bit[0], &data[index+0]);
        Byte2Int(&data64Bit[1], &data[index+4]);
        
        encipher((uint32_t *)data64Bit,(uint32_t *)key);
        
        int2Byte(data64Bit[0], &data[index+0]);
        int2Byte(data64Bit[1], &data[index+4]);
        
        index += 8;    
    }
    while(index  < dataSize);

    testData(data,keyStr,dataSize);
    free(key);
    free(keyStr);
    return dataSize;    
}

int teaDecrypt(unsigned char* data, char *keyStrIn, int dataSize)
{
    int index = 0;
    unsigned long data64Bit[2] = {0};
    
    char *keyStr = malloc((16*strlen(keyStrIn))+2);
    strcpy(keyStr,keyStrIn);
    while (strlen(keyStr) < 16)
    {
        strcat(keyStr,keyStrIn);
    }

    unsigned long *key = convertKey(keyStr);

    do
    {
        Byte2Int(&data64Bit[0], &data[index+0]);
        Byte2Int(&data64Bit[1], &data[index+4]);
        
        decipher((uint32_t *)data64Bit,(uint32_t *)key);
        
        int2Byte(data64Bit[0], &data[index+0]);
        int2Byte(data64Bit[1], &data[index+4]);
        
        index += 8;    
    }
    while(index  < dataSize);
    free(key);
    free(keyStr);
    return 0;    
}


// Use encryptBlock/decryptBlock for 16-bit processors (i.e. Insight)

/* encryptBlock
 *   Encrypts byte array data of length len with key using TEA
 * Arguments:
 *   data - pointer to 8 bit data array to be encrypted
 *   len - length of array
 *   key - Pointer to four integer array (16 bytes) holding TEA key
 * Returns:
 *   data - encrypted data held here
 *   len - size of the new data array
 * Side effects:
 *   Modifies data
 */
int encryptBlock(uint8_t * data, uint32_t * key, int len)
{
   uint32_t blocks, i;
   uint32_t * data32;
   int l;

   // treat the data as 32 bit unsigned integers
   data32 = (uint32_t *) data;

   // Find the number of 8 byte blocks, add one for the length
   blocks = (((len) + 7) / 8) + 1;

   // Set the last block to the original data length
   data32[(blocks*2) - 1] = len;

   // Set the encrypted data length
   l = blocks * 8;

   for(i = 0; i< blocks; i++)
   {
       encipher(&data32[i*2], key);
   }

   return l;
}

/* decryptBlock
 *   Decrypts byte array data of length len with key using TEA
 * Arguments:
 *   data - pointer to 8 bit data array to be decrypted
 *   len - length of array
 *   key - Pointer to four integer array (16 bytes) holding TEA key
 * Returns:
 *   data - decrypted data held here
 * Side effects:
 *   Modifies data
 */
int teaDecryptBlock(uint8_t * data, uint32_t * key, int len)
{
   uint32_t blocks, i;
   uint32_t * data32;

   // treat the data as 32 bit unsigned integers
   data32 = (uint32_t *) data;

   // Find the number of 8 byte blocks
   blocks = (len)/8;

   for(i = 0; i< blocks; i++)
   {
      decipher(&data32[i*2], key);
   }

   // Return the length of the original data
   //len = data32[(blocks*2) - 1];
   return 0;
}
