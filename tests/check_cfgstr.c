#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/cfgstr.h"

TEST_DEF(test_cfgstr_init)
{
        struct cfgstr cfgstr;

        cfgstr_init(&cfgstr);

        TEST_ASSERT(!cfgstr_is_set(&cfgstr),
                    "cfgstr_is_set(&cfgstr) succeeded but expect failed !");

        cfgstr_unset(&cfgstr);
}

TEST_DEF(test_cfgstr_set)
{
        struct cfgstr cfgstr;
        const char *mychars = "My cfgstr";

        cfgstr_init(&cfgstr);
        cfgstr_set(&cfgstr, mychars);

        TEST_ASSERT(cfgstr_is_set(&cfgstr),
                    "cfgstr_is_set(&cfgstr) failed");

        TEST_ASSERT(strcmp(cfgstr_get(&cfgstr), mychars) == 0,
                    "%s != %s (equal expect !)",
                    cfgstr_get(&cfgstr), mychars);

        cfgstr_unset(&cfgstr);
}

TEST_DEF(test_cfgstr_dup)
{
        struct cfgstr cfgstr;
        const char *mychars = "My cfgstr";

        cfgstr_init(&cfgstr);
        cfgstr_dup(&cfgstr, mychars);

        TEST_ASSERT(cfgstr_is_set(&cfgstr),
                    "cfgstr_is_set(&cfgstr) failed");

        TEST_ASSERT(strcmp(cfgstr_get(&cfgstr), mychars) == 0,
                    "%s != %s (equal expect !)",
                    cfgstr_get(&cfgstr), mychars);

        cfgstr_unset(&cfgstr);

        /* it will be nice to test the free() of strdup */
}

TEST_DEF(test_cfgstr_unset)
{
        struct cfgstr cfgstr;
        const char *mychars = "My cfgstr";

        cfgstr_init(&cfgstr);
        cfgstr_set(&cfgstr, mychars);
        cfgstr_unset(&cfgstr);

        TEST_ASSERT(!cfgstr_is_set(&cfgstr),
                    "cfgstr_is_set(&cfgstr) succeeded but expect failed !");
}

TEST_DEF(test_cfgstr_move)
{
        struct cfgstr cfgstr,
                cfgstrdst;
        const char *mychars = "My cfgstr";

        cfgstr_init(&cfgstr);

        cfgstr_set(&cfgstr, mychars);

        cfgstr_move(&cfgstr, &cfgstrdst);

        TEST_ASSERT(!cfgstr_is_set(&cfgstr),
                    "cfgstr_is_set(&cfgstr) succeeded but expect failed !");

        TEST_ASSERT(cfgstr_is_set(&cfgstrdst),
                    "cfgstr_is_set(&cfgstrdst) failed");

        TEST_ASSERT(strcmp(cfgstr_get(&cfgstrdst), mychars) == 0,
                    "%s != %s (equal expect !)",
                    cfgstr_get(&cfgstrdst), mychars);

        cfgstr_unset(&cfgstr);
        cfgstr_unset(&cfgstrdst);
}

int main(void)
{
        TEST_INIT("cfgstr");

        TEST_RUN(test_cfgstr_init);
        TEST_RUN(test_cfgstr_set);
        TEST_RUN(test_cfgstr_dup);
        TEST_RUN(test_cfgstr_unset);
        TEST_RUN(test_cfgstr_move);

	return TEST_RETURN;
}
