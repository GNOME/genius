/* GENIUS Calculator
 * Copyright (C) 1997-2004 Jiri (George) Lebl
 *
 * Author: Jiri (George) Lebl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the  Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
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

	int default_mpf_prec;
	gboolean double_math; /* instead of gmp, use doubles */
	
	gpointer data;
};
#endif

static MpwRealNum *free_reals = NULL;
static int free_reals_n = 0;

static MpwRealNum *zero = NULL;
static MpwRealNum *one = NULL;

static int default_mpf_prec = 0;

#define FREE_LIST_SIZE 1000
static mpz_ptr free_mpz[FREE_LIST_SIZE] = { NULL, };
static mpz_ptr *free_mpz_top = free_mpz;
static mpq_ptr free_mpq[FREE_LIST_SIZE] = { NULL, };
static mpq_ptr *free_mpq_top = free_mpq;
static mpfr_ptr free_mpf[FREE_LIST_SIZE] = { NULL, };
static mpfr_ptr *free_mpf_top = free_mpf;

#define GET_INIT_MPZ(THE_z)				\
	if (free_mpz_top == free_mpz) {			\
		THE_z = g_new(__mpz_struct,1);		\
		mpz_init (THE_z);			\
	} else {					\
		free_mpz_top--;				\
		THE_z = *free_mpz_top;			\
	}
#define CLEAR_FREE_MPZ(THE_z)				\
	if (free_mpz_top == &free_mpz[FREE_LIST_SIZE-1] || \
	    mpz_size (THE_z) > 2) {			\
		mpz_clear (THE_z);			\
		g_free (THE_z);				\
	} else {					\
		*free_mpz_top = THE_z;			\
		free_mpz_top++;				\
	}
#define GET_INIT_MPQ(THE_q)				\
	if (free_mpq_top == free_mpq) {			\
		THE_q = g_new(__mpq_struct,1);		\
		mpq_init (THE_q);			\
	} else {					\
		free_mpq_top--;				\
		THE_q = *free_mpq_top;			\
	}
#define CLEAR_FREE_MPQ(THE_q)				\
	if (free_mpq_top == &free_mpq[FREE_LIST_SIZE-1] || \
	    mpz_size (mpq_denref (THE_q)) > 2 ||	\
	    mpz_size (mpq_numref (THE_q)) > 2) {	\
		mpq_clear (THE_q);			\
		g_free (THE_q);				\
	} else {					\
		*free_mpq_top = THE_q;			\
		free_mpq_top++;				\
	}
#define GET_INIT_MPF(THE_f)				\
	if (free_mpf_top == free_mpf) {			\
		THE_f = g_new(__mpfr_struct,1);		\
		mpf_init (THE_f);			\
	} else {					\
		free_mpf_top--;				\
		THE_f = *free_mpf_top;			\
	}
#define CLEAR_FREE_MPF(THE_f)				\
	if (free_mpf_top == &free_mpf[FREE_LIST_SIZE-1]) { \
		mpf_clear (THE_f);			\
		g_free (THE_f);				\
	} else {					\
		*free_mpf_top = THE_f;			\
		free_mpf_top++;				\
	}

#define MAKE_CPLX_OPS(THE_op,THE_r,THE_i) {		\
	if(rop==THE_op) {				\
		THE_r = g_new0(MpwRealNum,1);		\
		THE_i = g_new0(MpwRealNum,1);		\
		mpwl_init_type(THE_r,MPW_FLOAT);	\
		mpwl_init_type(THE_i,MPW_FLOAT);	\
		mpwl_set(THE_r,THE_op->r);		\
		mpwl_set(THE_i,THE_op->i);		\
	} else {					\
		THE_r = THE_op->r;			\
		THE_i = THE_op->i;			\
	}						\
}
#define BREAK_CPLX_OPS(THE_op,THE_r,THE_i) {		\
	if(rop==THE_op) {				\
		mpwl_free(THE_i,TRUE);			\
		mpwl_free(THE_r,TRUE);			\
	}						\
}


#define GET_NEW_REAL(n) {				\
	if(!free_reals) {				\
		n = g_new0(MpwRealNum,1);		\
	} else {					\
		n = free_reals;				\
		free_reals = free_reals->alloc.next;	\
		free_reals_n--;				\
	}						\
}
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
#define MAKE_REAL(n) {					\
	if ((n)->type == MPW_COMPLEX) {			\
		(n)->type = MPW_REAL;			\
		if((n)->i != zero) {			\
			(n)->i->alloc.usage--;		\
			if((n)->i->alloc.usage==0)	\
				mpwl_free((n)->i,FALSE);\
			(n)->i = zero;			\
			zero->alloc.usage++;		\
		}					\
	}						\
}
#define MAKE_IMAG(n) {					\
	(n)->type = MPW_COMPLEX;			\
	if((n)->r != zero) {				\
		(n)->r->alloc.usage--;			\
		if((n)->r->alloc.usage==0)		\
			mpwl_free((n)->r,FALSE);	\
		(n)->r = zero;				\
		zero->alloc.usage++;			\
	}						\
}

/*************************************************************************/
/*low level stuff prototypes                                             */
/*************************************************************************/

/*my own power function for floats, very simple :) */
static void mympfr_pow_z(mpf_t rop,mpf_t op,mpz_t e);

/*my own power function for ints, very simple :) */
static void mympz_pow_z(mpz_t rop,mpz_t op,mpz_t e);

static gboolean mympq_perfect_square_p (mpq_t op);

static void mympq_set_fr (mpq_ptr q, mpfr_srcptr fr);

/*clear extra variables of type type, if type=op->type nothing is done*/
static void mpwl_clear_extra_type(MpwRealNum *op,int type);

/*only set the type, don't free it, and don't set the type variable
  create an extra type of the variable for temporary use*/
static void mpwl_make_extra_type (MpwRealNum *op, int type);
static void mpwl_make_extra_type_no_convert (MpwRealNum *op, int type);

static void mpwl_make_type(MpwRealNum *op,int type);

/*this only adds a value of that type but does nto clear the old one!
  retuns the new extra type set*/
static int mpwl_make_same_extra_type(MpwRealNum *op1,MpwRealNum *op2);
/*
static int mpwl_make_same_extra_type_3(MpwRealNum *op1,MpwRealNum *op2);
*/

/*make new type and clear the old one*/
static void mpwl_make_same_type(MpwRealNum *op1,MpwRealNum *op2);

static void mpwl_clear(MpwRealNum *op);

static void mpwl_init_type(MpwRealNum *op,int type);

static void mpwl_free(MpwRealNum *op, int local);

static int mpwl_sgn(MpwRealNum *op);

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

static void mpwl_dblfac_ui (MpwRealNum *rop, unsigned int op);
static void mpwl_dblfac (MpwRealNum *rop, MpwRealNum *op);

static gboolean mpwl_pow_q(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

/*power to an unsigned long and optionaly invert the answer*/
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
static void mpwl_pi (MpwRealNum *rop);
static void mpwl_ln2 (MpwRealNum *rop);
static void mpwl_euler_constant (MpwRealNum *rop);
static void mpwl_rand (MpwRealNum *rop);
static void mpwl_randint (MpwRealNum *rop, MpwRealNum *op);

static int mpwl_cmp(MpwRealNum *op1, MpwRealNum *op2);

static int mpwl_cmp_ui(MpwRealNum *op, unsigned long int i);

static void mpwl_make_int(MpwRealNum *rop);

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
static double mpwl_get_double(MpwRealNum *op, int *ex);

/*round off the number at some digits*/
static void str_make_max_digits (char *s, int digits, long *exponent);
/*trim trailing zeros*/
static void str_trim_trailing_zeros(char *s);

/*formats a floating point with mantissa in p and exponent in e*/
static char * str_format_float(char *p,long int e,int scientific_notation);

static char * str_getstring_z(mpz_t num, int max_digits,
			      gboolean scientific_notation,
			      int integer_output_base,
			      const char *postfix);
static char * str_getstring_q(mpq_t num, int max_digits,
			      gboolean scientific_notation,
			      gboolean mixed_fractions,
			      GelOutputStyle style,
			      const char *postfix);
static char * str_getstring_f(mpf_t num, int max_digits,
			      gboolean scientific_notation,
			      const char *postfix);

static char * mpwl_getstring(MpwRealNum * num, int max_digits,
			     gboolean scientific_notation,
			     gboolean results_as_floats,
			     gboolean mixed_fractions,
			     GelOutputStyle style,
			     int integer_output_base,
			     const char *postfix);

#define mpwl_getstring_for_error(n) \
		mpwl_getstring ((n),					\
				12 /* max_digits */,			\
				FALSE /* scientific_notation */,	\
				FALSE /* results_as_floats */,		\
				FALSE /* mixed_fractions */,		\
				GEL_OUTPUT_NORMAL,			\
				10 /* integer_output_base */,		\
				"" /* postfix */);

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

/*my own power function for floats, very simple :) */
static void
mympfr_pow_z(mpfr_t rop,mpfr_t op,mpz_t e)
{
	int esgn = mpz_sgn (e);
	gboolean neg = FALSE;
	if (esgn == 0) {
		mpfr_set_ui (rop, 1, GMP_RNDN);
		return;
	} else if (esgn < 0) {
		neg = TRUE;
		mpz_neg (e, e);
	}

	if (mpz_fits_ulong_p (e)) {
		unsigned int exp = mpz_get_ui (e);
		mpfr_pow_ui (rop, op, exp, GMP_RNDN);
		if (neg) {
			mpfr_ui_div (rop, 1, rop, GMP_RNDN);
			mpz_neg (e, e);
		}
	} else {
		mpfr_t fe;

		/* we don't need a negative here */
		if (neg) {
			mpz_neg (e, e);
		}

		mpfr_init (fe);
		mpfr_set_z (fe, e, GMP_RNDN);

		mpfr_pow (rop, op, fe, GMP_RNDN);

		mpfr_clear (fe);
	}
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
			error_num=NUMERICAL_MPW_ERROR;
			mpz_set_ui (rop, 1);
		}
	}
}

/*clear extra variables of type type, if type=op->type nothing is done*/
static void
mpwl_clear_extra_type(MpwRealNum *op,int type)
{
	if(op->type==type)
		return;
	switch(type) {
	case MPW_INTEGER:
		if(op->data.ival) {
			CLEAR_FREE_MPZ (op->data.ival);
			op->data.ival = NULL;
		}
		break;
	case MPW_RATIONAL:
		if(op->data.rval) {
			CLEAR_FREE_MPQ (op->data.rval);
			op->data.rval = NULL;
		}
		break;
	case MPW_FLOAT:
		if(op->data.fval) {
			CLEAR_FREE_MPF (op->data.fval);
			op->data.fval = NULL;
		}
		break;
	}
}

static gboolean
mympq_perfect_square_p (mpq_t op)
{
	return mpz_perfect_square_p (mpq_numref(op)) &&
		mpz_perfect_square_p (mpq_denref(op));
}

/*only set the type, don't free it, and don't set the type variable
  create an extra type of the variable for temporary use*/
static void
mpwl_make_extra_type(MpwRealNum *op,int type)
{
	if(op->type==type)
		return;
	switch(type) {
	case MPW_INTEGER:
		if (op->data.ival == NULL) {
			GET_INIT_MPZ (op->data.ival);
			if(op->type==MPW_FLOAT)
				mpz_set_fr (op->data.ival,
					    op->data.fval,
					    GMP_RNDN);
			else /* if(op->type==MPW_RATIONAL) */
				mpz_set_q(op->data.ival,op->data.rval);
		}
		break;
	case MPW_RATIONAL:
		if (op->data.rval == NULL) {
			GET_INIT_MPQ (op->data.rval);
			if(op->type==MPW_INTEGER)
				mpq_set_z(op->data.rval,op->data.ival);
			else /* if(op->type==MPW_FLOAT) */
				mympq_set_fr (op->data.rval,op->data.fval);
		}
		break;
	case MPW_FLOAT:
		if (op->data.fval == NULL) {
			GET_INIT_MPF (op->data.fval);
			if(op->type==MPW_INTEGER) {
				mpf_set_z(op->data.fval,op->data.ival);
			} else /* if(op->type==MPW_RATIONAL) */ {
				mpf_set_q(op->data.fval,op->data.rval);
				/* XXX: a hack!!
				 * get around a mpf_set_q bug*/
				if(mpq_sgn(op->data.rval)<0 &&
				   mpf_sgn(op->data.fval)>0) {
					mpf_neg(op->data.fval,op->data.fval);
				}
			}
		}
		break;
	}
}

/*only set the type, don't free it, and don't set the type variable
  create an extra type of the variable for temporary use*/

static void
mpwl_make_extra_type_no_convert (MpwRealNum *op, int type)
{
	if(op->type==type)
		return;
	switch(type) {
	case MPW_INTEGER:
		if (op->data.ival == NULL) {
			GET_INIT_MPZ (op->data.ival);
		}
		break;
	case MPW_RATIONAL:
		if (op->data.rval == NULL) {
			GET_INIT_MPQ (op->data.rval);
		}
		break;
	case MPW_FLOAT:
		if (op->data.fval == NULL) {
			GET_INIT_MPF (op->data.fval);
		}
		break;
	}
}

static void
mpwl_make_type(MpwRealNum *op,int type)
{
	int t;

	if(op->type==type)
		return;
	t=op->type;
	mpwl_make_extra_type(op,type);
	op->type=type;
	mpwl_clear_extra_type(op,t);
}

/*this only adds a value of that type but does nto clear the old one!
  retuns the new extra type set*/
static int
mpwl_make_same_extra_type (MpwRealNum *op1,MpwRealNum *op2)
{
	if (op1->type == op2->type) {
		return op1->type;
	} else if (op1->type > op2->type) {
		mpwl_make_extra_type (op2, op1->type);
		return op1->type;
	} else { /*if(op1->type < op2->type)*/
		mpwl_make_extra_type (op1, op2->type);
		return op2->type;
	}
}

#if 0
/*this only adds a value of that type but does nto clear the old one!
  retuns the new extra type set*/
static int
mpwl_make_same_extra_type_3 (MpwRealNum *op1, MpwRealNum *op2, MpwRealNum *op3)
{
	int maxtype = MAX (MAX (op1->type, op2->type), op3->type);
	if (op1->type < maxtype)
		mpwl_make_extra_type (op1, maxtype);
	if (op2->type < maxtype)
		mpwl_make_extra_type (op2, maxtype);
	if (op3->type < maxtype)
		mpwl_make_extra_type (op3, maxtype);
	return maxtype;
}
#endif

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
mpwl_clear(MpwRealNum *op)
{
	if(!op) return;

	if(op->data.ival) {
		CLEAR_FREE_MPZ (op->data.ival);
		op->data.ival = NULL;
	}
	if(op->data.rval) {
		CLEAR_FREE_MPQ (op->data.rval);
		op->data.rval = NULL;
	}
	if(op->data.fval) {
		CLEAR_FREE_MPF (op->data.fval);
		op->data.fval = NULL;
	}
}

static void
mpwl_init_type(MpwRealNum *op,int type)
{
	if(!op) return;

	op->type=type;

	switch(type) {
	case MPW_INTEGER:
		if (op->data.ival == NULL) {
			GET_INIT_MPZ (op->data.ival);
		}
		break;
	case MPW_RATIONAL:
		if (op->data.rval == NULL) {
			GET_INIT_MPQ (op->data.rval);
		}
		break;
	case MPW_FLOAT:
		if (op->data.fval == NULL) {
			GET_INIT_MPF (op->data.fval);
		}
		break;
	default: ;
	}
}

static void
mpwl_free(MpwRealNum *op, gboolean local)
{
	if(!op) return;
	mpwl_clear(op);
	if(local) return;
	/*FIXME: the 2000 should be settable*/
	/*if we want to store this so that we don't allocate new one
	  each time, up to a limit of 2000, unless it was some local
	  var in which case it can't be freed nor put on the free
	  stack*/
	if(free_reals_n>2000) {
		g_free(op);
	} else {
		op->alloc.next = free_reals;
		free_reals = op;
		free_reals_n++;
	}
}

static int
mpwl_sgn(MpwRealNum *op)
{
	switch(op->type) {
	case MPW_FLOAT: return mpf_sgn(op->data.fval);
	case MPW_RATIONAL: return mpq_sgn(op->data.rval);
	case MPW_INTEGER: return mpz_sgn(op->data.ival);
	}
	return 0;
}

static int
mpwl_cmp(MpwRealNum *op1, MpwRealNum *op2)
{
	int r=0;
	int t;

	t=mpwl_make_same_extra_type(op1,op2);
	switch(t) {
	case MPW_FLOAT:
		r=mpf_cmp(op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		r=mpq_cmp(op1->data.rval,op2->data.rval);
		break;
	case MPW_INTEGER:
		r=mpz_cmp(op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	return r;
}

static int
mpwl_cmp_ui(MpwRealNum *op, unsigned long int i)
{
	switch(op->type) {
	case MPW_FLOAT: return mpf_cmp_ui(op->data.fval,i);
	case MPW_RATIONAL: return mpq_cmp_ui(op->data.rval,i,1);
	case MPW_INTEGER: return mpz_cmp_ui(op->data.ival,i);
	}
	return 0;
}

static void
mpwl_set_d(MpwRealNum *rop,double d)
{
	switch(rop->type) {
	case MPW_FLOAT:
		mpf_set_d(rop->data.fval,d);
		break;
	case MPW_RATIONAL:
	case MPW_INTEGER:
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
		mpf_set_d(rop->data.fval,d);
		break;
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
	}
}

/*the original op should be a local or not be used anymore*/
static void
mpwl_move(MpwRealNum *rop,MpwRealNum *op)
{
	if (rop == op)
		return;
	
	if (rop->data.ival != NULL) {
		CLEAR_FREE_MPZ (rop->data.ival);
	}
	if (rop->data.rval != NULL) {
		CLEAR_FREE_MPQ (rop->data.rval);
	}
	if (rop->data.fval != NULL) {
		CLEAR_FREE_MPF (rop->data.fval);
	}
	memcpy(rop,op,sizeof(MpwRealNum));
	rop->alloc.usage=1;
	/* not necessary 
	op->type = 0;
	op->data.ival = NULL;
	op->data.rval = NULL;
	op->data.fval = NULL;*/
}

static void
mpwl_set(MpwRealNum *rop,MpwRealNum *op)
{
	if(rop==op)
		return;
	else if(rop->type==op->type) {
		switch(op->type) {
		case MPW_FLOAT:
			mpf_set(rop->data.fval,op->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_set(rop->data.rval,op->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_set(rop->data.ival,op->data.ival);
			break;
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
	
	/*special case*/
	if(op1->type==op2->type && rop!=op1 && rop!=op2) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpf_add(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_add(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_add(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	mpwl_init_type(&r,t);

	switch(t) {
	case MPW_FLOAT:
		mpf_add(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpq_add(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpz_add(r.data.ival,op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
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
		mpf_add_ui(rop->data.fval,op->data.fval,i);
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
	}
}

static void
mpwl_sub(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	/*special case*/
	if(op1->type==op2->type && rop!=op1 && rop!=op2) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpf_sub(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_sub(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_sub(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	mpwl_init_type(&r,t);

	switch(t) {
	case MPW_FLOAT:
		mpf_sub(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpq_sub(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpz_sub(r.data.ival,op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
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
		mpf_sub_ui(rop->data.fval,op->data.fval,i);
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
		mpf_ui_sub(rop->data.fval,i,op->data.fval);
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
	}
}

static void
mpwl_mul(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	/*special case*/
	if(op1->type==op2->type && rop!=op1 && rop!=op2) {
		switch(op1->type) {
		case MPW_FLOAT:
			if(rop->type != MPW_FLOAT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_FLOAT);
			}
			mpf_mul(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_mul(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			if(rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			mpz_mul(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	switch(t) {
	case MPW_FLOAT:
		mpwl_init_type(&r,t);
		mpf_mul(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpwl_init_type(&r,t);
		mpq_mul(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpwl_init_type(&r,t);
		mpz_mul(r.data.ival,op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
}

static void
mpwl_mul_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i)
{
	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_mul_ui(rop->data.fval,op->data.fval,i);
		break;
	case MPW_RATIONAL:
		if(rop->type!=MPW_RATIONAL) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_RATIONAL);
		}
		mpz_mul_ui(mpq_numref(rop->data.rval),
			   mpq_numref(op->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		if(rop->type!=MPW_INTEGER) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_INTEGER);
		}
		mpz_mul_ui(rop->data.ival,op->data.ival,i);
		break;
	}
}

static void
mpwl_div(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY (mpwl_sgn(op2)==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	/*special case*/
	if(op1->type > MPW_INTEGER && op1->type==op2->type && rop!=op2) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpf_div(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_div(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		default: ;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	switch(t) {
	case MPW_FLOAT:
		mpwl_init_type(&r,t);
		mpf_div(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpwl_init_type(&r,t);
		mpq_div(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpwl_init_type(&r,MPW_RATIONAL);
		mpq_set_z(r.data.rval,op1->data.ival);
		mpz_set(mpq_denref(r.data.rval),
			op2->data.ival);
		mpwl_make_int(&r);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
}

static void
mpwl_div_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i)
{
	int t;
	if G_UNLIKELY (i==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_div_ui(rop->data.fval,op->data.fval,i);
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
		t = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_RATIONAL);
		rop->type = MPW_RATIONAL;
		mpq_set_z(rop->data.rval,op->data.ival);
		mpz_set_ui(mpq_denref(rop->data.rval),i);
		mpwl_clear_extra_type(rop,t);
		mpwl_make_int (rop);
		break;
	}
}

static void
mpwl_ui_div(MpwRealNum *rop,unsigned long int i,MpwRealNum *op)
{
	int t;
	if G_UNLIKELY (mpwl_sgn(op)==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_ui_div(rop->data.fval,i,op->data.fval);
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
		t = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_RATIONAL);
		rop->type = MPW_RATIONAL;
		mpz_set_ui(mpq_numref(rop->data.rval),i);
		mpz_set(mpq_denref(rop->data.rval),op->data.ival);
		mpwl_clear_extra_type(rop,t);
		mpwl_make_int (rop);
		break;
	}
}

static void
mpwl_mod(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_UNLIKELY (mpwl_sgn(op2)==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		int t1;

		t1 = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_INTEGER);
		rop->type = MPW_INTEGER;
		mpz_mod(rop->data.ival,op1->data.ival,op2->data.ival);
		mpwl_clear_extra_type(rop,t1);
	} else {
		gel_errorout (_("Can't do modulo of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_gcd(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		int t1;

		t1 = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_INTEGER);
		rop->type = MPW_INTEGER;
		mpz_gcd(rop->data.ival,op1->data.ival,op2->data.ival);
		mpwl_clear_extra_type(rop,t1);
	} else {
		gel_errorout (_("Can't do GCD of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static gboolean
mpwl_invert (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	if G_LIKELY (op1->type == MPW_INTEGER && op2->type == MPW_INTEGER) {
		gboolean suc = FALSE;
		mpz_ptr ret;

		GET_INIT_MPZ (ret);

		suc = mpz_invert (ret, op1->data.ival, op2->data.ival);

		if (suc) {
			mpwl_clear (rop);
			rop->type = MPW_INTEGER;
			rop->data.ival = ret;
		} else {
			CLEAR_FREE_MPZ (ret);
		}

		return suc;
	} else {
		gel_errorout (_("Can't modulo invert non integers!"));
		error_num=NUMERICAL_MPW_ERROR;

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
		gel_errorout (_("Can't get jacobi symbols of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
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
		gel_errorout (_("Can't get legendre symbols of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
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
		gel_errorout (_("Can't get jacobi symbol with Kronecker extension of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_lucnum (MpwRealNum *rop, MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER) {
		gel_errorout (_("Lucas must get an integer argument!"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	if G_UNLIKELY (mpz_cmp_ui(op->data.ival,G_MAXULONG)>0) {
		gel_errorout (_("Number too large to compute lucas number!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
		gel_errorout (_("No such thing as negative lucas numbers!"));
		error_num=NUMERICAL_MPW_ERROR;
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
		error_num = NUMERICAL_MPW_ERROR;
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
		error_num=NUMERICAL_MPW_ERROR;
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
		error_num=NUMERICAL_MPW_ERROR;
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
		error_num=NUMERICAL_MPW_ERROR;
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
		error_num=NUMERICAL_MPW_ERROR;
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
		mpf_neg(rop->data.fval,op->data.fval);
		break;
	case MPW_RATIONAL:
		mpq_neg(rop->data.rval,op->data.rval);
		break;
	case MPW_INTEGER:
		mpz_neg(rop->data.ival,op->data.ival);
		break;
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
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	if G_UNLIKELY (mpz_cmp_ui(op->data.ival,G_MAXULONG)>0) {
		gel_errorout (_("Number too large to compute factorial!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
		gel_errorout (_("Can't do factorials of negative numbers!"));
		error_num=NUMERICAL_MPW_ERROR;
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
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	if G_UNLIKELY (mpz_cmp_ui(op->data.ival,G_MAXULONG)>0) {
		gel_errorout (_("Number too large to compute factorial!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
		gel_errorout (_("Can't do factorials of negative numbers!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	mpwl_dblfac_ui(rop,mpz_get_ui(op->data.ival));
}

static gboolean
mpwl_pow_q(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	mpf_t fr;
	mpf_t fr2;
	mpf_t frt;
	mpf_t de;
	mpz_t des;
	unsigned long int den;
	int t;
	gboolean reverse = FALSE;

	if G_UNLIKELY (op2->type!=MPW_RATIONAL) {
		error_num=INTERNAL_MPW_ERROR;
		return FALSE;
	}

	if G_UNLIKELY (mpwl_sgn (op1) < 0 &&
			mpz_even_p (mpq_denref(op2->data.rval))) {
		/*it's gonna be complex*/
		error_num=NUMERICAL_MPW_ERROR;
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
			mpz_ptr z;

			GET_INIT_MPZ (z);

			if (mpz_root (z, op1->data.ival, den) != 0) {
				mympz_pow_z (z, z,
					     mpq_numref (op2->data.rval));
				mpwl_clear (rop);
				rop->type = MPW_INTEGER;
				rop->data.ival = z;

				return FALSE;
			}
			CLEAR_FREE_MPZ (z);
		} else if (op1->type == MPW_RATIONAL) {
			mpq_ptr q;
			GET_INIT_MPQ (q);

			if (mpz_root (mpq_numref (q),
				      mpq_numref (op1->data.rval),
				      den) != 0 &&
			    mpz_root (mpq_denref (q),
				      mpq_denref (op1->data.rval),
				      den) != 0) {
				mympz_pow_z (mpq_numref (q), mpq_numref (q),
					     mpq_numref (op2->data.rval));
				mympz_pow_z (mpq_denref (q), mpq_denref (q),
					     mpq_numref (op2->data.rval));
				mpwl_clear (rop);
				rop->type = MPW_RATIONAL;
				rop->data.rval = q;
				mpq_canonicalize (rop->data.rval);

				return FALSE;
			}
			CLEAR_FREE_MPQ (q);
		}
	}

	mpz_init_set(des,mpq_denref(op2->data.rval));
	mpz_sub_ui(des,des,1);

	mpf_init(de);
	mpf_set_z(de,mpq_denref(op2->data.rval));


	if(mpq_sgn(op2->data.rval)<0)
		reverse=TRUE;

	mpwl_make_extra_type(op1,MPW_FLOAT);
	t=MPW_FLOAT;


	/*
	 * Newton's method: Xn+1 = Xn - f(Xn)/f'(Xn)
	 */
	
	mpf_init(fr);
	mpf_init(fr2);
	mpf_init(frt);
	mpf_div_ui(fr,op1->data.fval,2); /*use half the value
					     as an initial guess*/
	for(;;) {
		mympfr_pow_z(fr2,fr,mpq_denref(op2->data.rval));
		mpf_sub(fr2,fr2,op1->data.fval);

		mympfr_pow_z(frt,fr,des);
		mpf_mul(frt,frt,de);
		mpf_div(fr2,fr2,frt);
		mpf_neg(fr2,fr2);
		mpf_add(fr2,fr2,fr);

		
		if(mpf_cmp(fr2,fr)==0)
			break;
		mpf_set(fr,fr2);
	}
	mpf_clear(fr2);
	mpf_clear(frt);
	mpz_clear(des);
	mpf_clear(de);

	if(reverse) {
		/*numerator will be negative*/
		mpz_neg(mpq_numref(op2->data.rval),mpq_numref(op2->data.rval));
		mympfr_pow_z(fr,fr,mpq_numref(op2->data.rval));
		mpz_neg(mpq_numref(op2->data.rval),mpq_numref(op2->data.rval));

		mpf_ui_div(fr,1,fr);
	} else
		mympfr_pow_z(fr,fr,mpq_numref(op2->data.rval));

	/*op1 might have equaled rop so clear extra type here*/
	mpwl_clear_extra_type(op1,t);

	mpwl_clear(rop);
	mpwl_init_type(rop,MPW_FLOAT);
	mpf_set(rop->data.fval,fr);
	return FALSE;
}

/*power to an unsigned long and optionaly invert the answer*/
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
		mpf_pow_ui (r.data.fval, op1->data.fval, e);

		if(reverse)
			mpf_ui_div(r.data.fval,1,r.data.fval);
		break;
	}
	mpwl_move(rop,&r);
}

static void
mpwl_pow_z(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	gboolean reverse = FALSE;;
	if G_UNLIKELY (op2->type!=MPW_INTEGER) {
		error_num=INTERNAL_MPW_ERROR;
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
			mympfr_pow_z(r.data.fval,op1->data.fval,
				    op2->data.ival);

			if(reverse)
				mpf_ui_div(r.data.fval,1,r.data.fval);
			break;
		}
		mpwl_move(rop,&r);
	} else {
		if(mpz_sgn(op2->data.ival)==0)
			mpwl_set_ui(rop,1);
		else 
			mpwl_pow_ui(rop,op1,mpz_get_ui(op2->data.ival),reverse);
	}

	if(reverse)
		mpz_neg(op2->data.ival,op2->data.ival);
}

static gboolean
mpwl_pow_f(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY (op2->type!=MPW_FLOAT) {
		error_num=INTERNAL_MPW_ERROR;
		return FALSE;
	}
	if(mpwl_sgn(op1)<=0)
		return TRUE;
	
	mpwl_make_extra_type(op1,MPW_FLOAT);
	
	mpwl_init_type(&r,MPW_FLOAT);

	mpfr_pow (r.data.fval, op1->data.fval, op2->data.fval, GMP_RNDN);

	mpwl_clear_extra_type(op1,MPW_FLOAT);

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
		error_num = NUMERICAL_MPW_ERROR;
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
				char *n1 = mpwl_getstring_for_error (&r);
				char *n2 = mpwl_getstring_for_error (mod);
				gel_errorout (_("Can't invert %s modulo %s "
					       "in %s"),
					      n1, n2, "powm");
				g_free (n1);
				g_free (n2);
				error_num = NUMERICAL_MPW_ERROR;
				mpwl_clear (&r);
				return;
			}
		}
		break;
	case MPW_FLOAT: 
	case MPW_RATIONAL:
		g_assert_not_reached ();
		break;
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
		error_num = NUMERICAL_MPW_ERROR;
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
		mpwl_init_type (&r, MPW_FLOAT);
		mpwl_make_extra_type (op, MPW_FLOAT);
		mpf_sqrt (r.data.fval, op->data.fval);
		mpwl_clear_extra_type (op,MPW_FLOAT);
	}
	if (is_complex)
		mpwl_neg (op, op);

	mpwl_move (rop, &r);
	return is_complex;
}

static void
mpwl_exp(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	mpfr_exp (r.data.fval, op->data.fval, GMP_RNDN);
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}

static gboolean
mpwl_ln(MpwRealNum *rop,MpwRealNum *op)
{
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	if (mpfr_sgn (op->data.fval) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op->data.fval, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log (r.data.fval, op->data.fval, GMP_RNDN);
		ret = TRUE;
	}
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
	
	return ret;
}

static gboolean
mpwl_log2(MpwRealNum *rop,MpwRealNum *op)
{
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	if (mpfr_sgn (op->data.fval) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op->data.fval, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log2 (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log2 (r.data.fval, op->data.fval, GMP_RNDN);
		ret = TRUE;
	}
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
	
	return ret;
}

static gboolean
mpwl_log10(MpwRealNum *rop,MpwRealNum *op)
{
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	if (mpfr_sgn (op->data.fval) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op->data.fval, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log10 (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log10 (r.data.fval, op->data.fval, GMP_RNDN);
		ret = TRUE;
	}
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
	
	return ret;
}

static void
mpwl_cos(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	mpfr_cos (r.data.fval, op->data.fval, GMP_RNDN);
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_sin(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	mpfr_sin (r.data.fval, op->data.fval, GMP_RNDN);
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_cosh(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	mpfr_cosh (r.data.fval, op->data.fval, GMP_RNDN);
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_sinh(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	mpfr_sinh (r.data.fval, op->data.fval, GMP_RNDN);
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_arctan(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
	mpfr_atan (r.data.fval, op->data.fval, GMP_RNDN);
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}

static void
mpwl_pi (MpwRealNum *rop)
{
	mpwl_clear(rop);
	mpwl_init_type(rop,MPW_FLOAT);
	mpfr_const_pi (rop->data.fval, GMP_RNDN);
}

static void
mpwl_ln2 (MpwRealNum *rop)
{
	mpwl_clear(rop);
	mpwl_init_type(rop,MPW_FLOAT);
	mpfr_const_log2 (rop->data.fval, GMP_RNDN);
}

static void
mpwl_euler_constant (MpwRealNum *rop)
{
	mpwl_clear (rop);
	mpwl_init_type (rop, MPW_FLOAT);
	mpfr_const_euler (rop->data.fval, GMP_RNDN);
}

/* Random state stuff: FIXME: this is evil */
/* static unsigned long randstate_seed = 0; */
static gmp_randstate_t rand_state;
static gboolean rand_state_inited = FALSE;

static inline void
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

	mpwl_clear (rop);
	mpwl_init_type (rop, MPW_FLOAT);

	mpf_urandomb (rop->data.fval, rand_state, default_mpf_prec);
	if G_UNLIKELY (mpf_sgn (rop->data.fval) < 0) {
		/* FIXME: GMP/MPFR bug */
		mpf_neg (rop->data.fval, rop->data.fval);
		/* FIXME: WHAT THE HELL IS GOING ON! */
		if (mpf_cmp_ui (rop->data.fval, 1L) > 0) {
			gel_errorout ("Can't recover from a GMP problem.  Random function "
				      "is not returning values in [0,1)");
		}
	}
}

static void
mpwl_randint (MpwRealNum *rop, MpwRealNum *op)
{
	long range;
	int ex;

	init_randstate();

	if G_UNLIKELY (op->type != MPW_INTEGER) {
		gel_errorout (_("Can't make random integer from a non-integer"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpwl_sgn (op) <= 0) {
		gel_errorout (_("Range for random integer must be positive"));
		error_num = NUMERICAL_MPW_ERROR;
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

	if (op == rop) {
		mpz_t z;
		mpz_init_set (z, op->data.ival);
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_INTEGER);
		mpz_urandomm (rop->data.ival, rand_state, z);
		mpz_clear (z);
	} else {
		mpwl_clear (rop);
		mpwl_init_type (rop, MPW_INTEGER);
		mpz_urandomm (rop->data.ival, rand_state, op->data.ival);
	}
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
				if (rop->data.ival == NULL) {
					GET_INIT_MPZ (rop->data.ival);
				}
				mpz_set (rop->data.ival,
					 mpq_numref (rop->data.rval));
				CLEAR_FREE_MPQ (rop->data.rval);
				rop->data.rval = NULL;
				rop->type=MPW_INTEGER;
			}
			break;
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
			rop->type=MPW_INTEGER;
			if (rop->data.ival == NULL) {
				GET_INIT_MPZ (rop->data.ival);
			}
			mpz_set_fr (rop->data.ival, rop->data.fval, GMP_RNDN);
			CLEAR_FREE_MPF (rop->data.fval);
			rop->data.fval = NULL;
		} else /*MPW_RATIONAL*/ {
			mpq_ptr tmp;
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
			rop->type=MPW_INTEGER;
			if (rop->data.ival == NULL) {
				GET_INIT_MPZ (rop->data.ival);
			}
			mpz_set_fr (rop->data.ival, rop->data.fval, GMP_RNDU);
			CLEAR_FREE_MPF (rop->data.fval);
			rop->data.fval = NULL;
		} else /*MPW_RATIONAL*/ {
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				rop->type=MPW_INTEGER;
				if (rop->data.ival == NULL) {
					GET_INIT_MPZ (rop->data.ival);
				}
				mpz_set (rop->data.ival,
					 mpq_numref (rop->data.rval));
				CLEAR_FREE_MPQ (rop->data.rval);
				rop->data.rval = NULL;
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
			rop->type=MPW_INTEGER;
			if (rop->data.ival == NULL) {
				GET_INIT_MPZ (rop->data.ival);
			}
			mpz_set_fr (rop->data.ival, rop->data.fval, GMP_RNDD);
			CLEAR_FREE_MPF (rop->data.fval);
			rop->data.fval = NULL;
		} else /*MPW_RATIONAL*/ {
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				rop->type=MPW_INTEGER;
				if (rop->data.ival == NULL) {
					GET_INIT_MPZ (rop->data.ival);
				}
				mpz_set (rop->data.ival,
					 mpq_numref (rop->data.rval));
				CLEAR_FREE_MPQ (rop->data.rval);
				rop->data.rval = NULL;
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
		rop->type = MPW_INTEGER;
		if (rop->data.ival == NULL) {
			GET_INIT_MPZ (rop->data.ival);
		}
		mpz_set_fr (rop->data.ival, rop->data.fval, GMP_RNDZ);
		CLEAR_FREE_MPF (rop->data.fval);
		rop->data.fval = NULL;
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
		error_num=NUMERICAL_MPW_ERROR;
	} else { /* must be rational */
		if(rop != op) {
			mpwl_clear(rop);
			mpwl_init_type(rop, MPW_INTEGER);
			mpz_set(rop->data.ival, mpq_numref(op->data.rval));
		} else {
			if (rop->data.ival == NULL) {
				GET_INIT_MPZ (rop->data.ival);
			}
			mpz_set (rop->data.ival,
				 mpq_numref(rop->data.rval));
			CLEAR_FREE_MPQ (rop->data.rval);
			rop->data.rval = NULL;
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
		error_num=NUMERICAL_MPW_ERROR;
	} else { /* must be rational */
		if(rop != op) {
			mpwl_clear(rop);
			mpwl_init_type(rop, MPW_INTEGER);
			mpz_set(rop->data.ival, mpq_denref(op->data.rval));
		} else {
			if (rop->data.ival == NULL) {
				GET_INIT_MPZ (rop->data.ival);
			}
			mpz_set (rop->data.ival,
				 mpq_denref(rop->data.rval));
			CLEAR_FREE_MPQ (rop->data.rval);
			rop->data.rval = NULL;
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
	mpf_set_str(rop->data.fval,s,base);

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
	mpz_set_str(rop->data.ival,s,base);
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

/*get a double if possible*/
static double
mpwl_get_double (MpwRealNum *op, int *ex)
{
	double d;

	mpwl_make_extra_type (op, MPW_FLOAT);

	if G_UNLIKELY (mpfr_cmp_d (op->data.fval, G_MAXDOUBLE) > 0 ||
		       mpfr_cmp_d (op->data.fval, -G_MAXDOUBLE) < 0) {
		*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
		return 0;
	}

	d = mpf_get_d (op->data.fval);

	mpwl_clear_extra_type (op, MPW_FLOAT);

	return d;
}

/*round off the number at some digits*/
static void
str_make_max_digits (char *s, int digits, long *exponent)
{
	int i;
	int sd=0; /*digit where the number starts*/

	if(s[0]=='-')
		sd=1;

	if(!s || digits<=0)
		return;

	digits+=sd;

	if(strlen(s)<=digits)
		return;

	if(s[digits]<'5') {
		s[digits]='\0';
		return;
	}
	s[digits]='\0';

	for(i=digits-1;i>=sd;i--) {
		if(s[i]<'9') {
			s[i]++;
			return;
		}
		s[i]='\0';
	}
	shiftstr (s, 1);
	s[sd]='1';
	/* if we add a digit in front increase exponent */
	(*exponent) ++;
}

/*trim trailing zeros*/
static void
str_trim_trailing_zeros(char *s)
{
	char *p,*pp;

	p = strrchr(s,'.');
	if(!p) return;
	for(pp=p+1;*pp;pp++) {
		if(*pp!='0')
			p = pp+1;
	}
	*p = '\0';
}

/*formats a floating point with mantissa in p and exponent in e*/
static char *
str_format_float(char *p,long int e,int scientific_notation)
{
	long int len;
	int i;
	if(((e-1)<-8 || (e-1)>8) || scientific_notation) {
		if (e != 0)
			p = g_realloc (p, strlen(p)+1+((int)log10(abs(e))+2)+1);
		else
			p = g_realloc (p, strlen(p) + 3);
			
		if(p[0]=='-') {
			if(strlen(p)>2) {
				shiftstr(p+2,1);
				p[2]='.';
			}
		} else {
			if(strlen(p)>1) {
				shiftstr(p+1,1);
				p[1]='.';
			}
		}
		str_trim_trailing_zeros(p);
		/* look above to see why this is one sprintf which is in
		   fact safe */
		sprintf(p,"%se%ld",p,e-1);
	} else if(e>0) {
		len=strlen(p);
		if(p[0]=='-')
			len--;
		if(e>len) {
			p = g_realloc (p, strlen(p)+1+e-len);
			for(i=0;i<e-len;i++)
				strcat(p,"0");
		} else if(e<len) {
			if(p[0]=='-') {
				shiftstr(p+1+e,1);
				p[e+1]='.';
			} else {
				shiftstr(p+e,1);
				p[e]='.';
			}
		}
		str_trim_trailing_zeros(p);
	} else { /*e<=0*/
		if(strlen(p)==0) {
			p = g_strdup ("0");
		} else {
			p = g_realloc (p, strlen(p)+1+(-e)+2);
			if(p[0]=='-') {
				shiftstr(p+1,2+(-e));
				p[1]='0';
				p[2]='.';
				for(i=0;i<(-e);i++)
					p[i+3]='0';
			} else {
				shiftstr(p,2+(-e));
				p[0]='0';
				p[1]='.';
				for(i=0;i<(-e);i++)
					p[i+2]='0';
			}
		}
		str_trim_trailing_zeros(p);
	}
	return p;
}

static char *
str_getstring_z(mpz_t num, int max_digits,int scientific_notation,
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
	if(max_digits>0 && max_digits<strlen(p)) {
		mpfr_init(fr);
		mpfr_set_z(fr,num, GMP_RNDN);
		p2=str_getstring_f(fr,max_digits,scientific_notation,postfix);
		mpf_clear(fr);
		if(strlen(p2)>=strlen(p)) {
			g_free(p2);
			return p;
		} else  {
			g_free(p);
			return p2;
		}
	}
	p=appendstr(p,postfix);
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
		p=prependstr(p,"\\frac{");
		p=appendstr(p,"}{");
		p2=mpz_get_str(NULL,10,den);
		p=appendstr(p,p2);
		l = strlen (p2);
		if (l > digits)
			digits = l;
		g_free(p2);
		p=appendstr(p,"}");
		p=appendstr(p,postfix);
	} else if (style == GEL_OUTPUT_TROFF) {
		int l;
		p=mpz_get_str(NULL,10,num);
		digits = strlen(p);
		p=prependstr(p,"{");
		p=appendstr(p,"} over {");
		p2=mpz_get_str(NULL,10,den);
		p=appendstr(p,p2);
		l = strlen (p2);
		if (l > digits)
			digits = l;
		g_free(p2);
		p=appendstr(p,"}");
		p=appendstr(p,postfix);
	} else {
		p=mpz_get_str(NULL,10,num);
		p=appendstr(p,postfix);
		p=appendstr(p,"/");
		p2=mpz_get_str(NULL,10,den);
		p=appendstr(p,p2);
		g_free(p2);
		digits = strlen(p) - 1; /* don't count the / */
		digits -= strlen (postfix); /* don't count the i */
	}
	
	*dig += digits;

	return p;
}	

static char *
str_getstring_q(mpq_t num,
		int max_digits,
		gboolean scientific_notation,
		gboolean mixed_fractions,
		GelOutputStyle style,
		const char *postfix)
{
	char *p,*p2;
	mpf_t fr;
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
		mpz_ptr tmp1, tmp2;
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
				p = prependstr (p, "\\left(");
			else if (style == GEL_OUTPUT_TROFF)
				p = prependstr (p, " left ( ");
			else
				p = prependstr (p, "(");
		}

		p=appendstr(p," ");

		p2 = get_frac (tmp2, mpq_denref (num),
			       style, "", &d);
		p=appendstr(p,p2);
		g_free(p2);

		if (postfix != NULL &&
		    *postfix != '\0') {
			if (style == GEL_OUTPUT_LATEX)
				p = appendstr (p, "\\right)");
			else if (style == GEL_OUTPUT_TROFF)
				p = appendstr (p, " right )~");
			else
				p = appendstr (p, ")");
			p = appendstr (p, postfix);
		}

		CLEAR_FREE_MPZ (tmp1);
		CLEAR_FREE_MPZ (tmp2);
	}
	if (max_digits > 0 && max_digits < digits) {
		mpf_init(fr);
		mpf_set_q(fr,num);
		p2=str_getstring_f(fr,max_digits,scientific_notation,
				   postfix);
		mpf_clear(fr);
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
str_getstring_f(mpf_t num,
		int max_digits,
		gboolean scientific_notation,
		const char *postfix)
{
	char *p;
	long e;

	p=mpf_get_str(NULL,&e,10,0,num);
	str_make_max_digits (p, max_digits, &e);
	p=str_format_float(p,e,scientific_notation);
	p=appendstr(p,postfix);

	return p;
}

static char *
mpwl_getstring(MpwRealNum * num, int max_digits,
	       gboolean scientific_notation,
	       gboolean results_as_floats,
	       gboolean mixed_fractions,
	       GelOutputStyle style,
	       int integer_output_base,
	       const char *postfix)
{
	mpf_t fr;
	char *p;
	switch(num->type) {
	case MPW_RATIONAL:
		if(results_as_floats) {
			mpf_init(fr);
			mpf_set_q(fr,num->data.rval);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation, postfix);
			mpf_clear(fr);
			return p;
		}
		return str_getstring_q(num->data.rval,
				       max_digits,
				       scientific_notation,
				       mixed_fractions,
				       style,
				       postfix);
	case MPW_INTEGER:
		if(results_as_floats) {
			mpf_init(fr);
			mpf_set_z(fr,num->data.ival);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation,
					  postfix);
			mpf_clear(fr);
			return p;
		}
		return str_getstring_z(num->data.ival,max_digits,
				       scientific_notation,
				       integer_output_base,
				       postfix);
	case MPW_FLOAT:
		return str_getstring_f(num->data.fval,max_digits,
				       scientific_notation,
				       postfix);
	}
	/*something bad happened*/
	return NULL;
}

#define mpw_uncomplex(rop)					\
{								\
	if ((rop)->i == zero) {					\
		(rop)->type = MPW_REAL;				\
	} else if ((rop)->type == MPW_COMPLEX &&		\
		   mpwl_sgn ((rop)->i) == 0) {			\
		(rop)->type = MPW_REAL;				\
		(rop)->i->alloc.usage--;			\
		if ((rop)->i->alloc.usage==0)			\
			mpwl_free ((rop)->i, FALSE);		\
		(rop)->i = zero;				\
		zero->alloc.usage ++;				\
	}							\
}

/*************************************************************************/
/*high level stuff                                                       */
/*************************************************************************/

/*set default precision*/
void
mpw_set_default_prec (unsigned long int prec)
{
	mpfr_ptr *p;
	mpf_set_default_prec (prec);

	/* whack the mpf cache */
	for (p = free_mpf; p != free_mpf_top; p++) {
		mpf_clear (*p);
		g_free (*p);
	}
	free_mpf_top = free_mpf;

	default_mpf_prec = prec;
}

/*initialize a number*/
void
mpw_init (mpw_ptr op)
{
	op->type=MPW_REAL;
	op->r = zero;
	zero->alloc.usage++;
	op->i = zero;
	zero->alloc.usage++;
}

void
mpw_init_set(mpw_ptr rop, mpw_ptr op)
{
	rop->type = op->type;
	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
	mpw_uncomplex (rop);
}

void
mpw_init_set_no_uncomplex (mpw_ptr rop, mpw_ptr op)
{
	rop->type = op->type;
	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
}

/*clear memory held by number*/
void
mpw_clear(mpw_ptr op)
{
	op->r->alloc.usage--;
	op->i->alloc.usage--;
	if(op->r->alloc.usage==0)
		mpwl_free(op->r,FALSE);
	if(op->i->alloc.usage==0)
		mpwl_free(op->i,FALSE);
	op->type=0;
}

/*make them the same type without loosing information*/
void
mpw_make_same_type(mpw_ptr op1,mpw_ptr op2)
{
	MAKE_COPY(op1->r);
	MAKE_COPY(op2->r);
	mpwl_make_same_type(op1->r,op2->r);
	if(op1->type==MPW_COMPLEX || op2->type==MPW_COMPLEX) {
		MAKE_COPY(op1->i);
		MAKE_COPY(op2->i);
		mpwl_make_same_type(op1->i,op2->i);
	}
}

void
mpw_set(mpw_ptr rop,mpw_ptr op)
{
	rop->type=op->type;
	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
	mpw_uncomplex(rop);
}

void
mpw_set_d(mpw_ptr rop,double d)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_set_d(rop->r,d);
}

void
mpw_set_d_complex (mpw_ptr rop, double real, double imag)
{
	MAKE_COPY (rop->r);
	MAKE_COPY (rop->i);
	rop->type = MPW_COMPLEX;
	mpwl_set_d (rop->r, real);
	mpwl_set_d (rop->i, imag);
}

void
mpw_set_si(mpw_ptr rop,signed long int i)
{
	MAKE_REAL(rop);
	if(i==0) {
		if(rop->r != zero) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = zero;
			zero->alloc.usage++;
		}
	} else if(i==1) {
		if(rop->r != one) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = one;
			one->alloc.usage++;
		}
	} else {
		MAKE_COPY(rop->r);
		mpwl_set_si(rop->r,i);
	}
}

void
mpw_set_ui(mpw_ptr rop,unsigned long int i)
{
	MAKE_REAL(rop);
	if(i==0) {
		if(rop->r != zero) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = zero;
			zero->alloc.usage++;
		}
	} else if(i==1) {
		if(rop->r != one) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = one;
			one->alloc.usage++;
		}
	} else {
		MAKE_COPY(rop->r);
		mpwl_set_ui(rop->r,i);
	}
}

void
mpw_set_mpz_use (mpw_ptr rop, mpz_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r,FALSE);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_INTEGER;
	rop->r->alloc.usage = 1;
	rop->r->data.ival = g_new (__mpz_struct, 1);
	memcpy (rop->r->data.ival, op, sizeof (__mpz_struct));
}

void
mpw_set_mpq_use (mpw_ptr rop, mpq_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r,FALSE);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_RATIONAL;
	rop->r->alloc.usage = 1;
	rop->r->data.rval = g_new (__mpq_struct, 1);
	memcpy (rop->r->data.rval, op, sizeof (__mpq_struct));
}

void
mpw_set_mpf_use (mpw_ptr rop, mpfr_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r,FALSE);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_FLOAT;
	rop->r->alloc.usage = 1;
	rop->r->data.fval = g_new (__mpfr_struct, 1);
	memcpy (rop->r->data.fval, op, sizeof (__mpfr_struct));
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
	if (op->type == MPW_COMPLEX &&
	    op->i->type == MPW_INTEGER)
		return op->i->data.ival;
	else
		return NULL;
}

mpq_ptr
mpw_peek_imag_mpq (mpw_ptr op)
{
	if (op->type == MPW_COMPLEX &&
	    op->i->type == MPW_RATIONAL)
		return op->i->data.rval;
	else
		return NULL;
}

mpfr_ptr
mpw_peek_imag_mpf (mpw_ptr op)
{
	if (op->type == MPW_COMPLEX &&
	    op->i->type == MPW_FLOAT)
		return op->i->data.fval;
	else
		return NULL;
}

int
mpw_sgn(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_sgn(op->r);
	} else {
		gel_errorout (_("Can't compare complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
	}
	return 0;
}

void
mpw_abs(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if(mpwl_sgn(op->r)<0)
			mpw_neg(rop,op);
		else
			mpw_set(rop,op);
	} else {
		MpwRealNum t = {{NULL}};

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		
		mpwl_init_type (&t, MAX (op->r->type, op->i->type));
		
		mpwl_mul(rop->r,op->r,op->r);
		mpwl_mul(&t,op->i,op->i);
		mpwl_add(rop->r,rop->r,&t);
		
		mpwl_free(&t,TRUE);

		mpwl_sqrt(rop->r,rop->r);
	}
}

void
mpw_neg(mpw_ptr rop,mpw_ptr op)
{
	MAKE_COPY(rop->r);
	mpwl_neg(rop->r,op->r);
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
	} else {
		MAKE_COPY(rop->i);
		mpwl_neg(rop->i,op->i);
	}
	rop->type = op->type;
}

void
mpw_add(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_add(rop->r,op1->r,op2->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_add(rop->r,op1->r,op2->r);
		mpwl_add(rop->i,op1->i,op2->i);

		mpw_uncomplex(rop);
	}
}

void
mpw_add_ui(mpw_ptr rop,mpw_ptr op, unsigned long i)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_add_ui(rop->r,op->r,i);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_add_ui(rop->r,op->r,i);
		mpwl_set(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_sub(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sub(rop->r,op1->r,op2->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_sub(rop->r,op1->r,op2->r);
		mpwl_sub(rop->i,op1->i,op2->i);

		mpw_uncomplex(rop);
	}
}

void
mpw_sub_ui(mpw_ptr rop,mpw_ptr op, unsigned long i)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sub_ui(rop->r,op->r,i);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_sub_ui(rop->r,op->r,i);
		mpwl_set(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_ui_sub(mpw_ptr rop,unsigned long i, mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_ui_sub(rop->r,i,op->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_ui_sub(rop->r,i,op->r);
		mpwl_neg(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_mul(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mul(rop->r,op1->r,op2->r);
	} else {
		MpwRealNum tr = {{NULL}};
		MpwRealNum ti = {{NULL}};
		MpwRealNum *r1;
		MpwRealNum *i1;
		MpwRealNum *r2;
		MpwRealNum *i2;
		
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op1,r1,i1);
		MAKE_CPLX_OPS(op2,r2,i2);

		mpwl_mul(rop->r,r1,r2);
		mpwl_mul(rop->i,i1,r2);

		mpwl_init_type(&tr,i1->type);
		mpwl_init_type(&ti,r1->type);

		/* tmp->type = MPW_COMPLEX; */
		mpwl_mul(&tr,i1,i2);
		mpwl_neg(&tr,&tr);
		mpwl_mul(&ti,r1,i2);
		
		mpwl_add(rop->r,rop->r,&tr);
		mpwl_add(rop->i,rop->i,&ti);

		mpwl_free(&tr,TRUE);
		mpwl_free(&ti,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op1,r1,i1);
		BREAK_CPLX_OPS(op2,r2,i2);
	}
}

void
mpw_mul_ui(mpw_ptr rop,mpw_ptr op, unsigned int i)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mul_ui(rop->r,op->r,i);
	} else {
		rop->type = MPW_COMPLEX;
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		mpwl_mul_ui(rop->r,op->r,i);
		mpwl_mul_ui(rop->i,op->i,i);

		mpw_uncomplex(rop);
	}
}

void
mpw_div(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op2->r)==0) {
			gel_errorout (_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_div(rop->r,op1->r,op2->r);
	} else {
		MpwRealNum t1 = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		MpwRealNum *r1;
		MpwRealNum *i1;
		MpwRealNum *r2;
		MpwRealNum *i2;
		if G_UNLIKELY (mpwl_sgn(op2->r)==0 && mpwl_sgn(op2->i)==0) {
			gel_errorout (_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

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

		mpwl_free(&t1,TRUE);
		mpwl_free(&t2,TRUE);

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
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_div_ui(rop->r,op->r,i);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		mpwl_div_ui(rop->r,op->r,i);
		mpwl_div_ui(rop->i,op->i,i);

		mpw_uncomplex(rop);
	}
}

void
mpw_ui_div(mpw_ptr rop,unsigned int in,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			gel_errorout (_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_ui_div(rop->r,in,op->r);
	} else {
		MpwRealNum t1 = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		if G_UNLIKELY (mpwl_sgn(op->r)==0 && mpwl_sgn(op->i)==0) {
			gel_errorout (_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

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

		mpwl_free(&t1,TRUE);
		mpwl_free(&t2,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_mod (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op2->r)==0) {
			gel_errorout (_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mod(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't modulo complex numbers"));
	}
}

void
mpw_invert (mpw_ptr rop, mpw_ptr op1, mpw_ptr mod)
{
	if  G_LIKELY (op1->type == MPW_REAL && mod->type == MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		if G_UNLIKELY ( ! mpwl_invert (rop->r, op1->r, mod->r)) {
			if (op1->r->type == MPW_INTEGER &&
			    mod->r->type == MPW_INTEGER) {
				char *n1, *n2;
				/* if the above just failed because of
				   types */
				n1 = mpwl_getstring_for_error (op1->r);
				n2 = mpwl_getstring_for_error (mod->r);
				error_num = NUMERICAL_MPW_ERROR;
				gel_errorout (_("Inverse of %s modulo "
						"%s not found!"),
					      n1, n2);
				g_free (n1);
				g_free (n2);
			}
		}
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't do modulo invert on complex numbers"));
	}
}

void
mpw_gcd(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_gcd(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't GCD complex numbers"));
	}
}
void
mpw_lcm (mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MpwRealNum gcd = {{NULL}};
		mpwl_init_type (&gcd, MPW_INTEGER);

		mpwl_gcd (&gcd, op1->r, op2->r);
		if G_UNLIKELY (error_num == NUMERICAL_MPW_ERROR)
			return;

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mul (rop->r, op1->r, op2->r);
		mpwl_div (rop->r, rop->r, &gcd);
		mpwl_clear (&gcd);
		if (mpwl_sgn (rop->r) < 0)
			mpwl_neg (rop->r, rop->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't LCM complex numbers"));
	}
}

void
mpw_jacobi(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_jacobi(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't get jacobi symbols of complex numbers"));
	}
}
void
mpw_legendre(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_legendre(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't get legendre symbols complex numbers"));
	}
}
void
mpw_kronecker(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_kronecker(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("Can't get jacobi symbol with Kronecker extension for complex numbers"));
	}
}
void
mpw_lucnum (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (op->type == MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		mpwl_lucnum (rop->r, op->r);
	} else {
		error_num = NUMERICAL_MPW_ERROR;
		gel_errorout  (_("Can't get lucas number for complex numbers"));
	}
}
void
mpw_nextprime (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (op->type == MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		mpwl_nextprime (rop->r, op->r);
	} else {
		error_num = NUMERICAL_MPW_ERROR;
		gel_errorout  (_("Can't get next prime for complex numbers"));
	}
}
gboolean
mpw_perfect_square(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_perfect_square(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "perfect_square");
		return FALSE;
	}
}
gboolean
mpw_perfect_power(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_perfect_power(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "perfect_power");
		return FALSE;
	}
}
gboolean
mpw_even_p(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_even_p(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "even_p");
		return FALSE;
	}
}
gboolean
mpw_odd_p(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_odd_p(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		gel_errorout (_("%s: can't work on complex numbers"),
			      "odd_p");
		return FALSE;
	}
}

void
mpw_pow (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(mpwl_pow(rop->r,op1->r,op2->r)) {
			goto backup_mpw_pow;
		}
	} else if (op2->type == MPW_REAL &&
		   op2->r->type == MPW_INTEGER &&
		   op1->i->type != MPW_FLOAT &&
		   mpwl_sgn (op1->r) == 0) {
		MpwRealNum t = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		mpwl_init_type (&t, op1->i->type);
		mpwl_init_type (&t2, op2->r->type);
		mpwl_set (&t2, op2->r);

		if (mpwl_pow (&t, op1->i, op2->r)) {
			mpwl_free (&t2, TRUE);
			mpwl_free (&t, TRUE);
			goto backup_mpw_pow;

		}

		if (mpwl_even_p (&t2)) {
			/*even*/
			MAKE_REAL (rop);
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

		mpwl_free (&t2, TRUE);
		mpwl_free (&t, TRUE);
	} else {
		goto backup_mpw_pow;
	}
	return;
backup_mpw_pow:
	{
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
mpw_powm(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2, mpw_ptr mod)
{
	if G_UNLIKELY (op1->type != MPW_REAL ||
		       op2->type != MPW_REAL ||
		       mod->type != MPW_REAL) {
		gel_errorout (_("%s: Bad types for mod power"),
			      "powm");
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	MAKE_REAL (rop);
	MAKE_COPY (rop->r);

	mpwl_powm (rop->r, op1->r, op2->r, mod->r);
}

void
mpw_powm_ui (mpw_ptr rop,mpw_ptr op, unsigned long int e, mpw_ptr mod)
{
	if G_UNLIKELY (op->type != MPW_REAL ||
		       mod->type != MPW_REAL) {
		gel_errorout (_("%s: Bad types for mod power"),
			      "powm");
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	MAKE_REAL (rop);
	MAKE_COPY (rop->r);

	mpwl_powm_ui (rop->r, op->r, e, mod->r);
}

void
mpw_sqrt(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(mpwl_sqrt(rop->r,op->r)) {
			MpwRealNum *t;
			t = rop->r;
			rop->r = rop->i;
			rop->i = t;
			rop->type=MPW_COMPLEX;
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
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_exp(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_exp(rop->r,r);
		mpwl_set(rop->i,rop->r);
		
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_ln(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			gel_errorout (_("%s: can't take logarithm of 0"),
				      "ln");
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(!mpwl_ln(rop->r,op->r)) {
			rop->type = MPW_COMPLEX;
			MAKE_COPY(rop->i);
			mpwl_pi(rop->i);
		}
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		if(mpwl_sgn(op->r)==0) {
			g_assert(mpwl_sgn(op->i)!=0);
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

		mpwl_free(&t,TRUE);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_log2(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			gel_errorout (_("%s: can't take logarithm of 0"),
				      "log2");
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(!mpwl_log2(rop->r,op->r)) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;
			MAKE_COPY(rop->i);
			mpwl_pi (rop->i);
			/* no need for init ln2 does that for us */
			mpwl_ln2 (&t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
		}
	} else {
		mpw_t two;
		if (mpwl_sgn(op->r)==0) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;

			MAKE_COPY(rop->r);
			MAKE_COPY(rop->i);

			g_assert(mpwl_sgn(op->i)!=0);
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
			/* no need for init ln2 does that for us */
			mpwl_ln2 (&t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
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
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			gel_errorout (_("%s: can't take logarithm of 0"),
				      "log10");
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(!mpwl_log10(rop->r,op->r)) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;
			MAKE_COPY(rop->i);
			mpwl_pi (rop->i);
			/* FIXME: implement caching */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_ui (&t, 10);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
		}
	} else {
		mpw_t ten;
		if (mpwl_sgn(op->r)==0) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;

			MAKE_COPY(rop->r);
			MAKE_COPY(rop->i);

			g_assert(mpwl_sgn(op->i)!=0);
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
			mpwl_set_ui (&t, 10);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
			return;
		}
		/* this is stupid, but simple to implement for now */
		mpw_init (ten);
		/* FIXME: implement caching */
		mpw_set_ui (ten, 10);
		mpw_ln (ten, ten);
		mpw_ln (rop, op);
		mpw_div (rop, rop, ten);
		mpw_clear (ten);
	}
}

void
mpw_sin(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sin(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_sin(rop->r,r);
		mpwl_cosh(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_cos(rop->i,r);
		mpwl_sinh(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_cos(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_cos(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_cos(rop->r,r);
		mpwl_cosh(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sin(rop->i,r);
		mpwl_neg(rop->i,rop->i);
		mpwl_sinh(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_sinh(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sinh(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_sinh(rop->r,r);
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_cosh(rop->i,r);
		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_cosh(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_cosh(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_cosh(rop->r,r);
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sinh(rop->i,r);
		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_arctan(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
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
		
		error_num = 0;
		
		mpw_init(tmp1);
		mpw_set_ui(tmp1,1);
		mpw_add(tmp1,tmp1,ai);
		
		mpw_init(tmp2);
		mpw_set_ui(tmp2,1);
		mpw_sub(tmp2,tmp2,ai);
		mpw_clear(ai);

		mpw_div(tmp1,tmp1,tmp2);
		mpw_clear(tmp2);
		
		if G_UNLIKELY (error_num) {
			mpw_clear(tmp1);
			return;
		}
		
		mpw_ln(tmp1,tmp1);

		if G_UNLIKELY (error_num) {
			mpw_clear(tmp1);
			return;
		}

		/*divide by 2i*/
		tmp1->type = MPW_COMPLEX;
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
mpw_pi (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_pi (rop->r);
}

void
mpw_ln2 (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_ln2 (rop->r);
}

void
mpw_euler_constant (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_euler_constant (rop->r);
}

void
mpw_rand (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_rand (rop->r);
}

void
mpw_randint (mpw_ptr rop, mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		gel_errorout (_("Can't make random integer out of a complex number"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_randint (rop->r, op->r);
}

void
mpw_i (mpw_ptr rop)
{
	mpw_clear (rop);

	rop->type = MPW_COMPLEX;
	rop->r = zero;
	zero->alloc.usage++;
	rop->i = one;
	one->alloc.usage++;
}

void
mpw_conj (mpw_ptr rop, mpw_ptr op)
{
	if (op->type == MPW_REAL) {
		if (rop != op) {
			MAKE_REAL (rop);
			rop->r->alloc.usage--;
			if (rop->r->alloc.usage==0)
				mpwl_free (rop->r, FALSE);
			rop->r = op->r;
			op->r->alloc.usage++;
		}
	} else {
		if (rop != op) {
			rop->type = MPW_COMPLEX;
			rop->r->alloc.usage--;
			if (rop->r->alloc.usage==0)
				mpwl_free (rop->r, FALSE);
			rop->r = op->r;
			op->r->alloc.usage++;

			MAKE_COPY(rop->i);
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
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
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
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		return mpwl_cmp(op1->r,op2->r);
	} else {
		gel_errorout (_("Can't compare complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
}

int
mpw_cmp_ui(mpw_ptr op, unsigned long int i)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_cmp_ui(op->r,i);
	} else {
		gel_errorout (_("Can't compare complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
}

gboolean
mpw_eql(mpw_ptr op1, mpw_ptr op2)
{
	return (mpwl_cmp(op1->r,op2->r)==0 && mpwl_cmp(op1->i,op2->i)==0);
}

gboolean 
mpw_eql_ui(mpw_ptr op, unsigned long int i)
{
	if (op->type == MPW_REAL) {
		return mpwl_cmp_ui (op->r, i) == 0;
	} else {
		return FALSE;
	}
}

void
mpw_fac_ui(mpw_ptr rop,unsigned long int i)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_fac_ui(rop->r,i);
}

void
mpw_fac(mpw_ptr rop,mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_fac(rop->r,op->r);
	} else {
		gel_errorout (_("Can't make factorials of complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

void
mpw_dblfac (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		mpwl_dblfac (rop->r, op->r);
	} else {
		gel_errorout (_("Can't make factorials of complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

/*make a number int if possible*/
void
mpw_make_int(mpw_ptr rop)
{
	if(rop->type==MPW_REAL) {
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
	if(rop->type==MPW_REAL) {
		MAKE_COPY(rop->r);
		mpwl_make_float(rop->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		mpwl_make_float(rop->r);
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

	GET_NEW_REAL(zero);
	mpwl_init_type(zero,MPW_INTEGER);
	mpwl_set_ui(zero,0);
	zero->alloc.usage = 1;
	GET_NEW_REAL(one);
	mpwl_init_type(one,MPW_INTEGER);
	mpwl_set_ui(one,1);
	one->alloc.usage = 1;
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
	if(num->type==MPW_REAL) {
		return mpwl_getstring(num->r,max_digits,scientific_notation,
			results_as_floats,mixed_fractions,style,
			integer_output_base,"");
	} else {
		char *p1 = NULL, *p2, *r;
		int justreal = mpwl_sgn(num->r) == 0;
		if (! justreal)
			p1 = mpwl_getstring(num->r,
					    max_digits,
					    scientific_notation,
					    results_as_floats,
					    mixed_fractions, 
					    style,
					    integer_output_base,
					    "" /* postfix */);
		p2 = mpwl_getstring(num->i,
				    max_digits,
				    scientific_notation,
				    results_as_floats,
				    mixed_fractions, 
				    style,
				    integer_output_base,
				    "i" /* postfix */);
		if (justreal) {
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

void
mpw_set_str_float (mpw_ptr rop, const char *s, int base)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_set_str_float(rop->r,s,base);
}

void
mpw_set_str_int (mpw_ptr rop, const char *s, int base)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_set_str_int(rop->r,s,base);
}

void
mpw_set_str_complex_int(mpw_ptr rop,const char *s,int base)
{
	char *p;
	int size;

	rop->type = MPW_COMPLEX;

	p = g_strdup(s);
	size = strlen(p);
	if(p[size-1] == 'i')
		p[size-1] = '\0';
	MAKE_COPY(rop->i);
	mpwl_set_str_int(rop->i,p,base);

	g_free(p);

	mpw_uncomplex(rop);
}

void
mpw_set_str_complex(mpw_ptr rop,const char *s,int base)
{
	char *p;
	int size;

	rop->type = MPW_COMPLEX;

	p = g_strdup(s);
	size = strlen(p);
	if(p[size-1] == 'i')
		p[size-1] = '\0';
	MAKE_COPY(rop->i);
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
		char *ptrptr;

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
	char *ptrptr;
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
	return op->type == MPW_COMPLEX;
}

gboolean
mpw_is_integer(mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		gel_errorout (_("Can't determine type of a complex number"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_INTEGER;
}

gboolean
mpw_is_complex_integer(mpw_ptr op)
{
	if(op->type == MPW_COMPLEX) {
		return op->r->type == MPW_INTEGER &&
		       op->i->type == MPW_INTEGER;
	} else {
		return op->r->type == MPW_INTEGER;
	}
}

gboolean
mpw_is_rational(mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		gel_errorout (_("Can't determine type of a complex number"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_RATIONAL;
}

gboolean
mpw_is_complex_rational_or_integer(mpw_ptr op)
{
	if(op->type == MPW_COMPLEX) {
		return op->r->type <= MPW_RATIONAL &&
			op->i->type <= MPW_RATIONAL;
	} else {
		return op->r->type <= MPW_RATIONAL;
	}
}

gboolean
mpw_is_float(mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		gel_errorout (_("Can't determine type of a complex number"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_FLOAT;
}

gboolean
mpw_is_complex_float(mpw_ptr op)
{
	if(op->type == MPW_COMPLEX) {
		return op->r->type == MPW_FLOAT ||
			op->i->type == MPW_FLOAT;
	} else {
		return op->r->type == MPW_FLOAT;
	}
}

void
mpw_im(mpw_ptr rop, mpw_ptr op)
{
	MAKE_REAL(rop);
	rop->r=op->i;
	op->i->alloc.usage++;
}

void
mpw_re(mpw_ptr rop, mpw_ptr op)
{
	MAKE_REAL(rop);
	rop->r=op->r;
	op->r->alloc.usage++;
}

void
mpw_round(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_round(rop->r);
	if(op->type==MPW_COMPLEX) {
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
	if(op->type==MPW_COMPLEX) {
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
	if(op->type==MPW_COMPLEX) {
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
	if(op->type==MPW_COMPLEX) {
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
	if G_UNLIKELY (op->type==MPW_COMPLEX) {
		gel_errorout (_("Can't convert complex number into integer"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_long(op->r,&ex);
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		gel_errorout (_("Can't convert real number to integer"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} else if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		gel_errorout (_("Integer too large for this operation"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

double
mpw_get_double (mpw_ptr op)
{
	double r;
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	if G_UNLIKELY (op->type==MPW_COMPLEX) {
		gel_errorout (_("Can't convert complex number into integer"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_double (op->r, &ex);
	/* currently there is no conversion error exception for
	   get_double */
#if 0
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		gel_errorout (_("Can't convert real number to double"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} else
#endif
	if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		gel_errorout (_("Number too large for this operation"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

void
mpw_denominator(mpw_ptr rop, mpw_ptr op)
{
	if(op->type==MPW_COMPLEX) {
		MpwRealNum r1 = {{NULL}};
		MpwRealNum r2 = {{NULL}};

		mpwl_init_type (&r1, MPW_INTEGER);
		mpwl_init_type (&r1, MPW_INTEGER);

		mpwl_denominator (&r1, op->r);
		mpwl_denominator (&r2, op->i);

		if G_UNLIKELY (error_num != NO_ERROR) {
			mpwl_clear (&r1);
			mpwl_clear (&r2);
			return;
		}

		MAKE_REAL (rop);
		MAKE_COPY (rop->r);

		mpwl_mul (rop->r, &r1, &r2);
		mpwl_gcd (&r1, &r1, &r2);
		mpwl_div (rop->r, rop->r, &r1);

		mpwl_clear (&r1);
		mpwl_clear (&r2);
	} else {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_denominator(rop->r, op->r);
	}
}

void
mpw_numerator(mpw_ptr rop, mpw_ptr op)
{
	if(op->type==MPW_COMPLEX) {
		MpwRealNum r1 = {{NULL}};
		MpwRealNum r2 = {{NULL}};
		MpwRealNum n1 = {{NULL}};
		MpwRealNum n2 = {{NULL}};

		mpwl_init_type (&r1, MPW_INTEGER);
		mpwl_init_type (&r1, MPW_INTEGER);
		mpwl_init_type (&n1, MPW_INTEGER);
		mpwl_init_type (&n1, MPW_INTEGER);

		mpwl_denominator (&r1, op->r);
		mpwl_denominator (&r2, op->i);
		mpwl_numerator (&n1, op->r);
		mpwl_numerator (&n2, op->i);

		if G_UNLIKELY (error_num != NO_ERROR) {
			mpwl_clear (&r1);
			mpwl_clear (&r2);
			mpwl_clear (&n1);
			mpwl_clear (&n2);
			return;
		}

		rop->type = MPW_COMPLEX;
		MAKE_COPY (rop->r);
		MAKE_COPY (rop->i);

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
		MAKE_COPY(rop->r);
		mpwl_numerator(rop->r, op->r);
	}
}
