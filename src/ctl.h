#ifndef _YADDNS_CTL_H_
#define _YADDNS_CTL_H_

#include <sys/time.h>

#include "list.h"
#include "config.h"
#include "service.h"

struct accountctl {
	enum {
                SError = -1,
                SOk = 1,
                SWorking,
        } status;
        struct service *def;
	struct accountcfg *cfg;
	struct timeval last_update;
        int updated; /* account is updated ? */
	int locked;
	int freezed;
	struct timeval freeze_time;
	struct timeval freeze_interval;
        struct list_head list;
};

struct updatepkt {
        struct accountctl *ctl;
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
extern struct list_head accountctl_list;

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

/* after reading cfg, create account controlers */
extern int ctl_account_mapcfg(struct cfg *cfg);

/* after reading a new cfg, resync controler */
extern int ctl_account_mapnewcfg(struct cfg *oldcfg,
                                 const struct cfg *newcfg);

extern struct accountctl *ctl_account_get(const char *accountname);

/********* services.c *********/
extern struct list_head service_list;

extern void services_populate_list(void);

/********* yaddns.c ***********/
extern struct timeval timeofday;

#endif
