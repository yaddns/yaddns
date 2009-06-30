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

#define EOL "\n"

/* colors for VT102 terminal */
#define COLOR_ESCAPE		"\033"

#define COLOR_RESET		COLOR_ESCAPE "[0m"

#define COLOR_BLACK(txt)	COLOR_ESCAPE "[0;30m" txt COLOR_RESET
#define COLOR_RED(txt)		COLOR_ESCAPE "[0;31m" txt COLOR_RESET
#define COLOR_GREEN(txt)	COLOR_ESCAPE "[0;32m" txt COLOR_RESET
#define COLOR_BROWN(txt)	COLOR_ESCAPE "[0;33m" txt COLOR_RESET
#define COLOR_BLUE(txt)		COLOR_ESCAPE "[0;34m" txt COLOR_RESET
#define COLOR_PURPLE(txt)	COLOR_ESCAPE "[0;35m" txt COLOR_RESET
#define COLOR_CYAN(txt)		COLOR_ESCAPE "[0;36m" txt COLOR_RESET
#define COLOR_GRAY(txt)		COLOR_ESCAPE "[0;37m" txt COLOR_RESET

#define COLOR_DARK_GRAY(txt)	COLOR_ESCAPE "[1;30m" txt COLOR_RESET
#define COLOR_LIGHT_RED(txt)	COLOR_ESCAPE "[1;31m" txt COLOR_RESET
#define COLOR_LIGHT_GREEN(txt)	COLOR_ESCAPE "[1;32m" txt COLOR_RESET
#define COLOR_YELLOW(txt)	COLOR_ESCAPE "[1;33m" txt COLOR_RESET
#define COLOR_LIGHT_BLUE(txt)	COLOR_ESCAPE "[1;34m" txt COLOR_RESET
#define COLOR_LIGHT_PURPLE(txt)	COLOR_ESCAPE "[1;35m" txt COLOR_RESET
#define COLOR_LIGHT_CYAN(txt)	COLOR_ESCAPE "[1;36m" txt COLOR_RESET
#define COLOR_WHITE(txt)	COLOR_ESCAPE "[1;37m" txt COLOR_RESET

/*
 *  Log critical message
 */
#define log_critical(fmt, ...)                                          \
	do {                                                            \
		log_it(LOG_CRIT, COLOR_LIGHT_RED("%s %s - " fmt EOL), \
                       "--- CRITICAL ---", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log error message
 */
#define log_error(fmt, ...)                                             \
	do {                                                            \
		log_it(LOG_ERR, COLOR_RED("%s %s - " fmt EOL),		\
			"--- ERROR ---", __func__, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log warning message
 */
#define log_warning(fmt, ...)                                           \
	do {                                                            \
                log_it(LOG_WARNING, COLOR_PURPLE("%s %s - " fmt EOL),   \
                       "--- warning ---", __func__, ##__VA_ARGS__);     \
	} while (0)

/*
 *  Log notice message
 */
#define log_notice(fmt, ...)					\
	do {							\
		log_it(LOG_NOTICE, fmt EOL, ##__VA_ARGS__);     \
	} while (0)

/*
 *  Log info message
 */
#define log_info(fmt, ...)					\
	do {							\
		log_it(LOG_INFO, fmt EOL, ##__VA_ARGS__);	\
	} while (0)

/*
 *  Log debug message
 */
#ifdef DEBUG
#define log_debug(fmt, ...)                                             \
	do {                                                            \
                log_it(LOG_DEBUG, COLOR_BLUE("%s %s - " fmt EOL),       \
                       "--- DEBUG ---", __func__, ##__VA_ARGS__);	\
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
