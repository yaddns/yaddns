SUBDIRS = services

bin_PROGRAMS = yaddns

yaddns_SOURCES = yaddns.c yaddns.h \
	config.c config.h \
	account.c account.h \
	request.c request.h \
	log.c log.h \
	util.c util.h \
	myip.c myip.h \
	list.h cfgstr.h \
	services.c services.h service.h
yaddns_LDADD = services/libservices.a
