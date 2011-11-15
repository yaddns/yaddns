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

#ifndef _YADDNS_SERVICE_H_
#define _YADDNS_SERVICE_H_

#include <string.h>

#include "list.h"
#include "config.h"
#include "request.h"

struct rc_report {
	enum {
		up_success,
		up_unknown_error,
		up_syntax_error,
		up_account_error,
		up_account_loginpass_error,
		up_account_hostname_error,
		up_account_abuse_error,
		up_server_error,
	} code;
	char proprio_return[16];      /* proprietary return code */
	char proprio_return_info[64]; /* explanation about proprio return */
};

struct service {
	const char const *name;
	const char const *ipserv;
        short unsigned int portserv;
	int (*ctor) (void);
	int (*dtor) (void);
	int (*make_query) (const struct cfg_account *cfg,
                           const char const *newwanip,
                           struct request_buff *buff);
	int (*read_resp) (struct request_buff *buff,
                          struct rc_report *report);
	struct list_head list;
};

#endif
