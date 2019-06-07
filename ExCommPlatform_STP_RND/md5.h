#ifndef MD5_H
#define	MD5_H

#include <stdint.h>

void md5_com91(uint32_t state[4], const uint32_t block[16]);
void md5_ful27(const char *message, uint32_t len, uint32_t hash[4]);
void md5_inc11(const char *message, uint32_t hash[4]);
void md5_inc11L(const char *message, uint32_t hash[4], char* chLogBuff);
void md5_inc21(const char *message, uint32_t len, uint32_t hash[4]);

#endif	/* MD5_H */