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

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define NUM_WORKING_SERVER 2

void errProc(const char *);
void * client_module(void *);
int makeNbSocket(SOCKET);

static int nextNonse = 0;
static int resultNonse = -1;
static pthread_mutex_t mutex;

int main(int argc, char** argv)
{
	SOCKET listenSd, connectSd[NUM_WORKING_SERVER];
	struct sockaddr_in clntAddr;
	int clntAddrLen, strLen;
	pthread_t thread;

	if(argc != 2)
	{
		printf(">> Usage: %s [Port Number]\n", argv[0]);
		return -1;
	}

	printf(">> Server started\n");

  // 메인서버 주소를 설정한다.
  struct addrinfo hints;
  struct addrinfo *bind_address;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, argv[1], &hints, &bind_address)) {
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
    printf("A working server is connected...\n");
	}

  // 난이도와 서버당 작업량, 챌린지를 입력받는다.
  // TODO: 입력된 정보를 바탕으로 패킷 구조체 초기화
  char read[1000];
  printf(">> Input difficulty:\n");
  fgets(read, 1000, stdin);
  
  printf(">> Input workload:\n");
  fgets(read, 1000, stdin);
  
  printf(">> Input challenge:\n");
  fgets(read, 1000, stdin);

  // Proof of Work 시작
  clock_t startTime, elapsedtime;
  startTime = clock();

  // 연결된 작업서버에 대한 요청 전송을 멀티스레드로 관리한다.
  // TODO: client_module에 소켓에 더해 패킷도 전달
  for (int i = 0; i < NUM_WORKING_SERVER; i++) {
		pthread_create(&thread, NULL, client_module, (void *) &connectSd[i]);
		pthread_detach(thread);
  }

  bool isFinished = false;
  while (true) {
    pthread_mutex_lock(&mutex); // 뮤텍스를 통해 resultNonse 보호
    isFinished = resultNonse >= 0;
    pthread_mutex_unlock(&mutex);
    
    if (isFinished) {
      break;
    }
  }

  elapsedtime = clock() - startTime;

  printf(">> Elapsed Time: %.2lf\n", elapsedtime/(double)1000);
  printf(">> Nonse: %d\n", resultNonse);

	CLOSESOCKET(listenSd);
  pthread_mutex_destroy(&mutex);
	return 0;
}

/**
  * 발생한 에러에 관한 에러 메시지를 출력하는 함수이다.
*/
void errProc(const char* str)
{
	fprintf(stderr,"%s: %s", str, strerror(errno));
	exit(errno);
}

/**
  * 작업서버를 관리하는 멀티스레드 함수이다.
*/
void * client_module(void * data)
{
  SOCKET connectSd;
	char sBuff[BUFSIZ] = "hello\n";
  connectSd = *((int *) data);
  makeNbSocket(connectSd);

  // 연결된 작업서버에 작업 요청 전송
  // TODO
  send(connectSd, sBuff, strlen(sBuff), 0);
	
  char rBuff[BUFSIZ];
	int recvLen;
  bool isFinished = false;

	while(true) {
    pthread_mutex_lock(&mutex); // 뮤텍스를 통해 resultNonse 보호
    isFinished = resultNonse >= 0;
    pthread_mutex_unlock(&mutex);

    // 다른 스레드에서 정답을 구한 경우, 연결된 작업서버에 중단 요청 전송
    if (isFinished) {
      // TODO
      send(connectSd, sBuff, strlen(sBuff), 0);
      break;
    }

    recvLen = recv(connectSd, rBuff, sizeof(rBuff), 0);
		if (recvLen <= 0) {
      continue;
    }

		// 패킷 type에 맞는 처리
    // TODO
	}

	fprintf(stderr,"The client is disconnected.\n");
	close(connectSd);
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