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

int dwp_create_req(int difficulty, unsigned int workload, const char* challenge, int bodylen, dwp_packet* packet)
{
  packet->data.qr = DWP_QR_REQUEST;
  packet->data.type = DWP_TYPE_WORK;
  packet->data.difficulty = difficulty;
  packet->data.bodylen = bodylen;
  packet->nonce = 0;
  packet->workload = workload;
  packet->challenge = strdup(challenge);
  if (packet->challenge == NULL) {
    return -1;
  }
  return 0;
}

int dwp_create_res(int difficulty, unsigned int nonce, unsigned int workload, const char* challenge, int bodylen, dwp_packet* packet)
{
  packet->data.qr = DWP_QR_RESPONSE;
  packet->data.type = DWP_TYPE_SUCCESS;
  packet->data.difficulty = difficulty;
  packet->data.bodylen = bodylen;
  packet->nonce = nonce;
  packet->workload = workload;
  packet->challenge = strdup(challenge);
  if (packet->challenge == NULL) {
    return -1;
  }
  return 0;
}

int dwp_to_arraybuffer(const dwp_packet* packet, char* buffer)
{
  int totalSize = 0;
  int size = 0;

  // data 필드 복사
  size = sizeof(packet->data);
  if ((totalSize += size) > DWP_LENGTH) {
    return -1;
  }
  memcpy(buffer, &(packet->data), size);
  buffer += size;

  // nonce 필드 복사
  size = sizeof(packet->nonce);
  if ((totalSize += size) > DWP_LENGTH) {
    return -1;
  }
  memcpy(buffer, &(packet->nonce), size);
  buffer += size;
  
  // workload 필드 복사
  size = sizeof(packet->workload);
  if ((totalSize += size) > DWP_LENGTH) {
    return -1;
  }
  memcpy(buffer, &(packet->workload), size);
  buffer += size;
  
  if (packet->challenge != NULL) {
    // challenge 필드 복사
    size = strlen(packet->challenge);
    if ((totalSize += size) > DWP_LENGTH) {
      return -1;
    }
    memcpy(buffer, packet->challenge, size);  // '\0'은 buffer에서 제외된다.
    totalSize += size;
  }

  return totalSize;
}

int dwp_to_struct(const char* buffer, dwp_packet* packet)
{
  int totalSize = 0;
  int size = 0;

  // data 필드 복사
  size = sizeof(packet->data);
  memcpy(&(packet->data), buffer, size);
  buffer += size;
  totalSize += size;

  // nonce 필드 복사
  size = sizeof(packet->nonce);
  memcpy(&(packet->nonce), buffer, size);
  buffer += size;
  totalSize += size;
  
  // workload 필드 복사
  size = sizeof(packet->workload);
  memcpy(&(packet->workload), buffer, size);
  buffer += size;
  totalSize += size;

  // challenge 필드 복사
  int bodylen = packet->data.bodylen;
  if (bodylen > 0) {
    packet->challenge = (char*)malloc(bodylen + 1);
    memcpy(packet->challenge, buffer, bodylen);
    packet->challenge[bodylen] = '\0';
  }
  else {
    packet->challenge = NULL;
  }

  return totalSize;
}

int dwp_send(int fd, int qr, int type, const dwp_packet* packet)
{
  char packetArray[DWP_LENGTH];
  int size;
  switch (type) {
    case DWP_TYPE_WORK:
      size = dwp_to_arraybuffer(packet, packetArray);
      break;
    case DWP_TYPE_STOP:
      {
        dwp_packet* tmpPacket;
        memset(tmpPacket, 0, sizeof(tmpPacket));
        tmpPacket->data.qr = qr;
        tmpPacket->data.type = DWP_TYPE_STOP;
        size = dwp_to_arraybuffer(tmpPacket, packetArray);
      }
      break;
    default:
      return -1;
  }

  int res = send(fd, packetArray, size, 0);
  return res;
}

int dwp_recv(int fd, dwp_packet* packet)
{
  char packetArray[DWP_LENGTH];
  int length;

  length = recv(fd, packetArray, DWP_LENGTH, 0);
  if (length <= 0) {
    return -1;
  }

  length = dwp_to_struct(packetArray, packet);

  return length;
}

int dwp_copy(dwp_packet* dest, const dwp_packet* src)
{
  dest->data = src->data;
  dest->nonce = src->nonce;
  dest->workload = src->workload;

  // 이전에 할당된 challenge 메모리를 해제
  free(dest->challenge);

  // 새로운 challenge 메모리를 할당하고 src->challenge를 복사
  dest->challenge = (char*)malloc(strlen(src->challenge));
  if (dest->challenge == NULL) {
    return 0;
  }
  strcpy(dest->challenge, src->challenge);

  return 0;
}

int dwp_destroy(dwp_packet* packet)
{
  free(packet->challenge);
  packet->challenge = NULL;
}