/*
 *  Yaddns - Yet Another ddns client
 *  Copyright (C) 2008 Anthony Viallard <anthony.viallard@patatrac.info>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "controler.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "event.h"
#include "defs.h"

#define START_FLAG 0x00
#define STOP_FLAG 0x01
#define WAIT_CONF_MOD_FLAG 0x02

short int service_flag;
char ip_currently_updated[16];

int controler_start(void)
{
	char wan_ip[16];
	int up_rc;

	/* set flags */
	service_flag = START_FLAG;

	/* default */
	snprintf(ip_currently_updated, sizeof(ip_currently_updated), "%s", "0.0.0.0");
	
	/* initialize service */
	if(service->ctor != NULL)
	{
		service->ctor();
	}
		 
	/* the main loop */
	while(service_flag != STOP_FLAG)
	{
		/* check event */
		if(event_check(EVENT_EXIT))
		{
			LAYER_LOG_NOTICE("Exit event received - exit program cleanly");
			
			service_flag = STOP_FLAG;
			event_del(EVENT_EXIT);
			continue;
		}
		else if(event_check(EVENT_RELOAD_CONF))
		{
			LAYER_LOG_NOTICE("Reload conf event received - reload config");
			
			if(layer->conf_reload() != 0)
			{
				LAYER_LOG_FATAL("reload config failed ! - exit program cleanly");
				service_flag = STOP_FLAG;
			}
			else if(service_flag == WAIT_CONF_MOD_FLAG)
			{
				service_flag = START_FLAG;
			}
			
			event_del(EVENT_RELOAD_CONF);
			continue;
		}
		
		/* wait conf modification ? yes ? loop ! */
		if(service_flag == WAIT_CONF_MOD_FLAG)
		{
			LAYER_LOG_DEBUG("service_flag == WAIT_CONF_MOD_FLAG; wait");
			sleep(1);
			continue;
		}

		/* get the current wan ip */
		if(layer->get_wan_ip(wan_ip, sizeof(wan_ip)) != 0)
		{
			LAYER_LOG_WARN("Unable to get wan ip ! Temporary wan failure ?");
			layer->publish_status_code(service->name, wa_sc_nowan);
			sleep(60);
			continue;
		}
		
		/* check if we need update */
		if(strcmp(wan_ip, ip_currently_updated) == 0)
		{
			LAYER_LOG_DEBUG("no new ip address");
			sleep(60);
			continue;
		}
		
		LAYER_LOG_INFO("new ip address detected ! need update !");
		
		/* we need to update. Call service update function */
		up_rc = service->update(wan_ip);
		
		/* publish update status to layer */
		layer->publish_status_code(service->name, up_rc);
		
		/* update variable environment */
		if(up_rc == up_sc_good || up_rc == up_sc_nochg)
		{
			/* All is fine ! (up_rc_nochg => need to temporize) */
			/* save the ip use in update */
			snprintf(ip_currently_updated, sizeof(ip_currently_updated),
				 "%s", wan_ip);
		}
		else if(up_rc == wa_sc_warning || up_rc == wa_sc_nowan)
		{
			/* no wan */
			LAYER_LOG_WARN("Temporary wan failure ?");
			printf("!!!!!!!!!!!!!!!!!!!!!!\n");
			sleep(60);
		}
		else if(up_rc == up_sc_servererror)
		{
			/* temporary server error ? */
			sleep(360); /* need to sleep 1 hour */
		}
		else if(up_rc >= fe_sc_error)
		{
			LAYER_LOG_FATAL("Need to stop after fatal error send by update ! (code='%d')", up_rc);
			service_flag = STOP_FLAG;
		}
		else
		{
			/* 
			 * not stop program, wait user conf modification 
			 * and signal
			 */
			LAYER_LOG_DEBUG("WAIT_CONF_MOD_FLAG !!");
			
			service_flag = WAIT_CONF_MOD_FLAG;
		}
	}

	/* uninitialize service */
	if(service->dtor != NULL)
	{
		service->dtor();
	}

	return 0;
}

int controler_stop(void)
{
	service_flag = STOP_FLAG;

	return 0;
}
