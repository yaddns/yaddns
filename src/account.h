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

#ifndef _YADDNS_CTL_H_
#define _YADDNS_CTL_H_

#include <sys/time.h>

#include "list.h"
#include "config.h"
#include "service.h"

struct account {
	enum {
                ASError = -1,
                ASHatched = 0,
                ASOk = 1,
                ASWorking,
        } status;
        struct service *def;
	struct cfg_account *cfg;
	struct timeval last_update;
        int updated; /* account is updated ? */
	int locked;
	int freezed;
	struct timeval freeze_time;
	struct timeval freeze_interval;
        struct list_head list;
};

/********* ctl.c *********/
extern struct list_head account_list;

/* init the allocated structures */
extern void account_ctl_init(void);

/* free the allocated structures */
extern void account_ctl_cleanup(void);

/* manage account list:
 * - unfreeze account if freeze time is over
 * ...
 */
extern void account_ctl_manage(const struct cfg *cfg);

/* set not updated all accounts
 */
extern void account_ctl_needupdate(void);

/* unfreeze all accounts
 */
extern void account_ctl_unfreeze_all(void);

/* after reading cfg, create account controlers */
extern int account_ctl_mapcfg(struct cfg *cfg);

/* after reading a new cfg, resync controler */
extern int account_ctl_mapnewcfg(const struct cfg *newcfg);

extern struct account *account_ctl_get(const char *accountname);

/********* services.c *********/
extern struct list_head service_list;

#endif
