#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include "dwp.h"

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define NUM_WORKING_SERVER 2
#define MAX_INPUT_LENGTH 1000

void errProc(const char *);
void * client_module(void *);
int makeNbSocket(SOCKET);

static int nextNonce = 0;
static int resultNonce = -1;
static pthread_mutex_t mutex;

typedef struct {
  dwp_packet packet;
  SOCKET socket;
} ThreadParams;

int main(int argc, char** argv)
{
	SOCKET listenSd, connectSd[NUM_WORKING_SERVER];
	struct sockaddr_in clntAddr;
	int clntAddrLen;
  dwp_packet reqPacket;
	pthread_t thread[NUM_WORKING_SERVER];

	if(argc < 3) {
    fprintf(stderr, ">> usage: main_server hostname port\n");
		return -1;
	}

	printf(">> Server started\n");

  // 메인서버 주소를 설정한다.
  struct addrinfo hints;
  struct addrinfo *bind_address;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(argv[1], argv[2], &hints, &bind_address)) {
    errProc("getaddrinfo");
  }

  // 소켓을 생성한다.
	listenSd = socket(bind_address->ai_family, 
    bind_address->ai_socktype, bind_address->ai_protocol);
	if (!ISVALIDSOCKET(listenSd)) {
    errProc("socket");
  }

  // 메인서버 소켓에 주소를 지정한다.
	if (bind(listenSd, bind_address->ai_addr, bind_address->ai_addrlen)) {
    errProc("bind");
  }
  freeaddrinfo(bind_address);

  // 작업서버의 연결을 기다린다.
	if (listen(listenSd, NUM_WORKING_SERVER) < 0) {
    errProc("listen");
  }
	
  // 뮤텍스 객체를 초기화한다.
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    errProc("pthread_mutex_init");
  }

  // 주어진 작업서버의 개수만큼 연결을 허용한다.
	clntAddrLen = sizeof(clntAddr);
	for (int i = 0; i < NUM_WORKING_SERVER; i++) {
		connectSd[i] = accept(listenSd,
			   	(struct sockaddr *) &clntAddr, &clntAddrLen);
		if(!ISVALIDSOCKET(connectSd[i])) {
			errProc("accept");
		}
    printf(">> A working server is connected\n");
	}

  // 난이도와 서버당 작업량, 챌린지를 입력받는다.
  int difficulty, bodylen;
  unsigned int workload;
  char* challenge;
  char input[MAX_INPUT_LENGTH];
  memset(&reqPacket, 0, sizeof(reqPacket));

  while (true) {
    printf(">> Input difficulty:\n");
    fgets(input, sizeof(input), stdin);
    difficulty = atoi(input);
    if (difficulty > 0) {
      break;
    }
  }
  
  while (true) {
    printf(">> Input workload:\n");
    fgets(input, sizeof(input), stdin);
    workload = strtoul(input, NULL, 10);
    if (workload > 0) {
      break;
    }
  }
  
  while (true) {
    printf(">> Input challenge:\n");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0'; // 개행문자를 널문자로 변경
    challenge = strdup(input);
    bodylen = strlen(challenge);
    if (bodylen > 0) {
      break;
    }
    free(challenge);
  }
  
  // 입력받은 데이터를 토대로 초기 DWP 요청 패킷을 생성한다.
  dwp_create_req(difficulty, workload, challenge, bodylen, &reqPacket);

  // Proof of Work 시작
  clock_t startTime, elapsedtime;
  startTime = clock();

  // 연결된 작업서버에 대한 요청 전송을 멀티스레드로 관리한다.
  for (int i = 0; i < NUM_WORKING_SERVER; i++) {
    ThreadParams* params = malloc(sizeof(ThreadParams));
    params->packet = reqPacket;
    params->socket = connectSd[i];
		pthread_create(&thread[i], NULL, client_module, (void *)params);
  }

  // 결과 nonce를 찾았는지 계속해서 확인한다.
  bool isFinished = false;
  while (true) {
    pthread_mutex_lock(&mutex); // 뮤텍스를 통해 resultNonce 보호
    isFinished = resultNonce >= 0;
    pthread_mutex_unlock(&mutex);
    
    if (isFinished) {
      break;
    }
  }

  // Proof of Work 종료
  elapsedtime = clock() - startTime;

  printf(">> Elapsed Time: %.2lf sec\n", elapsedtime/(double)CLOCKS_PER_SEC);
  printf(">> Challenge: %s\n", challenge);
  printf(">> Difficulty: %d\n", difficulty);
  printf(">> Nonce: %d\n", resultNonce);

  for (int i = 0; i < NUM_WORKING_SERVER; i++) {
    pthread_join(thread[i], NULL);
  }

	CLOSESOCKET(listenSd);
  pthread_mutex_destroy(&mutex);
  free(challenge);
  dwp_destroy(&reqPacket);
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
  * 작업서버를 관리하는 멀티스레드 함수이다.
*/
void * client_module(void * arg)
{
  ThreadParams* params = (ThreadParams*)arg;
  SOCKET connectSd = params->socket;
	dwp_packet reqPacket = params->packet;
  free(params);
  
  // 작업서버 소켓을 논블로킹 소켓으로 설정한다.
  makeNbSocket(connectSd);

  // 각 멀티스레드에 중복되지 않도록 작업을 분배한다.
  pthread_mutex_lock(&mutex);
  reqPacket.nonce = nextNonce;
  nextNonce += reqPacket.workload;
  pthread_mutex_unlock(&mutex);

  // 연결된 작업서버에 작업 요청을 전송한다.
  dwp_send(connectSd, DWP_QR_REQUEST, DWP_TYPE_WORK, &reqPacket);
	printf(">> The work request is sent to #%d\n", connectSd);

  dwp_packet resPacket;
	int recvLen;
  bool isFinished = false;

	while(true) {
    pthread_mutex_lock(&mutex); // 뮤텍스를 통해 resultNonce 보호
    isFinished = resultNonce >= 0;
    pthread_mutex_unlock(&mutex);

    // 다른 스레드에서 정답을 구한 경우, 연결된 작업서버에 중단 요청 전송
    if (isFinished) {
      dwp_send(connectSd, DWP_QR_REQUEST, DWP_TYPE_STOP, NULL);
      printf(">> The stop request is sent to #%d\n", connectSd);
      break;
    }

    // 작업서버에서 온 패킷이 있는지 확인한다.
    recvLen = dwp_recv(connectSd, &resPacket);
		if (recvLen <= 0) { 
      continue;
    }

    // 패킷이 요청(request) 패킷인 경우 무시한다.
    if (resPacket.data.qr == DWP_QR_REQUEST) {
      fprintf(stderr, "#%d Invalid request packet.\n", connectSd);
      continue;
    }

    switch (resPacket.data.type) {
      case DWP_TYPE_SUCCESS:  // 수신한 패킷이 성공 응답인 경우
        pthread_mutex_lock(&mutex);
        isFinished = resultNonce >= 0;
        // 가장 빠르게 제출된 답안을 채택한다.
        if (!isFinished) {
          resultNonce = resPacket.nonce;
          printf(">> The answer is Found: %d\n", resultNonce);
        }
        pthread_mutex_unlock(&mutex);
        break;
      case DWP_TYPE_FAIL: // 수신한 패킷이 실패 응답인 경우
        pthread_mutex_lock(&mutex);
        isFinished = resultNonce >= 0;
        reqPacket.nonce = nextNonce;
        nextNonce += reqPacket.workload;
        // 아직 성공한 작업서버가 없다면, 실패한 작업서버에 다음 작업을 분배
        if (!isFinished) {
          dwp_send(connectSd, DWP_QR_REQUEST, DWP_TYPE_WORK, &reqPacket);
          printf(">> The work request is sent to #%d\n", connectSd);
        }
        pthread_mutex_unlock(&mutex);
        break;
      default:
        fprintf(stderr, "#%d Invalid packet type.\n", connectSd);
        break;
    }
    dwp_destroy(&resPacket);
	}

	CLOSESOCKET(connectSd);
	fprintf(stderr,">> The client #%d is disconnected.\n", connectSd);
}

/**
  * 소켓을 Non-blocking으로 변경하는 함수이다.
*/
int makeNbSocket(SOCKET socket)
{	
  int res;

  res = fcntl(socket, F_GETFL, 0);
  if (res == -1) errProc("fcntl");
  res |= O_NONBLOCK;
  res = fcntl(socket, F_SETFL, res);
  if (res == -1) errProc("fcntl");
  
  return 0;
}