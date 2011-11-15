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
 * https://www.no-ip.com/integrate/request/
 */

#define DDNS_NAME "no-ip"
#define DDNS_HOST "dynupdate.no-ip.com"
#define DDNS_PORT 80

static int ddns_write(const struct cfg_account *cfg,
                      const char const *newwanip,
                      struct request_buff *buff);

static int ddns_read(struct request_buff *buff,
                     struct rc_report *report);

struct service noip_service = {
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
	{ .propcode = "badauth",
          .propcode_info = "Invalid username password combination.",
          .code = up_account_loginpass_error,
        },
	{ .propcode = "badagent",
          .propcode_info = "Client disabled.",
          .code = up_syntax_error,
        },
	{ .propcode = "good",
          .propcode_info = "DNS hostname update successful.",
          .code = up_success,
        },
	{ .propcode = "nochg",
          .propcode_info = "IP address is current, no update performed.",
          .code = up_success,
        },
	{ .propcode = "nohost",
          .propcode_info = "The hostname specified does not exist.",
          .code = up_account_hostname_error,
        },
	{ .propcode = "!donator",
          .propcode_info = "An update request was sent including a feature that is"
          " not available to that particular user such as offline options.",
          .code = up_account_error,
        },
	{ .propcode = "abuse",
          .propcode_info = "Username is blocked due to abuse.",
	  .code = up_account_abuse_error,
        },
	{ .propcode = "911",
          .propcode_info = "A fatal error on our side such as a database outage.",
          .code = up_server_error,
        },
	{ NULL,	NULL, 0, }
};

static int ddns_write(const struct cfg_account *cfg,
                      const char const *newwanip,
                      struct request_buff *buff)
{
	char buf[256];
	char *b64_loginpass = NULL;
	size_t b64_loginpass_size;
	int n;

	/* make the update packet */
	snprintf(buf, sizeof(buf), "%s:%s",
                 cfgstr_get(&(cfg->username)), cfgstr_get(&(cfg->passwd)));

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
                     cfgstr_get(&(cfg->hostname)),
                     newwanip,
                     b64_loginpass);
        if(n < 0)
        {
                log_error("Unable to write data buffer");
                return -1;
        }

	buff->data_size = (size_t)n;

	free(b64_loginpass);

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
