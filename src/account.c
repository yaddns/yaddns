#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "account.h"
#include "yaddns.h"
#include "request.h"
#include "services.h"
#include "log.h"
#include "util.h"

/* decs public variables */
struct list_head account_list;

/* defs static functions */
static void account_reqhook_readresponse(struct account *account,
                                         struct request_buff *buff);
static void account_reqhook_error(struct account *account,
                                  int errcode);

/*
 * Decs static functions
 */
static void account_reqhook_readresponse(struct account *account,
                                         struct request_buff *buff)
{
        int ret;
        struct upreply_report report;

        ret = account->def->read_resp(buff,
                                      &report);
        if(ret != 0)
        {
                log_error("Service %s read failed (Unknown error)",
                          account->def->name);
                account->locked = 1;
                account->status = ASError;
                return;
        }
                
        if(report.code == up_success)
        {
                log_info("update success for account '%s'",
                         account->cfg->name);

                account->status = ASOk;
                account->updated = 1;
                account->last_update.tv_sec = timeofday.tv_sec;
        }
        else
        {
                log_notice("update failed for account '%s' (rc=%d)",
                           account->cfg->name, report.code);

                account->status = ASError;
                account->locked = report.rcmd_lock;
                if(report.rcmd_freeze)
                {
                        account->freezed = 1;
                        account->freeze_time.tv_sec = timeofday.tv_sec;
                        account->freeze_interval.tv_sec =
                                report.rcmd_freezetime;
                }
        }
}

static void account_reqhook_error(struct account *account,
                                  int errcode)
{
        account->status = ASError;
        log_error("account %s update failed (errcode=%d)", errcode);
}

void account_reqhook(struct request *request, void *data)
{
        struct account *account = data;

        if(request->state == FSError)
        {
                account_reqhook_error(account, request->errcode);
        }
        else if(request->state == FSResponseReceived)
        {
                account_reqhook_readresponse(account, &(request->buff));
        }
}

void account_ctl_init(void)
{
        INIT_LIST_HEAD(&account_list);
}

void account_ctl_cleanup(void)
{
        struct account *account = NULL,
                *safe_act = NULL;

        list_for_each_entry_safe(account, safe_act,
                                 &(account_list), list)
        {
                list_del(&(account->list));
                free(account);
        }
}

/*
 * create updatepkt if:
 * - wan ip has changed and the service isn't locked of freezed
 * - 28 days after last update if IP no changed yet otherwise dyndns server
 *   don't know we are still alive
 */
void account_ctl_manage(void)
{
        struct request_host req_host;
        struct request_ctl req_ctl = {
                .hook_func = account_reqhook,
        };
        struct request_buff req_buff;
        struct request_opt req_opt;
        char buf_wanip[32];
        struct account *account = NULL;

        /* transform wan ip raw in ascii char */
        if(!inet_ntop(AF_INET, &wanip, buf_wanip, sizeof(buf_wanip)))
        {
                log_error("inet_ntop(): %m");
                return;
        }

        /* start update processus for service which need to update */
        list_for_each_entry(account,
                            &(account_list), list)
        {
                /* unfreeze account ? */
                if(account->freezed)
                {
                        if(timeofday.tv_sec - account->freeze_time.tv_sec
                           >= account->freeze_interval.tv_sec)
                        {
                                /* unfreeze ! */
                                account->freezed = 0;
                        }
                }

                if(account->locked || account->freezed)
                {
                        /* no deal with locked or freezed account
                         * even if needs to be updated
                         */
                        continue;
                }

                if(account->updated
                   && timeofday.tv_sec - account->last_update.tv_sec
                   >= 2419200)
                {
                        /*
                         * 28 days after last update, we need to send an updatepkt
                         * otherwise dyndns server don't know we are still alive
                         * and desactive the account.
                         */
                        log_notice("re-update service after 28 beautiful days");
                        account->updated = 0;
                }

                if(have_wanip
                   && !account->updated
                   && account->status != ASWorking)
                {
                        log_debug("Account '%s' service '%s'"
                                  " need to be updated !",
                                  account->cfg->name,
                                  account->cfg->service);

                        /* req_host structure */
                        snprintf(req_host.addr, sizeof(req_host.addr),
                                 "%s", account->def->ipserv);
                        req_host.port = account->def->portserv;

                        /* req_ctl structure */
                        req_ctl.hook_data = account;

                        /* req_buff structure, tell to service to fill it */
                        memset(&req_buff, 0, sizeof(req_buff));

                        if(account->def->make_query(*(account->cfg),
                                                    buf_wanip,
                                                    &req_buff) != 0)
                        {
                                account->status = ASError;
                                continue;
                        }

                        /* req opt */
                        req_opt.mask = REQ_OPT_BIND_ADDR;
                        req_opt.bind_addr = wanip;

                        /* send request */
                        if(request_send(&req_host, &req_ctl,
                                        &req_buff, &req_opt) != 0)
                        {
                                account->status = ASError;
                                continue;
                        }

                        /* all is ok */
                        account->status = ASWorking;
                }
        }
}

void account_ctl_needupdate(void)
{
        struct account *account = NULL;

        list_for_each_entry(account,
                            &(account_list), list)
        {
                account->updated = 0;
        }
}

int account_ctl_mapcfg(struct cfg *cfg)
{
        struct service *service = NULL;
        struct accountcfg *accountcfg = NULL;
        struct account *account = NULL,
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
                                account = calloc(1,
                                                 sizeof(struct account));
                                account->def = service;
                                account->cfg = accountcfg;

                                list_add(&(account->list),
                                         &(account_list));

                                ismapped = 1;
                                break;
                        }
                }

                if(!ismapped)
                {
                        log_error("No service named '%s' available !",
                                  accountcfg->service);

                        list_for_each_entry_safe(account, safe,
                                                 &(account_list), list)
                        {
                                list_del(&(account->list));
                                free(account);
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
        struct account *ctl;
        struct list_head list;
};

int account_ctl_mapnewcfg(struct cfg *oldcfg,
                          const struct cfg *newcfg)
{
        int ret = 0;
        int ismapped = 0;

        struct accountcfg *new_actcfg = NULL,
                *old_actcfg = NULL;
        struct account *actctl = NULL;
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
                        actctl = account_ctl_get(new_actcfg->name);
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
                                                        sizeof(struct account));
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
                                /* add to the account list */
                                list_add(&(bridger->ctl->list),
                                         &(account_list));
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
                                actctl = account_ctl_get(old_actcfg->name);
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

struct account *account_ctl_get(const char *accountname)
{
        struct account *account = NULL;

        list_for_each_entry(account,
                            &(account_list), list)
        {
                if(strcmp(account->cfg->name, accountname) == 0)
                {
                        return account;
                }
        }

        return NULL;
}
