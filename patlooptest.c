#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <dahdi/user.h>
#include "version.h"

#define BLOCK_SIZE 2039

void print_packet(unsigned char *buf, int len)
{
	int x;
	printf("{ ");
	for (x=0;x<len;x++)
		printf("%02x ",buf[x]);
	printf("}\n");
}

int main(int argc, char *argv[])
{
	int fd;
	int res, x;
	int i;
	DAHDI_PARAMS tp;
	int bs = BLOCK_SIZE;
	int skipcount = 10;
	unsigned char c=0,c1=0;
	unsigned char inbuf[BLOCK_SIZE];
	unsigned char outbuf[BLOCK_SIZE];
	int setup=0;
	int errors=0;
	int bytes=0;
	int timeout=0;
	time_t start_time=0;
	if (argc < 2 || argc > 3 ) {
		fprintf(stderr, "Usage: %s <DAHDI device> [timeout]\n",argv[0]);
		exit(1);
	}
	fd = open(argv[1], O_RDWR, 0600);
	if (fd < 0) {
		fprintf(stderr, "Unable to open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}
	if (ioctl(fd, DAHDI_SET_BLOCKSIZE, &bs)) {
		fprintf(stderr, "Unable to set block size to %d: %s\n", bs, strerror(errno));
		exit(1);
	}
	if (ioctl(fd, DAHDI_GET_PARAMS, &tp)) {
		fprintf(stderr, "Unable to get channel parameters\n");
		exit(1);
	}
	ioctl(fd, DAHDI_GETEVENT);

	i = DAHDI_FLUSH_ALL;
	if (ioctl(fd,DAHDI_FLUSH,&i) == -1)
	   {
		perror("tor_flush");
		exit(255);
	   }
	if(argc==3){
		timeout=atoi(argv[2]);
		start_time=time(NULL);
		printf("Using Timeout of %d Seconds\n",timeout);
	}

	for(;;) {
		res = bs;
		for (x=0;x<bs;x++) 
			outbuf[x] = c1++;

		res = write(fd,outbuf,bs);
		if (res != bs)
		   {
			printf("Res is %d: %s\n", res, strerror(errno));
			ioctl(fd, DAHDI_GETEVENT, &x);
			printf("Event: %d\n", x);
			exit(1);
		}

		if (skipcount)
		   {
			if (skipcount > 1) read(fd,inbuf,bs);
			skipcount--;
			if (!skipcount) puts("Going for it...");
			continue;
		   }

		res = read(fd, inbuf, bs);
		if (res < bs) {
			printf("Res is %d\n", res);
			exit(1);
		}
		if (!setup) {
			c = inbuf[0];
			setup++;
		}
		for (x=0;x<bs;x++)  {
			if (inbuf[x] != c) {
				printf("(Error %d): Unexpected result, %d != %d, %d bytes since last error.\n", ++errors, inbuf[x],c, bytes);
				c = inbuf[x];
				bytes=0;
			}
			c++;
			bytes++;
		}
#if 0
		printf("(%d) Wrote %d bytes\n", packets++, res);
#endif
		if(timeout && (time(NULL)-start_time)>timeout){
			printf("Timeout achieved Ending Program\n");
			return errors;
		}
	}
	
}
