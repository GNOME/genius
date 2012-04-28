## Process this file with automake to produce Makefile.in

# The  following line defines  auxiliary routines  needed by  both the
# Level 2 and  Level 3 BLAS.  Comment it out only  if you already have
# both the Level 2 and 3 BLAS.
BLASAUX	= \
	lsame.f		xerbla.f

#page
## --------------------------------------------------------------------
## BLAS level 1.
## --------------------------------------------------------------------

# Single precision real BLAS routines
SBLAS1 = \
	isamax.f	sasum.f		saxpy.f		\
	scopy.f		sdot.f		snrm2.f		\
	srot.f		srotg.f		sscal.f		\
	sswap.f		sdsdot.f	srotmg.f	\
	srotm.f

# Single precision complex BLAS routines
CBLAS1 = \
	scabs1.f	scasum.f	scnrm2.f	\
	icamax.f	caxpy.f		ccopy.f		\
	cdotc.f		cdotu.f		csscal.f	\
	crotg.f		cscal.f		cswap.f		\
	csrot.f

# Double precision real BLAS routines
DBLAS1 = \
	idamax.f	dasum.f		daxpy.f		\
	dcopy.f		ddot.f		dnrm2.f		\
	drot.f		drotg.f		dscal.f		\
	dsdot.f		dswap.f		drotmg.f	\
	drotm.f

# Double precision complex BLAS routines
ZBLAS1 = \
	dcabs1.f	dzasum.f	dznrm2.f	\
	izamax.f	zaxpy.f		zcopy.f		\
	zdotc.f		zdotu.f		zdscal.f	\
	zrotg.f		zscal.f		zswap.f		\
	zdrot.f

# Real BLAS routines called by complex routines
CB1AUX = isamax.f sasum.f saxpy.f scopy.f snrm2.f sscal.f

# Double precision real BLAS routines called by d.p. complex
ZB1AUX = idamax.f dasum.f daxpy.f dcopy.f dnrm2.f dscal.f

#page
## --------------------------------------------------------------------
## BLAS level 2.
## --------------------------------------------------------------------

# Single precision real BLAS2 routines
SBLAS2 = \
	sgemv.f		sgbmv.f		ssymv.f		\
	ssbmv.f		sspmv.f		strmv.f		\
	stbmv.f		stpmv.f		strsv.f		\
	stbsv.f		stpsv.f		sger.f		\
	ssyr.f		sspr.f		ssyr2.f		\
	sspr2.f

# Single precision complex BLAS2 routines
CBLAS2 = \
	cgemv.f		cgbmv.f		chemv.f		\
	chbmv.f		chpmv.f		ctrmv.f		\
	ctbmv.f		ctpmv.f		ctrsv.f		\
	ctbsv.f		ctpsv.f		cgerc.f		\
	cgeru.f		cher.f		chpr.f		\
	cher2.f		chpr2.f

# Double precision real BLAS2 routines
DBLAS2 = \
	dgemv.f		dgbmv.f		dsymv.f		\
	dsbmv.f		dspmv.f		dtrmv.f		\
	dtbmv.f		dtpmv.f		dtrsv.f		\
	dtbsv.f		dtpsv.f		dger.f		\
	dsyr.f		dspr.f		dsyr2.f		\
	dspr2.f

# Double precision complex BLAS2 routines
ZBLAS2 = \
	zgemv.f		zgbmv.f		zhemv.f		\
	zhbmv.f		zhpmv.f		ztrmv.f		\
	ztbmv.f		ztpmv.f		ztrsv.f		\
	ztbsv.f		ztpsv.f		zgerc.f		\
	zgeru.f		zher.f		zhpr.f		\
	zher2.f		zhpr2.f

#page
## --------------------------------------------------------------------
## BLAS level 3.
## --------------------------------------------------------------------

# Single precision real BLAS3 routines
SBLAS3 = \
	sgemm.f		ssymm.f		ssyrk.f		\
	ssyr2k.f	strmm.f		strsm.f


# Single precision complex BLAS3 routines
CBLAS3 = \
	cgemm.f		csymm.f		csyrk.f		\
	csyr2k.f	ctrmm.f		ctrsm.f		\
	chemm.f		cherk.f		cher2k.f

# Double precision real BLAS3 routines
DBLAS3 = \
	dgemm.f		dsymm.f		dsyrk.f		\
	dsyr2k.f	dtrmm.f		dtrsm.f

# Double precision complex BLAS3 routines
ZBLAS3 = \
	zgemm.f		zsymm.f		zsyrk.f		\
	zsyr2k.f	ztrmm.f		ztrsm.f		\
	zhemm.f		zherk.f		zher2k.f

#page
## --------------------------------------------------------------------
## Library.
## --------------------------------------------------------------------

lib_LTLIBRARIES		= libblas.la
libblas_la_LDFLAGS	= -version-info 1:0:0
libblas_la_SOURCES	= \
		$(BLASAUX)				\
		$(SBLAS1) $(CBLAS1) $(DBLAS1) $(ZBLAS1) \
		$(SBLAS2) $(CBLAS2) $(DBLAS2) $(ZBLAS2)	\
		$(SBLAS3) $(CBLAS3) $(DBLAS3) $(ZBLAS3)

### end of file
