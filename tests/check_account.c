#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/account.h"
#include "../src/config.h"
#include "../src/util.h"

TEST_DEF(test_account_map)
{
        struct cfg cfg;
        const char *cfgfn = NULL;

        request_ctl_init();
        account_ctl_init();
        config_init(&cfg);

        cfgfn = "yaddns.good.conf";
        TEST_ASSERT(config_parse_file(&cfg, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        TEST_ASSERT(account_ctl_mapcfg(&cfg) == 0,
                    "account_ctl_mapcfg() failed !");

        account_ctl_cleanup();
        config_free(&cfg);
}

TEST_DEF(test_account_remap)
{
        struct cfg cfg, cfgre;
        const char *cfgfn = NULL;

        request_ctl_init();
        account_ctl_init();
        config_init(&cfg);

        /* map the primary cfg file */
        cfgfn = "yaddns.good.conf";
        TEST_ASSERT(config_parse_file(&cfg, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        TEST_ASSERT(account_ctl_mapcfg(&cfg) == 0,
                    "account_ctl_mapcfg() failed !");

        /******* reload good cfg file ********/
        config_init(&cfgre);

        cfgfn = "yaddns.good.2.conf";
        TEST_ASSERT(config_parse_file(&cfgre, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) == 0,
                    "Failed to account_ctl_mapnewcfg() !");

        /* update configuration */
        config_move(&cfgre, &cfg);
        config_free(&cfgre);

        /******* reload an another bad cfg file ********/
        config_init(&cfgre);

        cfgfn = "yaddns.invalid.account2_has_invalid_service.conf";
        TEST_ASSERT(config_parse_file(&cfg, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) != 0,
                    "account_ctl_mapnewcfg() success but we attempt fail !");

        config_free(&cfgre);

        /******* reload bad cfg file ********/
        config_init(&cfgre);

        cfgfn = "yaddns.invalid.unknown_service.conf";
        TEST_ASSERT(config_parse_file(&cfgre, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) != 0,
                    "account_ctl_mapnewcfg() success but we attempt fail !");

        config_free(&cfgre);

        /******* reload the first file ********/
        config_init(&cfgre);

        cfgfn = "yaddns.good.conf";
        TEST_ASSERT(config_parse_file(&cfgre, cfgfn) == 0,
                    "Failed to config_parse_file(%s)", cfgfn);

        TEST_ASSERT(account_ctl_mapnewcfg(&cfgre) == 0,
                    "account_ctl_mapnewcfg() failed !");

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
