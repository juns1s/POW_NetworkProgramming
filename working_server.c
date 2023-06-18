/*
 * MIT License
 *
 * Copyright (c) 2018 Lewis Van Winkle
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include "dwp.h"
#include "proof_of_work.h"

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

void errProc(const char* str);
void* readThread(void *);
void* findNonceThread(void*);

static bool isFinished = false;
static bool isWorkRequested = false;
static dwp_packet packet;

// 조건 변수와 뮤텍스 선언
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) 
{
  if (argc < 3) {
      fprintf(stderr, ">> usage: tcp_client hostname port\n");
      return 1;
  }

  // hostname과 port를 사용해서 메인서버의 주소를 구한다. 
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *peer_address;
  if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
      errProc("getaddrinfo");
  }

  // 소켓을 생성한다.
  SOCKET serverSd;
  serverSd = socket(peer_address->ai_family,
          peer_address->ai_socktype, peer_address->ai_protocol);
  if (!ISVALIDSOCKET(serverSd)) {
      errProc("socket");
  }

  // 메인서버에 연결한다.
  if (connect(serverSd,
              peer_address->ai_addr, peer_address->ai_addrlen)) {
      errProc("connect");
  }
  freeaddrinfo(peer_address); // peer_address에 대한 메모리를 해제한다.

  printf(">> Connected to main server\n");

  memset(&packet, 0, sizeof(packet));

  // 서버로부터 메시지를 수신하는 작업과, nonce 값을 찾는 작업을 멀티스레드를 통해 동시에 수행한다.
  pthread_t thread_read, thread_find_nonce;
  pthread_create(&thread_read, NULL, readThread, (void *)&serverSd);
  pthread_create(&thread_find_nonce, NULL, findNonceThread, (void *)&serverSd);
  pthread_join(thread_read, NULL);
  pthread_join(thread_find_nonce, NULL);

  // 자원을 반환한다.
  CLOSESOCKET(serverSd);
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
  dwp_destroy(&packet);
  return 0;
}

/**
  * 발생한 에러에 관한 에러 메시지를 출력하는 함수이다.
*/
void errProc(const char* str)
{
	fprintf(stderr,"## %s: %s\n", str, strerror(errno));
	exit(errno);
}

/**
 * @brief 연결된 메인서버 소켓에서 패킷을 수신하고 처리하는 함수이다.
 * 
 * @param arg 메인서버의 소켓 포인터
 */
void* readThread(void* arg)
{
  SOCKET serverSd = *(SOCKET*)arg;

  dwp_packet reqPacket;
  memset(&reqPacket, 0, sizeof(reqPacket));
  int recvLen;
  while (true) {
    // 메인서버에서 온 패킷이 있는지 확인한다.
    recvLen = dwp_recv(serverSd, &reqPacket);
    if (recvLen < 0) {
      printf(">> Connection closed by main server.\n");
      break;
    }

    // 패킷이 응답(response) 패킷인 경우 무시한다.
    if (reqPacket.data.qr == DWP_QR_RESPONSE) {
      fprintf(stderr, "## Invalid response packet.\n");
      continue;
    }

    switch (reqPacket.data.type) {
      case DWP_TYPE_WORK: // 수신한 패킷이 작업 요청인 경우
        printf(">> The work request is received\n");
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&cond);
        dwp_copy(&packet, &reqPacket);
        isWorkRequested = true;
        pthread_mutex_unlock(&mutex);
        dwp_destroy(&reqPacket);
        break;
      case DWP_TYPE_STOP: // 수신한 패킷이 중단 요청인 경우
        printf(">> The stop request is received\n");
        terminateFindNonce = true;
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&cond);
        isFinished = true;
        pthread_mutex_unlock(&mutex);
        break;
      default:
        fprintf(stderr, "## Invalid packet type.\n");
        break;
    }

    if (isFinished) {
      break;
    }
  }

  CLOSESOCKET(serverSd);
  dwp_destroy(&reqPacket);
}

/**
 * @brief findNonce 함수를 실행시키고, 그 결과에 대한 처리를 하는 함수이다.
 * 
 * @param arg 메인서버의 소켓 포인터
 */
void* findNonceThread(void* arg)
{
  SOCKET serverSd = *(SOCKET*)arg;
  dwp_packet reqPacket, resPacket;
  while (true) {
    pthread_mutex_lock(&mutex);

    // 작업 요청이나 중단 요청이 올 때까지 대기
    while (!isFinished && !isWorkRequested) {
      pthread_cond_wait(&cond, &mutex);
    }

    // 중단 요청이 온 경우 findNonce 및 스레드 종료
    if (isFinished) { 
      pthread_mutex_unlock(&mutex);
      break;
    }

    // 작업 요청이 온 경우 findNonce 함수 실행
    isWorkRequested = false;
    memset(&reqPacket, 0, sizeof(reqPacket));
    dwp_copy(&reqPacket, &packet);
    dwp_destroy(&packet);
    pthread_mutex_unlock(&mutex);

    unsigned int resultNonce;   // 결과 nonce
    char sha256Hash[65];        // 챌린지와 결과 nonce에 해당하는 해시 값

    int difficulty = reqPacket.data.difficulty;     // 난이도
    unsigned int startNonce = reqPacket.nonce;      // 작업 시작 nonce
    unsigned int workload = reqPacket.workload;     // 작업량
    char* challenge = strdup(reqPacket.challenge);  // 챌린지

    printf(">> Start to find nonce in range: [%d..%d)\n", startNonce, startNonce + workload);

    // nonce 값을 찾는다.
    int res = findNonce(&resultNonce, sha256Hash, challenge, difficulty, startNonce, workload);

    printf(">> End to find nonce\n");

    switch (res) {
      case POW_NOTFOUND:  // 난이도 조건을 만족하는 nonce 값이 없는 경우
        dwp_send(serverSd, DWP_QR_RESPONSE, DWP_TYPE_FAIL, NULL);
        printf(">> Failure response is sent\n");
        break;
      case POW_SUCCESS: // nonce 값을 찾은 경우
        memset(&resPacket, 0, sizeof(resPacket));
        dwp_create_res(difficulty, resultNonce, workload, challenge, strlen(challenge), &resPacket);
        dwp_send(serverSd, DWP_QR_RESPONSE, DWP_TYPE_SUCCESS, &resPacket);
        printf(">> Success response is sent\n");
        break;
      case POW_TERMINATED:  // findNonce가 중단된 경우
        printf(">> findNonce is terminated\n");
        break;
      default:
        break;
    }

    free(challenge);
    dwp_destroy(&reqPacket);
  }

  dwp_destroy(&reqPacket);
  dwp_destroy(&resPacket);
}