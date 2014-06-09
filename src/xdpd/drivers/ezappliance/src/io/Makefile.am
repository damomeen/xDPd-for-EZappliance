MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = packet_classifiers

noinst_LTLIBRARIES = libxdpd_driver_gnu_linux_io.la

libxdpd_driver_gnu_linux_io_ladir = $(includedir)/drivers/ezappliance/io

libxdpd_driver_gnu_linux_io_la_HEADERS =

libxdpd_driver_gnu_linux_io_la_SOURCES = \
	bufferpool.cc \
	bufferpool.h \
	pktin_dispatcher.cc \
	pktin_dispatcher.h \
	datapacket_storage.cc \
	datapacket_storage.h \
	datapacketx86.cc \
	datapacketx86.h \
	iface_utils.cc\
	iface_utils.h

#Define classifier
CLASSIFIER_LIB=packet_classifiers/c_pktclassifier/libxdpd_driver_gnu_linux_io_classifiers_c.la

libxdpd_driver_gnu_linux_io_la_LIBADD = \
	$(CLASSIFIER_LIB)
