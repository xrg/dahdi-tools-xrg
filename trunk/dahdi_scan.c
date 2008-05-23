/*
 * Scan and output information about DAHDI spans and ports.
 * 
 * Written by Brandon Kruse <bkruse@digium.com>
 * and Kevin P. Fleming <kpfleming@digium.com>
 * Copyright (C) 2007 Digium, Inc.
 *
 * Based on zttool written by Mark Spencer <markster@digium.com>
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
 */

#include <stdio.h> 
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <dahdi/user.h>

#include "version.h"

int main(int argc, char *argv[])
{
	int ctl;
	int x, y;
	struct dahdi_params params;
	unsigned int basechan = 1;
	struct dahdi_spaninfo s;
	char buf[100];
	char alarms[50];

	if ((ctl = open("/dev/dahdi/ctl", O_RDWR)) < 0) {
		fprintf(stderr, "Unable to open /dev/dahdi/ctl: %s\n", strerror(errno));
		exit(1);
	}
	
	for (x = 1; x < DAHDI_MAX_SPANS; x++) {
		memset(&s, 0, sizeof(s));
		s.spanno = x;
		if (ioctl(ctl, DAHDI_SPANSTAT, &s))
			continue;

		alarms[0] = '\0';
		if (s.alarms) {
			if (s.alarms & DAHDI_ALARM_BLUE)
				strcat(alarms,"BLU/");
			if (s.alarms & DAHDI_ALARM_YELLOW)
				strcat(alarms, "YEL/");
			if (s.alarms & DAHDI_ALARM_RED)
				strcat(alarms, "RED/");
			if (s.alarms & DAHDI_ALARM_LOOPBACK)
				strcat(alarms,"LB/");
			if (s.alarms & DAHDI_ALARM_RECOVER)
				strcat(alarms,"REC/");
			if (s.alarms & DAHDI_ALARM_NOTOPEN)
				strcat(alarms, "NOP/");
			if (!strlen(alarms))
				strcat(alarms, "UUU/");
			if (strlen(alarms)) {
				/* Strip trailing / */
				alarms[strlen(alarms)-1]='\0';
			}
		} else {
			if (s.numchans)
				strcpy(alarms, "OK");
			else
				strcpy(alarms, "UNCONFIGURED");
		}

		fprintf(stdout, "[%d]\n", x);
		fprintf(stdout, "active=yes\n");
		fprintf(stdout, "alarms=%s\n", alarms);
		fprintf(stdout, "description=%s\n", s.desc);
		fprintf(stdout, "name=%s\n", s.name);
		fprintf(stdout, "manufacturer=%s\n", s.manufacturer);
		fprintf(stdout, "devicetype=%s\n", s.devicetype);
		fprintf(stdout, "location=%s\n", s.location);
		fprintf(stdout, "basechan=%d\n", basechan);
		fprintf(stdout, "totchans=%d\n", s.totalchans);
		fprintf(stdout, "irq=%d\n", s.irq);
		y = basechan;
		memset(&params, 0, sizeof(params));
		params.channo = y;
		if (ioctl(ctl, DAHDI_GET_PARAMS, &params)) {
			basechan += s.totalchans;
			continue;
		}

		if (params.sigcap & (__DAHDI_SIG_DACS |  DAHDI_SIG_CAS)) {
			/* this is a digital span */
			fprintf(stdout, "type=digital-%s\n", s.spantype);
			fprintf(stdout, "syncsrc=%d\n", s.syncsrc);
			fprintf(stdout, "lbo=%s\n", s.lboname);
			fprintf(stdout, "coding_opts=");
			buf[0] = '\0';
			if (s.linecompat & DAHDI_CONFIG_B8ZS) strcat(buf, "B8ZS,");
			if (s.linecompat & DAHDI_CONFIG_AMI) strcat(buf, "AMI,");
			if (s.linecompat & DAHDI_CONFIG_HDB3) strcat(buf, "HDB3,");
			buf[strlen(buf) - 1] = '\0';
			fprintf(stdout, "%s\n", buf);
			fprintf(stdout, "framing_opts=");
			buf[0] = '\0';
			if (s.linecompat & DAHDI_CONFIG_ESF) strcat(buf, "ESF,");
			if (s.linecompat & DAHDI_CONFIG_D4) strcat(buf, "D4,");
			if (s.linecompat & DAHDI_CONFIG_CCS) strcat(buf, "CCS,");
			if (s.linecompat & DAHDI_CONFIG_CRC4) strcat(buf, "CRC4,");
			buf[strlen(buf) - 1] = '\0';
			fprintf(stdout, "%s\n", buf);
			fprintf(stdout, "coding=");
			if (s.lineconfig & DAHDI_CONFIG_B8ZS) fprintf(stdout, "B8ZS");
			else if (s.lineconfig & DAHDI_CONFIG_AMI) fprintf(stdout, "AMI");
			else if (s.lineconfig & DAHDI_CONFIG_HDB3) fprintf(stdout, "HDB3");
			fprintf(stdout, "\n");
			fprintf(stdout, "framing=");
			if (s.lineconfig & DAHDI_CONFIG_ESF) fprintf(stdout, "ESF");
			else if (s.lineconfig & DAHDI_CONFIG_D4) fprintf(stdout, "D4");
			else if (s.lineconfig & DAHDI_CONFIG_CCS) fprintf(stdout, "CCS");
			else if (s.lineconfig & DAHDI_CONFIG_CRC4) fprintf(stdout, "/CRC4");
			fprintf(stdout, "\n");
		} else {
			/* this is an analog span */
			fprintf(stdout, "type=analog\n");
			for (y = basechan; y < (basechan + s.totalchans); y++) {
				memset(&params, 0, sizeof(params));
				params.channo = y;
				if (ioctl(ctl, DAHDI_GET_PARAMS, &params)) {
					fprintf(stdout, "port=%d,unknown\n", y);
					continue;
				};
				fprintf(stdout, "port=%d,", y);
				switch (params.sigcap & (__DAHDI_SIG_FXO | __DAHDI_SIG_FXS)) {
				case __DAHDI_SIG_FXO:
					fprintf(stdout, "FXS");
					break;
				case __DAHDI_SIG_FXS:
					fprintf(stdout, "FXO");
					break;
				default:
					fprintf(stdout, "none");
				}
				if (params.sigcap & DAHDI_SIG_BROKEN)
					fprintf(stdout, " FAILED");
				fprintf(stdout, "\n");
			}
		}
	  
		basechan += s.totalchans;
	}

	exit(0);
}
