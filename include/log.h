/*
 *  Yaddns - Yet Another ddns client
 *  Copyright (C) 2009 Anthony Viallard <anthony.viallard@patatrac.info>
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

#ifndef _YADDNS_LOG_H_
#define _YADDNS_LOG_H_

#include <syslog.h>

#ifdef ENABLE_SYSLOG
#define EOL
#else
#define EOL "\n"
#endif

/*
 *  Log critical message
 */
#define log_critical(fmt, ...)					\
	do {							\
		log_it(LOG_CRITICAL, "%s::%s "fmt EOL,		\
			"critical", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log error message
 */
#define log_error(fmt, ...)					\
	do {							\
		log_it(LOG_ERR, "%s::%s "fmt EOL,		\
			"error", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log warning message
 */
#define log_warning(fmt, ...)					\
	do {							\
		log_it(LOG_WARNING, "%s::%s "fmt EOL,		\
			"warning", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log notice message
 */
#define log_notice(fmt, ...)					\
	do {							\
		log_it(LOG_NOTICE, "%s::%s "fmt EOL,		\
			"notice", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log info message
 */
#define log_info(fmt, ...)					\
	do {							\
		log_it(LOG_INFO, "%s::%s "fmt EOL,		\
			"info", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log debug message
 */
#ifdef DEBUG
#define log_debug(fmt, ...)					\
	do {							\
		log_it(LOG_DEBUG, "%s::%s "fmt EOL,		\
			"debug", __func__, ##__VA_ARGS__);	\
	} while (0)
#else
#define log_debug(fmt, ...)
#endif

/*
 * open/configure log system
 */
extern void log_open(void);

/*
 * close log system.
 */
extern void log_close( void );

/*
 * log the message
 */
extern void log_it(int priority, char const *format, ...);

#endif
