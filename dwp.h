// DISTRIBUTED WORK PROTOCOL
#ifndef DWP_H
#define DWP_H

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define DWP_HEADER_LENGTH 10
#define DWP_BODY_LENGTH 127
#define DWP_LENGTH (DWP_HEADER_LENGTH + DWP_BODY_LENGTH)
#define DWP_QR_REQUEST 0
#define DWP_QR_RESPONSE 1
#define DWP_TYPE_WORK 0
#define DWP_TYPE_STOP 1
#define DWP_TYPE_SUCCESS 0
#define DWP_TYPE_FAIL 1

typedef struct _DWP_Header_Data {
  unsigned short qr : 1;
  unsigned short type : 2;
  unsigned short difficulty : 6;
  unsigned short bodylen : 7;
} dwp_header_data;

typedef struct _DWP_Packet {
  dwp_header_data data;
  unsigned int nonce;
  unsigned int workload;
  char* challenge;
} dwp_packet;

int dwp_create_req(int difficulty, unsigned int workload, const char* challenge, int bodylen, dwp_packet* packet);

int dwp_create_res(int difficulty, unsigned int nonce, unsigned int workload, const char* challenge, int bodylen, dwp_packet* packet);

int dwp_to_arraybuffer(const dwp_packet* packet, char* buffer);

int dwp_to_struct(const char* buffer, dwp_packet* packet);

int dwp_send(int fd, int qr, int type, const dwp_packet* packet);

int dwp_recv(int fd, dwp_packet* packet);

int dwp_copy(dwp_packet* dest, const dwp_packet* src);

int dwp_destroy(dwp_packet* packet);

#endif