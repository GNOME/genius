/* GENIUS Calculator
 * Copyright (C) 1997-2017 Jiri (George) Lebl
 *
 * Author: Jiri (George) Lebl
 *
 * This file is part of Genius.
 *
 * Genius is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <locale.h>
#include "calc.h"
#include "util.h"

#include "mpwrap.h"

/* for backward compat */
#ifndef G_MAXINT32
#define G_MAXINT32        ((gint32)  0x7fffffff)
#endif

#if 0
/* Context sutff */
struct _MpwCtx {
	int ref_count;

	MpwErrorFunc errorout;
	int error_num;

	int default_mpfr_prec;
	gboolean double_math; /* instead of gmp, use doubles */
	
	gpointer data;
};
#endif

MpwRealNum *gel_zero = NULL;
MpwRealNum *gel_one = NULL;

static int default_mpfr_prec = 0;

#define FREE_LIST_SIZE 1125
static __mpz_struct free_mpz[FREE_LIST_SIZE];
static __mpz_struct *free_mpz_top = free_mpz;
static __mpq_struct free_mpq[FREE_LIST_SIZE];
static __mpq_struct *free_mpq_top = free_mpq;
static __mpfr_struct free_mpfr[FREE_LIST_SIZE];
static __mpfr_struct *free_mpfr_top = free_mpfr;

#define GET_INIT_MPZ(THE_z)				\
	if (free_mpz_top == free_mpz) {			\
		mpz_init (THE_z);			\
	} else {					\
		free_mpz_top--;				\
		memcpy (THE_z, free_mpz_top, sizeof (__mpz_struct));	\
	}
#define CLEAR_FREE_MPZ(THE_z)				\
	if (free_mpz_top == &free_mpz[FREE_LIST_SIZE-1] || \
	    mpz_size (THE_z) > 2) {			\
		mpz_clear (THE_z);			\
	} else {					\
		memcpy (free_mpz_top, THE_z, sizeof (__mpz_struct));	\
		free_mpz_top++;				\
	}
#define GET_INIT_MPQ(THE_q)				\
	if (free_mpq_top == free_mpq) {			\
		mpq_init (THE_q);			\
	} else {					\
		free_mpq_top--;				\
		memcpy (THE_q, free_mpq_top, sizeof (__mpq_struct));	\
	}
#define CLEAR_FREE_MPQ(THE_q)				\
	if (free_mpq_top == &free_mpq[FREE_LIST_SIZE-1] || \
	    mpz_size (mpq_denref (THE_q)) > 2 ||	\
	    mpz_size (mpq_numref (THE_q)) > 2) {	\
		mpq_clear (THE_q);			\
	} else {					\
		memcpy (free_mpq_top, THE_q, sizeof (__mpq_struct));	\
		free_mpq_top++;				\
	}
#define GET_INIT_MPF(THE_f)				\
	if (free_mpfr_top == free_mpfr) {		\
		mpfr_init (THE_f);			\
	} else {					\
		free_mpfr_top--;			\
		memcpy (THE_f, free_mpfr_top, sizeof (__mpfr_struct));	\
	}
#define CLEAR_FREE_MPF(THE_f)				\
	if (free_mpfr_top == &free_mpfr[FREE_LIST_SIZE-1] || \
	    mpfr_get_prec (THE_f) != default_mpfr_prec) { \
		mpfr_clear (THE_f);			\
	} else {					\
		memcpy (free_mpfr_top, THE_f, sizeof (__mpfr_struct));	\
		free_mpfr_top++;			\
	}

#define MAKE_CPLX_OPS(THE_op,THE_r,THE_i) {		\
	if(rop==THE_op) {				\
		THE_r = g_alloca (sizeof (MpwRealNum));	\
		THE_i = g_alloca (sizeof (MpwRealNum));	\
		mpwl_init_type(THE_r,THE_op->r->type);	\
		mpwl_init_type(THE_i,THE_op->i->type);	\
		mpwl_set(THE_r,THE_op->r);		\
		mpwl_set(THE_i,THE_op->i);		\
	} else {					\
		THE_r = THE_op->r;			\
		THE_i = THE_op->i;			\
	}						\
}
#define BREAK_CPLX_OPS(THE_op,THE_r,THE_i) {		\
	if(rop==THE_op) {				\
		mpwl_clear(THE_i);			\
		mpwl_clear(THE_r);			\
	}						\
}

#ifdef MEM_DEBUG_FRIENDLY
# define GET_NEW_REAL(n) (n = g_new0 (MpwRealNum, 1));
#else

/* In tests it seems that this achieves better then 4096 */
#define GEL_CHUNK_SIZE 4048
#define ALIGNED_SIZE(t) (sizeof(t) + sizeof (t) % G_MEM_ALIGN)

static MpwRealNum *free_reals = NULL;

static void
_gel_make_free_reals (void)
{
	guint i;
	char *p;

	p = g_malloc ((GEL_CHUNK_SIZE / ALIGNED_SIZE (MpwRealNum)) *
		      ALIGNED_SIZE (MpwRealNum));
	for (i = 0; i < (GEL_CHUNK_SIZE / ALIGNED_SIZE (MpwRealNum)); i++) {
		MpwRealNum *t = (MpwRealNum *)p;
		/*put onto the free list*/
		t->alloc.next = free_reals;
		free_reals = t;
		p += ALIGNED_SIZE (MpwRealNum);
	}
}
#define GET_NEW_REAL(n) {				\
	if G_UNLIKELY (free_reals == NULL) {		\
		_gel_make_free_reals ();		\
	}						\
	(n) = free_reals;				\
    	free_reals = free_reals->alloc.next;		\
}

#endif

#define MAKE_COPY(n) {					\
	if((n)->alloc.usage>1) {			\
		MpwRealNum *m;				\
		GET_NEW_REAL(m);			\
		m->alloc.usage = 1;			\
		mpwl_init_type(m,(n)->type);		\
		mpwl_set(m,(n));			\
		(n)->alloc.usage --;			\
		(n) = m;				\
	}						\
}
#define MAKE_EMPTY(n,type) {				\
	if((n)->alloc.usage>1) {			\
		MpwRealNum *m;				\
		GET_NEW_REAL(m);			\
		m->alloc.usage = 1;			\
		mpwl_init_type(m,type);			\
		(n)->alloc.usage --;			\
		(n) = m;				\
	}						\
}
#define DEALLOC_MPWL(n) {				\
		(n)->alloc.usage--;			\
		if((n)->alloc.usage==0)			\
			mpwl_free((n));			\
}
#define ALLOC_MPWL(n) {					\
		(n)->alloc.usage++;			\
}
#define MAKE_REAL(n) {					\
		if((n)->i != gel_zero) {		\
			(n)->i->alloc.usage--;		\
			if((n)->i->alloc.usage==0)	\
				mpwl_free((n)->i);	\
			(n)->i = gel_zero;		\
			gel_zero->alloc.usage++;	\
		}					\
}
#define MAKE_IMAG(n) {					\
	if((n)->r != gel_zero) {			\
		(n)->r->alloc.usage--;			\
		if((n)->r->alloc.usage==0)		\
			mpwl_free((n)->r);		\
		(n)->r = gel_zero;			\
		gel_zero->alloc.usage++;		\
	}						\
}

/* Just quick hacks to get a mpf, tmp should be an unused mpfr_ptr,
   rop should be mpfr_ptr and op should be mpw_ptr */
#define MPWL_MPF(rop,op,tmp) { \
	if (op->type == MPW_FLOAT) { \
		rop = op->data.fval; \
	} else if (op->type == MPW_INTEGER) { \
		GET_INIT_MPF (tmp); \
		mpfr_set_z (tmp, op->data.ival, GMP_RNDN); \
		rop = tmp; \
	} else /* if (op->r->type == MPW_RATIONAL) */ { \
		GET_INIT_MPF (tmp); \
		mpfr_set_q (tmp, op->data.rval, GMP_RNDN); \
		rop = tmp; \
	} \
}
#define MPWL_MPF_KILL(rop,tmp) { if (tmp == rop) {CLEAR_FREE_MPF (tmp);} }

#define MPWL_MPZ(rop,op,tmp) { \
	if (op->type == MPW_FLOAT) { \
		GET_INIT_MPZ (tmp); \
		mpz_set_fr (tmp, \
			    op->data.fval, \
			    GMP_RNDN); \
		rop = tmp; \
	} else if (op->type == MPW_INTEGER) { \
		rop = op->data.ival; \
	} else /* if (op->r->type == MPW_RATIONAL) */ { \
		GET_INIT_MPZ (tmp); \
		mpz_set_q (tmp, op->data.rval); \
		rop = tmp; \
	} \
}
#define MPWL_MPZ_KILL(rop,tmp) { if (tmp == rop) {CLEAR_FREE_MPZ (tmp);} }

#define MPWL_MPQ(rop,op,tmp) { \
	if (op->type == MPW_FLOAT) { \
		GET_INIT_MPQ (tmp); \
		mympq_set_fr (tmp, \
			      op->data.fval); \
		rop = tmp; \
	} else if (op->type == MPW_INTEGER) { \
		GET_INIT_MPQ (tmp); \
		mpq_set_z (tmp, op->data.ival); \
		rop = tmp; \
	} else /* if (op->r->type == MPW_RATIONAL) */ { \
		rop = op->data.rval; \
	} \
}
#define MPWL_MPQ_KILL(rop,tmp) { if (tmp == rop) {CLEAR_FREE_MPQ (tmp);} }

#define MPWL_MAX_TYPE(op1,op2) MAX(op1->type,op2->type)

/*************************************************************************/
/*low level stuff prototypes                                             */
/*************************************************************************/

/*my own power function for ints, very simple :) */
static void mympz_pow_z(mpz_t rop,mpz_t op,mpz_t e);

static gboolean mympq_perfect_square_p (mpq_t op);

static void mympq_set_fr (mpq_ptr q, mpfr_srcptr fr);

static void mpwl_make_type(MpwRealNum *op,int type);

/*make new type and clear the old one*/
static void mpwl_make_same_type(MpwRealNum *op1,MpwRealNum *op2);

static void mpwl_clear(MpwRealNum *op);

static void mpwl_init_type(MpwRealNum *op,int type);

static void mpwl_free(MpwRealNum *op);

static int mpwl_sgn (MpwRealNum *op) G_GNUC_PURE;
static int mpwl_zero_p (MpwRealNum *op) G_GNUC_PURE;

static long mpwl_get_exp(MpwRealNum *op);

static void mpwl_set_si(MpwRealNum *rop,signed long int i);
static void mpwl_set_ui(MpwRealNum *rop,unsigned long int i);
static void mpwl_set_d(MpwRealNum *rop,double d);

static void mpwl_move(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_set(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_add(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_add_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i);
static void mpwl_sub(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_sub_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i);
static void mpwl_ui_sub(MpwRealNum *rop,unsigned long i,MpwRealNum *op);

static void mpwl_mul(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_mul_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i);

static void mpwl_div(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_div_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i);
static void mpwl_ui_div(MpwRealNum *rop,unsigned long int i,MpwRealNum *op);

static void mpwl_mod(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static gboolean mpwl_invert (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2);
static void mpwl_gcd(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_jacobi(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_legendre(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_kronecker(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_lucnum (MpwRealNum *rop, MpwRealNum *op);
static void mpwl_nextprime (MpwRealNum *rop, MpwRealNum *op);
static gboolean mpwl_perfect_square(MpwRealNum *op);
static gboolean mpwl_perfect_power(MpwRealNum *op);
static gboolean mpwl_even_p(MpwRealNum *op);
static gboolean mpwl_odd_p(MpwRealNum *op);

static void mpwl_neg(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_fac_ui (MpwRealNum *rop, unsigned int op);
static void mpwl_fac (MpwRealNum *rop, MpwRealNum *op);

static void mpwl_bin_ui (MpwRealNum *rop, MpwRealNum *op, unsigned long r);

static void mpwl_dblfac_ui (MpwRealNum *rop, unsigned int op);
static void mpwl_dblfac (MpwRealNum *rop, MpwRealNum *op);

static gboolean mpwl_pow_q(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

/*power to an unsigned long and optionally invert the answer*/
static void mpwl_pow_ui(MpwRealNum *rop,MpwRealNum *op1,unsigned int e,
			gboolean reverse);

static void mpwl_pow_z(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static gboolean mpwl_pow_f(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static gboolean mpwl_pow(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static void mpwl_powm(MpwRealNum *rop,
		      MpwRealNum *op1,
		      MpwRealNum *op2,
		      MpwRealNum *mod);
static void mpwl_powm_ui(MpwRealNum *rop,
			 MpwRealNum *op,
			 unsigned long int e,
			 MpwRealNum *mod);

static gboolean mpwl_sqrt(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_exp(MpwRealNum *rop,MpwRealNum *op);
static gboolean mpwl_ln(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_sin(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_cos(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_sinh(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_cosh(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_arctan(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_arctan2(MpwRealNum *rop,MpwRealNum *op1, MpwRealNum *op2);
static void mpwl_pi (MpwRealNum *rop);
static void mpwl_ln2 (MpwRealNum *rop);
static void mpwl_euler_constant (MpwRealNum *rop);
static void mpwl_catalan_constant (MpwRealNum *rop);
static void mpwl_rand (MpwRealNum *rop);
static void mpwl_randint (MpwRealNum *rop, MpwRealNum *op);

static int mpwl_cmp(MpwRealNum *op1, MpwRealNum *op2);

static int mpwl_cmp_ui(MpwRealNum *op, unsigned long int i);

static int mpwl_eql(MpwRealNum *op1, MpwRealNum *op2);

static inline void mpwl_make_int(MpwRealNum *rop);

/*make number into a float, this might be neccessary for unprecise
  calculations*/
static void mpwl_make_float(MpwRealNum *rop);

/*round to an integer*/
static void mpwl_round(MpwRealNum *rop);
static void mpwl_ceil(MpwRealNum *rop);
static void mpwl_floor(MpwRealNum *rop);

static void mpwl_set_str_float(MpwRealNum *rop,const char *s,int base);
static void mpwl_set_str_int(MpwRealNum *rop,const char *s,int base);

static void mpwl_numerator(MpwRealNum *rop, MpwRealNum *op);
static void mpwl_denominator(MpwRealNum *rop, MpwRealNum *op);

/**************/
/*output stuff*/

enum {
	MPWL_EXCEPTION_NO_EXCEPTION = 0,
	MPWL_EXCEPTION_CONVERSION_ERROR,
	MPWL_EXCEPTION_NUMBER_TOO_LARGE,
};

/*get a long if possible*/
static long mpwl_get_long(MpwRealNum *op, int *ex);
static unsigned long mpwl_get_ulong(MpwRealNum *op, int *ex);
/*get a double if possible*/
static double mpwl_get_double(MpwRealNum *op, int *ex);

/*trim trailing zeros*/
static void str_trim_trailing_zeros (char *s, gboolean leave_first_zero);

/*formats a floating point with mantissa in p and exponent in e*/
static char * str_format_float (char *p,
				long int e,
				int max_digits,
				gboolean scientific_notation);

static char * str_getstring_z (mpz_ptr num, int max_digits,
			      gboolean scientific_notation,
			      int integer_output_base,
			      const char *postfix);
static char * str_getstring_q (mpq_ptr num, int max_digits,
			       gboolean scientific_notation,
			       gboolean mixed_fractions,
			       GelOutputStyle style,
			       const char *postfix,
			       int float_chop);
static char * str_getstring_f (mpfr_ptr num, int max_digits,
			       gboolean scientific_notation,
			       const char *postfix,
			       int chop);

static char * mpwl_getstring(MpwRealNum * num, int max_digits,
			     gboolean scientific_notation,
			     gboolean results_as_floats,
			     gboolean mixed_fractions,
			     GelOutputStyle style,
			     int integer_output_base,
			     const char *postfix,
			     int chop /* -1 if not chopping */);

#define mpwl_getstring_for_error(n) \
		mpwl_getstring ((n),					\
				12 /* max_digits */,			\
				FALSE /* scientific_notation */,	\
				FALSE /* results_as_floats */,		\
				FALSE /* mixed_fractions */,		\
				GEL_OUTPUT_NORMAL,			\
				10 /* integer_output_base */,		\
				"" /* postfix */,			\
				-1 /* chop */);

#define mpw_getstring_for_error(n) \
		mpw_getstring ((n),					\
			       12 /* max_digits */,			\
			       FALSE /* scientific_notation */,		\
			       FALSE /* results_as_floats */,		\
			       FALSE /* mixed_fractions */,		\
			       GEL_OUTPUT_NORMAL,			\
			       10 /* integer_output_base */,		\
			       FALSE /* add_parenths */);

/*************************************************************************/
/*low level stuff                                                        */
/*************************************************************************/


static void
mympq_set_fr (mpq_ptr q, mpfr_srcptr fr)
{
	long exp;
	int sgn = mpfr_sgn (fr);
	
	/* FIXME: check if correct */
	mpz_set_ui (mpq_denref (q), 1);
	exp = mpfr_get_z_exp (mpq_numref (q), fr);
	if (exp > 0)
		mpq_mul_2exp (q, q, exp);
	else if (exp < 0)
		mpq_div_2exp (q, q, -exp);
	if (sgn < 0)
		mpq_neg (q, q);
}

/*my own power function for ints, very simple :), assumes that
 * e is positive */
static void
mympz_pow_z(mpz_t rop,mpz_t op,mpz_t e)
{
	if (mpz_sgn (e) == 0) {
		mpz_set_ui (rop, 1);
		return;
	}

	if G_LIKELY (mpz_fits_ulong_p (e)) {
		unsigned int exp = mpz_get_ui (e);
		mpz_pow_ui (rop, op, exp);
	} else {
		if (mpz_cmp_ui (op, 1) == 0) {
			mpz_set_ui (rop, 1);
		} else if (mpz_cmp_si (op, -1) == 0) {
			if (mpz_even_p (e))
				mpz_set_ui (rop, 1);
			else
				mpz_set_si (rop, -1);
		} else {
			gel_errorout (_("Integer exponent too large to compute"));
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			mpz_set_ui (rop, 1);
		}
	}
}

static gboolean
mympq_perfect_square_p (mpq_t op)
{
	return mpz_perfect_square_p (mpq_numref(op)) &&
		mpz_perfect_square_p (mpq_denref(op));
}

static void
mpwl_make_type (MpwRealNum *op, int type)
{
	if (op->type == type)
		return;
	switch (type) {
	case MPW_INTEGER:
		{
			mpz_t ival; 
			GET_INIT_MPZ (ival);

			if (op->type == MPW_FLOAT) {
				mpz_set_fr (ival,
					    op->data.fval,
					    GMP_RNDN);
				CLEAR_FREE_MPF (op->data.fval);
			} else /* if(op->type==MPW_RATIONAL) */ {
				mpz_set_q (ival, op->data.rval);
				CLEAR_FREE_MPQ (op->data.rval);
			}
			op->type = MPW_INTEGER;
			memcpy (op->data.ival, ival,
				sizeof (__mpz_struct));
		}
		break;
	case MPW_RATIONAL:
		{
			mpq_t rval; 
			GET_INIT_MPQ (rval);
			if (op->type == MPW_INTEGER) {
				mpq_set_z (rval, op->data.ival);
				CLEAR_FREE_MPZ (op->data.ival);
			} else /* if(op->type==MPW_FLOAT) */ {
				mympq_set_fr (rval,op->data.fval);
				CLEAR_FREE_MPF (op->data.fval);
			}
			op->type = MPW_RATIONAL;
			memcpy (op->data.rval, rval,
				sizeof (__mpq_struct));
		}
		break;
	case MPW_FLOAT:
		{
			mpfr_t fval; 
			GET_INIT_MPF (fval);

			if (op->type == MPW_INTEGER) {
				mpfr_set_z (fval, op->data.ival, GMP_RNDN);
				CLEAR_FREE_MPZ (op->data.ival);
			} else /* if(op->type==MPW_RATIONAL) */ {
				mpfr_set_q (fval, op->data.rval, GMP_RNDN);
#if 0
				/* XXX: a hack!!
				 * get around a mpf_set_q bug*/
				if (mpq_sgn (op->data.rval) < 0 &&
				    mpfr_sgn (fval) > 0) {
					mpfr_neg (fval, fval, GMP_RNDN);
				}
#endif
				CLEAR_FREE_MPQ (op->data.rval);
			}
			op->type = MPW_FLOAT;
			memcpy (op->data.fval, fval,
				sizeof (__mpfr_struct));
		}
		break;
	default:
		break;
	}
}

/*make new type and clear the old one*/
static void
mpwl_make_same_type(MpwRealNum *op1,MpwRealNum *op2)
{
	if(op1->type==op2->type)
		return;
	else if(op1->type > op2->type)
		mpwl_make_type(op2,op1->type);
	else /*if(op1->type < op2->type)*/
		mpwl_make_type(op1,op2->type);
}

static void
mpwl_clear (MpwRealNum *op)
{
	if G_UNLIKELY (op == NULL)
		return;

	switch(op->type) {
	case MPW_INTEGER:
		CLEAR_FREE_MPZ (op->data.ival);
		break;
	case MPW_RATIONAL:
		CLEAR_FREE_MPQ (op->data.rval);
		break;
	case MPW_FLOAT:
		CLEAR_FREE_MPF (op->data.fval);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
mpwl_init_type(MpwRealNum *op,int type)
{
	if G_UNLIKELY (op == NULL)
		return;

	op->type = type;

	switch (type) {
	case MPW_INTEGER:
		GET_INIT_MPZ (op->data.ival);
		break;
	case MPW_RATIONAL:
		GET_INIT_MPQ (op->data.rval);
		break;
	case MPW_FLOAT:
		GET_INIT_MPF (op->data.fval);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
mpwl_free(MpwRealNum *op)
{
	if G_UNLIKELY (op == NULL)
		return;

	mpwl_clear(op);

#ifdef MEM_DEBUG_FRIENDLY
	g_free (op);
#else
	op->alloc.next = free_reals;
	free_reals = op;
#endif
}

static inline int
mpwl_sgn(MpwRealNum *op)
{
	switch(op->type) {
	case MPW_FLOAT: return mpfr_sgn(op->data.fval);
	case MPW_RATIONAL: return mpq_sgn(op->data.rval);
	case MPW_INTEGER: return mpz_sgn(op->data.ival);
	default: return 0;
	}
}

static int
mpwl_zero_p (MpwRealNum *op) /* PURE!*/
{
	switch(op->type) {
	case MPW_FLOAT: return mpfr_zero_p (op->data.fval);
	case MPW_RATIONAL: return mpq_sgn(op->data.rval) == 0;
	case MPW_INTEGER: return mpz_sgn(op->data.ival) == 0;
	default: return 0;
	}
}

static long
mpwl_get_exp (MpwRealNum *op)
{
	if (op->type == MPW_FLOAT) {
		return mpfr_get_exp (op->data.fval);
	} else {
		long e;
		mpfr_ptr op_f;
		mpfr_t op_tmp;
		MPWL_MPF (op_f, op, op_tmp);
		e = mpfr_get_exp (op_f);
		MPWL_MPF_KILL (op_f, op_tmp);
		return e;
	}
}

static gboolean
mpwl_eql (MpwRealNum *op1, MpwRealNum *op2)
{
	int r=0;

	if (op1->type == op2->type) {
		switch(op1->type) {
		case MPW_FLOAT:
			return mpfr_equal_p (op1->data.fval,op2->data.fval);
		case MPW_RATIONAL:
			return mpq_equal (op1->data.rval,op2->data.rval);
		case MPW_INTEGER:
			return (mpz_cmp (op1->data.ival,op2->data.ival) == 0);
		default:
			break;
		}
	} else {
		switch (MPWL_MAX_TYPE (op1, op2)) {
		case MPW_FLOAT:
			{
				mpfr_ptr op1_f, op2_f;
				mpfr_t op1_tmp, op2_tmp;
				MPWL_MPF (op1_f, op1, op1_tmp);
				MPWL_MPF (op2_f, op2, op2_tmp);
				r = mpfr_equal_p (op1_f, op2_f);
				MPWL_MPF_KILL (op1_f, op1_tmp);
				MPWL_MPF_KILL (op2_f, op2_tmp);
			}
			break;
		case MPW_RATIONAL:
			{
				mpq_ptr op1_q, op2_q;
				mpq_t op1_tmp, op2_tmp;
				MPWL_MPQ (op1_q, op1, op1_tmp);
				MPWL_MPQ (op2_q, op2, op2_tmp);
				r = mpq_equal (op1_q, op2_q);
				MPWL_MPQ_KILL (op1_q, op1_tmp);
				MPWL_MPQ_KILL (op2_q, op2_tmp);
			}
			break;
			/*
		case MPW_INTEGER:
			return mpz_cmp(op1->data.ival,op2->data.ival);
			*/
		default:
			break;
		}
	}
	return r;
}

static int
mpwl_cmp(MpwRealNum *op1, MpwRealNum *op2)
{
	int r=0;

	if (op1->type == op2->type) {
		switch(op1->type) {
		case MPW_FLOAT:
			return mpfr_cmp(op1->data.fval,op2->data.fval);
		case MPW_RATIONAL:
			return mpq_cmp(op1->data.rval,op2->data.rval);
		case MPW_INTEGER:
			return mpz_cmp(op1->data.ival,op2->data.ival);
		default:
			break;
		}
	} else {
		switch (MPWL_MAX_TYPE (op1, op2)) {
		case MPW_FLOAT:
			{
				mpfr_ptr op1_f, op2_f;
				mpfr_t op1_tmp, op2_tmp;
				MPWL_MPF (op1_f, op1, op1_tmp);
				MPWL_MPF (op2_f, op2, op2_tmp);
				r = mpfr_cmp (op1_f, op2_f);
				MPWL_MPF_KILL (op1_f, op1_tmp);
				MPWL_MPF_KILL (op2_f, op2_tmp);
			}
			break;
		case MPW_RATIONAL:
			{
				mpq_ptr op1_q, op2_q;
				mpq_t op1_tmp, op2_tmp;
				MPWL_MPQ (op1_q, op1, op1_tmp);
				MPWL_MPQ (op2_q, op2, op2_tmp);
				r = mpq_cmp (op1_q, op2_q);
				MPWL_MPQ_KILL (op1_q, op1_tmp);
				MPWL_MPQ_KILL (op2_q, op2_tmp);
			}
			break;
			/*
		case MPW_INTEGER:
			return mpz_cmp(op1->data.ival,op2->data.ival);
			*/
		default:
			break;
		}
	}
	return r;
}

static int
mpwl_cmp_ui(MpwRealNum *op, unsigned long int i)
{
	switch(op->type) {
	case MPW_FLOAT: return mpfr_cmp_ui(op->data.fval,i);
	case MPW_RATIONAL: return mpq_cmp_ui(op->data.rval,i,1);
	case MPW_INTEGER: return mpz_cmp_ui(op->data.ival,i);
	default: return 0;
	}
}

static void
mpwl_set_d(MpwRealNum *rop,double d)
{
	switch(rop->type) {
	case MPW_FLOAT:
		mpfr_set_d (rop->data.fval, d, GMP_RNDN);
		break;
	case MPW_RATIONAL:
	case MPW_INTEGER:
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
		mpfr_set_d (rop->data.fval, d, GMP_RNDN);
		break;
	default: break;
	}
}

static void
mpwl_set_si(MpwRealNum *rop,signed long int i)
{
	switch(rop->type) {
	case MPW_FLOAT:
		mpwl_clear(rop);
		break;
	case MPW_RATIONAL:
		mpwl_clear(rop);
		break;
	case MPW_INTEGER:
		mpz_set_si(rop->data.ival,i);
		return;
	default: break;
	}
	mpwl_init_type (rop, MPW_INTEGER);
	mpz_set_si (rop->data.ival,i);
}

static void
mpwl_set_ui(MpwRealNum *rop,unsigned long int i)
{
	switch(rop->type) {
	case MPW_FLOAT:
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
		mpz_set_ui(rop->data.ival,i);
		break;
	case MPW_RATIONAL:
		mpq_set_ui(rop->data.rval,i,1);
		break;
	case MPW_INTEGER:
		mpz_set_ui(rop->data.ival,i);
		break;
	default: break;
	}
}

/*the original op should be a local or not be used anymore*/
static void
mpwl_move(MpwRealNum *rop,MpwRealNum *op)
{
	if (rop == op)
		return;

	mpwl_clear (rop);
	memcpy(rop,op,sizeof(MpwRealNum));
	rop->alloc.usage=1;
}

static void
mpwl_set(MpwRealNum *rop,MpwRealNum *op)
{
	if(rop==op)
		return;
	else if(rop->type==op->type) {
		switch(op->type) {
		case MPW_FLOAT:
			mpfr_set (rop->data.fval, op->data.fval, GMP_RNDN);
			break;
		case MPW_RATIONAL:
			mpq_set(rop->data.rval,op->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_set(rop->data.ival,op->data.ival);
			break;
		default: break;
		}
	} else {
		mpwl_clear(rop);
		mpwl_init_type(rop,op->type);
		mpwl_set(rop,op);
	}
}

static void
mpwl_add(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};
	MpwRealNum *rp = NULL;
	
	/*special case*/
	if (op1->type == op2->type) {
		if (rop->type != op1->type) {
			mpwl_clear (rop);
			mpwl_init_type (rop, op1->type);
		}
		switch (op1->type) {
		case MPW_FLOAT:
			mpfr_add (rop->data.fval,
				  op1->data.fval, op2->data.fval,
				  GMP_RNDN);
			break;
		case MPW_RATIONAL:
			mpq_add(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_add(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		default: break;
		}
		return;
	}

	t = MPWL_MAX_TYPE (op1, op2);
	if (rop != op1 && rop != op2 && rop->type == t) {
		rp = rop;
	} else {
		mpwl_init_type (&r, t);
		rp = &r;
	}

	switch (t) {
	case MPW_FLOAT:
		if (op1->type == MPW_FLOAT) {
			switch(op2->type) {
			case MPW_RATIONAL:
				mpfr_add_q (rp->data.fval, op1->data.fval,
					    op2->data.rval, GMP_RNDN);
				break;
			case MPW_INTEGER:
				mpfr_add_z (rp->data.fval, op1->data.fval,
					    op2->data.ival, GMP_RNDN);
				break;
			default: break;
			}
		} else /* op2 is MPW_FLOAT */ {
			switch(op1->type) {
			case MPW_RATIONAL:
				mpfr_add_q (rp->data.fval, op2->data.fval,
					    op1->data.rval, GMP_RNDN);
				break;
			case MPW_INTEGER:
				mpfr_add_z (rp->data.fval, op2->data.fval,
					    op1->data.ival, GMP_RNDN);
				break;
			default: break;
			}
		}
		break;
	case MPW_RATIONAL:
		{
			mpq_ptr op1_q, op2_q;
			mpq_t op1_tmp, op2_tmp;
			MPWL_MPQ (op1_q, op1, op1_tmp);
			MPWL_MPQ (op2_q, op2, op2_tmp);
			mpq_add (rp->data.rval, op1_q, op2_q);
			mpwl_make_int (rp);
			MPWL_MPQ_KILL (op1_q, op1_tmp);
			MPWL_MPQ_KILL (op2_q, op2_tmp);
		}
		break;
		/*
	case MPW_INTEGER:
		mpz_add (rp->data.ival, op1->data.ival, op2->data.ival);
		break;
		*/
	default: break;
	}

	if (rp != rop)
		mpwl_move (rop, &r);
}

static void
mpwl_add_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i)
{
	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type != MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpfr_add_ui (rop->data.fval, op->data.fval, i, GMP_RNDN);
		break;
	case MPW_RATIONAL:
		{
			mpq_t tmp;
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_init(tmp);
			mpq_set_ui(tmp,i,1);
			mpq_add(rop->data.rval,op->data.rval,tmp);
			mpq_clear(tmp);
			mpwl_make_int(rop);
		}
		break;
	case MPW_INTEGER:
		mpz_add_ui(rop->data.ival,op->data.ival,i);
		break;
	default: break;
	}
}

static void
mpwl_sub(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};
	MpwRealNum *rp = NULL;

	/*special case*/
	if (op1->type == op2->type) {
		if (rop->type != op1->type) {
			mpwl_clear (rop);
			mpwl_init_type (rop, op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpfr_sub (rop->data.fval,
				  op1->data.fval, op2->data.fval,
				  GMP_RNDN);
			break;
		case MPW_RATIONAL:
			mpq_sub(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_sub(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		default: break;
		}
		return;
	}

	t = MPWL_MAX_TYPE (op1, op2);
	if (rop != op1 && rop != op2 && rop->type == t) {
		rp = rop;
	} else {
		mpwl_init_type (&r, t);
		rp = &r;
	}

	switch (t) {
	case MPW_FLOAT:
		if (op1->type == MPW_FLOAT) {
			switch(op2->type) {
			case MPW_RATIONAL:
				mpfr_sub_q (rp->data.fval, op1->data.fval,
					    op2->data.rval, GMP_RNDN);
				break;
			case MPW_INTEGER:
				mpfr_sub_z (rp->data.fval, op1->data.fval,
					    op2->data.ival, GMP_RNDN);
				break;
			default: break;
			}
		} else /* op2 is MPW_FLOAT */ {
			switch(op1->type) {
			case MPW_RATIONAL:
				mpfr_sub_q (rp->data.fval, op2->data.fval,
					    op1->data.rval, GMP_RNDN);
				break;
			case MPW_INTEGER:
				mpfr_sub_z (rp->data.fval, op2->data.fval,
					    op1->data.ival, GMP_RNDN);
				break;
			default: break;
			}
			mpfr_neg (rp->data.fval, rp->data.fval, GMP_RNDN);
		}
		break;
	case MPW_RATIONAL:
		{
			mpq_ptr op1_q, op2_q;
			mpq_t op1_tmp, op2_tmp;
			MPWL_MPQ (op1_q, op1, op1_tmp);
			MPWL_MPQ (op2_q, op2, op2_tmp);
			mpq_sub (rp->data.rval, op1_q, op2_q);
			mpwl_make_int (rp);
			MPWL_MPQ_KILL (op1_q, op1_tmp);
			MPWL_MPQ_KILL (op2_q, op2_tmp);
		}
		break;
		/*
	case MPW_INTEGER:
		mpz_sub (rp->data.ival, op1->data.ival, op2->data.ival);
		break;
		*/
	default: break;
	}

	if (rop != rp)
		mpwl_move (rop, &r);
}

static void
mpwl_sub_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i)
{
	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type != MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpfr_sub_ui (rop->data.fval, op->data.fval, i,
			     GMP_RNDN);
		break;
	case MPW_RATIONAL:
		{
			mpq_t tmp;
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_init(tmp);
			mpq_set_ui(tmp,i,1);
			mpq_neg(tmp,tmp);
			mpq_add(rop->data.rval,op->data.rval,tmp);
			mpq_clear(tmp);
			mpwl_make_int(rop);
		}
		break;
	case MPW_INTEGER:
		mpz_sub_ui(rop->data.ival,op->data.ival,i);
		break;
	default: break;
	}
}

static void
mpwl_ui_sub(MpwRealNum *rop, unsigned long i, MpwRealNum *op)
{
	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type != MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpfr_ui_sub (rop->data.fval, i, op->data.fval,
			     GMP_RNDN);
		break;
	case MPW_RATIONAL:
		{
			mpq_t tmp;
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_init(tmp);
			mpq_set_ui(tmp,i,1);
			mpq_sub(rop->data.rval,op->data.rval,tmp);
			mpq_clear(tmp);
			mpwl_make_int(rop);
		}
		break;
	case MPW_INTEGER:
		mpz_sub_ui(rop->data.ival,op->data.ival,i);
		mpz_neg(rop->data.ival,rop->data.ival);
		break;
	default: break;
	}
}

static void
mpwl_mul(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};
	MpwRealNum *rp = NULL;

	/*special case*/
	if (op1->type == op2->type) {
		if (rop->type != op1->type) {
			mpwl_clear (rop);
			mpwl_init_type (rop, op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpfr_mul (rop->data.fval,
				  op1->data.fval, op2->data.fval,
				  GMP_RNDN);
			break;
		case MPW_RATIONAL:
			mpq_mul(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_mul(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		default: break;
		}
		return;
	}

	t = MPWL_MAX_TYPE (op1, op2);
	if (rop != op1 && rop != op2 && rop->type == t) {
		rp = rop;
	} else {
		mpwl_init_type (&r, t);
		rp = &r;
	}

	switch (t) {
	case MPW_FLOAT:
		if (op1->type == MPW_FLOAT) {
			switch(op2->type) {
			case MPW_RATIONAL:
				mpfr_mul_q (rp->data.fval, op1->data.fval,
					    op2->data.rval, GMP_RNDN);
				break;
			case MPW_INTEGER:
				mpfr_mul_z (rp->data.fval, op1->data.fval,
					    op2->data.ival, GMP_RNDN);
				break;
			default: break;
			}
		} else /* op2 is MPW_FLOAT */ {
			switch(op1->type) {
			case MPW_RATIONAL:
				mpfr_mul_q (rp->data.fval, op2->data.fval,
					    op1->data.rval, GMP_RNDN);
				break;
			case MPW_INTEGER:
				mpfr_mul_z (rp->data.fval, op2->data.fval,
					    op1->data.ival, GMP_RNDN);
				break;
			default: break;
			}
		}
		break;
	case MPW_RATIONAL:
		{
			mpq_ptr op1_q, op2_q;
			mpq_t op1_tmp, op2_tmp;
			MPWL_MPQ (op1_q, op1, op1_tmp);
			MPWL_MPQ (op2_q, op2, op2_tmp);
			mpq_mul (rp->data.rval, op1_q, op2_q);
			mpwl_make_int (rp);
			MPWL_MPQ_KILL (op1_q, op1_tmp);
			MPWL_MPQ_KILL (op2_q, op2_tmp);
		}
		break;
		/*
	case MPW_INTEGER:
		mpz_mul (rp->data.ival, op1->data.ival, op2->data.ival);
		break;
		*/
	default: break;
	}

	if (rop != rp)
		mpwl_move (rop, &r);
}

static void
mpwl_mul_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i)
{
	if (rop->type != op->type) {
		mpwl_clear (rop);
		mpwl_init_type (rop, op->type);
	}
	switch(op->type) {
	case MPW_FLOAT:
		mpfr_mul_ui (rop->data.fval, op->data.fval, i, GMP_RNDN);
		break;
	case MPW_RATIONAL:
		mpz_mul_ui(mpq_numref(rop->data.rval),
			   mpq_numref(op->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		mpz_mul_ui(rop->data.ival,op->data.ival,i);
		break;
	default: break;
	}
}

static void
mpwl_div(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY (mpwl_zero_p (op2)) {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	/*special case*/
	if(op1->type > MPW_INTEGER && op1->type == op2->type) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpfr_div (rop->data.fval,
				  op1->data.fval, op2->data.fval,
				  GMP_RNDN);
			break;
		case MPW_RATIONAL:
			mpq_div(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		default: break;
		}
		return;
	}

	t = MPWL_MAX_TYPE (op1, op2);

	switch (t) {
	case MPW_FLOAT:
		mpwl_init_type (&r, t);
		{
			mpfr_ptr op1_f, op2_f;
			mpfr_t op1_tmp, op2_tmp;
			MPWL_MPF (op1_f, op1, op1_tmp);
			MPWL_MPF (op2_f, op2, op2_tmp);
			mpfr_div (r.data.fval, op1_f, op2_f, GMP_RNDN);
			MPWL_MPF_KILL (op1_f, op1_tmp);
			MPWL_MPF_KILL (op2_f, op2_tmp);
		}
		break;
	case MPW_RATIONAL:
		mpwl_init_type (&r, t);
		{
			mpq_ptr op1_q, op2_q;
			mpq_t op1_tmp, op2_tmp;
			MPWL_MPQ (op1_q, op1, op1_tmp);
			MPWL_MPQ (op2_q, op2, op2_tmp);
			mpq_div (r.data.rval, op1_q, op2_q);
			mpwl_make_int (&r);
			MPWL_MPQ_KILL (op1_q, op1_tmp);
			MPWL_MPQ_KILL (op2_q, op2_tmp);
		}
		break;
	case MPW_INTEGER:
		/* types MUST BE equal as integer is the lowest type */
		mpwl_init_type (&r, MPW_RATIONAL);
		mpz_set (mpq_numref (r.data.rval), op1->data.ival);
		mpz_set (mpq_denref (r.data.rval), op2->data.ival);
		mpwl_make_int (&r);
		break;
	default: break;
	}

	mpwl_move (rop, &r);
}

static void
mpwl_div_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i)
{
	if G_UNLIKELY (i==0) {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpfr_div_ui (rop->data.fval, op->data.fval, i,
			     GMP_RNDN);
		break;
	case MPW_RATIONAL:
		if(rop->type!=MPW_RATIONAL) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_RATIONAL);
		}
		mpz_mul_ui(mpq_denref(rop->data.rval),
			   mpq_denref(op->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		{
			MpwRealNum r = {{NULL}};
			mpwl_init_type (&r, MPW_RATIONAL);

			mpq_set_z (r.data.rval, op->data.ival);
			mpz_set_ui (mpq_denref (r.data.rval), i);
			mpwl_move (rop, &r);
			mpwl_make_int (rop);
		}
		break;
	default: break;
	}
}

static void
mpwl_ui_div(MpwRealNum *rop,unsigned long int i,MpwRealNum *op)
{
	if G_UNLIKELY (mpwl_zero_p (op)) {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpfr_ui_div (rop->data.fval, i, op->data.fval, GMP_RNDN);
		break;
	case MPW_RATIONAL:
		if(rop->type!=MPW_RATIONAL) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_RATIONAL);
		}
		mpq_inv(rop->data.rval,op->data.rval);
		mpz_mul_ui(mpq_numref(rop->data.rval),
			   mpq_numref(rop->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		{
			MpwRealNum r = {{NULL}};
			mpwl_init_type (&r, MPW_RATIONAL);
			mpz_set_ui (mpq_numref (r.data.rval), i);
			mpz_set (mpq_denref (r.data.rval), op->data.ival);
			mpwl_move (rop, &r);
			mpwl_make_int (rop);
		}
		break;
	default: break;
	}
}

static void
mpwl_mod(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_UNLIKELY (mpwl_zero_p (op2)) {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		MpwRealNum r = {{NULL}};
		mpwl_init_type (&r, MPW_INTEGER);
		mpz_mod (r.data.ival, op1->data.ival, op2->data.ival);
		mpwl_move (rop, &r);
	} else {
		gel_errorout (_("Can't do modulo of floats or rationals!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_gcd(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		MpwRealNum r = {{NULL}};
		mpwl_init_type (&r, MPW_INTEGER);
		mpz_gcd (r.data.ival, op1->data.ival, op2->data.ival);
		mpwl_move (rop, &r);
	} else {
		gel_errorout (_("Can't do GCD of floats or rationals!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

static gboolean
mpwl_invert (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		gboolean suc = FALSE;
		mpz_t ret;

		GET_INIT_MPZ (ret);

		suc = mpz_invert (ret, op1->data.ival, op2->data.ival);

		if (suc) {
			mpwl_clear (rop);
			rop->type = MPW_INTEGER;
			memcpy (rop->data.ival, ret,
				sizeof (__mpz_struct));
		} else {
			CLEAR_FREE_MPZ (ret);
		}

		return suc;
	} else {
		gel_errorout (_("Can't modulo invert non integers!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;

		return FALSE;
	}
}

static void
mpwl_jacobi(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		int ret = 0;

		ret = mpz_jacobi(op1->data.ival,op2->data.ival);

		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpwl_set_si (rop, ret);
	} else {
		gel_errorout (_("Can't get Jacobi symbols of floats or rationals!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_legendre(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		int ret = 0;

		ret = mpz_legendre(op1->data.ival,op2->data.ival);

		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpwl_set_si (rop, ret);
	} else {
		gel_errorout (_("Can't get Legendre symbols of floats or rationals!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_kronecker (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		int ret = 0;

		ret = mpz_kronecker(op1->data.ival,op2->data.ival);

		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpwl_set_si (rop, ret);
	} else {
		gel_errorout (_("Can't get Jacobi symbol with Kronecker extension of floats or rationals!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_lucnum (MpwRealNum *rop, MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER) {
		gel_errorout (_("Lucas must get an integer argument!"));
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	if G_UNLIKELY (mpz_cmp_ui(op->data.ival,G_MAXULONG)>0) {
		gel_errorout (_("Number too large to compute Lucas number!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
		gel_errorout (_("No such thing as negative Lucas numbers!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if (rop->type != MPW_INTEGER) {
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_INTEGER);
	}
	mpz_lucnum_ui (rop->data.ival, mpz_get_ui (op->data.ival));
}

static void
mpwl_nextprime (MpwRealNum *rop, MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER) {
		gel_errorout (_("Cannot get next prime after non-integer!"));
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	if (rop->type != MPW_INTEGER) {
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_INTEGER);
	}
	mpz_nextprime (rop->data.ival, op->data.ival);
}

static gboolean
mpwl_perfect_square(MpwRealNum *op)
{
	if (op->type == MPW_INTEGER) {
		return mpz_perfect_square_p (op->data.ival);
	} else if (op->type == MPW_RATIONAL) {
		return mympq_perfect_square_p (op->data.rval);
	} else {
		gel_errorout (_("%s: can't work on non-integers!"),
			      "perfect_square");
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static gboolean
mpwl_perfect_power (MpwRealNum *op)
{
	if (op->type==MPW_INTEGER) {
		return mpz_perfect_power_p(op->data.ival);
	} else {
		gel_errorout (_("%s: can't work on non-integers!"),
			      "perfect_power");
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static gboolean
mpwl_even_p (MpwRealNum *op)
{
	if(op->type == MPW_INTEGER) {
		return mpz_even_p (op->data.ival);
	} else {
		gel_errorout (_("%s: can't work on non-integers!"),
			      "even_p");
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static gboolean
mpwl_odd_p (MpwRealNum *op)
{
	if(op->type == MPW_INTEGER) {
		return mpz_odd_p (op->data.ival);
	} else {
		gel_errorout (_("%s: can't work on non-integers!"),
			      "odd_p");
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static void
mpwl_neg(MpwRealNum *rop,MpwRealNum *op)
{
	if(rop->type!=op->type) {
		mpwl_clear(rop);
		mpwl_init_type(rop,op->type);
	}

	switch(op->type) {
	case MPW_FLOAT:
		mpfr_neg (rop->data.fval, op->data.fval, GMP_RNDN);
		break;
	case MPW_RATIONAL:
		mpq_neg(rop->data.rval,op->data.rval);
		break;
	case MPW_INTEGER:
		mpz_neg(rop->data.ival,op->data.ival);
		break;
	default: break;
	}
}

static void
mpwl_fac_ui(MpwRealNum *rop,unsigned int op)
{
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	mpz_fac_ui(rop->data.ival,op);
}

static void
mpwl_fac(MpwRealNum *rop,MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER) {
		gel_errorout (_("Can't do factorials of rationals or floats!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	if G_UNLIKELY (mpz_cmp_ui(op->data.ival,G_MAXULONG)>0) {
		gel_errorout (_("Number too large to compute factorial!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
		gel_errorout (_("Can't do factorials of negative numbers!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	mpwl_fac_ui(rop,mpz_get_ui(op->data.ival));
}

static void
mpwl_dblfac_ui(MpwRealNum *rop,unsigned int op)
{
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	mpz_set_ui (rop->data.ival, 1);
	if (op == 0)
		return;
	for (;;) {
		mpz_mul_ui (rop->data.ival, rop->data.ival, op);
		if (op <= 2)
			break;
		op -= 2;
	}
}

static void
mpwl_dblfac (MpwRealNum *rop,MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER) {
		gel_errorout (_("Can't do factorials of rationals or floats!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	if G_UNLIKELY (mpz_cmp_ui(op->data.ival,G_MAXULONG)>0) {
		gel_errorout (_("Number too large to compute factorial!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
		gel_errorout (_("Can't do factorials of negative numbers!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	mpwl_dblfac_ui(rop,mpz_get_ui(op->data.ival));
}

static void
mpwl_bin_ui(MpwRealNum *rop,MpwRealNum *op, unsigned long r)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER) {
		gel_errorout (_("Can't do binomials of rationals or floats!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	mpz_bin_ui(rop->data.ival, op->data.ival, r);
}

/* returns TRUE if must make complex power */
static gboolean
mpwl_pow_q(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	mpfr_ptr op1_f;
	mpfr_t op1_tmp;
	mpfr_ptr op2_f;
	mpfr_t op2_tmp;

	MpwRealNum r = {{NULL}};
	unsigned long int den;
	int op1_sgn;


	if G_UNLIKELY (op2->type!=MPW_RATIONAL) {
		gel_error_num = GEL_INTERNAL_MPW_ERROR;
		return FALSE;
	}

	op1_sgn = mpwl_sgn (op1);

	if G_UNLIKELY (op1_sgn < 0 &&
			mpz_even_p (mpq_denref(op2->data.rval))) {
		/*it's gonna be complex*/
		return TRUE;
	}

	if (mpz_cmp_ui (mpq_denref (op2->data.rval), G_MAXULONG) <= 0 &&
	    op1->type <= MPW_RATIONAL) {
		den = mpz_get_ui (mpq_denref (op2->data.rval));
		/* We can do square root, perhaps symbolically */
		if (den == 2 || den == 4 || den == 8 || den == 16) {
			MpwRealNum n = {{NULL}};
			mpwl_init_type (&n, MPW_INTEGER);
			mpz_set (n.data.ival, mpq_numref (op2->data.rval));
			mpwl_sqrt (rop, op1);
			if (den > 2) {
				mpwl_sqrt (rop, rop);
				if (den > 4) {
					mpwl_sqrt (rop, rop);
					if (den > 8)
						mpwl_sqrt (rop, rop);
				}
			}
			mpwl_pow_z (rop, rop, &n);
			mpwl_clear (&n);

			return FALSE;
		} else if (op1->type == MPW_INTEGER) {
			mpz_t z;

			GET_INIT_MPZ (z);

			if (mpz_root (z, op1->data.ival, den) != 0) {
				mpz_ptr num = mpq_numref (op2->data.rval);
				if (mpz_sgn (num) < 0) {
					mpz_neg (num, num);
					mympz_pow_z (z, z, num);
					mpz_neg (num, num);
					mpwl_clear (rop);
					rop->type = MPW_RATIONAL;
					mpq_init (rop->data.rval);
					mpq_set_z (rop->data.rval, z);
					mpq_inv (rop->data.rval,
						 rop->data.rval);
					CLEAR_FREE_MPZ (z);
				} else {
					mympz_pow_z (z, z, num);
					mpwl_clear (rop);
					rop->type = MPW_INTEGER;
					memcpy (rop->data.ival, z,
						sizeof (__mpz_struct));
				}

				return FALSE;
			}
			CLEAR_FREE_MPZ (z);
		} else if (op1->type == MPW_RATIONAL) {
			mpq_t q;
			GET_INIT_MPQ (q);

			if (mpz_root (mpq_numref (q),
				      mpq_numref (op1->data.rval),
				      den) != 0 &&
			    mpz_root (mpq_denref (q),
				      mpq_denref (op1->data.rval),
				      den) != 0) {
				mpz_ptr num = mpq_numref (op2->data.rval);
				if (mpz_sgn (num) < 0) {
					mpz_neg (num, num);
					mympz_pow_z (mpq_numref (q),
						     mpq_numref (q),
						     num);
					mympz_pow_z (mpq_denref (q),
						     mpq_denref (q),
						     num);
					mpz_neg (num, num);
					mpq_inv (q, q);
				} else {
					mympz_pow_z (mpq_numref (q),
						     mpq_numref (q),
						     num);
					mympz_pow_z (mpq_denref (q),
						     mpq_denref (q),
						     num);
				}
				mpwl_clear (rop);
				rop->type = MPW_RATIONAL;
				memcpy (rop->data.rval, q,
					sizeof (__mpq_struct));
				// the below does mpq_canonicalize
				mpwl_make_int (rop);

				return FALSE;
			}
			CLEAR_FREE_MPQ (q);
		}
	}

	MPWL_MPF (op1_f, op1, op1_tmp);
	MPWL_MPF (op2_f, op2, op2_tmp);
	
	mpwl_init_type (&r, MPW_FLOAT);

	if (op1_sgn < 0) {
		g_assert (op2_f == op2_tmp);
		/* we know op2 denominator was odd else we wouldn't be here
		 * also we know for sure that op2_f != op1_f since 
		 * op2 was rational to begin with */
		mpfr_neg (op1_f, op1_f, GMP_RNDN);
		mpfr_pow (r.data.fval, op1_f, op2_f, GMP_RNDN);
		mpfr_neg (op1_f, op1_f, GMP_RNDN);
		if (mpz_odd_p (mpq_numref(op2->data.rval))) {
			mpfr_neg (r.data.fval, r.data.fval, GMP_RNDN);
		}
	} else {
		mpfr_pow (r.data.fval, op1_f, op2_f, GMP_RNDN);
	}

	MPWL_MPF_KILL (op1_f, op1_tmp);
	MPWL_MPF_KILL (op2_f, op2_tmp);

	if (mpfr_nan_p (r.data.fval)) {
		mpwl_clear (&r);
		return TRUE;
	}

	mpwl_move(rop,&r);

	return FALSE;
}

/*power to an unsigned long and optionally invert the answer*/
static void
mpwl_pow_ui(MpwRealNum *rop,MpwRealNum *op1,unsigned int e, gboolean reverse)
{
	MpwRealNum r = {{NULL}};

	switch(op1->type) {
	case MPW_INTEGER:
		if(!reverse) {
			mpwl_init_type(&r,MPW_INTEGER);
			mpz_pow_ui(r.data.ival,
				   op1->data.ival,e);
		} else {
			mpwl_init_type(&r,MPW_RATIONAL);
			mpz_pow_ui(mpq_denref(r.data.rval),
				   op1->data.ival,e);
			mpz_set_ui(mpq_numref(r.data.rval),1);
			mpwl_make_int(&r);
		}
		break;
	case MPW_RATIONAL:
		mpwl_init_type(&r,MPW_RATIONAL);
		mpz_pow_ui(mpq_numref(r.data.rval),
			   mpq_numref(op1->data.rval),e);
		mpz_pow_ui(mpq_denref(r.data.rval),
			   mpq_denref(op1->data.rval),e);
		/*the exponent was negative! reverse the result!*/
		if(reverse)
			mpq_inv(r.data.rval,r.data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_FLOAT:
		mpwl_init_type(&r,MPW_FLOAT);
		mpfr_pow_ui (r.data.fval, op1->data.fval, e, GMP_RNDN);

		if(reverse)
			mpfr_ui_div (r.data.fval, 1, r.data.fval, GMP_RNDN);
		break;
	default: break;
	}
	mpwl_move(rop,&r);
}

static void
mpwl_pow_z(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	gboolean reverse = FALSE;;
	if G_UNLIKELY (op2->type!=MPW_INTEGER) {
		gel_error_num = GEL_INTERNAL_MPW_ERROR;
		return;
	}
	
	reverse = FALSE;
	if(mpz_sgn(op2->data.ival)<0) {
		reverse = TRUE;
		mpz_neg(op2->data.ival,op2->data.ival);
	}

	if(mpz_cmp_ui(op2->data.ival,G_MAXULONG)>0) {
		MpwRealNum r = {{NULL}};

		switch(op1->type) {
		case MPW_INTEGER:
			if(!reverse) {
				mpwl_init_type(&r,MPW_INTEGER);
				mympz_pow_z(r.data.ival,
					    op1->data.ival,
					    op2->data.ival);
			} else {
				mpwl_init_type(&r,MPW_RATIONAL);
				mympz_pow_z(mpq_denref(r.data.rval),
					    op1->data.ival,
					    op2->data.ival);
				mpz_set_ui(mpq_numref(r.data.rval),1);
				mpwl_make_int(&r);
			}
			break;
		case MPW_RATIONAL:
			mpwl_init_type(&r,MPW_RATIONAL);
			mympz_pow_z(mpq_numref(r.data.rval),
				    mpq_numref(op1->data.rval),
				    op2->data.ival);
			mympz_pow_z(mpq_denref(r.data.rval),
				    mpq_denref(op1->data.rval),
				    op2->data.ival);
			/*the exponent was negative! reverse the result!*/
			if(reverse)
				mpq_inv(r.data.rval,r.data.rval);
			mpwl_make_int(&r);
			break;
		case MPW_FLOAT:
			mpwl_init_type(&r,MPW_FLOAT);
			mpfr_pow_z (r.data.fval, op1->data.fval,
				    op2->data.ival, GMP_RNDN);

			if(reverse)
				mpfr_ui_div (r.data.fval, 1, r.data.fval,
					     GMP_RNDN);
			break;
		default: break;
		}
		mpwl_move(rop,&r);
	} else {
		if(mpz_sgn(op2->data.ival)==0)
			mpwl_set_ui(rop,1);
		else 
			mpwl_pow_ui(rop,op1,mpz_get_ui(op2->data.ival),reverse);
	}

	if(reverse && op2 != rop)
		mpz_neg(op2->data.ival,op2->data.ival);
}

static gboolean
mpwl_pow_f(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	mpfr_ptr op1_f;
	mpfr_t op1_tmp;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY (op2->type!=MPW_FLOAT) {
		gel_error_num = GEL_INTERNAL_MPW_ERROR;
		return FALSE;
	}
	if(mpwl_sgn(op1)<=0)
		return TRUE;
	
	MPWL_MPF (op1_f, op1, op1_tmp);
	
	mpwl_init_type (&r, MPW_FLOAT);

	mpfr_pow (r.data.fval, op1_f, op2->data.fval, GMP_RNDN);

	MPWL_MPF_KILL (op1_f, op1_tmp);

	if (mpfr_nan_p (r.data.fval)) {
		mpwl_clear (&r);
		return TRUE;
	}

	mpwl_move(rop,&r);
	return FALSE;
}

static gboolean
mpwl_pow (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	int sgn1 = mpwl_sgn(op1);
	int sgn2 = mpwl_sgn(op2);
	if (sgn2 == 0) {
		mpwl_set_ui(rop,1);
		return FALSE;
	} else if (sgn1 == 0) {
		mpwl_set_ui(rop,0);
		return FALSE;
	}

	switch(op2->type) {
	case MPW_FLOAT: return mpwl_pow_f(rop,op1,op2);
	case MPW_RATIONAL: return mpwl_pow_q(rop,op1,op2);
	case MPW_INTEGER: mpwl_pow_z(rop,op1,op2); break;
	default: break;
	}
	return FALSE;
}

static void
mpwl_powm (MpwRealNum *rop,
	   MpwRealNum *op1,
	   MpwRealNum *op2,
	   MpwRealNum *mod)
{
	int sgn1, sgn2;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY ((op1->type != MPW_INTEGER) ||
		       (op2->type != MPW_INTEGER) ||
		       (mod->type != MPW_INTEGER)) {
		gel_errorout (_("%s: Bad types for mod power"),
			      "powm");
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	sgn1 = mpwl_sgn(op1);
	sgn2 = mpwl_sgn(op2);
	if (sgn2 == 0) {
		mpwl_set_ui(rop,1);
		return;
	} else if (sgn1 == 0) {
		mpwl_set_ui(rop,0);
		return;
	}

	mpwl_init_type (&r, MPW_INTEGER);

	switch(op2->type) {
	case MPW_INTEGER:
		if (sgn2 > 0) {
			mpz_powm (r.data.ival,
				  op1->data.ival,
				  op2->data.ival,
				  mod->data.ival);
		} else {
			mpz_neg (op2->data.ival, op2->data.ival);
			mpz_powm (r.data.ival,
				  op1->data.ival,
				  op2->data.ival,
				  mod->data.ival);
			mpz_neg (op2->data.ival, op2->data.ival);
			if G_UNLIKELY ( ! mpz_invert (r.data.ival,
						      r.data.ival,
						      mod->data.ival)) {
				char *n1, *n2;
				n1 = mpwl_getstring_for_error (&r);
				n2 = mpwl_getstring_for_error (mod);
				gel_errorout (_("Can't invert %s modulo %s "
					       "in %s"),
					      n1, n2, "powm");
				g_free (n1);
				g_free (n2);
				gel_error_num = GEL_NUMERICAL_MPW_ERROR;
				mpwl_clear (&r);
				return;
			}
		}
		break;
	case MPW_FLOAT: 
	case MPW_RATIONAL:
		g_assert_not_reached ();
		break;
	default: break;
	}

	mpwl_move (rop, &r);
}

static void
mpwl_powm_ui (MpwRealNum *rop,
	      MpwRealNum *op,
	      unsigned long int e,
	      MpwRealNum *mod)
{
	int sgn;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY ((op->type != MPW_INTEGER) ||
		       (mod->type != MPW_INTEGER)) {
		gel_errorout (_("%s: Bad types for mod power"),
			      "powm");
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	sgn = mpwl_sgn (op);
	if (e == 0) {
		mpwl_set_ui (rop, 1);
		return;
	} else if (sgn == 0) {
		mpwl_set_ui (rop, 0);
		return;
	}

	mpwl_init_type (&r, MPW_INTEGER);

	mpz_powm_ui (r.data.ival,
		     op->data.ival,
		     e,
		     mod->data.ival);

	mpwl_move (rop, &r);
}

static gboolean
mpwl_sqrt (MpwRealNum *rop, MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};
	gboolean is_complex = FALSE;

	if (mpwl_sgn (op) < 0) {
		is_complex = TRUE;
		mpwl_neg (op, op);
	}
	if (op->type == MPW_INTEGER &&
	    mpwl_perfect_square (op)) {
		mpwl_init_type (&r, MPW_INTEGER);
		mpz_sqrt (r.data.ival, op->data.ival);
	} else if (op->type == MPW_RATIONAL &&
		   mpwl_perfect_square (op)) {
		mpwl_init_type (&r, MPW_RATIONAL);
		mpz_sqrt (mpq_numref (r.data.rval), mpq_numref (op->data.rval));
		mpz_sqrt (mpq_denref (r.data.rval), mpq_denref (op->data.rval));
	} else {
		mpfr_ptr op_f;
		mpfr_t op_tmp;

		mpwl_init_type (&r, MPW_FLOAT);

		MPWL_MPF (op_f, op, op_tmp);
		mpfr_sqrt (r.data.fval, op_f, GMP_RNDN);
		MPWL_MPF_KILL (op_f, op_tmp);
	}
	if (is_complex)
		mpwl_neg (op, op);

	mpwl_move (rop, &r);
	return is_complex;
}

static gboolean
mpwl_ln(MpwRealNum *rop,MpwRealNum *op)
{
	mpfr_ptr op_f;
	mpfr_t op_tmp;
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	MPWL_MPF (op_f, op, op_tmp);
	if (mpfr_sgn (op_f) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op_f, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log (r.data.fval, op_f, GMP_RNDN);
		ret = TRUE;
	}
	MPWL_MPF_KILL (op_f, op_tmp);

	mpwl_move(rop,&r);
	
	return ret;
}

static gboolean
mpwl_log2(MpwRealNum *rop,MpwRealNum *op)
{
	mpfr_ptr op_f;
	mpfr_t op_tmp;
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	MPWL_MPF (op_f, op, op_tmp);
	if (mpfr_sgn (op_f) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op_f, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log2 (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log2 (r.data.fval, op_f, GMP_RNDN);
		ret = TRUE;
	}
	MPWL_MPF_KILL (op_f, op_tmp);

	mpwl_move(rop,&r);
	
	return ret;
}

static gboolean
mpwl_log10(MpwRealNum *rop,MpwRealNum *op)
{
	mpfr_ptr op_f;
	mpfr_t op_tmp;
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	MPWL_MPF (op_f, op, op_tmp);
	if (mpfr_sgn (op_f) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op_f, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log10 (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log10 (r.data.fval, op_f, GMP_RNDN);
		ret = TRUE;
	}
	MPWL_MPF_KILL (op_f, op_tmp);

	mpwl_move(rop,&r);
	
	return ret;
}

#define DEFINE_SIMPLE_MPWL_MPFR(mpwl_func,mpfr_func) \
static void							\
mpwl_func (MpwRealNum *rop,MpwRealNum *op)			\
{								\
	mpfr_ptr op_f;						\
	mpfr_t op_tmp;						\
	MPWL_MPF (op_f, op, op_tmp);				\
								\
	if (rop != op) {					\
		if (rop->type != MPW_FLOAT) {			\
			mpwl_clear(rop);			\
			mpwl_init_type(rop,MPW_FLOAT);		\
		}						\
		mpfr_func (rop->data.fval, op_f, GMP_RNDN);	\
	} else {						\
		MpwRealNum r = {{NULL}};			\
								\
		mpwl_init_type(&r,MPW_FLOAT);			\
		mpfr_func (r.data.fval, op_f, GMP_RNDN);	\
		mpwl_move(rop,&r);				\
	}							\
	MPWL_MPF_KILL (op_f, op_tmp);				\
								\
}

DEFINE_SIMPLE_MPWL_MPFR (mpwl_exp, mpfr_exp)
DEFINE_SIMPLE_MPWL_MPFR (mpwl_cos, mpfr_cos)
DEFINE_SIMPLE_MPWL_MPFR (mpwl_sin, mpfr_sin)
DEFINE_SIMPLE_MPWL_MPFR (mpwl_cosh, mpfr_cosh)
DEFINE_SIMPLE_MPWL_MPFR (mpwl_sinh, mpfr_sinh)
DEFINE_SIMPLE_MPWL_MPFR (mpwl_arctan, mpfr_atan)

static void
mpwl_arctan2 (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	mpfr_ptr op1_f, op2_f;
	mpfr_t op1_tmp, op2_tmp;
	MPWL_MPF (op1_f, op1, op1_tmp);
	MPWL_MPF (op2_f, op2, op2_tmp);

	if (rop != op1 && rop != op2) {
		if (rop->type != MPW_FLOAT) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_FLOAT);
		}
		mpfr_atan2 (rop->data.fval, op1_f, op2_f, GMP_RNDN);
	} else {
		MpwRealNum r = {{NULL}};

		mpwl_init_type (&r, MPW_FLOAT);
		mpfr_atan2 (r.data.fval, op1_f, op2_f, GMP_RNDN);
		mpwl_move (rop, &r);
	}
	MPWL_MPF_KILL (op1_f, op1_tmp);
	MPWL_MPF_KILL (op2_f, op2_tmp);
}

static void
mpwl_pi (MpwRealNum *rop)
{
	if (rop->type != MPW_FLOAT) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
	}
	mpfr_const_pi (rop->data.fval, GMP_RNDN);
}

static void
mpwl_ln2 (MpwRealNum *rop)
{
	if (rop->type != MPW_FLOAT) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
	}
	mpfr_const_log2 (rop->data.fval, GMP_RNDN);
}

static void
mpwl_euler_constant (MpwRealNum *rop)
{
	if (rop->type != MPW_FLOAT) {
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_FLOAT);
	}
	mpfr_const_euler (rop->data.fval, GMP_RNDN);
}

static void
mpwl_catalan_constant (MpwRealNum *rop)
{
	if (rop->type != MPW_FLOAT) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
	}
	mpfr_const_catalan (rop->data.fval, GMP_RNDN);
}

/* Random state stuff: FIXME: this is evil */
/* static unsigned long randstate_seed = 0; */
static gmp_randstate_t rand_state;
static gboolean rand_state_inited = FALSE;

static void
init_randstate (void)
{
	if G_UNLIKELY ( ! rand_state_inited) {
		gmp_randinit_default (rand_state);
		gmp_randseed_ui (rand_state, g_random_int ());
		rand_state_inited = TRUE;
	}
}

static void
mpwl_rand (MpwRealNum *rop)
{
	init_randstate();

	if (rop->type != MPW_FLOAT) {
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_FLOAT);
	}

	mpfr_urandomb (rop->data.fval, rand_state);
#if 0
	if G_UNLIKELY (mpfr_sgn (rop->data.fval) < 0) {
		/* FIXME: GMP/MPFR bug */
		mpfr_neg (rop->data.fval, rop->data.fval, GMP_RNDN);
		/* FIXME: WHAT THE HELL IS GOING ON! */
		if (mpfr_cmp_ui (rop->data.fval, 1L) > 0) {
			gel_errorout ("Can't recover from a GMP problem.  Random function "
				      "is not returning values in [0,1)");
		}
	}
#endif
}

static void
mpwl_randint (MpwRealNum *rop, MpwRealNum *op)
{
	long range;
	int ex;

	init_randstate();

	if G_UNLIKELY (op->type != MPW_INTEGER) {
		gel_errorout (_("Can't make random integer from a non-integer"));
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpwl_sgn (op) <= 0) {
		gel_errorout (_("Range for random integer must be positive"));
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	ex = 0;
	range = mpwl_get_long (op, &ex);
	if G_LIKELY (ex == 0 && range >= 0 && range < G_MAXINT32) {
		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpwl_set_ui (rop, g_random_int_range (0, range));
		return;
	}

	/* op must be an integer */
	if (rop->type != MPW_INTEGER) {
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_INTEGER);
	}
	mpz_urandomm (rop->data.ival, rand_state, op->data.ival);
}

static void
mpwl_make_int(MpwRealNum *rop)
{
	switch(rop->type) {
		case MPW_INTEGER:
		case MPW_FLOAT: return;
		case MPW_RATIONAL:
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				mpz_t ival;
				GET_INIT_MPZ (ival);
				mpz_set (ival, mpq_numref (rop->data.rval));
				CLEAR_FREE_MPQ (rop->data.rval);
				memcpy (rop->data.ival, ival,
					sizeof (__mpz_struct));
				rop->type = MPW_INTEGER;
			}
			break;
		default: break;
	}

}

/*make number into a float, this might be neccessary for unprecise
  calculations*/
static void
mpwl_make_float(MpwRealNum *rop)
{
	mpwl_make_type(rop,MPW_FLOAT);
}

static void
mpwl_round(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER) {
		if(rop->type == MPW_FLOAT) {
			mpz_t ival;
			GET_INIT_MPZ (ival);
			mpz_set_fr (ival, rop->data.fval, GMP_RNDN);
			CLEAR_FREE_MPF (rop->data.fval);
			memcpy (rop->data.ival, ival,
				sizeof (__mpz_struct));
			rop->type = MPW_INTEGER;
		} else /*MPW_RATIONAL*/ {
			mpq_t tmp;
			GET_INIT_MPQ (tmp);
			mpq_set_ui(tmp,1,2);
			if(mpq_sgn(rop->data.rval)<0)
				mpq_sub(rop->data.rval,rop->data.rval,tmp);
			else
				mpq_add(rop->data.rval,rop->data.rval,tmp);
			CLEAR_FREE_MPQ (tmp);
			if (mpz_cmp_ui (mpq_denref (rop->data.rval), 1) == 0) {
				if (mpq_sgn (rop->data.rval) > 0) {
					mpwl_make_type(rop,MPW_INTEGER);
					mpz_sub_ui (rop->data.ival,
						    rop->data.ival, 1);
				} else {
					mpwl_make_type(rop,MPW_INTEGER);
					mpz_add_ui (rop->data.ival,
						    rop->data.ival, 1);
				}
			} else {
				mpwl_make_type(rop,MPW_INTEGER);
			}
		}
	}
}

static void
mpwl_ceil(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER) {
		if(rop->type == MPW_FLOAT) {
			mpz_t ival;
			GET_INIT_MPZ (ival);
			mpz_set_fr (ival, rop->data.fval, GMP_RNDU);
			CLEAR_FREE_MPF (rop->data.fval);
			memcpy (rop->data.ival, ival,
				sizeof (__mpz_struct));
			rop->type = MPW_INTEGER;
		} else /*MPW_RATIONAL*/ {
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				mpz_t ival;
				GET_INIT_MPZ (ival);
				mpz_set (ival,
					 mpq_numref (rop->data.rval));
				CLEAR_FREE_MPQ (rop->data.rval);
				memcpy (rop->data.ival, ival,
					sizeof (__mpz_struct));
				rop->type = MPW_INTEGER;
			} else {
				if(mpq_sgn(rop->data.rval)>0) {
					mpwl_make_type(rop,MPW_INTEGER);
					mpz_add_ui(rop->data.ival,
						   rop->data.ival,1);
				} else 
					mpwl_make_type(rop,MPW_INTEGER);
			}
		}
	}
}

static void
mpwl_floor(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER) {
		if(rop->type == MPW_FLOAT) {
			mpz_t ival;
			GET_INIT_MPZ (ival);
			mpz_set_fr (ival, rop->data.fval, GMP_RNDD);
			CLEAR_FREE_MPF (rop->data.fval);
			memcpy (rop->data.ival, ival,
				sizeof (__mpz_struct));
			rop->type = MPW_INTEGER;
		} else /*MPW_RATIONAL*/ {
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				mpz_t ival;
				GET_INIT_MPZ (ival);
				mpz_set (ival,
					 mpq_numref (rop->data.rval));
				CLEAR_FREE_MPQ (rop->data.rval);
				memcpy (rop->data.ival, ival,
					sizeof (__mpz_struct));
				rop->type = MPW_INTEGER;
			} else {
				if(mpq_sgn(rop->data.rval)<0) {
					mpwl_make_type(rop,MPW_INTEGER);
					mpz_sub_ui(rop->data.ival,
						   rop->data.ival,1);
				} else 
					mpwl_make_type(rop,MPW_INTEGER);
			}
		}
	}
}

static void
mpwl_trunc(MpwRealNum *rop)
{
	if (rop->type == MPW_RATIONAL) {
		mpwl_make_type(rop,MPW_INTEGER);
	} else if (rop->type == MPW_FLOAT) {
		mpz_t ival;
		GET_INIT_MPZ (ival);
		mpz_set_fr (ival, rop->data.fval, GMP_RNDZ);
		CLEAR_FREE_MPF (rop->data.fval);
		memcpy (rop->data.ival, ival,
			sizeof (__mpz_struct));
		rop->type = MPW_INTEGER;
	}
}

static void
mpwl_numerator(MpwRealNum *rop, MpwRealNum *op)
{
	if (op->type == MPW_INTEGER) {
		if(rop != op)
			mpwl_set(rop, op);
	} else if G_UNLIKELY (op->type == MPW_FLOAT) {
		gel_errorout (_("Can't get numerator of floating types"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	} else { /* must be rational */
		if(rop != op) {
			if G_UNLIKELY (rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop, MPW_INTEGER);
			}
			mpz_set(rop->data.ival, mpq_numref(op->data.rval));
		} else {
			mpz_t ival;
			GET_INIT_MPZ (ival);
			mpz_set (ival,
				 mpq_numref (rop->data.rval));
			CLEAR_FREE_MPQ (rop->data.rval);
			memcpy (rop->data.ival, ival,
				sizeof (__mpz_struct));
			rop->type = MPW_INTEGER;
		}
	}
}

static void
mpwl_denominator(MpwRealNum *rop, MpwRealNum *op)
{
	if (op->type == MPW_INTEGER) {
		mpwl_set_si(rop, 1);
	} else if G_UNLIKELY (op->type == MPW_FLOAT) {
		gel_errorout (_("Can't get numerator of floating types"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	} else { /* must be rational */
		if(rop != op) {
			if G_UNLIKELY (rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop, MPW_INTEGER);
			}
			mpz_set(rop->data.ival, mpq_denref(op->data.rval));
		} else {
			mpz_t ival;
			GET_INIT_MPZ (ival);
			mpz_set (ival,
				 mpq_denref (rop->data.rval));
			CLEAR_FREE_MPQ (rop->data.rval);
			memcpy (rop->data.ival, ival,
				sizeof (__mpz_struct));
			rop->type = MPW_INTEGER;
		}
	}
}

static void
mpwl_set_str_float(MpwRealNum *rop,const char *s,int base)
{
	char *old_locale = setlocale (LC_NUMERIC, NULL);
	if (old_locale != NULL &&
	    strcmp (old_locale, "C") != 0) {
		old_locale = g_strdup (old_locale);
		setlocale (LC_NUMERIC, "C");
	} else {
		old_locale = NULL;
	}

	if(rop->type!=MPW_FLOAT) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
	}
	mpfr_set_str (rop->data.fval, s, base, GMP_RNDN);

	if (old_locale != NULL) {
		setlocale (LC_NUMERIC, old_locale);
		g_free (old_locale);
	}
}

static void
mpwl_set_str_int(MpwRealNum *rop,const char *s,int base)
{
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	if G_UNLIKELY (mpz_set_str(rop->data.ival,s,base) < 0) {
		gel_errorout (_("Invalid integer format"));
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
	}
}


/**************/
/*output stuff*/

/*get a long if possible*/
static long
mpwl_get_long (MpwRealNum *op, int *ex)
{
	if G_UNLIKELY (op->type > MPW_INTEGER) {
		*ex = MPWL_EXCEPTION_CONVERSION_ERROR;
		return 0;
	} else { /*real integer*/
		if G_UNLIKELY ( ! mpz_fits_slong_p (op->data.ival)) {
			*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
			return 0;
		} else {
			return mpz_get_si(op->data.ival);
		}
	}
}

/*get an unsigned long if possible*/
static unsigned long
mpwl_get_ulong (MpwRealNum *op, int *ex)
{
	if G_UNLIKELY (op->type > MPW_INTEGER) {
		*ex = MPWL_EXCEPTION_CONVERSION_ERROR;
		return 0;
	} else { /*real integer*/
		if G_UNLIKELY ( ! mpz_fits_ulong_p (op->data.ival)) {
			*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
			return 0;
		} else {
			return mpz_get_ui(op->data.ival);
		}
	}

}

/*get a double if possible*/
static double
mpwl_get_double (MpwRealNum *op, int *ex)
{
	double d = 0.0;

	switch (op->type) {
	case MPW_RATIONAL:
		d = mpq_get_d (op->data.rval);
		break;
	case MPW_INTEGER:
		d = mpz_get_d (op->data.ival);
		break;
	case MPW_FLOAT:
		/*if G_UNLIKELY (mpfr_cmp_d (op->data.fval, G_MAXDOUBLE) > 0 ||
			       mpfr_cmp_d (op->data.fval, -G_MAXDOUBLE) < 0) {
			*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
			return 0;
		}*/
		d = mpfr_get_d (op->data.fval, GMP_RNDN);
		break;
	default: break;
	}
/* What if this is not defined? */
#ifdef isfinite
	if G_UNLIKELY ( ! isfinite (d)) {
		*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
		return 0;
	}
#endif

	return d;
}


/*trim trailing zeros*/
static void
str_trim_trailing_zeros (char *s, gboolean leave_first_zero)
{
	char *p, *pp;

	p = strrchr(s,'.');
	if(!p) return;

	if (leave_first_zero &&
	    *(p+1) == '0') {
		p = p+2;
		pp = p;
	} else {
		pp=p+1;
	}
	for (; *pp != '\0'; pp++) {
		if (*pp != '0')
			p = pp+1;
	}
	*p = '\0';
}

/*formats a floating point with mantissa in p and exponent in e*/
static char *
str_format_float (char *p,
		  long int e,
		  int max_digits,
		  gboolean scientific_notation)
{
	long int len;
	int i;
	int max_exp;
	int min_exp;
	int trailzeros;

	len = strlen (p);
	for (trailzeros = 0; trailzeros < len; trailzeros++)
		if (p[len-trailzeros-1] != '0')
			break;

	if (max_digits > 1) {
		/* Negative 2 is there to ensure that a number is never
		 * printed as an integer when it is a float that was rounded
		 * off */
		max_exp = max_digits-2;
		min_exp = MIN ((len-trailzeros) - max_digits + 2, -2);
	} else {
		max_exp = MAX ((len-trailzeros)-2, 10);
		min_exp = -10;
	}

	if(((e-1)<min_exp || (e-1)>max_exp) || scientific_notation) {
		if (e != 0)
			p = g_realloc (p, len+2+((int)log10(e > 0 ? e : -e)+2)+1);
		else
			p = g_realloc (p, len + 4);
			
		if(p[0]=='-') {
			if (len > 2) {
				gel_shiftstr(p+2,1);
				p[2]='.';
			}
		} else {
			if (len > 1) {
				gel_shiftstr(p+1,1);
				p[1]='.';
			}
		}
		str_trim_trailing_zeros (p, FALSE /* not_first_zero */);
		len = strlen(p);
		/* if we actually just have zero */
		if (strcmp (p, "0") == 0) {
			strcpy (p, "0e0");
		} else {
			/* look above to see why this is one sprintf which is in
			   fact safe */
			sprintf(p+len,"e%ld",e-1);
		}
	} else if(e>0) {
		if(p[0]=='-')
			len--;
		if(e>len) {
			p = g_realloc (p, strlen(p)+2+e-len);
			for(i=0;i<e-len;i++)
				strcat(p,"0");
		} else if(e<len) {
			p = g_realloc (p, strlen(p) + 2);
			if(p[0]=='-') {
				gel_shiftstr(p+1+e,1);
				p[e+1]='.';
			} else {
				gel_shiftstr(p+e,1);
				p[e]='.';
			}
		}
		str_trim_trailing_zeros (p, TRUE /* leave_first_zero */);
	} else { /*e<=0*/
		if(len == 0) {
			g_free (p);
			p = g_strdup ("0.0");
		} else {
			p = g_realloc (p, strlen(p)+2+(-e)+2);
			if(p[0]=='-') {
				gel_shiftstr(p+1,2+(-e));
				p[1]='0';
				p[2]='.';
				for(i=0;i<(-e);i++)
					p[i+3]='0';
			} else {
				gel_shiftstr(p,2+(-e));
				p[0]='0';
				p[1]='.';
				for(i=0;i<(-e);i++)
					p[i+2]='0';
			}
		}
		str_trim_trailing_zeros (p, TRUE /* leave_first_zero */);
	}
	return p;
}

static char *
str_getstring_z (mpz_ptr num, int max_digits,int scientific_notation,
		int integer_output_base, const char *postfix)
{
	char *p,*p2;
	mpfr_t fr;

	p=mpz_get_str(NULL,integer_output_base,num);
	if(integer_output_base==16) {
		p2 = g_strconcat("0x",p,NULL);
		g_free(p);
		p = p2;
	} else if(integer_output_base==8) {
		p2 = g_strconcat("0",p,NULL);
		g_free(p);
		p = p2;
	} else if(integer_output_base!=10) {
		p2 = g_strdup_printf("%d\\%s",integer_output_base,p);
		g_free(p);
		p = p2;
	}
	if(max_digits > 0 && max_digits < (int)strlen(p)) {
		mpfr_init(fr);
		mpfr_set_z(fr,num, GMP_RNDN);
		p2=str_getstring_f(fr,max_digits,scientific_notation,postfix,
				   -1 /* chop */);
		mpfr_clear(fr);
		if(strlen(p2)>=strlen(p)) {
			g_free(p2);
			return p;
		} else  {
			g_free(p);
			return p2;
		}
	}
	p=gel_appendstr(p,postfix);
	return p;
}

static char *
get_frac (mpz_t num,
	  mpz_t den,
	  GelOutputStyle style,
	  const char *postfix,
	  int *dig)
{
	char *p, *p2;
	int digits = 0;

	if (style == GEL_OUTPUT_LATEX) {
		int l;
		p=mpz_get_str(NULL,10,num);
		digits = strlen(p);
		p=gel_prependstr(p,"\\frac{");
		p=gel_appendstr(p,"}{");
		p2=mpz_get_str(NULL,10,den);
		p=gel_appendstr(p,p2);
		l = strlen (p2);
		if (l > digits)
			digits = l;
		g_free(p2);
		p=gel_appendstr(p,"}");
		p=gel_appendstr(p,postfix);
	} else if (style == GEL_OUTPUT_TROFF) {
		int l;
		p=mpz_get_str(NULL,10,num);
		digits = strlen(p);
		p=gel_prependstr(p,"{");
		p=gel_appendstr(p,"} over {");
		p2=mpz_get_str(NULL,10,den);
		p=gel_appendstr(p,p2);
		l = strlen (p2);
		if (l > digits)
			digits = l;
		g_free(p2);
		p=gel_appendstr(p,"}");
		p=gel_appendstr(p,postfix);
	} else {
		p=mpz_get_str(NULL,10,num);
		p=gel_appendstr(p,postfix);
		p=gel_appendstr(p,"/");
		p2=mpz_get_str(NULL,10,den);
		p=gel_appendstr(p,p2);
		g_free(p2);
		digits = strlen(p) - 1; /* don't count the / */
		digits -= strlen (postfix); /* don't count the i */
	}
	
	*dig += digits;

	return p;
}	

static char *
str_getstring_q (mpq_ptr num,
		 int max_digits,
		 gboolean scientific_notation,
		 gboolean mixed_fractions,
		 GelOutputStyle style,
		 const char *postfix,
		 int float_chop)
{
	char *p,*p2;
	mpfr_t fr;
	int digits = 0;
	
	if(mixed_fractions) {
		if(mpq_sgn(num)>0) {
			if(mpz_cmp(mpq_numref(num),mpq_denref(num))<0)
				mixed_fractions = FALSE;
		} else {
			mpz_neg(mpq_numref(num),mpq_numref(num));
			if(mpz_cmp(mpq_numref(num),mpq_denref(num))<0)
				mixed_fractions = FALSE;
			mpz_neg(mpq_numref(num),mpq_numref(num));
		}
	}

	if(!mixed_fractions) {
		p = get_frac (mpq_numref (num), mpq_denref (num),
			      style, postfix, &digits);
	} else {
		int d;
		mpz_t tmp1, tmp2;
		GET_INIT_MPZ (tmp1);
		GET_INIT_MPZ (tmp2);
		mpz_tdiv_qr(tmp1,tmp2,mpq_numref(num),mpq_denref(num));
		if(mpz_sgn(tmp2)<0)
			mpz_neg(tmp2,tmp2);
		p=mpz_get_str(NULL,10,tmp1);
		digits = strlen (p);

		if (postfix != NULL &&
		    *postfix != '\0') {
			if (style == GEL_OUTPUT_LATEX)
				p = gel_prependstr (p, "\\left(");
			else if (style == GEL_OUTPUT_TROFF)
				p = gel_prependstr (p, " left ( ");
			else
				p = gel_prependstr (p, "(");
		}

		p=gel_appendstr(p," ");

		p2 = get_frac (tmp2, mpq_denref (num),
			       style, "", &d);
		p=gel_appendstr(p,p2);
		g_free(p2);

		if (postfix != NULL &&
		    *postfix != '\0') {
			if (style == GEL_OUTPUT_LATEX)
				p = gel_appendstr (p, "\\right)");
			else if (style == GEL_OUTPUT_TROFF)
				p = gel_appendstr (p, " right )~");
			else
				p = gel_appendstr (p, ")");
			p = gel_appendstr (p, postfix);
		}

		CLEAR_FREE_MPZ (tmp1);
		CLEAR_FREE_MPZ (tmp2);
	}
	if (max_digits > 0 && max_digits < digits) {
		mpfr_init (fr);
		mpfr_set_q (fr, num, GMP_RNDN);
		p2=str_getstring_f(fr,max_digits,scientific_notation,
				   postfix,
				   float_chop);
		mpfr_clear(fr);
		if(strlen(p2)>=strlen(p)) {
			g_free(p2);
			return p;
		} else  {
			g_free(p);
			return p2;
		}
	}
	return p;
}

static char *
str_getstring_f (mpfr_ptr num,
		 int max_digits,
		 gboolean scientific_notation,
		 const char *postfix,
		 int chop)
{
	char *p;
	long e;

	if (chop > 0) {
		/* approximately the exponent base 10 */
		e = mpfr_get_exp (num) / 3.32192809489;
		if (e < -chop) {
			const char *sign = "";
			if (mpfr_sgn (num) < 0)
				sign = "-";
			if (scientific_notation)
				return g_strconcat (sign, "0e0", postfix, NULL);
			else
				return g_strconcat (sign, "0.0", postfix, NULL);
		}
	}

	if (max_digits > 1) {
		p = g_new(char, MAX (max_digits + 2, 7));

		mpfr_get_str (p, &e, 10, max_digits, num, GMP_RNDN);
	} else {
		char *mp;
		mp = mpfr_get_str (NULL, &e, 10, 0, num, GMP_RNDN);
		p = g_strdup (mp);
		mpfr_free_str (mp);
	}
	p = str_format_float (p, e, max_digits, scientific_notation);
	p = gel_appendstr (p, postfix);

	return p;
}

static char *
mpwl_getstring(MpwRealNum * num, int max_digits,
	       gboolean scientific_notation,
	       gboolean results_as_floats,
	       gboolean mixed_fractions,
	       GelOutputStyle style,
	       int integer_output_base,
	       const char *postfix,
	       int chop)
{
	mpfr_t fr;
	char *p;
	switch(num->type) {
	case MPW_RATIONAL:
		if(results_as_floats) {
			mpfr_init (fr);
			mpfr_set_q (fr, num->data.rval, GMP_RNDN);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation, postfix,
					  chop);
			mpfr_clear(fr);
			return p;
		}
		return str_getstring_q(num->data.rval,
				       max_digits,
				       scientific_notation,
				       mixed_fractions,
				       style,
				       postfix,
				       chop);
	case MPW_INTEGER:
		if(results_as_floats) {
			mpfr_init_set_z (fr, num->data.ival, GMP_RNDN);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation,
					  postfix,
					  -1 /* never chop an integer */);
			mpfr_clear(fr);
			return p;
		}
		return str_getstring_z(num->data.ival,max_digits,
				       scientific_notation,
				       integer_output_base,
				       postfix);
	case MPW_FLOAT:
		return str_getstring_f(num->data.fval,max_digits,
				       scientific_notation,
				       postfix,
				       chop);
	default: break;
	}
	/*something bad happened*/
	return NULL;
}

#define mpw_uncomplex(rop)					\
{								\
	if ((rop)->i != gel_zero &&				\
	    mpwl_zero_p ((rop)->i)) {				\
		(rop)->i->alloc.usage--;			\
		if ((rop)->i->alloc.usage==0)			\
			mpwl_free ((rop)->i);			\
		(rop)->i = gel_zero;				\
		gel_zero->alloc.usage ++;			\
	}							\
}

/*************************************************************************/
/*high level stuff                                                       */
/*************************************************************************/

/*set default precision*/
void
mpw_set_default_prec (unsigned long int prec)
{
	__mpfr_struct *p;

	mpfr_set_default_prec (prec);

	/* whack the mpf cache */
	for (p = free_mpfr; p != free_mpfr_top; p++) {
		mpfr_clear (p);
	}
	free_mpfr_top = free_mpfr;

	default_mpfr_prec = prec;
}

/*initialize a number*/
#undef mpw_init
void
mpw_init (mpw_ptr op)
{
	mpw_init_inline(op);
}
#define mpw_init(op) mpw_init_inline(op)

void
mpw_init_set(mpw_ptr rop, mpw_ptr op)
{
	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
	mpw_uncomplex (rop);
}

#undef mpw_init_set_no_uncomplex

/*clear memory held by number*/
void
mpw_clear(mpw_ptr op)
{
	op->r->alloc.usage--;
	op->i->alloc.usage--;
	if(op->r->alloc.usage==0)
		mpwl_free (op->r);
	if(op->i->alloc.usage==0)
		mpwl_free (op->i);
}

/*make them the same type without loosing information*/
void
mpw_make_same_type (mpw_ptr op1, mpw_ptr op2)
{
	if (op1->r->type != op2->r->type) {
		MAKE_COPY (op1->r);
		MAKE_COPY (op2->r);
		mpwl_make_same_type (op1->r, op2->r);
	}
	if ((MPW_IS_COMPLEX (op1) || MPW_IS_COMPLEX (op2)) &&
	    (op1->i->type != op2->i->type)) {
		MAKE_COPY (op1->i);
		MAKE_COPY (op2->i);
		mpwl_make_same_type (op1->i, op2->i);
	}
}

void
mpw_set(mpw_ptr rop,mpw_ptr op)
{
	if (rop == op)
		return;

	mpw_clear (rop);

	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
	/* it shouldn't need uncomplexing*/
	/* mpw_uncomplex(rop); */
}

void
mpw_set_d(mpw_ptr rop,double d)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_set_d (rop->r, d);
}

void
mpw_set_d_complex (mpw_ptr rop, double real, double imag)
{
	if (imag == 0.0) {
		MAKE_REAL (rop);
		MAKE_EMPTY (rop->r, MPW_FLOAT);
		mpwl_set_d (rop->r, real);
	} else {
		MAKE_EMPTY (rop->r, MPW_FLOAT);
		MAKE_EMPTY (rop->i, MPW_FLOAT);
		mpwl_set_d (rop->r, real);
		mpwl_set_d (rop->i, imag);
	}
}

void
mpw_set_si(mpw_ptr rop,signed long int i)
{
	MAKE_REAL(rop);
	if(i==0) {
		if(rop->r != gel_zero) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free (rop->r);
			rop->r = gel_zero;
			gel_zero->alloc.usage++;
		}
	} else if(i==1) {
		if(rop->r != gel_one) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free (rop->r);
			rop->r = gel_one;
			gel_one->alloc.usage++;
		}
	} else {
		MAKE_EMPTY(rop->r, MPW_INTEGER);
		mpwl_set_si(rop->r,i);
	}
}

void
mpw_set_ui(mpw_ptr rop,unsigned long int i)
{
	MAKE_REAL(rop);
	if(i==0) {
		if(rop->r != gel_zero) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free (rop->r);
			rop->r = gel_zero;
			gel_zero->alloc.usage++;
		}
	} else if(i==1) {
		if(rop->r != gel_one) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free (rop->r);
			rop->r = gel_one;
			gel_one->alloc.usage++;
		}
	} else {
		MAKE_EMPTY(rop->r, MPW_INTEGER);
		mpwl_set_ui(rop->r,i);
	}
}

void
mpw_set_mpz_use (mpw_ptr rop, mpz_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_INTEGER;
	rop->r->alloc.usage = 1;
	memcpy (rop->r->data.ival, op, sizeof (__mpz_struct));
}

void
mpw_set_mpq_use (mpw_ptr rop, mpq_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_RATIONAL;
	rop->r->alloc.usage = 1;
	memcpy (rop->r->data.rval, op, sizeof (__mpq_struct));
}

void
mpw_set_mpf_use (mpw_ptr rop, mpfr_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_FLOAT;
	rop->r->alloc.usage = 1;
	memcpy (rop->r->data.fval, op, sizeof (__mpfr_struct));
}

void
mpw_make_copy (mpw_ptr op)
{
	MAKE_COPY (op->r);
	MAKE_COPY (op->i);
}

void
mpw_make_copy_real (mpw_ptr op)
{
	MAKE_COPY (op->r);
}

void
mpw_make_copy_imag (mpw_ptr op)
{
	MAKE_COPY (op->r);
	MAKE_COPY (op->i);
}

mpz_ptr
mpw_peek_real_mpz (mpw_ptr op)
{
	if (op->r->type == MPW_INTEGER)
		return op->r->data.ival;
	else
		return NULL;
}

mpq_ptr
mpw_peek_real_mpq (mpw_ptr op)
{
	if (op->r->type == MPW_RATIONAL)
		return op->r->data.rval;
	else
		return NULL;
}

mpfr_ptr
mpw_peek_real_mpf (mpw_ptr op)
{
	if (op->r->type == MPW_FLOAT)
		return op->r->data.fval;
	else
		return NULL;
}

mpz_ptr
mpw_peek_imag_mpz (mpw_ptr op)
{
	if (MPW_IS_COMPLEX (op) &&
	    op->i->type == MPW_INTEGER)
		return op->i->data.ival;
	else
		return NULL;
}

mpq_ptr
mpw_peek_imag_mpq (mpw_ptr op)
{
	if (MPW_IS_COMPLEX (op) &&
	    op->i->type == MPW_RATIONAL)
		return op->i->data.rval;
	else
		return NULL;
}

mpfr_ptr
mpw_peek_imag_mpf (mpw_ptr op)
{
	if (MPW_IS_COMPLEX (op) &&
	    op->i->type == MPW_FLOAT)
		return op->i->data.fval;
	else
		return NULL;
}

int
mpw_sgn(mpw_ptr op) 
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		return mpwl_sgn(op->r);
	} else {
		gel_errorout (_("Can't compare complex numbers"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
	return 0;
}

int
mpw_re_sgn(mpw_ptr op) /* PURE */
{
	return mpwl_sgn(op->r);
}

int
mpw_im_sgn(mpw_ptr op) /* PURE */
{
	return mpwl_sgn(op->i);
}

void
mpw_abs(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		if(mpwl_sgn(op->r)<0)
			mpw_neg(rop,op);
		else
			mpw_set(rop,op);
	} else {
		MpwRealNum t = {{NULL}};

		if (rop != op) {
			MAKE_EMPTY(rop->r, op->r->type);
		} else {
			MAKE_COPY (rop->r);
		}
		
		mpwl_init_type (&t, op->i->type);
		
		mpwl_mul(rop->r,op->r,op->r);
		mpwl_mul(&t,op->i,op->i);
		mpwl_add(rop->r,rop->r,&t);
		
		mpwl_clear(&t);

		mpwl_sqrt(rop->r,rop->r);

		MAKE_REAL (rop);
	}
}

void
mpw_abs_sq (mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		/* have to actually square now */
		mpw_mul (rop, op, op);
	} else {
		MpwRealNum t = {{NULL}};

		if (rop != op) {
			MAKE_EMPTY(rop->r, op->r->type);
		} else {
			MAKE_COPY (rop->r);
		}
		
		mpwl_init_type (&t, op->i->type);
		
		mpwl_mul(rop->r,op->r,op->r);
		mpwl_mul(&t,op->i,op->i);
		mpwl_add(rop->r,rop->r,&t);

		mpwl_clear(&t);

		MAKE_REAL (rop);
	}
}

void
mpw_neg(mpw_ptr rop,mpw_ptr op)
{
	if (rop != op) {
		MAKE_EMPTY(rop->r, op->r->type);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_neg(rop->r,op->r);
	if (MPW_IS_REAL (op)) {
		MAKE_REAL (rop);
	} else {
		if (rop != op) {
			MAKE_EMPTY(rop->i, op->i->type);
		} else {
			MAKE_COPY (rop->i);
		}
		mpwl_neg (rop->i, op->i);
	}
}

void
mpw_add(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if (rop != op1 && rop != op2) {
		MAKE_EMPTY (rop->r, MAX (op1->r->type, op2->r->type));
	} else {
		MAKE_COPY(rop->r);
	}
	mpwl_add(rop->r,op1->r,op2->r);
	if (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->i, MAX (op1->i->type, op2->i->type));
		} else {
			MAKE_COPY (rop->i);
		}
		mpwl_add(rop->i,op1->i,op2->i);

		mpw_uncomplex(rop);
	}
}

void
mpw_add_ui(mpw_ptr rop,mpw_ptr op, unsigned long i)
{
	if (rop != op) {
		MAKE_EMPTY(rop->r, op->r->type);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_add_ui(rop->r,op->r,i);

	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op) {
			DEALLOC_MPWL (rop->i);
			rop->i = op->i;
			ALLOC_MPWL (rop->i);
		}

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_sub(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if (rop != op1 && rop != op2) {
		MAKE_EMPTY (rop->r, MAX (op1->r->type, op2->r->type));
	} else {
		MAKE_COPY(rop->r);
	}
	mpwl_sub(rop->r,op1->r,op2->r);
	if (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->i, MAX (op1->i->type, op2->i->type));
		} else {
			MAKE_COPY (rop->i);
		}
		mpwl_sub(rop->i,op1->i,op2->i);

		mpw_uncomplex(rop);
	}
}

void
mpw_sub_ui(mpw_ptr rop,mpw_ptr op, unsigned long i)
{
	if (rop != op) {
		MAKE_EMPTY(rop->r, op->r->type);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_sub_ui(rop->r,op->r,i);

	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op) {
			DEALLOC_MPWL (rop->i);
			rop->i = op->i;
			ALLOC_MPWL (rop->i);
		}

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_ui_sub(mpw_ptr rop,unsigned long i, mpw_ptr op)
{
	if (rop != op) {
		MAKE_EMPTY (rop->r, op->r->type);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_ui_sub(rop->r,i,op->r);
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op) {
			MAKE_EMPTY (rop->i, op->i->type);
		} else {
			MAKE_COPY (rop->i);
		}
		mpwl_neg(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_mul(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MAX (op1->r->type, op2->r->type));
		} else {
			MAKE_COPY(rop->r);
		}
		mpwl_mul(rop->r,op1->r,op2->r);
	} else {
		MpwRealNum tr = {{NULL}};
		MpwRealNum ti = {{NULL}};
		MpwRealNum *r1;
		MpwRealNum *i1;
		MpwRealNum *r2;
		MpwRealNum *i2;
		
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MAX (op1->r->type, op2->r->type));
			MAKE_EMPTY (rop->i, MAX (op1->i->type, op2->r->type));
		} else {
			MAKE_COPY(rop->r);
			MAKE_COPY(rop->i);
		}

		MAKE_CPLX_OPS(op1,r1,i1);
		MAKE_CPLX_OPS(op2,r2,i2);

		mpwl_mul(rop->r,r1,r2);
		mpwl_mul(rop->i,i1,r2);

		mpwl_init_type(&tr,i1->type);
		mpwl_init_type(&ti,r1->type);

		/* tmp is complex; */
		mpwl_mul(&tr,i1,i2);
		mpwl_neg(&tr,&tr);
		mpwl_mul(&ti,r1,i2);
		
		mpwl_add(rop->r,rop->r,&tr);
		mpwl_add(rop->i,rop->i,&ti);

		mpwl_clear(&tr);
		mpwl_clear(&ti);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op1,r1,i1);
		BREAK_CPLX_OPS(op2,r2,i2);
	}
}

void
mpw_mul_ui(mpw_ptr rop,mpw_ptr op, unsigned int i)
{
	if (rop != op) {
		MAKE_EMPTY(rop->r, op->r->type);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_mul_ui(rop->r,op->r,i);

	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op) {
			MAKE_EMPTY(rop->i, op->i->type);
		} else {
			MAKE_COPY (rop->i);
		}
		mpwl_mul_ui(rop->i,op->i,i);

		mpw_uncomplex(rop);
	}
}

void
mpw_div(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		if G_UNLIKELY (mpwl_zero_p (op2->r)) {
			gel_errorout (_("Division by zero!"));
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MAX (op1->r->type, op2->r->type));
		} else {
			MAKE_COPY(rop->r);
		}
		mpwl_div(rop->r,op1->r,op2->r);
	} else {
		MpwRealNum t1 = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		MpwRealNum *r1;
		MpwRealNum *i1;
		MpwRealNum *r2;
		MpwRealNum *i2;
		if G_UNLIKELY (mpwl_zero_p (op2->r) && mpwl_zero_p (op2->i)) {
			gel_errorout (_("Division by zero!"));
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}

		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MAX (op1->r->type, op2->r->type));
			MAKE_EMPTY (rop->i,
				    MAX (MAX (op1->i->type, op2->i->type),
					 rop->r->type));
		} else {
			MAKE_COPY(rop->r);
			MAKE_COPY(rop->i);
		}

		MAKE_CPLX_OPS(op1,r1,i1);
		MAKE_CPLX_OPS(op2,r2,i2);

		mpwl_init_type(&t1,MPW_INTEGER);
		mpwl_init_type(&t2,MPW_INTEGER);

		/*real part (r1r2 + i1i2)/(r2r2 + i2i2)*/
		mpwl_mul(rop->r,r1,r2);
		mpwl_mul(&t1,i1,i2);
		mpwl_add(rop->r,rop->r,&t1);

		mpwl_mul(&t1,r2,r2);
		mpwl_mul(&t2,i2,i2);
		mpwl_add(&t2,&t2,&t1);

		mpwl_div(rop->r,rop->r,&t2);


		/*imaginary part (i1r2 - r1i2)/(r2r2 + i2i2)*/
		mpwl_mul(rop->i,i1,r2);
		mpwl_mul(&t1,r1,i2);
		mpwl_neg(&t1,&t1);
		mpwl_add(rop->i,rop->i,&t1);

		/*t2 is calculated above*/

		mpwl_div(rop->i,rop->i,&t2);

		mpwl_clear(&t1);
		mpwl_clear(&t2);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op1,r1,i1);
		BREAK_CPLX_OPS(op2,r2,i2);
	}
}

void
mpw_div_ui(mpw_ptr rop,mpw_ptr op, unsigned int i)
{
	if G_UNLIKELY (i==0) {
		gel_errorout (_("Division by zero!"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	if (rop != op) {
		MAKE_EMPTY(rop->r, op->r->type);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_div_ui(rop->r,op->r,i);
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
	} else {
		if (rop != op) {
			MAKE_EMPTY(rop->i, op->i->type);
		} else {
			MAKE_COPY (rop->i);
		}
		mpwl_div_ui(rop->i,op->i,i);

		mpw_uncomplex(rop);
	}
}

void
mpw_ui_div(mpw_ptr rop,unsigned int in,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		if G_UNLIKELY (mpwl_zero_p (op->r)) {
			gel_errorout (_("Division by zero!"));
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY(rop->r, op->r->type);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_ui_div(rop->r,in,op->r);
	} else {
		MpwRealNum t1 = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		if G_UNLIKELY (mpwl_zero_p (op->r) && mpwl_zero_p (op->i)) {
			gel_errorout (_("Division by zero!"));
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}

		if (rop != op) {
			MAKE_EMPTY (rop->r, op->r->type);
			MAKE_EMPTY (rop->i, op->i->type);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t1,r->type);
		mpwl_init_type(&t2,i->type);

		/*real part (r1r2)/(r2r2 + i2i2)*/
		mpwl_mul_ui(rop->r,r,in);

		mpwl_mul(&t1,r,r);
		mpwl_mul(&t2,i,i);
		mpwl_add(&t2,&t2,&t1);

		mpwl_div(rop->r,rop->r,&t2);

		/*imaginary part (- r1i2)/(r2r2 + i2i2)*/
		mpwl_mul_ui(rop->i,i,in);
		mpwl_neg(rop->i,rop->i);

		/*t2 is calculated above*/

		mpwl_div(rop->i,rop->i,&t2);

		mpwl_clear (&t1);
		mpwl_clear (&t2);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_mod (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		if G_UNLIKELY (mpwl_zero_p (op2->r)) {
			gel_errorout (_("Division by zero!"));
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_mod(rop->r,op1->r,op2->r);
	} else {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't modulo complex numbers"));
	}
}

void
mpw_invert (mpw_ptr rop, mpw_ptr op1, mpw_ptr mod)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (mod)) {
		MAKE_REAL (rop);
		if (rop != op1 && rop != mod) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		if G_UNLIKELY ( ! mpwl_invert (rop->r, op1->r, mod->r)) {
			if (op1->r->type == MPW_INTEGER &&
			    mod->r->type == MPW_INTEGER) {
				char *n1, *n2;
				/* if the above just failed because of
				   types */
				n1 = mpwl_getstring_for_error (op1->r);
				n2 = mpwl_getstring_for_error (mod->r);
				gel_error_num = GEL_NUMERICAL_MPW_ERROR;
				gel_errorout (_("Inverse of %s modulo "
						"%s not found!"),
					      n1, n2);
				g_free (n1);
				g_free (n2);
			}
		}
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't do modulo invert on complex numbers"));
	}
}

void
mpw_gcd (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL (rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_gcd (rop->r, op1->r, op2->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't GCD complex numbers"));
	}
}
void
mpw_lcm (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MpwRealNum gcd = {{NULL}};
		mpwl_init_type (&gcd, MPW_INTEGER);

		mpwl_gcd (&gcd, op1->r, op2->r);
		if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
			mpwl_clear (&gcd);
			return;
		}

		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_mul (rop->r, op1->r, op2->r);
		mpwl_div (rop->r, rop->r, &gcd);
		mpwl_clear (&gcd);
		if (mpwl_sgn (rop->r) < 0)
			mpwl_neg (rop->r, rop->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't LCM complex numbers"));
	}
}

void
mpw_jacobi (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_jacobi(rop->r,op1->r,op2->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't get Jacobi symbols of complex numbers"));
	}
}

void
mpw_legendre(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_legendre(rop->r,op1->r,op2->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't get Legendre symbols of complex numbers"));
	}
}

void
mpw_kronecker (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_kronecker(rop->r,op1->r,op2->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't get Jacobi symbol with Kronecker extension for complex numbers"));
	}
}

void
mpw_lucnum (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		MAKE_REAL (rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_lucnum (rop->r, op->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout  (_("Can't get Lucas number for complex numbers"));
	}
}
void
mpw_nextprime (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		MAKE_REAL (rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_nextprime (rop->r, op->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout  (_("Can't get next prime for complex numbers"));
	}
}
gboolean
mpw_perfect_square(mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		return mpwl_perfect_square(op->r);
	} else {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "perfect_square");
		return FALSE;
	}
}
gboolean
mpw_perfect_power(mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		return mpwl_perfect_power(op->r);
	} else {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "perfect_power");
		return FALSE;
	}
}
gboolean
mpw_even_p(mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		return mpwl_even_p(op->r);
	} else {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "even_p");
		return FALSE;
	}
}
gboolean
mpw_odd_p(mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		return mpwl_odd_p(op->r);
	} else {
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "odd_p");
		return FALSE;
	}
}

/* exact zero, not a float! */
gboolean
mpw_exact_zero_p (mpw_ptr op) /* PURE! */
{
	if (MPW_IS_REAL (op) &&
	    (op->r == gel_zero ||
	     ((op->r->type == MPW_INTEGER ||
	       op->r->type == MPW_RATIONAL) &&
	      mpwl_zero_p (op->r)))) {
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean
mpw_zero_p (mpw_ptr op) /* PURE!*/
{
	if ((op->r == gel_zero || mpwl_zero_p (op->r)) &&
	    (op->i == gel_zero || mpwl_zero_p (op->i))) {
		return TRUE;
	} else {
		return FALSE;
	}
}


void
mpw_pow (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL(rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, op1->r->type);
		} else {
			MAKE_COPY (rop->r);
		}
		if(mpwl_pow(rop->r,op1->r,op2->r)) {
			goto backup_mpw_pow;
		}
	} else if (MPW_IS_REAL (op2) &&
		   op2->r->type == MPW_INTEGER &&
		   op1->i->type != MPW_FLOAT &&
		   mpwl_zero_p (op1->r)) {
		MpwRealNum t = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		mpwl_init_type (&t, op1->i->type);
		mpwl_init_type (&t2, op2->r->type);
		mpwl_set (&t2, op2->r);

		if (mpwl_pow (&t, op1->i, op2->r)) {
			mpwl_clear (&t2);
			mpwl_clear (&t);
			goto backup_mpw_pow;

		}

		if (mpwl_even_p (&t2)) {
			/*even*/
			MAKE_REAL (rop);
			/* FIXME: use MAKE_EMPTY when possible */
			MAKE_COPY (rop->r);

			mpwl_div_ui (&t2, &t2, 2);
			if (mpwl_even_p (&t2)) {
				/* divisible by 4 */
				mpwl_set (rop->r, &t);
			} else {
				mpwl_neg (rop->r, &t);
			}
		} else {
			/*odd*/
			MAKE_IMAG (rop);
			/* FIXME: use MAKE_EMPTY when possible */
			MAKE_COPY (rop->i);

			mpwl_sub_ui (&t2, &t2, 1);
			mpwl_div_ui (&t2, &t2, 2);
			if (mpwl_even_p (&t2)) {
				/* (exponent-1) divisible by 4 */
				mpwl_set (rop->i, &t);
			} else {
				mpwl_neg (rop->i, &t);
			}
		}

		mpwl_clear (&t2);
		mpwl_clear (&t);
	} else {
		goto backup_mpw_pow;
	}
	return;
backup_mpw_pow:
	if (mpwl_zero_p (op1->r) &&
	    mpwl_zero_p (op1->i)) {
		mpw_set_ui (rop, 0);
	} else {
		mpw_t tmp;
		mpw_init (tmp);
		mpw_ln (tmp, op1);
		mpw_mul (tmp, tmp, op2);
		mpw_exp (tmp, tmp);
		mpw_set (rop, tmp);
		mpw_clear (tmp);
	}
}

void
mpw_powm (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2, mpw_ptr mod)
{
	if G_UNLIKELY (MPW_IS_COMPLEX (op1) ||
		       MPW_IS_COMPLEX (op2) ||
		       MPW_IS_COMPLEX (mod)) {
		gel_errorout (_("%s: Bad types for mod power"),
			      "powm");
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	MAKE_REAL (rop);
	if (rop != op1 && rop != op2 && rop != mod) {
		MAKE_EMPTY (rop->r, MPW_INTEGER);
	} else {
		MAKE_COPY (rop->r);
	}

	mpwl_powm (rop->r, op1->r, op2->r, mod->r);
}

void
mpw_powm_ui (mpw_ptr rop,mpw_ptr op, unsigned long int e, mpw_ptr mod)
{
	if G_UNLIKELY (MPW_IS_COMPLEX (op) ||
		       MPW_IS_COMPLEX (mod)) {
		gel_errorout (_("%s: Bad types for mod power"),
			      "powm");
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}

	MAKE_REAL (rop);
	if (rop != op && rop != mod) {
		MAKE_EMPTY (rop->r, MPW_INTEGER);
	} else {
		MAKE_COPY (rop->r);
	}

	mpwl_powm_ui (rop->r, op->r, e, mod->r);
}

void
mpw_sqrt(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, op->r->type);
		} else {
			MAKE_COPY (rop->r);
		}
		if(mpwl_sqrt(rop->r,op->r)) {
			MpwRealNum *t;
			t = rop->r;
			rop->r = rop->i;
			rop->i = t;
		}
	} else {
		mpw_ln(rop,op);
		mpw_div_ui(rop,rop,2);
		mpw_exp(rop,rop);
	}
}

void
mpw_exp(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_exp(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
			MAKE_EMPTY (rop->i, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_exp(rop->r,r);
		mpwl_set(rop->i,rop->r);
		
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_clear (&t);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_ln(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		if G_UNLIKELY (mpwl_zero_p (op->r)) {
			gel_errorout (_("%s: can't take logarithm of 0"),
				      "ln");
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		if(!mpwl_ln(rop->r,op->r)) {
			MAKE_EMPTY (rop->i, MPW_FLOAT);
			mpwl_pi (rop->i);
		}
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
			MAKE_EMPTY (rop->i, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		if(mpwl_zero_p (op->r)) {
			g_assert( ! mpwl_zero_p (op->i));
			/*don't set the pi before the ln, for the case
			  rop==op*/
			if(mpwl_ln(rop->r,op->i)) {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
			} else {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
				mpwl_neg(rop->i,rop->i);
			}
			return;
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_mul(rop->r,r,r);
		mpwl_mul(&t,i,i);
		mpwl_add(rop->r,rop->r,&t);
		mpwl_ln(rop->r,rop->r);
		mpwl_div_ui(rop->r,rop->r,2);

		mpwl_div(rop->i,i,r);
		mpwl_arctan(rop->i,rop->i);

		if(mpwl_sgn(r)<0) {
			mpwl_pi(&t);
			if(mpwl_sgn(i)<0)
				mpwl_sub(rop->i,rop->i,&t);
			else
				mpwl_add(rop->i,rop->i,&t);
		}

		mpw_uncomplex(rop);

		mpwl_clear (&t);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_log2(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		if G_UNLIKELY (mpwl_zero_p (op->r)) {
			gel_errorout (_("%s: can't take logarithm of 0"),
				      "log2");
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		if(!mpwl_log2(rop->r,op->r)) {
			MpwRealNum t = {{NULL}};
			MAKE_EMPTY(rop->i, MPW_FLOAT);
			mpwl_pi (rop->i);
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_ln2 (&t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_clear (&t);
		}
	} else {
		mpw_t two;
		if (mpwl_zero_p (op->r)) {
			MpwRealNum t = {{NULL}};

			if (rop != op) {
				MAKE_EMPTY (rop->r, MPW_FLOAT);
				MAKE_EMPTY (rop->i, MPW_FLOAT);
			} else {
				MAKE_COPY (rop->r);
				MAKE_COPY (rop->i);
			}

			g_assert( ! mpwl_zero_p (op->i));
			/*don't set the pi before the ln, for the case
			  rop==op*/
			if(mpwl_log2(rop->r,op->i)) {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
			} else {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
				mpwl_neg(rop->i,rop->i);
			}
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_ln2 (&t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_clear (&t);
			return;
		}
		/* this is stupid, but simple to implement for now */
		mpw_init (two);
		mpw_ln2 (two);
		mpw_ln (rop, op);
		mpw_div (rop, rop, two);
		mpw_clear (two);
	}
}

void
mpw_log10(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		if G_UNLIKELY (mpwl_zero_p (op->r)) {
			gel_errorout (_("%s: can't take logarithm of 0"),
				      "log10");
			gel_error_num=GEL_NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		if(!mpwl_log10(rop->r,op->r)) {
			MpwRealNum t = {{NULL}};
			MAKE_EMPTY (rop->i, MPW_FLOAT);
			mpwl_pi (rop->i);
			/* FIXME: implement caching */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_d (&t, 10.0);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_clear (&t);
		}
	} else {
		mpw_t ten;
		if (mpwl_zero_p (op->r)) {
			MpwRealNum t = {{NULL}};

			if (rop != op) {
				MAKE_EMPTY (rop->r, MPW_FLOAT);
				MAKE_EMPTY (rop->i, MPW_FLOAT);
			} else {
				MAKE_COPY (rop->r);
				MAKE_COPY (rop->i);
			}

			g_assert( ! mpwl_zero_p (op->i));
			/*don't set the pi before the ln, for the case
			  rop==op*/
			if(mpwl_log10(rop->r,op->i)) {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
			} else {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
				mpwl_neg(rop->i,rop->i);
			}
			/* FIXME: implement caching */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_d (&t, 10.0);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_clear (&t);
			return;
		}
		/* this is stupid, but simple to implement for now */
		mpw_init (ten);
		/* FIXME: implement caching */
		mpw_set_d (ten, 10.0);
		mpw_ln (ten, ten);
		mpw_ln (rop, op);
		mpw_div (rop, rop, ten);
		mpw_clear (ten);
	}
}

void
mpw_sin(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_sin(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
			MAKE_EMPTY (rop->i, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_sin(rop->r,r);
		mpwl_cosh(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_cos(rop->i,r);
		mpwl_sinh(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_clear (&t);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_cos(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_cos(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
			MAKE_EMPTY (rop->i, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_cos(rop->r,r);
		mpwl_cosh(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sin(rop->i,r);
		mpwl_neg(rop->i,rop->i);
		mpwl_sinh(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_clear (&t);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_sinh(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_sinh(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
			MAKE_EMPTY (rop->i, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_sinh(rop->r,r);
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_cosh(rop->i,r);
		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_clear (&t);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_cosh(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_cosh(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
			MAKE_EMPTY (rop->i, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_cosh(rop->r,r);
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sinh(rop->i,r);
		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_clear (&t);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_arctan(mpw_ptr rop,mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_FLOAT);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_arctan(rop->r,op->r);
	} else {
		mpw_t tmp1;
		mpw_t tmp2;
		mpw_t ai;
		MpwRealNum *t;
		
		/*multiply op by i and put it into ai*/
		mpw_init_set(ai,op);
		MAKE_COPY(ai->r);
		MAKE_COPY(ai->i);
		
		t = ai->i;
		ai->i = ai->r;
		ai->r = t;
		
		mpwl_neg(ai->r,ai->r);
		
		gel_error_num = 0;
		
		mpw_init(tmp1);
		mpw_set_ui(tmp1,1);
		mpw_add(tmp1,tmp1,ai);
		
		mpw_init(tmp2);
		mpw_set_ui(tmp2,1);
		mpw_sub(tmp2,tmp2,ai);
		mpw_clear(ai);

		mpw_div(tmp1,tmp1,tmp2);
		mpw_clear(tmp2);
		
		if G_UNLIKELY (gel_error_num) {
			mpw_clear(tmp1);
			return;
		}
		
		mpw_ln(tmp1,tmp1);

		if G_UNLIKELY (gel_error_num) {
			mpw_clear(tmp1);
			return;
		}

		/*divide by 2i*/
		MAKE_COPY(tmp1->r);
		MAKE_COPY(tmp1->i);
		
		t = tmp1->i;
		tmp1->i = tmp1->r;
		tmp1->r = t;
		
		mpwl_neg(tmp1->i,tmp1->i);
		mpwl_div_ui(tmp1->r,tmp1->r,2);
		mpwl_div_ui(tmp1->i,tmp1->i,2);
		
		mpw_uncomplex(tmp1);
		
		mpw_set(rop,tmp1);
		mpw_clear(tmp1);
	}
}

void
mpw_arctan2 (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		MAKE_REAL (rop);
		if (rop != op1 && rop != op2) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_arctan2 (rop->r, op1->r, op2->r);
	} else {
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		gel_errorout (_("arctan2 not defined for complex numbers"));
	}
}

void
mpw_pi (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_pi (rop->r);
}

void
mpw_ln2 (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_ln2 (rop->r);
}

void
mpw_euler_constant (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_euler_constant (rop->r);
}

void
mpw_catalan_constant (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_catalan_constant (rop->r);
}

void
mpw_rand (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_rand (rop->r);
}

void
mpw_randint (mpw_ptr rop, mpw_ptr op)
{
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't make random integer out of a complex number"));
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}
	MAKE_REAL (rop);
	if (rop != op) {
		MAKE_EMPTY (rop->r, MPW_INTEGER);
	} else {
		MAKE_COPY (rop->r);
	}
	mpwl_randint (rop->r, op->r);
}

void
mpw_i (mpw_ptr rop)
{
	mpw_clear (rop);

	rop->r = gel_zero;
	gel_zero->alloc.usage++;
	rop->i = gel_one;
	gel_one->alloc.usage++;
}

void
mpw_conj (mpw_ptr rop, mpw_ptr op)
{
	if (MPW_IS_REAL (op)) {
		if (rop != op) {
			MAKE_REAL (rop);
			DEALLOC_MPWL (rop->r);
			rop->r = op->r;
			ALLOC_MPWL (rop->r);
		}
	} else {
		if (rop != op) {
			DEALLOC_MPWL (rop->r);
			rop->r = op->r;
			ALLOC_MPWL (rop->r);

			MAKE_EMPTY (rop->i, op->i->type);
			mpwl_neg (rop->i, op->i);
		} else {
			MAKE_COPY(rop->i);
			mpwl_neg(rop->i,op->i);
		}
	}
}

void
mpw_pow_ui(mpw_ptr rop,mpw_ptr op, unsigned long int e)
{
	if (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, op->r->type);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_pow_ui(rop->r,op->r,e,FALSE);
	} else {
		mpw_ln(rop,op);
		mpw_mul_ui(rop,rop,e);
		mpw_exp(rop,rop);
	}
}

int
mpw_cmp(mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (MPW_IS_REAL (op1) && MPW_IS_REAL (op2)) {
		int ret = mpwl_cmp(op1->r,op2->r);
		/* normalize, gmp is liable to return things other than -1,0,1
		 * and we want ONLY -1,0,1 values in genius, see implementation
		 * of for loops. */
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		} else {
			return 0;
		}
	} else {
		gel_errorout (_("Can't compare complex numbers"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	}
}

int
mpw_cmp_ui(mpw_ptr op, unsigned long int i)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		int ret = mpwl_cmp_ui(op->r,i);
		/* normalize, gmp is liable to return things other than -1,0,1
		 * and we want ONLY -1,0,1 values in genius, see implementation
		 * of for loops. */
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		} else {
			return 0;
		}
	} else {
		gel_errorout (_("Can't compare complex numbers"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	}
}

gboolean
mpw_eql(mpw_ptr op1, mpw_ptr op2)
{
	if (MPW_IS_COMPLEX (op1) || MPW_IS_COMPLEX (op2))
		return (mpwl_eql(op1->r,op2->r) && mpwl_eql(op1->i,op2->i));
	else
		return mpwl_eql(op1->r,op2->r);
}

gboolean
mpw_symbolic_eql(mpw_ptr op1, mpw_ptr op2)
{
	/* Here we're assuming that rationals of the form n/1 are
	 * now integers */
	if (op1->r->type == op2->r->type &&
	    op1->i->type == op2->i->type)
		return mpw_eql (op1, op2);
	else
		return FALSE;
}

gboolean 
mpw_eql_ui(mpw_ptr op, unsigned long int i)
{
	if (MPW_IS_REAL (op)) {
		return mpwl_cmp_ui (op->r, i) == 0;
	} else {
		return FALSE;
	}
}

void
mpw_fac_ui(mpw_ptr rop,unsigned long int i)
{
	MAKE_REAL (rop);
	MAKE_EMPTY (rop->r, MPW_INTEGER);
	mpwl_fac_ui (rop->r, i);
}

void
mpw_fac(mpw_ptr rop,mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_fac(rop->r,op->r);
	} else {
		gel_errorout (_("Can't make factorials of complex numbers"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

void
mpw_dblfac (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		MAKE_REAL (rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_dblfac (rop->r, op->r);
	} else {
		gel_errorout (_("Can't make factorials of complex numbers"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

void
mpw_bin_ui(mpw_ptr rop,mpw_ptr op, unsigned long r)
{
	if G_LIKELY (MPW_IS_REAL (op)) {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_bin_ui(rop->r,op->r,r);
	} else {
		gel_errorout (_("Can't make binomials of complex numbers"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
	}
}

/*make a number int if possible*/
void
mpw_make_int(mpw_ptr rop)
{
	if (MPW_IS_REAL (rop)) {
		mpwl_make_int(rop->r);
	} else {
		mpwl_make_int(rop->r);
		mpwl_make_int(rop->i);
	}
}

/*make number into a float, this might be neccessary for unprecise
  calculations*/
void
mpw_make_float(mpw_ptr rop)
{
	if (rop->r->type != MPW_FLOAT) {
		MAKE_COPY(rop->r);
		mpwl_make_float(rop->r);
	}
	if ( ! MPW_IS_REAL (rop) &&
	    rop->i->type != MPW_FLOAT) {
		MAKE_COPY(rop->i);
		mpwl_make_float(rop->i);
	}
}

/*init the mp stuff*/
void
mpw_init_mp(void)
{
	static gboolean done = FALSE;
	if (done)
		return;

	GET_NEW_REAL(gel_zero);
	mpwl_init_type(gel_zero,MPW_INTEGER);
	mpwl_set_ui(gel_zero,0);
	gel_zero->alloc.usage = 1;
	GET_NEW_REAL(gel_one);
	mpwl_init_type(gel_one,MPW_INTEGER);
	mpwl_set_ui(gel_one,1);
	gel_one->alloc.usage = 1;
	done = TRUE;
}

char *
mpw_getstring(mpw_ptr num, int max_digits,
	      gboolean scientific_notation,
	      gboolean results_as_floats,
	      gboolean mixed_fractions,
	      /* GelOutputStyle */ int style,
	      int integer_output_base,
	      gboolean add_parenths)
{
	mpw_uncomplex(num);
	if (MPW_IS_REAL (num)) {
		return mpwl_getstring(num->r,max_digits,scientific_notation,
			results_as_floats,mixed_fractions,style,
			integer_output_base,"",
			-1 /* chop */);
	} else {
		char *p1 = NULL, *p2, *r;
		gboolean justimaginary = mpwl_zero_p (num->r);
		if (! justimaginary)
			p1 = mpwl_getstring(num->r,
					    max_digits,
					    scientific_notation,
					    results_as_floats,
					    mixed_fractions, 
					    style,
					    integer_output_base,
					    "" /* postfix */,
					    -1 /* chop */);
		p2 = mpwl_getstring(num->i,
				    max_digits,
				    scientific_notation,
				    results_as_floats,
				    mixed_fractions, 
				    style,
				    integer_output_base,
				    "i" /* postfix */,
				    -1 /* chop */);
		if (justimaginary) {
			r = p2;
			p2 = NULL;
		} else if (mpwl_sgn(num->i)>=0) {
			if (add_parenths)
				r = g_strconcat("(",p1,"+",p2,")",NULL);
			else
				r = g_strconcat(p1,"+",p2,NULL);
		} else {
			if (add_parenths)
				r = g_strconcat("(",p1,p2,")",NULL);
			else
				r = g_strconcat(p1,p2,NULL);
		}
		g_free(p1);
		g_free(p2);
		return r;
	}
}

gboolean
mpw_chop_p (mpw_ptr num,
	    int chop_when)
{
	if (MPW_IS_REAL (num)) {
		if (mpwl_zero_p (num->r))
			return FALSE;
		return (num->r->type == MPW_INTEGER ||
			/* approximately the exponent base 10 */
			mpwl_get_exp (num->r) / 3.32192809489 > -chop_when);
	} else {
		if ( ! mpwl_zero_p (num->r))
			return (num->r->type == MPW_INTEGER ||
				num->i->type == MPW_INTEGER ||
				/* approximately the exponent base 10 */
				mpwl_get_exp (num->r) / 3.32192809489 > -chop_when ||
				mpwl_get_exp (num->i) / 3.32192809489 > -chop_when);
		else
			return (num->i->type == MPW_INTEGER ||
				/* approximately the exponent base 10 */
				mpwl_get_exp (num->i) / 3.32192809489 > -chop_when);
	}
}

char *
mpw_getstring_chop (mpw_ptr num, int max_digits,
		    gboolean scientific_notation,
		    gboolean results_as_floats,
		    gboolean mixed_fractions,
		    /* GelOutputStyle */ int style,
		    int integer_output_base,
		    gboolean add_parenths,
		    int chop,
		    int chop_when,
		    gboolean force_chop)
{
	mpw_uncomplex(num);
	if (chop > 0 &&
	    chop_when >= chop)
		force_chop = TRUE;
	if (MPW_IS_REAL (num)) {
		return mpwl_getstring(num->r,max_digits,scientific_notation,
			results_as_floats,mixed_fractions,style,
			integer_output_base,"",
			force_chop ? chop : -1);
	} else {
		char *p1 = NULL, *p2, *r;
		gboolean justimaginary = mpwl_zero_p (num->r);
		int chop_tmp = force_chop ? chop : -1;

		if (! justimaginary) {
			if (force_chop ||
			    num->r->type == MPW_INTEGER ||
			    num->i->type == MPW_INTEGER ||
			    /* approximately the exponent base 10 */
			    mpwl_get_exp (num->r) / 3.32192809489 > -chop_when ||
			    mpwl_get_exp (num->i) / 3.32192809489 > -chop_when)
				chop_tmp = chop;
			p1 = mpwl_getstring(num->r,
					    max_digits,
					    scientific_notation,
					    results_as_floats,
					    mixed_fractions, 
					    style,
					    integer_output_base,
					    "" /* postfix */,
					    chop_tmp);
		}
		p2 = mpwl_getstring(num->i,
				    max_digits,
				    scientific_notation,
				    results_as_floats,
				    mixed_fractions, 
				    style,
				    integer_output_base,
				    "i" /* postfix */,
				    chop_tmp);
		if (justimaginary) {
			r = p2;
			p2 = NULL;
		} else if (p2[0] != '+' && p2[0] != '-') {
			if (add_parenths)
				r = g_strconcat("(",p1,"+",p2,")",NULL);
			else
				r = g_strconcat(p1,"+",p2,NULL);
		} else {
			if (add_parenths)
				r = g_strconcat("(",p1,p2,")",NULL);
			else
				r = g_strconcat(p1,p2,NULL);
		}
		g_free(p1);
		g_free(p2);
		return r;
	}
}

void
mpw_set_str_float (mpw_ptr rop, const char *s, int base)
{
	MAKE_REAL(rop);
	MAKE_EMPTY (rop->r, MPW_FLOAT);
	mpwl_set_str_float(rop->r,s,base);
}

void
mpw_set_str_int (mpw_ptr rop, const char *s, int base)
{
	MAKE_REAL(rop);
	MAKE_EMPTY (rop->r, MPW_INTEGER);
	mpwl_set_str_int(rop->r,s,base);
}

void
mpw_set_str_complex_int(mpw_ptr rop,const char *s,int base)
{
	char *p;
	int size;

	p = g_strdup(s);
	size = strlen(p);
	if(p[size-1] == 'i')
		p[size-1] = '\0';
	MAKE_EMPTY (rop->i, MPW_INTEGER);
	mpwl_set_str_int(rop->i,p,base);

	g_free(p);

	mpw_uncomplex(rop);
}

void
mpw_set_str_complex(mpw_ptr rop,const char *s,int base)
{
	char *p;
	int size;

	p = g_strdup(s);
	size = strlen(p);
	if(p[size-1] == 'i')
		p[size-1] = '\0';
	MAKE_EMPTY(rop->i, MPW_FLOAT);
	mpwl_set_str_float(rop->i,p,base);

	g_free(p);

	mpw_uncomplex(rop);
}

static gboolean
looks_like_float (const char *s)
{
	/*floats, FIXME: this is pretty hackish */
	if (strchr(s,'.') != NULL)
		return TRUE;
	return (strchr(s,'e') != NULL ||
		strchr(s,'E') != NULL) &&
		strncmp(s,"0x",2) != 0 &&
		strchr(s,'\\') == NULL;
}

/*set one element (space separated)*/
static void
mpw_set_str_one(mpw_ptr rop,const char *s,int base)
{
	/*rationals*/
	if(strchr(s,'/')) {
		char *p = g_strdup(s);
		char *pp;
		mpw_t tmp;
		char *ptrptr = NULL;

		mpw_init(tmp);

		/* numerator */
		pp = strtok_r (p,"/", &ptrptr);
		if (strchr (pp, 'i') == NULL)
			mpw_set_str_int(rop,pp,base);
		else
			mpw_set_str_complex_int(rop,pp,base);

		/* denominator */
		pp = strtok_r (NULL,"/", &ptrptr);
		if (strchr (pp, 'i') == NULL)
			mpw_set_str_int(tmp,pp,base);
		else
			mpw_set_str_complex_int(tmp,pp,base);

		g_free(p);
		
		mpw_div(rop,rop,tmp);
	/*complex*/
	} else if(strchr(s,'i')) {
		char *p = g_strdup(s);
		char *pp;
		mpw_t tmp;

		for(pp=p;*pp;pp++) {
			if(*pp=='+') {
				*pp='\0';
				pp++;
				break;
			} else if(*pp=='-') {
				if(pp>p && *(pp-1)!='e') {
					*pp='\0';
					pp++;
					break;
				}
			}
		}
		/*must be a pure imaginary*/
		if(!*pp) {
			g_free(p);
			if (looks_like_float (s))
				mpw_set_str_complex(rop,s,base);
			else
				mpw_set_str_complex_int(rop,s,base);
			return;
		}
		mpw_set_str(rop,p,base);
		mpw_init(tmp);
		if (looks_like_float (pp))
			mpw_set_str_complex(tmp,pp,base);
		else
			mpw_set_str_complex_int(tmp,pp,base);
		mpw_add(rop,rop,tmp);
		mpw_clear(tmp);
	} else if (looks_like_float (s)) {
		mpw_set_str_float(rop,s,base);
	} else {
		mpw_set_str_int(rop,s,base);
	}
}

void
mpw_set_str(mpw_ptr rop,const char *s,int base)
{
	char *p;
	char *d;
	char *ptrptr = NULL;
	mpw_t tmp;
	p = strchr(s,' ');
	if(!p) {
		mpw_set_str_one(rop,s,base);
		return;
	}
	mpw_init(tmp);
	mpw_set_ui(rop,0);
	d = g_strdup(s);
	p = strtok_r (d, " ", &ptrptr);
	while(p) {
		mpw_set_str_one(tmp,p,base);
		mpw_add(rop,rop,tmp);
		p = strtok_r (NULL, " ", &ptrptr);
	}
	mpw_clear(tmp);
	g_free(d);
}

gboolean
mpw_is_complex(mpw_ptr op)
{
	mpw_uncomplex(op);
	return MPW_IS_COMPLEX (op);
}

gboolean
mpw_is_integer(mpw_ptr op)
{
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't determine type of a complex number"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_INTEGER;
}

gboolean
mpw_is_rational(mpw_ptr op)
{
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't determine type of a complex number"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_RATIONAL;
}

gboolean
mpw_is_float(mpw_ptr op)
{
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't determine type of a complex number"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_FLOAT;
}

void
mpw_im(mpw_ptr rop, mpw_ptr op)
{
	if (rop == op) {
		MAKE_IMAG(rop);
		rop->r = rop->i;
		/* Note that rop->r is set to gel_zero
		   and it is allocated in MAKE_IMAG */
		rop->i = gel_zero;
		return;
	}
	MAKE_REAL(rop);
	DEALLOC_MPWL (rop->r);
	rop->r = op->i;
	ALLOC_MPWL (rop->r);
}

void
mpw_re(mpw_ptr rop, mpw_ptr op)
{
	MAKE_REAL(rop);
	if (rop == op) {
		return;
	}
	DEALLOC_MPWL (rop->r);
	rop->r = op->r;
	ALLOC_MPWL (rop->r);
}

void
mpw_round(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_round(rop->r);
	if (MPW_IS_COMPLEX (op)) {
		MAKE_COPY(rop->i);
		mpwl_round(rop->i);
		mpw_uncomplex(rop);
	}
}

void
mpw_floor(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_floor(rop->r);
	if (MPW_IS_COMPLEX (op)) {
		MAKE_COPY(rop->i);
		mpwl_floor(rop->i);
		mpw_uncomplex(rop);
	}
}

void
mpw_ceil(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_ceil(rop->r);
	if (MPW_IS_COMPLEX (op)) {
		MAKE_COPY(rop->i);
		mpwl_ceil(rop->i);
		mpw_uncomplex(rop);
	}
}

void
mpw_trunc(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_trunc(rop->r);
	if (MPW_IS_COMPLEX (op)) {
		MAKE_COPY(rop->i);
		mpwl_trunc(rop->i);
		mpw_uncomplex(rop);
	}
}

long
mpw_get_long(mpw_ptr op)
{
	long r;
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't convert complex number into integer"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_long(op->r,&ex);
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		gel_errorout (_("Can't convert real number to integer"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	} else if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		gel_errorout (_("Integer too large for this operation"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

unsigned long
mpw_get_ulong(mpw_ptr op)
{
	unsigned long r;
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't convert complex number into integer"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_ulong(op->r,&ex);
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		gel_errorout (_("Can't convert real number to integer"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	} else if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		gel_errorout (_("Integer too large for this operation"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

double
mpw_get_double (mpw_ptr op)
{
	double r;
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	if G_UNLIKELY (MPW_IS_COMPLEX (op)) {
		gel_errorout (_("Can't convert complex number into a double"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_double (op->r, &ex);
	/* currently there is no conversion error exception for
	   get_double */
#if 0
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		gel_errorout (_("Can't convert real number to double"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	} else
#endif
	if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		gel_errorout (_("Number too large for this operation"));
		gel_error_num=GEL_NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

void
mpw_get_complex_double (mpw_ptr op, double *r, double *i)
{
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	*r = mpwl_get_double (op->r, &ex);
	*i = mpwl_get_double (op->i, &ex);

	if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		gel_errorout (_("Number too large for this operation"));
		*r = 0.0;
		*i = 0.0;
		gel_error_num = GEL_NUMERICAL_MPW_ERROR;
		return;
	}
}

void
mpw_denominator(mpw_ptr rop, mpw_ptr op)
{
	if (MPW_IS_COMPLEX (op)) {
		MpwRealNum r1 = {{NULL}};
		MpwRealNum r2 = {{NULL}};

		mpwl_init_type (&r1, MPW_INTEGER);
		mpwl_init_type (&r2, MPW_INTEGER);

		mpwl_denominator (&r1, op->r);
		mpwl_denominator (&r2, op->i);

		if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
			mpwl_clear (&r1);
			mpwl_clear (&r2);
			return;
		}

		MAKE_REAL (rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}

		mpwl_mul (rop->r, &r1, &r2);
		mpwl_gcd (&r1, &r1, &r2);
		mpwl_div (rop->r, rop->r, &r1);

		mpwl_clear (&r1);
		mpwl_clear (&r2);
	} else {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_denominator(rop->r, op->r);
	}
}

void
mpw_numerator(mpw_ptr rop, mpw_ptr op)
{
	if (MPW_IS_COMPLEX (op)) {
		MpwRealNum r1 = {{NULL}};
		MpwRealNum r2 = {{NULL}};
		MpwRealNum n1 = {{NULL}};
		MpwRealNum n2 = {{NULL}};

		mpwl_init_type (&r1, MPW_INTEGER);
		mpwl_init_type (&r2, MPW_INTEGER);
		mpwl_init_type (&n1, MPW_INTEGER);
		mpwl_init_type (&n2, MPW_INTEGER);

		mpwl_denominator (&r1, op->r);
		mpwl_denominator (&r2, op->i);
		mpwl_numerator (&n1, op->r);
		mpwl_numerator (&n2, op->i);

		if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
			mpwl_clear (&r1);
			mpwl_clear (&r2);
			mpwl_clear (&n1);
			mpwl_clear (&n2);
			return;
		}

		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
			MAKE_EMPTY (rop->i, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
			MAKE_COPY (rop->i);
		}

		mpwl_mul (rop->r, &n1, &r2);
		mpwl_mul (rop->i, &n2, &r1);

		mpwl_gcd (&r1, &r1, &r2);

		mpwl_div (rop->r, rop->r, &r1);
		mpwl_div (rop->i, rop->i, &r1);

		mpwl_clear (&r1);
		mpwl_clear (&r2);
		mpwl_clear (&n1);
		mpwl_clear (&n2);

		mpw_uncomplex (rop);
	} else {
		MAKE_REAL(rop);
		if (rop != op) {
			MAKE_EMPTY (rop->r, MPW_INTEGER);
		} else {
			MAKE_COPY (rop->r);
		}
		mpwl_numerator(rop->r, op->r);
	}
}
