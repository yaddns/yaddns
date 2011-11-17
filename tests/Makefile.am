EXTRA_DIST = yatest.h \
	yaddns.good.2.conf \
	yaddns.good.conf \
	yaddns.invalid.account2_has_invalid_service.conf \
	yaddns.invalid.conf \
	yaddns.invalid.unknown_service.conf

TESTS = check_request check_cfgstr check_config check_account check_util

check_PROGRAMS = $(TESTS)

YADDNS_OBJS = $(top_builddir)/src/request.o \
		$(top_builddir)/src/services.o \
		$(top_builddir)/src/services/libservices.a \
		$(top_builddir)/src/account.o \
		$(top_builddir)/src/config.o \
		$(top_builddir)/src/util.o \
		$(top_builddir)/src/log.o

check_request_SOURCES = check_request.c $(top_builddir)/src/request.h
check_request_LDADD = $(YADDNS_OBJS)

check_cfgstr_SOURCES = check_cfgstr.c $(top_builddir)/src/cfgstr.h
check_cfgstr_LDADD = $(YADDNS_OBJS)

check_config_SOURCES = check_config.c $(top_builddir)/src/config.h
check_config_LDADD = $(YADDNS_OBJS)

check_account_SOURCES = check_account.c $(top_builddir)/src/account.h \
		$(top_builddir)/src/config.h \
		$(top_builddir)/src/service.h
check_account_LDADD = $(YADDNS_OBJS)

check_util_SOURCES = check_util.c $(top_builddir)/src/util.h
check_util_LDADD = $(YADDNS_OBJS)
