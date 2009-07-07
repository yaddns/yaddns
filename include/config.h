#ifndef _YADDNS_CONFIG_H_
#define _YADDNS_CONFIG_H_

#include "list.h"

enum wan_cnt_type {
        wan_cnt_direct = 0,
        wan_cnt_indirect,
};

struct cfg {
        int wan_cnt_type;
        char *wan_ifname;
        char *optionsfile;
        struct list_head accountcfg_list;
};

struct accountcfg {
        char *name; /* must be unique */
        char *service;
	char *username;
	char *passwd;
	char *hostname;
        struct list_head list;
};

extern int config_parse(struct cfg *cfg, int argc, char **argv);

extern int config_parse_file(struct cfg *cfg, const char *filename);

extern int config_free(struct cfg *cfg);

extern void config_print(struct cfg *cfg);

#endif
