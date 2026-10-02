#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
#   autoreconf -fi        (only needed after editing configure.ac or Makefile.am)
#   ./configure && make && make check && make install
#
# Keep the version in step with VERSION_HYPERSCRIPT in hypdef.h.
#
AC_PREREQ([2.69])
AC_INIT([hs],[3.9.5],[support@abinition.com])
AM_INIT_AUTOMAKE([foreign -Wall -Wno-portability])
# Build rules that re-run autoconf/automake are off unless --enable-maintainer-mode.
# git does not preserve timestamps, so without this a fresh checkout tries to
# regenerate configure and Makefile.in and fails where autotools are absent.
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([hs.c])
AC_CONFIG_HEADERS([config.h:config.in])

HS_MAJOR=3
HS_MINOR=9
HS_RELEASE=5
AC_SUBST([HS_MAJOR])
AC_SUBST([HS_MINOR])
AC_SUBST([HS_RELEASE])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries (always linked; see LDADD in Makefile.am).
AC_CHECK_LIB([m], [sqrt], [], [AC_MSG_ERROR([libm is required])])
AC_CHECK_LIB([z], [deflate], [], [AC_MSG_ERROR([zlib (libz) is required])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/file.h sys/ioctl.h sys/socket.h sys/systeminfo.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([alarm bzero dup2 floor gethostbyaddr gethostbyname gethostname gettimeofday inet_ntoa memchr memmove memset mkdir mkfifo pow putenv select setenv socket sqrt strchr strcspn strerror strrchr strspn strstr sysinfo tzset])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
