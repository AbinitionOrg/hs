# HyperScript Makefile.am
#
# Modifications:
#
#	$Log: Makefile.am,v $
#	Revision 1.1  2010-10-23 02:53:11  bergsma
#	HS officially installs now with configure, make, and make install.
#	Tools used are autoscan, automake, autoheader, autoconf, aclocal
#	This file is either a source file or an auto-generated file.
#	
#
# Description of Compiler OPTIONS:
#
#	Automatically set
#	AS_UNIX		UNIX OS
#	AS_VMS		VMS OS
#	AS_WINDOWS	Windows NT/2000/XP
#	AS_PROMIS	PROMIS Application
#	AS_PPC		Win32 WCE
#	AS_ALPHA	Alpha Processor
#
#	Must be set
#	AS_SOLARIS	Solaris UNIX
#	AS_HPUX		HP-UX UNIX
#	AS_TRU64	Tru64 UNIX
#	AS_DMBX		FASTech's DMBX
#	AS_SQL		SQL access 
#		+ AS_SQLSERVER	- SQLSERVER	
#		+ AS_MYSQL	- MySQL
#		+ AS_PROC	- ORACLE, using proC pre-compiler
#		+ AS_PGSQL	- POSTgresSQL
#
#	TCP/IP Stack related
#	AS_UCX
#	AS_MULTINET
#	AS_
#
#	Other features:
#	AS_SSL	OpenSSL
#	AS_DLL	Windows DLL (for both ActiveX and JAVA controls)
#	AS_JNI	Java JNI (for JAVA control)
#	AS_ATL  Windows ATL (for ActiveX control)
#	AS_GD   GDlib graphics api
#	AS_MAPI Windows RPC
#	AS_OWL  Semantic Web (future)
#	
#	Optional
#	AS_MEMTRACK	Memory Leak Detector
#
#
HS_BASENAME=hs@HS_MAJOR@@HS_MINOR@

# ToDo: Make AS_<features> detectable and configurable
##CFLAGS = -g -O -Wall -DAS_SQL -DAS_MYSQL -DAS_SSL -DAS_GD
AM_CFLAGS = -g -O -Wall -Wno-unused -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-format-security -Wno-pointer-sign -Wno-unused-result -Wno-uninitialized -Wno-implicit-function-declaration

# ToDo: Add checks for each in configure.ac 
##LDADD = -lc -lm -lz -lmysqlclient -lssl -lcrypto -lgd -ljpeg 
# -lm and -lz are found and added to LIBS by configure.ac
LDADD =

bin_PROGRAMS = hs
hs_SOURCES = hs.c aimsg.c data.c dateparse.c hash.c channel.c signal.c tcp.c util.c branch.c cgi.c env.c fileio.c frame.c function.c hsms.c http.c hyp.c concept.c instance.c label.c load.c memtrack.c method.c operand.c operator.c parse.c port.c promis.c route.c router.c secs.c secs1.c secs2.c sock.c sort.c ssl.c stack.c stmt.c system.c type.c sql.c gd.c

include_HEADERS = aimsg.h data.h channel.h hash.h util.h router.h sock.h signl.h tcp.h branch.h concept.h cgi.h dateparse.h env.h fileio.h frame.h function.h hyp.h gd.h instance.h label.h load.h method.h operand.h operator.h promis.h parse.h hsms.h secs.h secs1.h secs2.h route.h port.h http.h sort.h ssl.h stack.h stmt.h system.h sql.h mapi.h type.h

noinst_HEADERS = auto.h hypdef.h 

# man page
#man_MANS = hs.1

# make check: smoke tests against the freshly built binary
TESTS = tests/smoke.sh
AM_TESTS_ENVIRONMENT = HS=$(builddir)/hs; export HS;

# extra files for the dist tarball
EXTRA_DIST = router.hyp $(TESTS)
