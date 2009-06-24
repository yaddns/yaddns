#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>

#include "config.h"
#include "log.h"

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
                
                /* service definition ? */
                if(memcmp(n, "service", sizeof("service") - 1) == 0)
                {
                        /* maybe a service line definition ? */
                        if((equals = strchr(n, '{')) != NULL)
                        {
                                /* remove whitespaces before { */
                                for(t = equals - 1; 
                                    t > n && isspace(*t); 
                                    t--)
                                {
                                        *t = '\0';
                                }

                                /* maybe { is glued to service ? */
                                *equals = '\0';
                                
                                *name = n;
                                *value = NULL;
                                ret = 0;
                        }
                        else
                        {
                                log_error("parsing error at '%s'. Invalid "
                                          "service declaration. "
                                          "(file %s - line %d)",
                                          n, filename, (*linenum));
                                ret = -1;
                        }

                        break;
                }

                /* maybe end of service definition ? */
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
        
        int c, ind;
        char short_options[] = "vhlf:";
        struct option long_options [] = {
                {"version", no_argument, 0, 'v' },
                {"help", no_argument, 0, 'h' },
                {"list-service", no_argument, 0, 'l' },
                /*{"daemon", no_argument, 0, 'D' },*/
                {"cfg", required_argument, 0, 'f' },
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
                        printf("Usage: %s [-f cfg_filename]\n", argv[0]);
                        printf("Options:\n");
                        printf(" -f, --cfg=FILE\t\tConfig file to be used\n");
                        printf(" -h, --help\t\tDisplay this help\n");
                        printf(" -l, --list-service\tDisplay the "
                               "service list managed by " D_NAME "\n");
                        printf(" -v, --version\t\tDisplay the version\n");
                        return -1;
                        
                case 'v':
                        printf(D_NAME " - version " D_VERSION "\n");
                        return -1;

                case 'l':
                        log_info("services listing /!\\ TODO /!\\");
                        return -1;
                        
                case 'f':
                        cfg->optionsfile = strdup(optarg);
                        optionsfile_flag = 1;
                        
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

        /* check if there service defined */
        
	return 0;
}

int config_parse_file(struct cfg *cfg, const char *filename)
{
	FILE *file = NULL;
        int ret = 0;
	char buffer[1024];
	int linenum = 0;
	char *name = NULL, *value = NULL;
        int servicedef_scope = 0;
        struct servicecfg *servicecfg = NULL;
        
        INIT_LIST_HEAD( &(cfg->servicecfg_list) );
        
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
                
                if(servicedef_scope)
                {
                        if(name == NULL)
                        {
                                servicedef_scope = 0;
                                
                                /* check and insert */
                                if(servicecfg->name == NULL
                                   || servicecfg->username == NULL
                                   || servicecfg->passwd == NULL
                                   || servicecfg->hostname == NULL)
                                {
                                        log_error("Missing value(s) for "
                                                  "service '%s' (file %s "
                                                  "line %d)",
                                                  servicecfg->name,
                                                  filename, linenum);
                                        
                                        CFG_FREE(servicecfg->name);
                                        CFG_FREE(servicecfg->username);
                                        CFG_FREE(servicecfg->passwd);
                                        CFG_FREE(servicecfg->name);
                                        free(servicecfg);
                                        
                                        ret = -1;
                                        break;
                                }

                                list_add(&(servicecfg->list), 
                                         &(cfg->servicecfg_list));
                        }
                        else if(strcmp(name, "name") == 0)
                        {
                                CFG_SET_VALUE(servicecfg->name, value);
                        }
                        else if(strcmp(name, "username") == 0)
                        {
                                CFG_SET_VALUE(servicecfg->username, value);
                        }
                        else if(strcmp(name, "password") == 0)
                        {
                                CFG_SET_VALUE(servicecfg->passwd, value);
                        }
                        else if(strcmp(name, "hostname") == 0)
                        {
                                CFG_SET_VALUE(servicecfg->hostname, value);
                        }
                        else
                        {
                                log_error("Invalid option name '%s' for "
                                          "service definition (file %s "
                                          "line %d)",
                                          name, filename, linenum);
                                ret = -1;
                                break;
                        }
                }
                else if(strcmp(name, "service") == 0)
                {
                        servicedef_scope = 1;
                        servicecfg = calloc(1, sizeof(struct servicecfg));
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
        
        if(ret == -1 && feof(file))
        {
                /* end of file */
                log_debug("End of configuration file");
                ret = 0;
        }

        if(servicedef_scope)
        {
                log_error("No found closure for service '%s' "
                          "(file %s line %d)", 
                          servicecfg->name, filename, linenum);
                ret = -1;
        }

        fclose(file);
        
	return ret;
}

int config_free(struct cfg *cfg)
{
        struct servicecfg *servicecfg = NULL,
                *safe = NULL;
        
        CFG_FREE(cfg->wan_ifname);
        CFG_FREE(cfg->optionsfile);
        
        list_for_each_entry_safe(servicecfg, safe, 
                                 &(cfg->servicecfg_list), list) 
        {
                CFG_FREE(servicecfg->name);
                CFG_FREE(servicecfg->username);
                CFG_FREE(servicecfg->passwd);
                CFG_FREE(servicecfg->hostname);
                
                list_del(&(servicecfg->list));
                free(servicecfg);
        }

	return 0;
}

void config_print(struct cfg *cfg)
{
        struct servicecfg *servicecfg = NULL;
        
        printf("Configuration:\n");
        printf(" cfg file = '%s'\n", cfg->optionsfile);
        printf(" wan ifname = '%s'\n", cfg->wan_ifname);
        printf(" wan mode = '%d'\n", cfg->wan_cnt_type);
        
        list_for_each_entry(servicecfg,
                            &(cfg->servicecfg_list), list) 
        {
                printf(" ---- service name '%s' ----\n", servicecfg->name);
                printf("   username = '%s'\n", servicecfg->username);
                printf("   password = '%s'\n", servicecfg->passwd);
                printf("   hostname = '%s'\n", servicecfg->hostname);
        }
}
