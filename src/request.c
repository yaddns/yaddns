#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "request.h"
#include "log.h"

/* decs public variables */
struct list_head request_list;

/* defs static functions */
static int request_open_socket(struct request *request);
static void request_connect(struct request *request);
static void request_process(struct request *request);
static void request_process_send(struct request *request);
static void request_process_recv(struct request *request);

/*
 * decs static functions
 */
static int request_open_socket(struct request *request)
{
	int flags;
	struct sockaddr_in sockname;

        /* create socket */
        request->s = socket(PF_INET, SOCK_STREAM, 0);
        if(request->s < 0)
        {
                log_error("socket(): %m");
                goto exit_error;
        }

        if((flags = fcntl(request->s, F_GETFL, 0)) < 0)
        {
		log_error("fcntl(..F_GETFL..): %m");
		goto exit_error;
	}

        /* no blockant */
	if(fcntl(request->s, F_SETFL, flags | O_NONBLOCK) < 0)
        {
		log_error("fcntl(..F_SETFL..): %m");
		goto exit_error;
        }

        /* bind ? */
        if(request->opt.mask & REQ_OPT_BIND_ADDR)
        {
                memset(&sockname, 0, sizeof(struct sockaddr_in));
                sockname.sin_family = AF_INET;
                sockname.sin_addr.s_addr = request->opt.bind_addr.s_addr;

                log_debug("bind to %s", inet_ntoa(sockname.sin_addr));

                if(bind(request->s,
                        (struct sockaddr *)&sockname,
                        (socklen_t)sizeof(struct sockaddr_in)) < 0)
                {
                        log_error("bind(): %m");
                        goto exit_error;
                }
        }

        return 0;

exit_error:
        if(request->s >= 0)
        {
                close(request->s);
                request->s = -1;
        }

        return -1;
}

static void request_connect(struct request *request)
{
        struct addrinfo hints;
        struct addrinfo *res = NULL, *rp = NULL;
        int e;
        char serv[6];
        int connected = 0;
        int ret;

        snprintf(serv, sizeof(serv),
                 "%u", request->host.port);

        memset(&hints, '\0', sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_ADDRCONFIG;

        e = getaddrinfo(request->host.addr,
                        serv,
                        &hints,
                        &res);
        if(e != 0)
        {
                log_error("getaddrinfo(%s, %u) failed: %s\n",
                          request->host.addr,
                          request->host.port,
                          gai_strerror(e));
                request->state = FSError;
                return;
        }

        log_debug("connecting to %s:%u",
                  request->host.addr,
                  request->host.port);

        request->state = FSConnecting;

        connected = 0;
        for (rp = res; rp != NULL; rp = rp->ai_next)
        {
                log_debug("try to connect to %s:%u ...",
                          inet_ntoa(((struct sockaddr_in*)rp->ai_addr)->sin_addr),
                          ntohs(((struct sockaddr_in*)rp->ai_addr)->sin_port));

                ret = connect(request->s,
                              rp->ai_addr, rp->ai_addrlen);
                if(ret == 0)
                {
                        connected = 1;
                        break;
                }
                else if(ret < 0)
                {
                        log_notice("connect(): %m.");

                        if(errno == EINPROGRESS)
                        {
                                /* guess the connection will be successful
                                 * (view man connect for more information)
                                 */
                                connected = 1;
                                break;
                        }
                }
        }

        freeaddrinfo(res);

        if(!connected)
        {
                log_error("Unable to connect !");
                request->state = FSError;
        }
}

static void request_process(struct request *request)
{
        switch(request->state)
        {
	case FSConnecting:
	case FSSending:
                log_debug("&request:%p - state:%d - socket:%d"
                          " match 'FSConnecting or FSSending'",
                          request, request->state, request->s);

		request_process_send(request);
                if(request->state != FSFinished)
                {
                        log_debug("&request:%p - state:%d - "
                                  "socket:%d (request->state != FSFinished)",
                                  request, request->state, request->s);
                        break;
                }
	case FSFinished:
                log_debug("FSFinished. close socket.");
		close(request->s);
		request->s = -1;
		break;
	case FSWaitingResponse:
		request_process_recv(request);
		break;
	default:
		log_error("Unknown state");
	}
}

static void request_process_send(struct request *request)
{
        ssize_t i;
        ssize_t remain = request->buff.data_size - request->buff.data_ack;

        log_debug("send on %d: %.*s",
                  request->s,
                  remain,
                  request->buff.data + request->buff.data_ack);

        i = send(request->s,
                 request->buff.data + request->buff.data_ack,
                 remain,
                 0);
        if(i < 0)
        {
                log_error("send(): %d %m", errno);
                request->state = FSError;
                return;
        }
        else if(i != remain)
        {
                log_notice("%d bytes send out of %d",
                           i, remain);
        }

        log_debug("sent %d bytes", i);

        request->buff.data_ack += i;
        if(request->buff.data_ack == request->buff.data_size)
        {
                request->state = FSWaitingResponse;
        }

        return;
}

static void request_process_recv(struct request *request)
{
        ssize_t n;

        n = recv(request->s,
                 request->buff.data,
                 sizeof(request->buff.data),
                 0);
        if(n < 0)
        {
                  log_error("Error when reading socket %d: %m",
                            request->s);
                  request->state = FSError;
                  return;
        }

        log_debug("Recv %u bytes: %.*s", n, n, request->buff.data);

        request->buff.data_size = n;
        request->buff.data_ack = n;

        request->state = FSResponseReceived;

        /* call hook func */
        request->ctl.hook_func(request, request->ctl.hook_data);

        request->state = FSFinished;

        return;
}

/*
 * decs API functions
 */
void request_ctl_init(void)
{
        INIT_LIST_HEAD(&request_list);
}

void request_ctl_cleanup(void)
{
        struct request *request = NULL,
                *safe_request = NULL;

        list_for_each_entry_safe(request, safe_request,
                                 &(request_list), list)
        {
                list_del(&(request->list));
                free(request);
        }
}

int request_send(struct request_host *host,
                 struct request_ctl *ctl,
                 struct request_buff *buff,
                 struct request_opt *opt)
{
        struct request *request = NULL;

        request = calloc(1, sizeof(struct request));

        /* fill host structure */
        snprintf(request->host.addr, sizeof(request->host.addr),
                 "%s", host->addr);
        request->host.port = host->port;

        /* fill ctl structure */
        request->ctl.hook_func = ctl->hook_func;
        request->ctl.hook_data = ctl->hook_data;

        /* fill buf */
        snprintf(request->buff.data, sizeof(request->buff.data),
                 "%s", buff->data);
        request->buff.data_size = buff->data_size;
        request->buff.data_ack = 0;

        /* request options */
        if(opt != NULL)
        {
                if(opt->mask & REQ_OPT_BIND_ADDR)
                {
                        request->opt.bind_addr = opt->bind_addr;
                }
        }

        /* open socket */
        if(request_open_socket(request) != 0)
        {
                log_error("Error opening socket. Abort request.");
                free(request);
                return -1;
        }

        /* all is ok, add to request list */
        request->state = FSCreated;
        list_add(&(request->list), &request_list);

        return 0;
}

void request_ctl_selectfds(fd_set *readset, fd_set *writeset, int *max_fd)
{
        struct request *request = NULL;

        list_for_each_entry(request,
                            &(request_list), list)
        {
                log_debug("---------------------------");
                log_debug("selectfds(): &request:%p - state:%d - socket:%d",
                          request, request->state, request->s);
                if(request->s >= 0)
                {
			switch(request->state)
                        {
			case FSCreated:
                                log_debug("FSCreated, try to connect");
                                request_connect(request);
				if(request->state != FSConnecting)
                                {
					break;
                                }
			case FSConnecting:
			case FSSending:
                                log_debug("FSConnecting|FSSending, writeset");
                                FD_SET(request->s, writeset);
                                if(request->s > *max_fd)
                                {
                                        *max_fd = request->s;
                                }

				break;
			case FSWaitingResponse:
                                log_debug("FSWaitingForResponse, readset");
				FD_SET(request->s, readset);
                                if(request->s > *max_fd)
                                {
                                        *max_fd = request->s;
                                }

                                break;
                        default:
                                break;
                        }
                }
        }
}

void request_ctl_processfds(fd_set *readset, fd_set *writeset)
{
        struct request *request = NULL,
                *safe = NULL;

        list_for_each_entry(request,
                            &request_list, list)
        {
                if(request->s >= 0
                   && (FD_ISSET(request->s, readset)
                       || FD_ISSET(request->s, writeset)))
                {
                        log_debug("---------------------------");
                        log_debug("&request:%p - state:%d - socket:%d",
                                  request, request->state, request->s);
                        request_process(request);
                }
        }

        /* remove finished or error pkt */
        list_for_each_entry_safe(request, safe,
                                 &request_list, list)
        {
                if(request->state == FSError
                   || request->state == FSFinished)
                {
                        log_debug("remove &request:%p - state:%d - socket:%d",
                                  request, request->state, request->s);

                        if(request->state == FSError)
                        {
                                request->ctl.hook_func(request,
                                                       request->ctl.hook_data);
                        }

                        if(request->s >= 0)
                        {
                                close(request->s);
                        }

                        list_del(&(request->list));
                        free(request);
                }
        }
}

