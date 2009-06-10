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
#include "event.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <signal.h>

static const char *a_errmsg[] = {
	"good",
	"nochg",
	"abuse",
	"badsyntax",
	"badauth",
	"badhostname",
	"notyourhostname",
	"needcredit",
	"servererror",
	"generalerror",
	"warning",
	"nowan",
	"error",
	"errconfig",
};


static void unix_signal_setup(void);
static void unix_signal_handler(int sn, siginfo_t *si, void *se);
static void unix_log_open(void);
static void unix_log_close(void);

static int unix_ctor(void);
static int unix_dtor(void);
static int unix_get_wan_ip(char *buf, size_t buf_size);
static int unix_conf_get(const char* service_name, const char* conf_name,  char *buf, size_t buf_size);
static int unix_conf_reload(void);
static int unix_log(int level, const char *fmt, ...);
static int unix_publish_status_code(const char* service_name, int codeno);

struct layer unix_layer = {
	.ctor = unix_ctor,
	.dtor = unix_dtor,
	.get_wan_ip = unix_get_wan_ip,
	.conf_get = unix_conf_get,
	.conf_reload = unix_conf_reload,
	.log = unix_log,
	.publish_status_code = unix_publish_status_code,
};

/*
 * static functions - private
 */
static void unix_signal_setup(void)
{
	struct sigaction sa;

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = unix_signal_handler;

	/*
	 * https://www.securecoding.cert.org/
	 *  -> SIG00-C
	 */
	if (sigemptyset(&sa.sa_mask) != 0) 
	{
		LAYER_LOG_ERROR("sigemptyset failed");
	}
	
	if (sigaddset(&sa.sa_mask, SIGTERM)) 
	{
		LAYER_LOG_ERROR("sigaddset SIGTERM failed");
	}
	
	if (sigaddset(&sa.sa_mask, SIGHUP)) 
	{
		LAYER_LOG_ERROR("sigaddset SIGHUP failed");
	}
	
	if (sigaddset(&sa.sa_mask, SIGUSR1)) 
	{
		LAYER_LOG_ERROR("sigaddset SIGUSR1 failed");
	}
  
	if(sigaction(SIGSEGV, &sa, NULL) != 0)
	{
		LAYER_LOG_ERROR("sigaction with SIGSEGV failed");
	}

	if(sigaction(SIGTERM, &sa, NULL) != 0)
	{
		LAYER_LOG_ERROR("sigaction with SIGTERM failed");
	}

	if(sigaction(SIGHUP, &sa, NULL) != 0)
	{
		LAYER_LOG_ERROR("sigaction with SIGHUP failed");
	}
	
	if(sigaction(SIGUSR1, &sa, NULL) != 0)
	{
		LAYER_LOG_ERROR("sigaction with SIGUSR1 failed");
	}
}

static void unix_signal_handler(int sn, siginfo_t *si, void *se)
{
	UNUSED(se);
	
	switch (sn)
	{
	case SIGSEGV:
	case SIGBUS:
		if(sn == SIGBUS)
		{
			LAYER_LOG_NOTICE("-> SIGBUS received, exit with failure");
		}
		else
		{
			LAYER_LOG_NOTICE("-> SEGV %s", 
					 (si->si_code == SEGV_MAPERR) ? "Address not mapped to object" : "Invalid permissions for mapped object");
			LAYER_LOG_NOTICE("-> SEGV addr: %p", si->si_addr);
		}
		
		exit(EXIT_FAILURE);
		break;

	case SIGTERM:
		LAYER_LOG_NOTICE("SIGTERM received, wait for exit \"cleanly\"");
		event_add(EVENT_EXIT);
		break;
		
	case SIGINT:
		LAYER_LOG_NOTICE("SIGINT received, exit \"cleanly\"");
		event_add(EVENT_EXIT);
		break;
		
	case SIGHUP:
		LAYER_LOG_NOTICE("SIGHUP received, set reload conf trigger");
		event_add(EVENT_RELOAD_CONF);
		break;
		
	case SIGUSR1:
		LAYER_LOG_NOTICE("SIGUSR1 received, unblock sleep");
		break;
		
	default:
		LAYER_LOG_ERROR("Signal %d received but not processed", sn);
		break;
		
	}
}

static void unix_log_open(void)
{
	openlog(D_NAME, LOG_PID, LOG_DAEMON);
      
	return;
}

static void unix_log_close(void)
{
	closelog();

	return;
}

/*
 * layer defs
 */
static int unix_ctor(void)
{
	/* setup signals handler */
	unix_signal_setup();

	/* open log */
	unix_log_open();

	return 0;
}

static int unix_dtor(void)
{
	/* close log */
	unix_log_close();

	return 0;
}

static int unix_get_wan_ip(char *buf, size_t buf_size)
{
        /* SIOCGIFADDR struct ifreq *  */
	int s;
	struct ifreq ifr;
	size_t ifrlen;
	struct in_addr addr;
        char *ifname = getenv("DYNDNS_WANIFNAME");
        
	if(!ifname || ifname[0]=='\0')
		return -1;
        
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		LAYER_LOG_ERROR("socket(PF_INET, SOCK_DGRAM): %m");
		return -1;
	}
        
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET; /* IPv4 IP address */
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	ifrlen = sizeof(ifr);

	if(ioctl(s, SIOCGIFADDR, &ifr, &ifrlen) < 0)
	{
		LAYER_LOG_ERROR("ioctl(s, SIOCGIFADDR, ...): %m");
		close(s);
		return -1;
	}
        
        close(s);
        
	addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

        if(!inet_ntop(AF_INET, &addr, buf, buf_size))
	{
		LAYER_LOG_ERROR("inet_ntop(): %m");
		return -1;
	}
        
	return 0;
}

static int unix_conf_get(const char* service_name, const char* conf_name,  char* buf, size_t buf_size)
{
	char* val = NULL;
	int ret = 0;
	
	if(strcmp(service_name, "dyndns") == 0)
	{
		if(strcmp(conf_name, "username") == 0)
		{
			snprintf(buf, buf_size, "%s", ((val = getenv("DYNDNS_USER")) != NULL ? val : ""));
		}
		else if(strcmp(conf_name, "password") == 0)
		{
			snprintf(buf, buf_size, "%s", ((val = getenv("DYNDNS_PASSWD")) != NULL ? val : ""));
		}
		else if(strcmp(conf_name, "hostname") == 0)
		{
			snprintf(buf, buf_size, "%s", ((val = getenv("DYNDNS_HOSTNAME")) != NULL ? val : ""));
		}
		else
		{
			LAYER_LOG_ERROR("Conf name '%s' doesn't exist !",
					conf_name);
			ret = -1;
		}
	}
	else
	{
		LAYER_LOG_ERROR("Service name '%s' doesn't exist !",
				service_name);
		ret = -1;
	}
	
	return ret;
}

static int unix_conf_reload(void)
{
	
	return 0;
}

static int unix_log(int level, const char *fmt, ...)
{
	va_list args;

	UNUSED(level);
	
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
	
	return 0;
}

static int unix_publish_status_code(const char* service_name, int codeno)
{
	printf("STATUS %s service: (%d) %s\n", service_name, codeno, a_errmsg[codeno]);
	
	return 0;
}

