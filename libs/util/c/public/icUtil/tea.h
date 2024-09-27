/*-----------------------------------------------
 * tea.h
 *
 * TEA encrypt/decrypt functions that are carryover
 * from QNX days.  No idea where this code originally
 * came from, but we do know it works.
 *
 * Author: unknown
 *-----------------------------------------------*/

#ifndef IC_TEA_H_
#define IC_TEA_H_

#include <stdint.h>

#define KEY_SIZE 4
#define TRUE 1
#define FALSE 0

extern int teaEncrypt(unsigned char* data, char *keyStr, int dataSize);

extern int teaDecrypt(unsigned char* data, char *keyStr, int dataSize);

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
int teaDecryptBlock(uint8_t * data, uint32_t * key, int len);

#endif /* IC_TEA_H_*/
