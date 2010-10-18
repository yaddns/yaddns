INCLUDES = -I$(srcdir)/include/
LDADD = -lrt

bin_PROGRAMS = yaddns

yaddns_SOURCES = src/yaddns.c \
	src/config.c include/config.h \
	src/ctl.c include/ctl.h \
	src/log.c include/log.h \
	src/util.c include/util.h \
	include/list.h include/service.h \
	src/services.c \
	src/services/dyndns.c

EXTRA_DIST = etc/yaddns.conf