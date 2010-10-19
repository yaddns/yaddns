#include "service.h"
#include "list.h"

struct list_head service_list;

extern struct service dyndns_service;

void services_populate_list(void)
{
INIT_LIST_HEAD(&service_list);
list_add_tail( &dyndns_service.list, &service_list );
}
