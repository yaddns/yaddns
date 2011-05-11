#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "myip.h"

#include "request.h"
#include "yaddns.h"
#include "log.h"

/* sleep REQ_SLEEPTIME_ON_ERROR when got an request error */
#define REQ_SLEEPTIME_ON_ERROR 60

static struct myip_ctl {
        enum {
                MISError = -1,
                MISNeedUpdate = 0,
                MISHaveIp = 1,
                MISWorking,
        } status;
        struct in_addr wanaddr;
        struct timeval timelasterror;
        struct timeval timelastok;
} myip_ctl = {
        .status = MISNeedUpdate,
        .wanaddr = {0},
        .timelasterror = {0, 0},
        .timelastok = {0, 0},
};

static void myip_reqhook_recv(struct request_buff *buff)
{
	int ip1 = 0,
                ip2 = 0,
                ip3 = 0,
                ip4 = 0;
        short int found = 0;
	char *p_digit = NULL;
        const char *data = NULL;
        char ip[16];
        struct in_addr inp;
        int count;

        data = buff->data;

        /* check http response */
        if(!(strstr(data, "HTTP/1.1 200 OK") ||
             strstr(data, "HTTP/1.0 200 OK")))
	{
                log_error("HTTP code different to 200 in myip response");
                log_debug("PACKET: %s", data);
                myip_ctl.status = MISError;
                memcpy(&myip_ctl.timelasterror,
                       &timeofday, sizeof(struct timeval));
                return;
        }

        /* try to find wan ip address */
        found = 0;
        do
        {
                p_digit = strpbrk(data, "0123456789");
                if(p_digit != NULL)
                {
                        count = sscanf(p_digit, "%d.%d.%d.%d",
                                       &ip1, &ip2, &ip3, &ip4);
			if (count == 4
                            && (ip1 >= 0 && ip1 <= 255)
                            && (ip2 >= 0 && ip2 <= 255)
                            && (ip3 >= 0 && ip3 <= 255)
                            && (ip4 >= 0 && ip4 <= 255))
			{
				found = 1;
                                break;
			}
			
                        data = p_digit + 1;
                }
        } while(p_digit != NULL);

        if(!found)
        {
                log_error("No found wan ip address in myip response");
                log_debug("PACKET: %s", data);
                myip_ctl.status = MISError;
                memcpy(&myip_ctl.timelasterror,
                       &timeofday, sizeof(struct timeval));
                return;
        }

        snprintf(ip, sizeof(ip),
                 "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
        if(inet_aton(ip, &inp) == 0)
        {
                log_error("inet_aton(%s) failed: %m", ip);
                myip_ctl.status = MISError;
                memcpy(&myip_ctl.timelasterror,
                       &timeofday, sizeof(struct timeval));
                return;
        }

        /* update myip_ctl structure */
        myip_ctl.status = MISHaveIp;
        myip_ctl.wanaddr.s_addr = inp.s_addr;
        memcpy(&myip_ctl.timelastok, &timeofday, sizeof(struct timeval));
}

static void myip_reqhook_error(struct request *request)
{
        log_error("myip failed to retrieve wan ip address from %s:%u "
                  "(errcode=%d)",
                  request->host.addr, request->host.port,
                  request->errcode);

        /* update myip_ctl structure */
        if(request->errcode == REQ_ERR_CONNECT_TIMEOUT
           || request->errcode == REQ_ERR_RESPONSE_TIMEOUT
           || request->errcode == REQ_ERR_SENDING_TIMEOUT)
        {
                /* try again immediatly */
                myip_ctl.status = MISNeedUpdate;
        }
        else
        {
                myip_ctl.status = MISError;
                memcpy(&myip_ctl.timelasterror,
                       &timeofday, sizeof(struct timeval));
        }
}

static void myip_reqhook(struct request *request, void *data)
{
        (void)data;

        if(request->state == FSResponseReceived)
        {
                myip_reqhook_recv(&(request->buff));
        }
        else if(request->state == FSError)
        {
                myip_reqhook_error(request);
        }
}

static int myip_sendrequest(const char *host,
                            unsigned short int port, const char *path)
{
        struct request_host req_host;
        struct request_ctl req_ctl = {
                .hook_func = myip_reqhook,
                .hook_data = NULL,
        };
        struct request_buff req_buff;
        struct request_opt req_opt = {
                .mask = 0,
        };
        int n;

        /* req_host structure */
        snprintf(req_host.addr, sizeof(req_host.addr),
                 "%s", host);
        req_host.port = port;

        /* req_buff structure, tell to service to fill it */
        memset(&req_buff, 0, sizeof(req_buff));

        n = snprintf(req_buff.data, sizeof(req_buff.data),
                     "GET %s HTTP/1.0\r\n"
                     "Host: %s\r\n\r\n",
                     path, host);
        req_buff.data_size = n;

        /* send request */
        if(request_send(&req_host, &req_ctl,
                        &req_buff, &req_opt) != 0)
        {
                log_error("request_send failed on %s:%u",
                          host, port);
                return -1;
        }

        return 0;
}

/*
 * An first call, we don't have wan ip address yet. We need to wait.
 */
int myip_getwanipaddr(const struct cfg_myip *cfg_myip, struct in_addr *wanaddr)
{
        int ret = -1;

        if(myip_ctl.timelastok.tv_sec != 0)
        {
                /* return the last wan ip address got */
                *wanaddr = myip_ctl.wanaddr;
                ret = 0;
        }

        if((myip_ctl.status == MISHaveIp
            && (timeofday.tv_sec - myip_ctl.timelastok.tv_sec
                >= cfg_myip->upint))
           || (myip_ctl.status == MISError
               && (timeofday.tv_sec - myip_ctl.timelasterror.tv_sec
                   >= REQ_SLEEPTIME_ON_ERROR)))
        {
                /* timeout, need update */
                myip_ctl.status = MISNeedUpdate;
        }

        if(myip_ctl.status == MISNeedUpdate)
        {
                /* send a request */
                if(myip_sendrequest(cfgstr_get(&(cfg_myip->host)),
                                    cfg_myip->port,
                                    cfgstr_get(&(cfg_myip->path))) == 0)
                {
                        myip_ctl.status = MISWorking;
                }
                else
                {
                        myip_ctl.status = MISError;
                        memcpy(&myip_ctl.timelasterror,
                               &timeofday, sizeof(struct timeval));
                }
        }

        return ret;
}

void myip_needupdate(void)
{
        myip_ctl.status = MISNeedUpdate;
}
