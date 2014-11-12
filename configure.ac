AC_INIT([libupnpp], [0.9.0], [jfd@lesbonscomptes.com],
             [libupnpp], [http://www.lesbonscomptes.com/upmpdcli])

# Lib version info. See:
#   https://www.sourceware.org/autobook/autobook/autobook_91.html#SEC91
#   https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# The Makefile uses both -release and -version-info: for now the lib
# interface is unstable and dependant packages should be updated every time
# the lib release changes. Once things are stabilized, we should drop the
# -release flag. Lib names for now: libupnpp-x.y.z.so.0.0.0
VERSION_INFO=0:0:0

AC_PREREQ([2.53])
AC_CONFIG_SRCDIR([libupnpp/upnpplib.hxx])
AC_CONFIG_HEADERS([libupnpp/config.h])


AM_INIT_AUTOMAKE([1.10 no-define subdir-objects foreign])
AC_DISABLE_STATIC
LT_INIT

AC_PROG_CXX

# libupnp is configured with large file support, and we need to do the same,
# else a difference in off_t size impacts struct File_Info and prevents the
# vdir to work. This does make a difference, for exemple, for Raspbian
# on the Raspberry PI. Use the same directives as libupnp's configure.ac
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_DEFINE([_LARGE_FILE_SOURCE], [], [Large files support])
AC_DEFINE([_FILE_OFFSET_BITS], [64], [File Offset size])

AC_CHECK_LIB([rt], [clock_gettime], [], [])
AC_CHECK_LIB([pthread], [pthread_create], [], [])
AC_CHECK_LIB([upnp], [UpnpInit], [], AC_MSG_ERROR([libupnp not found]))
AC_CHECK_LIB([curl], [curl_easy_init], [], AC_MSG_ERROR([libcurl not found]))
AC_CHECK_LIB([ixml], [ixmlDocument_getElementsByTagName], [], 
		     AC_MSG_ERROR([libixml (usually installed as part of libupnp) not found]))
AC_CHECK_FUNCS([getifaddrs] [UpnpSetLogLevel])
AC_CHECK_LIB([expat], [XML_ParserCreate], [], 
		      AC_MSG_ERROR([libexpat not found]))
LIBUPNPP_LIBS="$LIBS"

LIBS=""
                     
AC_SUBST(LIBUPNPP_LIBS)
AC_SUBST(VERSION_INFO)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
