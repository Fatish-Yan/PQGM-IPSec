# gmalg plugin Makefile.am

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/libstrongswan \
	-I/usr/local/include/gmssl

AM_CFLAGS = \
	$(PLUGIN_CFLAGS)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-gmalg.la
libstrongswan_gmalg_la_LIBADD = -L/usr/local/lib -lgmssl
else
plugin_LTLIBRARIES = libstrongswan-gmalg.la
libstrongswan_gmalg_la_LIBADD = \
	$(top_builddir)/src/libstrongswan/libstrongswan.la \
	-L/usr/local/lib \
	-lgmssl
endif

libstrongswan_gmalg_la_SOURCES = \
	gmalg_plugin.c \
	gmalg_plugin.h \
	gmalg_hasher.c \
	gmalg_hasher.h \
	gmalg_crypter.c \
	gmalg_crypter.h

libstrongswan_gmalg_la_LDFLAGS = -module -avoid-version
