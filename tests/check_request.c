#include <stdlib.h>
#include <stdio.h>

#include "yatest.h"

#include "../src/request.h"
#include "../src/util.h"

extern struct list_head request_list;

static void setup(void)
{
        request_ctl_init();
}

static void teardown(void)
{
        request_ctl_cleanup();
}


TEST_DEF(test_request_remove)
{
        struct request *request = NULL;
        unsigned int i = 0;
        int ret = 0;
        const char *datas[] = {
                "0", "1", "2", "3", "4", "5", "6", "7", "8",
        };

        request_ctl_cleanup();

        for(i = 0; i < ARRAY_SIZE(datas); ++i)
        {
                request = calloc(1, sizeof(struct request));

                request->ctl.hook_data = (void*)datas[i];
                
                list_add(&(request->list), &request_list);
        }

        ret = request_ctl_remove_by_hook_data(datas[0]);
        TEST_ASSERT(ret == 1,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    datas[0], ret);

        ret = request_ctl_remove_by_hook_data(datas[0]);
        TEST_ASSERT(ret == 0,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    datas[0], ret);

        for(i = 0; i < ARRAY_SIZE(datas); ++i)
        {
                request = calloc(1, sizeof(struct request));

                request->ctl.hook_data = (void*)datas[i];

                list_add(&(request->list), &request_list);
        }

        ret = request_ctl_remove_by_hook_data(datas[0]);
        TEST_ASSERT(ret == 1,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    datas[0], ret);

        ret = request_ctl_remove_by_hook_data(datas[6]);
        TEST_ASSERT(ret == 2,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    datas[6], ret);

        ret = request_ctl_remove_by_hook_data(datas[6]);
        TEST_ASSERT(ret == 0,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    datas[6], ret);

        ret = request_ctl_remove_by_hook_data(NULL);
        TEST_ASSERT(ret == 0,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    NULL, ret);

        ret = request_ctl_remove_by_hook_data(&i);
        TEST_ASSERT(ret == 0,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    &i, ret);

        request_ctl_cleanup();

        ret = request_ctl_remove_by_hook_data(datas[6]);
        TEST_ASSERT(ret == 0,
                    "request_ctl_remove_by_hook_data(%p) = %d",
                    datas[6], ret);
}

int main(void)
{
        TEST_INIT("request");

        setup();

        TEST_RUN(test_request_remove);

        teardown();

	return TEST_RETURN;
}
