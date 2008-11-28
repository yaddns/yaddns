#ifndef _EVENT_H_
#define _EVENT_H_

#define EVENT_EXIT 0x01
#define EVENT_RELOAD_CONF 0x02

extern void event_add(int event);
extern void event_del(int event);

/*
 *
 * @return: 1 if event has found, 0 otherwise
 */
extern int event_check(int event);

#endif
