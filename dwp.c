#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define DWP_HEADER_LENGTH 10  // DWP 패킷 헤더 길이
#define DWP_BODY_LENGTH 127 // DWP 패킷 바디 최대 길이
#define DWP_LENGTH (DWP_HEADER_LENGTH + DWP_BODY_LENGTH)  // DWP 패킷 최대 길이
#define DWP_QR_REQUEST 0  // DWP 패킷 QR-요청 필드
#define DWP_QR_RESPONSE 1 // DWP 패킷 QR-응답 필드
#define DWP_TYPE_WORK 0 // DWP 패킷 Type-작업요청 필드
#define DWP_TYPE_STOP 1 // DWP 패킷 Type-중단요청 필드
#define DWP_TYPE_SUCCESS 0  // DWP 패킷 Type-성공 필드
#define DWP_TYPE_FAIL 1 // DWP 패킷 Type-실패 필드

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

/**
 * @brief 초기 DWP 요청 패킷을 생성하는 함수이다.
 * 
 * @param difficulty 난이도
 * @param workload 작업량
 * @param challenge 챌린지
 * @param bodylen 바디의 길이
 * @param packet 반환되는 요청 패킷
 * @return int 함수 실행 결과값
 */
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

/**
 * @brief 성공 응답 패킷을 생성하는 함수이다.
 * 
 * @param difficulty 난이도
 * @param nonce 정답 nonce
 * @param workload 작업량
 * @param challenge 챌린지
 * @param bodylen 바디의 길이
 * @param packet 반환되는 성공응답 패킷
 * @return int 함수 실행 결과값
 */
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

/**
 * @brief DWP 패킷 구조체를 기반으로 문자 배열을 생성하는 함수이다.
 * 
 * @param packet src. - 패킷 구조체
 * @param buffer dest. - 문자 배열
 * @return int buffer 변수에 할당된 총 바이트 수. 실패 시 -1
 */
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
  }

  return totalSize;
}

/**
 * @brief DWP 문자 배열을 기반으로 패킷 구조체을 생성하는 함수이다.
 * 
 * @param buffer src. - 문자 배열
 * @param packet dest. - 패킷 구조체
 * @return int packet 변수에 할당된 총 바이트 수. 실패 시 -1
 */
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
  totalSize += bodylen;

  return totalSize;
}

/**
 * @brief 지정된 파일디스크립터를 통해 패킷을 송신하는 함수이다.
 * 
 * @param fd 패킷을 송신할 파일디스크립터
 * @param qr 패킷의 QR 필드
 * @param type 패킷의 TYPE 필드
 * @param packet 송신할 패킷. type이 DWP_TYPE_STOP이나 DWP_TYPE_FAIL인 경우 무시
 * @return int 함수의 실행결과
 */
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
        // 작업중단/실패 패킷을 생성한다.
        dwp_packet tmpPacket;
        tmpPacket.data.qr = qr;
        tmpPacket.data.type = DWP_TYPE_STOP;
        tmpPacket.data.difficulty = 0;
        tmpPacket.data.bodylen = 0;
        tmpPacket.nonce = 0;
        tmpPacket.workload = 0;
        tmpPacket.challenge = NULL;
        size = dwp_to_arraybuffer(&tmpPacket, packetArray);
      }
      break;
    default:
      return -1;
  }

  // 패킷 정보가 담긴 문자 배열을 전송한다.
  int res = send(fd, packetArray, size, 0);
  return res;
}

/**
 * @brief 지정된 파일디스크립터를 통해 패킷을 수신하는 함수이다.
 * 
 * @param fd 패킷을 수신할 파일디스크립터
 * @param packet 수신한 패킷이 담길 패킷 구조체
 * @return int 함수의 실행결과
 */
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

/**
 * @brief 패킷의 깊은 복사를 수행하는 함수이다.
 * 
 * @param dest 복사되는 패킷 구조체
 * @param src 복사하는 패킷 구조체
 * @return int 함수 실행 결과
 */
int dwp_copy(dwp_packet* dest, const dwp_packet* src)
{
  dest->data = src->data;
  dest->nonce = src->nonce;
  dest->workload = src->workload;

  // 이전에 할당된 challenge 메모리를 해제
  free(dest->challenge);

  // 새로운 challenge 메모리를 할당하고 src->challenge를 복사
  int bodylen = src->data.bodylen;
  dest->challenge = (char*)malloc(bodylen + 1);
  if (dest->challenge == NULL) {
    return -1;
  }
  strncpy(dest->challenge, src->challenge, bodylen);
  dest->challenge[bodylen] = '\0';

  return 0;
}

/**
 * @brief packet 구조체에 할당된 자원을 반환하는 함수이다.
 * 
 * @param packet 자원을 반환할 패킷 구조체
 * @return int 함수 실행결과
 */
int dwp_destroy(dwp_packet* packet)
{
  if (packet->challenge != NULL) {
    free(packet->challenge);
    packet->challenge = NULL;
  }
  
  return 0;
}