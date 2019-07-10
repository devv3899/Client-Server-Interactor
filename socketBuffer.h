#ifndef SOCKET_BUFFER_H
#define SOCKET_BUFFER_H

#include <stdio.h>
#include <stdlib.h>

	
typedef struct SocketNode {
	char c;
	struct SocketNode *next;
} SocketNode;

typedef struct SocketBuffer {
	int size;
	SocketNode *head;
	SocketNode *tail;
} SocketBuffer;

static void addCharToBuffer(SocketBuffer *socketBuffer, char c) {
	SocketNode *node = malloc(sizeof(SocketNode));
	node->c = c;
	node->next = NULL;
	
	if(socketBuffer->tail == NULL) {
		socketBuffer->tail = node;
		socketBuffer->head = node;
	} else {
		socketBuffer->tail->next = node;
		socketBuffer->tail = node;
	}
	socketBuffer->size += 1;
}

static SocketBuffer *createBuffer() {
	SocketBuffer *socketBuffer = malloc(sizeof(SocketBuffer));
	socketBuffer->head = NULL;
	socketBuffer->tail = NULL;
	socketBuffer->size = 0;
	return socketBuffer;
}

// this function returns a string, which user should deallocate himself.
static char* readAllBuffer(SocketBuffer *socketBuffer) {
	char *result = malloc(sizeof(char) * (socketBuffer->size + 1));
	SocketNode *node = socketBuffer->head;
	
	int i = 0;
	while(node != NULL) {
		result[i++] = node->c;
		SocketNode *d = node;
		node = node->next;
		free(d);
	}
	result[i] = '\0'; // Add null terminator at last
	socketBuffer->head = NULL;
	socketBuffer->tail = NULL;
	socketBuffer->size = 0;
	return result;
}

static void readNBytes(SocketBuffer *socketBuffer, int sockfd, long int numBytes) {
	char c;
	long int i = 0;
	while(i++ < numBytes) {
		read(sockfd, &c, 1);
		if(c == '\0') {			
			break; // Client disconnected.
		}
		addCharToBuffer(socketBuffer, c);
	}
}

static void readTillDelimiter(SocketBuffer *socketBuffer, int sockfd, char delimiter) {
	char c;
	while(1) {
		read(sockfd, &c, 1);
		
		// for files,if EOF is reached, we get \0
		if(c == '\0') {
			break; // Client disconnected.
		}
		
		if(c == delimiter) {
			break;
		}
		
		// Do not read the delimiter to buffer.
		addCharToBuffer(socketBuffer, c);
	}
}

static void clearSocketBuffer(SocketBuffer *socketBuffer) {
	SocketNode *node = socketBuffer->head;

	while(node != NULL) {
		SocketNode *d = node;
		node = node->next;
		free(d);
	}
	
	socketBuffer->head = NULL;
	socketBuffer->tail = NULL;
	socketBuffer->size = 0;
	// Do not free the buffer object
}

static void freeSocketBuffer(SocketBuffer *socketBuffer) {
	clearSocketBuffer(socketBuffer);
	free(socketBuffer);
}

#endif