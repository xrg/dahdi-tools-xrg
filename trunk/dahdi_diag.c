#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dahdi/user.h>
#include "version.h"

int main(int argc, char *argv[])
{
	int fd;
	int chan;
	if ((argc < 2) || (sscanf(argv[1], "%d", &chan) != 1)) {
		fprintf(stderr, "Usage: dahdi_diag <channel>\n");
		exit(1);
	}
	fd = open("/dev/dahdi/ctl", O_RDWR);
	if (fd < 0) {
		perror("open(/dev/dahdi/ctl");
		exit(1);
	}
	if (ioctl(fd, DAHDI_CHANDIAG, &chan)) {
		perror("ioctl(DAHDI_CHANDIAG)");
		exit(1);
	}
	exit(0);
}
