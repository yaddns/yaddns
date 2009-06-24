#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"
#include "log.h"

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
	
        memset(&cfg, 0, sizeof(struct cfg));
        
	/* get config */
	if(config_parse(&cfg, argc, argv) != 0)
	{
		return 1;
	}

        config_print(&cfg);
	
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

	/* init service */
	
	/* yaddns loop */
	while(!quitting)
	{
		sleep(5);
	}

exit_clean:
	/* close log */
	log_close();

	/* free allocated config */
	config_free(&cfg);

	return ret;
}
