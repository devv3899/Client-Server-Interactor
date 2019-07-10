#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <err.h>

#include "util.h"
#include "manifest.h"
#include "socketBuffer.h"


char *CONFIG_FILE = ".configure";
char *UPDATE_FILE = ".update";
char *COMMIT_FILE = ".commit";
char *REQUEST_FILE = ".request";
char *RESPONSE_FILE = ".response";

/*
 * The function get_sockaddr converts the server's address and port into a form usable to create a 
 * scoket
*/
struct addrinfo* get_sockaddr(const char* hostname, const char *port) {
	struct addrinfo hints;
	struct addrinfo* results;
	int rv;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;          //Return socket address for the server's IPv4 addresses
	hints.ai_socktype = SOCK_STREAM;    //Return TCP socket addresses

	/* Use getaddrinfo will get address information for the host specified by hostnae */
	rv = getaddrinfo(hostname, port, &hints, &results);
	if (rv != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	return results;
}

/*
 * The function open_connection establishes a connection to the server
*/
int open_connection(struct addrinfo* addr_list) {
	struct addrinfo* p;
	int sockfd;
	//Iterate through each addr info in the list; Stop when we successully connect to one

	for (p = addr_list; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

		// Try the next address since the socket operation failed
		if (sockfd == -1) continue;

		//Stop iterating of we are able to connect to the server

		if (connect(sockfd,p->ai_addr, p->ai_addrlen) != -1) break;
	}

	freeaddrinfo(addr_list);

	if (p == NULL)
		err(EXIT_FAILURE, "%s", "Unable to connect");
	else
		return sockfd;
}

int isProjectConfiguredLocally(char *project) {
	
	// Check if project exists
	if(!checkDirectoryExists(project)) {
		printf("Error: The project does not exist.\n");
		return 0;
	}
	
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	
	// check if project directory contains manifest
	sprintf(path, "%s/%s", project, MANIFEST_FILE);
	if(!checkFileExists(path)) {
		printf("Error: Please create project first.\n");
		free(path);
		return 0;
	}
	
	return 1;
}


// returns dynamically created Manifest, Delete yourself.
Manifest* readClientProjectManifest(char *project) {
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	
	sprintf(path, "%s/%s", project, MANIFEST_FILE);
	if(!checkFileExists(path)) {
		free(path);
		return NULL;
	}
	
	int manifestFd = open(path, O_RDONLY, 0777);
	Manifest *manifest = readManifestContents(manifestFd);
	close(manifestFd);
	
	free(path);
	return manifest;
}

Manifest* readServerProjectManifest(char *project, int socket) {
	
	// Make Request.
	// getManifest:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "getManifest", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	// Server Sends back 
	// <manifestNameLen>:<manifestName><ManifestLenBytes>:<ManifestContents>
	
	SocketBuffer *socketBuffer = createBuffer();
	
	// IGNORE manifestNameLen, manifestName
	readTillDelimiter(socketBuffer, socket, ':');
	char *manNameLen = readAllBuffer(socketBuffer);
	long int nameLen = atol(manNameLen);
	free(manNameLen);
	
	readNBytes(socketBuffer, socket, nameLen);
	char *x = readAllBuffer(socketBuffer);
	free(x);
	
	readTillDelimiter(socketBuffer, socket, ':');
	char *manContentLen = readAllBuffer(socketBuffer);
	long int contentLen = atol(manContentLen);
	free(manContentLen);
	
	// Read Manifest now.
	Manifest *serverManifest = NULL;
	
	// If server gave manifest
	if(contentLen != -1) {
		serverManifest = readManifestContents(socket);
	}
	
	freeSocketBuffer(socketBuffer);
	return serverManifest;
}

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

// Compession required as multiple files may come.
void checkoutProject(char *project, int socket) {
	printf("Trying to checkout Project: %s.\n", project);
	fflush(stdout);
	
	// Check if project already exists locally, then fail.
	if(checkDirectoryExists(project)) {
		printf("Error: The project already exists locally.\n");
		return;
	}
	
	// Make Request.
	// checkout:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "checkout", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	
	// Server Sends back 
	// sendfile:<compressed data bytes><compressed data>
	// 
	// <numFiles>:
	//		<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
	//		<File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
	// ...
	// In case of error, Response comes as "failed:<fail Reason>:"	
	SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {
		
		// REMEMBER: COMPRESSED ZLIB RESPONSE
		char *serverRespPath = malloc(sizeof(char) * (strlen(project) + strlen(RESPONSE_FILE) + 50));
		
		// Now, store N bytes unencrypted into the response file.
		sprintf(serverRespPath, "%s_%lld_%d", RESPONSE_FILE, current_timestamp_millis(), rand());
		convertZlibToResponse(socket, serverRespPath, ".");
		
		int responseFd = open(serverRespPath, O_RDONLY, 0777);
		
		/* Core logic now */	
		// Next download files.
		readTillDelimiter(socketBuffer, responseFd, ':');
		char *numFilesStr = readAllBuffer(socketBuffer);
		long numFiles = atol(numFilesStr);
		free(numFilesStr);
		
		// Now read N files, and save them
		while(numFiles-- > 0) {
			writeFileFromSocket(responseFd, project);
		}
		
		/* Core logic ends here */
		close(responseFd);
		unlink(serverRespPath);
		free(serverRespPath);
		printf("Done.\n");
		
	} else {
		printf("Project checkout failed on server.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	free(responseCode);
	freeSocketBuffer(socketBuffer);
}

// Compession Not required for this step, as just single file
// is sent over the network
void createProject(char *project, int socket) {
	printf("Trying to create Project: %s.\n", project);
	fflush(stdout);
	
	if(checkDirectoryExists(project)) {
		printf("Error: The project already exists locally.\n");
		return;
	}
	
	// Make Request.
	// create:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "create", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	
	// Server Sends back 
	// sendfile:<numFiles>:
	//		<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
	//		<File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
	// ...
	// In case of error, Response comes as "failed:<fail Reason>:"
	
	
	SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {
		readTillDelimiter(socketBuffer, socket, ':');
		char *numFilesStr = readAllBuffer(socketBuffer);
		long numFiles = atol(numFilesStr);
		
		// Now read N files, and save them
		// BTW, for create case, only 1 file of manifest will come.
		while(numFiles-- > 0) {
			writeFileFromSocket(socket, project);
		}
		printf("Done.\n");
		free(numFilesStr);
		
	} else {
		printf("Project creation failed on server.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	freeSocketBuffer(socketBuffer);
	free(responseCode);
}

// Compession Not required for this step, as no file
// is sent over the network
void destroyProject(char *project, int socket) {
	printf("Trying to destroy Project: %s.\n", project);
	fflush(stdout);
	
	// Make Request.
	// destroy:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "destroy", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	// Server Sends back 
	// ok:
	// ...
	// In case of error, Response comes as "failed:<fail Reason>:"
	SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "ok") == 0) {
		printf("Project destroyed successfully\n");
		printf("Done.\n");
		
	} else {
		printf("Project could not be destroyed on server.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	freeSocketBuffer(socketBuffer);
	free(responseCode);
}

// Compession Not required for this step, as no file
// is sent over the network
void addFileToProject(char *project, char *filePath) {
	// This command, just makes the changes to local manifest
	
	// Check if project exists
	if(!isProjectConfiguredLocally(project)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}
	
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	
	// check if given file exists
	sprintf(path, "%s/%s", project, filePath);
	if(!checkFileExists(path)) {
		printf("Error: File does not exist locally in project.\n");
		printf("Unable to find: %s\n", path);
		free(path);
		return;
	}
	
	// check if project directory contains manifest
	sprintf(path, "%s/%s", project, MANIFEST_FILE);
	
	// Make changes to manifest
	int manifestFd = open(path, O_RDONLY, 0777);
	Manifest *manifest = readManifestContents(manifestFd);
	ManifestNode *manifestNode = searchFile(manifest, filePath);
	close(manifestFd);
	
	char buffer[100];
	
	sprintf(path, "%s/%s", project, filePath);
	computeFileHash(path, buffer);
	buffer[HASH_STRING_LEN] = '\0';
	
	char *md5 = strdup(buffer);
	
	// start with version 1.
	char *version = strdup("1");
	
	if(manifestNode == NULL) {		
		addFileToManifest(manifest, md5, version, strdup(filePath));
		
		// Reopen the file, and put changes.
		sprintf(path, "%s/%s", project, MANIFEST_FILE);
		createDirStructureIfNeeded(path);
		manifestFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
		writeManifestToFile(manifest, manifestFd);
		close(manifestFd);
		printf("File added to manifest.\n");
		
	} else {
		printf("File is already present in manifest\n");
	}
	
	
	free(path);
	freeManifest(manifest);
}


// Compession Not required for this step, as no file
// is sent over the network
void removeFileInProject(char *project, char *filePath) {
	// This command, just makes the changes to local manifest
	
	// Check if project exists
	if(!isProjectConfiguredLocally(project)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}
	
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
		
	// check if project directory contains manifest
	sprintf(path, "%s/%s", project, MANIFEST_FILE);
	if(!checkFileExists(path)) {
		printf("Error: Please create project first.\n");
		free(path);
		return;
	}
	
	// Make changes to manifest
	int manifestFd = open(path, O_RDONLY, 0777);
	Manifest *manifest = readManifestContents(manifestFd);
	removeFileFromManifest(manifest, filePath);
	close(manifestFd);
	
	createDirStructureIfNeeded(path);
	manifestFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	writeManifestToFile(manifest, manifestFd);
	close(manifestFd);
	printf("File removed from manifest.\n");
	
	free(path);
	freeManifest(manifest);
}

// Compession Not required for this step, as just single file
// is sent over the network
void getProjectCurrentVersion(char *project, int socket) {
	// Issue command for server to create project.
	// IF successul, server sends back manifest.
	printf("Trying to get current version of Project: %s.\n", project);
	fflush(stdout);
	
	// Make Request.
	// currentversion:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "currentversion", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	// Server responds back
	// sendfile:
	//	<numFiles>:
	//  <ManifestNameLen>:<manifest name><numBytes>:<contents>
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	
	SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {	
		
		// ignore till numBytes
		readTillDelimiter(socketBuffer, socket, ':');
		readTillDelimiter(socketBuffer, socket, ':');
		readTillDelimiter(socketBuffer, socket, ':');
		clearSocketBuffer(socketBuffer);
		
		// First download the server manifest.
		Manifest *serverManifest = readManifestContents(socket);
		
		// Just iterate on manifest and show contents.
		printf("Project: %s\n", serverManifest->projectName);
		printf("Project Version: %s\n", serverManifest->versionNumber);

		printf("Project Files:\n");
		
		ManifestNode *node = serverManifest->head;
		while(node != NULL) {
			printf("\tVersion: %s, File: %s\n", node->version, node->filePath);
			node = node->next;
		}
		
		printf("Done.\n");		
		freeManifest(serverManifest);
		
	} else {
		printf("Could not fetch project version from server.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	freeSocketBuffer(socketBuffer);
	free(responseCode);
}


// Compession Not required for this step, as just single file
// is sent over the network
void getProjectHistory(char *project, int socket) {
	printf("Trying to get history of Project: %s.\n", project);
	fflush(stdout);
	
	// Make Request.
	// history:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "history", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	// Server responds back
	// ok:<historyFileBytes>:<contents>
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	
	SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "ok") == 0) {
		
		readTillDelimiter(socketBuffer, socket, ':');
		char *contentLenStr = readAllBuffer(socketBuffer);
		long contentLen = atol(contentLenStr);
		
		// Now read contentLen chars and display
		while(contentLen-- > 0) {
			char c;
			read(socket, &c, 1);
			printf("%c", c);
		}
		
		free(contentLenStr);
		printf("Done.\n");
		
	} else {
		printf("Could not fetch project history from server.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	freeSocketBuffer(socketBuffer);
	free(responseCode);
}


// Compession Not required for this step, as no file
// is sent over the network
void rollbackProject(char *project, char *version, int socket) {
	printf("Trying to get rollback of Project: %s.\n", project);
	fflush(stdout);
	
	// Make Request.
	// rollback:<projectNameLength>:<projectName><version>:
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "rollback", strlen(project), project);
	write(socket, command, strlen(command));
	write(socket, version, strlen(version));
	write(socket, ":", 1);
	free(command);
	
	// Server responds back
	// ok:
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	
	SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "ok") == 0) {
		printf("Rollback successul\n");
		printf("Done.\n");
		
	} else {
		printf("Could not Rollback project on server.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	freeSocketBuffer(socketBuffer);
	free(responseCode);
}


// Compession Not required for this step, as just single file
// is sent over the network
void createUpdateFile(char *project, int socket) {
	// Issue command for server to create project.
	// IF successul, server sends back manifest.
	printf("Trying to create .update files: %s\n", project);
	fflush(stdout);
	
	// This command, just makes the changes to local manifest
	
	// Check if project exists
	if(!isProjectConfiguredLocally(project)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}
	
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	sprintf(path, "%s/%s", project, MANIFEST_FILE);
	
	
	// Make Request.
	// update:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "update", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	// Server responds back
	// sendfile:<ManifestNameLen>:<manifest name><numBytes>:<contents>
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	
	SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {
		
		// ignore till numBytes
		readTillDelimiter(socketBuffer, socket, ':');
		readTillDelimiter(socketBuffer, socket, ':');
		clearSocketBuffer(socketBuffer);
		
		// First download the server manifest.
		Manifest *serverManifest = readManifestContents(socket);
		
		sprintf(path, "%s/%s", project, MANIFEST_FILE);
		int manifestFd = open(path, O_RDONLY, 0777);
		Manifest *localManifest = readManifestContents(manifestFd);
		close(manifestFd);
		
		// Now compare both manifests.
		printf("Comparing manifests.\n");
		fflush(stdout);
		
		sprintf(path, "%s/%s", project, UPDATE_FILE);
		createDirStructureIfNeeded(path);
		int updateFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
		if(compareManifests(serverManifest, localManifest, updateFd) == -1) {
			printf("Conflicts detected. Please resolve before updating again.\n");
			fflush(stdout);
			close(updateFd);
			unlink(path);
			
		} else {
			// There were no errors. So just output files after reading UPDATE_FILE
			close(updateFd);
			updateFd = open(path, O_RDONLY, 0777);
			
			clearSocketBuffer(socketBuffer);
			
			while(1) {
				readTillDelimiter(socketBuffer, updateFd, ' ');
				char *code = readAllBuffer(socketBuffer);
				if(strlen(code) == 0) {
					free(code);
					break;
				}
				
				// ignore version and liveHash
				readTillDelimiter(socketBuffer, updateFd, ' ');
				readTillDelimiter(socketBuffer, updateFd, ' ');
				clearSocketBuffer(socketBuffer);
				
				readTillDelimiter(socketBuffer, updateFd, '\n');
				char *filePath = readAllBuffer(socketBuffer);
				
				printf("%s %s\n", code, filePath);
				fflush(stdout);
				free(code);
				free(filePath);
			}
			
			close(updateFd);
		}
		
		printf("Done.\n");
		
		freeManifest(serverManifest);
		freeManifest(localManifest);
		
	} else {
		printf("Server sent error message.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	// Now delete the manifest, and its contents
	free(responseCode);
	freeSocketBuffer(socketBuffer);
	free(path);
}

// This step, needs to use the compression by zlib library
void upgradeProject(char *project, int socket) {
	// Issue command for server to upgrade project.
	// IF successul, server sends back manifest.
	printf("Trying to upgrade project: %s\n", project);
	fflush(stdout);
	
	// Check if project exists
	if(!isProjectConfiguredLocally(project)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}
	
	// Check if configured.
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	
	// check if update file present.
	sprintf(path, "%s/%s", project, UPDATE_FILE);
	if(!checkFileExists(path)) {
		printf("Error: Update file does not exist.\n");
		free(path);
		return;
	}
	
	SocketBuffer *socketBuffer = createBuffer();
	
	// First process entries for deleting the files locally
	int updateFd = open(path, O_RDONLY, 0777);
	
	int filesProcessed = 0;
	while(1) {
		readTillDelimiter(socketBuffer, updateFd, ' ');
		char *code = readAllBuffer(socketBuffer);
		if(strlen(code) == 0) {
			free(code);
			break;
		}
		
		// ignore file version and new hash
		readTillDelimiter(socketBuffer, updateFd, ' ');
		readTillDelimiter(socketBuffer, updateFd, ' ');
		clearSocketBuffer(socketBuffer);
		
		readTillDelimiter(socketBuffer, updateFd, '\n');
		char *filePath = readAllBuffer(socketBuffer);
		
		// We now have fileCode and filePath from UPDATE_FILE
		if(strcmp(code, "D") == 0) {
			filesProcessed++;
						
			char *fullPath = malloc(sizeof(char) * (strlen(project) + strlen(filePath) + 50));
			sprintf(fullPath, "%s/%s", project, filePath);
			unlink(fullPath);
			free(fullPath);
		}
		free(code);
		free(filePath);
	}	
	close(updateFd);	
		
		
	// Then send MA entries to server. Server simply ignores entries for D.
	
	// Run the command for upgrade on server.
	// "upgrade:<projectNameLength>:<projectName><UpdateFileNameLen>:<UpdateFileName><numBytes>:<contents>"
	// ..
	// server sends the response:
	// sendfile:
	//		<numFiles>:
	//		<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
	//		<File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	
	// Make Request.
	char *command = malloc(sizeof(char) * (strlen(project) + 50));
	sprintf(command, "%s:%d:%s", "upgrade", strlen(project), project);
	write(socket, command, strlen(command));
	writeFileDetailsToSocket(UPDATE_FILE, project, socket); // defined in util.h
	free(command);
	
	
	// Read response now.	
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {
		// REMEMBER: COMPRESSED ZLIB RESPONSE
		
		char *serverRespPath = malloc(sizeof(char) * (strlen(project) + strlen(RESPONSE_FILE) + 50));
		
		// Now, store N bytes unencrypted into the response file.
		sprintf(serverRespPath, "%s_%lld_%d", RESPONSE_FILE, current_timestamp_millis(), rand());
		convertZlibToResponse(socket, serverRespPath, ".");
		
		int responseFd = open(serverRespPath, O_RDONLY, 0777);
		
		/* Core logic now */	
		// Next download files.
		readTillDelimiter(socketBuffer, responseFd, ':');
		char *numFilesStr = readAllBuffer(socketBuffer);
		long numFiles = atol(numFilesStr);
		free(numFilesStr);
		filesProcessed += numFiles;
		
		// Now read N files, and save them
		while(numFiles-- > 0) {
			writeFileFromSocket(responseFd, project);
		}
		
		// Manifest file always comes from server.
		if(filesProcessed == 1) {
			printf("Project Up-to-date.\n");
		} else {
			printf("%d files updated.\n", filesProcessed);
		}
		/* Core logic ends here */
		
		close(responseFd);
		unlink(serverRespPath);
		free(serverRespPath);
		
	} else {
		printf("Server sent error message.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	printf("Done.\n");
	
	// Once done, delete UPDATE_FILE
	sprintf(path, "%s/%s", project, UPDATE_FILE);
	unlink(path);

	// Now delete the manifest, and its contents
	free(responseCode);
	freeSocketBuffer(socketBuffer);
	free(path);
}


// Compession Not required for this step, as just single file
// is sent over the network
void commitProject(char *project, int socket) {
	printf("Trying to commit project: %s\n", project);
	fflush(stdout);
	
	// Check if project exists
	if(!isProjectConfiguredLocally(project)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}
	
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	
	// check if update file present and not empty
	sprintf(path, "%s/%s", project, UPDATE_FILE);
	if(checkFileExists(path) && findFileSize(path) != 0) {
		printf("Error: Update file exists.\n");
		free(path);
		return;
	}
	
	// Make Request.
	// commit:<projectNameLength>:<projectName>
	char *command = malloc(sizeof(char) * (strlen(project) + 50));;
	sprintf(command, "%s:%d:%s", "commit", strlen(project), project);
	write(socket, command, strlen(command));
	free(command);
	
	// Server responds back
	// sendfile:<ManifestNameLen>:<manifest name><numBytes>:<contents>
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {
		// ignore till numBytes
		readTillDelimiter(socketBuffer, socket, ':');
		readTillDelimiter(socketBuffer, socket, ':');
		clearSocketBuffer(socketBuffer);
		
		// First download the server manifest.
		Manifest *serverManifest = readManifestContents(socket);
		
		Manifest* clientManifest = readClientProjectManifest(project);
			
		if(strcmp(clientManifest->versionNumber, serverManifest->versionNumber) != 0) {
			printf("Error: Manifest version mismatch. please update project first.\n");
			fflush(stdout);
			
			free(responseCode);
			freeSocketBuffer(socketBuffer);
			freeManifest(clientManifest);
			freeManifest(serverManifest);
			free(path);
			return;
		}
		
		// Open .COMMIT_FILE
		sprintf(path, "%s/%s", project, COMMIT_FILE);	
		createDirStructureIfNeeded(path);
		int commitFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);	
		
		char liveHash[100];
		int error = 0;
		
		ManifestNode* serverFileNode;
		ManifestNode *clientFileNode;
		
		clientFileNode = clientManifest->head;
		while(clientFileNode != NULL && error == 0) {
			
			// find live hash of file.
			char *filepath = malloc(sizeof(char) * (strlen(project) + strlen(clientFileNode->filePath) + 50));
			sprintf(filepath, "%s/%s", project, clientFileNode->filePath);
			computeFileHash(filepath, liveHash);
			free(filepath);
			liveHash[HASH_STRING_LEN] = '\0';
			
			// Search client file in Server
			serverFileNode = searchFile(serverManifest, clientFileNode->filePath);
			
			// files that need to be added.
			if(serverFileNode == NULL) {
				
				write(commitFd, "A", 1);
				write(commitFd, " ", 1);
				write(commitFd, "1", 1);
				write(commitFd, " ", 1);
				write(commitFd, liveHash, strlen(liveHash));
				write(commitFd, " ", 1);
				write(commitFd, clientFileNode->filePath, strlen(clientFileNode->filePath));
				write(commitFd, "\n", 1);
				
			} else {
				
				// check for conflict case.
				// if server has different hashcode, and has higher version.
				if(strcmp(serverFileNode->md5, clientFileNode->md5) != 0) {
					int clientVersion = atoi(clientFileNode->version);
					int serverVersion = atoi(serverFileNode->version);
					
					if(serverVersion > clientVersion) {
						error = 1; // Client must sync first.
					}
				}
				
				if((strcmp(clientFileNode->md5, liveHash) != 0) && (error != 1)) {
				
					// File is in both, but client's version is better
					// Add entry to commit file with newer version
					// increment current version
					int currentVersion = atoi(clientFileNode->version);
					char version[20];
					sprintf(version, "%d", currentVersion + 1);
					
					write(commitFd, "U", 1);
					write(commitFd, " ", 1);
					write(commitFd, version, strlen(version));
					write(commitFd, " ", 1);
					write(commitFd, liveHash, strlen(liveHash));
					write(commitFd, " ", 1);
					write(commitFd, clientFileNode->filePath, strlen(clientFileNode->filePath));
					write(commitFd, "\n", 1);
				}
			}	
			
			clientFileNode = clientFileNode->next;
		}
		
		// check for deleted files.
		serverFileNode = serverManifest->head;
		while(serverFileNode != NULL && error == 0) {
			
			// Search client file in Server
			clientFileNode = searchFile(clientManifest, serverFileNode->filePath);
			
			// client has deleted this file.
			if(clientFileNode == NULL) {
				write(commitFd, "D", 1);
				write(commitFd, " ", 1);
				write(commitFd, serverFileNode->version, strlen(serverFileNode->version));
				write(commitFd, " ", 1);
				write(commitFd, serverFileNode->md5, strlen(serverFileNode->md5));
				write(commitFd, " ", 1);
				write(commitFd, serverFileNode->filePath, strlen(serverFileNode->filePath));
				write(commitFd, "\n", 1);				
			}
			
			serverFileNode = serverFileNode->next;
		}
		
		close(commitFd);
		
		// Now .commit file is ready, 	
		// Check if .commit is valid, 
		if(error != 0) {
			printf("Please synch with repository before committing the changes.\n");
			sprintf(path, "%s/%s", project, COMMIT_FILE);	
			unlink(path);
		} else {
			
			// Our .commit file is ready and correct.
			// we should ship it to server now.
			
			// Make Request.
			// commitfile:<projectNameLength>:<projectName>
			//		<numFiles>:
			// 	<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
			//		<File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
			// Though, we just ship a .commit file in this case.
			command = malloc(sizeof(char) * (strlen(project) + 50));;
			sprintf(command, "%s:%d:%s", "commitfile", strlen(project), project);
			write(socket, command, strlen(command));
			write(socket, "1:", 2);
			writeFileDetailsToSocket(COMMIT_FILE, project, socket);
			free(command);

			
			// Again check the response from server.
			// If server fails, We need to delete COMMIT_FILE and show error to user.
			char c;
			read(socket, &c, 1);
			
			if(c == '1') {
				// All good.
				printf("%s pushed to server successully\n", COMMIT_FILE);
			} else {
				printf("Error while pushing %s file to server.\n", COMMIT_FILE);
				sprintf(path, "%s/%s", project, COMMIT_FILE);	
				unlink(path);
			}
			
		}
		
		printf("Done.\n");
		
		freeManifest(serverManifest);
		freeManifest(clientManifest);
		
	} else {
		printf("Server sent error message.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	free(responseCode);
	freeSocketBuffer(socketBuffer);
	free(path);
}

// This step required ZLIB compression.
void pushProject(char *project, int socket) {
	printf("Trying to push project: %s\n", project);
	fflush(stdout);
	
	// Check if project exists
	if(!isProjectConfiguredLocally(project)) {
		printf("Error: Please configure the project correctly.\n");
		return;
	}
	
	char *path = malloc(sizeof(char) * (strlen(project) + 50));
	
	// check if .COMMIT_FILE file exists.
	sprintf(path, "%s/%s", project, COMMIT_FILE);
	if(!checkFileExists(path)) {
		printf("Error: .Commit file does not exist.\n");
		free(path);
		return;
	}
	
	SocketBuffer *socketBuffer = createBuffer();
	
	// check if update file present and not empty
	sprintf(path, "%s/%s", project, UPDATE_FILE);
	if(checkFileExists(path) && findFileSize(path) != 0) {
		
		// check if update file has any Modify Codes.
		int updateFd = open(path, O_RDONLY, 0777);
		
		while(1) {
			readTillDelimiter(socketBuffer, updateFd, ' ');
			char *code = readAllBuffer(socketBuffer);
			if(strlen(code) == 0) {
				free(code);
				break;
			}
			
			// ignore file version, hash and path		
			readTillDelimiter(socketBuffer, updateFd, '\n');
			clearSocketBuffer(socketBuffer);
			
			// We now have fileCode from UPDATE_FILE
			if(strcmp(code, "M") == 0) {
				printf("Error: Update file has some files pending for modification\n");
				free(code);
				free(path);
				close(updateFd);
				freeSocketBuffer(socketBuffer);
				return;
			}
			free(code);
		}
		close(updateFd);
	}
	
	// We are now ready to send our files.
	typedef struct FileNode {
		char *filePath;
		struct FileNode *next;
	} FileNode;
	
	FileNode *listOfFiles = NULL;
	int numFiles = 0;
	
	// Read how many files from .Commit file need to be shipped..
	// Only A and U types.
	// Though, .commit will have entries for all of them.
	// But, we will just ship, A or U files.

	sprintf(path, "%s/%s", project, COMMIT_FILE);
	int commitFd = open(path, O_RDONLY, 0777);
	
	while(1) {
		readTillDelimiter(socketBuffer, commitFd, ' ');
		char *code = readAllBuffer(socketBuffer);
		if(strlen(code) == 0) {
			free(code);
			break;
		}
		
		// ignore file version, hash	
		readTillDelimiter(socketBuffer, commitFd, ' ');
		readTillDelimiter(socketBuffer, commitFd, ' ');
		clearSocketBuffer(socketBuffer);
		
		// read filePath
		readTillDelimiter(socketBuffer, commitFd, '\n');
		char *fPath = readAllBuffer(socketBuffer);
		
		// If it is a A or U file.
		if(strcmp(code, "D") != 0) {
			FileNode *tmp = malloc(sizeof(FileNode));
			tmp->next = listOfFiles;
			listOfFiles = tmp;
			tmp->filePath = strdup(fPath);
			numFiles++;
		}
		free(code);
		free(fPath);
	}
	close(commitFd);
	

	// We now have required files to be sent to the server.
	// We can start making the command now.	
	
	// Make Request.
	// pushfiles:<projectNameLength>:<projectName><compressed data>
	// 
	// compressed data format:
	//		<numFiles>:
	//		<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
	//		<File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
	char buffer[100];
	char *command = malloc(sizeof(char) * (strlen(project) + 50));
	sprintf(command, "%s:", "pushfiles");
	write(socket, command, strlen(command));
	sprintf(command, "%d:%s", strlen(project), project);
	write(socket, command, strlen(command));
	
	// REMEMBER: COMPRESSED ZLIB RESPONSE
	////////////////////////////////////////////////////
	// Compression is required now, 
	
	char *clientReqPath = malloc(sizeof(char) * (strlen(project) + strlen(RESPONSE_FILE) + 50));
	
	// Now, store N bytes encrypted into the response file.
	sprintf(clientReqPath, "%s_%lld_%d", REQUEST_FILE, current_timestamp_millis(), rand());
	
	int requestFd = open(clientReqPath, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			
	/* Core logic for compression starts now */	
	sprintf(buffer, "%d:", (numFiles + 1)); // +1 for commit file.
	write(requestFd, buffer, strlen(buffer));
	free(command);
	
	// Now write commit file.
	writeFileDetailsToSocket(COMMIT_FILE, project, requestFd);
	
	// Now write the A or U files:
	FileNode *start = listOfFiles;
	while(start != NULL) {
		FileNode *curr = start;
		writeFileDetailsToSocket(curr->filePath, project, requestFd);
		start = start->next;
		free(curr->filePath);
		free(curr);
	}
	
	close(requestFd);
	
	convertResponseToZlib(socket, clientReqPath, ".");
	unlink(clientReqPath);
	free(clientReqPath);
	
	////////////////////////////////////////////////////
	// Compression is done now, 
	
	
	
	// Server responds back
	// sendfile:<ManifestNameLen>:<manifest name><numBytes>:<contents>
	// ..
	// In case of error, Response comes as "failed:<fail Reason>:"
	clearSocketBuffer(socketBuffer);
	readTillDelimiter(socketBuffer, socket, ':');
	char *responseCode = readAllBuffer(socketBuffer);
	
	if(strcmp(responseCode, "sendfile") == 0) {
		// ignore till numBytes
		readTillDelimiter(socketBuffer, socket, ':');
		readTillDelimiter(socketBuffer, socket, ':');
		clearSocketBuffer(socketBuffer);
		
		// First download the server manifest.
		Manifest *serverManifest = readManifestContents(socket);
		
		// We need to write the server's manifest now into local
		// So that versions are in synch now.
		sprintf(path, "%s/%s", project, MANIFEST_FILE);
		createDirStructureIfNeeded(path);
		int clientManifestFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
		writeManifestToFile(serverManifest, clientManifestFd);
		close(clientManifestFd);
		
		printf("Done.\n");
		
		freeManifest(serverManifest);
		
		// remove commit file.
		sprintf(path, "%s/%s", project, COMMIT_FILE);
		unlink(path);
	} else {
		printf("Server sent error message.\n");		
		readTillDelimiter(socketBuffer, socket, ':');
		char *reason = readAllBuffer(socketBuffer);
		printf("ResponseCode: %s\n", responseCode);
		printf("Reason: %s\n", reason);
		free(reason);
	}
	
	free(responseCode);
	freeSocketBuffer(socketBuffer);
	free(path);
}


int main(int argc, char *argv[]) {
	srand(current_timestamp());
	if(argc < 2) {
		printf("Error: Mention some command.\n");
		return 0;
	}
	
	if(strcmp(argv[1], "configure") == 0) {
		if(argc < 4) {
			printf("Error: Params missing\n");
		} else {
			
			int fd = open(CONFIG_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0777);
			write(fd, argv[2], strlen(argv[2]));
			write(fd, " ", 1);
			write(fd, argv[3], strlen(argv[3]));
			write(fd, " ", 1);
			close(fd);
			printf("Done!\n");
		}
		return 0;
	}
	
	if(!checkFileExists(CONFIG_FILE)) {
		printf("Error: Run configure command first.\n");
		return 0;
	}
	
	// Now retrieve the ip and port from Config file.
	int fd = open(CONFIG_FILE, O_RDONLY);
	SocketBuffer *socketBuffer = createBuffer();
	readTillDelimiter(socketBuffer, fd, ' ');
	char *ipAddress = readAllBuffer(socketBuffer);
	readTillDelimiter(socketBuffer, fd, ' ');
	char *port = readAllBuffer(socketBuffer);
	freeSocketBuffer(socketBuffer);
	close(fd);
	
	// We got IP and PORT from file.
    struct addrinfo* results = get_sockaddr(ipAddress, port);
    int sockfd = open_connection(results);

	// Socket, always wait till the time, we close the connection.
	
	// Now check the commands.
	if(strcmp(argv[1], "checkout") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			checkoutProject(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "update") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			createUpdateFile(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "upgrade") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			upgradeProject(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "commit") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			commitProject(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "push") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			pushProject(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "create") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			createProject(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "destroy") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			destroyProject(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "add") == 0) {
		if(argc < 4) {
			printf("Error: Params missing\n");
		} else {
			addFileToProject(argv[2], argv[3]);
		}
		
	} else if(strcmp(argv[1], "remove") == 0) {
		if(argc < 4) {
			printf("Error: Params missing\n");
		} else {
			removeFileInProject(argv[2], argv[3]);
		}
		
	} else if(strcmp(argv[1], "currentversion") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			getProjectCurrentVersion(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "history") == 0) {
		if(argc < 3) {
			printf("Error: Params missing\n");
		} else {
			getProjectHistory(argv[2], sockfd);
		}
		
	} else if(strcmp(argv[1], "rollback") == 0) {
		if(argc < 4) {
			printf("Error: Params missing\n");
		} else {
			rollbackProject(argv[2], argv[3], sockfd);
		}		
	} else {
		printf("Invalid command. Please check.\n");
	}
	
	free(ipAddress);
	free(port);
	close(sockfd);
    
    return 0;
}