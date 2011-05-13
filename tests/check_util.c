#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/util.h"

TEST_DEF(test_util_base64)
{
        char *buf64 = NULL;
        size_t buf64_size;
        unsigned int i;

        struct {
                const char *src;
                const char *src64;
        } tarray[] = {
                { "test0001:tub78jk", "dGVzdDAwMDE6dHViNzhqaw==", },
        };

        for(i = 0; i < ARRAY_SIZE(tarray); ++i)
        {
                TEST_ASSERT(util_base64_encode(tarray[i].src,
                                               &buf64, &buf64_size) == 0,
                            "util_base64_encode(%s) failed !", tarray[i].src);

                TEST_ASSERT(strcmp(buf64, tarray[i].src64) == 0,
                            "util_base64_encode(%s) failed !", tarray[i].src);

                free(buf64);
        }
}

int main(void)
{
        TEST_INIT("util");

        TEST_RUN(test_util_base64);

	return TEST_RETURN;
}
