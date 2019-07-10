#include "util.h"
#include "compressor.h"

void computeFileHash(char *filename, unsigned char hash[HASH_STRING_LEN]) {

    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        return;
    }

	// read file in chunks
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
	
		
	// convert the hex
	unsigned char tmp[MD5_DIGEST_LENGTH];
    MD5_Final (tmp, &mdContext);
	
	// one by one convert character to hex chars.
	for(i=0; i<MD5_DIGEST_LENGTH; i++) {
		sprintf(hash + 2*i, "%02x", tmp[i]);
	}
	
    fclose (inFile);
}

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    return te.tv_sec; // Only consider seconds, no millis
}

long long current_timestamp_millis() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    return te.tv_sec * 1000 + te.tv_usec / 1000; // millis
}

long long getLastModifiedTime(char *filePath) {
	struct stat statinfo;
    stat(filePath, &statinfo);
	
	// Elapsed seconds since EPOCH till now.
	long long value = statinfo.st_mtime;
	
    return value;
}

int checkDirectoryExists(char *dirName) {
	struct stat s = {0};
	if (!stat(dirName, &s)) {
		return (s.st_mode & S_IFDIR);
	} else {
		return 0;
	}
}

int checkFileExists(char *fileName) {
	struct stat buffer;
	return (stat(fileName, &buffer) == 0);
}

void createDirectory(char *dirName) {
	struct stat s = {0};
	if (stat(dirName, &s) == -1) {
		mkdir(dirName, 0777);
	}
}

long int findFileSize(char *fileName) {
		
	struct stat buffer;
	int status = stat(fileName, &buffer);
	
	// if permission available
	if(status == 0) {
		return buffer.st_size;
	}
	return -1;
}

// Returns dynamically created string, delete it Yourself.
char *readFileContents(char *fileName) {
	if(!checkFileExists(fileName)) {
		printf("Unable to read file contents. File %s does not exist.", fileName);
		return NULL;
	}
	char *result = malloc(sizeof(char) * (findFileSize(fileName) + 10));
	
	// Now write file char by char on the socket		
	int fileFd = open(fileName, O_RDONLY, 0777);
	int i = 0;
	while (read(fileFd, &result[i], 1) == 1) {
		i++;
	}
	close(fileFd);
	result[i] = '\0';
	return result;
}

// Open system call fails for nester directories before file.
// So for a/b/c/d.txt, It create a, b & c directories.
void createDirStructureIfNeeded(char *path) {
	char *x = strchr(path, '/');
	int l = strlen(path);
	char dir[l + 1];
	
	while(x != NULL) {
		
		strcpy(dir, path);
		int i = (int)(x - path);
		dir[i] = '\0';
		
		if(!checkDirectoryExists(dir)) {
			createDirectory(dir);
		}
		
		x = strchr(x + 1, '/');
	}
}

/*
Manifest file format:
<project Name>
<project Version>
<numFiles>
<md5hash><space><version><space><file path>
<md5hash><space><version><space><file path>
<md5hash><space><version><space><file path>
..

The filePaths are relative to the manifest file.
*/
void writeFileDetailsInManifest(int manifestFd, char *filePath) {
	unsigned char fileHash[HASH_STRING_LEN];
	computeFileHash(filePath, fileHash);
	
	write(manifestFd, fileHash, HASH_STRING_LEN);
	write(manifestFd, " ", 1);
	
	char timeStamp[20];
	sprintf(timeStamp, "%lld", getLastModifiedTime(filePath));
	
	write(manifestFd, timeStamp, strlen(timeStamp));
	write(manifestFd, " ", 1);
	write(manifestFd, filePath, strlen(filePath));
	write(manifestFd, "\n", 1);
}



/*
This is helper method to write the contents to the file, which are sent
back from server.
Format:
<FileNameLen>:<FileName><FileLenBytes>:<FileContents>

Precondition: This method is called once we are sure that server is going to supply the contents.
*/
void writeFileFromSocket(int sockToRead, char *baseDir) {
	SocketBuffer *socketBuffer = createBuffer();

	readTillDelimiter(socketBuffer, sockToRead, ':');
	char *nameLenStr = readAllBuffer(socketBuffer);
	long nameLen = atol(nameLenStr);
	
	readNBytes(socketBuffer, sockToRead, nameLen);
	char *filePath = readAllBuffer(socketBuffer);
	
	readTillDelimiter(socketBuffer, sockToRead, ':');
	char *contentLenStr = readAllBuffer(socketBuffer);
	long contentLen = atol(contentLenStr);
		
	char *fullpath = malloc(sizeof(char) * (strlen(filePath) + 15 + strlen(baseDir)));
	sprintf(fullpath, "%s/%s", baseDir, filePath);
	
	// Create the directory structure if needed.
	createDirStructureIfNeeded(fullpath);
	
	// Write data to the file now.
	int fd = open(fullpath, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	char c;
	long i = 0;
	while(i++ < contentLen) {
		read(sockToRead, &c, 1);
		if(c == '\0') {
			break; // Socket disconnected.
		}
		write(fd, &c, 1);
	}
	close(fd);	
	
	// de-allocate memory
	freeSocketBuffer(socketBuffer);
	free(nameLenStr);
	free(filePath);
	free(contentLenStr);
	free(fullpath);
}

/*
<File1NameLen>:<File1Name><File1LenBytes>:<File1Contents>
*/
void writeFileDetailsToSocket(char *filePath, char *baseDir, int socket) {
	char buffer[100];
	char *path = malloc(sizeof(char) * (strlen(filePath) + strlen(baseDir) + 25));
	sprintf(path, "%s/%s", baseDir, filePath);

	sprintf(buffer, "%d:", strlen(filePath));
	write(socket, buffer, strlen(buffer));
	write(socket, filePath, strlen(filePath));
	
	long size = findFileSize(path);
	sprintf(buffer, "%ld:", size);
	write(socket, buffer, strlen(buffer));
	
	// write size bytes from file to socket.
	int fd = open(path, O_RDONLY, 0777);
	
	char c;
	long i = 0;
	while(i++ < size) {
		read(fd, &c, 1);
		if(c == '\0') {
			break; // Socket disconnected.
		}
		write(socket, &c, 1);
	}
	
	close(fd);
	free(path);
}


int removeDirectoryCompletely(char *path) {

   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d) {
      struct dirent *p;

      r = 0;

      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          struct stat statbuf;

          snprintf(buf, len, "%s/%s", path, p->d_name);

          if (!stat(buf, &statbuf)) {
             if (S_ISDIR(statbuf.st_mode)) {
                r2 = removeDirectoryCompletely(buf);
             } else {
                r2 = unlink(buf);
             }
          } else {
			  printf("Stat error while deleting complete folder: %s\n", buf);
			  fflush(stdout);
		  }

          free(buf);
          r = r2;
      }

   } else {
	  printf("Could not open dir while deleting complete folder: %s.\n", path);
	  fflush(stdout);
   }
   closedir(d);

   if (!r) {
      r = rmdir(path);
   }

   return r;
}

/*
This function searches for any file
*/
int checkForFileMatch(char *filePath, char *dirToSearch, char *prefix) {
	
	char buffer1[100], buffer2[100];
	
	char *path = malloc(sizeof(char) * (strlen(dirToSearch) + 25));
	
	computeFileHash(filePath, buffer1);
	buffer1[HASH_STRING_LEN] = '\0';	
	
    DIR *d;
    struct dirent *dir;
    d = opendir(dirToSearch);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
			if (dir->d_type == DT_REG) {
				
				char *fName = dir->d_name;
				
				sprintf(path, "%s/%s", dirToSearch, fName);
				
				// if file starts with prefix.
				if(strstr(fName, prefix) == fName) {
					
					computeFileHash(path, buffer2);
					buffer2[HASH_STRING_LEN] = '\0';	
					
					// same file
					if(strcmp(buffer1, buffer2) == 0) {
						return 1;
					}
					
				}
			}  
        }
    }
	closedir(d);
	
	free(path);
	
	return 0;
}

void deleteFilesWithPrefix(char *dirToSearch, char *prefix) {
	
	char *path = malloc(sizeof(char) * (strlen(dirToSearch) + 25));
	
    DIR *d;
    struct dirent *dir;
    d = opendir(dirToSearch);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
			if (dir->d_type == DT_REG) {
				
				char *fName = dir->d_name;
				
				// if file starts with prefix.
				if(strstr(fName, prefix) == fName) {
					sprintf(path, "%s/%s", dirToSearch, fName);
					unlink(path);					
				}
			}
        }
    }
	closedir(d);
	free(path);
}


void copyFile(char *srcFilePath, char *destFilePath) {
	
	createDirStructureIfNeeded(destFilePath);
	
    int src_fd = open(srcFilePath, O_RDONLY);
    int dst_fd = open(destFilePath, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	
	char buffer[4096];
    while (1) {
        int n = read(src_fd, buffer, 4096);
        if (n > 0) {
            write(dst_fd, buffer, n);
        }

        if (n == 0) break;
    }

    close(src_fd);
    close(dst_fd);	
}


void writeNBytesToFile(long nBytes, int sockToRead, int sockToWrite) {	
	while(nBytes-- > 0) {
		char c;
		read(sockToRead, &c, 1);
		write(sockToWrite, &c, 1);
	}
}


// This function writes the response after decompressing it
// Server sends the compressed zlib response from socket.
// Client passes the path of file on which the unencrypted data
// should be written.
// 
// The server Format is below:
// numBytes:<content>
//
// Error checking is done before calling this function
void convertZlibToResponse(int sockFd, char *responseFile, char *baseDir) {
	SocketBuffer *socketBuffer = createBuffer();
	readTillDelimiter(socketBuffer, sockFd, ':');
	char *numBytesStr = readAllBuffer(socketBuffer);
	long numBytes = atol(numBytesStr);
	
	printf("Reading %ld bytes from socket\n", numBytes); fflush(stdout);
	
	char path[100];
	sprintf(path, "%s/tmp_res%lld_%d", baseDir, current_timestamp(), rand());
	createDirStructureIfNeeded(path);
	int writeFd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	
	// first right encrypted data to a temp file.
	writeNBytesToFile(numBytes, sockFd, writeFd); 
	close(writeFd);
	
	// now unecrypt data from this file, and write to response file.
	decompressFile(path, responseFile);
	
	// delete temp file.
	unlink(path);
	
	free(numBytesStr);
	freeSocketBuffer(socketBuffer);
}




// This function reads the response file
// And writes to socket in below format:
// <contentLen>:<compressed data>
void convertResponseToZlib(int sockFd, char *responseFile, char *baseDir) {
	
	char *path = malloc(sizeof(char) * (strlen(baseDir) + 50));		
	
	// convert response file to zlib compressed.
	char buffer[100];
	sprintf(path, "%s/tmp_res%lld_%d", baseDir, current_timestamp_millis(), rand());
	
	compressFile(responseFile, path);
	long numBytes = findFileSize(path);
	
	int readFd = open(path, O_RDONLY, 0777);
	
	sprintf(buffer, "%ld:", numBytes);
	write(sockFd, buffer, strlen(buffer));
	
	// now write compressed data to socket.
	writeNBytesToFile(numBytes, readFd, sockFd);
	
	close(readFd);
	unlink(path);
	free(path);
}



