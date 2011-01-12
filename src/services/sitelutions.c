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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "service.h"
#include "request.h"
#include "util.h"
#include "log.h"

/*
 * http://www.sitelutions.com/help/dynamic_dns_clients#updatespec
 */

#define DDNS_NAME "sitelutions"
#define DDNS_HOST "www.sitelutions.com"
#define DDNS_PORT 80

static int ddns_write(const struct accountcfg cfg,
						const char const *newwanip,
						struct request_buff *buff);

static int ddns_read(struct request_buff *buff,
						struct upreply_report *report);

struct service sitelutions_service = {
	.name = DDNS_NAME,
	.ipserv = DDNS_HOST,
	.portserv = DDNS_PORT,
	.make_query = ddns_write,
	.read_resp = ddns_read
};

static struct {
	const char *code;
	const char *text;
	int unified_rc;
	int lock;
	int freeze;
	int freezetime;
} rc_map[] = {
	{ "success",
		"Record has been updated successfully.",
		up_success,
		0, 0, 0 },
	{ "noauth",
		"User authentication failed (user e-mail address or password invalid).",
		up_account_loginpass_error,
		1, 0, 0 },
	{ "failure (invalid ip)",
		"You provided an invalid IP address (or didn't provide one at all).",
		up_syntax_error,
		1, 0, 0 },
	{ "failure (invalid ttl)",
		"You provided an invalid time-to-live.",
		up_syntax_error,
		1, 0, 0 },
	{ "failure (no record)",
		"You failed to provide a record ID to update.",
		up_syntax_error,
		1, 0, 0 },
	{ "failure (not owner)",
		"You are not the owner of the record you are trying to update.",
		up_account_hostname_error,
		1, 0, 0 },
	{ "failure (dberror)",
		"A database error of some sort occurred. This is an unusual and unlikely error. Contact support.",
		up_server_error,
		0, 1, 3600 },
	{ NULL,	NULL, 0, 0, 0, 0 }
};

static int ddns_write(const struct accountcfg cfg,
						const char const *newwanip,
						struct request_buff *buff)
{
	int n;

	/* make the update packet */
	n = snprintf(buff->data, sizeof(buff->data),
					"GET /dnsup?id=%s"
					"&user=%s"
					"&pass=%s"
					"&ip=%s"
					" HTTP/1.0\r\n"
					"Host: " DDNS_HOST "\r\n"
					"User-Agent: " PACKAGE "/" VERSION "\r\n"
					"Connection: close\r\n"
					"Pragma: no-cache\r\n\r\n",
					cfg.hostname,
					cfg.username,
					cfg.passwd,
					newwanip);

	buff->data_size = n;

	return 0;
}

static int ddns_read(struct request_buff *buff,
						struct upreply_report *report)
{
	int ret = 0;
	char *ptr = NULL;
	int f = 0;
	int n = 0;

	report->code = up_unknown_error;

	if(strstr(buff->data, "HTTP/1.1 200 OK") ||
		strstr(buff->data, "HTTP/1.0 200 OK"))
	{
		(void) strtok(buff->data, "\n");
		while (!f && (ptr = strtok(NULL, "\n")) != NULL)
		{
			for (n = 0; rc_map[n].code != NULL; n++)
			{
				if (strstr(ptr, rc_map[n].code))
				{
					report->code = rc_map[n].unified_rc;

					snprintf(report->custom_rc,
								sizeof(report->custom_rc),
								"%s",
								rc_map[n].code);

					snprintf(report->custom_rc_text,
								sizeof(report->custom_rc_text),
								"%s",
								rc_map[n].text);

					report->rcmd_lock = rc_map[n].lock;
					report->rcmd_freeze = rc_map[n].freeze;
					report->rcmd_freezetime = rc_map[n].freezetime;

					f = 1;
					break;
				}
			}
		}

		if (!f)
		{
			log_notice("Unknown return message received.");
			report->rcmd_lock = 1;
		}
	}
	else if (strstr(buff->data, "401 Authorization Required"))
	{
		report->code = up_account_error;
		report->rcmd_freeze = 1;
		report->rcmd_freezetime = 3600;
	}
	else
	{
		report->code = up_server_error;
		report->rcmd_freeze = 1;
		report->rcmd_freezetime = 3600;
	}

	return ret;
}
