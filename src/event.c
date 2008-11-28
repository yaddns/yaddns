#include "event.h"

/* event bits */
static volatile int reg_event = 0;

void event_add(int event)
{
	reg_event |= event;
}

void event_del(int event)
{
	reg_event &= ~event;
}

int event_check(int event)
{
	return ((reg_event & event) == event);
}
