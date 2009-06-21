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

#include "service.h"
#include "util.h"
#include "log.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/*
 * This dyndns client is inspired by updatedd dyndns service client
 *  http://freshmeat.net/projects/updatedd/
 */

#define DYNDNS_HOST "members.dyndns.org"
#define DYNDNS_PORT 80

static struct {
	const char *code;
	const char *text;
	int rc;
} return_server_codes[] = {
	{ "badauth",	"Bad authorization (username or password).",		-1 },
	{ "badsys",	"The system parameter given was not valid.",		-1 },
	{ "badagent",	"The useragent your client sent has been blocked "
          "at the access level.",						-1
	},
	{ "good",	"Update good and successful, IP updated.",		-1 },
	{ "nochg",	"No changes, update considered abusive.",		-1 },
	{ "nohost",	"The hostname specified does not exist.",		-1 },
	{ "!donator",	"The offline setting was set, when the user is "
          "not a donator.",							-1
	},
	{ "!yours",	"The hostname specified exists, but not under "
          "the username currently being used.",					-1
	},
	{ "abuse",	"The hostname specified is blocked for abuse",		-1 },
	{ "notfqdn",	"No hosts are given.",					-1 },
	{ "numhost",	"Too many or too few hosts found.",			-1 },
	{ "dnserr",	"DNS error encountered.",				-1 },
	{ "911",	"911 error encountered.",				-1 },
	{ NULL,		NULL,							0 }
};

static int dyndns_write(const struct servicecfg cfg, 
			  const char const *newwanip, 
			  char *buffer, 
			  size_t buffer_size);
static int dyndns_read(char *buffer, 
		       struct upreply_report *report);

struct service dyndns_service = {
	.name = "dyndns",
	.make_up_query = dyndns_write,
	.read_up_resp = dyndns_read,
};

static int dyndns_write(const struct servicecfg cfg, 
			const char const *newwanip, 
			char *buffer, 
			size_t buffer_size)
{
	char buf[256];
	char *b64_loginpass = NULL;
	size_t b64_loginpass_size;

	/* make the update packet */
	snprintf(buf, sizeof(buf), "%s:%s", cfg.username, cfg.passwd);
	
	if(util_base64_encode(buf, &b64_loginpass, &b64_loginpass_size) != 0)
	{
		/* publish_error_status ?? */
		log_error("Unable to encode in base64");
		return -1;
	}

	snprintf(buffer, buffer_size, 
		 "GET /nic/update?system=dyndns&hostname=%s&wildcard=OFF"
		 "&myip=%s"
		 "&backmx=NO&offline=NO"
		 " HTTP/1.1\r\n"
		 "Host: " DYNDNS_HOST "\r\n"
		 "Authorization: Basic %s\r\n"
		 "User-Agent: " D_NAME "/" D_VERSION " - " D_INFO "\r\n"
		 "Connection: close\r\n"
		 "Pragma: no-cache\r\n\r\n",
		 cfg.hostname,
		 newwanip,
		 b64_loginpass);
	
	free(b64_loginpass);
	
	return 0;
}

static int dyndns_read(char *buffer, 
		       struct upreply_report *report)
{
	int ret = 0;
	char *ptr = NULL;
	int f = 0;
	int n = 0;
	
	report->code = -1;
	
	if(strstr(buffer, "HTTP/1.1 200 OK") ||
	   strstr(buffer, "HTTP/1.0 200 OK")) 
	{
		(void)strtok(buffer, "\n");
		while(!f && (ptr = strtok(NULL, "\n")) != NULL) 
		{
			for(n = 0; return_server_codes[n].code != NULL; n++) 
			{
				if(strstr(ptr, return_server_codes[n].code)) 
				{
					report->code = return_server_codes[n].rc;
					f = 1;
					break;
				}
			}
		}
	}
	else if(strstr(buffer, "401 Authorization Required")) 
	{
		report->code = -1;
	}
	else 
	{
		ret = -1;
	}

	return ret;
}
