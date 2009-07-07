#include <string.h>
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

#include "ctl.h"
#include "log.h"
#include "util.h"

/* decs public variables */
struct list_head servicectl_list;

/* decs static variables */
static struct in_addr wanip;
static struct list_head updatepkt_list;

/* defs static functions */
static void ctl_process_send(struct updatepkt *updatepkt);
static void ctl_process_recv(struct updatepkt *updatepkt);
static int ctl_getifaddr(const char *ifname, struct in_addr *addr);
static struct updatepkt *ctl_create_updatepkt(struct in_addr *addr);
static void ctl_connect(struct updatepkt *updatepkt);

static void ctl_process_send(struct updatepkt *updatepkt)
{
        ssize_t i;

        log_debug("send on %d: %.*s",
                  updatepkt->s,
                  updatepkt->buf_tosend - updatepkt->buf_sent,
                  updatepkt->buf + updatepkt->buf_sent);
        
        i = send(updatepkt->s,
                 updatepkt->buf + updatepkt->buf_sent,
                 updatepkt->buf_tosend - updatepkt->buf_sent,
                 0);
        if(i < 0)
        {
                log_error("send(): %d %m", errno);
                return;
        }
        else if(i != (updatepkt->buf_tosend - updatepkt->buf_sent))
        {
                log_notice("%d bytes send out of %d", 
                           i, updatepkt->buf_tosend);
        }

        log_debug("sent %d bytes", i);

        updatepkt->buf_sent += i;
        if(updatepkt->buf_sent == updatepkt->buf_tosend)
        {
                updatepkt->state = EWaitingForResponse;
        }
}

static void ctl_process_recv(struct updatepkt *updatepkt)
{
        int n, ret;
        struct upreply_report report;

        n = recv(updatepkt->s, 
                 updatepkt->buf, sizeof(updatepkt->buf), 0);
        if(n < 0)
        {
                  log_error("Error when reading socket %d: %m",
                            updatepkt->s);
                  return;
        }

        log_debug("Recv %u bytes: %.*s", n, n, updatepkt->buf);
        
        ret = updatepkt->ctl->def->read_up_resp(updatepkt->buf, &report);
        
        if(ret != 0)
        {
                log_error("Unknown error when reading response.");
                updatepkt->ctl->locked = 1;
                updatepkt->ctl->status = SNeedToUpdate;
        }
        else
        {
                if(report.code == up_success)
                {
                        updatepkt->ctl->status = SUpdated;
                        updatepkt->ctl->last_update.tv_sec
                                = timeofday.tv_sec;
                }
                else
                {
                        log_notice("update failed for '%s' (rc=%d)",
                                   updatepkt->ctl->def->name,
                                   report.code);
                        
                        updatepkt->ctl->status = SNeedToUpdate;
                        updatepkt->ctl->locked = report.rcmd_lock;
                        if(report.rcmd_freeze)
                        {
                                updatepkt->ctl->freezed = 1;
                                updatepkt->ctl->freeze_time.tv_sec 
                                        = timeofday.tv_sec;
                                updatepkt->ctl->freeze_interval.tv_sec 
                                        = report.rcmd_freezetime;
                        }
                }
        }
        
	updatepkt->state = EFinished;
}

static int ctl_getifaddr(const char *ifname, struct in_addr *addr)
{
#if 1
        UNUSED(ifname);
        
        inet_aton("91.121.120.56", addr);
#else
        /* SIOCGIFADDR struct ifreq *  */
	int s;
	struct ifreq ifr;
	int ifrlen;

	if(!ifname || ifname[0]=='\0')
		return -1;
        
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		log_error("socket(PF_INET, SOCK_DGRAM): %m");
		return -1;
	}
        
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET; /* IPv4 IP address */
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
	ifrlen = sizeof(ifr);

	if(ioctl(s, SIOCGIFADDR, &ifr, &ifrlen) < 0)
	{
		log_error("ioctl(s, SIOCGIFADDR, ...): %m");
		close(s);
		return -1;
	}
        
	*addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

	close(s);
#endif

	return 0;
}

static struct updatepkt *ctl_create_updatepkt(struct in_addr *addr)
{
	int flags;
        struct updatepkt *updatepkt = NULL;
        
        updatepkt = calloc(1, sizeof(struct updatepkt));
        if(updatepkt == NULL)
        {
                log_critical("calloc(): %m");
                return NULL;
        }

        updatepkt->state = ECreated;
        updatepkt->s = socket(PF_INET, SOCK_STREAM, 0);
        if(updatepkt->s < 0)
        {
                log_error("socket(): %m");
                goto exit_error;
        }

        
        if((flags = fcntl(updatepkt->s, F_GETFL, 0)) < 0) 
        {
		log_error("fcntl(..F_GETFL..): %m");
		goto exit_error;
	}
        
	if(fcntl(updatepkt->s, F_SETFL, flags | O_NONBLOCK) < 0) {
		log_error("fcntl(..F_SETFL..): %m");
		goto exit_error;
        }

#if 1
        UNUSED(addr);
#else
        if(bind(updatepkt->s, 
                (struct sockaddr *)addr, 
                (socklen_t)sizeof(struct sockaddr_in)) < 0)
	{
		log_error("bind(): %m");
		goto exit_error;
	}
#endif
   
        log_debug("updatepkt created: %p", updatepkt);
        
        list_add(&(updatepkt->list), &updatepkt_list);
        
        return updatepkt;
        
exit_error:
        if(updatepkt->s >= 0)
        {
                close(updatepkt->s);
        }
        
        free(updatepkt);
        return NULL;
}

static void ctl_connect(struct updatepkt *updatepkt)
{
	struct sockaddr_in addr;
        struct hostent *host = NULL;
        
        memset(&addr, 0, sizeof(struct sockaddr_in));
        
        if((host = gethostbyname(updatepkt->ctl->def->ipserv)) == NULL)
        {
                log_error("gethostbyname() failed ! %s %d", 
                          hstrerror(h_errno), h_errno);
                updatepkt->state = EError;
                return;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr = *(struct in_addr*)host->h_addr;
        addr.sin_port = htons(updatepkt->ctl->def->portserv);

        log_debug("connecting to %s:%d", 
                  updatepkt->ctl->def->ipserv, 
                  updatepkt->ctl->def->portserv);
        updatepkt->state = EConnecting;
        
        if(connect(updatepkt->s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
		if(errno != EINPROGRESS && errno != EWOULDBLOCK) 
                {
			log_error("connect(): %m");
			updatepkt->state = EError;
		}
	}
}

static void ctl_process(struct updatepkt *updatepkt)
{
        switch(updatepkt->state) 
        {
	case EConnecting:
	case ESending:
		ctl_process_send(updatepkt);
                if(updatepkt->state != EFinished)
                {
                        break;
                }
	case EFinished:
                log_debug("EFinished. close socket.");
		close(updatepkt->s);
		updatepkt->s = -1;
		break;
	case EWaitingForResponse:
		ctl_process_recv(updatepkt);
		break;
	default:
		log_error("unknown state");
	}
}

void ctl_init(void)
{
        INIT_LIST_HEAD(&servicectl_list);
        INIT_LIST_HEAD(&updatepkt_list);

        inet_aton("0.0.0.0", &wanip);
}

void ctl_free(void)
{
        struct servicectl *servicectl = NULL,
                *safe_sctl = NULL;
        struct updatepkt *updatepkt = NULL,
                *safe_upkt = NULL;
        
        list_for_each_entry_safe(servicectl, safe_sctl, 
                                 &(servicectl_list), list) 
        {
                list_del(&(servicectl->list));
                free(servicectl);
        }
        
        list_for_each_entry_safe(updatepkt, safe_upkt, 
                                 &(updatepkt_list), list) 
        {
                list_del(&(updatepkt->list));
                free(updatepkt);
        }
}

/*
 * create updatepkt if:
 * - wan ip has changed and the service isn't locked of freezed
 * - (dyndns spec) 28 days after last update if IP no changed yet
 */
void ctl_preselect(struct cfg *cfg)
{
        struct in_addr curr_wanip;
        struct servicectl *servicectl = NULL;
        struct updatepkt *updatepkt = NULL;
        char buf_wanip[32];
        
        /* TODO: indirect mode */
        
        if(ctl_getifaddr(cfg->wan_ifname, &curr_wanip) != 0)
        {
                /* no wan ? */
                return;
        }
        
        
        /* transform wan ip raw in ascii char */
        if(!inet_ntop(AF_INET, &wanip, buf_wanip, sizeof(buf_wanip)))
        {
                log_error("inet_ntop(): %m");
                return;
        }

        /****** wan ip has changed ? ******/
        if(curr_wanip.s_addr != wanip.s_addr)
        {
                log_debug("new wan ip !");
                
                list_for_each_entry(servicectl, 
                                    &(servicectl_list), list) 
                {
                        servicectl->status = SNeedToUpdate;
                }

                wanip.s_addr = curr_wanip.s_addr;
        }
        
        /****** 28 days after last update ******/
        list_for_each_entry(servicectl, 
                            &(servicectl_list), list) 
        {
                if(servicectl->status != SUpdated)
                {
                        continue;
                }
                
                log_debug("last update: %d, timeofday: %d",
                          servicectl->last_update.tv_sec,
                          timeofday.tv_sec);
                if(timeofday.tv_sec - servicectl->last_update.tv_sec
                   >= 2419200)
                {
                        log_notice("re-update service after 28 beautiful days");
                        servicectl->status = SNeedToUpdate;
                }
        }

        /*********************/
        
        /* start update processus for service which need to update */
        list_for_each_entry(servicectl, 
                            &(servicectl_list), list) 
        {  
                if(servicectl->locked || servicectl->freezed)
                {
                        continue;
                }
                        
                if(servicectl->status == SNeedToUpdate)
                {
                        log_debug("Service '%s' need to update !",
                                  servicectl->def->name);
                        
                        updatepkt = ctl_create_updatepkt(&curr_wanip);
                        if(updatepkt != NULL)
                        {
                                /* link pkt and servicectl */
                                updatepkt->ctl = servicectl;
                                
                                /* call service update maker */
                                if(servicectl->def->make_up_query(*(servicectl->cfg),
                                                                  buf_wanip,
                                                                  updatepkt->buf,
                                                                  sizeof(updatepkt->buf)) != 0)
                                {
                                        updatepkt->state = EError;
                                        continue;
                                }

                                updatepkt->buf_tosend = strlen(updatepkt->buf);

                                log_debug("+++ pkt: %s", updatepkt->buf);
                                servicectl->status = SUpdating;
                        }
                }
        }
}

void ctl_selectfds(fd_set *readset, fd_set *writeset, int * max_fd)
{
        struct updatepkt *updatepkt = NULL;
        
        list_for_each_entry(updatepkt, 
                            &(updatepkt_list), list) 
        {
                log_debug("selectfds(): &updatepkt:%p - state:%d - socket:%d",
                          updatepkt, updatepkt->state, updatepkt->s);
                if(updatepkt->s >= 0) 
                {
			switch(updatepkt->state) 
                        {
			case ECreated:
                                log_debug("ECreated, try to connect");
				ctl_connect(updatepkt);
				if(updatepkt->state != EConnecting)
                                {
					break;
                                }
			case EConnecting:
			case ESending:
                                log_debug("EConnecting, ESending, writeset FD_SET");
                                FD_SET(updatepkt->s, writeset);
                                if(updatepkt->s > *max_fd)
                                {
                                        *max_fd = updatepkt->s;
                                }
                                
				break;
			case EWaitingForResponse:
                                log_debug("EWaitingForResponse, readset FD_SET");
				FD_SET(updatepkt->s, readset);
                                if(updatepkt->s > *max_fd)
                                {
                                        *max_fd = updatepkt->s;
                                }
                                
                                break;
                        default:
                                break;
			}
		}
        }
}

void ctl_processfds(fd_set *readset, fd_set *writeset)
{
        struct updatepkt *updatepkt = NULL,
                *safe = NULL;
        
        list_for_each_entry(updatepkt,
                            &updatepkt_list, list)
        {
                if(updatepkt->s >= 0 
                   && (FD_ISSET(updatepkt->s, readset)
                       || FD_ISSET(updatepkt->s, writeset)))
                {
                        ctl_process(updatepkt);
                }
        }
        
        /* remove finished or error pkt */
        list_for_each_entry_safe(updatepkt, safe,
                                 &updatepkt_list, list)
        {
                if(updatepkt->state == EError 
                   || updatepkt->state == EFinished)
                {
                        log_debug("remove &updatepkt:%p - state:%d - socket:%d",
                                  updatepkt, updatepkt->state, updatepkt->s);
                        
                        if(updatepkt->s >= 0)
                        {
                                close(updatepkt->s);
                        }

                        if(updatepkt->state == EError)
                        {
                                /* need to retry ! */
                                updatepkt->ctl->status = SNeedToUpdate;
                        }

                        list_del(&(updatepkt->list));
                        free(updatepkt);
                }
        }
}

int ctl_service_mapcfg(struct cfg *cfg)
{
        struct service *service = NULL;
        struct servicecfg *servicecfg = NULL;
        struct servicectl *servicectl = NULL,
                *safe = NULL;
        int ismapped = 0;
        int ret = 0;

        list_for_each_entry(servicecfg, 
                            &(cfg->servicecfg_list), list) 
        {
                ismapped = 0;
                
                list_for_each_entry(service,
                                    &(service_list), list) 
                {
                        if(strcmp(service->name, servicecfg->name) == 0)
                        {
                                servicectl = calloc(1, 
                                                    sizeof(struct servicectl));
                                servicectl->def = service;
                                servicectl->cfg = servicecfg;

                                list_add(&(servicectl->list), 
                                         &(servicectl_list));

                                ismapped = 1;
                                break;
                        }
                }

                if(!ismapped)
                {
                        log_error("No service named '%s' available !",
                                  servicecfg->name);

                        list_for_each_entry_safe(servicectl, safe, 
                                 &(servicectl_list), list) 
                        {
                                list_del(&(servicectl->list));
                                free(servicectl);
                        }

                        ret = -1;
                        break;
                }
        }

        return ret;
}

/*
 * update service already living
 * create if service not exist
 */
int ctl_service_mapnewcfg(struct cfg *oldcfg,
                          const struct cfg *newcfg)
{
        int ismapped = 0;
        
        struct servicecfg *newservicecfg = NULL;

        UNUSED(oldcfg);

        list_for_each_entry(newservicecfg, 
                            &(newcfg->servicecfg_list), list) 
        {
                ismapped = 0;
                
                /* TODO */
        }

        return 0;
}
