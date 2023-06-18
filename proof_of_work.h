#ifndef SHA256_H
#define SHA256_H

#include <string.h>
#include <stdbool.h>
#include <openssl/evp.h>

#define POW_SUCCESS 0
#define POW_NOTFOUND -1
#define POW_TERMINATED -2

extern volatile bool terminateFindNonce;

/// @brief sha256을 사용하여 hash값을 버퍼에 저장
/// @param inputString 
/// @param outputBuffer 
void sha256_hash_string(const unsigned char *inputString, char outputBuffer[65]);

/// @brief 난이도에 맞는 nonce를 찾아서 반환
/// @param nonce 찾은 nonce 정수
/// @param hashresult 정답 nonce의 hash값 반환 버퍼
/// @param challenge 챌린지 문자열
/// @param difficulty 난이도 정수
/// @param startNonce 시작 nonce값
/// @param nonceRange nonce 범위
/// @return 
int findNonce(unsigned int* nonce, char hashresult[65], const char * challenge, int difficulty, unsigned int startNonce, unsigned int nonceRange);

#endif