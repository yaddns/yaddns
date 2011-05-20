#ifndef _YADDNS_CONFIG_H_
#define _YADDNS_CONFIG_H_

#include "list.h"
#include "cfgstr.h"

struct cfg_myip {
        struct cfgstr host;
        unsigned short int port;
        struct cfgstr path;
        int upint;
};

struct cfg {
        enum {
                wan_cnt_direct = 0,
                wan_cnt_indirect,
        } wan_cnt_type;
        struct cfgstr wan_ifname;
        struct cfg_myip myip;
        struct cfgstr cfgfile;
        struct cfgstr pidfile;
        int daemonize;
        int use_syslog;
        struct list_head account_list;
};

struct cfg_account {
        struct cfgstr name; /* must be unique */
        struct cfgstr service;
	struct cfgstr username;
	struct cfgstr passwd;
	struct cfgstr hostname;
        struct list_head list;
};

extern int config_parse(struct cfg *cfg, int argc, char **argv);

extern int config_parse_file(struct cfg *cfg);

extern void config_init(struct cfg *cfg);

extern int config_free(struct cfg *cfg);

extern struct cfg_account * config_account_get(const struct cfg *cfg, const char *name);

extern void config_print(struct cfg *cfg);

extern void config_move(struct cfg *cfgsrc, struct cfg *cfgdst);

#endif
