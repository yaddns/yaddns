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

#include "../service.h"
#include "../request.h"
#include "../util.h"
#include "../log.h"

/*
 * http://www.sitelutions.com/help/dynamic_dns_clients#updatespec
 */

#define DDNS_NAME "sitelutions"
#define DDNS_HOST "www.sitelutions.com"
#define DDNS_PORT 80

static int ddns_write(const struct cfg_account *cfg,
                      const char const *newwanip,
                      struct request_buff *buff);

static int ddns_read(struct request_buff *buff,
                     struct rc_report *report);

struct service sitelutions_service = {
	.name = DDNS_NAME,
	.ipserv = DDNS_HOST,
	.portserv = DDNS_PORT,
	.make_query = ddns_write,
	.read_resp = ddns_read
};

static struct {
	const char *propcode;
	const char *propcode_info;
	int code;
} rc_map[] = {
	{ .propcode = "success",
          .propcode_info = "Record has been updated successfully.",
          .code = up_success,
        },
	{ .propcode = "noauth",
          .propcode_info = "User authentication failed (user e-mail address or password invalid).",
          .code = up_account_loginpass_error,
        },
	{ .propcode = "failure (invalid ip)",
          .propcode_info = "You provided an invalid IP address (or didn't provide one at all).",
          .code = up_syntax_error,
        },
	{ .propcode = "failure (invalid ttl)",
          .propcode_info = "You provided an invalid time-to-live.",
          .code = up_syntax_error,
        },
	{ .propcode = "failure (no record)",
          .propcode_info = "You failed to provide a record ID to update.",
          .code = up_syntax_error,
        },
	{ .propcode = "failure (not owner)",
          .propcode_info = "You are not the owner of the record you are trying to update.",
          .code = up_account_hostname_error,
        },
	{ .propcode = "failure (dberror)",
          .propcode_info = "A database error of some sort occurred. This is an unusual and unlikely error. Contact support.",
          .code = up_server_error,
        },
	{ NULL,	NULL, 0, }
};

static int ddns_write(const struct cfg_account *cfg,
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
                     cfgstr_get(&(cfg->hostname)),
                     cfgstr_get(&(cfg->username)),
                     cfgstr_get(&(cfg->passwd)),
                     newwanip);
        if(n < 0)
        {
                log_error("Unable to write data buffer");
                return -1;
        }

	buff->data_size = (size_t)n;

	return 0;
}

static int ddns_read(struct request_buff *buff,
                     struct rc_report *report)
{
        char *str = NULL, *token = NULL, *saveptr = NULL;
	int found = 0;
	int n = 0;

        for(str = buff->data;; str = NULL)
        {
               token = strtok_r(str, "\n", &saveptr);
               if(token == NULL)
               {
                       break;
               }

               for (n = 0; rc_map[n].propcode != NULL; ++n)
               {
                       if (strstr(token, rc_map[n].propcode) != NULL)
                       {
                               report->code = rc_map[n].code;

                               snprintf(report->proprio_return,
                                        sizeof(report->proprio_return),
                                        "%s", rc_map[n].propcode);

                               snprintf(report->proprio_return_info,
                                        sizeof(report->proprio_return_info),
                                        "%s", rc_map[n].propcode_info);

                               found = 1;
                               break;
                       }
               }
        }

        if(!found)
        {
                log_error("Unknown return message received.");

                report->code = up_unknown_error;

                snprintf(report->proprio_return,
                         sizeof(report->proprio_return),
                         "unknown");

                snprintf(report->proprio_return_info,
                         sizeof(report->proprio_return_info),
                         "Unknown return message received");
        }

	return 0;
}
