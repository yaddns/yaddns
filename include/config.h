#ifndef _YADDNS_CONFIG_H_
#define _YADDNS_CONFIG_H_

extern int config_parse(int argc, char **argv);

extern int config_parse_file(const char *filename);

extern int config_free(void);

#endif
