## Process this file with automake to produce Makefile.in

MACHINE_DEP_DIR	= $(srcdir)/../machine-dependent

### Timer for the SECOND and DSECND routines
#
# Default: SECOND and DSECND will  use a call to the EXTERNAL FUNCTION
# ETIME.
#
#TIMER    = EXT_ETIME
#
# For  RS6K :  SECOND  and DSECND  will  use a  call  to the  EXTERNAL
# FUNCTION ETIME_
#
# TIMER    = EXT_ETIME_
#
# For  gfortran compiler: SECOND  and DSECND  will use  a call  to the
# INTERNAL FUNCTION ETIME.
TIMER    = INT_ETIME
# If your  Fortran compiler does  not provide etime (like  Nag Fortran
# Compiler, etc...)  SECOND and DSECND will use a call to the INTERNAL
# FUNCTION CPU_TIME
#
#TIMER    = INT_CPU_TIME
#
#If neither  of this  works...you can use  the NONE value...   In that
#case, SECOND and DSECND will always return 0
#
#TIMER     = NONE

#page
## --------------------------------------------------------------------
## Sources.
## --------------------------------------------------------------------

# The functions LSAME, SECOND, DSECND,  SLAMCH, and DLAMCH may have to
# be   installed  before   compiling  the   library.   Refer   to  the
# installation guide, LAPACK Working Note 41, for instructions.

# Auxiliary routines called from all precisions
ALLAUX = \
	ilaenv.f	ieeeck.f	lsamen.f	\
	xerbla.f	xerbla_array.f	iparmq.f	\
	ilaprec.f	ilatrans.f	ilauplo.f	\
	iladiag.f	chla_transtype.f		\
	$(MACHINE_DEP_DIR)/ilaver.f			\
	$(MACHINE_DEP_DIR)/lsame.f			\
	$(MACHINE_DEP_DIR)/slamch.f

# Auxiliary routines called from both REAL and COMPLEX.
SCLAUX = \
	sbdsdc.f	sbdsqr.f	sdisna.f	\
	slabad.f	slacpy.f	sladiv.f	\
	slae2.f		slaebz.f	slaed0.f	\
	slaed1.f	slaed2.f	slaed3.f	\
	slaed4.f	slaed5.f	slaed6.f	\
	slaed7.f	slaed8.f	slaed9.f	\
	slaeda.f	slaev2.f	slagtf.f	\
	slagts.f	slamrg.f	slanst.f	\
	slapy2.f	slapy3.f	slarnv.f	\
	slarra.f	slarrb.f	slarrc.f	\
	slarrd.f	slarre.f	slarrf.f	\
	slarrj.f	slarrk.f	slarrr.f	\
	slaneg.f	slartg.f	slaruv.f	\
	slas2.f		slascl.f	slasd0.f	\
	slasd1.f	slasd2.f	slasd3.f	\
	slasd4.f	slasd5.f	slasd6.f	\
	slasd7.f	slasd8.f	slasda.f	\
	slasdq.f	slasdt.f	slaset.f	\
	slasq1.f	slasq2.f	slasq3.f	\
	slasq4.f	slasq5.f	slasq6.f	\
	slasr.f		slasrt.f	slassq.f	\
	slasv2.f	spttrf.f	sstebz.f	\
	sstedc.f	ssteqr.f	ssterf.f	\
	slaisnan.f	sisnan.f	slartgp.f	\
	slartgs.f					\
	$(MACHINE_DEP_DIR)/second_$(TIMER).f

# Auxiliary routines called from both DOUBLE and COMPLEX*16.
DZLAUX = \
	dbdsdc.f	dbdsqr.f	ddisna.f	\
	dlabad.f	dlacpy.f	dladiv.f	\
	dlae2.f		dlaebz.f	dlaed0.f	\
	dlaed1.f	dlaed2.f	dlaed3.f	\
	dlaed4.f	dlaed5.f	dlaed6.f	\
	dlaed7.f	dlaed8.f	dlaed9.f	\
	dlaeda.f	dlaev2.f	dlagtf.f	\
	dlagts.f	dlamrg.f	dlanst.f	\
	dlapy2.f	dlapy3.f	dlarnv.f	\
	dlarra.f	dlarrb.f	dlarrc.f	\
	dlarrd.f	dlarre.f	dlarrf.f	\
	dlarrj.f	dlarrk.f	dlarrr.f	\
	dlaneg.f	dlartg.f	dlaruv.f	\
	dlas2.f		dlascl.f	dlasd0.f	\
	dlasd1.f	dlasd2.f	dlasd3.f	\
	dlasd4.f	dlasd5.f	dlasd6.f	\
	dlasd7.f	dlasd8.f	dlasda.f	\
	dlasdq.f	dlasdt.f	dlaset.f	\
	dlasq1.f	dlasq2.f	dlasq3.f	\
	dlasq4.f	dlasq5.f	dlasq6.f	\
	dlasr.f		dlasrt.f	dlassq.f	\
	dlasv2.f	dpttrf.f	dstebz.f	\
	dstedc.f	dsteqr.f	dsterf.f	\
	dlaisnan.f	disnan.f	dlartgp.f	\
	dlartgs.f					\
	$(MACHINE_DEP_DIR)/dlamch.f			\
	$(MACHINE_DEP_DIR)/dsecnd_$(TIMER).f

# Single precision real LAPACK routines
SLASRC = \
	sgbbrd.f	sgbcon.f	sgbequ.f	\
	sgbrfs.f	sgbsv.f		sgbsvx.f	\
	sgbtf2.f	sgbtrf.f	sgbtrs.f	\
	sgebak.f	sgebal.f	sgebd2.f	\
	sgebrd.f	sgecon.f	sgeequ.f	\
	sgees.f		sgeesx.f	sgeev.f		\
	sgeevx.f	sgegs.f		sgegv.f		\
	sgehd2.f	sgehrd.f	sgelq2.f	\
	sgelqf.f	sgels.f		sgelsd.f	\
	sgelss.f	sgelsx.f	sgelsy.f	\
	sgeql2.f	sgeqlf.f	sgeqp3.f	\
	sgeqpf.f	sgeqr2.f	sgeqr2p.f	\
	sgeqrf.f	sgeqrfp.f	sgerfs.f	\
	sgerq2.f	sgerqf.f	sgesc2.f	\
	sgesdd.f	sgesv.f		sgesvd.f	\
	sgesvx.f	sgetc2.f	sgetf2.f	\
	sgetri.f	sggbak.f	sggbal.f	\
	sgges.f		sggesx.f	sggev.f		\
	sggevx.f	sggglm.f	sgghrd.f	\
	sgglse.f	sggqrf.f	sggrqf.f	\
	sggsvd.f	sggsvp.f	sgtcon.f	\
	sgtrfs.f	sgtsv.f		sgtsvx.f	\
	sgttrf.f	sgttrs.f	sgtts2.f	\
	shgeqz.f	shsein.f	shseqr.f	\
	slabrd.f	slacon.f	slacn2.f	\
	slaein.f	slaexc.f	slag2.f		\
	slags2.f	slagtm.f	slagv2.f	\
	slahqr.f	slahrd.f	slahr2.f	\
	slaic1.f	slaln2.f	slals0.f	\
	slalsa.f	slalsd.f	slangb.f	\
	slange.f	slangt.f	slanhs.f	\
	slansb.f	slansp.f	slansy.f	\
	slantb.f	slantp.f	slantr.f	\
	slanv2.f	slapll.f	slapmt.f	\
	slaqgb.f	slaqge.f	slaqp2.f	\
	slaqps.f	slaqsb.f	slaqsp.f	\
	slaqsy.f	slaqr0.f	slaqr1.f	\
	slaqr2.f	slaqr3.f	slaqr4.f	\
	slaqr5.f	slaqtr.f	slar1v.f	\
	slar2v.f	ilaslr.f	ilaslc.f	\
	slarf.f		slarfb.f	slarfg.f	\
	slarfgp.f	slarft.f	slarfx.f	\
	slargv.f	slarrv.f	slartv.f	\
	slarz.f		slarzb.f	slarzt.f	\
	slaswp.f	slasy2.f	slasyf.f	\
	slatbs.f	slatdf.f	slatps.f	\
	slatrd.f	slatrs.f	slatrz.f	\
	slatzm.f	slauu2.f	slauum.f	\
	sopgtr.f	sopmtr.f	sorg2l.f	\
	sorg2r.f	sorgbr.f	sorghr.f	\
	sorgl2.f	sorglq.f	sorgql.f	\
	sorgqr.f	sorgr2.f	sorgrq.f	\
	sorgtr.f	sorm2l.f	sorm2r.f	\
	sormbr.f	sormhr.f	sorml2.f	\
	sormlq.f	sormql.f	sormqr.f	\
	sormr2.f	sormr3.f	sormrq.f	\
	sormrz.f	sormtr.f	spbcon.f	\
	spbequ.f	spbrfs.f	spbstf.f	\
	spbsv.f		spbsvx.f	spbtf2.f	\
	spbtrf.f	spbtrs.f	spocon.f	\
	spoequ.f	sporfs.f	sposv.f		\
	sposvx.f	spotf2.f	spotri.f	\
	spstrf.f	spstf2.f	sppcon.f	\
	sppequ.f	spprfs.f	sppsv.f		\
	sppsvx.f	spptrf.f	spptri.f	\
	spptrs.f	sptcon.f	spteqr.f	\
	sptrfs.f	sptsv.f		sptsvx.f	\
	spttrs.f	sptts2.f	srscl.f		\
	ssbev.f		ssbevd.f	ssbevx.f	\
	ssbgst.f	ssbgv.f		ssbgvd.f	\
	ssbgvx.f	ssbtrd.f	sspcon.f	\
	sspev.f		sspevd.f	sspevx.f	\
	sspgst.f	sspgv.f		sspgvd.f	\
	sspgvx.f	ssprfs.f	sspsv.f		\
	sspsvx.f	ssptrd.f	ssptrf.f	\
	ssptri.f	ssptrs.f	sstegr.f	\
	sstein.f	sstev.f		sstevd.f	\
	sstevr.f	sstevx.f	ssycon.f	\
	ssyev.f		ssyevd.f	ssyevr.f	\
	ssyevx.f	ssygs2.f	ssygst.f	\
	ssygv.f		ssygvd.f	ssygvx.f	\
	ssyrfs.f	ssysv.f		ssysvx.f	\
	ssytd2.f	ssytf2.f	ssytrd.f	\
	ssytrf.f	ssytri.f	ssytri2.f	\
	ssytri2x.f	ssyswapr.f	ssytrs.f	\
	ssytrs2.f	ssyconv.f	stbcon.f	\
	stbrfs.f	stbtrs.f	stgevc.f	\
	stgex2.f	stgexc.f	stgsen.f	\
	stgsja.f	stgsna.f	stgsy2.f	\
	stgsyl.f	stpcon.f	stprfs.f	\
	stptri.f	stptrs.f	strcon.f	\
	strevc.f	strexc.f	strrfs.f	\
	strsen.f	strsna.f	strsyl.f	\
	strti2.f	strtri.f	strtrs.f	\
	stzrqf.f	stzrzf.f	sstemr.f	\
	slansf.f	spftrf.f	spftri.f	\
	spftrs.f	ssfrk.f		stfsm.f		\
	stftri.f	stfttp.f	stfttr.f	\
	stpttf.f	stpttr.f	strttf.f	\
	strttp.f	sgejsv.f	sgesvj.f	\
	sgsvj0.f	sgsvj1.f	sgeequb.f	\
	ssyequb.f	spoequb.f	sgbequb.f	\
	sbbcsd.f	slapmr.f	sorbdb.f	\
	sorcsd.f	sgeqrt.f	sgeqrt2.f	\
	sgeqrt3.f	sgemqrt.f	stpqrt.f	\
	stpqrt2.f	stpmqrt.f	stprfb.f

# Double-single  mixed  precision real  routines  called from  single,
# single-extra and  double precision  real LAPACK routines  (i.e. from
# SLASRC, SXLASRC, DLASRC).
DSLASRC = \
	spotrs.f	sgetrs.f	spotrf.f	\
	sgetrf.f

# Single precision real LAPACK routines using extra precision.
#if USEXBLAS
#SXLASRC = \
#	sgesvxx.f	sgerfsx.f	sla_geamv.f	\
#	ssysvxx.f	ssyrfsx.f	sposvxx.f	\
#	sporfsx.f	sgbsvxx.f	sgbrfsx.f	\
#	slarscl2.f	slascl2.f			\
#	sla_gerfsx_extended.f	sla_gercond.f		\
#	sla_gerpvgrw.f		sla_syrfsx_extended.f	\
#	sla_syamv.f		sla_syrcond.f		\
#	sla_syrpvgrw.f		sla_porfsx_extended.f	\
#	sla_porcond.f		sla_gbrfsx_extended.f	\
#	sla_porpvgrw.f		sla_gbamv.f		\
#	sla_gbrcond.f		sla_gbrpvgrw.f		\
#	sla_lin_berr.f		sla_wwaddw.f
#endif

# Single precision complex LAPACK routines.
CLASRC = \
	cbdsqr.f	cgbbrd.f	cgbcon.f	\
	cgbequ.f	cgbrfs.f	cgbsv.f		\
	cgbsvx.f	cgbtf2.f	cgbtrf.f	\
	cgbtrs.f	cgebak.f	cgebal.f	\
	cgebd2.f	cgebrd.f	cgecon.f	\
	cgeequ.f	cgees.f		cgeesx.f	\
	cgeev.f		cgeevx.f	cgegs.f		\
	cgegv.f		cgehd2.f	cgehrd.f	\
	cgelq2.f	cgelqf.f	cgels.f		\
	cgelsd.f	cgelss.f	cgelsx.f	\
	cgelsy.f	cgeql2.f	cgeqlf.f	\
	cgeqp3.f	cgeqpf.f	cgeqr2.f	\
	cgeqr2p.f	cgeqrf.f	cgeqrfp.f	\
	cgerfs.f	cgerq2.f	cgerqf.f	\
	cgesc2.f	cgesdd.f	cgesv.f		\
	cgesvd.f	cgesvx.f	cgetc2.f	\
	cgetf2.f	cgetri.f	cggbak.f	\
	cggbal.f	cgges.f		cggesx.f	\
	cggev.f		cggevx.f	cggglm.f	\
	cgghrd.f	cgglse.f	cggqrf.f	\
	cggrqf.f	cggsvd.f	cggsvp.f	\
	cgtcon.f	cgtrfs.f	cgtsv.f		\
	cgtsvx.f	cgttrf.f	cgttrs.f	\
	cgtts2.f	chbev.f		chbevd.f	\
	chbevx.f	chbgst.f	chbgv.f		\
	chbgvd.f	chbgvx.f	chbtrd.f	\
	checon.f	cheev.f		cheevd.f	\
	cheevr.f	cheevx.f	chegs2.f	\
	chegst.f	chegv.f		chegvd.f	\
	chegvx.f	cherfs.f	chesv.f		\
	chesvx.f	chetd2.f	chetf2.f	\
	chetrd.f	chetrf.f	chetri.f	\
	chetri2.f	chetri2x.f	cheswapr.f	\
	chetrs.f	chetrs2.f	chgeqz.f	\
	chpcon.f	chpev.f		chpevd.f	\
	chpevx.f	chpgst.f	chpgv.f		\
	chpgvd.f	chpgvx.f	chprfs.f	\
	chpsv.f		chpsvx.f	chptrd.f	\
	chptrf.f	chptri.f	chptrs.f	\
	chsein.f	chseqr.f	clabrd.f	\
	clacgv.f	clacon.f	clacn2.f	\
	clacp2.f	clacpy.f	clacrm.f	\
	clacrt.f	cladiv.f	claed0.f	\
	claed7.f	claed8.f	claein.f	\
	claesy.f	claev2.f	clags2.f	\
	clagtm.f	clahef.f	clahqr.f	\
	clahrd.f	clahr2.f	claic1.f	\
	clals0.f	clalsa.f	clalsd.f	\
	clangb.f	clange.f	clangt.f	\
	clanhb.f	clanhe.f	clanhp.f	\
	clanhs.f	clanht.f	clansb.f	\
	clansp.f	clansy.f	clantb.f	\
	clantp.f	clantr.f	clapll.f	\
	clapmt.f	clarcm.f	claqgb.f	\
	claqge.f	claqhb.f	claqhe.f	\
	claqhp.f	claqp2.f	claqps.f	\
	claqsb.f	claqr0.f	claqr1.f	\
	claqr2.f	claqr3.f	claqr4.f	\
	claqr5.f	claqsp.f	claqsy.f	\
	clar1v.f	clar2v.f	ilaclr.f	\
	ilaclc.f	clarf.f		clarfb.f	\
	clarfg.f	clarft.f	clarfgp.f	\
	clarfx.f	clargv.f	clarnv.f	\
	clarrv.f	clartg.f	clartv.f	\
	clarz.f		clarzb.f	clarzt.f	\
	clascl.f	claset.f	clasr.f		\
	classq.f	claswp.f	clasyf.f	\
	clatbs.f	clatdf.f	clatps.f	\
	clatrd.f	clatrs.f	clatrz.f	\
	clatzm.f	clauu2.f	clauum.f	\
	cpbcon.f	cpbequ.f	cpbrfs.f	\
	cpbstf.f	cpbsv.f		cpbsvx.f	\
	cpbtf2.f	cpbtrf.f	cpbtrs.f	\
	cpocon.f	cpoequ.f	cporfs.f	\
	cposv.f		cposvx.f	cpotf2.f	\
	cpotri.f	cpstrf.f	cpstf2.f	\
	cppcon.f	cppequ.f	cpprfs.f	\
	cppsv.f		cppsvx.f	cpptrf.f	\
	cpptri.f	cpptrs.f	cptcon.f	\
	cpteqr.f	cptrfs.f	cptsv.f		\
	cptsvx.f	cpttrf.f	cpttrs.f	\
	cptts2.f	crot.f		cspcon.f	\
	cspmv.f		cspr.f		csprfs.f	\
	cspsv.f		cspsvx.f	csptrf.f	\
	csptri.f	csptrs.f	csrscl.f	\
	cstedc.f	cstegr.f	cstein.f	\
	csteqr.f	csycon.f	csymv.f		\
	csyr.f		csyrfs.f	csysv.f		\
	csysvx.f	csytf2.f	csytrf.f	\
	csytri.f	csytri2.f	csytri2x.f	\
	csyswapr.f	csytrs.f	csytrs2.f	\
	csyconv.f	ctbcon.f	ctbrfs.f	\
	ctbtrs.f	ctgevc.f	ctgex2.f	\
	ctgexc.f	ctgsen.f	ctgsja.f	\
	ctgsna.f	ctgsy2.f	ctgsyl.f	\
	ctpcon.f	ctprfs.f	ctptri.f	\
	ctptrs.f	ctrcon.f	ctrevc.f	\
	ctrexc.f	ctrrfs.f	ctrsen.f	\
	ctrsna.f	ctrsyl.f	ctrti2.f	\
	ctrtri.f	ctrtrs.f	ctzrqf.f	\
	ctzrzf.f	cung2l.f	cung2r.f	\
	cungbr.f	cunghr.f	cungl2.f	\
	cunglq.f	cungql.f	cungqr.f	\
	cungr2.f	cungrq.f	cungtr.f	\
	cunm2l.f	cunm2r.f	cunmbr.f	\
	cunmhr.f	cunml2.f	cunmlq.f	\
	cunmql.f	cunmqr.f	cunmr2.f	\
	cunmr3.f	cunmrq.f	cunmrz.f	\
	cunmtr.f	cupgtr.f	cupmtr.f	\
	icmax1.f	scsum1.f	cstemr.f	\
	chfrk.f		ctfttp.f	clanhf.f	\
	cpftrf.f	cpftri.f	cpftrs.f	\
	ctfsm.f		ctftri.f	ctfttr.f	\
	ctpttf.f	ctpttr.f	ctrttf.f	\
	ctrttp.f	cgeequb.f	cgbequb.f	\
	csyequb.f	cpoequb.f	cheequb.f	\
	cbbcsd.f	clapmr.f	cunbdb.f	\
	cuncsd.f	cgeqrt.f	cgeqrt2.f	\
	cgeqrt3.f	cgemqrt.f	ctpqrt.f	\
	ctpqrt2.f	ctpmqrt.f	ctprfb.f

# Single precision complex LAPACK routines using extra precision.
#if USEXBLAS
#CXLASRC = \
#	cgesvxx.f	cgerfsx.f	csysvxx.f	\
#	csyrfsx.f	cposvxx.f	cporfsx.f	\
#	cgbsvxx.f	cgbrfsx.f	chesvxx.f	\
#	cherfsx.f	clarscl2.f	clascl2.f 	\
#	cla_gerfsx_extended.f	cla_geamv.f		\
#	cla_gercond_c.f		cla_gercond_x.f		\
#	cla_gerpvgrw.f		cla_syrfsx_extended.f	\
#	cla_syamv.f		cla_syrcond_c.f		\
#	cla_syrcond_x.f		cla_syrpvgrw.f		\
#	cla_porfsx_extended.f	cla_porcond_c.f		\
#	cla_porcond_x.f		cla_porpvgrw.f		\
#	cla_gbrfsx_extended.f	cla_gbamv.f		\
#	cla_gbrcond_c.f		cla_gbrcond_x.f		\
#	cla_gbrpvgrw.f		cla_herfsx_extended.f	\
#	cla_heamv.f		cla_hercond_c.f		\
#	cla_hercond_x.f		cla_herpvgrw.f		\
#	cla_lin_berr.f		cla_wwaddw.f
#endif

# Double-single mixed  precision complex routines  called from single,
# single-extra and double precision complex LAPACK routines (i.e. from
# CLASRC, CXLASRC, ZLASRC).
ZCLASRC = \
	cpotrs.f	cgetrs.f	cpotrf.f	\
	cgetrf.f

# Double precision real LAPACK routines.
DLASRC = \
	dgbbrd.f	dgbcon.f	dgbequ.f	\
	dgbrfs.f	dgbsv.f		dgbsvx.f	\
	dgbtf2.f	dgbtrf.f	dgbtrs.f	\
	dgebak.f	dgebal.f	dgebd2.f	\
	dgebrd.f	dgecon.f	dgeequ.f	\
	dgees.f		dgeesx.f	dgeev.f		\
	dgeevx.f	dgegs.f		dgegv.f		\
	dgehd2.f	dgehrd.f	dgelq2.f	\
	dgelqf.f	dgels.f		dgelsd.f	\
	dgelss.f	dgelsx.f	dgelsy.f	\
	dgeql2.f	dgeqlf.f	dgeqp3.f	\
	dgeqpf.f	dgeqr2.f	dgeqr2p.f	\
	dgeqrf.f	dgeqrfp.f	dgerfs.f	\
	dgerq2.f	dgerqf.f	dgesc2.f	\
	dgesdd.f	dgesv.f		dgesvd.f	\
	dgesvx.f	dgetc2.f	dgetf2.f	\
	dgetrf.f	dgetri.f	dgetrs.f	\
	dggbak.f	dggbal.f	dgges.f		\
	dggesx.f	dggev.f		dggevx.f	\
	dggglm.f	dgghrd.f	dgglse.f	\
	dggqrf.f	dggrqf.f	dggsvd.f	\
	dggsvp.f	dgtcon.f	dgtrfs.f	\
	dgtsv.f		dgtsvx.f	dgttrf.f	\
	dgttrs.f	dgtts2.f	dhgeqz.f	\
	dhsein.f	dhseqr.f	dlabrd.f	\
	dlacon.f	dlacn2.f	dlaein.f	\
	dlaexc.f	dlag2.f		dlags2.f	\
	dlagtm.f	dlagv2.f	dlahqr.f	\
	dlahrd.f	dlahr2.f	dlaic1.f	\
	dlaln2.f	dlals0.f	dlalsa.f	\
	dlalsd.f	dlangb.f	dlange.f	\
	dlangt.f	dlanhs.f	dlansb.f	\
	dlansp.f	dlansy.f	dlantb.f	\
	dlantp.f	dlantr.f	dlanv2.f	\
	dlapll.f	dlapmt.f	dlaqgb.f	\
	dlaqge.f	dlaqp2.f	dlaqps.f	\
	dlaqsb.f	dlaqsp.f	dlaqsy.f	\
	dlaqr0.f	dlaqr1.f	dlaqr2.f	\
	dlaqr3.f	dlaqr4.f	dlaqr5.f	\
	dlaqtr.f	dlar1v.f	dlar2v.f	\
	iladlr.f	iladlc.f	dlarf.f		\
	dlarfb.f	dlarfg.f	dlarfgp.f	\
	dlarft.f	dlarfx.f	dlargv.f	\
	dlarrv.f	dlartv.f	dlarz.f		\
	dlarzb.f	dlarzt.f	dlaswp.f	\
	dlasy2.f	dlasyf.f	dlatbs.f	\
	dlatdf.f	dlatps.f	dlatrd.f	\
	dlatrs.f	dlatrz.f	dlatzm.f	\
	dlauu2.f	dlauum.f	dopgtr.f	\
	dopmtr.f	dorg2l.f	dorg2r.f	\
	dorgbr.f	dorghr.f	dorgl2.f	\
	dorglq.f	dorgql.f	dorgqr.f	\
	dorgr2.f	dorgrq.f	dorgtr.f	\
	dorm2l.f	dorm2r.f	dormbr.f	\
	dormhr.f	dorml2.f	dormlq.f	\
	dormql.f	dormqr.f	dormr2.f	\
	dormr3.f	dormrq.f	dormrz.f	\
	dormtr.f	dpbcon.f	dpbequ.f	\
	dpbrfs.f	dpbstf.f	dpbsv.f		\
	dpbsvx.f	dpbtf2.f	dpbtrf.f	\
	dpbtrs.f	dpocon.f	dpoequ.f	\
	dporfs.f	dposv.f		dposvx.f	\
	dpotf2.f	dpotrf.f	dpotri.f	\
	dpotrs.f	dpstrf.f	dpstf2.f	\
	dppcon.f	dppequ.f	dpprfs.f	\
	dppsv.f		dppsvx.f	dpptrf.f	\
	dpptri.f	dpptrs.f	dptcon.f	\
	dpteqr.f	dptrfs.f	dptsv.f		\
	dptsvx.f	dpttrs.f	dptts2.f	\
	drscl.f		dsbev.f		dsbevd.f	\
	dsbevx.f	dsbgst.f	dsbgv.f		\
	dsbgvd.f	dsbgvx.f	dsbtrd.f	\
	dspcon.f	dspev.f		dspevd.f	\
	dspevx.f	dspgst.f	dspgv.f	 	\
	dspgvd.f	dspgvx.f	dsprfs.f	\
	dspsv.f	 	dspsvx.f	dsptrd.f	\
	dsptrf.f	dsptri.f	dsptrs.f	\
	dstegr.f	dstein.f	dstev.f		\
	dstevd.f	dstevr.f	dstevx.f	\
	dsycon.f	dsyev.f		dsyevd.f	\
	dsyevr.f	dsyevx.f	dsygs2.f	\
	dsygst.f	dsygv.f		dsygvd.f	\
	dsygvx.f	dsyrfs.f	dsysv.f		\
	dsysvx.f	dsytd2.f	dsytf2.f	\
	dsytrd.f	dsytrf.f	dsytri.f	\
	dsytri2.f	dsytri2x.f	dsyswapr.f	\
	dsytrs.f	dsytrs2.f	dsyconv.f	\
	dtbcon.f	dtbrfs.f	dtbtrs.f	\
	dtgevc.f	dtgex2.f	dtgexc.f	\
	dtgsen.f	dtgsja.f	dtgsna.f	\
	dtgsy2.f	dtgsyl.f	dtpcon.f	\
	dtprfs.f	dtptri.f	dtptrs.f	\
	dtrcon.f	dtrevc.f	dtrexc.f	\
	dtrrfs.f	dtrsen.f	dtrsna.f	\
	dtrsyl.f	dtrti2.f	dtrtri.f	\
	dtrtrs.f	dtzrqf.f	dtzrzf.f	\
	dstemr.f	dsgesv.f	dsposv.f	\
	dlag2s.f	slag2d.f	dlat2s.f	\
	dlansf.f	dpftrf.f	dpftri.f	\
	dpftrs.f	dsfrk.f		dtfsm.f		\
	dtftri.f	dtfttp.f	dtfttr.f	\
	dtpttf.f	dtpttr.f	dtrttf.f	\
	dtrttp.f	dgejsv.f	dgesvj.f	\
	dgsvj0.f	dgsvj1.f	dgeequb.f	\
	dsyequb.f	dpoequb.f	dgbequb.f	\
	dbbcsd.f	dlapmr.f	dorbdb.f	\
	dorcsd.f	dgeqrt.f	dgeqrt2.f	\
	dgeqrt3.f	dgemqrt.f	dtpqrt.f	\
	dtpqrt2.f	dtpmqrt.f	dtprfb.f

# Double precision real LAPACK routines using extra precision.
#if USEXBLAS
#DXLASRC = \
#	dgesvxx.f	dgerfsx.f	dsysvxx.f	\
#	dsyrfsx.f	dposvxx.f	dporfsx.f	\
#	dgbsvxx.f	dgbrfsx.f	dlarscl2.f	\
#	dlascl2.f					\
#	dla_gerfsx_extended.f	dla_geamv.f		\
#	dla_gercond.f		dla_gerpvgrw.f		\
#	dla_syrfsx_extended.f	dla_syamv.f		\
#	dla_syrcond.f		dla_syrpvgrw.f		\
#	dla_porfsx_extended.f	dla_porcond.f		\
#	dla_porpvgrw.f	 	dla_gbrfsx_extended.f	\
#	dla_gbamv.f		dla_gbrcond.f		\
#	dla_gbrpvgrw.f		dla_lin_berr.f		\
#	dla_wwaddw.f
#endif

# Double precision complex LAPACK routines.
ZLASRC = \
	zbdsqr.f	zgbbrd.f	zgbcon.f	\
	zgbequ.f	zgbrfs.f	zgbsv.f		\
	zgbsvx.f	zgbtf2.f	zgbtrf.f	\
	zgbtrs.f	zgebak.f	zgebal.f	\
	zgebd2.f	zgebrd.f	zgecon.f	\
	zgeequ.f	zgees.f		zgeesx.f	\
	zgeev.f		zgeevx.f	zgegs.f		\
	zgegv.f		zgehd2.f	zgehrd.f	\
	zgelq2.f	zgelqf.f	zgels.f	 	\
	zgelsd.f	zgelss.f	zgelsx.f	\
	zgelsy.f	zgeql2.f	zgeqlf.f	\
	zgeqp3.f	zgeqpf.f	zgeqr2.f	\
	zgeqr2p.f	zgeqrf.f	zgeqrfp.f	\
	zgerfs.f	zgerq2.f	zgerqf.f	\
	zgesc2.f	zgesdd.f	zgesv.f		\
	zgesvd.f	zgesvx.f	zgetc2.f	\
	zgetf2.f	zgetrf.f	zgetri.f	\
	zgetrs.f	zggbak.f	zggbal.f	\
	zgges.f		zggesx.f	zggev.f		\
	zggevx.f	zggglm.f	zgghrd.f	\
	zgglse.f	zggqrf.f	zggrqf.f	\
	zggsvd.f	zggsvp.f	zgtcon.f	\
	zgtrfs.f	zgtsv.f		zgtsvx.f	\
	zgttrf.f	zgttrs.f	zgtts2.f	\
	zhbev.f		zhbevd.f	zhbevx.f	\
	zhbgst.f	zhbgv.f		zhbgvd.f	\
	zhbgvx.f	zhbtrd.f	zhecon.f	\
	zheev.f		zheevd.f	zheevr.f	\
	zheevx.f	zhegs2.f	zhegst.f	\
	zhegv.f		zhegvd.f	zhegvx.f	\
	zherfs.f	zhesv.f		zhesvx.f	\
	zhetd2.f	zhetf2.f	zhetrd.f	\
	zhetrf.f	zhetri.f	zhetri2.f	\
	zhetri2x.f	zheswapr.f	zhetrs.f	\
	zhetrs2.f	zhgeqz.f	zhpcon.f	\
	zhpev.f		zhpevd.f	zhpevx.f	\
	zhpgst.f	zhpgv.f		zhpgvd.f	\
	zhpgvx.f	zhprfs.f	zhpsv.f		\
	zhpsvx.f	zhptrd.f	zhptrf.f	\
	zhptri.f	zhptrs.f	zhsein.f	\
	zhseqr.f	zlabrd.f	zlacgv.f	\
	zlacon.f	zlacn2.f	zlacp2.f	\
	zlacpy.f	zlacrm.f	zlacrt.f	\
	zladiv.f	zlaed0.f	zlaed7.f	\
	zlaed8.f	zlaein.f	zlaesy.f	\
	zlaev2.f	zlags2.f	zlagtm.f	\
	zlahef.f	zlahqr.f	zlahrd.f	\
	zlahr2.f	zlaic1.f	zlals0.f	\
	zlalsa.f	zlalsd.f	zlangb.f	\
	zlange.f	zlangt.f	zlanhb.f	\
	zlanhe.f	zlanhp.f	zlanhs.f	\
	zlanht.f	zlansb.f	zlansp.f	\
	zlansy.f	zlantb.f	zlantp.f	\
	zlantr.f	zlapll.f	zlapmt.f	\
	zlaqgb.f	zlaqge.f	zlaqhb.f	\
	zlaqhe.f	zlaqhp.f	zlaqp2.f	\
	zlaqps.f	zlaqsb.f	zlaqr0.f	\
	zlaqr1.f	zlaqr2.f	zlaqr3.f	\
	zlaqr4.f	zlaqr5.f	zlaqsp.f	\
	zlaqsy.f	zlar1v.f	zlar2v.f	\
	ilazlr.f	ilazlc.f	zlarcm.f	\
	zlarf.f		zlarfb.f	zlarfg.f	\
	zlarft.f	zlarfgp.f	zlarfx.f	\
	zlargv.f	zlarnv.f	zlarrv.f	\
	zlartg.f	zlartv.f	zlarz.f		\
	zlarzb.f	zlarzt.f	zlascl.f	\
	zlaset.f	zlasr.f		zlassq.f	\
	zlaswp.f	zlasyf.f	zlatbs.f	\
	zlatdf.f	zlatps.f	zlatrd.f	\
	zlatrs.f	zlatrz.f	zlatzm.f	\
	zlauu2.f	zlauum.f	zpbcon.f	\
	zpbequ.f	zpbrfs.f	zpbstf.f	\
	zpbsv.f		zpbsvx.f	zpbtf2.f	\
	zpbtrf.f	zpbtrs.f	zpocon.f	\
	zpoequ.f	zporfs.f	zposv.f		\
	zposvx.f	zpotf2.f	zpotrf.f	\
	zpotri.f	zpotrs.f	zpstrf.f	\
	zpstf2.f	zppcon.f	zppequ.f	\
	zpprfs.f	zppsv.f	 	zppsvx.f	\
	zpptrf.f	zpptri.f	zpptrs.f	\
	zptcon.f	zpteqr.f	zptrfs.f	\
	zptsv.f		zptsvx.f	zpttrf.f	\
	zpttrs.f	zptts2.f	zrot.f		\
	zspcon.f	zspmv.f		zspr.f		\
	zsprfs.f	zspsv.f		zspsvx.f	\
	zsptrf.f	zsptri.f	zsptrs.f	\
	zdrscl.f	zstedc.f	zstegr.f	\
	zstein.f	zsteqr.f	zsycon.f	\
	zsymv.f		zsyr.f		zsyrfs.f	\
	zsysv.f		zsysvx.f	zsytf2.f	\
	zsytrf.f	zsytri.f	zsytri2.f	\
	zsytri2x.f	zsyswapr.f	zsytrs.f	\
	zsytrs2.f	zsyconv.f	ztbcon.f	\
	ztbrfs.f	ztbtrs.f	ztgevc.f	\
	ztgex2.f	ztgexc.f	ztgsen.f	\
	ztgsja.f	ztgsna.f	ztgsy2.f	\
	ztgsyl.f	ztpcon.f	ztprfs.f	\
	ztptri.f	ztptrs.f	ztrcon.f	\
	ztrevc.f	ztrexc.f	ztrrfs.f	\
	ztrsen.f	ztrsna.f	ztrsyl.f	\
	ztrti2.f	ztrtri.f	ztrtrs.f	\
	ztzrqf.f	ztzrzf.f	zung2l.f	\
	zung2r.f	zungbr.f	zunghr.f	\
	zungl2.f	zunglq.f	zungql.f	\
	zungqr.f	zungr2.f	zungrq.f	\
	zungtr.f	zunm2l.f	zunm2r.f	\
	zunmbr.f	zunmhr.f	zunml2.f	\
	zunmlq.f	zunmql.f	zunmqr.f	\
	zunmr2.f	zunmr3.f	zunmrq.f	\
	zunmrz.f	zunmtr.f	zupgtr.f	\
	zupmtr.f	izmax1.f	dzsum1.f	\
	zstemr.f	zcgesv.f	zcposv.f	\
	zlag2c.f	clag2z.f	zlat2c.f	\
	zhfrk.f		ztfttp.f	zlanhf.f	\
	zpftrf.f	zpftri.f	zpftrs.f	\
	ztfsm.f		ztftri.f	ztfttr.f	\
	ztpttf.f	ztpttr.f	ztrttf.f	\
	ztrttp.f	zgeequb.f	zgbequb.f	\
	zsyequb.f	zpoequb.f	zheequb.f	\
	zbbcsd.f	zlapmr.f	zunbdb.f	\
	zuncsd.f	zgeqrt.f	zgeqrt2.f	\
	zgeqrt3.f	zgemqrt.f	ztpqrt.f	\
	ztpqrt2.f	ztpmqrt.f	ztprfb.f

# Double precision complex LAPACK routines using extra precision.
#if USEXBLAS
#ZXLASRC = \
#	zgesvxx.f	zgerfsx.f	zsysvxx.f	\
#	zsyrfsx.f	zposvxx.f	zporfsx.f	\
#	zgbsvxx.f	zgbrfsx.f	zhesvxx.f	\
#	zherfsx.f	zlarscl2.f	zlascl2.f	\
#	zla_gerfsx_extended.f	zla_geamv.f		\
#	zla_gercond_c.f		zla_gercond_x.f		\
#	zla_gerpvgrw.f		zla_syrfsx_extended.f	\
#	zla_syamv.f		zla_syrcond_c.f		\
#	zla_syrcond_x.f		zla_syrpvgrw.f 		\
#	zla_porfsx_extended.f	zla_porcond_c.f		\
#	zla_porcond_x.f		zla_porpvgrw.f		\
#	zla_gbrfsx_extended.f	zla_gbamv.f		\
#	zla_gbrcond_c.f		zla_gbrcond_x.f		\
#	zla_gbrpvgrw.f		zla_herfsx_extended.f	\
#	zla_heamv.f		zla_hercond_c.f		\
#	zla_hercond_x.f		zla_herpvgrw.f		\
#	zla_lin_berr.f		zla_wwaddw.f
#endif

#page
## --------------------------------------------------------------------
## Library.
## --------------------------------------------------------------------

ALLOBJ	= \
	$(SLASRC)	$(DLASRC)	$(DSLASRC)	\
	$(CLASRC)	$(ZLASRC)	$(ZCLASRC)	\
	$(SCLAUX)	$(DZLAUX)	$(ALLAUX)
ALLXOBJ = $(SXLASRC) $(DXLASRC) $(CXLASRC) $(ZXLASRC)

noinst_LIBRARIES	= liblapack.a
#liblapack_la_LDFLAGS	= -version-info 1:0:0

#if USEXBLAS
#liblapack_a_SOURCES	= $(ALLOBJ) $(ALLXOBJ)
#else
liblapack_a_SOURCES	= $(ALLOBJ)
#endif

# .o.f:
# 	$(FORTRAN) $(OPTS) -c $< -o $@

### Without optimisation options
#
# slaruv.o: slaruv.f ; $(FORTRAN) $(NOOPT) -c $< -o $@
# dlaruv.o: dlaruv.f ; $(FORTRAN) $(NOOPT) -c $< -o $@
# sla_wwaddw.o: sla_wwaddw.f ; $(FORTRAN) $(NOOPT) -c $< -o $@
# dla_wwaddw.o: dla_wwaddw.f ; $(FORTRAN) $(NOOPT) -c $< -o $@
# cla_wwaddw.o: cla_wwaddw.f ; $(FORTRAN) $(NOOPT) -c $< -o $@
# zla_wwaddw.o: zla_wwaddw.f ; $(FORTRAN) $(NOOPT) -c $< -o $@

### end of file
