/* GENIUS Calculator
 * Copyright (C) 1997-2003 George Lebl
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
#include "mpzextra.h"
#include "eval.h"
#include "dict.h"
#include "funclib.h"
#include "calc.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "geloutput.h"

extern calcstate_t calcstate;

GelEFunc *_internal_ln_function = NULL;
GelEFunc *_internal_exp_function = NULL;

/*maximum number of primes to precalculate and store*/
#define MAXPRIMES 30000
GArray *primes = NULL;
int numprimes = 0;

static mpw_t e_cache;
static int e_iscached = FALSE;
static mpw_t golden_ratio_cache;
static int golden_ratio_iscached = FALSE;

#define RAISE_EXCEPTION(e) { if ((e) != NULL) *(e) = TRUE; }

static inline gboolean
check_argument_integer (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != VALUE_NODE ||
		       mpw_is_complex(a[argnum]->val.value) ||
		       ! mpw_is_integer (a[argnum]->val.value)) {
		gel_errorout (_("%s: argument number %d not an integer"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_integer_or_matrix (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY ((a[argnum]->type != VALUE_NODE ||
			mpw_is_complex(a[argnum]->val.value) ||
			! mpw_is_integer (a[argnum]->val.value)) &&
		       a[argnum]->type != MATRIX_NODE) {
		gel_errorout (_("%s: argument number %d not an integer or a matrix"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}


static inline gboolean
check_argument_positive_integer (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != VALUE_NODE ||
		       mpw_is_complex(a[argnum]->val.value) ||
		       ! mpw_is_integer (a[argnum]->val.value) ||
		       mpw_sgn (a[argnum]->val.value) < 0) {
		gel_errorout (_("%s: argument number %d not a positive integer"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_number (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type!=VALUE_NODE) {
		gel_errorout (_("%s: argument number %d not a number"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_real_number (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type!=VALUE_NODE ||
		       mpw_is_complex(a[argnum]->val.value)) {
		gel_errorout (_("%s: argument number %d not a real number"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_matrix (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != MATRIX_NODE) {
		gel_errorout (_("%s: argument number %d not a matrix"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_value_only_matrix (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != MATRIX_NODE ||
		       ! gel_is_matrix_value_only (a[argnum]->mat.matrix)) {
		gel_errorout (_("%s: argument number %d not a value only matrix"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_string (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != STRING_NODE) {
		gel_errorout (_("%s: argument number %d not a string"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_string_or_identifier (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != STRING_NODE &&
		       a[argnum]->type != IDENTIFIER_NODE) {
		gel_errorout (_("%s: argument number %d not a string"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

void
gel_break_fp_caches (void)
{
	if (e_iscached) {
		e_iscached = FALSE;
		mpw_clear (e_cache);
	}
	if (golden_ratio_iscached) {
		golden_ratio_iscached = FALSE;
		mpw_clear (golden_ratio_cache);
	}
}

static int
get_nonnegative_integer (mpw_ptr z, const char *funcname)
{
	long i;
	i = mpw_get_long(z);
	if G_UNLIKELY (error_num != 0) {
		error_num = 0;
		return -1;
	}
	if G_UNLIKELY (i <= 0) {
		gel_errorout (_("%s: argument can't be negative or 0"), funcname);
		return -1;
	}
	if G_UNLIKELY (i > INT_MAX) {
		gel_errorout (_("%s: argument too large"), funcname);
		return -1;
	}
	return i;
}

static GelETree *
manual_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GString *str;
	FILE *fp;

	str = g_string_new (NULL);

	fp = fopen ("../doc/manual.txt", "r");
	if G_LIKELY (fp == NULL)
		fp = fopen (LIBRARY_DIR "/manual.txt", "r");

	if G_UNLIKELY (fp != NULL) {
		char buf[256];
		while (fgets (buf, sizeof(buf), fp) != NULL) {
			g_string_append (str, buf);
		}

		fclose (fp);
	} else {
		g_string_append (str,
				 _("Cannot locate the manual"));
	}

	(*infoout) (str->str);
	error_num = IGNORE_ERROR;

	RAISE_EXCEPTION (exception);

	g_string_free (str, TRUE);

	return NULL;
}

static GelETree *
warranty_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_infoout (_("Genius %s\n"
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
	error_num = IGNORE_ERROR;
	RAISE_EXCEPTION (exception);
	return NULL;
}

static GelETree *
exit_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_got_eof = TRUE;
	RAISE_EXCEPTION (exception);
	return NULL;
}

static GelETree *
ninini_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_infoout ("We are the Knights Who Say... Ni!");
	RAISE_EXCEPTION (exception);
	error_num = IGNORE_ERROR;
	return NULL;
}

static GelETree *
shrubbery_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_infoout ("Then, when you have found the shrubbery, you must\n"
		     "cut down the mightiest tree in the forest... with...\n"
		     "A HERRING!");
	RAISE_EXCEPTION (exception);
	error_num = IGNORE_ERROR;
	return NULL;
}
	
/*error printing function*/
static GelETree *
error_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == STRING_NODE) {
		gel_errorout (a[0]->str.str);
	} else {
		GelOutput *gelo = gel_output_new();
		char *s;
		gel_output_setup_string (gelo, 0, NULL);
		pretty_print_etree(gelo, a[0]);
		s = gel_output_snarf_string(gelo);
		gel_output_unref(gelo);
		gel_errorout (s != NULL ? s : "");
		g_free (s);
	}
	return gel_makenum_null();
}
/*print function*/
static GelETree *
print_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type==STRING_NODE) {
		gel_output_printf_full (main_out, FALSE, "%s\n", a[0]->str.str);
	} else {
		/* FIXME: whack limit */
		pretty_print_etree (main_out, a[0]);
		gel_output_string (main_out,"\n");
	}
	gel_output_flush (main_out);
	return gel_makenum_null();
}
/*print function*/
static GelETree *
chdir_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_string (a, 0, "chdir"))
		return NULL;
	return gel_makenum_si (chdir (a[0]->str.str));
}
/*print function*/
static GelETree *
printn_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
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
display_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_string (a, 0, "display"))
		return NULL;
	gel_output_printf(main_out, "%s: ", a[0]->str.str);
	pretty_print_etree(main_out, a[1]);
	gel_output_string(main_out, "\n");
	gel_output_flush(main_out);
	return gel_makenum_null();
}

/*set function*/
static GelETree *
set_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *id;
	GelEFunc *func;
	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "set"))
		return NULL;
	if (a[0]->type == IDENTIFIER_NODE) {
		id = a[0]->id.id;
	} else /* STRING_NODE */ {
		id = d_intern (a[0]->str.str);
	}

	if G_UNLIKELY (id->protected) {
		gel_errorout (_("%s: trying to set a protected id!"),
			      "set");
		return NULL;
	}
	if G_UNLIKELY (id->parameter) {
		/* FIXME: fix this, this should just work too */
		gel_errorout (_("%s: trying to set a parameter, use the equals sign"),
			      "set");
		return NULL;
	}

	func = d_makevfunc (id, copynode (a[1]));
	/* make function global */
	func->context = 0;
	d_addfunc_global (func);

	return copynode (a[1]);
}

/*rand function*/
static GelETree *
rand_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int args;

	args = 0;
	while (a != NULL && a[args] != NULL)
		args++;

	if G_UNLIKELY (args > 2) {
		gel_errorout (_("%s: Too many arguments, should be at most %d"),
				"rand", 2);
		return NULL;
	}

	if (args == 0) {
		mpw_t fr; 
		mpw_init (fr);
		mpw_rand (fr);

		return gel_makenum_use (fr);
	} else if (args == 1) {
		GelETree *n;
		GelMatrix *m;
		int size, i;

		if ( ! check_argument_integer (a, 0, "rand"))
			return NULL;

		size = get_nonnegative_integer (a[0]->val.value, "rand");
		if (size < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, size, 1, FALSE /* padding */);
		for (i = 0; i < size; i++) {
			mpw_t fr; 
			mpw_init (fr);
			mpw_rand (fr);

			gel_matrix_index (m, i, 0) = gel_makenum_use (fr);
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = FALSE;

		return n;
	} else /* args == 2 */ {
		GelETree *n;
		GelMatrix *m;
		int sizex, sizey, i, j;

		if ( ! check_argument_integer (a, 0, "rand") ||
		     ! check_argument_integer (a, 1, "rand"))
			return NULL;

		sizey = get_nonnegative_integer (a[0]->val.value, "rand");
		if (sizey < 0)
			return NULL;
		sizex = get_nonnegative_integer (a[1]->val.value, "rand");
		if (sizex < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, sizex, sizey, FALSE /* padding */);
		for (i = 0; i < sizex; i++) {
			for (j = 0; j < sizey; j++) {
				mpw_t fr; 
				mpw_init (fr);
				mpw_rand (fr);

				gel_matrix_index (m, i, j) = gel_makenum_use (fr);
			}
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = FALSE;

		return n;
	}
}

/*rand function*/
static GelETree *
randint_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int args;

	args = 0;
	while (a[args] != NULL)
		args++;

	if G_UNLIKELY (args > 3) {
		gel_errorout (_("%s: Too many arguments, should be at most %d"),
				"randint", 3);
		return NULL;
	}

	if (args == 1) {
		mpw_t fr; 

		if ( ! check_argument_integer (a, 0, "randint"))
			return NULL;

		mpw_init (fr);
		mpw_randint (fr, a[0]->val.value);
		if (error_num != 0) {
			mpw_clear (fr);
			return NULL;
		}

		return gel_makenum_use (fr);
	} else if (args == 2) {
		GelETree *n;
		GelMatrix *m;
		int size, i;

		if ( ! check_argument_integer (a, 0, "randint") ||
		     ! check_argument_integer (a, 1, "randint"))
			return NULL;

		size = get_nonnegative_integer (a[1]->val.value, "randint");
		if (size < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, size, 1, FALSE /* padding */);
		for (i = 0; i < size; i++) {
			mpw_t fr;
			mpw_init (fr);
			mpw_randint (fr, a[0]->val.value);
			if (error_num != 0) {
				mpw_clear (fr);
				/* This can only happen if a[0]->val.value is
				 * evil, in which case we have not set any
				 * elements yet.  So we don't have to free any
				 * elements yet */
				g_assert (i == 0);
				gel_matrix_free (m);
				return NULL;
			}

			gel_matrix_index (m, i, 0) = gel_makenum_use (fr);
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = FALSE;

		return n;
	} else /* args == 3 */ {
		GelETree *n;
		GelMatrix *m;
		int sizex, sizey, i, j;

		if ( ! check_argument_integer (a, 0, "randint") ||
		     ! check_argument_integer (a, 1, "randint") ||
		     ! check_argument_integer (a, 2, "randint"))
			return NULL;

		sizey = get_nonnegative_integer (a[1]->val.value, "randint");
		if (sizey < 0)
			return NULL;
		sizex = get_nonnegative_integer (a[2]->val.value, "randint");
		if (sizex < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, sizex, sizey, FALSE /* padding */);
		for (i = 0; i < sizex; i++) {
			for (j = 0; j < sizey; j++) {
				mpw_t fr;
				mpw_init (fr);
				mpw_randint (fr, a[0]->val.value);
				if (error_num != 0) {
					mpw_clear (fr);
					/* This can only happen if a[0]->val.value is
					 * evil, in which case we have not set any
					 * elements yet.  So we don't have to free any
					 * elements yet */
					g_assert (i == 0 && j == 0);
					gel_matrix_free (m);
					return NULL;
				}

				gel_matrix_index (m, i, j) = gel_makenum_use (fr);
			}
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = FALSE;

		return n;
	}
}

static GelETree *
apply_func_to_matrixen (GelCtx *ctx,
			GelETree *mat1,
			GelETree *mat2,
			GelBIFunction function,
			const char *ident,
			gboolean *exception)
{
	GelMatrixW *m1 = NULL;
	GelMatrixW *m2 = NULL;
	GelMatrixW *new;
	GelETree *re_node = NULL;
	gboolean reverse = FALSE;
	GelETree *n;
	int i,j;
	int quote = 0;
	gboolean internal_exception = FALSE; 

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
	
	if G_UNLIKELY (m2 && (gel_matrixw_width(m1) != gel_matrixw_width(m2) ||
			      gel_matrixw_height(m1) != gel_matrixw_height(m2))) {
		gel_errorout (_("Cannot apply function to two differently sized matrixes"));
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
			if(!reverse) {
				t[0] = gel_matrixw_index(m1,i,j);
				t[1] = m2?gel_matrixw_index(m2,i,j):re_node;
			} else {
				t[0] = m2?gel_matrixw_index(m2,i,j):re_node;
				t[1] = gel_matrixw_index(m1,i,j);
			}
			if G_LIKELY ( ! internal_exception)
				e = (*function) (ctx, t, &internal_exception);
			else
				e = NULL;

			if G_UNLIKELY (e == NULL) {
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

	if G_UNLIKELY (internal_exception) {
		RAISE_EXCEPTION (exception);
	}
		
	return n;
}

static GelETree *
apply_func_to_matrix (GelCtx *ctx,
		      GelETree *mat, 
		      GelBIFunction function,
		      const char *ident, 
		      gboolean *exception)
{
	GelMatrixW *m;
	GelMatrixW *new;
	GelETree *n;
	int i,j;
	gboolean internal_exception = FALSE; 

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
			t[0] = gel_matrixw_index(m,i,j);

			if G_LIKELY ( ! internal_exception)
				e = (*function) (ctx, t, &internal_exception);
			else
				e = NULL;
			if G_UNLIKELY (e == NULL) {
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
				   ! mpw_is_complex (e->val.value) &&
				   mpw_is_integer (e->val.value) &&
				   mpw_sgn (e->val.value) == 0) {
				gel_freetree (e);
				gel_matrixw_set_index(new,i,j) = NULL;
			} else {
				gel_matrixw_set_index(new,i,j) = e;
			}
		}
	}

	if G_UNLIKELY (internal_exception) {
		RAISE_EXCEPTION (exception);
	}
		
	return n;
}

/* expand matrix function*/
static GelETree *
ExpandMatrix_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "ExpandMatrix"))
		return NULL;

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy (a[0]->mat.matrix);
	gel_expandmatrix (n);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
RowsOf_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "RowsOf"))
		return NULL;

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_rowsof (a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
ColumnsOf_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "ColumnsOf"))
		return NULL;

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_columnsof (a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
DiagonalOf_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "DiagonalOf"))
		return NULL;

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_diagonalof (a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	return n;
}

/*ComplexConjugate function*/
static GelETree *
ComplexConjugate_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrix (ctx, a[0], ComplexConjugate_op, "ComplexConjugate", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "ComplexConjugate"))
		return NULL;

	mpw_init (fr);

	mpw_conj (fr, a[0]->val.value);

	return gel_makenum_use (fr);
}

/*sin function*/
static GelETree *
sin_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sin_op,"sin", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "sin"))
		return NULL;

	mpw_init(fr);

	mpw_sin(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*sinh function*/
static GelETree *
sinh_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sinh_op,"sinh", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "sinh"))
		return NULL;

	mpw_init(fr);

	mpw_sinh(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*cos function*/
static GelETree *
cos_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],cos_op,"cos", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "cos"))
		return NULL;

	mpw_init(fr);

	mpw_cos(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*cosh function*/
static GelETree *
cosh_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],cosh_op,"cosh", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "cosh"))
		return NULL;

	mpw_init(fr);

	mpw_cosh(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*tan function*/
static GelETree *
tan_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;
	mpw_t fr2;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],tan_op,"tan", exception);

	if G_UNLIKELY ( ! check_argument_real_number (a, 0, "tan"))
		return NULL;

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
atan_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],atan_op,"atan", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "atan"))
		return NULL;

	mpw_init(fr);

	mpw_arctan(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}
	

/*e function (or e variable actually)*/
static GelETree *
e_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (e_iscached)
		return gel_makenum (e_cache);

	mpw_init (e_cache);
	mpw_set_ui (e_cache,1);
	mpw_exp (e_cache, e_cache);
	e_iscached = TRUE;
	return gel_makenum (e_cache);
}

/* Free fall accelleration */
static GelETree *
Gravity_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t g;
	mpw_init (g);
	mpw_set_d (g, 9.80665);
	return gel_makenum_use (g);
}

/* EulerConstant */
static GelETree *
EulerConstant_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t e;
	mpw_init (e);
	mpw_euler_constant (e);
	return gel_makenum_use (e);
}

/*pi function (or pi variable or whatever)*/
static GelETree *
pi_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr; 
	mpw_init (fr);
	mpw_pi (fr);

	return gel_makenum_use (fr);
}

static GelETree *
GoldenRatio_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (golden_ratio_iscached)
		return gel_makenum (golden_ratio_cache);

	mpw_init (golden_ratio_cache);
	mpw_set_ui (golden_ratio_cache, 5);
	mpw_sqrt (golden_ratio_cache, golden_ratio_cache);
	mpw_add_ui (golden_ratio_cache, golden_ratio_cache, 1);
	mpw_div_ui (golden_ratio_cache, golden_ratio_cache, 2);
	golden_ratio_iscached = TRUE;
	return gel_makenum (golden_ratio_cache);
}

static GelETree *
IsNull_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==NULL_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsValue_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==VALUE_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsString_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==STRING_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsMatrix_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsFunction_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==FUNCTION_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsFunctionRef_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
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
IsComplex_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsReal_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else
		return gel_makenum_ui(1);
}
static GelETree *
IsMatrixReal_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixReal"))
		return NULL;

	if (gel_is_matrix_value_only_real (a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
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
IsPositiveInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_integer(a[0]->val.value) &&
		mpw_sgn (a[0]->val.value) > 0)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsGaussInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex_integer(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsMatrixInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixInteger"))
		return NULL;

	if (gel_is_matrix_value_only_integer (a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsRational_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
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
IsComplexRational_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if (mpw_is_complex_rational_or_integer (a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsMatrixRational_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixRational"))
		return NULL;

	if (gel_is_matrix_value_only_rational (a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsFloat_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
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
trunc_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],trunc_op,"trunc", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "trunc"))
		return NULL;
	mpw_init(fr);
	mpw_trunc(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
floor_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],floor_op,"floor", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "floor"))
		return NULL;
	mpw_init(fr);
	mpw_floor(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
ceil_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],ceil_op,"ceil", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "ceil"))
		return NULL;
	mpw_init(fr);
	mpw_ceil(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
round_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],round_op,"round", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "round"))
		return NULL;
	mpw_init(fr);
	mpw_round(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
float_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],float_op,"float", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "float"))
		return NULL;
	mpw_init_set(fr,a[0]->val.value);
	mpw_make_float(fr);
	return gel_makenum_use(fr);
}

static GelETree *
Numerator_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Numerator_op,"Numerator", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Numerator"))
		return NULL;
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
Denominator_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Denominator_op,"Denominator", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Denominator"))
		return NULL;
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
Re_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Re_op,"Re", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Re"))
		return NULL;
	mpw_init(fr);
	mpw_re(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
Im_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Im_op,"Im", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Im"))
		return NULL;
	mpw_init(fr);
	mpw_im(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
sqrt_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sqrt_op,"sqrt", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "sqrt"))
		return NULL;
	if (ctx->modulo != NULL) {
		GelETree *arg[3];
		GelETree *ret;
		gboolean is_prime;
		mpz_ptr num;
		mpz_t tmp;
		GelEFunc *SqrtModPrime;
		static GelToken *SqrtModPrime_id = NULL;

		if G_UNLIKELY ( ! check_argument_integer (a, 0, "sqrt"))
			return NULL;

		MPW_MPZ_REAL (num, ctx->modulo, tmp);
		is_prime = mympz_is_prime (num, -1);
		MPW_MPZ_KILL (num, tmp);

		if ( ! is_prime) {
			gel_errorout (_("%s: square root for composite moduli "
					"is not yet implemented"), "sqrt");
			return NULL;
		}
		if (SqrtModPrime_id == NULL)
			SqrtModPrime_id = d_intern ("SqrtModPrime");
		SqrtModPrime = d_lookup_only_global (SqrtModPrime_id);
		if (SqrtModPrime == NULL) {
			gel_errorout (_("%s: Cannot find square root function "
					"for prime moduli"), "sqrt");
			return NULL;
		}
		arg[0] = a[0];
		arg[1] = gel_makenum (ctx->modulo);
		arg[2] = NULL;
		ret = funccall (ctx, SqrtModPrime, arg, 2);
		gel_freetree (arg[1]);

		return ret;
	} else {
		mpw_t fr;

		mpw_init(fr);
		mpw_sqrt(fr,a[0]->val.value);
		return gel_makenum_use(fr);
	}
}

static GelETree *
exp_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE) {
		if(gel_matrixw_width(a[0]->mat.matrix) !=
		   gel_matrixw_height(a[0]->mat.matrix)) {
			gel_errorout (_("%s: matrix argument is not square"),
				      "exp");
			return NULL;
		}
		return funccall(ctx,_internal_exp_function,a,1);
	}

	if G_UNLIKELY ( ! check_argument_number (a, 0, "exp"))
		return NULL;
	mpw_init(fr);
	mpw_exp(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
ln_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],ln_op,"ln", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "ln"))
		return NULL;
	mpw_init(fr);
	mpw_ln(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
log2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],log2_op,"log2", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "log2"))
		return NULL;
	mpw_init(fr);
	mpw_log2(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
log10_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],log10_op,"log10", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "log10"))
		return NULL;
	mpw_init(fr);
	mpw_log10(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

/*gcd function*/
static GelETree *
gcd2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],gcd2_op,"gcd", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "gcd") ||
			! check_argument_integer (a, 1, "gcd"))
		return NULL;

	mpw_init(tmp);
	mpw_gcd(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*gcd function*/
static GelETree *
gcd_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *gcd;
	int i;

	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			mpw_t gcd;
			if ( ! gel_is_matrix_value_only_integer (a[0]->mat.matrix)) {
				gel_errorout (_("%s: matrix argument must be integer only"),
					      "gcd");
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			mpw_init (gcd);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (i == 0 && j == 0) {
						mpw_abs (gcd, n->val.value);
					} else {
						mpw_gcd (gcd, gcd, n->val.value);
					}
				}
			}
			return gel_makenum_use (gcd);
		} else if (a[0]->type == VALUE_NODE) {
			mpw_t tmp;
			if (mpw_is_complex (a[0]->val.value) ||
			    ! mpw_is_integer (a[0]->val.value)) {
				gel_errorout (_("%s: argument must be an integer"),
					      "gcd");
				return NULL;
			}
			mpw_init (tmp);
			mpw_abs (tmp, a[0]->val.value);
			return gel_makenum_use (tmp);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	gcd = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { gcd, a[i] };
		GelETree *res;
		res = gcd2_op (ctx, argv, exception);
		if (res == NULL) {
			if (gcd != a[0])
				gel_freetree (gcd);
			return NULL;
		}
		if (gcd != a[0])
			gel_freetree (gcd);
		gcd = res;
	}
	if (gcd == a[0]) {
		mpw_t tmp;
		mpw_init (tmp);
		mpw_abs (tmp, a[0]->val.value);
		return gel_makenum_use (tmp);
	} else {
		return gcd;
	}

}

/*lcm function*/
static GelETree *
lcm2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],lcm2_op,"lcm", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "lcm") ||
			! check_argument_integer (a, 1, "lcm"))
		return NULL;

	mpw_init(tmp);
	mpw_lcm(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*lcm function*/
static GelETree *
lcm_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *lcm;
	int i;

	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			mpw_t lcm;
			if ( ! gel_is_matrix_value_only_integer (a[0]->mat.matrix)) {
				gel_errorout (_("%s: matrix argument must be integer only"),
					      "lcm");
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			mpw_init (lcm);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (i == 0 && j == 0) {
						mpw_set (lcm, n->val.value);
					} else {
						mpw_lcm (lcm, lcm, n->val.value);
					}
				}
			}
			return gel_makenum_use (lcm);
		} else if (a[0]->type == VALUE_NODE) {
			mpw_t tmp;
			if (mpw_is_complex (a[0]->val.value) ||
			    ! mpw_is_integer (a[0]->val.value)) {
				gel_errorout (_("%s: argument must be an integer"),
					      "lcm");
				return NULL;
			}
			mpw_init (tmp);
			mpw_abs (tmp, a[0]->val.value);
			return gel_makenum_use (tmp);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	lcm = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { lcm, a[i] };
		GelETree *res;
		res = lcm2_op (ctx, argv, exception);
		if (res == NULL) {
			if (lcm != a[0])
				gel_freetree (lcm);
			return NULL;
		}
		if (lcm != a[0])
			gel_freetree (lcm);
		lcm = res;
	}
	if (lcm == a[0]) {
		mpw_t tmp;
		mpw_init (tmp);
		mpw_abs (tmp, a[0]->val.value);
		return gel_makenum_use (tmp);
	} else {
		return lcm;
	}
}

/*jacobi function*/
static GelETree *
Jacobi_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],Jacobi_op,"Jacobi", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Jacobi") ||
			! check_argument_integer (a, 1, "Jacobi"))
		return NULL;

	mpw_init(tmp);
	mpw_jacobi(tmp,
		   a[0]->val.value,
		   a[1]->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*kronecker function*/
static GelETree *
JacobiKronecker_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1], JacobiKronecker_op, "JacobiKronecker", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "JacobiKronecker") ||
			! check_argument_integer (a, 1, "JacobiKronecker"))
		return NULL;

	mpw_init(tmp);
	mpw_kronecker(tmp,
		      a[0]->val.value,
		      a[1]->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*legendre function*/
static GelETree *
Legendre_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],Legendre_op,"Legendre", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Legendere") ||
			! check_argument_integer (a, 1, "Legendere"))
		return NULL;

	mpw_init(tmp);
	mpw_legendre(tmp,
		     a[0]->val.value,
		     a[1]->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*perfect square testing function*/
static GelETree *
IsPerfectSquare_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],IsPerfectSquare_op,"IsPerfectSquare", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsPerfectSquare"))
		return NULL;

	if(mpw_perfect_square(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if G_UNLIKELY (error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}


/*perfect square testing function*/
static GelETree *
IsPerfectPower_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],IsPerfectPower_op,"IsPerfectPower", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsPerfectPower"))
		return NULL;

	if(mpw_perfect_power(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if G_UNLIKELY (error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}

static GelETree *
IsEven_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],IsEven_op,"IsEven", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsEven"))
		return NULL;

	if(mpw_even_p(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if G_UNLIKELY (error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}

static GelETree *
IsOdd_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],IsOdd_op,"IsOdd", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsOdd"))
		return NULL;

	if(mpw_odd_p(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if G_UNLIKELY (error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}

/*max function for two elements */
static GelETree *
max2_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],max2_op,"max", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "max") ||
			! check_argument_number (a, 1, "max"))
		return NULL;

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)<0)
		return gel_makenum(a[1]->val.value);
	else {
		if G_UNLIKELY (error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum(a[0]->val.value);
	}
}

/*max function*/
static GelETree *
max_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *max = NULL;
	int i;
	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			if G_UNLIKELY ( ! gel_is_matrix_value_only (a[0]->mat.matrix)) {
				gel_errorout (_("%s: matrix argument must be value only"),
					      "max");
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (max == NULL) {
						max = n;
					} else if (max != n) {
						if (mpw_cmp (n->val.value, max->val.value) > 0)
							max = n;
					}
				}
			}
			g_assert (max != NULL);
			return gel_makenum (max->val.value);
		} else if (a[0]->type == VALUE_NODE) {
			return copynode (a[0]);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	max = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { max, a[i] };
		GelETree *res;
		res = max2_op (ctx, argv, exception);
		if (res == NULL) {
			if (max != a[0])
				gel_freetree (max);
			return NULL;
		}
		if (max != a[0])
			gel_freetree (max);
		max = res;
	}
	if (max == a[0])
		return copynode (a[0]);
	else
		return max;
}

/*min function*/
static GelETree *
min2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],min2_op,"min", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "min") ||
			! check_argument_number (a, 1, "min"))
		return NULL;

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)>0)
		return gel_makenum(a[1]->val.value);
	else {
		if G_UNLIKELY (error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum(a[0]->val.value);
	}
}

/*min function*/
static GelETree *
min_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *min = NULL;
	int i;
	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			if ( ! gel_is_matrix_value_only (a[0]->mat.matrix)) {
				gel_errorout (_("%s: matrix argument must be value only"),
					      "min");
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (min == NULL) {
						min = n;
					} else if (min != n) {
						if (mpw_cmp (n->val.value, min->val.value) < 0)
							min = n;
					}
				}
			}
			g_assert (min != NULL);
			return gel_makenum (min->val.value);
		} else if (a[0]->type == VALUE_NODE) {
			return copynode (a[0]);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	min = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { min, a[i] };
		GelETree *res;
		res = min2_op (ctx, argv, exception);
		if (res == NULL) {
			if (min != a[0])
				gel_freetree (min);
			return NULL;
		}
		if (min != a[0])
			gel_freetree (min);
		min = res;
	}
	if (min == a[0])
		return copynode (a[0]);
	else
		return min;
}

static GelETree *
IsValueOnly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsValueOnly"))
		return NULL;
	
	if(gel_is_matrix_value_only(a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
I_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long size;
	int i,j;
	static int cached_size = -1;
	static GelMatrixW *cached_m = NULL;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "I"))
		return NULL;

	size = get_nonnegative_integer (a[0]->val.value, "I");
	if G_UNLIKELY (size < 0)
		return NULL;

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.quoted = FALSE;

	if (cached_size == size) {
		n->mat.matrix = gel_matrixw_copy (cached_m);
	} else {
		if (cached_m != NULL)
			gel_matrixw_free (cached_m);
		n->mat.matrix = gel_matrixw_new();
		gel_matrixw_set_size(n->mat.matrix,size,size);

		for(i=0;i<size;i++)
			for(j=0;j<size;j++)
				if(i==j)
					gel_matrixw_set_index(n->mat.matrix,i,j) =
						gel_makenum_ui(1);

		cached_m = gel_matrixw_copy (n->mat.matrix);
		cached_size = -1;
	}

	return n;
}

static GelETree *
zeros_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long rows, cols;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "zeros") ||
			(a[1] != NULL &&
			 ! check_argument_integer (a, 1, "zeros")))
		return NULL;

	if G_UNLIKELY (a[1] != NULL && a[2] != NULL) {
		gel_errorout (_("%s: too many arguments"), "zeros");
		return NULL;
	}

	rows = get_nonnegative_integer (a[0]->val.value, "zeros");
	if G_UNLIKELY (rows < 0)
		return NULL;
	if (a[1] != NULL) {
		cols = get_nonnegative_integer (a[1]->val.value, "zeros");
		if G_UNLIKELY (cols < 0)
			return NULL;
	} else {
		/* In this case we want a row vector */
		cols = rows;
		rows = 1;
	}

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size(n->mat.matrix,cols,rows);
	
	return n;
}

static GelETree *
ones_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long rows, cols;
	int i, j;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "ones") ||
			(a[1] != NULL &&
			 ! check_argument_integer (a, 1, "ones")))
		return NULL;

	if G_UNLIKELY (a[1] != NULL && a[2] != NULL) {
		gel_errorout (_("%s: too many arguments"), "ones");
		return NULL;
	}

	rows = get_nonnegative_integer (a[0]->val.value, "ones");
	if (rows < 0)
		return NULL;
	if (a[1] != NULL) {
		cols = get_nonnegative_integer (a[1]->val.value, "ones");
		if (cols < 0)
			return NULL;
	} else {
		/* In this case we want a row vector */
		cols = rows;
		rows = 1;
	}

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size(n->mat.matrix,cols,rows);
	
	for(i=0;i<cols;i++)
		for(j=0;j<rows;j++)
			gel_matrixw_set_index(n->mat.matrix,i,j) =
				gel_makenum_ui(1);

	return n;
}

static GelETree *
rows_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == NULL_NODE)
		return gel_makenum_ui (0);
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "rows"))
		return NULL;
	return gel_makenum_ui(gel_matrixw_height(a[0]->mat.matrix));
}
static GelETree *
columns_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == NULL_NODE)
		return gel_makenum_ui (0);
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "columns"))
		return NULL;
	return gel_makenum_ui(gel_matrixw_width(a[0]->mat.matrix));
}
static GelETree *
elements_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == NULL_NODE)
		return gel_makenum_ui (0);
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "elements"))
		return NULL;
	return gel_makenum_ui (gel_matrixw_width (a[0]->mat.matrix) *
			       gel_matrixw_height (a[0]->mat.matrix));
}
static GelETree *
IsMatrixSquare_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixSquare"))
		return NULL;
	if (gel_matrixw_width (a[0]->mat.matrix) == gel_matrixw_height (a[0]->mat.matrix))
		return gel_makenum_ui (1);
	else
		return gel_makenum_ui (0);
}
static GelETree *
SetMatrixSize_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long w,h;

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "SetMatrixSize"))
		return NULL;
	if G_UNLIKELY ( ! check_argument_number (a, 1, "SetMatrixSize"))
		return NULL;
	if G_UNLIKELY ( ! check_argument_number (a, 2, "SetMatrixSize"))
		return NULL;

	w = get_nonnegative_integer (a[1]->val.value, "SetMatrixSize");
	if G_UNLIKELY (w < 0)
		return NULL;
	h = get_nonnegative_integer (a[2]->val.value, "SetMatrixSize");
	if G_UNLIKELY (h < 0)
		return NULL;

	n = copynode(a[0]);
	gel_matrixw_set_size (n->mat.matrix, h, w);
	return n;
}

static GelETree *
IndexComplement_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrix *nm;
	GelMatrixW *m;
	int nml;
	char *index;
	int i, ii, ml;
	int len;

	if G_UNLIKELY ( ! check_argument_integer_or_matrix (a, 0, "IndexComplement") ||
			! check_argument_integer (a, 1, "IndexComplement"))
		return NULL;

	len = get_nonnegative_integer (a[1]->val.value, "IndexComplement");
	if G_UNLIKELY (len < 0)
		return NULL;
	if (a[0]->type == MATRIX_NODE) {
		index = g_new0 (char, len);

		m = a[0]->mat.matrix;
		ml = gel_matrixw_elements (m);
		nml = len;
		for (i = 0; i < ml; i++) {
			GelETree *t = gel_matrixw_vindex (m, i);
			int elt;
			if G_UNLIKELY (t->type != VALUE_NODE) {
				(*errorout)(_("IndexComplement: vector argument not value only"));
				g_free (index);
				return NULL;
			}
			elt = get_nonnegative_integer (t->val.value, "IndexComplement");
			if G_UNLIKELY (elt < 0) {
				g_free (index);
				return NULL;
			}
			elt--;
			if G_UNLIKELY (elt >= len) {
				(*errorout)(_("IndexComplement: vector argument has too large entries"));
				g_free (index);
				return NULL;
			}

			if (index[elt] == 0) {
				nml --;
				index[elt] = 1;
			}
		}

		if (nml <= 0)
			return gel_makenum_null ();

		nm = gel_matrix_new ();
		gel_matrix_set_size (nm, nml, 1, FALSE /* padding */);
		ii = 0;
		for (i = 0; i < len; i++) {
			if (index[i] == 0) {
				gel_matrix_index (nm, ii++, 0) = gel_makenum_ui (i+1);
			}
		}

		g_free (index);
	} else {
		int elt = get_nonnegative_integer (a[0]->val.value, "IndexComplement");
		if G_UNLIKELY (elt < 0)
			return NULL;
		if G_UNLIKELY (elt > len) {
			(*errorout)(_("IndexComplement: vector argument has too large entries"));
			return NULL;
		}
		if (len == 1 && elt == 1)
			return gel_makenum_null ();

		nm = gel_matrix_new ();
		gel_matrix_set_size (nm, len-1, 1, FALSE /* padding */);
		ii = 0;
		for (i = 1; i <= len; i++) {
			if (i != elt)
				gel_matrix_index (nm, ii++, 0) = gel_makenum_ui (i);
		}
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (nm);
	if (a[0]->type == MATRIX_NODE)
		n->mat.quoted = a[0]->mat.quoted;
	else
		n->mat.quoted = TRUE;

	return n;
}

static GelETree *
det_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t ret;
	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "det"))
		return NULL;
	mpw_init(ret);
	if G_UNLIKELY ( ! gel_value_matrix_det (ctx, ret, a[0]->mat.matrix)) {
		mpw_clear(ret);
		return NULL;
	}
	return gel_makenum_use(ret);
}
static GelETree *
ref_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "ref"))
		return NULL;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss (ctx, n->mat.matrix, FALSE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = FALSE;
	return n;
}
static GelETree *
rref_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "rref"))
		return NULL;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss (ctx, n->mat.matrix, TRUE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = FALSE;
	return n;
}

static GelEFunc *
get_reference (GelETree *a, const char *argname, const char *func)
{
	if G_LIKELY (a->type == OPERATOR_NODE &&
		     a->op.oper == E_REFERENCE) {
		GelETree *arg = a->op.args;
		g_assert(arg);
		if G_UNLIKELY (arg->type != IDENTIFIER_NODE ||
			       d_lookup_global (arg->id.id) == NULL) {
			gel_errorout (_("%s: %s not a reference"),
				      func, argname);
			return NULL;
		}
		return d_lookup_global (arg->id.id);
	} else {
		gel_errorout (_("%s: %s not a reference"),
			      func, argname);
		return NULL;
	}
}

static gboolean
is_row_zero (GelMatrixW *m, int r)
{
	int i;
	int w = gel_matrixw_width (m);
	for (i = 0; i < w; i++) {
		GelETree *node = gel_matrixw_set_index (m, i, r);
		if (node != NULL &&
		    (node->type != VALUE_NODE ||
		     /* FIXME: perhaps use some zero tolerance */
		     mpw_sgn (node->val.value) != 0)) {
			return FALSE;
		}
	}
	return TRUE;
}

/* FIXME: work in modulo mode */
static GelETree *
SolveLinearSystem_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *RM, *RV;
	GelETree *n;
	GelEFunc *retm = NULL;
	GelEFunc *retv = NULL;
	gboolean ret;

	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "SolveLinearSystem") ||
			! check_argument_value_only_matrix (a, 1, "SolveLinearSystem"))
		return NULL;

	if G_UNLIKELY (gel_matrixw_height(a[0]->mat.matrix) != 
		       gel_matrixw_height(a[1]->mat.matrix)) {
		gel_errorout (_("%s: matrices not of the same height"), "SolveLinearSystem");
		return NULL;
	}

	if (a[2] != NULL) {
		retm = get_reference (a[2], _("third argument"),
				      "SolveLinearSystem");
		if G_UNLIKELY (retm == NULL)
			return NULL;
		if (a[3] != NULL) {
			retv = get_reference (a[3], _("fourth argument"),
					      "SolveLinearSystem");
			if G_UNLIKELY (retv == NULL)
				return NULL;
		}
	}

	RM = gel_matrixw_copy(a[0]->mat.matrix);
	RV = gel_matrixw_copy(a[1]->mat.matrix);

	ret = gel_value_matrix_gauss (ctx, RM, TRUE, FALSE, FALSE, NULL, RV);

	if (retm != NULL) {
		GET_NEW_NODE(n);
		n->type = MATRIX_NODE;
		n->mat.matrix = RM;
		n->mat.quoted = FALSE;
		d_set_value (retm, n);
	} else {
		gel_matrixw_free (RM);
	}

	if (retv != NULL) {
		GET_NEW_NODE(n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_copy (RV);
		n->mat.quoted = FALSE;
		d_set_value (retv, n);
	}

	if (ret) {
		int r;
		int h = gel_matrixw_height (RV);
		r = gel_matrixw_width (a[0]->mat.matrix);
		/* here we kill the zero rows such that only the
		 * solution is returned */
		if (r < h) {
			GelMatrixW *tmp;
			int *regx, *regy, i, w;
			for (i = r; i < h; i++) {
				if ( ! is_row_zero (RV, i)) {
					/* Yaikes, this means there is
					 * no solution! */
					gel_matrixw_free (RV);
					return gel_makenum_null ();
				}
			}
			w = gel_matrixw_width (RV);
			regx = g_new(int, w);
			for (i = 0; i < w; i++)
				regx[i] = i;
			regy = g_new(int, r);
			for (i = 0; i < r; i++)
				regy[i] = i;

			tmp = gel_matrixw_get_region (RV, regx, regy, w, r);
			g_free (regx);
			g_free (regy);

			gel_matrixw_free (RV);
			RV = tmp;
		}
		GET_NEW_NODE(n);
		n->type = MATRIX_NODE;
		n->mat.matrix = RV;
		n->mat.quoted = FALSE;
		return n;
	} else {
		gel_matrixw_free (RV);
		return gel_makenum_null ();
	}
}

/* this is utterly stupid, but only used for small primes
 * where it's all ok */
static gboolean
is_prime_small (unsigned int testnum)
{
	int i;
	unsigned int s = (unsigned int)sqrt(testnum);
	
	for(i=0;g_array_index(primes,unsigned int,i)<=s && i<numprimes;i++) {
		if((testnum%g_array_index(primes,unsigned int,i))==0) {
			return FALSE;
		}
	}
	return TRUE;
}

static GelETree *
Prime_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	long num;
	unsigned int i;
	unsigned int last_prime;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Prime_op,"prime", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Prime"))
		return NULL;

	num = get_nonnegative_integer (a[0]->val.value, "Prime");
	if G_UNLIKELY (num < 0)
		return NULL;
	
	if G_UNLIKELY (primes == NULL) {
		unsigned int b;
		primes = g_array_new(FALSE,FALSE,sizeof(unsigned int));
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
		return gel_makenum_ui(g_array_index(primes,unsigned int,num-1));

	last_prime = g_array_index (primes, unsigned int, numprimes-1);
	primes = g_array_set_size(primes,num);
	for(i=g_array_index(primes,unsigned int,numprimes-1)+2;
	    numprimes<=num-1 && numprimes <= MAXPRIMES && i<=UINT_MAX-1;i+=2) {
		if (is_prime_small (i)) {
			g_array_index(primes,unsigned int,numprimes++) = i;
			last_prime = i;
		}
	}

	if (numprimes <= num-1) {
		mpw_t prime;
		mpw_init (prime);
		mpw_set_ui (prime, last_prime);
		for (i = numprimes; i <= num-1; i++) {
			mpw_nextprime (prime, prime);
		}
		return gel_makenum_use (prime);
	}
	return gel_makenum_ui(g_array_index(primes,unsigned int,num-1));
}

static GelETree *
NextPrime_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t ret;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],NextPrime_op,"NextPrime", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "NextPrime"))
		return NULL;

	mpw_init (ret);
	mpw_nextprime (ret, a[0]->val.value);
	if G_UNLIKELY (error_num != NO_ERROR) {
		mpw_clear (ret);
		/* eek! should not happen */
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
LucasNumber_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t ret;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],LucasNumber_op,"LucasNumber", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "LucasNumber"))
		return NULL;

	mpw_init (ret);
	mpw_lucnum (ret, a[0]->val.value);
	if G_UNLIKELY (error_num != NO_ERROR) {
		mpw_clear (ret);
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
IsPrime_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr num;
	mpz_t tmp;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrix (ctx, a[0], IsPrime_op, "IsPrime", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsPrime"))
		return NULL;

	MPW_MPZ_REAL (num, a[0]->val.value, tmp);

	ret = mympz_is_prime (num, -1);

	MPW_MPZ_KILL (num, tmp);

	return gel_makenum_ui (ret);
}

static GelETree *
StrongPseudoprimeTest_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr num;
	mpz_t tmp;
	mpz_ptr b;
	mpz_t tmpb;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1],
					       StrongPseudoprimeTest_op,
					       "StrongPseudoprimeTest",
					       exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "StrongPseudoprimeTest") ||
			! check_argument_positive_integer (a, 1, "StrongPseudoprimeTest"))
		return NULL;

	MPW_MPZ_REAL (num, a[0]->val.value, tmp);
	MPW_MPZ_REAL (b, a[1]->val.value, tmpb);

	ret = mympz_strong_pseudoprime_test (num, b);

	MPW_MPZ_KILL (num, tmp);
	MPW_MPZ_KILL (b, tmpb);

	return gel_makenum_ui (ret);
}

static GelETree *
MillerRabinTest_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	int reps;
	mpz_ptr num;
	mpz_t tmp;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1],
					       MillerRabinTest_op,
					       "MillerRabinTest",
					       exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "MillerRabinTest") ||
			! check_argument_positive_integer (a, 1, "MillerRabinTest"))
		return NULL;

	reps = get_nonnegative_integer (a[1]->val.value, "MillerRabinTest");
	MPW_MPZ_REAL (num, a[0]->val.value, tmp);

	ret = mpz_millerrabin (num, reps);

	MPW_MPZ_KILL (num, tmp);

	return gel_makenum_ui (ret);
}

static GelETree *
MillerRabinTestSure_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr num;
	mpz_t tmp;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrix (ctx, a[0],
					     MillerRabinTestSure_op,
					     "MillerRabinTestSure", exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "MillerRabinTestSure"))
		return NULL;
	if G_UNLIKELY (mpw_cmp_ui (a[0]->val.value, 2) <= 0) {
		gel_errorout (_("%s: argument must be greater "
				"then 2"), "MillerRabinTestSure");
		return NULL;
	}

	MPW_MPZ_REAL (num, a[0]->val.value, tmp);

	ret = mympz_miller_rabin_test_sure (num);

	MPW_MPZ_KILL (num, tmp);

	return gel_makenum_ui (ret);
}

static GelETree *
Factorize_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpz_ptr num;
	mpz_t tmp;
	GArray *fact;
	GelETree *n;
	GelMatrixW *mn;
	int i;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrix (ctx, a[0],
					     Factorize_op,
					     "Factorize", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Factorize"))
		return NULL;

	MPW_MPZ_REAL (num, a[0]->val.value, tmp);

	fact = mympz_pollard_rho_factorize (num);

	MPW_MPZ_KILL (num, tmp);

	/* error or interrupt or whatnot */
	if G_UNLIKELY (fact == NULL) {
		RAISE_EXCEPTION (exception);
		return NULL;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (mn, fact->len, 2);
	
	for (i = 0; i < fact->len; i++) {
		GelFactor f = g_array_index (fact, GelFactor, i);
		mpw_t num;
		mpw_init (num);
		mpw_set_mpz_use (num, f.num);
		gel_matrixw_set_index (mn, i, 0) = gel_makenum_use (num);
		gel_matrixw_set_index (mn, i, 1) = gel_makenum_ui (f.exp);
	}

	g_array_free (fact, TRUE /*free segment */);

	return n;
}

static GelETree *
ModInvert_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t ret;

	if (a[0]->type == MATRIX_NODE ||
	    a[1]->type == MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1],
					       ModInvert_op,
					       "ModInvert",
					       exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "ModInvert") ||
			! check_argument_integer (a, 1, "ModInvert"))
		return NULL;

	mpw_init (ret);
	mpw_invert (ret, a[0]->val.value, a[1]->val.value);
	if G_UNLIKELY (error_num != NO_ERROR) {
		mpw_clear (ret);
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
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

static gboolean
check_poly(GelETree * *a, int args, char *func, gboolean complain)
{
	int i,j;

	for (j = 0; j < args; j++) {
		if (a[j]->type != MATRIX_NODE ||
		    gel_matrixw_height (a[j]->mat.matrix) != 1) {
			if G_UNLIKELY (complain)
				gel_errorout (_("%s: arguments not horizontal vectors"),
					      func);
			return FALSE;
		}

		for(i=0;i<gel_matrixw_width(a[j]->mat.matrix);i++) {
			GelETree *t = gel_matrixw_index(a[j]->mat.matrix,i,0);
			if (t->type != VALUE_NODE) {
				if G_UNLIKELY (complain)
					gel_errorout (_("%s: arguments not numeric only vectors"),
						      func);
				return FALSE;
			}
		}
	}
	return TRUE;
}

static GelETree *
AddPoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long size;
	int i;
	GelMatrixW *m1,*m2,*mn;
	
	if G_UNLIKELY ( ! check_poly(a,2,"AddPoly",TRUE))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
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
SubtractPoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long size;
	int i;
	GelMatrixW *m1,*m2,*mn;
	
	if G_UNLIKELY ( ! check_poly(a,2,"SubtractPoly",TRUE))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
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
MultiplyPoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long size;
	int i,j;
	mpw_t accu;
	GelMatrixW *m1,*m2,*mn;
	
	if G_UNLIKELY ( ! check_poly(a,2,"MultiplyPoly",TRUE))
		return NULL;
	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
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

/*
static GelETree *
DividePoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long size;
	int i,j;
	mpw_t accu;
	GelMatrixW *m1,*m2,*mn;
	GelEFunc *retmod = NULL;
	
	if G_UNLIKELY ( ! check_poly (a, 2, "DividePoly", TRUE))
		return NULL;
	if (a[2] != NULL) {
		retmod = get_reference (a[2], _("third argument"),
					"DividePoly");
		if G_UNLIKELY (retmod == NULL)
			return NULL;
	}
	if G_UNLIKELY (a[3] != NULL) {
		gel_errorout (_("%s: too many arguments"),
			      "DividePoly");
		return NULL;
	}

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	size = gel_matrixw_width (m1);
	gel_matrixw_set_size (mn, size, 1);

FIXME: implement long div
	
	poly_cut_zeros(mn);

	return n;
}
*/

static GelETree *
PolyDerivative_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if G_UNLIKELY ( ! check_poly(a,1,"PolyDerivative",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
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
Poly2ndDerivative_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if G_UNLIKELY ( ! check_poly(a,1,"Poly2ndDerivative",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
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
TrimPoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	
	if G_UNLIKELY ( ! check_poly(a,1,"TrimPoly",TRUE))
		return NULL;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	
	poly_cut_zeros(n->mat.matrix);

	return n;
}

static GelETree *
IsPoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(check_poly(a,1,"IsPoly",FALSE))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
PolyToString_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	int i;
	GString *gs;
	int any = FALSE;
	GelMatrixW *m;
	char *var;
	GelOutput *gelo;
	char *r;
	
	if G_UNLIKELY ( ! check_poly(a,1,"PolyToString",TRUE))
		return NULL;

	if (a[1] == NULL) {
		var = "x";
	} else if G_UNLIKELY ( ! check_argument_string (a, 1, "PolyToString")) {
		return NULL;
	} else {
		if G_UNLIKELY (a[2] != NULL) {
			gel_errorout (_("%s: too many arguments"),
				      "PolyToString");
			return NULL;
		}
		var = a[1]->str.str;
	}
	
	m = a[0]->mat.matrix;
	
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
				print_etree(gelo,t,FALSE);
			else if(mpw_cmp_ui(t->val.value,1)!=0) {
				print_etree(gelo,t,FALSE);
				g_string_append_c(gs,'*');
			}
			/*negative*/
		} else {
			if(any) g_string_append(gs," - ");
			else g_string_append_c(gs,'-');
			mpw_neg(t->val.value,t->val.value);
			if(i==0)
				print_etree(gelo,t,FALSE);
			else if(mpw_cmp_ui(t->val.value,1)!=0) {
				print_etree(gelo,t,FALSE);
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
PolyToFunction_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelETree *nn = NULL;
	int i;
	GelMatrixW *m;

	static GelToken *var = NULL;
	
	if G_UNLIKELY ( ! check_poly(a,1,"PolyToFunction",TRUE))
		return NULL;
	
	if G_UNLIKELY (var == NULL)
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
	n->func.func = d_makeufunc(NULL,nn,g_slist_append(NULL,var),1, NULL);
	n->func.func->context = -1;

	return n;
}

static GelETree *
StringToASCII_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	const char *s;
	int size;
	int i;
	GelMatrixW *m;

	if G_UNLIKELY ( ! check_argument_string (a, 0, "StringToASCII"))
		return NULL;

	s = a[0]->str.str;
	size = strlen(s);
	if (size == 0)
		return gel_makenum_null ();

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = m = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, size, 1);
	
	for (i = 0; i < size; i++) {
		gel_matrixw_set_index (m, i, 0) = gel_makenum_si (s[i]);
	}

	return n;
}

static GelETree *
ASCIIToString_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	char *s;
	int size;
	int i;
	GelMatrixW *m;

	if (a[0]->type == NULL_NODE)
		return gel_makenum_string ("");

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "ASCIIToString"))
		return NULL;

	m = a[0]->mat.matrix;

	size = gel_matrixw_elements (m);

	s = g_new0 (char, size+1);

	for (i = 0; i < size; i++) {
		GelETree *t;
		t = gel_matrixw_vindex (m, i);
		if (t->type != VALUE_NODE ||
		    mpw_is_complex (t->val.value) ||
		    ! mpw_is_integer (t->val.value) ||
		    mpw_sgn (t->val.value) < 0 ||
		    mpw_cmp_ui (t->val.value, 256) >= 0) {
			g_free (s);
			gel_errorout (_("%s: value out of range"),
				      "ASCIIToString");
			return NULL;
		}
		s[i] = mpw_get_long (t->val.value);
	}

	return gel_makenum_string_use (s);
}

static int
alphabet_value (char a, const char *alph)
{
	int i;
	for (i = 0; alph[i] != '\0'; i++) {
		if (alph[i] == a)
			return i;
	}
	return -1;
}

static GelETree *
StringToAlphabet_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	const char *s;
	const char *alph;
	int size;
	int i;
	GelMatrixW *m;

	if G_UNLIKELY ( ! check_argument_string (a, 0, "AlphabetToString") ||
			! check_argument_string (a, 1, "AlphabetToString"))
		return NULL;

	s = a[0]->str.str;
	alph = a[1]->str.str;
	size = strlen(s);
	if (size == 0)
		return gel_makenum_null ();

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = m = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, size, 1);
	
	for (i = 0; i < size; i++) {
		int val = alphabet_value (s[i], alph);
		gel_matrixw_set_index (m, i, 0) = gel_makenum_si (val);
	}

	return n;
}

static GelETree *
AlphabetToString_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	char *s;
	const char *alph;
	int size;
	int alph_size;
	int i;
	GelMatrixW *m;

	if (a[0]->type == NULL_NODE)
		return gel_makenum_string ("");

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "AlphabetToString") ||
			! check_argument_string (a, 1, "AlphabetToString"))
		return NULL;

	m = a[0]->mat.matrix;
	alph = a[1]->str.str;

	size = gel_matrixw_elements (m);
	alph_size = strlen (alph);

	s = g_new0 (char, size+1);

	for (i = 0; i < size; i++) {
		GelETree *t;
		t = gel_matrixw_vindex (m, i);
		if G_UNLIKELY (t->type != VALUE_NODE ||
			       mpw_is_complex (t->val.value) ||
			       ! mpw_is_integer (t->val.value) ||
			       mpw_sgn (t->val.value) < 0 ||
			       mpw_cmp_ui (t->val.value, alph_size) >= 0) {
			g_free (s);
			gel_errorout (_("%s: value out of range"),
				      "AlphabetToString");
			return NULL;
		}
		s[i] = alph[mpw_get_long (t->val.value)];
	}

	return gel_makenum_string_use (s);
}

static GelETree *
SetHelp_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	const char *fname;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "SetHelp") ||
			! check_argument_string (a, 1, "SetHelp") ||
			! check_argument_string (a, 2, "SetHelp"))
		return NULL;

	if (a[0]->type == IDENTIFIER_NODE) {
		fname = a[0]->id.id->token;
	} else /* STRING_NODE */ {
		fname = a[0]->str.str;
	}
	
	add_category (fname, a[1]->str.str);
	add_description (fname, a[2]->str.str);

	return gel_makenum_null();
}

static GelETree *
SetHelpAlias_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	const char *fname1;
	const char *fname2;
	
	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "SetHelpAlias") ||
			! check_argument_string_or_identifier (a, 1, "SetHelpAlias"))
		return NULL;

	if (a[0]->type == IDENTIFIER_NODE) {
		fname1 = a[0]->id.id->token;
	} else /* STRING_NODE */ {
		fname1 = a[0]->str.str;
	}

	if (a[1]->type == IDENTIFIER_NODE) {
		fname2 = a[1]->id.id->token;
	} else /* STRING_NODE */ {
		fname2 = a[1]->str.str;
	}
	
	add_alias (fname1, fname2);

	return gel_makenum_null();
}

static GelETree *
etree_out_of_int_vector (int *vec, int len)
{
	GelMatrix *mm;
	int i;
	GelETree *n;

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, len, 1, FALSE /* padding */);

	for (i = 0; i < len; i++) {
		gel_matrix_index (mm, i, 0) = gel_makenum_si (vec[i]);
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (mm);
	n->mat.quoted = FALSE;

	return n;
}

static GelETree *
etree_out_of_etree_list (GSList *list, int len)
{
	GelMatrix *mm;
	GSList *li;
	int i;
	GelETree *n;

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, len, 1, FALSE /* padding */);

	li = list;
	for (i = 0; i < len; i++) {
		gel_matrix_index (mm, i, 0) = li->data;
		li = li->next;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (mm);
	n->mat.quoted = FALSE;

	return n;
}

static gboolean
comb_get_next_combination (int *vec, int len, int n)
{
	int i = len;
	int j;

	/* do you like my gel -> C porting? */

	while (i > 0 && vec[i-1] == n-(len-i)) {
		i--;
	}
	if (i == 0) {
		return FALSE;
	} else {
		vec[i-1] ++;
		for (j = i+1; j <= len; j++)
			vec[j-1] = vec[j-2]+1;
	}
	return TRUE;
}

static gboolean
perm_is_pos_mobile (int *perm, char *arrow, int pos, int n)
{
	if (arrow[pos]=='L' && pos==0)
		return FALSE;
	else if (arrow[pos]=='R' && pos==n-1)
		return FALSE;
	else if (arrow[pos]=='L' && perm[pos] > perm[pos-1])
		return TRUE;
	else if (arrow[pos]=='R' && perm[pos] > perm[pos+1])
		return TRUE;
	else
		return FALSE;
}

static int
perm_get_highest_mobile (int *perm, char *arrow, int n)
{
	int highest = -1;
	int i;
	for (i = 0; i < n; i++) {
		if (perm_is_pos_mobile (perm, arrow, i, n) &&
		    (highest == -1 || perm[highest] < perm[i]))
			highest = i;
	}
	return highest;
}

static void
perm_move_pos (int *perm, char *arrow, int pos, int n)
{
	if (arrow[pos] == 'L') {
		char t;
		g_assert (pos > 0);
		t = perm[pos];
		perm[pos] = perm[pos-1];
		perm[pos-1] = t;
		t = arrow[pos];
		arrow[pos] = arrow[pos-1];
		arrow[pos-1] = t;
	} else {
		char t;
		g_assert (pos < n-1);
		t = perm[pos];
		perm[pos] = perm[pos+1];
		perm[pos+1] = t;
		t = arrow[pos];
		arrow[pos] = arrow[pos+1];
		arrow[pos+1] = t;
	}
}

static void
perm_switch_all_above (int *perm, char *arrow, int pos, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (perm[i] > perm[pos]) {
			if (arrow[i] == 'L')
				arrow[i] = 'R';
			else
				arrow[i] = 'L';
		}
	}
}

static GelETree *
Combinations_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	long k, n;
	int *comb;
	int i;
	GSList *list;
	int len;
	GelETree *r;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Combinations") ||
			! check_argument_integer (a, 1, "Combinations"))
		return NULL;

	error_num = 0;
	k = mpw_get_long(a[0]->val.value);
	if G_UNLIKELY (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	n = mpw_get_long(a[1]->val.value);
	if G_UNLIKELY (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (n < 1 || n > INT_MAX || k < 1 || k > n) {
		gel_errorout (_("%s: value out of range"),
			      "Combinations");
		return NULL;
	}

	list = NULL;
	len = 0;

	comb = g_new (int, k);
	for (i = 0; i < k; i++)
		comb[i] = i+1;

	do {
		list = g_slist_prepend (list, etree_out_of_int_vector (comb, k));
		len++;
	} while (comb_get_next_combination (comb, k, n));

	g_free (comb);

	list = g_slist_reverse (list);

	r = etree_out_of_etree_list (list, len);

	g_slist_free (list);

	return r;
}

static GelETree *
Permutations_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *r;
	GSList *list;
	long k, n, len;
	int *comb;
	int *perm;
	char *arrow;
	int i;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Permutations") ||
			! check_argument_integer (a, 1, "Permutations"))
		return NULL;

	error_num = 0;
	k = mpw_get_long(a[0]->val.value);
	if G_UNLIKELY (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	n = mpw_get_long(a[1]->val.value);
	if G_UNLIKELY (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (n < 1 || n > INT_MAX || k < 1 || k > n) {
		gel_errorout (_("%s: value out of range"),
			      "Permutations");
		return NULL;
	}

	arrow = g_new (char, k);
	perm = g_new (int, k);
	comb = g_new (int, k);

	for (i = 0; i < k; i++)
		comb[i] = i+1;

	list = NULL;
	len = 0;

	do {
		for (i = 0; i < k; i++)
			perm[i] = comb[i];
		for (i = 0; i < k; i++)
			arrow[i] = 'L';
		for (;;) {
			int m;

			list = g_slist_prepend (list, etree_out_of_int_vector (perm, k));
			len++;

			m = perm_get_highest_mobile (perm, arrow, k);
			if (m == -1)
				break;
			perm_switch_all_above (perm, arrow, m, k);
			perm_move_pos (perm, arrow, m, k);
		}
	} while (comb_get_next_combination (comb, k, n));

	g_free (comb);
	g_free (perm);
	g_free (arrow);

	list = g_slist_reverse (list);

	r = etree_out_of_etree_list (list, len);

	g_slist_free (list);

	return r;
}

static GelETree *
protect_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *tok;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "protect"))
		return NULL;

	if (a[0]->type == IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}
	
	tok->protected = 1;

	return gel_makenum_null();
}

static GelETree *
unprotect_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *tok;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "unprotect"))
		return NULL;

	if (a[0]->type == IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}
	
	tok->protected = 0;

	return gel_makenum_null();
}

static GelETree *
SetFunctionFlags_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelEFunc *f;
	GelToken *tok;
	int i;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "SetFunctionFlags"))
		return NULL;

	if (a[0]->type == IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}

	f = d_lookup_global (tok);
	if G_UNLIKELY (f == NULL) {
		gel_errorout (_("%s: undefined function"), "SetFunctionFlags");
		return NULL;
	}

	for (i = 1; a[i] != NULL; i++) {
		if G_UNLIKELY (a[i]->type != STRING_NODE) {
			gel_errorout (_("%s: flags argument must be a string"),
				      "SetFunctionFlags");
		} else if (a[i]->str.str != NULL) {
			if (g_ascii_strcasecmp (a[i]->str.str, "PropagateMod") == 0)
				f->propagate_mod = 1;
			else if (g_ascii_strcasecmp (a[i]->str.str, "NoModuloArguments") == 0)
				f->no_mod_all_args = 1;
		}
	}

	return gel_makenum_null();
}

static GelETree *
GetCurrentModulo_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_ptr modulo = gel_find_pre_function_modulo (ctx);
	if (modulo == NULL)
		return gel_makenum_null ();
	else
		return gel_makenum (modulo);
}

static GelETree *
set_FloatPrecision (GelETree * a)
{
	long bits;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_FloatPrecision"))
		return NULL;

	bits = mpw_get_long(a->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (bits < 60 || bits > 16384) {
		gel_errorout (_("%s: argument should be between %d and %d"),
			      "set_FloatPrecision", 60, 16384);
		return NULL;
	}
	
	if(calcstate.float_prec != bits) {
		calcstate.float_prec = bits;
		mpw_set_default_prec (calcstate.float_prec);
		gel_break_fp_caches ();
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.float_prec);
}

static GelETree *
get_FloatPrecision (void)
{
	return gel_makenum_ui(calcstate.float_prec);
}

static GelETree *
set_MaxDigits (GelETree * a)
{
	long digits;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_MaxDigits"))
		return NULL;

	digits = mpw_get_long(a->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (digits < 0 || digits > 256) {
		gel_errorout (_("%s: argument should be between %d and %d"),
			      "set_MaxDigits", 0, 256);
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
get_MaxDigits (void)
{
	return gel_makenum_ui(calcstate.max_digits);
}

static GelETree *
set_ResultsAsFloats (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_number (&a, 0, "set_ResultsAsFloats"))
		return NULL;
	calcstate.results_as_floats = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.results_as_floats)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_ResultsAsFloats (void)
{
	if(calcstate.results_as_floats)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
set_ScientificNotation (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_number (&a, 0, "set_ScientificNotation"))
		return NULL;
	calcstate.scientific_notation = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.scientific_notation)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_ScientificNotation (void)
{
	if(calcstate.scientific_notation)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
set_FullExpressions (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_number (&a, 0, "set_FullExpressions"))
		return NULL;
	calcstate.full_expressions = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.full_expressions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_FullExpressions (void)
{
	if(calcstate.full_expressions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
set_OutputStyle (GelETree * a)
{
	const char *token;
	GelOutputStyle output_style = GEL_OUTPUT_NORMAL;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (&a, 0, "set_OutputStyle"))
		return NULL;

	if (a->type == STRING_NODE)
		token = a->str.str;
	else
		token = a->id.id->token;

	if (token != NULL && g_ascii_strcasecmp (token, "normal") == 0) {
		output_style = GEL_OUTPUT_NORMAL;
	} else if (token != NULL && g_ascii_strcasecmp (token, "troff") == 0) {
		output_style = GEL_OUTPUT_TROFF;
	} else if (token != NULL && g_ascii_strcasecmp (token, "latex") == 0) {
		output_style = GEL_OUTPUT_LATEX;
	} else if (token != NULL && g_ascii_strcasecmp (token, "mathml") == 0) {
		output_style = GEL_OUTPUT_MATHML;
	} else {
		(*errorout)(_("OutputStyle must be one of normal, troff, latex or mathml"));
		return NULL;
	}

	calcstate.output_style = output_style;
	if (statechange_hook)
		(*statechange_hook)(calcstate);

	return gel_makenum_string (token);
}

static GelETree *
get_OutputStyle (void)
{
	const char *token;

	token = "normal";
	if (calcstate.output_style == GEL_OUTPUT_TROFF)
		token = "troff";
	else if (calcstate.output_style == GEL_OUTPUT_LATEX)
		token = "latex";
	else if (calcstate.output_style == GEL_OUTPUT_MATHML)
		token = "mathml";

	return gel_makenum_string (token);
}

static GelETree *
set_MaxErrors (GelETree * a)
{
	long errors;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_MaxErrors"))
		return NULL;

	errors = mpw_get_long(a->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (errors < 0) {
		gel_errorout (_("%s: argument should be larger or equal to 0"),
			      "MaxErrors");
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
get_MaxErrors (void)
{
	return gel_makenum_ui(calcstate.max_errors);
}

static GelETree *
set_MixedFractions (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_number (&a, 0, "set_MixedFractions"))
		return NULL;
	calcstate.mixed_fractions = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.mixed_fractions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_MixedFractions (void)
{
	if(calcstate.mixed_fractions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
set_IntegerOutputBase (GelETree * a)
{
	long base;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_IntegerOutputBase"))
		return NULL;

	base = mpw_get_long(a->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (base < 2 || base > 36) {
		gel_errorout (_("%s: argument should be between %d and %d"),
			      "IntegerOutputBase", 2, 36);
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
get_IntegerOutputBase (void)
{
	return gel_makenum_ui(calcstate.integer_output_base);
}

static GelETree *
set_IsPrimeMillerRabinReps (GelETree * a)
{
	long reps;

	if G_UNLIKELY ( ! check_argument_positive_integer (&a, 0, "set_IsPrimeMillerRabinReps"))
		return NULL;

	reps = mpw_get_long (a->val.value);
	if G_UNLIKELY (error_num) {
		error_num = 0;
		return NULL;
	}

	mympz_is_prime_miller_rabin_reps = reps;
	return gel_makenum_ui (mympz_is_prime_miller_rabin_reps);
}
static GelETree *
get_IsPrimeMillerRabinReps (void)
{
	return gel_makenum_ui (mympz_is_prime_miller_rabin_reps);
}

/*add the routines to the dictionary*/
void
gel_funclib_addall(void)
{
	GelEFunc *f;
	GelToken *id;

	new_category ("basic", _("Basic"));
	new_category ("parameters", _("Parameters"));
	new_category ("constants", _("Constants"));
	new_category ("numeric", _("Numeric"));
	new_category ("trigonometry", _("Trigonometry"));
	new_category ("number_theory", _("Number Theory"));
	new_category ("matrix", _("Matrix Manipulation"));
	new_category ("linear_algebra", _("Linear Algebra"));
	new_category ("combinatorics", _("Combinatorics"));
	new_category ("calculus", _("Calculus"));
	new_category ("functions", _("Functions"));
	new_category ("equation_solving", _("Equation Solving"));
	new_category ("statistics", _("Statistics"));
	new_category ("polynomial", _("Polynomials"));
	new_category ("sets", _("Set Theory"));
	new_category ("misc", _("Miscellaneous"));

	/* FIXME: add more help fields */
#define FUNC(name,args,argn,category,desc) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), name ## _op, args)); \
	d_add_named_args (f, argn); \
	add_category ( #name , category); \
	add_description ( #name , desc);
#define VFUNC(name,args,argn,category,desc) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), name ## _op, args)); \
	d_add_named_args (f, argn); \
	f->vararg = TRUE; \
	add_category ( #name , category); \
	add_description ( #name , desc);
#define ALIAS(name,args,aliasfor) \
	d_addfunc (d_makebifunc (d_intern ( #name ), aliasfor ## _op, args)); \
	add_alias ( #aliasfor , #name );
#define VALIAS(name,args,aliasfor) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), aliasfor ## _op, args)); \
	f->vararg = TRUE; \
	add_alias ( #aliasfor , #name );
#define PARAMETER(name,desc) \
	id = d_intern ( #name ); \
	id->parameter = 1; \
	id->built_in_parameter = 1; \
	id->data1 = set_ ## name; \
	id->data2 = get_ ## name; \
	add_category ( #name , "parameters"); \
	add_description ( #name , desc); \
	/* bogus value */ \
	d_addfunc_global (d_makevfunc (id, gel_makenum_null()));


	FUNC (manual, 0, "", "basic", _("Displays the user manual"));
	FUNC (warranty, 0, "", "basic", _("Gives the warranty information"));
	FUNC (exit, 0, "", "basic", _("Exits the program"));
	ALIAS (quit, 0, exit);
	FUNC (error, 1, "str", "basic", _("Prints a string to the error stream"));
	FUNC (print, 1, "str", "basic", _("Prints an expression"));
	FUNC (chdir, 1, "dir", "basic", _("Changes current directory"));
	FUNC (printn, 1, "str", "basic", _("Prints an expression without a trailing newline"));
	FUNC (display, 2, "str,expr", "basic", _("Display a string and an expression"));
	FUNC (set, 2, "id,val", "basic", _("Set a global variable"));

	FUNC (SetHelp, 3, "id,category,desc", "basic", _("Set the category and help description line for a function"));
	FUNC (SetHelpAlias, 2, "id,alias", "basic", _("Sets up a help alias"));

	VFUNC (rand, 1, "size", "numeric", _("Generate random float"));
	f->no_mod_all_args = 1;
	VFUNC (randint, 2, "max,size", "numeric", _("Generate random integer"));
	f->no_mod_all_args = 1;

	PARAMETER (FloatPrecision, _("Floating point precision"));
	PARAMETER (MaxDigits, _("Maximum digits to display"));
	PARAMETER (MaxErrors, _("Maximum errors to display"));
	PARAMETER (OutputStyle, _("Output style: normal, latex or troff"));
	PARAMETER (IntegerOutputBase, _("Integer output base"));
	PARAMETER (MixedFractions, _("If true, mixed fractions are printed"));
	PARAMETER (FullExpressions, _("Print full expressions, even if more then a line"));
	PARAMETER (ResultsAsFloats, _("Convert all results to floats before printing"));
	PARAMETER (ScientificNotation, _("Use scientific notation"));

	PARAMETER (IsPrimeMillerRabinReps, _("Number of extra Miller-Rabin tests to run on a number before declaring it a prime in IsPrime"));

	/* secret functions */
	d_addfunc(d_makebifunc(d_intern("ninini"),ninini_op,0));
	d_addfunc(d_makebifunc(d_intern("shrubbery"),shrubbery_op,0));

	FUNC (ExpandMatrix, 1, "M", "matrix", _("Expands a matrix just like we do on unquoted matrix input"));
	FUNC (RowsOf, 1, "M", "matrix", _("Gets the rows of a matrix as a vertical vector"));
	FUNC (ColumnsOf, 1, "M", "matrix", _("Gets the columns of a matrix as a horizontal vector"));
	FUNC (DiagonalOf, 1, "M", "matrix", _("Gets the diagonal entries of a matrix as a horizontal vector"));

	FUNC (ComplexConjugate, 1, "M", "numeric", _("Calculates the conjugate"));
	ALIAS (conj, 1, ComplexConjugate);
	ALIAS (Conj, 1, ComplexConjugate);

	FUNC (sin, 1, "x", "trigonometry", _("Calculates the sine function"));
	FUNC (cos, 1, "x", "trigonometry", _("Calculates the cossine function"));
	FUNC (sinh, 1, "x", "trigonometry", _("Calculates the hyperbolic sine function"));
	FUNC (cosh, 1, "x", "trigonometry", _("Calculates the hyperbolic cosine function"));
	FUNC (tan, 1, "x", "trigonometry", _("Calculates the tan function"));
	FUNC (atan, 1, "x", "trigonometry", _("Calculates the arctan function"));
	ALIAS (arctan, 1, atan);

	FUNC (pi, 0, "", "constants", _("The number pi"));
	FUNC (e, 0, "", "constants", _("The natural number e"));
	FUNC (GoldenRatio, 0, "", "constants", _("The Golden Ratio"));
	FUNC (Gravity, 0, "", "constants", _("Free fall acceleration"));
	FUNC (EulerConstant, 0, "", "constants",
	      _("Euler's Constant gamma good up to about precision of 9516 digits"));
	ALIAS (gamma, 0, EulerConstant);

	FUNC (sqrt, 1, "x", "numeric", _("The square root"));
	f->propagate_mod = 1;
	ALIAS (SquareRoot, 1, sqrt);
	FUNC (exp, 1, "x", "numeric", _("The exponential function"));
	FUNC (ln, 1, "x", "numeric", _("The natural logarithm"));
	FUNC (log2, 1, "x", "numeric", _("Logarithm of x base 2"));
	ALIAS (lg, 1, log2);
	FUNC (log10, 1, "x", "numeric", _("Logarithm of x base 10"));
	FUNC (round, 1, "x", "numeric", _("Round a number"));
	ALIAS (Round, 1, round);
	FUNC (floor, 1, "x", "numeric", _("Get the highest integer less then or equal to n"));
	ALIAS (Floor, 1, floor);
	FUNC (ceil, 1, "x", "numeric", _("Get the lowest integer more then or equal to n"));
	ALIAS (Ceiling, 1, ceil);
	FUNC (trunc, 1, "x", "numeric", _("Truncate number to an integer (return the integer part)"));
	ALIAS (Truncate, 1, trunc);
	ALIAS (IntegerPart, 1, trunc);
	FUNC (float, 1, "x", "numeric", _("Make number a float"));
	FUNC (Numerator, 1, "x", "numeric", _("Get the numerator of a rational number"));
	FUNC (Denominator, 1, "x", "numeric", _("Get the denominator of a rational number"));

	VFUNC (gcd, 2, "a,args", "number_theory", _("Greatest common divisor"));
	ALIAS (GCD, 2, gcd);
	VFUNC (lcm, 2, "a,args", "number_theory", _("Least common multiplier"));
	ALIAS (LCM, 2, lcm);
	FUNC (IsPerfectSquare, 1, "n", "number_theory", _("Check a number for being a perfect square"));
	FUNC (IsPerfectPower, 1, "n", "number_theory", _("Check a number for being any perfect power (a^b)"));
	FUNC (Prime, 1, "n", "number_theory", _("Return the n'th prime (up to a limit)"));
	ALIAS (prime, 1, Prime);
	FUNC (IsEven, 1, "n", "number_theory", _("Tests if an integer is even"));
	FUNC (IsOdd, 1, "n", "number_theory", _("Tests if an integer is odd"));

	FUNC (NextPrime, 1, "n", "number_theory", _("Returns the least prime greater than n (if n is positive)"));
	FUNC (LucasNumber, 1, "n", "number_theory", _("Returns the n'th Lucas number"));
	FUNC (ModInvert, 2, "n,m", "number_theory", _("Returns inverse of n mod m"));
	FUNC (IsPrime, 1, "n", "number_theory", _("Tests primality of integers, for numbers greater then 25*10^9 false positive is with low probability depending on IsPrimeMillerRabinReps"));
	FUNC (StrongPseudoprimeTest, 2, "n,b", "number_theory", _("Run the strong pseudoprime test base b on n"));
	FUNC (MillerRabinTest, 2, "n,reps", "number_theory", _("Use the Miller-Rabin primality test on n, reps number of times.  The probability of false positive is (1/4)^reps"));
	FUNC (MillerRabinTestSure, 1, "n", "number_theory", _("Use the Miller-Rabin primality test on n with enough bases that assuming the Generalized Reimann Hypothesis the result is deterministic"));
	FUNC (Factorize, 1, "n", "number_theory", _("Return factorization of a number as a matrix"));

	VFUNC (max, 2, "a,args", "numeric", _("Returns the maximum of arguments or matrix"));
	VALIAS (Max, 2, max);
	VALIAS (Maximum, 2, max);
	VFUNC (min, 2, "a,args", "numeric", _("Returns the minimum of arguments or matrix"));
	VALIAS (Min, 2, min);
	VALIAS (Minimum, 2, min);

	FUNC (Jacobi, 2, "a,b", "number_theory", _("Calculate the Jacobi symbol (a/b) (b should be odd)"));
	ALIAS (JacobiSymbol, 2, Jacobi);
	FUNC (JacobiKronecker, 2, "a,b", "number_theory", _("Calculate the Jacobi symbol (a/b) with the Kronecker extension (a/2)=(2/a) when a odd, or (a/2)=0 when a even"));
	ALIAS (JacobiKroneckerSymbol, 2, JacobiKronecker);
	FUNC (Legendre, 2, "a,p", "number_theory", _("Calculate the Legendre symbol (a/p)"));
	ALIAS (LegendreSymbol, 2, Legendre);

	FUNC (Re, 1, "z", "numeric", _("Get the real part of a complex number"));
	ALIAS (RealPart, 1, Re);
	FUNC (Im, 1, "z", "numeric", _("Get the imaginary part of a complex number"));
	ALIAS (ImaginaryPart, 1, Im);

	FUNC (I, 1, "n", "matrix", _("Make an identity matrix of a given size"));
	f->no_mod_all_args = 1;
	ALIAS (eye, 1, I);
	VFUNC (zeros, 2, "rows,columns", "matrix", _("Make an matrix of all zeros (or a row vector)"));
	f->no_mod_all_args = 1;
	VFUNC (ones, 2, "rows,columns", "matrix", _("Make an matrix of all ones (or a row vector)"));
	f->no_mod_all_args = 1;

	FUNC (rows, 1, "M", "matrix", _("Get the number of rows of a matrix"));
	FUNC (columns, 1, "M", "matrix", _("Get the number of columns of a matrix"));
	FUNC (IsMatrixSquare, 1, "M", "matrix", _("Is a matrix square"));
	FUNC (elements, 1, "M", "matrix", _("Get the number of elements of a matrix"));

	FUNC (ref, 1, "M", "linear_algebra", _("Get the row echelon form of a matrix"));
	f->propagate_mod = 1;
	ALIAS (REF, 1, ref);
	ALIAS (RowEchelonForm, 1, ref);
	FUNC (rref, 1, "M", "linear_algebra", _("Get the reduced row echelon form of a matrix"));
	f->propagate_mod = 1;
	ALIAS (RREF, 1, rref);
	ALIAS (ReducedRowEchelonForm, 1, rref);
	VFUNC (SolveLinearSystem, 3, "M,V,args", "linear_algebra", _("Solve linear system Mx=V, return solution V if there is a unique solution, null otherwise.  Extra two reference parameters can optionally be used to get the reduced M and V."));
	f->propagate_mod = 1;

	FUNC (det, 1, "M", "linear_algebra", _("Get the determinant of a matrix"));
	ALIAS (Determinant, 1, det);

	FUNC (SetMatrixSize, 3, "M,rows,columns", "matrix", _("Make new matrix of given size from old one"));
	FUNC (IndexComplement, 2, "vec,msize", "matrix", _("Return the index complement of a vector of indexes"));

	FUNC (IsValueOnly, 1, "M", "matrix", _("Check if a matrix is a matrix of numbers"));
	FUNC (IsMatrixInteger, 1, "M", "matrix", _("Check if a matrix is an integer (non-complex) matrix"));
	FUNC (IsMatrixRational, 1, "M", "matrix", _("Check if a matrix is a rational (non-complex) matrix"));
	FUNC (IsMatrixReal, 1, "M", "matrix", _("Check if a matrix is a real (non-complex) matrix"));

	FUNC (IsNull, 1, "arg", "basic", _("Check if argument is a null"));
	FUNC (IsValue, 1, "arg", "basic", _("Check if argument is a number"));
	FUNC (IsString, 1, "arg", "basic", _("Check if argument is a text string"));
	FUNC (IsMatrix, 1, "arg", "basic", _("Check if argument is a matrix"));
	FUNC (IsFunction, 1, "arg", "basic", _("Check if argument is a function"));
	FUNC (IsFunctionRef, 1, "arg", "basic", _("Check if argument is a function reference"));

	FUNC (IsComplex, 1, "num", "numeric", _("Check if argument is a complex (non-real) number"));
	FUNC (IsReal, 1, "num", "numeric", _("Check if argument is a real number"));
	FUNC (IsInteger, 1, "num", "numeric", _("Check if argument is an integer (non-complex)"));
	FUNC (IsPositiveInteger, 1, "num", "numeric", _("Check if argument is a positive real integer"));
	ALIAS (IsNaturalNumber, 1, IsPositiveInteger);
	FUNC (IsGaussInteger, 1, "num", "numeric", _("Check if argument is a possibly complex integer"));
	ALIAS (IsComplexInteger, 1, IsGaussInteger);
	FUNC (IsRational, 1, "num", "numeric", _("Check if argument is a rational number (non-complex)"));
	FUNC (IsComplexRational, 1, "num", "numeric", _("Check if argument is a possibly complex rational number"));
	FUNC (IsFloat, 1, "num", "numeric", _("Check if argument is a floating point number (non-complex)"));

	FUNC (AddPoly, 2, "p1,p2", "polynomial", _("Add two polynomials (vectors)"));
	FUNC (SubtractPoly, 2, "p1,p2", "polynomial", _("Subtract two polynomials (as vectors)"));
	FUNC (MultiplyPoly, 2, "p1,p2", "polynomial", _("Multiply two polynomials (as vectors)"));
	FUNC (PolyDerivative, 1, "p", "polynomial", _("Take polynomial (as vector) derivative"));
	FUNC (Poly2ndDerivative, 1, "p", "polynomial", _("Take second polynomial (as vector) derivative"));
	FUNC (TrimPoly, 1, "p", "polynomial", _("Trim zeros from a polynomial (as vector)"));
	FUNC (IsPoly, 1, "p", "polynomial", _("Check if a vector is usable as a polynomial"));
	VFUNC (PolyToString, 2, "p,var", "polynomial", _("Make string out of a polynomial (as vector)"));
	FUNC (PolyToFunction, 1, "p", "polynomial", _("Make function out of a polynomial (as vector)"));

	FUNC (Combinations, 2, "k,n", "combinatorics", _("Get all combinations of k numbers from 1 to n as a vector of vectors"));
	FUNC (Permutations, 2, "k,n", "combinatorics", _("Get all permutations of k numbers from 1 to n as a vector of vectors"));

	FUNC (StringToASCII, 1, "str", "misc", _("Convert a string to a vector of ASCII values"));
	FUNC (ASCIIToString, 1, "vec", "misc", _("Convert a vector of ASCII values to a string"));

	FUNC (StringToAlphabet, 2, "str,alphabet", "misc", _("Convert a string to a vector of 0-based alphabet values (positions in the alphabet string), -1's for unknown letters"));
	FUNC (AlphabetToString, 2, "vec,alphabet", "misc", _("Convert a vector of 0-based alphabet values (positions in the alphabet string) to a string"));

	FUNC (protect, 1, "id", "basic", _("Protect a variable from being modified"));
	FUNC (unprotect, 1, "id", "basic", _("Unprotect a variable from being modified"));
	VFUNC (SetFunctionFlags, 2, "id,flags", "basic", _("Set flags for a function, currently \"PropagateMod\" and \"NoModuloArguments\""));
	FUNC (GetCurrentModulo, 0, "", "basic", _("Get current modulo from the context outside the function"));

	/*temporary until well done internal functions are done*/
	_internal_ln_function = d_makeufunc(d_intern("<internal>ln"),
					    /*FIXME:this is not the correct 
					      function*/
					    gel_parseexp("error(\"ln not finished\")",
							 NULL, FALSE, FALSE,
							 NULL, NULL),
					    g_slist_append(NULL,d_intern("x")),1,
					    NULL);
	_internal_exp_function = d_makeufunc(d_intern("<internal>exp"),
					     gel_parseexp
					       ("s = float(x^0); "
						"fact = 1; "
						"for i = 1 to 100 do "
						"(fact = fact * x / i; "
						"s = s + fact) ; s",
						NULL, FALSE, FALSE,
						NULL, NULL),
					     g_slist_append(NULL,d_intern("x")),1,
					     NULL);
	/*protect EVERYthing up to this point*/
	d_protect_all();
}
