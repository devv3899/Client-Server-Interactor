#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <zlib.h>
#include <assert.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

// Compress the input file and write data on output file
static void compressFile(char *inFile, char *outFile)
{
	int readFd = open(inFile, O_RDONLY, 0777);
	int writeFd = open(outFile, O_CREAT | O_WRONLY | O_TRUNC, 0777);	
	
    int ret, flush;
    unsigned numBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
		printf("Error in ZLIB\n");
		close(readFd);
		close(writeFd);
        return;
	}

    /* compress until end of file */
    do {
        strm.avail_in = read(readFd, in, CHUNK);
        if (strm.avail_in < 0) {
            (void)deflateEnd(&strm);
			printf("Error in Reading file %s\n", inFile);
			close(readFd);
			close(writeFd);
            return;
        }
        flush = (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            numBytes = CHUNK - strm.avail_out;   // numBytes to write on output.
			write(writeFd, out, numBytes);
			
        } while (strm.avail_out == 0);

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
	
	close(readFd);
	close(writeFd);
}


// DeCompress the input file and write data on output file
static void decompressFile(char *inFile, char *outFile)
{
	int readFd = open(inFile, O_RDONLY, 0777);
	int writeFd = open(outFile, O_CREAT | O_WRONLY | O_TRUNC, 0777);	
	
    int ret;
    unsigned numBytes;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
		printf("Error in ZLIB\n");
		close(readFd);
		close(writeFd);
        return;
	}

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = read(readFd, in, CHUNK);
        if (strm.avail_in < 0) {
            (void)deflateEnd(&strm);
			printf("Error in Reading file %s\n", inFile);
			close(readFd);
			close(writeFd);
            return;
        }
		
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
				printf("Error in ZLIB: Memory issue.\n");
                (void)inflateEnd(&strm);
				close(readFd);
				close(writeFd);
                return;
            }
            numBytes = CHUNK - strm.avail_out;
			write(writeFd, out, numBytes);
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
	
	close(readFd);
	close(writeFd);
}

#endif