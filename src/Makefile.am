AM_CFLAGS = -Werror -Wall -Wextra -Wfloat-equal -Wundef -Wshadow \
	-Wpointer-arith -Wcast-align -Wstrict-overflow=5 \
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
	services.c services.h

yaddns_SOURCES +=  service.h \
	services/changeip.c \
	services/dyndns.c \
	services/no-ip.c \
	services/ovh.c
