#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "ctl.h"
#include "util.h"

volatile int quitting = 0;
volatile int reloadconf = 0;
volatile int wakeup = 0;
struct timeval timeofday = {0, 0};

static void sighdl(int signum)
{
	if(signum == SIGTERM || signum == SIGINT)
	{
		log_notice("Receive SIGTERM or SIGINT. Exit.");
		quitting = 1;
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

int main(int argc, char **argv)
{
	int ret = 0;
	struct sigaction sa;
        struct cfg cfg, cfgre;

        struct timeval timeout = {0, 0}, 
                lasttimeofday = {0, 0};
        struct accountctl *accountctl = NULL;
	fd_set readset, writeset;
	int max_fd = -1;
	FILE *fpid = NULL;
        
        /* init */
        ctl_init();
        services_populate_list();
        
	/* config */
        memset(&cfg, 0, sizeof(struct cfg));
	if(config_parse(&cfg, argc, argv) != 0)
	{
		return 1;
	}

        /* daemonize ? */
        if(cfg.daemonize)
        {
                if(daemon(0, 0) < 0)
                {
                        log_error("Failed to daemonize !");
                        return 1;
                }
        }
        
	/* open log */
	log_open();

	/* sighandler */
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sighdl;

	if(sigaction(SIGTERM, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGTERM: %m");
		ret = 1;
		goto exit_clean;
	}
	
	if(sigaction(SIGINT, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGINT: %m");
		ret = 1;
		goto exit_clean;
	}
        
        if(sigaction(SIGHUP, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGHUP: %m");
		ret = 1;
		goto exit_clean;
	}
        
        if(sigaction(SIGUSR1, &sa, NULL) != 0)
	{
		log_error("Failed to install signal handler for SIGUSR1: %m");
		ret = 1;
		goto exit_clean;
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
        if(ctl_account_mapcfg(&cfg) != 0)
        {
                ret = 1;
                goto exit_clean;
        }
        
	/* yaddns loop */
	while(!quitting)
	{
                max_fd = 0;
                
                util_getuptime(&timeofday);
                timeout.tv_sec = 15;
                
                /* reload config ? */
                if(reloadconf)
                {
                        log_debug("reload configuration");
                        
                        memset(&cfgre, 0, sizeof(struct cfg));
                        
                        if(config_parse_file(&cfgre, cfg.optionsfile) == 0)
                        {
                                if(ctl_account_mapnewcfg(&cfg, &cfgre) == 0)
                                {
                                        /* if change wan ifname, reupdate all 
                                         * accounts 
                                         */
                                        if(cfgre.wan_cnt_type == wan_cnt_direct
                                           && strcmp(cfgre.wan_ifname,
                                                     cfg.wan_ifname) != 0)
                                        {
                                                list_for_each_entry(accountctl,
                                                                    &accountctl_list, 
                                                                    list)
                                                {
                                                        accountctl->updated = 0;
                                                }
                                        }

                                        /* use new configuration */
                                        cfgre.optionsfile = strdup(cfg.optionsfile);
                                        
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

                /* unfreeze services ? */
                list_for_each_entry(accountctl,
                                    &accountctl_list, list)
                {
                        if(!accountctl->freezed)
                        {
                                continue;
                        }
                        
                        if(timeofday.tv_sec - accountctl->freeze_time.tv_sec 
                           >= accountctl->freeze_interval.tv_sec)
                        {
                                /* unfreeze ! */
                                accountctl->freezed = 0;
                        }
                }

                /* wan ip has changed ? */
                ctl_preselect(&cfg);

                /* select sockets ready to fight */
		FD_ZERO(&readset);
                FD_ZERO(&writeset);
                
                ctl_selectfds(&readset, &writeset, &max_fd);
                
                /* select */
                log_debug("select !");
                
                if(select(max_fd + 1, &readset, &writeset, 0, &timeout) < 0)
                {
                        /* error or interuption */
                        
                        if(quitting)
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

                /* there are informations to read ? */
                ctl_processfds(&readset, &writeset);

                /* save last timeofday */
                memcpy(&lasttimeofday, &timeofday, sizeof(struct timeval));
	}

        log_debug("cleaning before exit");
        
        /* free ctl */
        ctl_free();

exit_clean:
	/* close log */
	log_close();

	/* free allocated config */
	config_free(&cfg);

	return ret;
}
