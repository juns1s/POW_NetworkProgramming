all: main_server working_server

working_server: proof_of_work.o dwp.o working_server.o
	gcc -o working_server proof_of_work.o dwp.o working_server.o -lpthread -lssl -lcrypto

main_server: dwp.o main_server.o
	gcc -o main_server dwp.o main_server.o -lpthread

dwp.o: dwp.h dwp.c
	gcc -c -o dwp.o dwp.c

proof_of_work.o: proof_of_work.h proof_of_work.c
	gcc -c -o proof_of_work.o proof_of_work.c -lssl -lcrypto

main_server.o: main_server.c dwp.h
	gcc -c -o main_server.o main_server.c -lpthread

working_server.o: working_server.c dwp.h proof_of_work.h
	gcc -c -o working_server.o working_server.c -lpthread -lssl -lcrypto

clean:
	rm -f *.o
