#ifndef _YADDNS_MYIP_H_
#define _YADDNS_MYIP_H_

#include "config.h"

int myip_getwanipaddr(const struct cfg_myip *cfg_myip, struct in_addr *wanaddr);

void myip_needupdate(void);

#endif
