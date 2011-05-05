SUBDIRS = services

AM_CFLAGS = -Wall -Wextra -Wfloat-equal -Wundef -Wshadow \
	-Wpointer-arith -Wstrict-overflow=5 \
	-Wwrite-strings -Waggregate-return -Wformat=2

bin_PROGRAMS = yaddns

yaddns_SOURCES = yaddns.c yaddns.h \
	config.c config.h \
	account.c account.h \
	request.c request.h \
	log.c log.h \
	util.c util.h \
	myip.c myip.h \
	list.h \
	services.c services.h service.h
yaddns_LDADD = services/libservices.a
