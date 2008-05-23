/*
 * Monitor a Zaptel Channel
 *
 * Written by Mark Spencer <markster@digium.com>
 * Based on previous works, designs, and architectures conceived and
 * written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001 Linux Support Services, Inc.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under thet erms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 * Primary Author: Mark Spencer <markster@digium.com>
 *
 */

#include <stdio.h> 
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include <dahdi/user.h>
#include "tonezone.h"
#include "version.h"

#include <linux/soundcard.h>

/*
* defines for file handle numbers
*/
#define MON_BRX		   0	/*!< both channels if multichannel==1 or receive otherwise */
#define MON_TX		   1	/*!< transmit channel */
#define MON_PRE_BRX	   2	/*!< same as MON_BRX but before echo cancellation */
#define MON_PRE_TX	   3	/*!< same as MON_TX but before echo cancellation */
#define MON_STEREO     4	/*!< stereo mix of rx/tx streams */
#define MON_PRE_STEREO 5	/*!< stereo mix of rx/tx before echo can.  This is exactly what is fed into the echo can */

#define BLOCK_SIZE 240

#define BUFFERS 4

#define FRAG_SIZE 8

/* Put the ofh (output file handles) outside
 * the main loop in case we ever add a signal
 * handler.
 */
static FILE*  ofh[6] = {0, 0, 0, 0, 0, 0};

static int stereo = 0;
static int verbose = 0;

int audio_open(void)
{
	int fd;
	int speed = 8000;
	int fmt = AFMT_S16_LE;
	int fragsize = (BUFFERS << 16) | (FRAG_SIZE);
	struct audio_buf_info ispace, ospace;
	fd = open("/dev/dsp", O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open /dev/dsp: %s\n", strerror(errno));
		return -1;
	}
	/* Step 1: Signed linear */
	if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0) {
		fprintf(stderr, "ioctl(SETFMT) failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	/* Step 2: Make non-stereo */
	if (ioctl(fd, SNDCTL_DSP_STEREO, &stereo) < 0) {
		fprintf(stderr, "ioctl(STEREO) failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (stereo != 0) {
		fprintf(stderr, "Can't turn stereo off :(\n");
	}
	/* Step 3: Make 8000 Hz */
	if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) < 0) {
		fprintf(stderr, "ioctl(SPEED) failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (speed != 8000) 
		fprintf(stderr, "Warning: Requested 8000 Hz, got %d\n", speed);
	if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &fragsize)) {
		fprintf(stderr, "Sound card won't let me set fragment size to %u %u-byte buffers (%x)\n"
						"so sound may be choppy: %s.\n", BUFFERS, (1 << FRAG_SIZE), fragsize, strerror(errno));
	}	
	bzero(&ispace, sizeof(ispace));
	bzero(&ospace, sizeof(ospace));

	if (ioctl(fd, SNDCTL_DSP_GETISPACE, &ispace)) {
		/* They don't support block size stuff, so just return but notify the user */
		fprintf(stderr, "Sound card won't let me know the input buffering...\n");
	}
	if (ioctl(fd, SNDCTL_DSP_GETOSPACE, &ospace)) {
		/* They don't support block size stuff, so just return but notify the user */
		fprintf(stderr, "Sound card won't let me know the output buffering...\n");
	}
	fprintf(stderr, "New input space:  %d of %d %d byte fragments (%d bytes left)\n", 
		ispace.fragments, ispace.fragstotal, ispace.fragsize, ispace.bytes);
	fprintf(stderr, "New output space:  %d of %d %d byte fragments (%d bytes left)\n", 
		ospace.fragments, ospace.fragstotal, ospace.fragsize, ospace.bytes);
	return fd;
}

int pseudo_open(void)
{
	int fd;
	int x = 1;
	fd = open("/dev/zap/pseudo", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Unable to open pseudo channel: %s\n", strerror(errno));
		return -1;
	}
	if (ioctl(fd, DAHDI_SETLINEAR, &x)) {
		fprintf(stderr, "Unable to set linear mode: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	x = BLOCK_SIZE;
	if (ioctl(fd, DAHDI_SET_BLOCKSIZE, &x)) {
		fprintf(stderr, "unable to set sane block size: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

#define barlen 35
#define baroptimal 3250
//define barlevel 200
#define barlevel ((baroptimal/barlen)*2)
#define maxlevel (barlen*barlevel)

void draw_barheader()
{
	char bar[barlen+5];

	memset(bar, '-', sizeof(bar));
	memset(bar, '<', 1);
	memset(bar+barlen+2, '>', 1);
	memset(bar+barlen+3, '\0', 1);

	dahdi_copy_string(bar+(barlen/2), "(RX)", 4);
	printf("%s", bar);

	dahdi_copy_string(bar+(barlen/2), "(TX)", 4);
	printf(" %s\n", bar);
}

void draw_bar(int avg, int max)
{
	char bar[barlen+5];

	memset(bar, ' ', sizeof(bar));

	max /= barlevel;
	avg /= barlevel;
	if (avg > barlen)
		avg = barlen;
	if (max > barlen)
		max = barlen;
	
	if (avg > 0) 
		memset(bar, '#', avg);
	if (max > 0) 
		memset(bar + max, '*', 1);

	bar[barlen+1] = '\0';
	printf("%s", bar);
	fflush(stdout);
}

void visualize(short *tx, short *rx, int cnt)
{
	int x;
	float txavg = 0;
	float rxavg = 0;
	static int txmax = 0;
	static int rxmax = 0;
	static int sametxmax = 0;
	static int samerxmax = 0;
	static int txbest = 0;
	static int rxbest = 0;
	float ms;
	static struct timeval last;
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	ms = (tv.tv_sec - last.tv_sec) * 1000.0 + (tv.tv_usec - last.tv_usec) / 1000.0;
	for (x=0;x<cnt;x++) {
		txavg += abs(tx[x]);
		rxavg += abs(rx[x]);
	}
	txavg = abs(txavg / cnt);
	rxavg = abs(rxavg / cnt);
	
	if (txavg > txbest)
		txbest = txavg;
	if (rxavg > rxbest)
		rxbest = rxavg;
	
	/* Update no more than 10 times a second */
	if (ms < 100)
		return;
	
	/* Save as max levels, if greater */
	if (txbest > txmax) {
		txmax = txbest;
		sametxmax = 0;
	}
	if (rxbest > rxmax) {
		rxmax = rxbest;
		samerxmax = 0;
	}

	memcpy(&last, &tv, sizeof(last));

	/* Clear screen */
	printf("\r ");
	draw_bar(rxbest, rxmax);
	printf("   ");
	draw_bar(txbest, txmax);
	if (verbose)
		printf("   Rx: %5d (%5d) Tx: %5d (%5d)", rxbest, rxmax, txbest, txmax);
	txbest = 0;
	rxbest = 0;
	
	/* If we have had the same max hits for x times, clear the values */
	sametxmax++;
	samerxmax++;
	if (sametxmax > 6) {
		txmax = 0;
		sametxmax = 0;
	}
	if (samerxmax > 6) {
		rxmax = 0;
		samerxmax = 0;
	}
}

int main(int argc, char *argv[])
{
	int afd = -1;
	int pfd[4] = {-1, -1, -1, -1};
	short buf_brx[BLOCK_SIZE * 2];
	short buf_tx[BLOCK_SIZE * 4];
	short stereobuf[BLOCK_SIZE * 4];
	int res_brx, res_tx;
	int visual = 0;
	int multichannel = 0;
	int ossoutput = 0;
	int preecho = 0;
	int savefile = 0;
	int stereo_output = 0;
	int limit = 0;
	int readcount = 0;
	int x, i, chan;
	struct dahdi_confinfo zc;

	if ((argc < 2) || (atoi(argv[1]) < 1)) {
		fprintf(stderr, "Usage: ztmonitor <channel num> [-v[v]] [-m] [-o] [-p] [-l limit] [-f FILE | -s FILE | -r FILE1 -t FILE2] [-F FILE | -S FILE | -R FILE1 -T FILE2]\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "        -v: Visual mode.  Implies -m.\n");
		fprintf(stderr, "        -vv: Visual/Verbose mode.  Implies -m.\n");
		fprintf(stderr, "        -l LIMIT: Stop after reading LIMIT bytes\n");
		fprintf(stderr, "        -m: Separate rx/tx streams.\n");
		fprintf(stderr, "        -o: Output audio via OSS.  Note: Only 'normal' combined rx/tx streams are output via OSS.\n");
		fprintf(stderr, "        -p: Get a pre-echocanceled stream.\n");
		fprintf(stderr, "        -f FILE: Save combined rx/tx stream to FILE.  Cannot be used with -m.\n");
		fprintf(stderr, "        -r FILE: Save rx stream to FILE.  Implies -m.\n");
		fprintf(stderr, "        -t FILE: Save tx stream to FILE.  Implies -m.\n");
		fprintf(stderr, "        -s FILE: Save stereo rx/tx stream to FILE.\n");
		fprintf(stderr, "        -F FILE: Save combined pre-echocanceled rx/tx stream to FILE.  Cannot be used with -m.  Implies -p.\n");
		fprintf(stderr, "        -R FILE: Save pre-echocanceled rx stream to FILE.  Implies -m and -p.\n");
		fprintf(stderr, "        -T FILE: Save pre-echocanceled tx stream to FILE.  Implies -m and -p.\n");
		fprintf(stderr, "        -S FILE: Save pre-echocanceled stereo rx/tx stream to FILE.  Implies -p.\n");
		fprintf(stderr, "Examples:\n");
		fprintf(stderr, "Save a stream to a file\n");
		fprintf(stderr, "        ztmonitor 1 -f stream.raw\n");
		fprintf(stderr, "Visualize an rx/tx stream and save them to separate files.\n");
		fprintf(stderr, "        ztmonitor 1 -v -r streamrx.raw -t streamtx.raw\n");
		fprintf(stderr, "Play a combined rx/tx stream via OSS and save it to a file\n");
		fprintf(stderr, "        ztmonitor 1 -o -f stream.raw\n");
		fprintf(stderr, "Save a combined normal rx/tx stream and a combined 'preecho' rx/tx stream to files\n");
		fprintf(stderr, "        ztmonitor 1 -p -f stream.raw -F streampreecho.raw\n");
		fprintf(stderr, "Save a normal rx/tx stream and a 'preecho' rx/tx stream to separate files\n");
		fprintf(stderr, "        ztmonitor 1 -m -p -r streamrx.raw -t streamtx.raw -R streampreechorx.raw -T streampreechotx.raw\n");
		exit(1);
	}
    
	chan = atoi(argv[1]);
    
	for (i = 2; i < argc; ++i) {
		if (!strcmp(argv[i], "-v")) {
			if (visual)
				verbose = 1;
		        visual = 1;
			multichannel = 1;
		} else if (!strcmp(argv[i], "-vv")) {
			visual = 1;
			verbose = 1;
			multichannel = 1;
		} else if ((!strcmp(argv[i], "-f") || !strcmp(argv[i], "-r") || !strcmp(argv[i], "-t")
				|| !strcmp(argv[i], "-F") || !strcmp(argv[i], "-R") || !strcmp(argv[i], "-T")
				|| !strcmp(argv[i], "-s") || !strcmp(argv[i], "-S"))
				&& (i+1) < argc) {
			char *output_file;

			/* Set which file descriptor to use */
			if (!strcmp(argv[i], "-f")) {
				savefile = 1;
				x = MON_BRX;
			} else if (!strcmp(argv[i], "-r")) {
				savefile = 1;
				multichannel = 1;
				x = MON_BRX;
			} else if (!strcmp(argv[i], "-t")) {
				savefile = 1;
				multichannel = 1;
				x = MON_TX;
			} else if (!strcmp(argv[i], "-s")) {
				savefile = 1;
				stereo_output = 1;
				multichannel = 1;
				x = MON_STEREO;
			} else if (!strcmp(argv[i], "-F")) {
				savefile = 1;
				preecho = 1;
				x = MON_PRE_BRX;
			} else if (!strcmp(argv[i], "-R")) {
				savefile = 1;
				multichannel = 1;
				preecho = 1;
				x = MON_PRE_BRX;
			} else if (!strcmp(argv[i], "-T")) {
				savefile = 1;
				multichannel = 1;
				preecho = 1;
				x = MON_PRE_TX;
			} else if (!strcmp(argv[i], "-S")) {
				savefile = 1;
				preecho = 1;
				stereo_output = 1;
				multichannel = 1;
				x = MON_PRE_STEREO;
			} else
				x = MON_BRX;

			++i; /* we care about the file name */
			output_file = argv[i];
			fprintf(stderr, "Output to %s\n", output_file);
			if ((ofh[x] = fopen(output_file, "w"))<0) {
				fprintf(stderr, "Could not open %s for writing: %s\n", 
						output_file, strerror(errno));
				exit(1);
			}
			fprintf(stderr, "Run e.g., 'sox -r 8000 -s -w -c 1 %s %s.wav' to convert.\n", 
					output_file, output_file);
		} else if (!strcmp(argv[i], "-m")) {
			multichannel = 1;
		} else if (!strcmp(argv[i], "-o")) {
			ossoutput = 1;
		} else if (!strcmp(argv[i], "-p")) {
			preecho = 1;
		} else if (!strcmp(argv[i], "-l") && isdigit(argv[i+1][0])) {
			limit = atoi(argv[i+1]);
			i++;
		}
	}

	if (ossoutput) {
		if (multichannel) {
			printf("Multi-channel audio is enabled.  OSS output will be disabled.\n");
			ossoutput = 0;
		} else {
			/* Open audio */
			if ((afd = audio_open()) < 0) {
				printf("Cannot open audio ...\n");
				ossoutput = 0;
			}
		}
	}
	if (!ossoutput && !multichannel && !savefile) {
		fprintf(stderr, "Nothing to do with the stream(s) ...\n");
		exit(1);
	}

	/* Open Pseudo device */
	if ((pfd[MON_BRX] = pseudo_open()) < 0)
		exit(1);
	if (multichannel && ((pfd[MON_TX] = pseudo_open()) < 0))
		exit(1);
	if (preecho) {
		if ((pfd[MON_PRE_BRX] = pseudo_open()) < 0)
			exit(1);
		if (multichannel && ((pfd[MON_PRE_TX] = pseudo_open()) < 0))
			exit(1);
	}
	/* Conference them */
	if (multichannel) {
		memset(&zc, 0, sizeof(zc));
		zc.chan = 0;
		zc.confno = chan;
		/* Two pseudo's, one for tx, one for rx */
		zc.confmode = DAHDI_CONF_MONITOR;
		if (ioctl(pfd[MON_BRX], DAHDI_SETCONF, &zc) < 0) {
			fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
			exit(1);
		}
		memset(&zc, 0, sizeof(zc));
		zc.chan = 0;
		zc.confno = chan;
		zc.confmode = DAHDI_CONF_MONITORTX;
		if (ioctl(pfd[MON_TX], DAHDI_SETCONF, &zc) < 0) {
			fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
			exit(1);
		}
		if (preecho) {
			memset(&zc, 0, sizeof(zc));
			zc.chan = 0;
			zc.confno = chan;
			/* Two pseudo's, one for tx, one for rx */
			zc.confmode = DAHDI_CONF_MONITOR_RX_PREECHO;
			if (ioctl(pfd[MON_PRE_BRX], DAHDI_SETCONF, &zc) < 0) {
				fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
				exit(1);
			}
			memset(&zc, 0, sizeof(zc));
			zc.chan = 0;
			zc.confno = chan;
			zc.confmode = DAHDI_CONF_MONITOR_TX_PREECHO;
			if (ioctl(pfd[MON_PRE_TX], DAHDI_SETCONF, &zc) < 0) {
				fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
				exit(1);
			}
		}
	} else {
		memset(&zc, 0, sizeof(zc));
		zc.chan = 0;
		zc.confno = chan;
		zc.confmode = DAHDI_CONF_MONITORBOTH;
		if (ioctl(pfd[MON_BRX], DAHDI_SETCONF, &zc) < 0) {
			fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
			exit(1);
		}
		if (preecho) {
			memset(&zc, 0, sizeof(zc));
			zc.chan = 0;
			zc.confno = chan;
			zc.confmode = DAHDI_CONF_MONITORBOTH_PREECHO;
			if (ioctl(pfd[MON_PRE_BRX], DAHDI_SETCONF, &zc) < 0) {
				fprintf(stderr, "Unable to monitor: %s\n", strerror(errno));
				exit(1);
			}
		}
	}
	if (visual) {
		printf("\nVisual Audio Levels.\n");
		printf("--------------------\n");
		printf(" Use zapata.conf file to adjust the gains if needed.\n\n");
		printf("( # = Audio Level  * = Max Audio Hit )\n");
		draw_barheader();
	}
	/* Now, copy from pseudo to audio */
	for (;;) {
		res_brx = read(pfd[MON_BRX], buf_brx, sizeof(buf_brx));
		if (res_brx < 1)
			break;
		readcount += res_brx;
		if (ofh[MON_BRX])
			fwrite(buf_brx, 1, res_brx, ofh[MON_BRX]);

		if (multichannel) {
			res_tx = read(pfd[MON_TX], buf_tx, res_brx);
			if (res_tx < 1)
				break;
			if (ofh[MON_TX])
				fwrite(buf_tx, 1, res_tx, ofh[MON_TX]);

			if (stereo_output && ofh[MON_STEREO]) {
				for (x=0;x<res_tx;x++) {
					stereobuf[x*2] = buf_brx[x];
					stereobuf[x*2+1] = buf_tx[x];
				}
				fwrite(stereobuf, 1, res_tx*2, ofh[MON_STEREO]);
			}

			if (visual) {
				if (res_brx == res_tx)
					visualize((short *)buf_tx, (short *)buf_brx, res_brx/2);
				else
					printf("Huh?  res_tx = %d, res_brx = %d?\n", res_tx, res_brx);
			}
		}

		if (preecho) {
			res_brx = read(pfd[MON_PRE_BRX], buf_brx, sizeof(buf_brx));
			if (res_brx < 1)
				break;
			if (ofh[MON_PRE_BRX])
				fwrite(buf_brx, 1, res_brx, ofh[MON_PRE_BRX]);

			if (multichannel) {
				res_tx = read(pfd[MON_PRE_TX], buf_tx, res_brx);
				if (res_tx < 1)
					break;
				if (ofh[MON_PRE_TX])
					fwrite(buf_tx, 1, res_tx, ofh[MON_PRE_TX]);

				if (stereo_output && ofh[MON_PRE_STEREO]) {
					for (x=0;x<res_brx;x++) {
						stereobuf[x*2] = buf_brx[x];
						stereobuf[x*2+1] = buf_tx[x];
					}
					fwrite(stereobuf, 1, res_brx*2, ofh[MON_PRE_STEREO]);
				}

				/* XXX How are we going to visualize the preecho set of streams?
				if (visual) {
					if (res == res2)
						visualize((short *)buf, (short *)buf2, res/2);
					else
						printf("Huh?  res = %d, res2 = %d?\n", res, res2);
				} */
			}
		}

		if (ossoutput && afd) {
			if (stereo) {
				for (x=0;x<res_brx;x++)
					buf_tx[x<<1] = buf_tx[(x<<1) + 1] = buf_brx[x];
				write(afd, buf_tx, res_brx << 1);
			} else
				write(afd, buf_brx, res_brx);
		}

		if (limit && readcount >= limit) {
			/* bail if we've read too much */
			break;
		}
	}
	if (ofh[MON_BRX]) fclose(ofh[MON_BRX]);
	if (ofh[MON_TX]) fclose(ofh[MON_TX]);
	if (ofh[MON_PRE_BRX]) fclose(ofh[MON_PRE_BRX]);
	if (ofh[MON_PRE_TX]) fclose(ofh[MON_PRE_TX]);
	if (ofh[MON_STEREO]) fclose(ofh[MON_STEREO]);
	if (ofh[MON_PRE_STEREO]) fclose(ofh[MON_PRE_STEREO]);
	exit(0);
}
