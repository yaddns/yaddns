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

#ifndef _YADDNS_UTIL_H_
#define _YADDNS_UTIL_H_

#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>

#define UNUSED(x) ( (void)(x) )

#define ARRAY_SIZE(x) (sizeof (x) / sizeof ((x) [0]))

#define MIN(x, y)                               \
        ({                                      \
                typeof(x) _x = (x);             \
                typeof(y) _y = (y);             \
                (void) (&_x == &_y);            \
                _x < _y ? _x : _y;              \
        })

#define MAX(x, y)                               \
        ({                                      \
                typeof(x) _x = (x);             \
                typeof(y) _y = (y);             \
                (void) (&_x == &_y);            \
                _x > _y ? _x : _y;              \
        })

/*
 * Encode src txt in base64 and fill malloced ouput
 *
 * @return 0 if success, -1 otherwise
 */
extern int util_base64_encode(const char *src, char **output, size_t *output_size);

/*
 * Get system uptime in seconds
 */
extern time_t util_getuptime(void);

/*
 * Get ip address of an interface
 */
int util_getifaddr(const char *ifname, struct in_addr *addr);

/*
 * Allocate new string with trimming spaces, tabs, ", ' and \n in input string
 */
char *strdup_trim(const char *s);

/*
 * Return integer contened in a string
 */
long strtol_safe(char const *buf, long def);

#endif
