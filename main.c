#include <stdio.h>
#include <string.h>
#include "proof_of_work.h"

int main(){
    const char *inputString = "Hello, World?";
    char sha256Hash[65];

    sha256_hash_string((const unsigned char *)inputString, sha256Hash);

    printf("Input: %s\n", inputString);
    printf("SHA-256 Hash: %s\n", sha256Hash);

    return 0;
}