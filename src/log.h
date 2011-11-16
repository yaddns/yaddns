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
#include <stdio.h>

#include "config.h"

/* colors for VT102 terminal */
#if defined(ENABLE_LOG_COLOR)
 #define COLOR_ESCAPE		"\033"

 #define COLOR_RESET		COLOR_ESCAPE "[0m"

 #define COLOR_BLACK(txt)	COLOR_ESCAPE "[0;30m" txt COLOR_RESET
 #define COLOR_RED(txt)		COLOR_ESCAPE "[0;31m" txt COLOR_RESET
 #define COLOR_GREEN(txt)	COLOR_ESCAPE "[0;32m" txt COLOR_RESET
 #define COLOR_BROWN(txt)	COLOR_ESCAPE "[0;33m" txt COLOR_RESET
 #define COLOR_BLUE(txt)	COLOR_ESCAPE "[0;34m" txt COLOR_RESET
 #define COLOR_PURPLE(txt)	COLOR_ESCAPE "[0;35m" txt COLOR_RESET
 #define COLOR_CYAN(txt)	COLOR_ESCAPE "[0;36m" txt COLOR_RESET
 #define COLOR_GRAY(txt)	COLOR_ESCAPE "[0;37m" txt COLOR_RESET

 #define COLOR_DARK_GRAY(txt)	COLOR_ESCAPE "[1;30m" txt COLOR_RESET
 #define COLOR_LIGHT_RED(txt)	COLOR_ESCAPE "[1;31m" txt COLOR_RESET
 #define COLOR_LIGHT_GREEN(txt)	COLOR_ESCAPE "[1;32m" txt COLOR_RESET
 #define COLOR_YELLOW(txt)	COLOR_ESCAPE "[1;33m" txt COLOR_RESET
 #define COLOR_LIGHT_BLUE(txt)	COLOR_ESCAPE "[1;34m" txt COLOR_RESET
 #define COLOR_LIGHT_PURPLE(txt)COLOR_ESCAPE "[1;35m" txt COLOR_RESET
 #define COLOR_LIGHT_CYAN(txt)	COLOR_ESCAPE "[1;36m" txt COLOR_RESET
 #define COLOR_WHITE(txt)	COLOR_ESCAPE "[1;37m" txt COLOR_RESET
#else
 #define COLOR_BLACK(txt) txt
 #define COLOR_RED(txt) txt
 #define COLOR_GREEN(txt) txt
 #define COLOR_BROWN(txt) txt
 #define COLOR_BLUE(txt) txt
 #define COLOR_PURPLE(txt) txt
 #define COLOR_CYAN(txt) txt
 #define COLOR_GRAY(txt) txt

 #define COLOR_DARK_GRAY(txt) txt
 #define COLOR_LIGHT_RED(txt) txt
 #define COLOR_LIGHT_GREEN(txt) txt
 #define COLOR_YELLOW(txt) txt
 #define COLOR_LIGHT_BLUE(txt) txt
 #define COLOR_LIGHT_PURPLE(txt) txt
 #define COLOR_LIGHT_CYAN(txt) txt
 #define COLOR_WHITE(txt) txt
#endif

/*
 *  Log critical message
 */
#define log_critical(fmt, ...)                                          \
        log_it(LOG_CRIT, COLOR_RED("- CRITICAL - (%s) " fmt) "\n",      \
               __func__, ##__VA_ARGS__)

/*
 *  Log error message
 */
#define log_error(fmt, ...)                                             \
        log_it(LOG_ERR, COLOR_RED("- ERROR - " fmt) "\n",               \
               ##__VA_ARGS__)

/*
 *  Log warning message
 */
#define log_warning(fmt, ...)                                           \
        log_it(LOG_WARNING, COLOR_PURPLE("- WARNING - " fmt) "\n",      \
               ##__VA_ARGS__)

/*
 *  Log notice message
 */
#define log_notice(fmt, ...)					\
        log_it(LOG_NOTICE, fmt "\n", ##__VA_ARGS__)

/*
 *  Log info message
 */
#define log_info(fmt, ...)					\
        log_it(LOG_INFO, fmt "\n", ##__VA_ARGS__)

/*
 *  Log debug message
 */
#if defined(ENABLE_LOG_DEBUG)
#define log_debug(fmt, ...)                                             \
        log_it(LOG_DEBUG, COLOR_BLUE("- DEBUG - (%s) " fmt) "\n",       \
               __func__, ##__VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

/*
 * open/configure log system
 */
extern void log_open( const struct cfg *cfg );

/*
 * close log system.
 */
extern void log_close( void );

/*
 * write log
 */
extern void log_it(int priority, char const *format, ...);

#endif
