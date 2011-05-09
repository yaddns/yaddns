#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/account.h"
#include "../src/config.h"
#include "../src/util.h"
#include "../src/services.h"

TEST_DEF(test_account_map)
{
        struct cfg cfg;

        request_ctl_init();
        account_ctl_init();
        config_init(&cfg);

        cfgstr_set(&cfg.cfgfile, "yaddns.good.conf");
        TEST_ASSERT(config_parse_file(&cfg) == 0,
                    "Failed to config_parse_file(%s)",
                    cfgstr_get(&cfg.cfgfile));

        TEST_ASSERT(account_ctl_mapcfg(&cfg) == 0,
                    "account_ctl_mapcfg() failed !");

        account_ctl_cleanup();
        config_free(&cfg);
}

TEST_DEF(test_account_remap)
{
        struct cfg cfg, cfgre;

        request_ctl_init();
        account_ctl_init();
        config_init(&cfg);

        /* map the primary cfg file */
        cfgstr_set(&cfg.cfgfile, "yaddns.good.conf");
        TEST_ASSERT(config_parse_file(&cfg) == 0,
                    "config_parse_file(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        TEST_ASSERT(account_ctl_mapcfg(&cfg) == 0,
                    "account_ctl_mapcfg(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        /******* reload good cfg file ********/
        config_init(&cfgre);

        cfgstr_set(&cfgre.cfgfile, "yaddns.good.2.conf");
        TEST_ASSERT(config_parse_file(&cfgre) == 0,
                    "config_parse_file(%s) failed",
                    cfgstr_get(&cfg.cfgfile));

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) == 0,
                    "account_ctl_mapnewcfg(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        /* update configuration */
        config_move(&cfgre, &cfg);
        config_free(&cfgre);

        /******* reload an another bad cfg file ********/
        config_init(&cfgre);

        cfgstr_set(&cfgre.cfgfile,
                   "yaddns.invalid.account2_has_invalid_service.conf");
        TEST_ASSERT(config_parse_file(&cfgre) == 0,
                    "config_parse_file(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) != 0,
                    "account_ctl_mapnewcfg(%s) succeeded but we expected failed !",
                    cfgstr_get(&cfg.cfgfile));

        config_free(&cfgre);

        /******* reload bad cfg file ********/
        config_init(&cfgre);

        cfgstr_set(&cfgre.cfgfile, "yaddns.invalid.unknown_service.conf");
        TEST_ASSERT(config_parse_file(&cfgre) == 0,
                    "config_parse_file(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) != 0,
                    "account_ctl_mapnewcfg(%s) succeeded but we expected failed !",
                    cfgstr_get(&cfg.cfgfile));

        config_free(&cfgre);

        /******* reload the first file ********/
        config_init(&cfgre);

        cfgstr_set(&cfgre.cfgfile, "yaddns.good.conf");
        TEST_ASSERT(config_parse_file(&cfgre) == 0,
                    "config_parse_file(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) == 0,
                    "account_ctl_mapnewcfg(%s) failed !",
                    cfgstr_get(&cfg.cfgfile));

        /* update configuration */
        config_move(&cfgre, &cfg);
        config_free(&cfgre);

        /******* finish him ********/
        account_ctl_cleanup();
        config_free(&cfg);
}

int main(void)
{
        TEST_INIT("account");

        services_populate_list();

        TEST_RUN(test_account_map);
        TEST_RUN(test_account_remap);

	return TEST_RETURN;
}
