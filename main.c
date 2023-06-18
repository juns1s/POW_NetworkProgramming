#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "proof_of_work.h"

int main(){
    char *challenge = "201928732019283320192836";
    char sha256Hash[65];
    unsigned int nonce;

    int difficulty;
    unsigned int startNonce = 0;
    unsigned int nonceRange = 1000000000;

    printf("difficulty: ");
    scanf("%d", &difficulty);

    findNonce(nonce, sha256Hash, challenge, difficulty,startNonce,nonceRange);

    printf("Challenge + Nonce: %s + %s\n", challenge, nonce);
    printf("Hash: %s\n", sha256Hash);

    return 0;
}