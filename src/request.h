/*
 *  Yaddns - Yet Another ddns client
 *  Copyright (C) 2008 Anthony Viallard <anthony.viallard@patatrac.info>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _YADDNS_REQUEST_H_
#define _YADDNS_REQUEST_H_

#include <sys/time.h>
#include <netinet/in.h>

#include "list.h"
#include "util.h"

#define REQUEST_DATA_MAX_SIZE       512

#define REQUEST_PENDING_ACTION_TIMEOUT     30

#define REQ_OPT_BIND_ADDR           0x01 << 0

#define REQ_ERR_UNKNOWN             0
#define REQ_ERR_SYSTEM              1
#define REQ_ERR_CONNECT_FAILED      2
#define REQ_ERR_CONNECT_TIMEOUT     3
#define REQ_ERR_RESPONSE_TIMEOUT    4
#define REQ_ERR_SENDING_TIMEOUT     5

static inline const char *strreqerr(unsigned int req_err)
{
        const char *req_err_str[] = {
                "Unknown error",
                "System error",
                "Connection has failed",
                "Connection timeout",
                "Receive timeout",
                "Send timeout",
        };

        if(req_err >= ARRAY_SIZE(req_err_str))
        {
                req_err = 0;
        }

        return req_err_str[req_err];
}

/*
 * This module is for helping send an request and receive
 * response.
 *
 * A request has 8 flow states availables:
 * - FSError                     => An error occur (view errcode for more info)
 * - FSCreated                   => The request is created
 * - FSConnecting                => The request has a connecting
 *                                    socket to request_host
 * - FSConnected                 => The request has a connection
 * - FSSending                   => The request is sent
 * - FSWaitingResponse           => Waiting request response
 * - FSResponseReceived          => Request response was received
 * - FSFinished                  => Request is terminated
 *
 * A request with FSError is along of errcode variable which detail
 * the error (view REQ_ERR_* macro def).
 */

struct request;

struct request_ctl {
        void (*hook_func)(struct request *request, void *hook_data);
        /* unsigned long hook_mask; */
        void *hook_data; /* data given in arg to hook_func when is called */
};

struct request_host {
        char addr[128];
        unsigned short int port;
};

struct request_buff {
        char data[REQUEST_DATA_MAX_SIZE];
        size_t data_size; /* total count of chars in buf */
        size_t data_ack;  /* count of chars read or write yet */
};

struct request_opt {
        unsigned long mask;
        struct in_addr bind_addr;
};

struct request {
        int s;
        struct request_host host;
        struct request_ctl ctl;
        struct request_buff buff;
        struct request_opt opt;
        enum {
                FSError = -1,
                FSCreated,
                FSConnecting,
                FSConnected,
                FSSending,
                FSWaitingResponse,
                FSResponseReceived,
                FSFinished,
        } state;
        unsigned int errcode;
	struct timeval last_pending_action;
        struct list_head list;
};

/*
 * init request list
 */
void request_ctl_init(void);

/*
 * cleanup request list
 */
void request_ctl_cleanup(void);

/*
 * Send a request
 */
int request_send(struct request_host *host,
                 struct request_ctl *ctl,
                 struct request_buff *buff,
                 struct request_opt *opt);

/*
 * request_ctl_selectfds
 *
 * - fill fd_set with fd to watch
 * - set max_fd
 */
void request_ctl_selectfds(fd_set *readset, fd_set *writeset, int *max_fd);

/*
 * request_processfds
 *
 * - following fd_set, manage request flow
 */
void request_ctl_processfds(fd_set *readset, fd_set *writeset);

/*
 * Remove all current requests
 */
int request_ctl_remove_by_hook_data(const void *hook_data);

#endif
