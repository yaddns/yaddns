#ifndef _YADDNS_CTL_H_
#define _YADDNS_CTL_H_

#include <sys/time.h>

#include "list.h"
#include "config.h"
#include "service.h"

struct servicectl {
	enum {
                SNeedToUpdate,
                SUpdating,
                SUpdated,
        } status;
        struct service *def;
	struct servicecfg *cfg;
	struct timeval last_update;
	int locked;
	int freezed;
	struct timeval freeze_time;
	struct timeval freeze_interval;
        struct list_head list;
};

struct updatepkt {
        struct servicectl *ctl;
        int s; /* socket */
        enum {
                ECreated,
                EConnecting,
                ESending,
                EWaitingForResponse,
                EFinished,
                EError,
        } state;
        char buf[512];
        ssize_t buf_tosend;
        ssize_t buf_sent;
        struct list_head list;
};

/********* ctl.c *********/
extern struct list_head servicectl_list;

/* init the allocated structures */
extern void ctl_init(void);

/* free the allocated structures */
extern void ctl_free(void);

/* check wan ip */
extern void ctl_preselect(struct cfg *cfg);

/* select fds ready to fight */
extern void ctl_selectfds(fd_set *readset, fd_set *writeset, int * max_fd);

/* process fds to do something  */
extern void ctl_processfds(fd_set *readset, fd_set *writeset);

/* after reading cfg, create service controler */
extern int ctl_service_mapcfg(struct cfg *cfg);

/********* services.c *********/
extern struct list_head service_list;

extern void services_populate_list(void);

#endif
