#include "services.h"

#include "service.h"
#include "list.h"

extern struct service dyndns_service;
extern struct service noip_service;
extern struct service ovh_service;

void services_populate_list(void)
{
	INIT_LIST_HEAD(&service_list);
	list_add_tail( &dyndns_service.list, &service_list );
	list_add_tail( &noip_service.list, &service_list );
	list_add_tail( &ovh_service.list, &service_list );
}
