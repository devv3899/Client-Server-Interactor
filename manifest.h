#ifndef MANIFEST_H
#define MANIFEST_H

#include <stdio.h>
#include <stdlib.h>
#include "socketBuffer.h"


/*
Manifest file format:
<project Name>
<project Version>
<numFiles>
<md5hash><space><last modified time in ms><space><file path>
<md5hash><space><last modified time in ms><space><file path>
<md5hash><space><last modified time in ms><space><file path>
..

The filePaths are relative to the manifest file.
*/

typedef struct ManifestNode {
	char *md5;
	char *version;
	char *filePath;
	struct ManifestNode *next;
} ManifestNode;

typedef struct Manifest {
	char *projectName;
	char *versionNumber;
	int numFiles;
	ManifestNode *head;
	ManifestNode *tail;
} Manifest;

void freeManifestNode(ManifestNode *d);

void addFileToManifest(Manifest *manifest, char *md5, char *version, char *filePath) {
	
	ManifestNode *node = malloc(sizeof(ManifestNode));
	node->md5 = md5;
	node->version = version;
	node->filePath = filePath;
	node->next = NULL;
	
	if(manifest->tail == NULL) {
		manifest->tail = node;
		manifest->head = node;
	} else {
		manifest->tail->next = node;
		manifest->tail = node;
	}
	
	manifest->numFiles += 1;
}

// Read Manifest only reads the manifest content into the structure..
// No error checking is part of this.. We should do it before calling this method.
Manifest *readManifestContents(int fd) {
	
	SocketBuffer *socketBuffer = createBuffer();
	
	Manifest *manifest = malloc(sizeof(Manifest));
	manifest->head = NULL;
	manifest->tail = NULL;
	manifest->numFiles = 0;
	
	///////////////////// MANIFEST CONTENTS BEGIN NOW ////////////////
	
	// first line is projectName
	readTillDelimiter(socketBuffer, fd, '\n');
	manifest->projectName = readAllBuffer(socketBuffer);
	
	// second line is projectVersion
	readTillDelimiter(socketBuffer, fd, '\n');
	manifest->versionNumber = readAllBuffer(socketBuffer);
	
	// third line is numFiles
	readTillDelimiter(socketBuffer, fd, '\n');
	char *x = readAllBuffer(socketBuffer);
	int numFiles = atoi(x);
	free(x); // free the string given by buffer.
	
	// Now read n files.
	int i = 0;
	while(i++ < numFiles) {
		char *md5, *version, *filePath;
		
		readTillDelimiter(socketBuffer, fd, ' ');
		md5 = readAllBuffer(socketBuffer);
		
		readTillDelimiter(socketBuffer, fd, ' ');
		version = readAllBuffer(socketBuffer);
		
		readTillDelimiter(socketBuffer, fd, '\n');
		filePath = readAllBuffer(socketBuffer);

		addFileToManifest(manifest, md5, version, filePath);
	}
	
	// Now buffer's work is over, delete it.
	freeSocketBuffer(socketBuffer);
	
	return manifest;
}


// This method writes the given manifest to a socket/file descriptor.
void writeManifestToFile(Manifest *manifest, int fd) {
	
	char buffer[100];
	
	write(fd, manifest->projectName, strlen(manifest->projectName));
	write(fd, "\n", 1);
	write(fd, manifest->versionNumber, strlen(manifest->versionNumber));
	write(fd, "\n", 1);
	
	sprintf(buffer, "%d", manifest->numFiles);	
	write(fd, buffer, strlen(buffer));
	write(fd, "\n", 1);
	
	ManifestNode *start = manifest->head;
	while(start != NULL) {
		write(fd, start->md5, strlen(start->md5));
		write(fd, " ", 1);
		write(fd, start->version, strlen(start->version));
		write(fd, " ", 1);
		write(fd, start->filePath, strlen(start->filePath));
		write(fd, "\n", 1);
		
		start = start->next;
	}
}

ManifestNode* searchFile(Manifest *manifest, char *filePath) {
	
	ManifestNode *node = manifest->head;
	
	int i = 0;
	while(node != NULL) {
		if(strcmp(node->filePath, filePath) == 0) {
			return node;
		}
		node = node->next;
	}
	
	return NULL;
}

void removeFileFromManifest(Manifest *manifest, char *filePath) {
	
	if(manifest->numFiles == 0) {
		return;
	}
	
	ManifestNode *node = manifest->head;
	
	if(strcmp(node->filePath, filePath) == 0) {
		manifest->head = node->next;
		freeManifestNode(node);
		manifest->numFiles -= 1;
		if(manifest->numFiles == 0) {
			manifest->tail = NULL;
		}
	}
	
	ManifestNode *prev = node;
	node = node->next;
	while(node != NULL) {
		if(strcmp(node->filePath, filePath) == 0) {
			prev->next = node->next;
			if(node == manifest->tail) {
				manifest->tail = prev;
			}
			freeManifestNode(node);
			manifest->numFiles -= 1;
			return;
		}
		node = node->next;
	}
}

void freeManifestNode(ManifestNode *d) {
	if(d->md5 != NULL) {
		free(d->md5);
	}
	if(d->version != NULL) {
		free(d->version);
	}
	if(d->filePath != NULL) {
		free(d->filePath);
	}
	free(d);
}

void freeManifest(Manifest *manifest) {
	ManifestNode *node = manifest->head;

	while(node != NULL) {
		ManifestNode *d = node;
		node = node->next;
		freeManifestNode(d);
	}
	if(manifest->projectName != NULL) {
		free(manifest->projectName);
	}
	if(manifest->versionNumber != NULL) {
		free(manifest->versionNumber);
	}
	free(manifest);
}

// Compare and write differences to .update file descriptor
// returns -1 if conflicts are found.
int compareManifests(Manifest *server, Manifest *client, int updateFd) {
	int error = 0;
	int numUpdates = 0;
	char liveHash[100];
	
	ManifestNode* serverFileNode;
	ManifestNode *clientFileNode = client->head;
	while(clientFileNode != NULL) {
			
		// Search client file in Server
		serverFileNode = searchFile(server, clientFileNode->filePath);
		
		// client's file live hash
		char *path = malloc(sizeof(char) * (strlen(clientFileNode->filePath) + strlen(client->projectName) + 25));
		sprintf(path, "%s/%s", client->projectName, clientFileNode->filePath);
		computeFileHash(path, liveHash);
		free(path);
		liveHash[HASH_STRING_LEN] = '\0';
		
		// check for update. 
		// Currently ignoring Update files
		/* if((serverFileNode == NULL 
				&& strcmp(server->versionNumber, client->versionNumber) == 0) ||
			(serverFileNode != NULL 
				&& strcmp(serverFileNode->md5, liveHash) != 0
				&& strcmp(server->versionNumber, client->versionNumber) == 0)) {
			write(updateFd, "U", 1);
			write(updateFd, " ", 1);
			write(updateFd, clientFileNode->version, strlen(clientFileNode->version));
			write(updateFd, " ", 1);
			write(updateFd, liveHash, strlen(liveHash));
			write(updateFd, " ", 1);
			write(updateFd, clientFileNode->filePath, strlen(clientFileNode->filePath));
			write(updateFd, "\n", 1);
			numUpdates++;
		} else */
		
		// check for Modify
		if(serverFileNode != NULL 
			&& strcmp(serverFileNode->version, clientFileNode->version) != 0
			&& strcmp(server->versionNumber, client->versionNumber) != 0
			&& strcmp(clientFileNode->md5, liveHash) == 0) {
			write(updateFd, "M", 1);
			write(updateFd, " ", 1);
			write(updateFd, serverFileNode->version, strlen(serverFileNode->version));
			write(updateFd, " ", 1);
			write(updateFd, serverFileNode->md5, strlen(serverFileNode->md5));
			write(updateFd, " ", 1);
			write(updateFd, serverFileNode->filePath, strlen(serverFileNode->filePath));
			write(updateFd, "\n", 1);
			numUpdates++;
		}
		
		// check for Deletion
		else if(serverFileNode == NULL 
			&& strcmp(server->versionNumber, client->versionNumber) != 0) {
			write(updateFd, "D", 1);
			write(updateFd, " ", 1);
			write(updateFd, clientFileNode->version, strlen(clientFileNode->version));
			write(updateFd, " ", 1);
			write(updateFd, liveHash, strlen(liveHash));
			write(updateFd, " ", 1);
			write(updateFd, clientFileNode->filePath, strlen(clientFileNode->filePath));
			write(updateFd, "\n", 1);
			numUpdates++;
		}
		
		// check for error case.
		else if(serverFileNode != NULL
			&& strcmp(serverFileNode->version, clientFileNode->version) != 0
			&& strcmp(server->versionNumber, client->versionNumber) != 0
			&& strcmp(clientFileNode->md5, liveHash) != 0) {
			error = -1;
			printf("Conflict: %s\n", clientFileNode->filePath);
		}
		
		clientFileNode = clientFileNode->next;
	}
	
	if(error) {
		return error;
	}
	
	serverFileNode = server->head;
	while(serverFileNode != NULL) {
		
		// Search server file in Client now
		clientFileNode = searchFile(client, serverFileNode->filePath);
			
		// check for Addition
		if(clientFileNode == NULL 
			&& strcmp(server->versionNumber, client->versionNumber) != 0) {
			write(updateFd, "A", 1);
			write(updateFd, " ", 1);
			write(updateFd, serverFileNode->version, strlen(serverFileNode->version));
			write(updateFd, " ", 1);
			write(updateFd, serverFileNode->md5, strlen(serverFileNode->md5));
			write(updateFd, " ", 1);
			write(updateFd, serverFileNode->filePath, strlen(serverFileNode->filePath));
			write(updateFd, "\n", 1);
			numUpdates++;
		}
		
		serverFileNode = serverFileNode->next;
	}
	
	if(numUpdates == 0) {		
		printf("Project Up-To-Date\n");
	}
	
	return error;
}






#endif