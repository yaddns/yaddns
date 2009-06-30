#ifndef _YADDNS_SERVICE_H_
#define _YADDNS_SERVICE_H_

#include <string.h>

#include "list.h"
#include "config.h"

struct upreply_report {
	int code;
	int lock_wnsc; /* lock wait new service configuration */
	int freezetime;
};

struct service {
	const char const *name;
	const char const *ipserv;
        short int portserv;
	int (*ctor) (void);
	int (*dtor) (void);
	int (*make_up_query) (const struct servicecfg cfg, 
			      const char const *newwanip, 
			      char *buffer,
			      size_t buffer_size);
	int (*read_up_resp) (char *resp,
			     struct upreply_report *report);
	struct list_head list;
};

#endif
