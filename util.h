#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <openssl/md5.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "socketBuffer.h"

#define MAX_MSG_SIZE 1024

// as each hex character is in 2 ascii chars
#define HASH_STRING_LEN 2*MD5_DIGEST_LENGTH

static char *MANIFEST_FILE = ".manifest";

void computeFileHash(char *filename, unsigned char hash[]);

long long current_timestamp();
long long current_timestamp_millis();

long long getLastModifiedTime(char *filePath);

int checkDirectoryExists(char *dirName);

int checkFileExists(char *fileName);

void createDirectory(char *dirName);

long int findFileSize(char *fileName);

char *readFileContents(char *fileName);

void createDirStructureIfNeeded(char *path);

/*
This is helper method to write the contents to the file, which are sent
back from server.
Format:
<FileNameLen>:<FileName><FileLenBytes>:<FileContents>

*/
void writeFileFromSocket(int sockToRead, char *baseDir);

void writeFileDetailsToSocket(char *filePath, char *baseDir, int socket);

int removeDirectoryCompletely(char *path);
void copyFile(char *srcFilePath, char *destFilePath);
void deleteFilesWithPrefix(char *dirToSearch, char *prefix);
int checkForFileMatch(char *filePath, char *dirToSearch, char *prefix);

void convertZlibToResponse(int sockFd, char *responseFile, char *baseDir);
void convertResponseToZlib(int sockFd, char *responseFile, char *baseDir);

#endif