#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>

#include "config.h"
#include "log.h"
#include "service.h"
#include "services.h"
#include "util.h"

#define CFG_DEFAULT_FILENAME "/etc/yaddns.conf"
#define CFG_DEFAULT_WANIFNAME "ppp0"

/*
 * spaces = space, \f, \n, \r, \t and \v
 */
static int config_get_assignment(FILE *file, char *buffer, int buffer_size,
                                 const char *filename, int *linenum,
                                 char **name, char **value)
{
        char *t = NULL, *end = NULL;
        char *n = NULL, *equals = NULL, *v = NULL;
        int ret = -1;

        while(fgets(buffer, buffer_size, file) != NULL)
	{
                (*linenum)++;

                /* search \n */
                end = strchr(buffer, '\n');
                if(!end)
                {
                        /* case of last line not terminated by \n */
                        end = buffer + strlen(buffer) - 1;
                }

                /* remove spaces at end */
                while(end > buffer && isspace(*end))
                {
                        *end = '\0';
                        --end;
                }

                /* remove spaces at start */
                n = buffer;
                while(isspace(*n))
                {
                        n++;
                }

                /* skip comment or empty lines */
		if(n[0] == '#' || n[0] == '\0')
                {
                        continue;
                }

                /* account definition ? */
                if(memcmp(n, "account", sizeof("account") - 1) == 0)
                {
                        /* maybe a account line definition ? */
                        if((equals = strchr(n, '{')) != NULL)
                        {
                                /* remove whitespaces before { */
                                for(t = equals - 1;
                                    t > n && isspace(*t);
                                    t--)
                                {
                                        *t = '\0';
                                }

                                /* maybe { is glued to account ? */
                                *equals = '\0';

                                *name = n;
                                *value = NULL;
                                ret = 0;
                        }
                        else
                        {
                                log_error("parsing error at '%s'. Invalid "
                                          "account declaration. "
                                          "(file %s - line %d)",
                                          n, filename, (*linenum));
                                ret = -1;
                        }

                        break;
                }

                /* maybe end of account definition ? */
                if(memcmp(n, "}", sizeof("}") - 1) == 0)
                {
                        *name = NULL;
                        *value = NULL;
                        ret = 0;
                        break;
                }

                /* need a '=' for a correct assignement  */
                if(!(equals = strchr(n, '=')))
		{
			log_error("parsing error at '%s'. Invalid assignment. "
                                  "(file %s - line %d)",
                                  n, filename, (*linenum));
			break;
		}

                /* remove whitespaces before = */
                for(t = equals - 1;
                    t > n && isspace(*t);
                    t--)
                {
                        *t = '\0';
                }

                /* maybe = is glued to "name" ? */
                *equals = '\0';

		v = equals + 1;

                /* skip spaces */
		while(isspace(*v))
                {
			v++;
                }

                /* remove " if present */
                if(*v == '"')
                {
                        v++;
                }

                /* remove " at end if present */
                if(*end == '"')
                {
                        *end = '\0';
                        --end;
                }

                *name = n;
                *value = v;
                ret = 0;

                break;
        }

        return ret;
}

int config_parse(struct cfg *cfg, int argc, char **argv)
{
        int cfgfile_flag = 0;
        struct service *service = NULL;

        int c, ind;
        char short_options[] = "vhlf:p:DL";
        struct option long_options [] = {
                {"version", no_argument, 0, 'v' },
                {"help", no_argument, 0, 'h' },
                {"list-service", no_argument, 0, 'l' },
                {"daemonize", no_argument, 0, 'D' },
                {"cfg", required_argument, 0, 'f' },
                {"pid-file", required_argument, 0, 'p' },
                {"use-syslog", no_argument, 0, 'L' },
                {0, 0, 0, 0 }
        };

        while((c = getopt_long (argc, argv,
                                short_options, long_options, &ind)) != EOF)
        {
                switch(c)
                {
                case 0:
                        break;

                case 'h':
                        printf("Usage: %s [-f cfg_filename] [-p pid_file] [-D]\n", argv[0]);
                        printf("Options:\n");
                        printf(" -f, --cfg=FILE\t\tConfig file to be used\n");
                        printf(" -p, --pid-file=FILE\tPID file to be used\n");
                        printf(" -D, --daemonize\tDaemonize yaddns\n");
                        printf(" -L, --use-syslog\tSend log messages to syslog instead of stdout.\n");
                        printf(" -h, --help\t\tDisplay this help\n");
                        printf(" -l, --list-service\tDisplay the "
                               "list of available services\n");
                        printf(" -v, --version\t\tDisplay the version\n");
                        return -1;

                case 'v':
                        printf(PACKAGE_STRING "\n");
                        return -1;

                case 'l':
                        printf("List of available services:\n");
                        list_for_each_entry(service,
                                            &service_list, list)
                        {
                                printf("  - %s\n", service->name);
                        }
                        return -1;

                case 'D':
                        cfg->daemonize = 1;
                        break;

                case 'L':
                        cfg->use_syslog = 1;
                        break;

                case 'p':
                        cfgstr_dup(&(cfg->pidfile), optarg);
                        break;

                case 'f':
                        cfgstr_dup(&(cfg->cfgfile), optarg);
                        cfgfile_flag = 1;
                        break;

                default:
                        break;
                }
        }

        if(!cfgstr_is_set(&(cfg->cfgfile)))
        {
                cfgstr_set(&(cfg->cfgfile), CFG_DEFAULT_FILENAME);
        }

        if(config_parse_file(cfg) != 0)
        {
                if(access(cfgstr_get(&(cfg->cfgfile)), F_OK) == 0
                   || cfgfile_flag)
                {
                        config_free(cfg);
                        return -1;
                }
        }

        /* check if there account defined */
        if(list_empty(&(cfg->account_list)))
        {
                log_warning("No account defined.");
        }

	return 0;
}

int config_parse_file(struct cfg *cfg)
{
	FILE *file = NULL;
        int ret = 0;
        long n = 0;
	char buffer[1024];
	int linenum = 0;
	char *name = NULL, *value = NULL;
        int accountdef_scope = 0;
        struct cfg_account *accountcfg = NULL,
                *safe_accountcfg = NULL;
        int myip_assign_count = 0;
        const char *filename = NULL;

        if(!cfgstr_is_set(&(cfg->cfgfile)))
        {
                log_error("Config filename isn't set");
                return -1;
        }

        filename = cfgstr_get(&(cfg->cfgfile));
        log_debug("Trying to parse '%s' config file", filename);

        if((file = fopen(filename, "r")) == NULL)
        {
                log_error("Error when trying to open '%s' config file: %s",
                          filename, strerror(errno));
		return -1;
        }

        while((ret = config_get_assignment(file, buffer, sizeof(buffer),
                                           filename, &linenum,
                                           &name, &value)) != -1)
	{
                log_debug("assignment '%s' = '%s'", name, value);

                if(accountdef_scope)
                {
                        if(name == NULL)
                        {
                                accountdef_scope = 0;

                                /* check and insert */
                                if(!cfgstr_is_set(&(accountcfg->name))
                                   || !cfgstr_is_set(&(accountcfg->service))
                                   || !cfgstr_is_set(&(accountcfg->username))
                                   || !cfgstr_is_set(&(accountcfg->passwd))
                                   || !cfgstr_is_set(&(accountcfg->hostname)))
                                {
                                        log_error("Missing value(s) for "
                                                  "account name '%s' service '%s'"
                                                  "(file %s - line %d)",
                                                  cfgstr_get(&(accountcfg->name)),
                                                  cfgstr_get(&(accountcfg->service)),
                                                  filename, linenum);

                                        cfgstr_unset(&(accountcfg->name));
                                        cfgstr_unset(&(accountcfg->service));
                                        cfgstr_unset(&(accountcfg->username));
                                        cfgstr_unset(&(accountcfg->passwd));
                                        cfgstr_unset(&(accountcfg->hostname));
                                        free(accountcfg);

                                        ret = -1;
                                        break;
                                }

                                list_add(&(accountcfg->list),
                                         &(cfg->account_list));
                        }
                        else if(strcmp(name, "name") == 0)
                        {
                                cfgstr_dup(&(accountcfg->name), value);
                        }
                        else if(strcmp(name, "service") == 0)
                        {
                                cfgstr_dup(&(accountcfg->service), value);
                        }
                        else if(strcmp(name, "username") == 0)
                        {
                                cfgstr_dup(&(accountcfg->username), value);
                        }
                        else if(strcmp(name, "password") == 0)
                        {
                                cfgstr_dup(&(accountcfg->passwd), value);
                        }
                        else if(strcmp(name, "hostname") == 0)
                        {
                                cfgstr_dup(&(accountcfg->hostname), value);
                        }
                        else
                        {
                                log_error("Invalid option name '%s' for "
                                          "account name '%s' (file %s line %d)",
                                          name, cfgstr_get(&(accountcfg->name)),
                                          filename, linenum);

                                ret = -1;
                                break;
                        }
                }
                else if(strcmp(name, "account") == 0)
                {
                        accountdef_scope = 1;
                        accountcfg = calloc(1, sizeof(struct cfg_account));
                        log_debug("add accountcfg '%p'", accountcfg);
                }
                else if(strcmp(name, "wanifname") == 0)
                {
                        cfgstr_dup(&(cfg->wan_ifname), value);
                }
                else if(strcmp(name, "mode") == 0)
                {
                        if(strcmp(value, "indirect") == 0)
                        {
                                cfg->wan_cnt_type = wan_cnt_indirect;
                        }
                        else
                        {
                                cfg->wan_cnt_type = wan_cnt_direct;
                        }
                }
                else if(strcmp(name, "myip_host") == 0)
                {
                        cfgstr_dup(&(cfg->myip.host), value);
                        ++myip_assign_count;
                }
                else if(strcmp(name, "myip_port") == 0)
                {
                        n = strtol_safe(value, -1);
                        if(n == -1 || n <=0 || n > 65535)
                        {
                                log_error("Invalid myip port %s", value);
                                ret = -1;
                                break;
                        }

                        cfg->myip.port = (unsigned short int)n;
                        ++myip_assign_count;
                }
                else if(strcmp(name, "myip_path") == 0)
                {
                        cfgstr_dup(&(cfg->myip.path), value);
                        ++myip_assign_count;
                }
                else if(strcmp(name, "myip_upint") == 0)
                {
                        n = strtol_safe(value, -1);
                        if(n == -1 || n <=0 || n > INT_MAX)
                        {
                                log_error("Invalid myip upint %s", value);
                                ret = -1;
                                break;
                        }

                        cfg->myip.upint = (int)n;
                        ++myip_assign_count;
                }
                else
                {
                        log_error("Invalid option name '%s' (file %s "
                                  "line %d)",
                                  name, filename, linenum);
                        ret = -1;
                        break;
                }

        }

        if(ret == -1 && feof(file))
        {
                log_debug("End of configuration file reached");
                ret = 0;
        }

        if(cfg->wan_cnt_type == wan_cnt_direct
           && !cfgstr_is_set(&(cfg->wan_ifname)))
        {
                log_warning("wan ifname isn't defined. Use ppp0 by default");
                cfgstr_set(&(cfg->wan_ifname), CFG_DEFAULT_WANIFNAME);
        }

        if(cfg->wan_cnt_type == wan_cnt_indirect)
        {
                if(!cfgstr_is_set(&(cfg->myip.host))
                   || cfg->myip.port == 0
                   || !cfgstr_is_set(&(cfg->myip.path))
                   || cfg->myip.upint == 0)
                {
                        log_error("Invalid myip definition(s)."
                                  " Check config file.");
                        ret = -1;
                }
        }

        if(accountdef_scope)
        {
                log_error("No found closure for account name '%s' service '%s' "
                          "(file %s line %d)",
                          cfgstr_get(&(accountcfg->name)),
                          cfgstr_get(&(accountcfg->service)),
                          filename, linenum);
                ret = -1;
        }

        if(ret == -1)
        {
                /* error. need to cleanup */
                cfgstr_unset(&(cfg->wan_ifname));
                cfgstr_unset(&(cfg->myip.host));
                cfgstr_unset(&(cfg->myip.path));

                list_for_each_entry_safe(accountcfg, safe_accountcfg,
                                         &(cfg->account_list), list)
                {
                        cfgstr_unset(&(accountcfg->name));
                        cfgstr_unset(&(accountcfg->service));
                        cfgstr_unset(&(accountcfg->username));
                        cfgstr_unset(&(accountcfg->passwd));
                        cfgstr_unset(&(accountcfg->hostname));

                        list_del(&(accountcfg->list));
                        free(accountcfg);
                }

        }

        fclose(file);

	return ret;
}

struct cfg_account *config_account_get(const struct cfg *cfg, const char *name)
{
        struct cfg_account *accountcfg = NULL;

        list_for_each_entry(accountcfg,
                            &(cfg->account_list), list)
        {
                if(strcmp(cfgstr_get(&(accountcfg->name)), name) == 0)
                {
                        return accountcfg;
                }
        }

        return NULL;
}

void config_init(struct cfg *cfg)
{
        memset(cfg, 0, sizeof(struct cfg));

        INIT_LIST_HEAD( &(cfg->account_list) );
}

int config_free(struct cfg *cfg)
{
        struct cfg_account *accountcfg = NULL,
                *safe = NULL;

        cfgstr_unset(&(cfg->wan_ifname));
        cfgstr_unset(&(cfg->myip.host));
        cfgstr_unset(&(cfg->myip.path));
        cfgstr_unset(&(cfg->cfgfile));
        cfgstr_unset(&(cfg->pidfile));

        list_for_each_entry_safe(accountcfg, safe,
                                 &(cfg->account_list), list)
        {
                cfgstr_unset(&(accountcfg->name));
                cfgstr_unset(&(accountcfg->service));
                cfgstr_unset(&(accountcfg->username));
                cfgstr_unset(&(accountcfg->passwd));
                cfgstr_unset(&(accountcfg->hostname));

                list_del(&(accountcfg->list));
                free(accountcfg);
        }

	return 0;
}

void config_print(struct cfg *cfg)
{
        struct cfg_account *accountcfg = NULL;

        printf("Configuration:\n");
        printf(" cfg file = '%s'\n", cfgstr_get(&(cfg->cfgfile)));
        printf(" pid file = '%s'\n", cfgstr_get(&(cfg->pidfile)));
        printf(" daemonize = '%d'\n", cfg->daemonize);
        printf(" use syslog = '%d'\n", cfg->use_syslog);
        printf(" wan ifname = '%s'\n", cfgstr_get(&(cfg->wan_ifname)));
        printf(" wan mode = '%d'\n", cfg->wan_cnt_type);

        list_for_each_entry(accountcfg,
                            &(cfg->account_list), list)
        {
                printf(" ---- account name '%s' ----\n",
                       cfgstr_get(&(accountcfg->name)));
                printf("   service = '%s'\n",
                       cfgstr_get(&(accountcfg->service)));
                printf("   username = '%s'\n",
                       cfgstr_get(&(accountcfg->username)));
                printf("   password = '%s'\n",
                       cfgstr_get(&(accountcfg->passwd)));
                printf("   hostname = '%s'\n",
                       cfgstr_get(&(accountcfg->hostname)));
        }
}

void config_move(struct cfg *cfgsrc, struct cfg *cfgdst)
{
        struct cfg_account *actcfg = NULL,
                *safe_actcfg = NULL;

        /* general cfg */
        cfgdst->wan_cnt_type = cfgsrc->wan_cnt_type;
        cfgstr_move(&(cfgsrc->wan_ifname), &(cfgdst->wan_ifname));
        cfgstr_move(&(cfgsrc->cfgfile), &(cfgdst->cfgfile));
        cfgstr_move(&(cfgsrc->pidfile), &(cfgdst->pidfile));
        cfgdst->daemonize = cfgsrc->daemonize;
        cfgdst->use_syslog = cfgsrc->use_syslog;

        /* myip cfg */
        cfgstr_move(&(cfgsrc->myip.host), &(cfgdst->myip.host));
        cfgstr_move(&(cfgsrc->myip.path), &(cfgdst->myip.path));
        cfgdst->myip.port = cfgsrc->myip.port;
        cfgdst->myip.upint = cfgsrc->myip.upint;

        /* account(s) cfg */
        list_for_each_entry_safe(actcfg, safe_actcfg,
                                 &(cfgsrc->account_list), list)
        {
                list_move(&(actcfg->list), &(cfgdst->account_list));
        }

        /* it's a move, so clean up src config */
        config_free(cfgsrc);
}
