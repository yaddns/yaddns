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

#include "ctl.h"
#include "log.h"
#include "util.h"

/* decs public variables */
struct list_head accountctl_list;

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
                updatepkt->state = EError;
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
                updatepkt->ctl->status = SError;
        }
        else
        {
                if(report.code == up_success)
                {
                        log_info("update success for account '%s'",
                                 updatepkt->ctl->cfg->name);
                        
                        updatepkt->ctl->status = SOk;
                        updatepkt->ctl->updated = 1;
                        updatepkt->ctl->last_update.tv_sec
                                = timeofday.tv_sec;
                }
                else
                {
                        log_notice("update failed for account '%s' (rc=%d)",
                                   updatepkt->ctl->cfg->name,
                                   report.code);
                        
                        updatepkt->ctl->status = SError;
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

	return 0;
}

static struct updatepkt *ctl_create_updatepkt(struct in_addr *addr)
{
	int flags;
        struct updatepkt *updatepkt = NULL;
	struct sockaddr_in sockname;
        
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

        memset(&sockname, 0, sizeof(struct sockaddr_in));
        sockname.sin_family = AF_INET;
        sockname.sin_addr.s_addr = htonl(addr->s_addr);

        log_debug("bind to %s", inet_ntoa(sockname.sin_addr));

        if(bind(updatepkt->s, 
                (struct sockaddr *)&sockname, 
                (socklen_t)sizeof(struct sockaddr_in)) < 0)
	{
		log_error("bind(): %m");
		goto exit_error;
	}
   
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
        struct addrinfo hints;
        struct addrinfo *res = NULL, *rp = NULL;
        int e;
        char serv[6];
        int connected;
        int ret;

        snprintf(serv, sizeof(serv), 
                 "%d", updatepkt->ctl->def->portserv);
        
        memset(&hints, '\0', sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_ADDRCONFIG;
        
        e = getaddrinfo(updatepkt->ctl->def->ipserv,
                        serv,
                        &hints,
                        &res);
        if(e != 0)
        {
                log_error("getaddrinfo(%s, %s) failed: %s\n",
                          updatepkt->ctl->def->ipserv, 
                          updatepkt->ctl->def->portserv, 
                          gai_strerror(e));
                updatepkt->state = EError;
                return;
        }
        

        log_debug("connecting to %s:%d", 
                  updatepkt->ctl->def->ipserv, 
                  updatepkt->ctl->def->portserv);
        updatepkt->state = EConnecting;

        for (rp = res; rp != NULL; rp = rp->ai_next) 
        {
                log_debug("try to connect to %s:%u ...",
                          inet_ntoa(((struct sockaddr_in*)rp->ai_addr)->sin_addr),
                          ((struct sockaddr_in*)rp->ai_addr)->sin_port);
                
                ret = connect(updatepkt->s, 
                              rp->ai_addr, rp->ai_addrlen);
                if(ret == 0)
                {
                        connected = 1;
                        break;
                }
                else if(ret < 0 
                        && (errno == EINPROGRESS || errno == EWOULDBLOCK)) 
                {
                        log_notice("connect(): %m.");
                        connected = 1;
                        break;
                }
        }

        freeaddrinfo(res);

        if(!connected)
        {
                log_error("Unable to connect !");
                updatepkt->state = EError;
        }
}

static void ctl_process(struct updatepkt *updatepkt)
{
        switch(updatepkt->state) 
        {
	case EConnecting:
	case ESending:
                log_debug("ctl_process &updatepkt:%p - state:%d - socket:%d"
                          "match 'case EConnecting case ESending'",
                          updatepkt, updatepkt->state, updatepkt->s);
                
		ctl_process_send(updatepkt);
                if(updatepkt->state != EFinished)
                {
                        log_debug("ctl_process &updatepkt:%p - state:%d - "
                                  "socket:%d (updatepkt->state != EFinished)",
                                  updatepkt, updatepkt->state, updatepkt->s);
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
        INIT_LIST_HEAD(&accountctl_list);
        INIT_LIST_HEAD(&updatepkt_list);

        inet_aton("0.0.0.0", &wanip);
}

void ctl_free(void)
{
        struct accountctl *accountctl = NULL,
                *safe_actl = NULL;
        struct updatepkt *updatepkt = NULL,
                *safe_upkt = NULL;
        
        list_for_each_entry_safe(accountctl, safe_actl, 
                                 &(accountctl_list), list) 
        {
                list_del(&(accountctl->list));
                free(accountctl);
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
 * - 28 days after last update if IP no changed yet otherwise dyndns server
 *   don't know we are still alive
 */
void ctl_preselect(struct cfg *cfg)
{
        struct in_addr curr_wanip;
        struct accountctl *accountctl = NULL;
        struct updatepkt *updatepkt = NULL;
        char buf_wanip[32];
        
        /* TODO: indirect mode */
        
        if(ctl_getifaddr(cfg->wan_ifname, &curr_wanip) != 0)
        {
                /* no wan ? */
                return;
        }
        
        
        /* transform wan ip raw in ascii char */
        if(!inet_ntop(AF_INET, &curr_wanip, buf_wanip, sizeof(buf_wanip)))
        {
                log_error("inet_ntop(): %m");
                return;
        }

        /****** wan ip has changed ? ******/
        if(curr_wanip.s_addr != wanip.s_addr)
        {
                log_debug("new wan ip !");
                
                list_for_each_entry(accountctl, 
                                    &(accountctl_list), list) 
                {
                        accountctl->updated = 0;
                }

                wanip.s_addr = curr_wanip.s_addr;
        }

        /*********************/
        
        /* start update processus for service which need to update */
        list_for_each_entry(accountctl, 
                            &(accountctl_list), list) 
        {  
                if(accountctl->locked || accountctl->freezed)
                {
                        continue;
                }
                
                if(accountctl->updated
                   && timeofday.tv_sec - accountctl->last_update.tv_sec
                   >= 2419200)
                {
                        /*
                         * 28 days after last update, we need to send an updatepkt
                         * otherwise dyndns server don't know we are still alive
                         * and desactive the account.
                         */
                        log_notice("re-update service after 28 beautiful days");
                        accountctl->updated = 0;
                }

                if(!accountctl->updated && accountctl->status != SWorking)
                {
                        log_debug("Account '%s' service '%s' need to update !",
                                  accountctl->cfg->name, 
                                  accountctl->cfg->service);
                        
                        updatepkt = ctl_create_updatepkt(&curr_wanip);
                        if(updatepkt != NULL)
                        {
                                /* link pkt and accountctl */
                                updatepkt->ctl = accountctl;
                                
                                /* call service update maker */
                                if(accountctl->def->make_up_query(*(accountctl->cfg),
                                                                  buf_wanip,
                                                                  updatepkt->buf,
                                                                  sizeof(updatepkt->buf)) != 0)
                                {
                                        updatepkt->state = EError;
                                        continue;
                                }

                                updatepkt->buf_tosend = strlen(updatepkt->buf);

                                log_debug("+++ pkt: %s", updatepkt->buf);
                                accountctl->status = SWorking;
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
                        log_debug("&updatepkt:%p - state:%d - socket:%d",
                                  updatepkt, updatepkt->state, updatepkt->s);
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
                                updatepkt->ctl->status = SError;
                        }

                        list_del(&(updatepkt->list));
                        free(updatepkt);
                }
        }
}

int ctl_account_mapcfg(struct cfg *cfg)
{
        struct service *service = NULL;
        struct accountcfg *accountcfg = NULL;
        struct accountctl *accountctl = NULL,
                *safe = NULL;
        int ismapped = 0;
        int ret = 0;

        list_for_each_entry(accountcfg, 
                            &(cfg->accountcfg_list), list) 
        {
                ismapped = 0;
                
                list_for_each_entry(service,
                                    &(service_list), list) 
                {
                        if(strcmp(service->name, accountcfg->service) == 0)
                        {
                                accountctl = calloc(1, 
                                                    sizeof(struct accountctl));
                                accountctl->def = service;
                                accountctl->cfg = accountcfg;

                                list_add(&(accountctl->list), 
                                         &(accountctl_list));

                                ismapped = 1;
                                break;
                        }
                }

                if(!ismapped)
                {
                        log_error("No service named '%s' available !",
                                  accountcfg->service);

                        list_for_each_entry_safe(accountctl, safe, 
                                 &(accountctl_list), list) 
                        {
                                list_del(&(accountctl->list));
                                free(accountctl);
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
struct bridger {
        enum {
                TMirror = 0,
                TNew,
                TUpdate,
        } type;
        struct accountcfg *cfg;
        struct accountctl *ctl;
        struct list_head list;
};

int ctl_account_mapnewcfg(struct cfg *oldcfg,
                          const struct cfg *newcfg)
{
        int ret = 0;
        int ismapped = 0;
        
        struct accountcfg *new_actcfg = NULL,
                *old_actcfg = NULL;
        struct accountctl *actctl = NULL;
        struct service *service = NULL;
        
        struct bridger *bridger = NULL,
                *safe_bridger = NULL;
        struct list_head bridger_list;

        INIT_LIST_HEAD(&bridger_list);
        
        list_for_each_entry(new_actcfg, 
                            &(newcfg->accountcfg_list), list) 
        {
                old_actcfg = config_account_get(oldcfg, new_actcfg->name);
                if(old_actcfg != NULL)
                {
                        /* retrieve the ctl */
                        actctl = ctl_account_get(new_actcfg->name);
                        if(actctl == NULL)
                        {
                                log_critical("Unable to get account ctl "
                                             "for account '%s'", 
                                             new_actcfg->name);
                                ret = -1;
                                break;
                        }

                        bridger = calloc(1, sizeof(struct bridger));
                        bridger->cfg = new_actcfg; /* link the new cfg */
                        bridger->ctl = actctl; /* link the old ctl */
                        
                        if(strcmp(new_actcfg->service, 
                                  old_actcfg->service) != 0
                           || strcmp(new_actcfg->username, 
                                     old_actcfg->username) != 0
                           || strcmp(new_actcfg->passwd, 
                                     old_actcfg->passwd) != 0
                           || strcmp(new_actcfg->hostname, 
                                     old_actcfg->hostname) != 0)
                        {
                                /* it's an update */
                                bridger->type = TUpdate;
                        }
                        else
                        {
                                bridger->type = TMirror;
                        }
                        
                        list_add(&(bridger->list), 
                                 &bridger_list);
                        
                }
                else
                {
                        ismapped = 0;
                
                        /* not found so it's a new account */
                        list_for_each_entry(service,
                                            &(service_list), list) 
                        {
                                if(strcmp(service->name, 
                                          new_actcfg->service) == 0)
                                {
                                        actctl = calloc(1, 
                                                        sizeof(struct accountctl));
                                        actctl->def = service;
                                        
                                        bridger = calloc(1, 
                                                         sizeof(struct bridger));
                                        bridger->type = TNew;
                                        bridger->cfg = new_actcfg;
                                        bridger->ctl = actctl;
                        
                                        list_add(&(bridger->list), 
                                                 &bridger_list);
                                        
                                        ismapped = 1;
                                        break;
                                }
                        }

                        if(!ismapped)
                        {
                                log_error("No service named '%s' available !",
                                          new_actcfg->service);

                                ret = -1;
                                break;
                        }
                }

        }

        if(ret == 0)
        {
                /* all is good, so finish the mapping */
                list_for_each_entry(bridger,
                                    &(bridger_list), list)
                {
                        bridger->ctl->cfg = bridger->cfg;

                        if(bridger->type == TUpdate)
                        {
                                bridger->ctl->updated = 0;
                                bridger->ctl->locked = 0;
                                bridger->ctl->freezed = 0;
                        }

                        if(bridger->type == TNew)
                        {
                                /* add to the accountctl list */
                                list_add(&(bridger->ctl->list), 
                                         &(accountctl_list));
                        }
                }

                /* remove unused account ctl */
                list_for_each_entry(old_actcfg,
                                    &(oldcfg->accountcfg_list), list)
                {
                        new_actcfg = config_account_get(newcfg, old_actcfg->name);
                        if(new_actcfg == NULL)
                        {
                                /* need to remove */
                                actctl = ctl_account_get(old_actcfg->name);
                                if(actctl != NULL)
                                {
                                        log_debug("remove unused ctl '%p'",
                                                  actctl);
                                        
                                        list_del(&(actctl->list));
                                }
                                else
                                {
                                        log_critical("Unable to get account "
                                                     "ctl for '%s'. Continue..",
                                                     old_actcfg->name);
                                }
                        }
                }

        }
        
        /* clean up */
        list_for_each_entry_safe(bridger, safe_bridger, 
                                 &(bridger_list), list) 
        {
                if(ret != 0)
                {
                        if(bridger->type == TNew)
                        {
                                free(bridger->ctl);
                        }
                }
                
                list_del(&(bridger->list));
                free(bridger);
        }

        return ret;
}

struct accountctl *ctl_account_get(const char *accountname)
{
        struct accountctl *accountctl = NULL; 
        
        list_for_each_entry(accountctl, 
                            &(accountctl_list), list) 
        {
                if(strcmp(accountctl->cfg->name, accountname) == 0)
                {
                        return accountctl;
                }
        }

        return NULL;
}
