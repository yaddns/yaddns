/*
 *  Yaddns - Yet Another ddns client
 *  Copyright (C) 2008 Anthony Viallard <anthony.viallard@patatrac.info>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"

#include <stdlib.h>
#include <sys/sysinfo.h>

int util_base64_encode(const char *src, char **output, size_t *output_size)
{
	static char tbl[64] = {
		'A','B','C','D','E','F','G','H',
		'I','J','K','L','M','N','O','P',
		'Q','R','S','T','U','V','W','X',
		'Y','Z','a','b','c','d','e','f',
		'g','h','i','j','k','l','m','n',
		'o','p','q','r','s','t','u','v',
		'w','x','y','z','0','1','2','3',  
		'4','5','6','7','8','9','+','/'
	};
	int len, i;
	unsigned char *p = NULL;
       
	len = strlen(src);
	
	*output_size = 4 * ((len + 2) / 3) + 1;
	*output = (char *)malloc(*output_size);
	
	p = (unsigned char *)(*output);
	/* Transform the 3x8 bits to 4x6 bits, as required by base64. */
	for (i = 0; i < len; i += 3)
	{
		*p++ = tbl[src[0] >> 2];
		*p++ = tbl[((src[0] & 3) << 4) + (src[1] >> 4)];
		*p++ = tbl[((src[1] & 0xf) << 2) + (src[2] >> 6)];
		*p++ = tbl[src[2] & 0x3f];
		src += 3;
	}
	
	/* Pad the result if necessary... */
	if (i == len + 1)
	{
		*(p - 1) = '=';
	}
	else if (i == len + 2)
	{
		*(p - 1) = *(p - 2) = '=';
	}
	
	/* ...and zero-teminate it.  */
	*p = '\0';
	
	return 0;
}

void util_getuptime(struct timeval *tv)
{
        struct sysinfo si;

	sysinfo( &si );

	tv->tv_sec = si.uptime;
	tv->tv_usec = 0;
}
