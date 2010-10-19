SUBDIRS = src

EXTRA_DIST = autogen.sh \
	etc/yaddns.conf

indent:
	find src/ -name '*.[ch]' -exec perl -p -i -e 's/[ \t]*$///' {} \;
