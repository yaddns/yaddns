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

#ifndef _DEFS_H_
#define _DEFS_H_

#include <string.h>
#include <sys/types.h>

/*
 * macro to log with layer
 */
enum ll_code {
  ll_fatal = 0,
  ll_err,
  ll_warn,
  ll_notice,
  ll_info,
  ll_debug,
};

#define LAYER_LOG_FATAL(msg, ...) layer->log(ll_fatal, msg, ##__VA_ARGS__)
#define LAYER_LOG_ERROR(msg, ...) layer->log(ll_err, msg, ##__VA_ARGS__)
#define LAYER_LOG_WARN(msg, ...) layer->log(ll_warn, msg, ##__VA_ARGS__)

#ifdef DEBUG
#define LAYER_LOG_DEBUG(msg, ...) layer->log(ll_debug,msg, ##__VA_ARGS__)
#else
#define LAYER_LOG_DEBUG(msg, ...) do {;} while(0)
#endif

#define LAYER_LOG_NOTICE(msg, ...) layer->log(ll_notice, msg, ##__VA_ARGS__)
#define LAYER_LOG_INFO(msg, ...) layer->log(ll_info, msg, ##__VA_ARGS__)

/*
 * code returned in service function
 */
enum status_code {
	/* update return code */
	up_sc_good = 0,
	up_sc_nochg,
	up_sc_abuse,
	up_sc_badsyntax,
	up_sc_badauth,
	up_sc_badhostname,
	up_sc_notyourhostname,
	up_sc_needcredit,
	up_sc_servererror,
	up_sc_generalerror,
	/* warning */
	wa_sc_warning, /* generic warning error */
	wa_sc_nowan,
	/* fatal error */
	fe_sc_error, /* generic fatal error */
	fe_sc_config,
};

typedef struct layer {
	int (*ctor) (void); /* construtor */
	int (*dtor) (void); /* destructor */
	int (*get_wan_ip) (char*, size_t); /* return the current wan ip address */
	int (*conf_get) (const char *, const char *, char *buf, size_t buf_size); /* return the wanted value in config */
	int (*conf_reload) (void); /* reload conf */
	int (*log) (int, const char *, ...) __attribute__ ( ( __format__( __printf__, 2, 3 ) ) ); /* log */
	int (*publish_status_code) (const char *, int); /* callback to show status of program to user with return code of layer and service */
} layer_t;

typedef struct service {
	const char* name;
	int (*ctor) (void);
	int (*dtor) (void);
	enum status_code (*update) (const char *); /* arg1: wan ip. */
} service_t;

extern layer_t *layer;
extern service_t *service;

#endif
