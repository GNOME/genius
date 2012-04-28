## Process this file with automake to produce Makefile.in

include_HEADERS	= cblas.h cblas_f77.h

# The  following line defines  auxiliary routines  needed by  both the
# Level 2 and  Level 3 BLAS.  Comment it out only  if you already have
# both the Level 2 and 3 BLAS.
BLASAUX	= \
	cblas_globals.c	cblas_xerbla.c

#page
## --------------------------------------------------------------------
## BLAS level 1.
## --------------------------------------------------------------------

# Single precision real BLAS routines
SBLAS1 = \
	cblas_isamax.c	cblas_sasum.c	cblas_saxpy.c	\
	cblas_scopy.c	cblas_sdot.c	cblas_snrm2.c	\
	cblas_srot.c	cblas_srotg.c	cblas_sscal.c	\
	cblas_sswap.c	cblas_sdsdot.c	cblas_srotmg.c	\
	cblas_srotm.c

# Single precision complex BLAS routines
CBLAS1 = \
	cblas_scasum.c	cblas_scnrm2.c			\
	cblas_icamax.c	cblas_caxpy.c	cblas_ccopy.c	\
	cblas_csscal.c	cblas_cscal.c	cblas_cswap.c	\
	cblas_cdotc_sub.c cblas_cdotu_sub.c

# Double precision real BLAS routines
DBLAS1 = \
	cblas_idamax.c	cblas_dasum.c	cblas_daxpy.c	\
	cblas_dcopy.c	cblas_ddot.c	cblas_dnrm2.c	\
	cblas_drot.c	cblas_drotg.c	cblas_dscal.c	\
	cblas_dsdot.c	cblas_dswap.c	cblas_drotmg.c	\
	cblas_drotm.c

# Double precision complex BLAS routines
ZBLAS1 = \
	cblas_dzasum.c	cblas_dznrm2.c			\
	cblas_izamax.c	cblas_zaxpy.c	cblas_zcopy.c	\
	cblas_zscal.c	cblas_zswap.c	cblas_zdscal.c	\
	cblas_zdotc_sub.c cblas_zdotu_sub.c

#page
## --------------------------------------------------------------------
## BLAS level 2.
## --------------------------------------------------------------------

# Single precision real BLAS2 routines
SBLAS2 = \
	cblas_sgemv.c	cblas_sgbmv.c	cblas_ssymv.c	\
	cblas_ssbmv.c	cblas_sspmv.c	cblas_strmv.c	\
	cblas_stbmv.c	cblas_stpmv.c	cblas_strsv.c	\
	cblas_stbsv.c	cblas_stpsv.c	cblas_sger.c	\
	cblas_ssyr.c	cblas_sspr.c	cblas_ssyr2.c	\
	cblas_sspr2.c

# Single precision complex BLAS2 routines
CBLAS2 = \
	cblas_cgemv.c	cblas_cgbmv.c	cblas_chemv.c	\
	cblas_chbmv.c	cblas_chpmv.c	cblas_ctrmv.c	\
	cblas_ctbmv.c	cblas_ctpmv.c	cblas_ctrsv.c	\
	cblas_ctbsv.c	cblas_ctpsv.c	cblas_cgerc.c	\
	cblas_cgeru.c	cblas_cher.c	cblas_chpr.c	\
	cblas_cher2.c	cblas_chpr2.c

# Double precision real BLAS2 routines
DBLAS2 = \
	cblas_dgemv.c	cblas_dgbmv.c	cblas_dsymv.c	\
	cblas_dsbmv.c	cblas_dspmv.c	cblas_dtrmv.c	\
	cblas_dtbmv.c	cblas_dtpmv.c	cblas_dtrsv.c	\
	cblas_dtbsv.c	cblas_dtpsv.c	cblas_dger.c	\
	cblas_dsyr.c	cblas_dspr.c	cblas_dsyr2.c	\
	cblas_dspr2.c

# Double precision complex BLAS2 routines
ZBLAS2 = \
	cblas_zgemv.c	cblas_zgbmv.c	cblas_zhemv.c	\
	cblas_zhbmv.c	cblas_zhpmv.c	cblas_ztrmv.c	\
	cblas_ztbmv.c	cblas_ztpmv.c	cblas_ztrsv.c	\
	cblas_ztbsv.c	cblas_ztpsv.c	cblas_zgerc.c	\
	cblas_zgeru.c	cblas_zher.c	cblas_zhpr.c	\
	cblas_zher2.c	cblas_zhpr2.c

#page
## --------------------------------------------------------------------
## BLAS level 3.
## --------------------------------------------------------------------

# Single precision real BLAS3 routines
SBLAS3 = \
	cblas_sgemm.c	cblas_ssymm.c	cblas_ssyrk.c	\
	cblas_ssyr2k.c	cblas_strmm.c	cblas_strsm.c

# Single precision complex BLAS3 routines
CBLAS3 = \
	cblas_cgemm.c	cblas_csymm.c	cblas_csyrk.c	\
	cblas_csyr2k.c	cblas_ctrmm.c	cblas_ctrsm.c	\
	cblas_chemm.c	cblas_cherk.c	cblas_cher2k.c

# Double precision real BLAS3 routines
DBLAS3 = \
	cblas_dgemm.c	cblas_dsymm.c	cblas_dsyrk.c	\
	cblas_dsyr2k.c	cblas_dtrmm.c	cblas_dtrsm.c

# Double precision complex BLAS3 routines
ZBLAS3 = \
	cblas_zgemm.c	cblas_zsymm.c	cblas_zsyrk.c	\
	cblas_zsyr2k.c	cblas_ztrmm.c	cblas_ztrsm.c	\
	cblas_zhemm.c	cblas_zherk.c	cblas_zher2k.c

#page
## --------------------------------------------------------------------
## Fortran routines.
## --------------------------------------------------------------------

FBLAS	= \
	cdotcsub.f	cdotusub.f	dasumsub.f	\
	ddotsub.f	dnrm2sub.f	dsdotsub.f	\
	dzasumsub.f	dznrm2sub.f	icamaxsub.f	\
	idamaxsub.f	isamaxsub.f	izamaxsub.f	\
	sasumsub.f	scasumsub.f	scnrm2sub.f	\
	sdotsub.f	sdsdotsub.f	snrm2sub.f	\
	zdotcsub.f	zdotusub.f

#page
## --------------------------------------------------------------------
## Library.
## --------------------------------------------------------------------

lib_LTLIBRARIES		= libcblas.la
libcblas_la_LDFLAGS	= -version-info 1:0:0
libcblas_la_LIBADD	= ../blas/libblas.la
libcblas_la_SOURCES	= \
		$(BLASAUX)				\
		$(SBLAS1) $(CBLAS1) $(DBLAS1) $(ZBLAS1) \
		$(SBLAS2) $(CBLAS2) $(DBLAS2) $(ZBLAS2)	\
		$(SBLAS3) $(CBLAS3) $(DBLAS3) $(ZBLAS3) \
		$(FBLAS)

### end of file
