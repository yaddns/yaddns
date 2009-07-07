#ifndef _YADDNS_SERVICE_H_
#define _YADDNS_SERVICE_H_

#include <string.h>

#include "list.h"
#include "config.h"

struct upreply_report {
	enum {
                up_success,
                up_unknown_error,
                up_syntax_error,
                up_account_error,
                up_account_loginpass_error,
                up_account_hostname_error,
                up_account_abuse_error,
                up_server_error,
        } code;
        char custom_rc[16]; /* small return code receive by server */
        char custom_rc_text[64]; /* explanation of return code in english */
	int rcmd_lock; /* lock or not ? */
        int rcmd_freeze; /* freeze or not ? */
        int rcmd_freezetime; /* freezetime recommendation in seconds */
};

struct service {
	const char const *name;
	const char const *ipserv;
        short int portserv;
	int (*ctor) (void);
	int (*dtor) (void);
	int (*make_up_query) (const struct accountcfg cfg, 
			      const char const *newwanip, 
			      char *buffer,
			      size_t buffer_size);
	int (*read_up_resp) (char *resp,
			     struct upreply_report *report);
	struct list_head list;
};

#endif
