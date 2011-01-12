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
 * http://www.changeip.com/clients.asp
 */

#define DDNS_NAME "changeip"
#define DDNS_HOST "nic.changeip.com"
#define DDNS_PORT 80

static int ddns_write(const struct accountcfg cfg,
						const char const *newwanip,
						struct request_buff *buff);

static int ddns_read(struct request_buff *buff,
						struct upreply_report *report);

struct service changeip_service = {
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
	{ "200",
		"Successful Update",
		up_success,
		0, 0, 0 },
	{ NULL,	NULL, 0, 0, 0, 0 }
};

static int ddns_write(const struct accountcfg cfg,
						const char const *newwanip,
						struct request_buff *buff)
{
	char buf[256];
	char *b64_loginpass = NULL;
	size_t b64_loginpass_size;
	int n;

	/* make the update packet */
	snprintf(buf, sizeof(buf), "%s:%s", cfg.username, cfg.passwd);

	if (util_base64_encode(buf, &b64_loginpass, &b64_loginpass_size) != 0)
	{
		/* publish_error_status ?? */
		log_error("Unable to encode in base64");
		return -1;
	}

	n = snprintf(buff->data, sizeof(buff->data),
					"GET /nic/update?hostname=%s"
					"&myip=%s"
					" HTTP/1.0\r\n"
					"Host: " DDNS_HOST "\r\n"
					"Authorization: Basic %s\r\n"
					"User-Agent: " PACKAGE "/" VERSION "\r\n"
					"Connection: close\r\n"
					"Pragma: no-cache\r\n\r\n",
					cfg.hostname,
					newwanip,
					b64_loginpass);

	buff->data_size = n;

	free(b64_loginpass);

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

	if(strstr(buff->data, "HTTP/1.1 200 Successful Update") ||
		strstr(buff->data, "HTTP/1.0 200 Successful Update"))
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
	else if (strstr(buff->data, "401 Access Denied"))
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
