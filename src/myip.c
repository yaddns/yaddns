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

struct in_addr myip_wanaddr;
struct timeval myip_timelastupdate = {0, 0};
int updated = 0;
int requesting = 0;

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
                return;
        }

        snprintf(ip, sizeof(ip),
                 "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
        if(inet_aton(ip, &inp) == 0)
        {
                log_error("inet_aton(%s) failed: %m", ip);
                return;
        }

        /* keep new wan ip address and update status variables */
        myip_wanaddr.s_addr = inp.s_addr;
        updated = 1;
        requesting = 0;
        memcpy(&myip_timelastupdate, &timeofday, sizeof(struct timeval));
}

static void myip_reqhook_error(struct request *request)
{
        log_error("myip failed to get wan ip address from %s:%u "
                  "(errcode=%d)", request->host.addr, request->host.port,
                  request->errcode);
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

int myip_getwanipaddr(const struct cfg_myip *cfg_myip, struct in_addr *wanaddr)
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

        *wanaddr = myip_wanaddr;

        if(updated)
        {
                if(timeofday.tv_sec - myip_timelastupdate.tv_sec
                   >= cfg_myip->upint)
                {
                        /* need update */
                        updated = 0;
                }
                else
                {
                        /* return 0 for ok, read wanaddr */
                        return 0;
                }
        }

        if(requesting)
        {
                return 1; /* waiting for wanaddr */
        }

        /* req_host structure */
        snprintf(req_host.addr, sizeof(req_host.addr),
                 "%s", cfg_myip->host);
        req_host.port = cfg_myip->port;

        /* req_buff structure, tell to service to fill it */
        memset(&req_buff, 0, sizeof(req_buff));

        n = snprintf(req_buff.data, sizeof(req_buff.data),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s\r\n\r\n",
                     cfg_myip->path, cfg_myip->host);
        req_buff.data_size = n;

        /* send request */
        if(request_send(&req_host, &req_ctl,
                        &req_buff, &req_opt) != 0)
        {
                log_error("request_send failed on %s:%u",
                          cfg_myip->host, cfg_myip->port);
                return -1;
        }

        requesting = 1;

        return 1; /* waiting for wanaddr */
}

void myip_needupdate(void)
{
        updated = 0;
}
