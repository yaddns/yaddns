#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "yaddns.h"
#include "services.h"
#include "request.h"
#include "config.h"
#include "log.h"
#include "account.h"
#include "util.h"
#include "myip.h"

static volatile sig_atomic_t keep_going = 0;
static volatile sig_atomic_t reloadconf = 0;
static volatile sig_atomic_t wakeup = 0;

struct timeval timeofday = {0, 0};
struct in_addr wanip;
short int have_wanip = 0;

static void sig_handler(int signum)
{
	if(signum == SIGTERM || signum == SIGINT)
	{
		log_notice("Receive SIGTERM or SIGINT. Exit.");
		keep_going = 0;
	}
        else if(signum == SIGHUP)
        {
                log_notice("Receive SIGHUP. Reload configuration.");
                reloadconf = 1;
        }
        else if(signum == SIGUSR1)
        {
                log_notice("Receive SIGUSR1. Wake up !");
                wakeup = 1;
        }
}

static int sig_setup(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sig_handler;

	if(sigaction(SIGTERM, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGTERM: %m");
		return -1;
	}

	if(sigaction(SIGINT, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGINT: %m");
		return -1;
	}

        if(sigaction(SIGHUP, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGHUP: %m");
		return -1;
	}

        if(sigaction(SIGUSR1, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGUSR1: %m");
		return -1;
	}

        return 0;
}

static void sig_blockall(void)
{
	sigset_t set;

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);
}

static void sig_unblockall(void)
{
	sigset_t set;

	sigfillset(&set);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
}

int main(int argc, char **argv)
{
	int ret = 0;
        sigset_t unblocked;
        struct cfg cfg, cfgre;
        struct timespec timeout = {0, 0};
        struct timeval lasttimeofday = {0, 0};
	fd_set readset, writeset;
	int max_fd = -1;
	FILE *fpid = NULL;

        struct in_addr curr_wanip;

        /* init */
        account_ctl_init();
        request_ctl_init();
        services_populate_list();
        inet_aton("0.0.0.0", &wanip);

        /* sig setup */
        if(sig_setup() != 0)
        {
                ret = 1;
                goto exit_clean;
        }

        sig_blockall();

        sigemptyset(&unblocked);

	/* config */
        memset(&cfg, 0, sizeof(struct cfg));
	if(config_parse(&cfg, argc, argv) != 0)
	{
                ret = 1;
                goto exit_clean;
	}

        config_print(&cfg);

	/* open log */
	log_open(&cfg);

        /* daemonize ? */
        if(cfg.daemonize)
        {
                if(daemon(0, 0) < 0)
                {
                        log_error("Failed to daemonize !");
                        ret = 1;
                        goto exit_clean;
                }
        }

        /* create pid file ? */
        if(cfg.pidfile != NULL)
        {
                if((fpid = fopen(cfg.pidfile, "w")))
                {
                        fprintf(fpid, "%u", getpid());
                        fclose(fpid);
                        fpid = NULL;
                }
                else
                {
                        log_error("Failed to create pidfile %s: %m",
                                  cfg.pidfile);
                        goto exit_clean;
                }
        }

        /* create account ctls */
        if(account_ctl_mapcfg(&cfg) != 0)
        {
                ret = 1;
                goto exit_clean;
        }

	/* yaddns loop */
        keep_going = 1;
	while(keep_going)
	{
                /* reinit variables for pselect() */
                max_fd = 0;
		FD_ZERO(&readset);
                FD_ZERO(&writeset);

                /* get current time */
                util_getuptime(&timeofday);
                timeout.tv_sec = 15;

                /* reload config ? */
                if(reloadconf)
                {
                        log_debug("reload configuration");

                        memset(&cfgre, 0, sizeof(struct cfg));

                        if(config_parse_file(&cfgre, cfg.cfgfile) == 0)
                        {
                                if(account_ctl_mapnewcfg(&cfg, &cfgre) == 0)
                                {
                                        /* if change wan ifname, reupdate all
                                         * accounts
                                         */
                                        if(cfgre.wan_cnt_type == wan_cnt_direct
                                           && strcmp(cfgre.wan_ifname,
                                                     cfg.wan_ifname) != 0)
                                        {
                                                account_ctl_needupdate();
                                        }

                                        if(cfgre.wan_cnt_type == wan_cnt_indirect)
                                        {
                                                myip_needupdate();
                                        }

                                        /* use new configuration */
                                        cfgre.cfgfile = strdup(cfg.cfgfile);

                                        config_free(&cfg);
                                        config_copy(&cfg, &cfgre);
                                }
                                else
                                {
                                        log_error("Unable to map the new "
                                                  "configuration. Fix config file");
                                        config_free(&cfgre);
                                }
                        }
                        else
                        {
                                log_error("The new configuration file is "
                                          "invalid. fix it.");
                        }

                        reloadconf = 0;
                }

                /* get the current system wan ip address */
                if((cfg.wan_cnt_type = wan_cnt_direct
                    && util_getifaddr(cfg.wan_ifname, &curr_wanip) == 0)
                   || myip_getwanipaddr(&(cfg.myip), &curr_wanip) == 0)
                {
                        have_wanip = 1;

                        if(curr_wanip.s_addr != wanip.s_addr)
                        {
                                wanip.s_addr = curr_wanip.s_addr;

                                log_debug("new wan ip ! %s",
                                          inet_ntoa(wanip));

                                /* account need to be updated */
                                account_ctl_needupdate();
                        }
                }
                else
                {
                        /* no wan or using myip service ? */
                        have_wanip = 0;
                }

                /* manage accounts */
                account_ctl_manage();

                /* select request candidate fds */
                request_ctl_selectfds(&readset, &writeset, &max_fd);

                /* pselect */
                if(pselect(max_fd + 1,
                           &readset, &writeset, NULL,
                           &timeout, &unblocked) < 0)
                {
                        /* error or interuption */

                        if(!keep_going)
                        {
                                break;
                        }

                        if(reloadconf)
                        {
                                continue;
                        }

                        if(wakeup)
                        {
                                wakeup = 0;
                                continue;
                        }

                        /* very serious cause of error */
                        log_critical("select failed ! %m");
                        ret = 1;
                        break;
                }

                /* process fds with have new state */
                request_ctl_processfds(&readset, &writeset);

                /* save last timeofday */
                memcpy(&lasttimeofday, &timeofday, sizeof(struct timeval));
	}

        log_debug("cleaning before exit");

exit_clean:
        sig_unblockall();

	/* close log */
	log_close();

	/* free allocated config */
	config_free(&cfg);

        /* free ctl */
        request_ctl_cleanup();
        account_ctl_cleanup();

	return ret;
}
