## Process this file with automake to produce Makefile.in

AM_CPPFLAGS	= -I$(top_srcdir)/lapacke/src -I$(top_srcdir)/../atblas/cblas
check_PROGRAMS	= dgesvd
LDADD		= ../lapacke/liblapacke.a ../../atblas/cblas/libcblas.a
TESTS		= $(check_PROGRAMS)

EXTRA_DIST	= test-utils.h

### end of file
