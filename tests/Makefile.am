TESTS = check_request check_config check_account

check_PROGRAMS = check_request check_config check_account

check_request_SOURCES = check_request.c $(top_builddir)/src/request.h
check_request_CFLAGS = 
check_request_LDADD = $(top_builddir)/src/request.o \
		$(top_builddir)/src/log.o

check_config_SOURCES = check_config.c $(top_builddir)/src/config.h
check_config_CFLAGS = 
check_config_LDADD = $(top_builddir)/src/request.o \
		$(top_builddir)/src/config.o \
		$(top_builddir)/src/util.o \
		$(top_builddir)/src/log.o

check_account_SOURCES = check_account.c $(top_builddir)/src/account.h \
		$(top_builddir)/src/config.h \
		$(top_builddir)/src/service.h
check_account_CFLAGS = 
check_account_LDADD = $(top_builddir)/src/request.o \
		$(top_builddir)/src/services.o \
		$(top_builddir)/src/services/libservices.a \
		$(top_builddir)/src/account.o \
		$(top_builddir)/src/config.o \
		$(top_builddir)/src/util.o \
		$(top_builddir)/src/log.o