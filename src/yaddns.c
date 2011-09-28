#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
static volatile sig_atomic_t unfreeze = 0;

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
        else if(signum == SIGUSR2)
        {
                log_notice("Receive SIGUSR2. Unfreeze accounts !");
                unfreeze = 1;
        }
}

static int sig_setup(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sig_handler;

	if(sigaction(SIGTERM, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGTERM: %s",
                          strerror(errno));
		return -1;
	}

	if(sigaction(SIGINT, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGINT: %s",
                          strerror(errno));
		return -1;
	}

        if(sigaction(SIGHUP, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGHUP: %s",
                          strerror(errno));
		return -1;
	}

        if(sigaction(SIGUSR1, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGUSR1: %s",
                          strerror(errno));
		return -1;
	}

        if(sigaction(SIGUSR2, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGUSR2: %s",
                          strerror(errno));
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

static void wanip_manage(const struct cfg *cfg)
{
        int ret;
        struct in_addr fresh_wanip;

        /* get the current system wan ip address */
        if(cfg->wan_cnt_type == wan_cnt_direct)
        {
                ret = util_getifaddr(cfgstr_get(&(cfg->wan_ifname)),
                                     &fresh_wanip);
        }
        else
        {
                ret = myip_getwanipaddr(&(cfg->myip), &fresh_wanip);
        }

        if(ret != 0)
        {
                have_wanip = 0;
        }
        else
        {
                have_wanip = 1;

                if(fresh_wanip.s_addr != wanip.s_addr)
                {
                        wanip.s_addr = fresh_wanip.s_addr;

                        log_notice("We have a new wan ip = %s !",
                                  inet_ntoa(wanip));

                        /* account need to be updated */
                        account_ctl_needupdate();
                }
        }
}

static void wanip_needupdate(const struct cfg *cfg)
{
        if(cfg->wan_cnt_type == wan_cnt_indirect)
        {
                myip_needupdate();
        }
}

static int reload_conf(struct cfg *cfg)
{
        struct cfg cfgre;
        int ret = -1;

        config_init(&cfgre);
        cfgstr_copy(&(cfg->cfgfile), &(cfgre.cfgfile));

        if(config_parse_file(&cfgre) != 0)
        {
                log_error("The new configuration file is invalid. Fix it.");
                return -1;
        }

        if(account_ctl_mapnewcfg(&cfgre) == 0)
        {
                if(cfgre.wan_cnt_type == wan_cnt_direct)
                {
                        if(strcmp(cfgstr_get(&(cfgre.wan_ifname)),
                                  cfgstr_get(&(cfg->wan_ifname))) != 0)
                        {
                                /* if wan ifname change, reupdate all
                                 * accounts
                                 */
                                account_ctl_needupdate();
                        }
                }
                else if(cfgre.wan_cnt_type == wan_cnt_indirect)
                {
                        myip_needupdate();
                }

                /* update configuration */
                config_move(&cfgre, cfg);

                ret = 0;
        }
        else
        {
                log_error("Unable to map the new configuration."
                          " Fix config file");
                ret = -1;
        }

        config_free(&cfgre);

        return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;
        sigset_t unblocked;
        struct cfg cfg;
        struct timespec timeout = {0, 0};
	fd_set readset, writeset;
	int max_fd = -1;
	FILE *fpid = NULL;

        /* init */
        account_ctl_init();
        request_ctl_init();
        services_populate_list();
        inet_aton("0.0.0.0", &wanip);
        config_init(&cfg);

        /* sig setup */
        if(sig_setup() != 0)
        {
                ret = 1;
                goto exit_clean;
        }

        sig_blockall();

        sigemptyset(&unblocked);

	/* config */
	if(config_parse(&cfg, argc, argv) != 0)
	{
                ret = 1;
                goto exit_clean;
	}

#if defined(DEBUG_LOG)
        config_print(&cfg);
#endif

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
        if(cfgstr_is_set(&(cfg.pidfile)))
        {
                if((fpid = fopen(cfgstr_get(&(cfg.pidfile)), "w")))
                {
                        fprintf(fpid, "%u", getpid());
                        fclose(fpid);
                        fpid = NULL;
                }
                else
                {
                        log_error("Failed to create pidfile %s: %s",
                                  cfgstr_get(&(cfg.pidfile)),
                                  strerror(errno));
                        ret = 1;
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

                /* manage wan ip address */
                wanip_manage(&cfg);

                /* manage accounts */
                account_ctl_manage(&cfg);

                /* select request candidate fds */
                request_ctl_selectfds(&readset, &writeset, &max_fd);

                /* pselect */
                timeout.tv_sec = 15;
                if(pselect(max_fd + 1,
                           &readset, &writeset, NULL,
                           &timeout, &unblocked) < 0)
                {
                        if(errno == EINTR)
                        {
                                /* signal was caught */
                                log_debug("Signal was caught in pselect()");

                                if(!keep_going)
                                {
                                        break;
                                }

                                if(reloadconf)
                                {
                                        log_debug("reload configuration");

                                        reload_conf(&cfg);

                                        reloadconf = 0;
                                }

                                if(wakeup)
                                {
                                        log_debug("order retrieve wan ip addr");

                                        wanip_needupdate(&cfg);

                                        wakeup = 0;
                                }

                                if(unfreeze)
                                {
                                        log_debug("unfreeze all accounts");

                                        account_ctl_unfreeze_all();

                                        unfreeze = 0;
                                }
                                continue;
                        }

                        /* very serious cause of error */
                        log_critical("select failed ! %s",
                                     strerror(errno));
                        ret = 1;
                        break;
                }

                /* process fds with have new state */
                request_ctl_processfds(&readset, &writeset);
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
