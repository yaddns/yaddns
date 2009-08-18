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
	int unified_rc;
        int lock;
        int freeze;
        int freezetime;
} rc_map[] = {
	{ "badauth", 
          "Bad authorization (username or password).", 
          up_account_loginpass_error, 
          1, 0, 0 },
	{ "badsys", 
          "The system parameter given was not valid.",
          up_syntax_error,
          1, 0, 0 },
	{ "badagent", 
          "The useragent your client sent has been  blocked at the access level.",
          up_syntax_error,
          1, 0, 0 },
	{ "good", 
          "Update good and successful, IP updated.", 
          up_success,
          0, 0, 0 },
	{ "nochg", 
          "No changes. IP updated.", 
          up_success,
          0, 0, 0 },
	{ "nohost",
          "The hostname specified does not exist.",
          up_account_hostname_error,
          1, 0, 0 },
	{ "!donator", 
          "The offline setting was set, when the user is not a donator.",
          up_account_error,
          1, 0, 0 },
	{ "!yours", 
          "The hostname specified exists, but not under the username currently being used.",
          up_account_hostname_error,
          1, 0, 0 },
	{ "abuse", 
          "The hostname specified is blocked for abuse",
	  up_account_abuse_error,
          1, 0, 0 },
	{ "notfqdn",  
          "No hosts are given.",
          up_account_hostname_error,
          1, 0, 0 },
	{ "numhost",  
          "Too many or too few hosts found.",
          up_account_hostname_error,
          1, 0, 0 },
	{ "dnserr",   
          "DNS error encountered.",
          up_server_error,
          0, 1, 3600 },
	{ "911", 
          "911 error encountered.",
          up_server_error,
          0, 1, 3600 },
	{ NULL,	NULL, 0, 0, 0, 0 }
};

static int dyndns_write(const struct accountcfg cfg, 
			  const char const *newwanip, 
			  char *buffer, 
			  size_t buffer_size);
static int dyndns_read(char *buffer, 
		       struct upreply_report *report);

struct service dyndns_service = {
	.name = "dyndns",
        .ipserv = DYNDNS_HOST,
        .portserv = DYNDNS_PORT,
	.make_up_query = dyndns_write,
	.read_up_resp = dyndns_read,
};

static int dyndns_write(const struct accountcfg cfg, 
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
	
	report->code = up_unknown_error;
	
	if(strstr(buffer, "HTTP/1.1 200 OK") ||
	   strstr(buffer, "HTTP/1.0 200 OK")) 
	{
		(void)strtok(buffer, "\n");
		while(!f && (ptr = strtok(NULL, "\n")) != NULL) 
		{
			for(n = 0; rc_map[n].code != NULL; n++) 
			{
				if(strstr(ptr, rc_map[n].code)) 
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
                                        report->rcmd_freezetime 
                                                = rc_map[n].freezetime;
                                        
					f = 1;
					break;
				}
			}
		}

                if(!f)
                {
                        log_notice("Unknown return message received.");
                        report->rcmd_lock = 1;
                }
	}
	else if(strstr(buffer, "401 Authorization Required")) 
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
