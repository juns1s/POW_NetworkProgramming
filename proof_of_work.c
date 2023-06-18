#include <string.h>
#include <stdbool.h>
#include <openssl/sha.h>

#define POW_SUCCESS 0
#define POW_NOTFOUND -1
#define POW_TERMINATED -2

volatile bool terminateFindNonce = false;

void sha256_hash_string(const unsigned char *inputString, char outputBuffer[65])
{
    SHA256_CTX sha256Context;
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_Init(&sha256Context);
    SHA256_Update(&sha256Context, inputString, strlen((const char *)inputString));
    SHA256_Final(hash, &sha256Context);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }

    outputBuffer[64] = '\0';
}

int findNonce(unsigned int* nonce, char hashresult[65], const char * challenge, int difficulty, unsigned int startNonce, unsigned int nonceRange){
    char hash[65];

    // 난이도에 부합하는 비교용 문자열 생성
    char target[difficulty + 1];
    memset(target, '0', difficulty);
    target[difficulty] = '\0';

    for(int i = 0; i < nonceRange; i++){
        if (terminateFindNonce) {
          return POW_TERMINATED;
        }
        char inputString[100];
        // challenge + nonce 문자열
        sprintf(inputString, "%s%d", challenge, startNonce+i);

        // 해시값 계산
        sha256_hash_string((const unsigned char *)inputString, hash);

        printf("Input: %s\t ->SHA-256 Hash: ", inputString);
        printf("%s\n\n", hash);

        //hash값이 난이도 조건을 충족하는 경우
        if (strncmp(hash, target, difficulty) == 0) {
            printf("Nonce found: %d\n", startNonce+i);
            *nonce = startNonce + i;
            strcpy(hashresult, hash);
            return POW_SUCCESS;
        }

        if(i%10000==0)
            system("clear");
    }
    printf("Nonce is not this range\n");
    *nonce = -1;
    sprintf(hashresult, "failed");
    return POW_NOTFOUND;
}