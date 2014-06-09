MAINTAINERCLEANFILES = Makefile.in

#Do first the config
SUBDIRS = hal-imp io util pipeline-imp ezappliance idl

noinst_LTLIBRARIES = libxdpd_driver_gnu_linux_src.la

library_includedir=$(includedir)/drivers/ezappliance
include_HEADERS=

libxdpd_driver_gnu_linux_src_la_SOURCES = \
	config.cc\
	bg_taskmanager.cc \
	ls_internal_state.h

libxdpd_driver_gnu_linux_src_la_LIBADD = \
                idl/libxdpd_driver_gnu_linux_idl.la \
		ezappliance/libxdpd_driver_gnu_linux_ezappliance.la \
		hal-imp/libxdpd_driver_gnu_linux_afa_imp.la \
		io/libxdpd_driver_gnu_linux_io.la \
		pipeline-imp/libxdpd_driver_gnu_linux_pipeline_imp.la \
		util/libxdpd_driver_gnu_linux_util.la
		
		
library_include_HEADERS=


