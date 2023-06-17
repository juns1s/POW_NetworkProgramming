#include <string.h>
#include <openssl/evp.h>

void sha256_hash_string(const unsigned char *inputString, char outputBuffer[65])
{
    EVP_MD_CTX *mdctx;
    const EVP_MD *md;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;

    md = EVP_sha256();
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, inputString, strlen((const char *)inputString));
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);
    EVP_MD_CTX_free(mdctx);

    for (int i = 0; i < hashLen; i++)
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);

    outputBuffer[64] = 0;
}

void findNonce(char nonce[20], char hashresult[65], const char * challenge, int difficulty, int startNonce, int nonceRange){
    char hash[65];

    // 난이도에 부합하는 비교용 문자열 생성
    char target[difficulty + 1];
    memset(target, '0', difficulty);
    target[difficulty] = '\0';

    for(int i = 0; i < nonceRange; i++){
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
            sprintf(nonce, "%d", startNonce+i);
            strcpy(hashresult, hash);
            return;
        }

        if(i%10000==0)
            system("clear");
    }
    printf("Nonce is not this range\n");
    sprintf(nonce, "failed");
    sprintf(hashresult, "failed");
}