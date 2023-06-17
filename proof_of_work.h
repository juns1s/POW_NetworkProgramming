#ifndef SHA256_H
#define SHA256_H

void sha256_hash_string(const unsigned char *string, char outputBuffer[65]);

char * findNonce(char * challenge, int difficulty);

#endif