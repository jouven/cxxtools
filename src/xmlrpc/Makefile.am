AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/include -I$(top_srcdir)/include

lib_LTLIBRARIES = libcxxtools-xmlrpc.la

libcxxtools_xmlrpc_la_SOURCES = \
	client.cpp \
	clientimpl.cpp \
	httpclient.cpp \
	httpclientimpl.cpp \
	formatter.cpp \
	responder.cpp \
	scanner.cpp \
	service.cpp

noinst_HEADERS = \
	clientimpl.h \
	httpclientimpl.h

libcxxtools_xmlrpc_la_LIBADD = $(top_builddir)/src/libcxxtools.la $(top_builddir)/src/http/libcxxtools-http.la

libcxxtools_xmlrpc_la_LDFLAGS = -lws2_32 -no-undefined -version-info @sonumber@ @SHARED_LIB_FLAG@

