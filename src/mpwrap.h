/* GENIUS Calculator
 * Copyright (C) 1997-2002 George Lebl
 *
 * Author: George Lebl
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

#ifndef _MPWRAP_H_
#define _MPWRAP_H_

#ifdef HAVE_GMP2_INCLUDE_DIR
#include <gmp2/gmp.h>
#else
#include <gmp.h>
#endif

#define MPW_NATIVEINT	1
#define MPW_INTEGER	2
#define MPW_RATIONAL	3
#define MPW_FLOAT	4

#define MPW_REAL	1
#define MPW_COMPLEX	2

/*number structures, this is where low level stuff is stored so it will be
  different for each lib, members should never be directly accessed!*/

/*real only structure*/
typedef struct _MpwRealNum {
	int type;
	struct { /*this is done as a struct so that conversions don't require
		   a temporary, make sure to clear the old one!, in the worst
		   case we have two unused pointers*/
		mpz_ptr ival;
		mpq_ptr rval;
		mpf_ptr fval;
		long nval;
	} data;
	union {
		struct _MpwRealNum *next; /*used for free lists*/
		int usage; /*used for copy-on-write*/
	} alloc; /*private union for memory managment stuff*/
} MpwRealNum;

/*any number (includes complex) so it includes an imaginary member if type
  is MPW_COMPLEX

  this is used as the number type*/
struct _mpw_t {
	int type;
	MpwRealNum *r; /*real*/
	MpwRealNum *i; /*imaginary*/
};

typedef struct _mpw_t mpw_t[1];
typedef struct _mpw_t *mpw_ptr;

enum {
	MPW_NO_ERROR=0,
	MPW_INTERNAL_ERROR,
	MPW_NUMERICAL_ERROR,
};

/* private struct, use accessors for fields */
typedef struct _MpwCtx MpwCtx;

/*************************************************************************/
/*conext level stuff                                                     */
/*************************************************************************/

typedef void (*MpwErrorFunc) (MpwCtx *, char *);

/* make new context with a refcount of 1 */
MpwCtx *mpw_ctx_new(MpwErrorFunc errorout,
		    int default_mpf_prec,
		    gboolean double_math,
		    gpointer data);

void mpw_ctx_set_errorout(MpwCtx *mctx, MpwErrorFunc errorout);
void mpw_ctx_set_default_mpf_prec(MpwCtx *mctx, int default_mpf_prec);
void mpw_ctx_set_double_math(MpwCtx *mctx, gboolean double_math);
/* if mctx is one gotten from an GelExecCtx, then user code should not
   use data as it points to the GelExecCtx */
void mpw_ctx_set_data(MpwCtx *mctx, gpointer data);

MpwErrorFunc mpw_ctx_get_errorout(MpwCtx *mctx);
int mpw_ctx_get_default_mpf_prec(MpwCtx *mctx);
gboolean mpw_ctx_get_double_math(MpwCtx *mctx);
gpointer mpw_ctx_get_data(MpwCtx *mctx);

void mpw_ctx_ref(MpwCtx *mctx);
void mpw_ctx_unref(MpwCtx *mctx);


/*************************************************************************/
/*high level stuff                                                       */
/*************************************************************************/

/*set default precision*/
void mpw_set_default_prec(unsigned long int i);

/*initialize a number*/
void mpw_init(mpw_ptr op);
void mpw_init_set(mpw_ptr rop,mpw_ptr op);

/*clear memory held by number*/
void mpw_clear(mpw_ptr op);

/*make them the same type without loosing information*/
void mpw_make_same_type(mpw_ptr op1,mpw_ptr op2);

void mpw_set(mpw_ptr rop,mpw_ptr op);
void mpw_set_d(mpw_ptr rop,double d);
void mpw_set_si(mpw_ptr rop,signed long int i);
void mpw_set_ui(mpw_ptr rop,unsigned long int i);

void mpw_abs(mpw_ptr rop,mpw_ptr op);

int mpw_sgn(mpw_ptr op);

void mpw_neg(mpw_ptr rop,mpw_ptr op);

void mpw_add(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_add_ui(mpw_ptr rop,mpw_ptr op, unsigned long i);

void mpw_sub(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_sub_ui(mpw_ptr rop,mpw_ptr op, unsigned long i);
void mpw_ui_sub(mpw_ptr rop,unsigned long i, mpw_ptr op);

void mpw_mul(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_mul_ui(mpw_ptr rop,mpw_ptr op, unsigned int i);

void mpw_div(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_div_ui(mpw_ptr rop,mpw_ptr op, unsigned int i);
void mpw_ui_div(mpw_ptr rop,unsigned int i,mpw_ptr op);

void mpw_mod(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);

void mpw_gcd(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_jacobi(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_legendre(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_kronecker(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
int mpw_perfect_square(mpw_ptr op);

void mpw_pow(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_pow_ui(mpw_ptr rop,mpw_ptr op, unsigned long int e);
void mpw_sqrt(mpw_ptr rop,mpw_ptr op);

void mpw_exp(mpw_ptr rop,mpw_ptr op);
void mpw_ln(mpw_ptr rop,mpw_ptr op);

void mpw_sin(mpw_ptr rop,mpw_ptr op);
void mpw_cos(mpw_ptr rop,mpw_ptr op);
void mpw_sinh(mpw_ptr rop,mpw_ptr op);
void mpw_cosh(mpw_ptr rop,mpw_ptr op);
void mpw_arctan(mpw_ptr rop,mpw_ptr op);
void mpw_pi (mpw_ptr rop);
void mpw_i (mpw_ptr rop);
void mpw_rand (mpw_ptr rop);
void mpw_randint (mpw_ptr rop, mpw_ptr op);

void mpw_conj (mpw_ptr rop, mpw_ptr op);

int mpw_cmp(mpw_ptr op1, mpw_ptr op2);
int mpw_cmp_ui(mpw_ptr op, unsigned long int i);

int mpw_eql(mpw_ptr op1, mpw_ptr op2);
int mpw_eql_ui(mpw_ptr op, unsigned long int i);

void mpw_fac_ui(mpw_ptr rop,unsigned long int i);
void mpw_fac(mpw_ptr rop,mpw_ptr op);

/*make a number int if possible*/
void mpw_make_int(mpw_ptr rop);

/*make number into a float, this might be neccessary for unprecise
  calculations*/
void mpw_make_float(mpw_ptr rop);

/*init the mp stuff*/
void mpw_init_mp(void);

/*get a string (g_malloc'ed) with the number in it*/
char * mpw_getstring(mpw_ptr num,int max_digits,int scientific_notation,
	int results_as_floats,int mixed_fractions, int integer_output_base);

void mpw_set_str_float(mpw_ptr rop,const char *s,int base);

/*reads only the imaginary part (use add for real part)*/
void mpw_set_str_complex(mpw_ptr rop,const char *s,int base);

void mpw_set_str_int(mpw_ptr rop,const char *s,int base);

void mpw_set_str (mpw_ptr rop, const char *s, int base);

int mpw_is_complex(mpw_ptr op);
int mpw_is_integer(mpw_ptr op);
int mpw_is_rational(mpw_ptr op);
int mpw_is_float(mpw_ptr op);

void mpw_im(mpw_ptr rop, mpw_ptr op);
void mpw_re(mpw_ptr rop, mpw_ptr op);

void mpw_round(mpw_ptr rop, mpw_ptr op);
void mpw_floor(mpw_ptr rop, mpw_ptr op);
void mpw_ceil(mpw_ptr rop, mpw_ptr op);
void mpw_trunc(mpw_ptr rop, mpw_ptr op);

/*try to get a long number representation of the number*/
long mpw_get_long(mpw_ptr op);

void mpw_denominator(mpw_ptr rop, mpw_ptr op);
void mpw_numerator(mpw_ptr rop, mpw_ptr op);

#endif
