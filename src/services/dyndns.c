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

#include "defs.h"
#include "util.h"

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
	enum status_code rc;
} return_server_codes[] = {
	{ "badauth",	"Bad authorization (username or password).",		up_sc_badauth },
	{ "badsys",	"The system parameter given was not valid.",		up_sc_generalerror },
	{ "badagent",	"The useragent your client sent has been blocked "
          "at the access level.",						up_sc_generalerror
	},
	{ "good",	"Update good and successful, IP updated.",		up_sc_good },
	{ "nochg",	"No changes, update considered abusive.",		up_sc_nochg },
	{ "nohost",	"The hostname specified does not exist.",		up_sc_badhostname },
	{ "!donator",	"The offline setting was set, when the user is "
          "not a donator.",							up_sc_needcredit
	},
	{ "!yours",	"The hostname specified exists, but not under "
          "the username currently being used.",					up_sc_notyourhostname
	},
	{ "abuse",	"The hostname specified is blocked for abuse",		up_sc_abuse },
	{ "notfqdn",	"No hosts are given.",					up_sc_badhostname },
	{ "numhost",	"Too many or too few hosts found.",			up_sc_badhostname },
	{ "dnserr",	"DNS error encountered.",				up_sc_servererror },
	{ "911",	"911 error encountered.",				up_sc_servererror },
	{ NULL,		NULL,							0 }
};

static char _internal_buffer[1024];

static int dyndns_ctor(void);
static int dyndns_dtor(void);
static int dyndns_update(const char *wan_ip);

static int dyndns_connect(void);
static int dyndns_disconnect(int s);

struct service dyndns_service = {
	.name = "dyndns",
	.ctor = dyndns_ctor,
	.dtor = dyndns_dtor,
	.update = dyndns_update,
};

static int dyndns_ctor(void)
{
	return 0;
}

static int dyndns_dtor(void)
{
	return 0;
}

static int dyndns_update(const char *wan_ip)
{
	int s, ret, n;
	char login[128], password[128], hostname[256];
	char buf[256];
	char *b64_loginpass = NULL, *ptr = NULL;
	size_t b64_loginpass_size;
	
	/* get configuration */
	if(layer->conf_get("dyndns", "username", login, sizeof(login)) == -1) 
	{
		LAYER_LOG_ERROR("Unable to get login - Check your conf");
		return fe_sc_config;
	}

	if(layer->conf_get("dyndns", "password", password, sizeof(password)) == -1) 
	{
		LAYER_LOG_ERROR("Unable to get password - Check your conf");
		return fe_sc_config;
	}

	if(layer->conf_get("dyndns", "hostname", hostname, sizeof(hostname)) == -1) 
	{
		LAYER_LOG_ERROR("Unable to get hostname - Check your conf");
		return fe_sc_config;
	}

	/* make the update packet */
	snprintf(buf, sizeof(buf), "%s:%s", login, password);
	
	if(util_base64_encode(buf, &b64_loginpass, &b64_loginpass_size) != 0)
	{
		/* publish_error_status ?? */
		LAYER_LOG_ERROR("Unable to encode in base64");
		return fe_sc_error;
	}

	snprintf(_internal_buffer, sizeof(_internal_buffer), 
		 "GET /nic/update?system=dyndns&hostname=%s&wildcard=OFF"
		 "&myip=%s"
		 "&backmx=NO&offline=NO"
		 " HTTP/1.1\r\n"
		 "Host: " DYNDNS_HOST "\r\n"
		 "Authorization: Basic %s\r\n"
		 "User-Agent: " D_NAME "/" D_VERSION " - " D_INFO "\r\n"
		 "Connection: close\r\n"
		 "Pragma: no-cache\r\n\r\n",
		 hostname,
		 wan_ip,
		 b64_loginpass);
	
	free(b64_loginpass);
	
	printf("PACKET: %s", _internal_buffer);
	
	/* connect */
	s = dyndns_connect();
	if(s == -1)
	{
		/* fatal error */
		LAYER_LOG_ERROR("Unable to connect to server");
		return wa_sc_nowan; /* TODO: NEED TO IMPROVE */
	}
	
	if(write(s, _internal_buffer, strlen(_internal_buffer)) == -1) 
	{
		LAYER_LOG_ERROR("Write error ! %m");
		dyndns_disconnect(s);
		return wa_sc_nowan; /* TODO: NEED TO IMPROVE */
	}
	
	/* check return */
	if((ret = read(s, _internal_buffer, sizeof(_internal_buffer) - 1)) < 0) 
	{
		LAYER_LOG_ERROR("Read error ! %m");
		dyndns_disconnect(s);
		return wa_sc_nowan; /* TODO: NEED TO IMPROVE */
	}
        
	_internal_buffer[ret] = '\0';
	printf("Server message : \n %s\n", _internal_buffer);

	if(strstr(_internal_buffer, "HTTP/1.1 200 OK") ||
	   strstr(_internal_buffer, "HTTP/1.0 200 OK")) 
	{
		ret = -1;
		(void)strtok(_internal_buffer, "\n");
		while(ret == -1 && (ptr = strtok(NULL, "\n")) != NULL) 
		{
			for(n = 0; return_server_codes[n].code != NULL; n++) 
			{
				if(strstr(ptr, return_server_codes[n].code)) 
				{
					ret = return_server_codes[n].rc;
					break;
				}
			}
		}
	}
	else if(strstr(_internal_buffer, "401 Authorization Required")) 
	{
		ret = up_sc_badauth;
	}
	else 
	{
		ret = up_sc_servererror;
	}
	
	/* disconnect */
	dyndns_disconnect(s);

	return ret;
}

static int dyndns_connect(void)
{
	struct sockaddr_in addr;
        struct hostent *host;
        int s;

        if((host = gethostbyname(DYNDNS_HOST)) == NULL) 
	{
		LAYER_LOG_ERROR("gethostbyname() failed ! %s %d", hstrerror(h_errno), h_errno);
		return -1;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(DYNDNS_PORT);
        addr.sin_addr = *(struct in_addr*)host->h_addr;

        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s == -1) 
	{
                LAYER_LOG_ERROR("socket() failed %m");
                return -1;
        }

        if(connect(s, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)) == -1) 
	{
		LAYER_LOG_ERROR("connect() failed %m");
                return -1;
        }

        return s;
}

static int dyndns_disconnect(int s)
{
	int ret;

	if((ret = close(s)) != 0)
	{
		LAYER_LOG_ERROR("Warning, close failed %m");
	}

	return ret;
}
