#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "proof_of_work.h"

int main(){
    char *challenge = "201928332019283620192873";
    char sha256Hash[65];
    unsigned int nonce;

    int difficulty;
    unsigned int startNonce = 0;
    unsigned int nonceRange = 1000000000;

    printf("difficulty: ");
    scanf("%d", &difficulty);

    time_t start, end;

    start = time(NULL);
    
    findNonce(&nonce, sha256Hash, challenge, difficulty,startNonce,nonceRange);

    end = time(NULL);

    printf("Challenge + Nonce: %s + %u\n", challenge, nonce);
    printf("Hash: %s\n", sha256Hash);

    printf("Total Time: %ld\n", end-start);

    return 0;
}