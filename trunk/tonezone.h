/*
 * BSD Telephony Of Mexico "Tormenta" Tone Zone Support 2/22/01
 * 
 * Working with the "Tormenta ISA" Card 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 * Primary Author: Mark Spencer <markster@linux-support.net>
 *
 */

#ifndef _TONEZONE_H
#define _TONEZONE_H

#include <dahdi/user.h>

struct tone_zone_sound {
	int toneid;
	char data[256];				/* Actual zone description */
	/* Description is a series of tones of the format:
	   [!]freq1[+freq2][/time] separated by commas.  There
	   are no spaces.  The sequence is repeated back to the 
	   first tone description not preceeded by !.  time is
	   specified in milliseconds */
};

struct tone_zone {
	int zone;				/* Zone number */
	char country[10];			/* Country code */
	char description[40];			/* Description */
	int ringcadence[DAHDI_MAX_CADENCE];	/* Ring cadence */
	struct tone_zone_sound tones[DAHDI_TONE_MAX];
	int dtmf_high_level;			/* Power level of high frequency component
						   of DTMF, expressed in dBm0. */
	int dtmf_low_level;			/* Power level of low frequency component
						   of DTMF, expressed in dBm0. */
	int mfr1_level;				/* Power level of MFR1, expressed in dBm0. */
	int mfr2_level;				/* Power level of MFR2, expressed in dBm0. */
};

extern struct tone_zone builtin_zones[];

/* Register a given two-letter tone zone if we can */
int tone_zone_register(int fd, char *country);

/* Register a given two-letter tone zone if we can */
int tone_zone_register_zone(int fd, struct tone_zone *z);

/* Retrieve a raw tone zone structure */
struct tone_zone *tone_zone_find(char *country);

/* Retrieve a raw tone zone structure by id instead of country*/
struct tone_zone *tone_zone_find_by_num(int id);

/* Retrieve a string name for a given tone id */
char *tone_zone_tone_name(int id);

/* Set a given file descriptor into a given country -- USE THIS
   INTERFACE INSTEAD OF THE IOCTL ITSELF.  Auto-loads tone
   zone if necessary */
int tone_zone_set_zone(int fd, char *country);

/* Get the current tone zone */
int tone_zone_get_zone(int fd);

/* Play a given tone, loading tone zone automatically
   if necessary */
int tone_zone_play_tone(int fd, int toneid);

#endif
