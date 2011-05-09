#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/config.h"
#include "../src/util.h"

TEST_DEF(test_config_parse)
{
        struct cfg cfg;

        config_init(&cfg);

        cfgstr_set(&cfg.cfgfile, "__dontexist_conf_file__.conf");
        TEST_ASSERT(config_parse_file(&cfg) != 0,
                    "config_parse_file(%s) succeeded but we expected failed !",
                    cfgstr_get(&cfg.cfgfile));

        cfgstr_set(&cfg.cfgfile, "yaddns.good.conf");
        TEST_ASSERT(config_parse_file(&cfg) == 0,
                    "config_parse_file(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        cfgstr_set(&cfg.cfgfile, "yaddns.invalid.conf");
        TEST_ASSERT(config_parse_file(&cfg) != 0,
                    "config_parse_file(%s) succeeded but we expected failed !",
                    cfgstr_get(&cfg.cfgfile));

        config_free(&cfg);
}

int main(void)
{
        TEST_INIT("config");

        TEST_RUN(test_config_parse);

	return TEST_RETURN;
}
