#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/config.h"
#include "../src/util.h"

TEST_DEF(test_config_parse)
{
        struct cfg cfg;
        const char *cfgfn = NULL;

        config_init(&cfg);

        cfgfn = "dontexist_conf_file__.conf";
        TEST_ASSERT(config_parse_file(&cfg, cfgfn) != 0,
                    "config_parse_file(%s) success with an non existant conf file ?",
                    cfgfn);

        cfgfn = "yaddns.good.conf";
        TEST_ASSERT(config_parse_file(&cfg, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        cfgfn = "yaddns.invalid.conf";
        TEST_ASSERT(config_parse_file(&cfg, cfgfn) != 0,
                    "Success on config_parse_file(%s) !", cfgfn);

        config_free(&cfg);
}

int main(void)
{
        TEST_INIT("config");

        TEST_RUN(test_config_parse);

	return TEST_RETURN;
}
