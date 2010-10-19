AM_CFLAGS = -Wall -Wextra -Wfloat-equal -Wundef -Wshadow \
	-Wpointer-arith -Wcast-align -Wstrict-overflow=5 \
	-Wwrite-strings -Waggregate-return -Wformat=2

bin_PROGRAMS = yaddns

yaddns_SOURCES = yaddns.c \
	config.c config.h \
	ctl.c ctl.h \
	log.c log.h \
	util.c util.h \
	list.h service.h \
	services.c

yaddns_SOURCES += services/dyndns.c

