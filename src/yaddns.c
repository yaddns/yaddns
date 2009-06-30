#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "ctl.h"
#include "util.h"

int quitting = 0;
int reloadconf = 0;

void sighdl(int signum)
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

}

int main(int argc, char **argv)
{
	int ret = 0;
	struct sigaction sa;
        struct cfg cfg;

        struct timeval timeout = {0, 0}, timeofday, lasttimeofday = {0, 0};
        struct servicectl *servicectl = NULL;
	fd_set readset, writeset;
	int max_fd = -1;
        
        /* init */
        ctl_init();
        services_populate_list();
        
	/* config */
        memset(&cfg, 0, sizeof(struct cfg));
	if(config_parse(&cfg, argc, argv) != 0)
	{
		return 1;
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

        /* create ctl */
        if(ctl_service_mapcfg(&cfg) != 0)
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
                
                /* unfreeze services ? */
                list_for_each_entry(servicectl,
                                    &servicectl_list, list)
                {
                        if(!servicectl->freezed)
                        {
                                continue;
                        }
                        
                        if(timeofday.tv_sec - servicectl->freeze_time.tv_sec 
                           >= servicectl->freeze_interval.tv_sec)
                        {
                                /* unfreeze ! */
                                servicectl->freezed = 0;
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
                                /* TODO */
                                log_debug("Reloading configuration");
                                reloadconf = 0;
                        }

                        /* TODO, very serious cause of error */
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
