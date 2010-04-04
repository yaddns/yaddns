#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>

#include "config.h"
#include "log.h"
#include "ctl.h"
#include "service.h"

#define CFG_DEFAULT_FILENAME "/etc/yaddns.conf"

#define CFG_FREE(itname) do {                                           \
                if(itname != NULL)                                      \
                {                                                       \
                        free(itname);                                   \
                        itname = NULL;                                  \
                }                                                       \
        } while(0)

#define CFG_SET_VALUE(itname, value) do {                               \
                CFG_FREE(itname);                                       \
                itname = strdup_trim(value);                            \
        } while(0)

static char *strdup_trim (const char *s)
{
        size_t begin, end;
        char *r = NULL;

        if (!s)
                return NULL;

        end = strlen (s) - 1;

        for (begin = 0 ; begin < end ; begin++)
        {
                if (s[begin] != ' ' && s[begin] != '\t' && s[begin] != '"')
                {
                        break;
                }
        }

        for (; begin < end ; end--)
        {
                if (s[end] != ' ' && s[end] != '\t' && s[end] != '"' 
                    && s[end] != '\n')
                {
                        break;
                }
        }

        r = strndup (s + begin, end - begin + 1);

        return r;
}

static int config_get_assignment(FILE *file, char *buffer, size_t buffer_size,
                                 const char *filename, int *linenum,
                                 char **name, char **value)
{
        char *t = NULL;
        char *n = NULL, *equals = NULL, *v = NULL;
        int ret = -1;
        
        while(fgets(buffer, buffer_size, file) != NULL)
	{
                (*linenum)++;
                
                /* make buffer a correct string without space at end */
                t = strchr(buffer, '\n'); 
                if(t)
                {
                        *t = '\0';
                        t--;
                        while((t >= buffer) && isspace(*t))
                        {
                                *t = '\0';
                                t--;
                        }
                }
                
                /* remove space at start */
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
                
                /* skip leading whitespaces */
		while(isspace(*v))
                {
			v++;
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
        int optionsfile_flag = 0;
        struct service *service = NULL;
        
        int c, ind;
        char short_options[] = "vhlf:p:D";
        struct option long_options [] = {
                {"version", no_argument, 0, 'v' },
                {"help", no_argument, 0, 'h' },
                {"list-service", no_argument, 0, 'l' },
                /*{"daemon", no_argument, 0, 'D' },*/
                {"cfg", required_argument, 0, 'f' },
                {"pid-file", required_argument, 0, 'p' },
                {"daemonize", required_argument, 0, 'D' },
                {0, 0, 0, 0 }
        };

        cfg->optionsfile = NULL;
        cfg->pidfile = NULL;
        cfg->daemonize = 0;
        
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
                        printf(" -D, --daemonize\t\tDaemonize yaddns\n");
                        printf(" -h, --help\t\tDisplay this help\n");
                        printf(" -l, --list-service\tDisplay the "
                               "list of available services\n");
                        printf(" -v, --version\t\tDisplay the version\n");
                        return -1;
                        
                case 'v':
                        printf(D_NAME " - version " D_VERSION "\n");
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
                        
                case 'p':
                        cfg->pidfile = strdup(optarg);
                        break;
                        
                case 'f':
                        cfg->optionsfile = strdup(optarg);
                        optionsfile_flag = 1;
                        break;
                        
                default:
                        break;
                }
        }

        cfg->wan_cnt_type = wan_cnt_direct;
        cfg->wan_ifname = NULL;
        if(cfg->optionsfile == NULL)
        {
                cfg->optionsfile = strdup(CFG_DEFAULT_FILENAME);
        }
        
        if(config_parse_file(cfg, cfg->optionsfile) != 0)
        {
                if(access(cfg->optionsfile, F_OK) == 0 || optionsfile_flag)
                {
                        config_free(cfg);
                        return -1;
                }
        }
        
        if(cfg->wan_ifname == NULL)
        {
                log_warning("wan ifname isn't defined. Use ppp0 by default");
                cfg->wan_ifname = strdup("ppp0");
        }

        /* check if there account defined */
        if(list_empty(&(cfg->accountcfg_list)))
        {
                log_warning("No account defined.");
        }

	return 0;
}

int config_parse_file(struct cfg *cfg, const char *filename)
{
	FILE *file = NULL;
        int ret = 0;
	char buffer[1024];
	int linenum = 0;
	char *name = NULL, *value = NULL;
        int accountdef_scope = 0;
        struct accountcfg *accountcfg = NULL,
                *safe_accountcfg = NULL;
        
        INIT_LIST_HEAD( &(cfg->accountcfg_list) );
        
        log_debug("Trying to parse '%s' config file", filename);
        
        if((file = fopen(filename, "r")) == NULL)
        {
                log_error("Error when trying to open '%s' config file: %m",
                          filename);
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
                                if(accountcfg->name == NULL
                                   || accountcfg->service == NULL
                                   || accountcfg->username == NULL
                                   || accountcfg->passwd == NULL
                                   || accountcfg->hostname == NULL)
                                {
                                        log_error("Missing value(s) for "
                                                  "account name '%s' service '%s'"
                                                  "(file %s - line %d)",
                                                  accountcfg->name,
                                                  accountcfg->service,
                                                  filename, linenum);
                                        
                                        CFG_FREE(accountcfg->name);
                                        CFG_FREE(accountcfg->service);
                                        CFG_FREE(accountcfg->username);
                                        CFG_FREE(accountcfg->passwd);
                                        CFG_FREE(accountcfg->name);
                                        free(accountcfg);
                                        
                                        ret = -1;
                                        break;
                                }

                                list_add(&(accountcfg->list), 
                                         &(cfg->accountcfg_list));
                        }
                        else if(strcmp(name, "name") == 0)
                        {
                                CFG_SET_VALUE(accountcfg->name, value);
                        }
                        else if(strcmp(name, "service") == 0)
                        {
                                CFG_SET_VALUE(accountcfg->service, value);
                        }
                        else if(strcmp(name, "username") == 0)
                        {
                                CFG_SET_VALUE(accountcfg->username, value);
                        }
                        else if(strcmp(name, "password") == 0)
                        {
                                CFG_SET_VALUE(accountcfg->passwd, value);
                        }
                        else if(strcmp(name, "hostname") == 0)
                        {
                                CFG_SET_VALUE(accountcfg->hostname, value);
                        }
                        else
                        {
                                log_error("Invalid option name '%s' for "
                                          "an account (file %s line %d)",
                                          name, filename, linenum);
                                
                                ret = -1;
                                break;
                        }
                }
                else if(strcmp(name, "account") == 0)
                {
                        accountdef_scope = 1;
                        accountcfg = calloc(1, sizeof(struct accountcfg));
                        log_debug("add accountcfg '%p'", accountcfg);
                }
                else if(strcmp(name, "wanifname") == 0)
                {
                        CFG_SET_VALUE(cfg->wan_ifname, value);
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
                else
                {
                        log_error("Invalid option name '%s' (file %s "
                                  "line %d)",
                                  name, filename, linenum);
                        ret = -1;
                        break;
                }
                
        }
        
        if(ret == -1)
        {
                if(feof(file))
                {
                        /* end of file */
                        log_debug("End of configuration file");
                        ret = 0;
                }
                else
                {
                        /* error. need to cleanup */
                        CFG_FREE(cfg->wan_ifname);
                        
                        list_for_each_entry_safe(accountcfg, safe_accountcfg,
                                                 &(cfg->accountcfg_list), list)
                        {
                                CFG_FREE(accountcfg->name);
                                CFG_FREE(accountcfg->service);
                                CFG_FREE(accountcfg->username);
                                CFG_FREE(accountcfg->passwd);
                                CFG_FREE(accountcfg->name);
                                
                                list_del(&(accountcfg->list));
                                free(accountcfg);
                        }
                                        
                }
        }

        if(accountdef_scope)
        {
                log_error("No found closure for account name '%s' service '%s' "
                          "(file %s line %d)", 
                          accountcfg->name, accountcfg->service, 
                          filename, linenum);
                ret = -1;
        }

        fclose(file);
        
	return ret;
}

struct accountcfg * config_account_get(const struct cfg *cfg, const char *name)
{
        struct accountcfg *accountcfg = NULL;
        
        list_for_each_entry(accountcfg,
                            &(cfg->accountcfg_list), list) 
        {
                if(strcmp(accountcfg->name, name) == 0)
                {
                        return accountcfg;
                }
        }

        return NULL;
}

int config_free(struct cfg *cfg)
{
        struct accountcfg *accountcfg = NULL,
                *safe = NULL;
        
        CFG_FREE(cfg->wan_ifname);
        CFG_FREE(cfg->optionsfile);
        CFG_FREE(cfg->pidfile);
        
        list_for_each_entry_safe(accountcfg, safe, 
                                 &(cfg->accountcfg_list), list) 
        {
                CFG_FREE(accountcfg->name);
                CFG_FREE(accountcfg->service);
                CFG_FREE(accountcfg->username);
                CFG_FREE(accountcfg->passwd);
                CFG_FREE(accountcfg->hostname);
                
                list_del(&(accountcfg->list));
                free(accountcfg);
        }

	return 0;
}

void config_print(struct cfg *cfg)
{
        struct accountcfg *accountcfg = NULL;
        
        printf("Configuration:\n");
        printf(" cfg file = '%s'\n", cfg->optionsfile);
        printf(" pid file = '%s'\n", cfg->pidfile);
        printf(" daemonize = '%d'\n", cfg->daemonize);
        printf(" wan ifname = '%s'\n", cfg->wan_ifname);
        printf(" wan mode = '%d'\n", cfg->wan_cnt_type);
        
        list_for_each_entry(accountcfg,
                            &(cfg->accountcfg_list), list) 
        {
                printf(" ---- account name '%s' ----\n", accountcfg->name);
                printf("   service = '%s'\n", accountcfg->service);
                printf("   username = '%s'\n", accountcfg->username);
                printf("   password = '%s'\n", accountcfg->passwd);
                printf("   hostname = '%s'\n", accountcfg->hostname);
        }
}

void config_copy(struct cfg *cfgdst, const struct cfg *cfgsrc)
{          
        struct accountcfg *actcfg = NULL,
                *safe_actcfg = NULL;
        
        memcpy(cfgdst, cfgsrc, 
               sizeof(struct cfg) - sizeof(struct list_head));
        list_for_each_entry_safe(actcfg, safe_actcfg,
                                 &(cfgsrc->accountcfg_list), list)
        {
                list_move(&(actcfg->list),
                          &(cfgdst->accountcfg_list));
        }
}
