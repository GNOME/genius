/* GENIUS Calculator
 * Copyright (C) 1997-2012 Jiri (George) Lebl
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

#ifndef MPWRAP_H_
#define MPWRAP_H_

#ifdef HAVE_GMP2_INCLUDE_DIR
#include <gmp2/gmp.h>
#else
#include <gmp.h>
#endif

/* FIXME: we may need the same as above */
#include <mpfr.h>


enum {
	MPW_INTEGER = 1,
	MPW_RATIONAL,
	MPW_FLOAT
};

/*number structures, this is where low level stuff is stored so it will be
  different for each lib, members should never be directly accessed!*/

/*real only structure*/
typedef struct _MpwRealNum {
	union {
		struct _MpwRealNum *next; /*used for free lists*/
		int usage; /*used for copy-on-write*/
	} alloc; /*private union for memory managment stuff*/
	union {
		mpz_t ival;
		mpq_t rval;
		mpfr_t fval;
	} data;
	guint8 type;
} MpwRealNum;

/*any number (includes complex) so it includes an imaginary member if 
  i is not equal to gel_zero
  this is used as the number type*/
struct _mpw_t {
	MpwRealNum *r; /*real*/
	MpwRealNum *i; /*imaginary*/
};

#define MPW_IS_COMPLEX(n) ((n)->i != gel_zero)
#define MPW_IS_REAL(n) ((n)->i == gel_zero)

/* Should not be used outside */
extern MpwRealNum *gel_zero;
extern MpwRealNum *gel_one;

typedef struct _mpw_t mpw_t[1];
typedef struct _mpw_t *mpw_ptr;

/* FIXME: this is evil, error_num is used elsewhere, should
 * be some more generalized error interface */
enum {
	MPW_NO_ERROR=0,
	MPW_INTERNAL_ERROR,
	MPW_NUMERICAL_ERROR,
};

/*************************************************************************/
/*high level stuff                                                       */
/*************************************************************************/

/*set default precision*/
void mpw_set_default_prec(unsigned long int i);

/*initialize a number*/
void mpw_init_set(mpw_ptr rop,mpw_ptr op);

/* Maybe it is overkill to try to keep bin compat? */
void mpw_init(mpw_ptr op);
static inline void
mpw_init_inline (mpw_ptr op)
{
	op->r = gel_zero;
	gel_zero->alloc.usage++;
	op->i = gel_zero;
	gel_zero->alloc.usage++;
}
#define mpw_init(op) mpw_init_inline(op)

/* don't try to decomplexify the number */
#define mpw_init_set_no_uncomplex(rop,op) \
{	(rop)->r = (op)->r; \
	(rop)->r->alloc.usage++; \
	(rop)->i = (op)->i; \
	(rop)->i->alloc.usage++; }


/*clear memory held by number*/
void mpw_clear(mpw_ptr op);

/*make them the same type without loosing information*/
void mpw_make_same_type(mpw_ptr op1,mpw_ptr op2);

void mpw_set(mpw_ptr rop,mpw_ptr op);
void mpw_set_d(mpw_ptr rop,double d);
void mpw_set_d_complex (mpw_ptr rop, double real, double imag);
void mpw_set_si(mpw_ptr rop,signed long int i);
void mpw_set_ui(mpw_ptr rop,unsigned long int i);
void mpw_set_mpz_use (mpw_ptr rop, mpz_ptr op);
void mpw_set_mpq_use (mpw_ptr rop, mpq_ptr op);
void mpw_set_mpf_use (mpw_ptr rop, mpfr_ptr op);

/* Always use make_copy if peeking and then modifying the
 * peeked value!  Otherwise copy on write will bite us */
void mpw_make_copy (mpw_ptr op);
void mpw_make_copy_real (mpw_ptr op);
void mpw_make_copy_imag (mpw_ptr op);

mpz_ptr mpw_peek_real_mpz (mpw_ptr op);
mpq_ptr mpw_peek_real_mpq (mpw_ptr op);
mpfr_ptr mpw_peek_real_mpf (mpw_ptr op);

mpz_ptr mpw_peek_imag_mpz (mpw_ptr op);
mpq_ptr mpw_peek_imag_mpq (mpw_ptr op);
mpfr_ptr mpw_peek_imag_mpf (mpw_ptr op);

/* Just quick hacks to get a mpf, tmp should be an unused mpfr_t,
   rop should be mpfr_ptr and op should be mpw_ptr */
#define MPW_MPF_REAL(rop,op,tmp) { \
				   if (op->r->type == MPW_FLOAT) { \
					   rop = op->r->data.fval; \
				   } else if (op->r->type == MPW_INTEGER) { \
					   mpfr_init (tmp); \
					   mpfr_set_z (tmp, op->r->data.ival, GMP_RNDN); \
					   rop = tmp; \
				   } else /* if (op->r->type == MPW_RATIONAL) */ { \
					   mpfr_init (tmp); \
					   mpfr_set_q (tmp, op->r->data.rval, GMP_RNDN); \
					   rop = tmp; \
				   } \
			       }
#define MPW_MPF_IMAG(rop,op,tmp) { \
				   if (op->i->type == MPW_FLOAT) { \
					   rop = op->i->data.fval; \
				   } else if (op->i->type == MPW_INTEGER) { \
					   mpfr_init (tmp); \
					   mpfr_set_z (tmp, op->i->data.ival, GMP_RNDN); \
					   rop = tmp; \
				   } else /* if (op->r->type == MPW_RATIONAL) */ { \
					   mpfr_init (tmp); \
					   mpfr_set_q (tmp, op->i->data.rval, GMP_RNDN); \
					   rop = tmp; \
				   } \
			       }
#define MPW_MPF_KILL(rop,tmp) { if (rop == tmp) mpfr_clear (tmp); }


void mpw_abs(mpw_ptr rop,mpw_ptr op);
void mpw_abs_sq(mpw_ptr rop,mpw_ptr op);

int mpw_sgn(mpw_ptr op);

/* sign of the real part */
int mpw_re_sgn(mpw_ptr op) G_GNUC_PURE;
/* sign of the im part */
int mpw_im_sgn(mpw_ptr op) G_GNUC_PURE;

void mpw_neg(mpw_ptr rop,mpw_ptr op);

void mpw_add(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_add_ui(mpw_ptr rop,mpw_ptr op, unsigned long i);

void mpw_sub(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_sub_ui(mpw_ptr rop,mpw_ptr op, unsigned long i);
void mpw_ui_sub(mpw_ptr rop,unsigned long i, mpw_ptr op);

/* exact zero, not a float! */
gboolean mpw_exact_zero_p (mpw_ptr op) G_GNUC_PURE;

/* any zero will do */
gboolean mpw_zero_p (mpw_ptr op) G_GNUC_PURE;

void mpw_mul(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_mul_ui(mpw_ptr rop,mpw_ptr op, unsigned int i);

void mpw_div(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_div_ui(mpw_ptr rop,mpw_ptr op, unsigned int i);
void mpw_ui_div(mpw_ptr rop,unsigned int i,mpw_ptr op);

void mpw_mod(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);

void mpw_invert(mpw_ptr rop, mpw_ptr op1, mpw_ptr mod);
void mpw_gcd(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_lcm(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_jacobi(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_legendre(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_kronecker(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_lucnum (mpw_ptr rop, mpw_ptr op);
void mpw_nextprime (mpw_ptr rop, mpw_ptr op);
gboolean mpw_perfect_square(mpw_ptr op);
gboolean mpw_perfect_power(mpw_ptr op);
gboolean mpw_even_p(mpw_ptr op);
gboolean mpw_odd_p(mpw_ptr op);

void mpw_pow(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_pow_ui(mpw_ptr rop,mpw_ptr op, unsigned long int e);
void mpw_powm(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2, mpw_ptr mod);
void mpw_powm_ui(mpw_ptr rop,mpw_ptr op, unsigned long int e, mpw_ptr mod);
void mpw_sqrt(mpw_ptr rop,mpw_ptr op);

void mpw_exp(mpw_ptr rop,mpw_ptr op);
void mpw_ln(mpw_ptr rop,mpw_ptr op);
void mpw_log2(mpw_ptr rop,mpw_ptr op);
void mpw_log10(mpw_ptr rop,mpw_ptr op);

void mpw_sin(mpw_ptr rop,mpw_ptr op);
void mpw_cos(mpw_ptr rop,mpw_ptr op);
void mpw_sinh(mpw_ptr rop,mpw_ptr op);
void mpw_cosh(mpw_ptr rop,mpw_ptr op);
void mpw_arctan(mpw_ptr rop,mpw_ptr op);
void mpw_arctan2(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2);
void mpw_pi (mpw_ptr rop);
void mpw_ln2 (mpw_ptr rop);
void mpw_euler_constant (mpw_ptr rop);
void mpw_catalan_constant (mpw_ptr rop);
void mpw_i (mpw_ptr rop);
void mpw_rand (mpw_ptr rop);
void mpw_randint (mpw_ptr rop, mpw_ptr op);

void mpw_conj (mpw_ptr rop, mpw_ptr op);

int mpw_cmp(mpw_ptr op1, mpw_ptr op2);
int mpw_cmp_ui(mpw_ptr op, unsigned long int i);

gboolean mpw_eql(mpw_ptr op1, mpw_ptr op2);

/* must also be of same type! */
gboolean mpw_symbolic_eql(mpw_ptr op1, mpw_ptr op2);

gboolean mpw_eql_ui(mpw_ptr op, unsigned long int i);

void mpw_fac_ui(mpw_ptr rop,unsigned long int i);
void mpw_fac(mpw_ptr rop,mpw_ptr op);
void mpw_dblfac(mpw_ptr rop,mpw_ptr op);
void mpw_bin_ui(mpw_ptr rop, mpw_ptr op, unsigned long int r);

/*make a number int if possible*/
void mpw_make_int(mpw_ptr rop);

/*make number into a float, this might be neccessary for unprecise
  calculations*/
void mpw_make_float(mpw_ptr rop);

/*init the mp stuff*/
void mpw_init_mp(void);

/*get a string (g_malloc'ed) with the number in it*/
char * mpw_getstring (mpw_ptr num,
		      int max_digits,
		      gboolean scientific_notation,
		      gboolean results_as_floats,
		      gboolean mixed_fractions,
		      /* FIXME: solve dependency, this is defined in calc.h */
		      /* GelOutputStyle */int style,
		      int integer_output_base,
		      gboolean add_parenths);

char * mpw_getstring_chop (mpw_ptr num,
			   int max_digits,
			   gboolean scientific_notation,
			   gboolean results_as_floats,
			   gboolean mixed_fractions,
			   /* FIXME: solve dependency, this is defined in calc.h */
			   /* GelOutputStyle */int style,
			   int integer_output_base,
			   gboolean add_parenths,
			   int chop,
			   int chop_when,
			   gboolean force_chop);

void mpw_set_str_float(mpw_ptr rop,const char *s,int base);

/*reads only the imaginary part (use add for real part)*/
void mpw_set_str_complex(mpw_ptr rop,const char *s,int base);
void mpw_set_str_complex_int(mpw_ptr rop,const char *s,int base);

void mpw_set_str_int(mpw_ptr rop,const char *s,int base);

void mpw_set_str (mpw_ptr rop, const char *s, int base);

gboolean mpw_is_complex(mpw_ptr op);
gboolean mpw_is_integer(mpw_ptr op);
gboolean mpw_is_rational(mpw_ptr op);
gboolean mpw_is_float(mpw_ptr op);

#define mpw_is_real_part_float(op) ((op)->r->type == MPW_FLOAT)

#define mpw_is_complex_float(op) \
	 ( ((op)->r->type == MPW_FLOAT) || \
	   (MPW_IS_COMPLEX (op) && ((op)->i->type == MPW_FLOAT)) )

#define mpw_is_complex_rational_or_integer(op) \
	 ( ((op)->r->type <= MPW_RATIONAL) && \
	   ( ! MPW_IS_COMPLEX (op) || ((op)->i->type <= MPW_RATIONAL)) )

#define mpw_is_complex_integer(op) \
	 ( ((op)->r->type == MPW_INTEGER) && \
	   ( ! MPW_IS_COMPLEX (op) || ((op)->i->type == MPW_INTEGER)) )

void mpw_im(mpw_ptr rop, mpw_ptr op);
void mpw_re(mpw_ptr rop, mpw_ptr op);

void mpw_round(mpw_ptr rop, mpw_ptr op);
void mpw_floor(mpw_ptr rop, mpw_ptr op);
void mpw_ceil(mpw_ptr rop, mpw_ptr op);
void mpw_trunc(mpw_ptr rop, mpw_ptr op);

/*try to get a long number representation of the number*/
long mpw_get_long(mpw_ptr op);
unsigned long mpw_get_ulong(mpw_ptr op);
/*try to get a double representation of the number*/
double mpw_get_double(mpw_ptr op);
void mpw_get_complex_double(mpw_ptr op, double *r, double *i);

void mpw_denominator(mpw_ptr rop, mpw_ptr op);
void mpw_numerator(mpw_ptr rop, mpw_ptr op);

/* is the exponent less then -chop_when */
gboolean mpw_chop_p (mpw_ptr num, int chop_when);

#endif
