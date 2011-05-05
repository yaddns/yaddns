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
        account_ctl_init();

        account_ctl_cleanup();
}

int main(void)
{
        TEST_INIT("account");

        services_populate_list();

        TEST_RUN(test_account_map);
        TEST_RUN(test_account_remap);

	return TEST_RETURN;
}
