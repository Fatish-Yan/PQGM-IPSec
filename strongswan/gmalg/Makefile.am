# gmalg plugin Makefile.am

if USE_GMALG
plugin_LTLIBRARIES = gmalg.la
endif

gmalg_la_SOURCES = \
	gmalg_plugin.c \
	gmalg_plugin.h \
	gmalg_hasher.c \
	gmalg_hasher.h \
	gmalg_crypter.c \
	gmalg_crypter.h

gmalg_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/src/libstrongswan \
	-I/usr/local/include/gmssl

gmalg_la_LDFLAGS = \
	-module -avoid-version \
	-no-undefined \
	-L/usr/local/lib \
	-lgmssl

gmalg_la_LIBADD = \
	$(top_builddir)/src/libstrongswan/libstrongswan.la
