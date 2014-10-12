
AM_CPPFLAGS = -DDEBUG -g -Wall \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\"

AM_CXXFLAGS = -std=c++0x 

lib_LTLIBRARIES = libupnpp.la

libupnpp_la_SOURCES = \
    libupnpp/control/avlastchg.cxx \
    libupnpp/control/avlastchg.hxx \
    libupnpp/control/avtransport.cxx \
    libupnpp/control/avtransport.hxx \
    libupnpp/control/cdircontent.cxx \
    libupnpp/control/cdircontent.hxx \
    libupnpp/control/cdirectory.cxx \
    libupnpp/control/cdirectory.hxx \
    libupnpp/control/description.cxx \
    libupnpp/control/description.hxx \
    libupnpp/control/device.hxx \
    libupnpp/control/discovery.cxx \
    libupnpp/control/discovery.hxx \
    libupnpp/control/httpdownload.cxx \
    libupnpp/control/httpdownload.hxx \
    libupnpp/control/mediarenderer.cxx \
    libupnpp/control/mediarenderer.hxx \
    libupnpp/control/mediaserver.cxx \
    libupnpp/control/mediaserver.hxx \
    libupnpp/control/ohplaylist.cxx \
    libupnpp/control/ohplaylist.hxx \
    libupnpp/control/ohproduct.cxx \
    libupnpp/control/ohproduct.hxx \
    libupnpp/control/renderingcontrol.cxx \
    libupnpp/control/renderingcontrol.hxx \
    libupnpp/control/service.cxx \
    libupnpp/control/service.hxx \
    libupnpp/device/device.cxx \
    libupnpp/device/device.hxx \
    libupnpp/device/vdir.cxx \
    libupnpp/device/vdir.hxx \
    libupnpp/base64.hxx \
    libupnpp/base64.cxx \
    libupnpp/expatmm.hxx \
    libupnpp/getsyshwaddr.c \
    libupnpp/getsyshwaddr.h \
    libupnpp/ixmlwrap.cxx \
    libupnpp/ixmlwrap.hxx \
    libupnpp/log.cxx \
    libupnpp/log.hxx \
    libupnpp/md5.cxx \
    libupnpp/md5.hxx \
    libupnpp/ptmutex.hxx \
    libupnpp/soaphelp.cxx \
    libupnpp/soaphelp.hxx \
    libupnpp/upnpavutils.cxx \
    libupnpp/upnpavutils.hxx \
    libupnpp/upnpp_p.hxx \
    libupnpp/upnpplib.cxx \
    libupnpp/upnpplib.hxx \
    libupnpp/workqueue.hxx

nobase_include_HEADERS = \
    libupnpp/control/avtransport.hxx \
    libupnpp/control/cdircontent.hxx \
    libupnpp/control/cdirectory.hxx \
    libupnpp/control/description.hxx \
    libupnpp/control/device.hxx \
    libupnpp/control/discovery.hxx \
    libupnpp/control/mediarenderer.hxx \
    libupnpp/control/mediaserver.hxx \
    libupnpp/control/ohproduct.hxx \
    libupnpp/control/ohplaylist.hxx \
    libupnpp/control/renderingcontrol.hxx \
    libupnpp/control/service.hxx \
    libupnpp/device/device.hxx \
    libupnpp/base64.hxx \
    libupnpp/log.hxx \
    libupnpp/ptmutex.hxx \
    libupnpp/soaphelp.hxx \
    libupnpp/upnpavutils.hxx \
    libupnpp/upnpplib.hxx \
    libupnpp/upnpputils.hxx

libupnpp_la_LDFLAGS = -release $(VERSION)
libupnpp_la_LIBADD = $(LIBUPNPP_LIBS)

bin_PROGRAMS = upmpdcli

upmpdcli_SOURCES = \
     upmpd/conftree.cxx \
     upmpd/conftree.hxx \
     upmpd/ohmetacache.cxx \
     upmpd/ohmetacache.hxx \
     upmpd/mpdcli.cxx \
     upmpd/mpdcli.hxx \
     upmpd/upmpd.hxx \
     upmpd/upmpd.cxx \
     upmpd/upmpdutils.cxx \
     upmpd/upmpdutils.hxx \
     upmpd/renderctl.hxx \
     upmpd/renderctl.cxx \
     upmpd/avtransport.hxx \
     upmpd/avtransport.cxx \
     upmpd/conman.hxx \
     upmpd/conman.cxx \
     upmpd/ohproduct.hxx \
     upmpd/ohproduct.cxx \
     upmpd/ohinfo.hxx \
     upmpd/ohinfo.cxx \
     upmpd/ohtime.hxx \
     upmpd/ohtime.cxx \
     upmpd/ohvolume.hxx \
     upmpd/ohvolume.cxx \
     upmpd/ohplaylist.hxx \
     upmpd/ohplaylist.cxx
                           
upmpdcli_LDADD = libupnpp.la -lmpdclient -lcurl -lpthread -lrt

dist_pkgdata_DATA = upmpd/description.xml upmpd/AVTransport.xml \
                  upmpd/RenderingControl.xml upmpd/ConnectionManager.xml \
                  upmpd/OHProduct.xml upmpd/OHInfo.xml upmpd/OHTime.xml \
                  upmpd/OHVolume.xml upmpd/OHPlaylist.xml upmpd/protocolinfo.txt

dist_sysconf_DATA = upmpd/upmpdcli.conf

dist_man1_MANS = man/upmpdcli.1

EXTRA_DIST = systemd/upmpdcli.service
dist-hook:
	test -z "`git status -s | grep -v upmpdcli-$(VERSION)`"
	git tag -f -a upmpdcli-v$(VERSION) -m 'version $(VERSION)'
