all: util.o socket_client.o socket_server.o client server

util.o: util.c
	gcc -c util.c
	
socket_client.o: socket_client.c
	gcc -c socket_client.c 
	
socket_server.o: socket_server.c
	gcc -c socket_server.c

client: socket_client.o util.o
	gcc -o WTF util.o socket_client.o -lcrypto -lz
	
server: socket_server.o util.o
	gcc -o WTFserver util.o socket_server.o -lcrypto -lpthread -lz

test:
	gcc -o WTFtest test.c

clean:
	rm -rf WTF WTFserver WTFtest *.o TESTCASE server_repo .configure
