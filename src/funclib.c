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

#include "config.h"

#include <gnome.h>

#include <string.h>
#include <math.h>
#include <glib.h>
#include <limits.h>
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "funclib.h"
#include "calc.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "geloutput.h"

extern void (*errorout)(char *);
extern void (*infoout)(char *);
extern calc_error_t error_num;
extern int got_eof;
extern calcstate_t calcstate;

GelEFunc *_internal_ln_function = NULL;
GelEFunc *_internal_exp_function = NULL;

/*maximum number of primes to precalculate and store*/
#define MAXPRIMES 100000
GArray *primes = NULL;
int numprimes = 0;

static mpw_t e_cache;
static int e_iscached = FALSE;

void
gel_break_fp_caches(void)
{
	if(e_iscached) {
		e_iscached = FALSE;
		mpw_clear(e_cache);
	}
}

extern GelETree *free_trees;

static GelETree *
warranty_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	char *p;
	p = g_strdup_printf(_("Genius %s\n"
		    "%s\n\n"
		    "    This program is free software; you can redistribute it and/or modify\n"
		    "    it under the terms of the GNU General Public License as published by\n"
		    "    the Free Software Foundation; either version 2 of the License , or\n"
		    "    (at your option) any later version.\n"
		    "\n"
		    "    This program is distributed in the hope that it will be useful,\n"
		    "    but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		    "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		    "    GNU General Public License for more details.\n"
		    "\n"
		    "    You should have received a copy of the GNU General Public License\n"
		    "    along with this program. If not, write to the Free Software\n"
		    "    Foundation,  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,\n"
		    "    USA.\n"), 
			    VERSION,
			    COPYRIGHT_STRING);
	(*infoout)(p);
	g_free(p);
	error_num = IGNORE_ERROR;
	if(exception) *exception = TRUE; /*raise exception*/
	return NULL;
}

static GelETree *
exit_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	got_eof = TRUE;
	if(exception) *exception = TRUE; /*raise exception*/
	return NULL;
}

static GelETree *
ni_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	(*infoout)("We are the Knights Who Say... Ni!");
	if(exception) *exception = TRUE; /*raise exception*/
	error_num = IGNORE_ERROR;
	return NULL;
}

static GelETree *
shrubbery_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	(*infoout)("Then, when you have found the shrubbery, you must\n"
		   "cut down the mightiest tree in the forest... with...\n"
		   "A HERRING!");
	if(exception) *exception = TRUE; /*raise exception*/
	error_num = IGNORE_ERROR;
	return NULL;
}
	
/*error printing function*/
static GelETree *
error_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		(*errorout)(a[0]->str.str);
	else {
		GelOutput *gelo = gel_output_new();
		char *s;
		gel_output_setup_string (gelo, 0, NULL);
		pretty_print_etree(gelo, a[0]);
		s = gel_output_snarf_string(gelo);
		gel_output_unref(gelo);
		(*errorout)(s?s:"");
	}
	return gel_makenum_null();
}
/*print function*/
static GelETree *
print_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		gel_output_printf(main_out,"%s\n",a[0]->str.str);
	else {
		pretty_print_etree(main_out,a[0]);
		gel_output_string(main_out,"\n");
	}
	gel_output_flush(main_out);
	return gel_makenum_null();
}
/*print function*/
static GelETree *
printn_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		gel_output_printf(main_out, "%s", a[0]->str.str);
	else
		print_etree(main_out, a[0], TRUE);
	gel_output_flush(main_out);
	return gel_makenum_null();
}
/*print function*/
static GelETree *
display_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=STRING_NODE) {
		(*errorout)(_("display: first argument must be string!"));
		return NULL;
	}
	gel_output_printf(main_out, "%s: ", a[0]->str.str);
	pretty_print_etree(main_out, a[1]);
	gel_output_string(main_out, "\n");
	gel_output_flush(main_out);
	return gel_makenum_null();
}

/*print function*/
static GelETree *
set_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelToken *id;
	GelEFunc *func;
	if (a[0]->type != IDENTIFIER_NODE &&
	    a[0]->type != STRING_NODE) {
		(*errorout)(_("set: first argument must be an identifier or string!"));
		return NULL;
	}
	if (a[0]->type == IDENTIFIER_NODE) {
		id = a[0]->id.id;
	} else /* STRING_NODE */ {
		id = d_intern (a[0]->str.str);
	}

	if (id->protected) {
		(*errorout)(_("set: trying to set a protected id!"));
		return NULL;
	}

	func = d_makevfunc (id, copynode (a[1]));
	/* make function global */
	func->context = 0;
	d_addfunc_global (func);

	return copynode (a[1]);
}

static GelETree *
apply_func_to_matrixen(GelCtx *ctx, GelETree *mat1, GelETree *mat2,
		       GelETree * (*function)(GelCtx *ctx, GelETree **a,int *exception),
		       char *ident)
{
	GelMatrixW *m1 = NULL;
	GelMatrixW *m2 = NULL;
	GelMatrixW *new;
	GelETree *re_node = NULL;
	int reverse = FALSE;
	GelETree *n;
	int i,j;
	int quote = 0;

	if(mat1->type == MATRIX_NODE &&
	   mat2->type == MATRIX_NODE) {
		m1 = mat1->mat.matrix;
		m2 = mat2->mat.matrix;
		quote = mat1->mat.quoted || mat2->mat.quoted;
	} else if(mat1->type == MATRIX_NODE) {
		m1 = mat1->mat.matrix;
		quote = mat1->mat.quoted;
		re_node = mat2;
	} else /*if(mat2->type == MATRIX_NODE)*/ {
		m1 = mat2->mat.matrix;
		quote = mat2->mat.quoted;
		re_node = mat1;
		reverse = TRUE;
	}
	
	if(m2 && (gel_matrixw_width(m1) != gel_matrixw_width(m2) ||
		  gel_matrixw_height(m1) != gel_matrixw_height(m2))) {
		(*errorout)(_("Cannot apply function to two differently sized matrixes"));
		return NULL;
	}
	
	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	new = n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = quote;
	gel_matrixw_set_size(new,gel_matrixw_width(m1),gel_matrixw_height(m1));

	for(i=0;i<gel_matrixw_width(m1);i++) {
		for(j=0;j<gel_matrixw_height(m1);j++) {
			GelETree *t[2];
			GelETree *e;
			int ex = FALSE;
			if(!reverse) {
				t[0] = gel_matrixw_index(m1,i,j);
				t[1] = m2?gel_matrixw_index(m2,i,j):re_node;
			} else {
				t[0] = m2?gel_matrixw_index(m2,i,j):re_node;
				t[1] = gel_matrixw_index(m1,i,j);
			}
			e = (*function)(ctx, t,&ex);
			/*FIXME: handle exceptions*/
			if(!e) {
				GelETree *nn;
				GelETree *ni;
				GET_NEW_NODE(ni);
				ni->type = IDENTIFIER_NODE;
				ni->id.id = d_intern(ident);

				GET_NEW_NODE(nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_CALL;
				nn->op.nargs = 3;
				nn->op.args = ni;
				nn->op.args->any.next = copynode(t[0]);
				nn->op.args->any.next->any.next = copynode(t[1]);
				nn->op.args->any.next->any.next->any.next = NULL;

				gel_matrixw_set_index(new,i,j) = nn;
			} else {
				gel_matrixw_set_index(new,i,j) = e;
			}
		}
	}
	return n;
}

static GelETree *
apply_func_to_matrix (GelCtx *ctx, GelETree *mat, 
		      GelETree * (*function)(GelCtx *ctx, GelETree **a,int *exception),
		      char *ident)
{
	GelMatrixW *m;
	GelMatrixW *new;
	GelETree *n;
	int i,j;

	m = mat->mat.matrix;
	
	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	new = n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = mat->mat.quoted;
	gel_matrixw_set_size(new,gel_matrixw_width(m),gel_matrixw_height(m));

	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *t[1];
			GelETree *e;
			int ex = FALSE;
			t[0] = gel_matrixw_index(m,i,j);
			e = (*function)(ctx,t,&ex);
			/*FIXME: handle exceptions*/
			if(!e) {
				GelETree *nn;
				GelETree *ni;
				GET_NEW_NODE(nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_CALL;
				nn->op.args = NULL;
				nn->op.nargs = 2;
				
				GET_NEW_NODE(ni);
				ni->type = IDENTIFIER_NODE;
				ni->id.id = d_intern(ident);
				
				nn->op.args = ni;
				nn->op.args->any.next = copynode(t[0]);
				nn->op.args->any.next->any.next = NULL;

				gel_matrixw_set_index(new,i,j) = nn;
			} else if (e->type == VALUE_NODE &&
				   mpw_is_integer (e->val.value) &&
				   mpw_sgn (e->val.value) == 0) {
				gel_freetree (e);
				gel_matrixw_set_index(new,i,j) = NULL;
			} else {
				gel_matrixw_set_index(new,i,j) = e;
			}
		}
	}
	return n;
}

/* expand matrix function*/
static GelETree *
ExpandMatrix_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;

	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("ExpandMatrix: argument not a matrix"));
		return NULL;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy (a[0]->mat.matrix);
	gel_expandmatrix (n->mat.matrix);
	n->mat.quoted = 0;
	return n;
}

/*conj function*/
static GelETree *
conj_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrix (ctx, a[0], conj_op, "conj");

	if (a[0]->type != VALUE_NODE) {
		(*errorout)(_("conj: argument not a number"));
		return NULL;
	}

	mpw_init (fr);

	mpw_conj (fr, a[0]->val.value);

	return gel_makenum_use (fr);
}

/*sin function*/
static GelETree *
sin_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sin_op,"sin");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("sin: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_sin(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*sinh function*/
static GelETree *
sinh_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sinh_op,"sinh");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("sinh: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_sinh(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*cos function*/
static GelETree *
cos_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],cos_op,"cos");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("cos: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_cos(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*cosh function*/
static GelETree *
cosh_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],cosh_op,"cosh");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("cosh: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_cosh(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*tan function*/
static GelETree *
tan_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;
	mpw_t fr2;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],tan_op,"tan");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value)) {
		(*errorout)(_("tan: argument not a real number"));
		return NULL;
	}

	mpw_init(fr);
	mpw_set(fr,a[0]->val.value);

	/*is this algorithm always precise??? sin/cos*/
	mpw_init(fr2);
	mpw_cos(fr2,fr);
	mpw_sin(fr,fr);
	mpw_div(fr,fr,fr2);
	mpw_clear(fr2);

	return gel_makenum_use(fr);
}

/*atan (arctan) function*/
static GelETree *
atan_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],atan_op,"atan");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("atan: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_arctan(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}
	

/*e function (or e variable actually)*/
static GelETree *
e_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if(e_iscached)
		return gel_makenum(e_cache);

	mpw_init(e_cache);
	mpw_set_ui(e_cache,1);
	mpw_exp(e_cache,e_cache);
	e_iscached = TRUE;
	return gel_makenum(e_cache);
}

/*i function (or i variable actually)*/
static GelETree *
i_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t i;
	mpw_init (i);
	mpw_i (i);

	return gel_makenum_use (i);
}

/*pi function (or pi variable or whatever)*/
static GelETree *
pi_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	mpw_init(fr);
	mpw_pi(fr);

	return gel_makenum_use(fr);
}

static GelETree *
is_null_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==NULL_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_value_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==VALUE_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_string_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_matrix_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_function_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==FUNCTION_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_function_ref_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==OPERATOR_NODE &&
	   a[0]->op.oper == E_REFERENCE) {
		GelETree *arg = a[0]->op.args;
		g_assert(arg);
		if(arg->type==IDENTIFIER_NODE &&
		   d_lookup_global(arg->id.id))
			return gel_makenum_ui(1);
	}
	return gel_makenum_ui(0);
}
static GelETree *
is_complex_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_real_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else
		return gel_makenum_ui(1);
}
static GelETree *
is_integer_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_integer(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_rational_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_rational(a[0]->val.value) ||
		mpw_is_integer(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
is_float_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_float(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
trunc_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],trunc_op,"trunc");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("trunc: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_trunc(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
floor_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],floor_op,"floor");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("floor: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_floor(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
ceil_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],ceil_op,"ceil");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("ceil: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_ceil(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
round_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],round_op,"round");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("round: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_round(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
float_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],float_op,"float");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("float: argument not a number"));
		return NULL;
	}
	mpw_init_set(fr,a[0]->val.value);
	mpw_make_float(fr);
	return gel_makenum_use(fr);
}

static GelETree *
Numerator_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Numerator_op,"Numerator");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Numerator: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_numerator(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
Denominator_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Denominator_op,"Denominator");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Denominator: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_denominator(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
Re_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Re_op,"Re");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Re: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_re(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
Im_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Im_op,"Im");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Im: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_im(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
sqrt_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sqrt_op,"sqrt");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("sqrt: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_sqrt(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
exp_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE) {
		if(gel_matrixw_width(a[0]->mat.matrix) !=
		   gel_matrixw_height(a[0]->mat.matrix)) {
			(*errorout)(_("exp: matrix argument is not square"));
			return NULL;
		}
		return funccall(ctx,_internal_exp_function,a,1);
	}

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("exp: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_exp(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
ln_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],ln_op,"ln");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("ln: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_ln(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

/*gcd function*/
static GelETree *
gcd_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],gcd_op,"gcd");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("gcd: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_gcd(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*lcm function*/
static GelETree *
lcm_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;
	mpw_t prod;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],lcm_op,"lcm");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("lcm: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_gcd(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}
	mpw_init(prod);
	mpw_mul(prod,
		a[0]->val.value,
		a[1]->val.value);
	mpw_div(tmp,prod,tmp);
	mpw_clear(prod);

	return gel_makenum_use(tmp);
}

/*jacobi function*/
static GelETree *
jacobi_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],jacobi_op,"jacobi");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("jacobi: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_jacobi(tmp,
		   a[0]->val.value,
		   a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*legendre function*/
static GelETree *
legendre_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],legendre_op,"legendre");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("legendre: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_legendre(tmp,
		     a[0]->val.value,
		     a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*perfect square testing function*/
static GelETree *
PerfectSquare_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],PerfectSquare_op,"PerfectSquare");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("PerfectSquare: argument must be a number"));
		return NULL;
	}

	if(mpw_perfect_square(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}

/*max function*/
static GelETree *
max_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],max_op,"max");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("max: arguments must be numbers"));
		return NULL;
	}

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)>0)
		return gel_makenum(a[0]->val.value);
	else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum(a[1]->val.value);
	}
}

/*min function*/
static GelETree *
min_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],min_op,"min");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("min: arguments must be numbers"));
		return NULL;
	}

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)>0)
		return gel_makenum(a[1]->val.value);
	else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum(a[0]->val.value);
	}
}

static GelETree *
is_value_only_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("is_value_only: argument not a matrix"));
		return NULL;
	}
	
	if(gel_is_matrix_value_only(a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
I_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i,j;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("I: argument not an integer"));
		return NULL;
	}

	size = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(size<=0) {
		(*errorout)(_("I: argument can't be negative or 0"));
		return NULL;
	}
	if(size>INT_MAX) {
		(*errorout)(_("I: argument too large"));
		return NULL;
	}

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = 0;
	gel_matrixw_set_size(n->mat.matrix,size,size);
	
	for(i=0;i<size;i++)
		for(j=0;j<size;j++)
			if(i==j)
				gel_matrixw_set_index(n->mat.matrix,i,j) =
					gel_makenum_ui(1);

	return n;
}

static GelETree *
zeros_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long rows, cols;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value) ||
	   a[1]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[1]->val.value)) {
		(*errorout)(_("zeros: argument not an integer"));
		return NULL;
	}

	rows = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(rows<=0) {
		(*errorout)(_("zeros: argument can't be negative or 0"));
		return NULL;
	}
	if(rows>INT_MAX) {
		(*errorout)(_("zeros: argument too large"));
		return NULL;
	}
	cols = mpw_get_long(a[1]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(cols<=0) {
		(*errorout)(_("zeros: argument can't be negative or 0"));
		return NULL;
	}
	if(cols>INT_MAX) {
		(*errorout)(_("zeros: argument too large"));
		return NULL;
	}

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = 0;
	gel_matrixw_set_size(n->mat.matrix,cols,rows);
	
	return n;
}

static GelETree *
ones_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long rows, cols;
	int i, j;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value) ||
	   a[1]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[1]->val.value)) {
		(*errorout)(_("ones: argument not an integer"));
		return NULL;
	}

	rows = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(rows<=0) {
		(*errorout)(_("ones: argument can't be negative or 0"));
		return NULL;
	}
	if(rows>INT_MAX) {
		(*errorout)(_("ones: argument too large"));
		return NULL;
	}
	cols = mpw_get_long(a[1]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(cols<=0) {
		(*errorout)(_("ones: argument can't be negative or 0"));
		return NULL;
	}
	if(cols>INT_MAX) {
		(*errorout)(_("ones: argument too large"));
		return NULL;
	}

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = 0;
	gel_matrixw_set_size(n->mat.matrix,cols,rows);
	
	for(i=0;i<cols;i++)
		for(j=0;j<rows;j++)
			gel_matrixw_set_index(n->mat.matrix,i,j) =
				gel_makenum_ui(1);

	return n;
}

static GelETree *
rows_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("rows: argument not a matrix"));
		return NULL;
	}
	return gel_makenum_ui(gel_matrixw_height(a[0]->mat.matrix));
}
static GelETree *
columns_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("columns: argument not a matrix"));
		return NULL;
	}
	return gel_makenum_ui(gel_matrixw_width(a[0]->mat.matrix));
}
static GelETree *
SetMatrixSize_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long w,h;
	if(a[0]->type!=MATRIX_NODE ||
	   a[1]->type!=VALUE_NODE ||
	   a[2]->type!=VALUE_NODE) {
		(*errorout)(_("SetMatrixSize: wrong argument type"));
		return NULL;
	}
	w = mpw_get_long(a[1]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	h = mpw_get_long(a[2]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(w<=0 || h<=0) {
		(*errorout)(_("SetMatrixSize: rows/columns negative or 0"));
		return NULL;
	}
	if(w>INT_MAX || h>INT_MAX) {
		(*errorout)(_("SetMatrixSize: rows/columns too large"));
		return NULL;
	}
	n = copynode(a[0]);
	gel_matrixw_set_size(n->mat.matrix,h,w);
	return n;
}

static GelETree *
det_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t ret;
	if(a[0]->type!=MATRIX_NODE ||
	   !gel_is_matrix_value_only(a[0]->mat.matrix)) {
		(*errorout)(_("det: argument not a value only matrix"));
		return NULL;
	}
	mpw_init(ret);
	if(!gel_value_matrix_det(ret,a[0]->mat.matrix)) {
		mpw_clear(ret);
		return NULL;
	}
	return gel_makenum_use(ret);
}
static GelETree *
ref_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	if(a[0]->type!=MATRIX_NODE ||
	   !gel_is_matrix_value_only(a[0]->mat.matrix)) {
		(*errorout)(_("ref: argument not a value only matrix"));
		return NULL;
	}

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss(n->mat.matrix, FALSE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = 0;
	return n;
}
static GelETree *
rref_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	if(a[0]->type!=MATRIX_NODE ||
	   !gel_is_matrix_value_only(a[0]->mat.matrix)) {
		(*errorout)(_("rref: argument not a value only matrix"));
		return NULL;
	}

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss(n->mat.matrix, TRUE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = 0;
	return n;
}

static int
is_prime(unsigned long testnum)
{
	int i;
	unsigned long s = (unsigned long)sqrt(testnum);
	
	for(i=0;g_array_index(primes,unsigned long,i)<=s && i<numprimes;i++) {
		if((testnum%g_array_index(primes,unsigned long,i))==0) {
			return 0;
		}
	}
	return 1;
}


static GelETree *
prime_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	long num;
	unsigned long i;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],prime_op,"prime");

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("prime: argument not an integer"));
		return NULL;
	}

	num = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(num<=0) {
		(*errorout)(_("prime: argument can't be negative or 0"));
		return NULL;
	}
	if(num>MAXPRIMES) {
		(*errorout)(_("prime: argument is too large"));
		return NULL;
	}
	
	if(!primes) {
		unsigned long b;
		primes = g_array_new(FALSE,FALSE,sizeof(unsigned long));
		b = 2;
		primes = g_array_append_val(primes,b);
		b = 3;
		primes = g_array_append_val(primes,b);
		b = 5;
		primes = g_array_append_val(primes,b);
		b = 7;
		primes = g_array_append_val(primes,b);
		numprimes = 4;
	}
	
	if(num-1 < numprimes)
		return gel_makenum_ui(g_array_index(primes,unsigned long,num-1));
	

	primes = g_array_set_size(primes,num);
	for(i=g_array_index(primes,unsigned long,numprimes-1)+1;
	    numprimes<=num-1 && i<=ULONG_MAX;i++) {
		if(is_prime(i))
			g_array_index(primes,unsigned long,numprimes++) = i;
	}
	
	if(numprimes<=num-1) {
		(*errorout)(_("prime: argument is too large"));
		return NULL;
	}
	return gel_makenum_ui(g_array_index(primes,unsigned long,num-1));
}

static void
poly_cut_zeros(GelMatrixW *m)
{
	int i;
	int cutoff;
	for(i=gel_matrixw_width(m)-1;i>=1;i--) {
		GelETree *t = gel_matrixw_index(m,i,0);
	       	if(mpw_sgn(t->val.value)!=0)
			break;
	}
	cutoff = i+1;
	if(cutoff==gel_matrixw_width(m))
		return;
	gel_matrixw_set_size(m,cutoff,1);
}

static int
check_poly(GelETree * *a, int args, char *func, int complain)
{
	int i,j;

	for(j=0;j<args;j++) {
		if(a[j]->type!=MATRIX_NODE ||
		   gel_matrixw_height(a[j]->mat.matrix)!=1) {
			char buf[256];
			if(!complain) return FALSE;
			g_snprintf(buf,256,_("%s: arguments not horizontal vectors"),func);
			(*errorout)(buf);
			return FALSE;
		}

		for(i=0;i<gel_matrixw_width(a[j]->mat.matrix);i++) {
			GelETree *t = gel_matrixw_index(a[j]->mat.matrix,i,0);
			if(t->type != VALUE_NODE) {
				char buf[256];
				if(!complain) return FALSE;
				g_snprintf(buf,256,_("%s: arguments not numeric only vectors"),func);
				(*errorout)(buf);
				return FALSE;
			}
		}
	}
	return TRUE;
}

static GelETree *
addpoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i;
	GelMatrixW *m1,*m2,*mn;
	
	if(!check_poly(a,2,"addpoly",TRUE))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	size = MAX(gel_matrixw_width(m1), gel_matrixw_width(m2));
	gel_matrixw_set_size(mn,size,1);
	
	for(i=0;i<size;i++) {
		if(i<gel_matrixw_width(m1) &&
		   i<gel_matrixw_width(m2)) {
			GelETree *l,*r;
			mpw_t t;
			mpw_init(t);
			l = gel_matrixw_index(m1,i,0);
			r = gel_matrixw_index(m2,i,0);
			mpw_add(t,l->val.value,r->val.value);
			gel_matrixw_set_index(mn,i,0) = gel_makenum_use(t);
		} else if(i<gel_matrixw_width(m1)) {
			gel_matrixw_set_index(mn,i,0) =
				copynode(gel_matrixw_set_index(m1,i,0));
		} else /*if(i<gel_matrixw_width(m2)*/ {
			gel_matrixw_set_index(mn,i,0) =
				copynode(gel_matrixw_set_index(m2,i,0));
		}
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
subpoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i;
	GelMatrixW *m1,*m2,*mn;
	
	if(!check_poly(a,2,"subpoly",TRUE))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	size = MAX(gel_matrixw_width(m1), gel_matrixw_width(m2));
	gel_matrixw_set_size(mn,size,1);

	for(i=0;i<size;i++) {
		if(i<gel_matrixw_width(m1) &&
		   i<gel_matrixw_width(m2)) {
			GelETree *l,*r;
			mpw_t t;
			mpw_init(t);
			l = gel_matrixw_index(m1,i,0);
			r = gel_matrixw_index(m2,i,0);
			mpw_sub(t,l->val.value,r->val.value);
			gel_matrixw_set_index(mn,i,0) = gel_makenum_use(t);
		} else if(i<gel_matrixw_width(m1)) {
			gel_matrixw_set_index(mn,i,0) =
				copynode(gel_matrixw_set_index(m1,i,0));
		} else /*if(i<gel_matrixw_width(m2))*/ {
			GelETree *nn,*r;
			r = gel_matrixw_index(m2,i,0);
			nn = gel_makenum_ui(0);
			mpw_neg(nn->val.value,r->val.value);
			gel_matrixw_set_index(mn,i,0) = nn;
		}
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
mulpoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i,j;
	mpw_t accu;
	GelMatrixW *m1,*m2,*mn;
	
	if(!check_poly(a,2,"mulpoly",TRUE))
		return NULL;
	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	size = gel_matrixw_width(m1) + gel_matrixw_width(m2);
	gel_matrixw_set_size(mn,size,1);
	
	mpw_init(accu);
		
	for(i=0;i<gel_matrixw_width(m1);i++) {
		for(j=0;j<gel_matrixw_width(m2);j++) {
			GelETree *l,*r,*nn;
			l = gel_matrixw_index(m1,i,0);
			r = gel_matrixw_index(m2,j,0);
			if(mpw_sgn(l->val.value)==0 ||
			   mpw_sgn(r->val.value)==0)
				continue;
			mpw_mul(accu,l->val.value,r->val.value);
			nn = gel_matrixw_set_index(mn,i+j,0);
			if(nn)
				mpw_add(nn->val.value,nn->val.value,accu);
			else 
				gel_matrixw_set_index(mn,i+j,0) =
					gel_makenum(accu);
		}
	}

	mpw_clear(accu);
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
derpoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if(!check_poly(a,1,"derpoly",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	if(gel_matrixw_width(m)==1) {
		gel_matrixw_set_size(mn,1,1);
		return n;
	}
	gel_matrixw_set_size(mn,gel_matrixw_width(m)-1,1);
	
	for(i=1;i<gel_matrixw_width(m);i++) {
		GelETree *r;
		mpw_t t;
		mpw_init(t);
		r = gel_matrixw_index(m,i,0);
		mpw_mul_ui(t,r->val.value,i);
		gel_matrixw_set_index(mn,i-1,0) = gel_makenum_use(t);
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
der2poly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if(!check_poly(a,1,"der2poly",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	if(gel_matrixw_width(m)<=2) {
		gel_matrixw_set_size(mn,1,1);
		return n;
	}
	gel_matrixw_set_size(mn,gel_matrixw_width(m)-2,1);
	
	for(i=2;i<gel_matrixw_width(m);i++) {
		GelETree *r;
		mpw_t t;
		r = gel_matrixw_index(m,i,0);
		mpw_init(t);
		mpw_mul_ui(t,r->val.value,i*(i-1));
		gel_matrixw_set_index(mn,i-2,0) = gel_makenum_use(t);
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
trimpoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	
	if(!check_poly(a,1,"trimpoly",TRUE))
		return NULL;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	n->mat.quoted = 0;
	
	poly_cut_zeros(n->mat.matrix);

	return n;
}

static GelETree *
is_poly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(check_poly(a,1,"is_poly",FALSE))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
polytostring_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	int i;
	GString *gs;
	int any = FALSE;
	GelMatrixW *m;
	char *var;
	GelOutput *gelo;
	char *r;
	
	if(!check_poly(a,1,"polytostring",TRUE))
		return NULL;
	
	if(a[1]->type!=STRING_NODE) {
		(*errorout)(_("polytostring: 2nd argument not a string"));
		return NULL;
	}
	
	m = a[0]->mat.matrix;
	var = a[1]->str.str;
	
	gs = g_string_new("");

	gelo = gel_output_new();
	gel_output_setup_string(gelo, 0, NULL);
	gel_output_set_gstring(gelo, gs);

	for(i=gel_matrixw_width(m)-1;i>=0;i--) {
		GelETree *t;
		t = gel_matrixw_index(m,i,0);
		if(mpw_sgn(t->val.value)==0)
			continue;
		/*positive*/
		if(mpw_sgn(t->val.value)>0) {
			if(any) g_string_append(gs," + ");
			if(i==0)
				print_etree(gelo,t,TRUE);
			else if(mpw_cmp_ui(t->val.value,1)!=0) {
				print_etree(gelo,t,TRUE);
				g_string_append_c(gs,'*');
			}
			/*negative*/
		} else {
			if(any) g_string_append(gs," - ");
			else g_string_append_c(gs,'-');
			mpw_neg(t->val.value,t->val.value);
			if(i==0)
				print_etree(gelo,t,TRUE);
			else if(mpw_cmp_ui(t->val.value,1)!=0) {
				print_etree(gelo,t,TRUE);
				g_string_append_c(gs,'*');
			}
			mpw_neg(t->val.value,t->val.value);
		}
		if(i==1)
			g_string_sprintfa(gs,"%s",var);
		else if(i>1)
			g_string_sprintfa(gs,"%s^%d",var,i);
		any = TRUE;
	}
	if(!any)
		g_string_append(gs,"0");

	r = gel_output_snarf_string (gelo);
	gel_output_unref (gelo);

	GET_NEW_NODE(n);
	n->type = STRING_NODE;
	n->str.str = r;
	
	return n;
}

static GelETree *
ptf_makenew_power(GelToken *id, int power)
{
	GelETree *n;
	GelETree *tokn;
	GET_NEW_NODE(tokn);
	tokn->type = IDENTIFIER_NODE;
	tokn->id.id = id;

	if(power == 1)
		return tokn;

	GET_NEW_NODE(n);
	n->type = OPERATOR_NODE;
	n->op.oper = E_EXP;
	n->op.args = tokn;
	n->op.args->any.next = gel_makenum_ui(power);
	n->op.args->any.next->any.next = NULL;
	n->op.nargs = 2;

	return n;
}

static GelETree *
ptf_makenew_term(mpw_t mul, GelToken *id, int power)
{
	GelETree *n;
	
	/* we do the zero power the same as >1 so
	 * that we get an x^0 term.  This may seem
	 * pointless but it allows evaluating matrices
	 * as it will make the constant term act like
	 * c*I(n) */
	if (mpw_cmp_ui(mul,1)==0) {
		n = ptf_makenew_power(id,power);
	} else {
		GET_NEW_NODE(n);
		n->type = OPERATOR_NODE;
		n->op.oper = E_MUL;
		n->op.args = gel_makenum(mul);
		n->op.args->any.next = ptf_makenew_power(id,power);
		n->op.args->any.next->any.next = NULL;
		n->op.nargs = 2;
	}
	return n;
}

static GelETree *
polytofunc_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	GelETree *nn = NULL;
	int i;
	GelMatrixW *m;

	static GelToken *var = NULL;
	
	if(!check_poly(a,1,"polytofunc",TRUE))
		return NULL;
	
	if(!var)
		var = d_intern("x");
	
	m = a[0]->mat.matrix;

	for(i=gel_matrixw_width(m)-1;i>=0;i--) {
		GelETree *t;
		t = gel_matrixw_index(m,i,0);
		if(mpw_sgn(t->val.value)==0)
			continue;
		
		if(!nn)
			nn = ptf_makenew_term(t->val.value,var,i);
		else {
			GelETree *nnn;
			GET_NEW_NODE(nnn);
			nnn->type = OPERATOR_NODE;
			nnn->op.oper = E_PLUS;
			nnn->op.args = nn;
			nnn->op.args->any.next =
				ptf_makenew_term(t->val.value,var,i);
			nnn->op.args->any.next->any.next = NULL;
			nnn->op.nargs = 2;
			nn = nnn;
		}
	}
	if(!nn)
		nn = gel_makenum_ui(0);

	GET_NEW_NODE(n);
	n->type = FUNCTION_NODE;
	n->func.func = d_makeufunc(NULL,nn,g_slist_append(NULL,var),1);
	n->func.func->context = -1;

	return n;
}

static GelETree *
help_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GSList *funcs;
	GSList *li;
	
	funcs = d_getcontext();
	if(!funcs) return gel_makenum_null();
	funcs = g_slist_reverse(g_slist_copy(funcs));
	for(li=funcs;li;li=g_slist_next(li)) {
		GelEFunc *f = li->data;
		char *s;
		if(!f->id || !f->id->token ||
		   strcmp(f->id->token,"ni")==0 ||
		   strcmp(f->id->token,"shrubbery")==0)
			continue;
		s = g_strdup_printf("%-20s - %s",
				    f->id->token,get_description(f->id->token));
		(*infoout)(s);
		g_free(s);
	}
	g_slist_free(funcs);

	return gel_makenum_null();
}

static GelETree *
sethelp_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=STRING_NODE ||
	   a[1]->type!=STRING_NODE) {
		(*errorout)(_("sethelp: arguments must be strings (function name,help text)"));
		return NULL;
	}
	
	add_description(a[0]->str.str,a[1]->str.str);

	return gel_makenum_null();
}

static GelETree *
protect_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelToken *tok;

	if(a[0]->type!=STRING_NODE) {
		(*errorout)(_("protect: argument must be a string"));
		return NULL;
	}
	
	tok = d_intern(a[0]->str.str);
	tok->protected = 1;

	return gel_makenum_null();
}

static GelETree *
unprotect_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelToken *tok;

	if(a[0]->type!=STRING_NODE) {
		(*errorout)(_("unprotect: argument must be a string"));
		return NULL;
	}
	
	tok = d_intern(a[0]->str.str);
	tok->protected = 0;

	return gel_makenum_null();
}

static GelETree *
set_float_prec_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	long bits;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("set_float_prec: argument not an integer"));
		return NULL;
	}

	bits = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(bits<60 || bits>16384) {
		(*errorout)(_("set_float_prec: argument should be between 60 and 16384"));
		return NULL;
	}
	
	if(calcstate.float_prec != bits) {
		calcstate.float_prec = bits;
		mpw_set_default_prec(calcstate.float_prec);
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.float_prec);
}

static GelETree *
get_float_prec_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	return gel_makenum_ui(calcstate.float_prec);
}

static GelETree *
set_max_digits_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	long digits;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("set_max_digits: argument not an integer"));
		return NULL;
	}

	digits = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(digits<0 || digits>256) {
		(*errorout)(_("set_max_digits: argument should be between 0 and 256"));
		return NULL;
	}
	
	if(calcstate.max_digits != digits) {
		calcstate.max_digits = digits;
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.max_digits);
}

static GelETree *
get_max_digits_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	return gel_makenum_ui(calcstate.max_digits);
}

static GelETree *
set_results_as_floats_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("set_results_as_floats: argument not a value"));
		return NULL;
	}
	calcstate.results_as_floats = mpw_sgn(a[0]->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.results_as_floats)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_results_as_floats_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(calcstate.results_as_floats)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
set_scientific_notation_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("set_scientific_notation: argument not a value"));
		return NULL;
	}
	calcstate.scientific_notation = mpw_sgn(a[0]->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.scientific_notation)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_scientific_notation_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(calcstate.scientific_notation)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
set_full_expressions_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("set_full_expressions: argument not a value"));
		return NULL;
	}
	calcstate.full_expressions = mpw_sgn(a[0]->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.full_expressions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_full_expressions_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(calcstate.full_expressions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
set_output_style_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	const char *token;
	GelOutputStyle output_style = GEL_OUTPUT_NORMAL;

	if (a[0]->type != STRING_NODE) {
		(*errorout)(_("set_output_style: argument not a string"));
		return NULL;
	}

	token = a[0]->str.str;

	if (token != NULL && strcmp (token, "normal") == 0) {
		output_style = GEL_OUTPUT_NORMAL;
	} else if (token != NULL && strcmp (token, "troff") == 0) {
		output_style = GEL_OUTPUT_TROFF;
	} else if (token != NULL && strcmp (token, "latex") == 0) {
		output_style = GEL_OUTPUT_LATEX;
	} else {
		(*errorout)(_("set_output_style: argument not one of normal, troff or latex"));
		return NULL;
	}

	calcstate.output_style = output_style;
	if (statechange_hook)
		(*statechange_hook)(calcstate);

	return gel_makenum_string (token);
}

static GelETree *
get_output_style_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	const char *token;

	token = "normal";
	if (calcstate.output_style == GEL_OUTPUT_TROFF)
		token = "troff";
	else if (calcstate.output_style == GEL_OUTPUT_LATEX)
		token = "latex";

	return gel_makenum_string (token);
}

static GelETree *
set_max_errors_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	long errors;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("set_max_errors: argument not an integer"));
		return NULL;
	}

	errors = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(errors<0) {
		(*errorout)(_("set_max_errors: argument should be larger or equal to 0"));
		return NULL;
	}
	
	if(calcstate.max_errors != errors) {
		calcstate.max_errors = errors;
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.max_errors);
}

static GelETree *
get_max_errors_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	return gel_makenum_ui(calcstate.max_errors);
}

static GelETree *
set_mixed_fractions_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("set_mixed_fractions: argument not a value"));
		return NULL;
	}
	calcstate.mixed_fractions = mpw_sgn(a[0]->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.mixed_fractions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_mixed_fractions_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(calcstate.mixed_fractions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
set_integer_output_base_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	long base;

	if(a[0]->type!=VALUE_NODE ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("set_integer_output_base: argument not an integer"));
		return NULL;
	}

	base = mpw_get_long(a[0]->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(base<2 || base>36) {
		(*errorout)(_("set_integer_output_base: argument should be between 2 and 36"));
		return NULL;
	}
	
	if(calcstate.integer_output_base != base) {
		calcstate.integer_output_base = base;
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.integer_output_base);
}

static GelETree *
get_integer_output_base_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	return gel_makenum_ui(calcstate.integer_output_base);
}

/*add the routines to the dictionary*/
void
gel_funclib_addall(void)
{
	d_addfunc(d_makebifunc(d_intern("warranty"),warranty_op,0));
	add_description("warranty",_("Gives the warranty information"));
	d_addfunc(d_makebifunc(d_intern("exit"),exit_op,0));
	add_description("exit",_("Exits the program"));
	d_addfunc(d_makebifunc(d_intern("quit"),exit_op,0));
	add_description("quit",_("Exits the program"));
	d_addfunc(d_makebifunc(d_intern("error"),error_op,1));
	add_description("error",_("Prints a string to the error stream"));
	d_addfunc(d_makebifunc(d_intern("print"),print_op,1));
	add_description("print",_("Prints an expression"));
	d_addfunc(d_makebifunc(d_intern("printn"),printn_op,1));
	add_description("printn",_("Prints an expression without a trailing newline"));
	d_addfunc(d_makebifunc(d_intern("display"),display_op,2));
	add_description("display",_("Display a string and an expression"));

	d_addfunc(d_makebifunc(d_intern("set"),set_op,2));
	add_description("set",_("Set a global variable"));

	d_addfunc(d_makebifunc(d_intern("set_float_prec"),set_float_prec_op,1));
	add_description("set_float_prec",_("Set floating point precision"));
	d_addfunc(d_makebifunc(d_intern("get_float_prec"),get_float_prec_op,0));
	add_description("get_float_prec",_("Get floating point precision"));
	d_addfunc(d_makebifunc(d_intern("set_max_digits"),set_max_digits_op,1));
	add_description("set_max_digits",_("Set maximum digits to display"));
	d_addfunc(d_makebifunc(d_intern("get_max_digits"),get_max_digits_op,0));
	add_description("get_max_digits",_("Get maximum digits to display"));
	d_addfunc(d_makebifunc(d_intern("set_results_as_floats"),set_results_as_floats_op,1));
	d_addfunc(d_makebifunc(d_intern("get_results_as_floats"),get_results_as_floats_op,0));
	d_addfunc(d_makebifunc(d_intern("set_scientific_notation"),set_scientific_notation_op,1));
	d_addfunc(d_makebifunc(d_intern("get_scientific_notation"),get_scientific_notation_op,0));
	d_addfunc(d_makebifunc(d_intern("set_full_expressions"),set_full_expressions_op,1));
	d_addfunc(d_makebifunc(d_intern("get_full_expressions"),get_full_expressions_op,0));
	d_addfunc(d_makebifunc(d_intern("set_max_errors"),set_max_errors_op,1));
	add_description("set_max_errors",_("Set maximum number of errors printed"));
	d_addfunc(d_makebifunc(d_intern("get_max_errors"),get_max_errors_op,0));
	add_description("get_max_errors",_("Get maximum number of errors printed"));
	d_addfunc(d_makebifunc(d_intern("set_mixed_fractions"),set_mixed_fractions_op,1));
	add_description("set_mixed_fractions",_("Set if we print fractions in mixed format"));
	d_addfunc(d_makebifunc(d_intern("get_mixed_fractions"),get_mixed_fractions_op,0));
	add_description("get_mixed_fractions",_("Get if we print fractions in mixed format"));
	d_addfunc(d_makebifunc(d_intern("set_integer_output_base"),set_integer_output_base_op,1));
	add_description("set_integer_output_base",_("Set the integer output base"));
	d_addfunc(d_makebifunc(d_intern("get_integer_output_base"),get_integer_output_base_op,0));
	add_description("get_integer_output_base",_("Get the integer output base"));
	d_addfunc(d_makebifunc(d_intern("set_output_style"),set_output_style_op,1));
	d_addfunc(d_makebifunc(d_intern("get_output_style"),get_output_style_op,0));

	d_addfunc(d_makebifunc(d_intern("ni"),ni_op,0));
	d_addfunc(d_makebifunc(d_intern("shrubbery"),shrubbery_op,0));
	d_addfunc(d_makebifunc(d_intern("ExpandMatrix"),ExpandMatrix_op,1));
	add_description("ExpandMatrix",_("Expands a matrix just like we do on unquoted matrix input"));
	d_addfunc(d_makebifunc(d_intern("conj"),conj_op,1));
	add_description("conj",_("Calculates the conjugate"));
	d_addfunc(d_makebifunc(d_intern("sin"),sin_op,1));
	add_description("sin",_("Calculates the sine function"));
	d_addfunc(d_makebifunc(d_intern("cos"),cos_op,1));
	add_description("cos",_("Calculates the cosine function"));
	d_addfunc(d_makebifunc(d_intern("sinh"),sinh_op,1));
	add_description("sinh",_("Calculates the hyperbolic sine function"));
	d_addfunc(d_makebifunc(d_intern("cosh"),cosh_op,1));
	add_description("cosh",_("Calculates the hyperbolic cosine function"));
	d_addfunc(d_makebifunc(d_intern("tan"),tan_op,1));
	add_description("tan",_("Calculates the tan function"));
	d_addfunc(d_makebifunc(d_intern("atan"),atan_op,1));
	add_description("atan",_("Calculates the arctan function"));
	d_addfunc(d_makebifunc(d_intern("pi"),pi_op,0));
	add_description("pi",_("The number pi"));
	d_addfunc(d_makebifunc(d_intern("e"),e_op,0));
	add_description("e",_("The natural number e"));
	d_addfunc(d_makebifunc(d_intern("i"),i_op,0));
	add_description("i",_("The imaginary number"));
	d_addfunc(d_makebifunc(d_intern("sqrt"),sqrt_op,1));
	add_description("sqrt",_("The square root"));
	d_addfunc(d_makebifunc(d_intern("exp"),exp_op,1));
	add_description("exp",_("The exponential function"));
	d_addfunc(d_makebifunc(d_intern("ln"),ln_op,1));
	add_description("ln",_("The natural logarithm function"));
	d_addfunc(d_makebifunc(d_intern("gcd"),gcd_op,2));
	add_description("gcd",_("Greatest common divisor"));
	d_addfunc(d_makebifunc(d_intern("lcm"),lcm_op,2));
	add_description("lcm",_("Least common multiplier"));
	d_addfunc(d_makebifunc(d_intern("jacobi"),jacobi_op,2));
	d_addfunc(d_makebifunc(d_intern("legendre"),legendre_op,2));
	d_addfunc(d_makebifunc(d_intern("PerfectSquare"),PerfectSquare_op,1));
	add_description("PerfectSquare",_("Check a number for being a perfect square"));
	d_addfunc(d_makebifunc(d_intern("max"),max_op,2));
	add_description("max",_("Return the larger of two arguments"));
	d_addfunc(d_makebifunc(d_intern("min"),min_op,2));
	add_description("min",_("Return the smaller of two arguments"));
	d_addfunc(d_makebifunc(d_intern("prime"),prime_op,1));
	add_description("prime",_("Return the n'th prime (up to a limit)"));
	d_addfunc(d_makebifunc(d_intern("round"),round_op,1));
	add_description("round",_("Round a number"));
	d_addfunc(d_makebifunc(d_intern("floor"),floor_op,1));
	add_description("floor",_("Get the highest integer less then or equal to n"));
	d_addfunc(d_makebifunc(d_intern("ceil"),ceil_op,1));
	add_description("ceil",_("Get the lowest integer more then or equal to n"));
	d_addfunc(d_makebifunc(d_intern("trunc"),trunc_op,1));
	add_description("trunc",_("Truncate a number to an integer"));
	d_addfunc(d_makebifunc(d_intern("float"),float_op,1));
	add_description("float",_("Make number a float"));
	d_addfunc(d_makebifunc(d_intern("Numerator"),Numerator_op,1));
	add_description("Numerator",_("Get the Numerator of a rational"));
	d_addfunc(d_makebifunc(d_intern("Denominator"),Denominator_op,1));
	add_description("Denominator",_("Get the Denominator of a rational"));
	d_addfunc(d_makebifunc(d_intern("Re"),Re_op,1));
	add_description("Re",_("Get the real part of a complex number"));
	d_addfunc(d_makebifunc(d_intern("Im"),Im_op,1));
	add_description("Im",_("Get the imaginary part of a complex number"));
	d_addfunc(d_makebifunc(d_intern("I"),I_op,1));
	add_description("I",_("Make an identity matrix of a given size"));
	d_addfunc(d_makebifunc(d_intern("eye"),I_op,1));
	add_description("eye",_("Make an identity matrix of a given size"));
	d_addfunc(d_makebifunc(d_intern("zeros"),zeros_op,2));
	add_description("zeros",_("Make an matrix of all zeros"));
	d_addfunc(d_makebifunc(d_intern("ones"),ones_op,2));
	add_description("ones",_("Make an matrix of all ones"));
	d_addfunc(d_makebifunc(d_intern("rows"),rows_op,1));
	add_description("rows",_("Get the rows of a matrix"));
	d_addfunc(d_makebifunc(d_intern("columns"),columns_op,1));
	add_description("columns",_("Get the columns of a matrix"));
	d_addfunc(d_makebifunc(d_intern("SetMatrixSize"),SetMatrixSize_op,3));
	add_description("SetMatrixSize",_("Make new matrix of given size from old one"));
	d_addfunc(d_makebifunc(d_intern("det"),det_op,1));
	add_description("det",_("Get the determinant of a matrix"));
	d_addfunc(d_makebifunc(d_intern("ref"),ref_op,1));
	add_description("ref",_("Get the row echelon form of a matrix"));
	d_addfunc(d_makebifunc(d_intern("rref"),rref_op,1));
	add_description("rref",_("Get the reduced row echelon form of a matrix"));
	d_addfunc(d_makebifunc(d_intern("is_value_only"),is_value_only_op,1));
	add_description("is_value_only",_("Check if a matrix is a value only matrix"));
	d_addfunc(d_makebifunc(d_intern("is_null"),is_null_op,1));
	add_description("is_null",_("Check if null"));
	d_addfunc(d_makebifunc(d_intern("is_value"),is_value_op,1));
	add_description("is_value",_("Check if a number"));
	d_addfunc(d_makebifunc(d_intern("is_string"),is_string_op,1));
	add_description("is_string",_("Check if a string"));
	d_addfunc(d_makebifunc(d_intern("is_matrix"),is_matrix_op,1));
	add_description("is_matrix",_("Check if a matrix"));
	d_addfunc(d_makebifunc(d_intern("is_function"),is_function_op,1));
	add_description("is_function",_("Check if a function"));
	d_addfunc(d_makebifunc(d_intern("is_function_ref"),is_function_ref_op,1));
	add_description("is_function_ref",_("Check if a function reference"));
	d_addfunc(d_makebifunc(d_intern("is_complex"),is_complex_op,1));
	add_description("is_complex",_("Check if a number is complex"));
	d_addfunc(d_makebifunc(d_intern("is_real"),is_real_op,1));
	add_description("is_real",_("Check if a number is real"));
	d_addfunc(d_makebifunc(d_intern("is_integer"),is_integer_op,1));
	add_description("is_integer",_("Check if a real number is an integer"));
	d_addfunc(d_makebifunc(d_intern("is_rational"),is_rational_op,1));
	add_description("is_rational",_("Check if a real number is rational"));
	d_addfunc(d_makebifunc(d_intern("is_float"),is_float_op,1));
	add_description("is_float",_("Check if a real number is a float"));
	d_addfunc(d_makebifunc(d_intern("addpoly"),addpoly_op,2));
	d_addfunc(d_makebifunc(d_intern("subpoly"),subpoly_op,2));
	d_addfunc(d_makebifunc(d_intern("mulpoly"),mulpoly_op,2));
	d_addfunc(d_makebifunc(d_intern("derpoly"),derpoly_op,1));
	d_addfunc(d_makebifunc(d_intern("der2poly"),der2poly_op,1));
	d_addfunc(d_makebifunc(d_intern("trimpoly"),trimpoly_op,1));
	d_addfunc(d_makebifunc(d_intern("is_poly"),is_poly_op,1));
	d_addfunc(d_makebifunc(d_intern("polytostring"),polytostring_op,2));
	d_addfunc(d_makebifunc(d_intern("polytofunc"),polytofunc_op,1));
	d_addfunc(d_makebifunc(d_intern("help"),help_op,0));
	add_description("help",_("Display function list with small help"));
	d_addfunc(d_makebifunc(d_intern("sethelp"),sethelp_op,2));
	add_description("sethelp",_("Set the help line for a function"));
	d_addfunc(d_makebifunc(d_intern("protect"),protect_op,1));
	add_description("protect",_("Protect a variable from being modified"));
	d_addfunc(d_makebifunc(d_intern("unprotect"),unprotect_op,1));
	add_description("unprotect",_("Unprotect a variable from being modified"));

	/*temporary until well done internal functions are done*/
	_internal_ln_function = d_makeufunc(d_intern("<internal>ln"),
					    /*FIXME:this is not the correct 
					      function*/
					    parseexp("error(\"ln not finished\")",
						     NULL, FALSE, FALSE,
						     NULL, NULL),
					    g_slist_append(NULL,d_intern("x")),1);
	_internal_exp_function = d_makeufunc(d_intern("<internal>exp"),
					     parseexp("s = float(x^0); "
						      "fact = 1; "
						      "for i = 1 to 100 do "
						      "(fact = fact * x / i; "
						      "s = s + fact) ; s",
						      NULL, FALSE, FALSE,
						      NULL, NULL),
					     g_slist_append(NULL,d_intern("x")),1);
	/*protect EVERYthing up to this point*/
	d_protect_all();
}
