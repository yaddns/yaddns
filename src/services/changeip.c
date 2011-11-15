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
 * http://www.changeip.com/clients.asp
 */

#define DDNS_NAME "changeip"
#define DDNS_HOST "nic.changeip.com"
#define DDNS_PORT 80

static int ddns_write(const struct cfg_account *cfg,
                      const char const *newwanip,
                      struct request_buff *buff);

static int ddns_read(struct request_buff *buff,
                     struct rc_report *report);

struct service changeip_service = {
	.name = DDNS_NAME,
	.ipserv = DDNS_HOST,
	.portserv = DDNS_PORT,
	.make_query = ddns_write,
	.read_resp = ddns_read
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
        if(strstr(buff->data, "200 Successful Update"))
	{
                report->code = up_success;

                snprintf(report->proprio_return,
                         sizeof(report->proprio_return),
                         "good");

                snprintf(report->proprio_return_info,
                         sizeof(report->proprio_return_info),
                         "Update good and successful, IP updated.");
        }
        else if(strstr(buff->data, "401 Access Denied")
                || strstr(buff->data, "401 Unauthorized"))
        {
                report->code = up_account_error;

                snprintf(report->proprio_return,
                         sizeof(report->proprio_return),
                         "badauth");

                snprintf(report->proprio_return_info,
                         sizeof(report->proprio_return_info),
                         "Bad authorization (username or password).");
        }
        else
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
