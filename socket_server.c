#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "util.h"
#include "socketBuffer.h"
#include "manifest.h"
#include "compressor.h"

char client_message[MAX_MSG_SIZE];
char buffer[MAX_MSG_SIZE];

char BASE_DIRECTORY[] = "./server_repo";
char VERSION_FILE[] = ".version";
char UPDATE_FILE[] = ".update";
char COMMIT_FILE[] = ".commit";
char HISTORY_FILE[] = ".history";

char *REQUEST_FILE = ".request";
char *RESPONSE_FILE = ".response";

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int checkProject(char *projectName) {
	if(!checkDirectoryExists(BASE_DIRECTORY)) {
		return 0;
	}
	char *path = malloc(sizeof(char) * (strlen(BASE_DIRECTORY) + strlen(projectName) + 5));
	sprintf(path, "%s/%s", BASE_DIRECTORY, projectName);
	int result = checkDirectoryExists(path);
	free(path);
	return result;
}

// assuming project exists.
char *readCurrentVersion(char *projectName) {
	char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(VERSION_FILE) + 40 + strlen(BASE_DIRECTORY)));
	sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, VERSION_FILE);
	
	char *result = readFileContents(path);
	free(path);
	return result;
}

// Thie method writes a file in below format To a socket
// <FileNameLen>:<FileName><FileLenBytes>:<FileContents>
// Precodition: Project exists.
// It adds the required version prefix.
void writeFileToSocket(int sockfd, char *projectName, const char *filePath) {
	
	// Check first if project exists.
	printf("Writing File %s in project: %s to client.\n", filePath, projectName);
	char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(filePath) + 50 + strlen(BASE_DIRECTORY)));
	
	// Read current version of project.
	char *version = readCurrentVersion(projectName);

	// create file path on server.
	sprintf(path, "%s/%s/%s/%s", BASE_DIRECTORY, projectName, version, filePath);
	free(version);
	
	// Check if file exists.
	if(!checkFileExists(path)) {
		printf("File do not exist: %s\n", path);
	} else {
		
		long fileSize = findFileSize(path);				
		int fileFd = open(path, O_RDONLY, 0777);
		
		sprintf(path, "%d:%s%ld:", strlen(filePath), filePath, fileSize);
		write(sockfd, path, strlen(path));
		
		// Now write file char by char on the socket	
		char c;
		while (read(fileFd, &c, 1) == 1) {
			write(sockfd, &c, 1);
		}
		
		close(fileFd);
	}

	free(path);
}

// Precodition: project exists.
Manifest *readCurrentSeverManifest(char *projectName) {
	
	char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(MANIFEST_FILE) + 50 + strlen(BASE_DIRECTORY)));
	
	// Read current version of project.
	char *version = readCurrentVersion(projectName);

	// create file path on server.
	sprintf(path, "%s/%s/%s/%s", BASE_DIRECTORY, projectName, version, MANIFEST_FILE);
	
	int manifestFd = open(path, O_RDONLY, 0777);
	
	Manifest *result = readManifestContents(manifestFd);
	
	close(manifestFd);
	free(version);
	free(path);
	
	return result;
}

void appendToHistoryFile(char *projectName, char *data) {
	
	char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(HISTORY_FILE) + 50 + strlen(BASE_DIRECTORY)));
	
	// create history file path
	sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, HISTORY_FILE);
	createDirStructureIfNeeded(path);
	int hfd = open(path, O_RDWR | O_APPEND, 0777);
	write(hfd, data, strlen(data));
	close(hfd);
	
	free(path);
}

void pushFileToHistory(char *projectName, char *fPath) {
	
	char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(HISTORY_FILE) + 50 + strlen(BASE_DIRECTORY)));
	
	// create history file path
	sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, HISTORY_FILE);
	createDirStructureIfNeeded(path);
	
	int hfd = open(path, O_RDWR | O_APPEND, 0777);	
	int readFd = open(fPath, O_RDONLY, 0777);
	
	write(hfd, "push\n", strlen("push\n"));
	char *cVersion = readCurrentVersion(projectName);
	write(hfd, cVersion, strlen(cVersion));
	write(hfd, "\n", 1);
	free(cVersion);
	
	// Now write file char by char on the socket	
	char c;
	while (read(readFd, &c, 1) == 1) {
		write(hfd, &c, 1);
	}
	write(hfd, "\n", 1);
	
	close(hfd);
	close(readFd);
	
	free(path);
}

void createProject(int sockfd, char *projectName) {
	printf("Creating project: %s.\n", projectName);
	char *path = malloc(sizeof(char) * (strlen(projectName) + 50 + strlen(BASE_DIRECTORY)));
	
	sprintf(path, "%s/%s", BASE_DIRECTORY, projectName);
	
	// We can create the project.
	createDirStructureIfNeeded(path);
	createDirectory(path);
	
	char version[10] = "1";
	
	// create version file
	sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, VERSION_FILE);
	createDirStructureIfNeeded(path);
	int vfd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);	
	write(vfd, version, strlen(version));
	close(vfd);
	
	// create history file 
	sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, HISTORY_FILE);
	createDirStructureIfNeeded(path);
	int hfd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	close(hfd);
	
	// create version directory.
	sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, version);
	createDirectory(path);
	
	// Create manifest inside version directory.
	sprintf(path, "%s/%s/%s/%s", BASE_DIRECTORY, projectName, version, MANIFEST_FILE);
	createDirStructureIfNeeded(path);
	int manifestFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);	
	
	write(manifestFd, projectName, strlen(projectName));
	write(manifestFd, "\n", 1);
	write(manifestFd, version, strlen(version));
	write(manifestFd, "\n", 1);
	write(manifestFd, "0", 1);  // there are no files in start 
	write(manifestFd, "\n", 1);
	close(manifestFd);
	
	free(path);
}

void writeErrorToSocket(int sockfd, char *error) {
	write(sockfd, "failed:", strlen("failed:"));
	write(sockfd, error, strlen(error));
	write(sockfd, ":", 1);
}

void processCommand(int sockfd) {
	char buffer[1000];
	
	SocketBuffer *socketBuffer = createBuffer();
	
	readTillDelimiter(socketBuffer, sockfd, ':');
	char *command = readAllBuffer(socketBuffer);
	
	// If client's command is empty, then just terminate
	if(strlen(command) == 0) {
		free(command);
		freeSocketBuffer(socketBuffer);
		return;
	}
	
	printf("Client issued command: %s\n", command);
	
	if(strcmp(command, "checkout") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
			
			Manifest *serverManifest = readCurrentSeverManifest(projectName);
			write(sockfd, "sendfile:", strlen("sendfile:"));
			
			////////////////////////////////////////////////////
			// Compression is required now, 
			
			char *serverRespPath = malloc(sizeof(char) * (strlen(BASE_DIRECTORY) + strlen(projectName) + 50));		
			sprintf(serverRespPath, "%s/%s/%s%lld_%d", BASE_DIRECTORY, projectName, RESPONSE_FILE, current_timestamp_millis(), rand());
			
			int responseFd = open(serverRespPath, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			
			// Add 1 for MANIFEST_FILE
			sprintf(buffer, "%d:", 1 + serverManifest->numFiles);
			write(responseFd, buffer, strlen(buffer));
			
			writeFileToSocket(responseFd, projectName, MANIFEST_FILE);
			
			ManifestNode *node = serverManifest->head;
			while(node != NULL) {
				writeFileToSocket(responseFd, projectName, node->filePath);
				node = node->next;
			}
			close(responseFd);
						
			convertResponseToZlib(sockfd, serverRespPath, BASE_DIRECTORY);
			
			unlink(serverRespPath);
			free(serverRespPath);
			
			////////////////////////////////////////////////////
			// Compression is done now, 
			
			freeManifest(serverManifest);
		}
		
		free(nameLen);
		free(projectName);
		
	} else if(strcmp(command, "create") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project Already exists.");
			
		} else {
			createProject(sockfd, projectName);
			write(sockfd, "sendfile:", strlen("sendfile:"));
			write(sockfd, "1:", 2);
			writeFileToSocket(sockfd, projectName, MANIFEST_FILE);
			
			char *hbuffer = malloc(sizeof(char) * (strlen(projectName) + 50));
			sprintf(hbuffer, "Project created: %s\n\n", projectName);
			appendToHistoryFile(projectName, hbuffer);
			free(hbuffer);
		}
		
		free(nameLen);
		free(projectName);
		
	} else if(strcmp(command, "currentversion") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
			// Just send manifest back.
			write(sockfd, "sendfile:", strlen("sendfile:"));
			write(sockfd, "1:", 2);
			writeFileToSocket(sockfd, projectName, MANIFEST_FILE);
		}
		
		free(nameLen);
		free(projectName);
		
	} else if(strcmp(command, "history") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
		} else {
			
			// TODO: Write the history file, and its size
			// which got created while pushing project.	
			write(sockfd, "ok:", strlen("ok:"));
			
			char *historyPath = malloc(sizeof(char) * (strlen(projectName) + strlen(HISTORY_FILE) + 50 + strlen(BASE_DIRECTORY)));
			
			sprintf(historyPath, "%s/%s/%s", BASE_DIRECTORY, projectName, HISTORY_FILE);
			
			long hfsize = findFileSize(historyPath);
			sprintf(buffer, "%ld:", hfsize);
			write(sockfd, buffer, strlen(buffer));
			
			// Now write history file contents.						
			int fileFd = open(historyPath, O_RDONLY, 0777);
			char c;
			while (read(fileFd, &c, 1) == 1) {
				write(sockfd, &c, 1);
			}
			close(fileFd);
		}
		
		free(nameLen);
		free(projectName);
		
	}  else if(strcmp(command, "destroy") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
		} else {
			char *path = malloc(sizeof(char) * (strlen(projectName) + 50 + strlen(BASE_DIRECTORY)));
			sprintf(path, "%s/%s", BASE_DIRECTORY, projectName);
			removeDirectoryCompletely(path);
			free(path);
			
			write(sockfd, "ok:", strlen("ok:"));
		}
		
		free(nameLen);
		free(projectName);
		
	} else if(strcmp(command, "rollback") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
			
			char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(BASE_DIRECTORY) + 50));
			
			readTillDelimiter(socketBuffer, sockfd, ':');
			char *version = readAllBuffer(socketBuffer);
			char *currVersion = readCurrentVersion(projectName);
			
			// check the version, if valid.	
			sprintf(path, "%s/%s/%s.zlib", BASE_DIRECTORY, projectName, version);
			
			if(strcmp(version, currVersion) == 0) {
				writeErrorToSocket(sockfd, "Project already on provided Version.");
			} else if(!checkFileExists(path)) {
				writeErrorToSocket(sockfd, "Invalid Version.");
			} else {
				
				printf("Server rollback requested for version %s\n", version);
				
				// Now we need to delete all the directories, which are
				// having better version.
				
				sprintf(path, "%s/%s", BASE_DIRECTORY, projectName);
				DIR *dir = opendir(path);

				struct dirent *entry = readdir(dir);
				
				// iterate on directory and remove higher versions.
				while (entry != NULL) {
					
					char *fName = entry->d_name;
					
					// The compressed directories are in format
					// <version>.zlib
					if((strstr(fName, "zlib") != NULL) 
						&& (entry->d_type == DT_REG)) {
							
						char name[50];
						strncpy(name, fName, strlen(fName) - 5);
						
						int v = atoi(version);
						int zlibVersion = atoi(name);
						
						if(zlibVersion > v) {
							sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, entry->d_name);
							unlink(path);
						}
					}

					entry = readdir(dir);
				}

				closedir(dir);
				
				// Now, delete the project current version directory.
				sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, currVersion);
				removeDirectoryCompletely(path);
				
				// Now, uncompress the zlib for the request version.
				sprintf(path, "%s/%s/%s.zlib", BASE_DIRECTORY, projectName, version);
				char *uncompressZlibPath = malloc(sizeof(char) * (strlen(projectName) + strlen(BASE_DIRECTORY) + 50));
				sprintf(uncompressZlibPath, "%s/%s/%s.zlib_tmp", BASE_DIRECTORY, projectName, version);
				
				decompressFile(path, uncompressZlibPath);
				
				// Now, reCreate the files from uncompressed zlib
				int oldVersionZlibFd = open(uncompressZlibPath, O_RDONLY, 0777);
	
				readTillDelimiter(socketBuffer, oldVersionZlibFd, ':');
				char *numFilesStr = readAllBuffer(socketBuffer);
				long numFiles = atol(numFilesStr);
				
				sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, version);	
				while(numFiles-- > 0) {
					writeFileFromSocket(oldVersionZlibFd, path);
				}
				free(numFilesStr);
				close(oldVersionZlibFd);
				
				// Now delete the zlib_tmp and zlib file.
				unlink(uncompressZlibPath);
				sprintf(path, "%s/%s/%s.zlib", BASE_DIRECTORY, projectName, version);
				unlink(path);
				
				free(uncompressZlibPath);
				
				//////////////////////////////////////////////////
				
				char hbuffer[100];
				sprintf(hbuffer, "Project rolled back to version: %s\n\n", version);
				appendToHistoryFile(projectName, hbuffer);
					
				// Write new version in .version file
				sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, VERSION_FILE);
				createDirStructureIfNeeded(path);
				int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
				write(fd, version, strlen(version));
				close(fd);
				
				// Return response to client.
				write(sockfd, "ok:", strlen("ok:"));
			}
			
			free(version);
			free(currVersion);
			free(path);
		}
		
		free(nameLen);
		free(projectName);
		
	} else if(strcmp(command, "update") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
			// Just send manifest back.
			write(sockfd, "sendfile:", strlen("sendfile:"));
			writeFileToSocket(sockfd, projectName, MANIFEST_FILE);
		}
		
		free(nameLen);
		free(projectName);
		
	} else if(strcmp(command, "upgrade") == 0) {
		
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
			
			// First read the .update file now.
			
			// We simply create the .update file locally on server.
			char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(BASE_DIRECTORY) + 50));
			
			// We pass the base directory path, inside which file need to be created.
			sprintf(path, "%s/%s", BASE_DIRECTORY, projectName);
			writeFileFromSocket(sockfd, path);
			
			// Now read the update file.
			sprintf(path, "%s/%s/%s", BASE_DIRECTORY, projectName, UPDATE_FILE);
			
			typedef struct FileNode {
				char *filePath;
				char *code;
				struct FileNode *next;
			} FileNode;
			
			FileNode *listOfFiles = NULL;
			int numFiles = 0;
			
			int updateFd = open(path, O_RDONLY, 0777);
			
			// create a linkedlist and read into that..
			while(1) {
				readTillDelimiter(socketBuffer, updateFd, ' ');
				char *code = readAllBuffer(socketBuffer);
				if(strlen(code) == 0) {
					free(code);
					break;
				}
				
				FileNode *tmp = malloc(sizeof(FileNode));
				tmp->code = code;
				
				// ignore file version and new hash
				readTillDelimiter(socketBuffer, updateFd, ' ');
				readTillDelimiter(socketBuffer, updateFd, ' ');
				clearSocketBuffer(socketBuffer);
				
				readTillDelimiter(socketBuffer, updateFd, '\n');
				tmp->filePath = readAllBuffer(socketBuffer);
				tmp->next = listOfFiles;
				listOfFiles = tmp;
				
				// If it is not a delete entry, then count.
				if(strcmp(code, "D") != 0) {
					numFiles++;
				}
			}
			
			close(updateFd);
			
			// Give response code.
			sprintf(buffer, "%s:", "sendfile");
			write(sockfd, buffer, strlen(buffer));
			
			////////////////////////////////////////////////////
			// Compression is required now, 
			
			char *serverRespPath = malloc(sizeof(char) * (strlen(BASE_DIRECTORY) + 50));		
			sprintf(serverRespPath, "%s/%s%lld_%d", BASE_DIRECTORY, RESPONSE_FILE, current_timestamp_millis(), rand());
			
			int responseFd = open(serverRespPath, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			
			sprintf(buffer, "%d:", numFiles + 1);
			write(responseFd, buffer, strlen(buffer));
			
			// Now write all files, 
			// first write manifest.
			writeFileToSocket(responseFd, projectName, MANIFEST_FILE);
						
			// Now write the A or U files, and ignore D files.
			FileNode *start = listOfFiles;
			while(start != NULL) {
				FileNode *curr = start;
				start = start->next;
				
				if(strcmp(curr->code, "D") != 0) {
					writeFileToSocket(responseFd, projectName, curr->filePath);
				}
				
				free(curr->filePath);
				free(curr->code);
				free(curr);
			}
			
			close(responseFd);
			
			convertResponseToZlib(sockfd, serverRespPath, BASE_DIRECTORY);
			
			unlink(serverRespPath);
			free(serverRespPath);
			
			////////////////////////////////////////////////////
			// Compression is done now, 
			
			// delete the update file which we created locally
			unlink(path);
			
			free(path);
		}
		
		free(nameLen);
		free(projectName);
		
	}  else if(strcmp(command, "commit") == 0) {
	
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
			// Just send manifest back.
			write(sockfd, "sendfile:", strlen("sendfile:"));
			writeFileToSocket(sockfd, projectName, MANIFEST_FILE);
		}
		
		free(nameLen);
		free(projectName);
		
	}  else if(strcmp(command, "commitfile") == 0) {
	
		// Clinet uses: "commitfile:<projectNameLength>:<projectName>1:7:.Commit:<size>:<contents>"
		// for commiting
		
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			write(sockfd, "0", 1); // Send failure.
			
		} else {
			
			// ignore number of files.
			readTillDelimiter(socketBuffer, sockfd, ':');
			clearSocketBuffer(socketBuffer);
			
			// We are just doing the commit.
			// So take the current timestamp, and append it to 
			// the "Commit"
						
			readTillDelimiter(socketBuffer, sockfd, ':');
			char *nameLenStr = readAllBuffer(socketBuffer);
			long nameLen = atol(nameLenStr);
			
			readNBytes(socketBuffer, sockfd, nameLen);
			clearSocketBuffer(socketBuffer);
			
			readTillDelimiter(socketBuffer, sockfd, ':');
			char *contentLenStr = readAllBuffer(socketBuffer);
			long contentLen = atol(contentLenStr);
			
			// create a .commit in project directory with name
			// Commit<timestamp>
			char *fullpath = malloc(sizeof(char) * (strlen(COMMIT_FILE) + 50 + strlen(BASE_DIRECTORY) + strlen(projectName)));
			sprintf(fullpath, "%s/%s/%s%lld", BASE_DIRECTORY, projectName, COMMIT_FILE, current_timestamp());
			
			// Write data to the file now.
			createDirStructureIfNeeded(fullpath);
			int fd = open(fullpath, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			char c;
			long i = 0;
			while(i++ < contentLen) {
				read(sockfd, &c, 1);
				if(c == '\0') {
					break; // Socket disconnected.
				}
				write(fd, &c, 1);
			}
			close(fd);	
			
			write(sockfd, "1", 1); // Send success.
			
			free(nameLenStr);
			free(contentLenStr);
			free(fullpath);
		}
		
		free(nameLen);
		free(projectName);
		
		
		
	} else if(strcmp(command, "pushfiles") == 0) {
		// Client uses:
		// pushfiles:<compressed data>
		// 
		// compressed data format:
		// <projectNameLength>:<projectName>
		//		<numFiles>:
		//		<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
		//		<File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
		//
		// Here, the first file is a .COMMIT_FILE		
		
		// REMEMBER, this is a COMPRESSED response sent by client.
		
		readTillDelimiter(socketBuffer, sockfd, ':');
		char *nameLen = readAllBuffer(socketBuffer);
		int projNameLen = atoi(nameLen);
		
		readNBytes(socketBuffer, sockfd, projNameLen);
		char *projectName = readAllBuffer(socketBuffer);
		
		if(!checkProject(projectName)) {
			writeErrorToSocket(sockfd, "Project does not exist.");
			
		} else {
				
			char *clientReqPath = malloc(sizeof(char) * (strlen(projectName) + strlen(BASE_DIRECTORY) + 50));
			
			// Now, store N bytes unencrypted into the response file.
			sprintf(clientReqPath, "%s/%s/%s%lld_%d", BASE_DIRECTORY, projectName, REQUEST_FILE, current_timestamp_millis(), rand());
			convertZlibToResponse(sockfd, clientReqPath, BASE_DIRECTORY);
			
			int requestFd = open(clientReqPath, O_RDONLY, 0777);
			
			/* Decompression starts here. */
			
			// We simply create the .update file locally on server.
			char *path = malloc(sizeof(char) * (strlen(projectName) + strlen(BASE_DIRECTORY) + 50));
			
			char *currentVersionStr = readCurrentVersion(projectName);
			int newVersion = 1 + atoi(currentVersionStr);
			
			// We will create a dummy directory with greater version.
			// If any error, we will change the 
			sprintf(path, "%s/%s", BASE_DIRECTORY, projectName);
			
			char *projDir = strdup(path);
			
			sprintf(path, "%s/%s/%d", BASE_DIRECTORY, projectName, newVersion);
			createDirectory(path);
			
			// Now download everything in this new directory, whichever files client 
			// sends.
			readTillDelimiter(socketBuffer, requestFd, ':');
			char *numFilesStr = readAllBuffer(socketBuffer);
			int numFiles = atoi(numFilesStr);
			
			// only A or U files will be sent by client.
			// D files will be present in .Manifest.
			while(numFiles-- > 0) {
				writeFileFromSocket(requestFd, path); // create required files in new directory.
			}
			
			// Now, we need to see if the .commit file matches with our copy
			sprintf(path, "%s/%s/%d/%s", BASE_DIRECTORY, projectName, newVersion, COMMIT_FILE);
			int status = checkForFileMatch(path, projDir, COMMIT_FILE);
			
			// Decompression code ends here.
			close(requestFd);
			unlink(clientReqPath);
			free(clientReqPath);
			
			
			if(status == 0) {
				// We could not find the matching .COMMIT_FILE 
				writeErrorToSocket(sockfd, "No matching commit file found on server.");
				
				// remove the newly created directory.
				sprintf(path, "%s/%s/%d", BASE_DIRECTORY, projectName, newVersion);
				removeDirectoryCompletely(path);
				
				
			} else {
				// remove all pending commits.
				deleteFilesWithPrefix(projDir, COMMIT_FILE);
				
				// Now, we need to copy all the files from old version dir
				// To new version directory.
				// Except the files, which have been modified.
				
				// Read server manifest from current version.
				Manifest *serverManifest = readCurrentSeverManifest(projectName);
				
				// Copy all files from manifest to new directory, Do not overwrite.
				ManifestNode *node = serverManifest->head;
				while(node != NULL) {
					
					char *srcPath = malloc(sizeof(char) * (strlen(projDir) + strlen(node->filePath) + 50));
					char *destPath = malloc(sizeof(char) * (strlen(projDir) + strlen(node->filePath) + 50));
					
					sprintf(srcPath, "%s/%s/%s", projDir, currentVersionStr, node->filePath);
					sprintf(destPath, "%s/%d/%s", projDir, newVersion, node->filePath);
					
					if(!checkFileExists(destPath)) {
						copyFile(srcPath, destPath);
					}
					
					free(srcPath);
					free(destPath);
					
					node = node->next;
				}
				
				
				///////////////////////////////////////////////////////////////
				// At this point, lets compress the old directory.
				//
				// Compressed file format is exactly similar to how we 
				// send files to the client.
				// 
				// <numFiles>:
				// <File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
				// <File2NameLen>:<File2Name><File2LenBytes>:<File2Contents>
				sprintf(path, "%s/%s.zlib_temp", projDir, currentVersionStr);
				
				int compressedTempFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
				
				sprintf(buffer, "%d:", serverManifest->numFiles + 1); // + 1 for manifest file.
				write(compressedTempFd, buffer, strlen(buffer));
				writeFileToSocket(compressedTempFd, projectName, MANIFEST_FILE);
				
				// Now one by one iterate on all files of manifest.
				node = serverManifest->head;
				while(node != NULL) {
					writeFileToSocket(compressedTempFd, projectName, node->filePath);
					node = node->next;
				}
				
				close(compressedTempFd);
				
				// Now temp file is ready.. We just need to compress this.
				char *zipFilePath = malloc(sizeof(char) * (strlen(projectName) + strlen(BASE_DIRECTORY) + 50));			
				sprintf(zipFilePath, "%s/%s.zlib", projDir, currentVersionStr);
				compressFile(path, zipFilePath);
				free(zipFilePath);
				
				// delete temp file
				unlink(path);
				
				// delete project directory old version
				sprintf(path, "%s/%s", projDir, currentVersionStr);
				removeDirectoryCompletely(path);
				
				///////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////
				
				
				// Read .COMMIT file now.
				// Delete the entries present into it.
				// For modifying entries, update version, md5.
				// For added entry, give version 1, and md5.
				// For deleted one, just remove from manifest.
							
				sprintf(path, "%s/%d/%s", projDir, newVersion, COMMIT_FILE);
				
				// add commit file to history
				pushFileToHistory(projectName, path);
				
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
					char *version = readAllBuffer(socketBuffer);
					
					readTillDelimiter(socketBuffer, commitFd, ' ');
					char *md5 = readAllBuffer(socketBuffer);
					
					// read filePath
					readTillDelimiter(socketBuffer, commitFd, '\n');
					char *fPath = readAllBuffer(socketBuffer);
					
					if(strcmp(code, "D") == 0) {
						// We need to delete the file locally also.
						char *fullPath = malloc(sizeof(char) * (strlen(projDir) + strlen(fPath) + 50));
						
						sprintf(fullPath, "%s/%d/%s", projDir, newVersion, fPath);
						
						if(checkFileExists(fullPath)) {
							unlink(fullPath);
						}
						free(fullPath);
						
						removeFileFromManifest(serverManifest, fPath);
					
					} else if(strcmp(code, "A") == 0) {
						
						addFileToManifest(serverManifest, strdup(md5), strdup(version), strdup(fPath));
						
					} else if(strcmp(code, "M") == 0) {
						
						// remove old entry from manifest
						removeFileFromManifest(serverManifest, fPath);
						
						// make new entry in manifest with updated version and md5.
						addFileToManifest(serverManifest, strdup(md5), strdup(version), strdup(fPath));
					}
							
					free(code);
					free(version);
					free(md5);
					free(fPath);
				}
				close(commitFd);
				
				// change the current version number in .VERSION_FILE
				sprintf(path, "%s/%s", projDir, VERSION_FILE);
				int versionFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
				sprintf(buffer, "%d", newVersion);
				write(versionFd, buffer, strlen(buffer));
				close(versionFd);
				
				// Increment project version.
				if(serverManifest->versionNumber != NULL) {
					free(serverManifest->versionNumber);
					serverManifest->versionNumber = readCurrentVersion(projectName);
				}
				
				sprintf(path, "%s/%d/%s", projDir, newVersion, MANIFEST_FILE);
				// At last Write the modified manifest into new directory
				createDirStructureIfNeeded(path);
				int manifestFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
				writeManifestToFile(serverManifest, manifestFd);
				close(manifestFd);
				
				// remove the .Commit file.
				sprintf(path, "%s/%d/%s", projDir, newVersion, COMMIT_FILE);
				if(checkFileExists(path)) {
					unlink(path);
				}
				
				freeManifest(serverManifest);
				
				// At last, Just send the manifest back to the client.
				write(sockfd, "sendfile:", strlen("sendfile:"));
				writeFileToSocket(sockfd, projectName, MANIFEST_FILE);
			}
			
			free(path);
			free(currentVersionStr);
			free(projDir);
			free(numFilesStr);
		}
		
		free(nameLen);
		free(projectName);
	}	
	
	free(command);
	freeSocketBuffer(socketBuffer);	
	
	// Probably Client wanted to ask more now.
	processCommand(sockfd);
}

void * socketThread(void *arg) {
	
	printf("Starting Client Thread\n");
	
	int clientSock = *((int *)arg);
	
	// Send message to the client socket
	pthread_mutex_lock(&lock);
	
	// Process the command from client.
	processCommand(clientSock);
	
	pthread_mutex_unlock(&lock);
	
	printf("Terminating Client connection\n\n");
	
	close(clientSock);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	srand(current_timestamp());
	
	if(argc < 2) {
		printf("Error: Please mention port.\n");
		printf("Usgae: ./server <port>\n");
		return 0;
	}
	
	// TODO: Check if BASE_DIRECTORY exists or not.
	// Create, if not.
	if(!checkDirectoryExists(BASE_DIRECTORY)) {
		createDirectory(BASE_DIRECTORY);
	}
	
	int serverSocket, newSocket;
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size;
	
	//Create the socket. 
	serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	
	// Configure settings of the server address struct
	// Address family = Internet 
	serverAddr.sin_family = AF_INET;
	//Set port number, using htons function to use proper byte order 
	serverAddr.sin_port = htons(atoi(argv[1]));
	//Set IP address to localhost 
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	//Set all bits of the padding field to 0 
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	//Bind the address struct to the socket 
	bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	
	//Listen on the socket, with 40 max connection requests queued 
	if (listen(serverSocket, 50) == 0)
		printf("Listening\n");
	else
		printf("Error\n");
	
	pthread_t tid[60];
	int i = 0;
	while (1) {
		
		//Accept call creates a new socket for the incoming connection
		addr_size = sizeof serverStorage;
		
		newSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);
		
		printf("After accepting:\n");
		
		//for each client request creates a thread and assign the client request to it to process
		//so the main thread can entertain next request
		if (pthread_create(&tid[i++], NULL, socketThread, &newSocket) != 0)
			printf("Failed to create thread\n");
		if (i >= 50) {
			i = 0;
			while (i < 50) {
				pthread_join(tid[i++], NULL);
			}
			i = 0;
		}
	}
	return 0;
}