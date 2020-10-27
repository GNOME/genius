/* GENIUS Calculator
 * Copyright (C) 1997-2018 Jiri (George) Lebl
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
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <glib.h>
#include "calc.h"
#include "mpwrap.h"
#include "mpzextra.h"
#include "eval.h"
#include "dict.h"
#include "funclib.h"
#include "symbolic.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "geloutput.h"

#include "binreloc.h"

/* FIXME:static GelEFunc *_internal_ln_function = NULL; */
static GelEFunc *_internal_exp_function = NULL;
static GelEFunc *_internal_erf_function = NULL;

static GelEFunc *conj_function = NULL;
static GelEFunc *sin_function = NULL;
static GelEFunc *sinc_function = NULL;
static GelEFunc *cos_function = NULL;
static GelEFunc *sinh_function = NULL;
static GelEFunc *cosh_function = NULL;
static GelEFunc *tan_function = NULL;
static GelEFunc *atan_function = NULL;
static GelEFunc *sqrt_function = NULL;
static GelEFunc *exp_function = NULL;
static GelEFunc *ln_function = NULL;
static GelEFunc *log2_function = NULL;
static GelEFunc *log10_function = NULL;
static GelEFunc *round_function = NULL;
static GelEFunc *floor_function = NULL;
static GelEFunc *ceil_function = NULL;
static GelEFunc *trunc_function = NULL;
static GelEFunc *float_function = NULL;
static GelEFunc *Numerator_function = NULL;
static GelEFunc *Denominator_function = NULL;
static GelEFunc *Re_function = NULL;
static GelEFunc *Im_function = NULL;
static GelEFunc *ErrorFunction_function = NULL;
static GelEFunc *RiemannZeta_function = NULL;
static GelEFunc *GammaFunction_function = NULL;
static GelEFunc *BesselJ0_function = NULL;
static GelEFunc *BesselJ1_function = NULL;
static GelEFunc *BesselY0_function = NULL;
static GelEFunc *BesselY1_function = NULL;
/*static GelEFunc *BesselJn_function = NULL;
static GelEFunc *BesselYn_function = NULL;*/
static GelEFunc *pi_function = NULL;
static GelEFunc *e_function = NULL;
static GelEFunc *GoldenRatio_function = NULL;
static GelEFunc *Gravity_function = NULL;
static GelEFunc *EulerConstant_function = NULL;

/*maximum number of primes to precalculate and store*/
#define MAXPRIMES 30000
static GArray *primes = NULL;
static int numprimes = 0;

static mpw_t e_cache;
static int e_iscached = FALSE;
static mpw_t golden_ratio_cache;
static int golden_ratio_iscached = FALSE;

#include "funclibhelper.cP"

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

int
gel_get_nonnegative_integer (mpw_ptr z, const char *funcname)
{
	long i;
	i = mpw_get_long(z);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return -1;
	}
	if G_UNLIKELY (i < 0) {
		/* This should already have been checked */
		/*gel_errorout (_("%s: argument can't be negative"), funcname);*/
		return -1;
	}
	if G_UNLIKELY (i > G_MAXINT) {
		gel_errorout (_("%s: argument too large"), funcname);
		return -1;
	}
	return i;
}

static GelETree *
manual_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_call_help (NULL);
	gel_error_num = GEL_IGNORE_ERROR;
	RAISE_EXCEPTION (exception);
	return NULL;
}

static GelETree *
version_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int v,b,c;
	GelETree *n;
	GelMatrix *m;

	if (sscanf (VERSION, "%d.%d.%d", &v, &b, &c) != 3) {
		if (sscanf (VERSION, "%d.%d", &v, &b) == 2) {
			c = 0;
		} else if (sscanf (VERSION, "%d", &v) == 1) {
			b = 0;
			c = 0;
		} else {
			gel_errorout (_("Cannot parse version string: %s"),
				      VERSION);
			gel_error_num = GEL_IGNORE_ERROR;
			RAISE_EXCEPTION (exception);
			return NULL;
		}
	}

	m = gel_matrix_new ();
	gel_matrix_set_size (m, 3, 1, FALSE /* padding */);
	gel_matrix_index (m, 0, 0) = gel_makenum_ui (v);
	gel_matrix_index (m, 1, 0) = gel_makenum_ui (b);
	gel_matrix_index (m, 2, 0) = gel_makenum_ui (c);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix_value_only_integer (m);
	n->mat.quoted = FALSE;

	return n;
}


static GelETree *
warranty_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_infoout (_("Genius %s\n"
		       "%s\n\n"
		       "    This program is free software: you can redistribute it and/or modify\n"
		       "    it under the terms of the GNU General Public License as published by\n"
		       "    the Free Software Foundation, either version 3 of the License, or\n"
		       "    (at your option) any later version.\n"
		       "\n"
		       "    This program is distributed in the hope that it will be useful,\n"
		       "    but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		       "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		       "    GNU General Public License for more details.\n"
		       "\n"
		       "    You should have received a copy of the GNU General Public License\n"
		       "    along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"),
			    VERSION,
			    _(GENIUS_COPYRIGHT_STRING));
	gel_error_num = GEL_IGNORE_ERROR;
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
	gel_error_num = GEL_IGNORE_ERROR;
	return NULL;
}

static GelETree *
shrubbery_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gel_infoout ("Then, when you have found the shrubbery, you must\n"
		     "cut down the mightiest tree in the forest... with...\n"
		     "A HERRING!");
	RAISE_EXCEPTION (exception);
	gel_error_num = GEL_IGNORE_ERROR;
	return NULL;
}

static GelETree *
IsDefined_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *tok;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix
			(ctx, a[0], IsDefined_op, "IsDefined",
			 exception);

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "IsDefined"))
		return NULL;

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}

	if (d_lookup_global (tok) != NULL)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}

static GelETree *
undefine_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *tok;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix
			(ctx, a[0], undefine_op, "undefine",
			 exception);

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "undefine"))
		return NULL;

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}

	if G_UNLIKELY (tok->protected_) {
		gel_errorout (_("%s: trying to undefine a protected id!"),
			      "undefine");
		return NULL;
	}

	d_delete (tok);

	return gel_makenum_null ();
}

static GelETree *
UndefineAll_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GSList *li;
	GSList *list;

	list = g_slist_copy (d_getcontext_global ());

	for (li = list;
	     li != NULL;
	     li = li->next) {
		GelEFunc *f = li->data;
		GelToken *tok = f->id;
		if ( ! tok->protected_ &&
		    strcmp (tok->token, "Ans") != 0) {
			d_delete_global (tok);
		}
	}

	return gel_makenum_null ();
}

static GelETree *
ProtectAll_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	d_protect_all ();

	return gel_makenum_null ();
}

static GelETree *
UserVariables_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GSList *li;
	GelMatrix *m;
	GelETree *n;
	int len, i;

	len = 0;

	for (li = d_getcontext_global ();
	     li != NULL;
	     li = li->next) {
		GelEFunc *f = li->data;
		GelToken *tok = f->id;
		if ( ! tok->protected_ &&
		    strcmp (tok->token, "Ans") != 0) {
			len++;
		}
	}

	if (len == 0)
		return gel_makenum_null ();

	m = gel_matrix_new ();
	gel_matrix_set_size (m, len, 1, FALSE /* padding */);

	i = 0;
	for (li = d_getcontext_global ();
	     li != NULL;
	     li = li->next) {
		GelEFunc *f = li->data;
		GelToken *tok = f->id;
		if ( ! tok->protected_ &&
		    strcmp (tok->token, "Ans") != 0) {
			gel_matrix_index (m, i, 0) =
				gel_makenum_identifier (tok);
			i++;
		}
	}

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (m);
	n->mat.quoted = FALSE;

	return n;
}

static GelETree *
true_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	return gel_makenum_bool (1);
}

static GelETree *
false_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	return gel_makenum_bool (0);
}

static GelETree *
CurrentTime_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tm;
	struct timeval tv;

	mpw_init (tm);
	gettimeofday (&tv, NULL);
	mpw_set_ui (tm, tv.tv_usec);
	mpw_make_float (tm);
	mpw_div_ui (tm, tm, 1000000);
	mpw_add_ui (tm, tm, tv.tv_sec);
	return gel_makenum_use (tm);
}

/*sin function*/
static GelETree *
IntegerFromBoolean_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int i;
	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix
			(ctx, a[0], IntegerFromBoolean_op, "IntegerFromBoolean",
			 exception);

	if G_UNLIKELY ( ! check_argument_bool (a, 0, "IntegerFromBoolean"))
		return NULL;

	if (a[0]->type == GEL_VALUE_NODE)
		i = mpw_zero_p (a[0]->val.value) ? 0 : 1;
	else /* a->type == GEL_BOOL_NODE */
		i = a[0]->bool_.bool_ ? 1 : 0;

	return gel_makenum_ui (i);
}
	
/*error printing function*/
static GelETree *
error_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_STRING_NODE) {
		gel_errorout ("%s", a[0]->str.str);
	} else {
		GelOutput *gelo = gel_output_new();
		char *s;
		gel_output_setup_string (gelo, 0, NULL);
		gel_pretty_print_etree (gelo, a[0]);
		s = gel_output_snarf_string (gelo);
		gel_output_unref (gelo);
		gel_errorout ("%s", s != NULL ? s : "");
		g_free (s);
	}
	return gel_makenum_null();
}

static GelETree *
wait_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	double dsecs;
	long msecs;
	struct timeval tv;
	struct timeval tv2;

	if G_UNLIKELY ( ! check_argument_nonnegative_number (a, 0, "wait"))
		return NULL;

	dsecs = mpw_get_double (a[0]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}

	msecs = (long)(dsecs * 1000);
	gettimeofday (&tv, NULL);
	for (;;) {
		if (gel_evalnode_hook != NULL)
			(*gel_evalnode_hook)();
		if G_UNLIKELY (gel_interrupted) {
			break;
		}
		gettimeofday (&tv2, NULL);

		if ( ((tv2.tv_sec - tv.tv_sec) * 1000
		      - (tv.tv_usec / 1000)
		      + (tv2.tv_usec / 1000))
		     >= msecs)
			break;

		/* sleep 10ms, this is a HORRIBLE HACK! */
		/* FIXME: do some mainloop thingie over here */
		usleep (10000);
	}

	if G_UNLIKELY (gel_interrupted)
		return NULL;
	else
		return gel_makenum_null ();
}

/*print function*/
static GelETree *
print_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gboolean old_limit = gel_main_out->length_limit;
	gel_output_set_length_limit (gel_main_out, FALSE);
	if (a[0]->type==GEL_STRING_NODE) {
		gel_output_printf_full (gel_main_out, FALSE, "%s\n", a[0]->str.str);
	} else {
		gel_pretty_print_etree (gel_main_out, a[0]);
		gel_output_string (gel_main_out,"\n");
	}
	gel_output_set_length_limit (gel_main_out, old_limit);
	gel_output_flush (gel_main_out);
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
	gboolean old_limit = gel_main_out->length_limit;
	gel_output_set_length_limit (gel_main_out, FALSE);
	if(a[0]->type==GEL_STRING_NODE)
		gel_output_printf (gel_main_out, "%s", a[0]->str.str);
	else
		gel_print_etree (gel_main_out, a[0], TRUE);
	gel_output_set_length_limit (gel_main_out, old_limit);
	gel_output_flush(gel_main_out);
	return gel_makenum_null();
}
/*print function*/
static GelETree *
display_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	gboolean old_limit = gel_main_out->length_limit;
	if G_UNLIKELY ( ! check_argument_string (a, 0, "display"))
		return NULL;

	gel_output_set_length_limit (gel_main_out, FALSE);

	gel_output_printf(gel_main_out, "%s: ", a[0]->str.str);
	gel_pretty_print_etree (gel_main_out, a[1]);
	gel_output_string(gel_main_out, "\n");

	gel_output_set_length_limit (gel_main_out, old_limit);

	gel_output_flush(gel_main_out);
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
	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		id = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		id = d_intern (a[0]->str.str);
	}

	if G_UNLIKELY (id->protected_) {
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

	func = d_makevfunc (id, gel_copynode (a[1]));
	/* make function global */
	func->context = 0;
	d_addfunc_global (func);

	/*
	 * Evil optimization to avoid copying the node from the argument
	 */
	return gel_stealnode (a[1]);
}

static GelETree *
SetElement_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *id;
	GelEFunc *func;
	GelMatrixW *mat;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "SetElement"))
		return NULL;
	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		id = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		id = d_intern (a[0]->str.str);
	}

	if G_UNLIKELY (id->protected_) {
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

	func = d_lookup_only_global (id);

	if (func == NULL ||
	    func->type != GEL_VARIABLE_FUNC ||
	    func->data.user->type != GEL_MATRIX_NODE) {
		GelETree *t;

		GEL_GET_NEW_NODE (t);
		t->type = GEL_MATRIX_NODE;
		mat = t->mat.matrix = gel_matrixw_new ();
		t->mat.quoted = FALSE;
		gel_matrixw_set_size (mat, 1, 1);

		if G_UNLIKELY ( ! _gel_iter_set_element (mat, a[3], a[1], a[2])) {
			gel_freetree (t);
			return NULL;
		}

		func = d_makevfunc (id, t);
		/* make function global */
		func->context = 0;
		d_addfunc_global (func);
	} else {
		mat = func->data.user->mat.matrix;
		if G_UNLIKELY ( ! _gel_iter_set_element (mat, a[3], a[1], a[2])) {
			return NULL;
		}
	}

	/*
	 * Evil optimization to avoid copying the node from the argument
	 */
	return gel_stealnode (a[3]);
}

static GelETree *
SetVElement_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *id;
	GelEFunc *func;
	GelMatrixW *mat;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "SetVElement"))
		return NULL;
	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		id = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		id = d_intern (a[0]->str.str);
	}

	if G_UNLIKELY (id->protected_) {
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

	func = d_lookup_only_global (id);

	if (func == NULL ||
	    func->type != GEL_VARIABLE_FUNC ||
	    func->data.user->type != GEL_MATRIX_NODE) {
		GelETree *t;

		GEL_GET_NEW_NODE (t);
		t->type = GEL_MATRIX_NODE;
		mat = t->mat.matrix = gel_matrixw_new ();
		t->mat.quoted = FALSE;
		gel_matrixw_set_size (mat, 1, 1);

		if G_UNLIKELY ( ! _gel_iter_set_velement (mat, a[2], a[1])) {
			gel_freetree (t);
			return NULL;
		}

		func = d_makevfunc (id, t);
		/* make function global */
		func->context = 0;
		d_addfunc_global (func);
	} else {
		mat = func->data.user->mat.matrix;
		if G_UNLIKELY ( ! _gel_iter_set_velement (mat, a[2], a[1])) {
			return NULL;
		}
	}

	/*
	 * Evil optimization to avoid copying the node from the argument
	 */
	return gel_stealnode (a[2]);
}

static void
display_all_vars (void)
{
	GelContextFrame *all_contexts, *lic;
	GSList *funcs;
	GSList *li;
	gboolean printed_local_title = FALSE;

	all_contexts = d_get_all_contexts ();
	funcs = d_getcontext_global ();

	gel_output_string (gel_main_out,
			   _("Global variables:\n\n"));

	for (li = funcs; li != NULL; li = li->next) {
		GelEFunc *f = li->data;
		if (f->type != GEL_VARIABLE_FUNC ||
		    f->id == NULL ||
		    /* only for toplevel */ f->id->parameter ||
		    /* only for toplevel */ f->id->protected_ ||
		    f->id->token == NULL ||
		    f->data.user == NULL ||
		    f->context > 0)
			continue;

		gel_output_printf (gel_main_out, "%s = ", f->id->token);
		gel_print_etree (gel_main_out, f->data.user, FALSE /*no toplevel, keep this short*/);
		gel_output_string (gel_main_out, "\n");
	}


	if (d_curcontext () > 0) {
		int i = d_curcontext ();

		gel_output_string
			(gel_main_out, _("\nFunction call stack:\n"));
		gel_output_string
			(gel_main_out, _("(depth of context in parentheses)\n\n"));

		/* go over all local contexts (not the last one, that is global) */
		for (lic = all_contexts; lic != NULL && lic->next != NULL; lic = lic->next) {
			GelToken *tok = lic->name;

			if (tok == NULL) {
				gel_output_string (gel_main_out, "??");
			} else {
				gel_output_string (gel_main_out, tok->token);
			}

			gel_output_printf (gel_main_out, " (%d)", i);

			if (i <= 1) {
				gel_output_string (gel_main_out, "\n");
			} else {
				gel_output_string (gel_main_out, ", ");
			}

			i--;
		}
	}


	/* go over all local contexts (not the last one, that is global) */
	for (lic = all_contexts; lic != NULL && lic->next != NULL; lic = lic->next) {
		for (li = lic->functions; li != NULL; li = li->next) {
			GelEFunc *f = li->data;
			if (f->type != GEL_VARIABLE_FUNC ||
			    f->id == NULL ||
			    f->id->token == NULL ||
			    f->data.user == NULL ||
			    f->context <= 0)
				continue;

			if ( ! printed_local_title) {
				gel_output_string (gel_main_out,
					_("\nLocal variables:\n"));
				gel_output_string (gel_main_out,
					_("(depth of context in parentheses)\n\n"));
				printed_local_title = TRUE;
			}

			gel_output_printf (gel_main_out, "(%d) %s = ", f->context, f->id->token);
			gel_print_etree (gel_main_out, f->data.user, FALSE /*no toplevel, keep this short*/);
			gel_output_string (gel_main_out, "\n");
		}
	}
}

/*set function*/
static GelETree *
DisplayVariables_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int j;

	if (a == NULL)
		display_all_vars ();

	j = 0;
	while (a != NULL && a[j] != NULL) {
		GelToken *id;
		GelEFunc *f;
		if (a[j]->type == GEL_IDENTIFIER_NODE) {
			id = a[j]->id.id;
		} else if (a[j]->type == GEL_STRING_NODE) {
			id = d_intern (a[j]->str.str);
		} else {
			gel_errorout (_("%s: Argument number %d not a string or identifier"),
				      "DisplayVariables",
				      j+1);
			return NULL;
		}

		f = d_lookup_global (id);

		if (f == NULL) {
			gel_output_printf(gel_main_out, _("%s undefined\n"), id->token);
		} else {
			gel_output_printf(gel_main_out, "%s = ", id->token);
			gel_print_func (gel_main_out, f);
			gel_output_string(gel_main_out, "\n");
		}
		j++;
	}

	return gel_makenum_null ();
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

		if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 0, "rand"))
			return NULL;

		size = gel_get_nonnegative_integer (a[0]->val.value, "rand");
		if G_UNLIKELY (size < 0)
			return NULL;

		if (size == 0)
			return gel_makenum_null ();

		m = gel_matrix_new ();
		gel_matrix_set_size (m, size, 1, FALSE /* padding */);
		for (i = 0; i < size; i++) {
			mpw_t fr; 
			mpw_init (fr);
			mpw_rand (fr);

			gel_matrix_index (m, i, 0) = gel_makenum_use (fr);
		}

		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix_value_only_real_nonrational (m);
		n->mat.quoted = FALSE;

		return n;
	} else /* args == 2 */ {
		GelETree *n;
		GelMatrix *m;
		int sizex, sizey, i, j;

		if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 0, "rand") ||
				! check_argument_nonnegative_integer (a, 1, "rand"))
			return NULL;

		sizey = gel_get_nonnegative_integer (a[0]->val.value, "rand");
		if G_UNLIKELY (sizey < 0)
			return NULL;
		sizex = gel_get_nonnegative_integer (a[1]->val.value, "rand");
		if G_UNLIKELY (sizex < 0)
			return NULL;

		if (sizex == 0 || sizey == 0)
			return gel_makenum_null ();

		m = gel_matrix_new ();
		gel_matrix_set_size (m, sizex, sizey, FALSE /* padding */);
		for (j = 0; j < sizey; j++) {
			for (i = 0; i < sizex; i++) {
				mpw_t fr; 
				mpw_init (fr);
				mpw_rand (fr);

				gel_matrix_index (m, i, j) = gel_makenum_use (fr);
			}
		}

		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix_value_only_real_nonrational (m);
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

		if G_UNLIKELY ( ! check_argument_integer (a, 0, "randint"))
			return NULL;

		mpw_init (fr);
		mpw_randint (fr, a[0]->val.value);
		if G_UNLIKELY (gel_error_num != 0) {
			mpw_clear (fr);
			return NULL;
		}

		return gel_makenum_use (fr);
	} else if (args == 2) {
		GelETree *n;
		GelMatrix *m;
		int size, i;

		if G_UNLIKELY ( ! check_argument_integer (a, 0, "randint") ||
				! check_argument_nonnegative_integer (a, 1, "randint"))
			return NULL;

		size = gel_get_nonnegative_integer (a[1]->val.value, "randint");
		if G_UNLIKELY (size < 0)
			return NULL;

		if (size == 0)
			return gel_makenum_null ();

		m = gel_matrix_new ();
		gel_matrix_set_size (m, size, 1, FALSE /* padding */);
		for (i = 0; i < size; i++) {
			mpw_t fr;
			mpw_init (fr);
			mpw_randint (fr, a[0]->val.value);
			if G_UNLIKELY (gel_error_num != 0) {
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

		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix_value_only_integer (m);
		n->mat.quoted = FALSE;

		return n;
	} else /* args == 3 */ {
		GelETree *n;
		GelMatrix *m;
		int sizex, sizey, i, j;

		if G_UNLIKELY ( ! check_argument_integer (a, 0, "randint") ||
				! check_argument_nonnegative_integer (a, 1, "randint") ||
				! check_argument_nonnegative_integer (a, 2, "randint"))
			return NULL;

		sizey = gel_get_nonnegative_integer (a[1]->val.value, "randint");
		if G_UNLIKELY (sizey < 0)
			return NULL;
		sizex = gel_get_nonnegative_integer (a[2]->val.value, "randint");
		if G_UNLIKELY (sizex < 0)
			return NULL;

		if (sizex == 0 || sizey == 0)
			return gel_makenum_null ();

		m = gel_matrix_new ();
		gel_matrix_set_size (m, sizex, sizey, FALSE /* padding */);
		for (j = 0; j < sizey; j++) {
			for (i = 0; i < sizex; i++) {
				mpw_t fr;
				mpw_init (fr);
				mpw_randint (fr, a[0]->val.value);
				if G_UNLIKELY (gel_error_num != 0) {
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

		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix_value_only_integer (m);
		n->mat.quoted = FALSE;

		return n;
	}
}

GelETree *
gel_apply_func_to_matrixen (GelCtx *ctx,
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
	int i, j, w, h;
	int quote = 0;
	gboolean internal_exception = FALSE; 

	if(mat1->type == GEL_MATRIX_NODE &&
	   mat2->type == GEL_MATRIX_NODE) {
		m1 = mat1->mat.matrix;
		m2 = mat2->mat.matrix;
		quote = mat1->mat.quoted || mat2->mat.quoted;
	} else if(mat1->type == GEL_MATRIX_NODE) {
		m1 = mat1->mat.matrix;
		quote = mat1->mat.quoted;
		re_node = mat2;
	} else /*if(mat2->type == GEL_MATRIX_NODE)*/ {
		m1 = mat2->mat.matrix;
		quote = mat2->mat.quoted;
		re_node = mat1;
		reverse = TRUE;
	}
	
	if G_UNLIKELY (m2 && (gel_matrixw_width(m1) != gel_matrixw_width(m2) ||
			      gel_matrixw_height(m1) != gel_matrixw_height(m2))) {
		gel_errorout (_("Cannot apply function to two differently sized matrices"));
		return NULL;
	}

	w = gel_matrixw_width (m1);
	h = gel_matrixw_height (m1);
	
	/*make us a new empty node*/
	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	new = n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = quote;
	gel_matrixw_set_size (new, w, h);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
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
				GEL_GET_NEW_NODE(ni);
				ni->type = GEL_IDENTIFIER_NODE;
				ni->id.id = d_intern(ident);
				ni->id.uninitialized = FALSE;

				GEL_GET_NEW_NODE(nn);
				nn->type = GEL_OPERATOR_NODE;
				nn->op.oper = GEL_E_CALL;
				nn->op.nargs = 3;
				nn->op.args = ni;
				nn->op.args->any.next = gel_copynode(t[0]);
				nn->op.args->any.next->any.next = gel_copynode(t[1]);
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

GelETree *
gel_apply_func_to_matrix (GelCtx *ctx,
			  GelETree *mat, 
			  GelBIFunction function,
			  const char *ident, 
			  gboolean *exception)
{
	GelMatrixW *m;
	GelMatrixW *new;
	GelETree *n;
	int i, j, w, h;
	gboolean internal_exception = FALSE; 

	m = mat->mat.matrix;

	w = gel_matrixw_width(m);
	h = gel_matrixw_height(m);
	
	/*make us a new empty node*/
	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	new = n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = mat->mat.quoted;
	gel_matrixw_set_size (new, w, h);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
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
				GEL_GET_NEW_NODE(nn);
				nn->type = GEL_OPERATOR_NODE;
				nn->op.oper = GEL_E_CALL;
				nn->op.args = NULL;
				nn->op.nargs = 2;
				
				GEL_GET_NEW_NODE(ni);
				ni->type = GEL_IDENTIFIER_NODE;
				ni->id.id = d_intern(ident);
				ni->id.uninitialized = FALSE;
				
				nn->op.args = ni;
				nn->op.args->any.next = gel_copynode(t[0]);
				nn->op.args->any.next->any.next = NULL;

				gel_matrixw_set_index(new,i,j) = nn;
			} else if (e->type == GEL_VALUE_NODE &&
				   mpw_exact_zero_p (e->val.value)) {
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

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "ExpandMatrix"))
		return NULL;

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy (a[0]->mat.matrix);
	gel_expandmatrix (n);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
RowsOf_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "RowsOf"))
		return NULL;

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_rowsof (a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
ColumnsOf_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "ColumnsOf"))
		return NULL;

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_columnsof (a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
DiagonalOf_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "DiagonalOf"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_diagonalof (a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	return n;
}

static GelETree *
CountZeroColumns_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m;
	int i, j, w, h;
	int cnt;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "CountZeroColumns"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_ui (0);

	m = a[0]->mat.matrix;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	cnt = 0;
	/* Must be done in this order and not rowise as is usual for genius! */
	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if ( ! ( t == NULL ||
				 t->type == GEL_NULL_NODE ||
				 (t->type == GEL_VALUE_NODE &&
				  mpw_zero_p (t->val.value)))) {
				cnt++;
				break;
			}
		}
	}

	return gel_makenum_ui (w-cnt);
}

static GelETree *
StripZeroColumns_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	GelMatrix *nm;
	int i, j, w, h, tj;
	int cnt;
	GSList *cols, *li;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "StripZeroColumns"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	m = a[0]->mat.matrix;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	cnt = 0;
	cols = NULL;
	/* Must be done in this order and not rowise as is usual for genius! */
	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if ( ! ( t == NULL ||
				 t->type == GEL_NULL_NODE ||
				 (t->type == GEL_VALUE_NODE &&
				  mpw_zero_p (t->val.value)))) {
				cols = g_slist_prepend (cols,
							GINT_TO_POINTER (i));
				cnt++;
				break;
			}
		}
	}

	if (cnt == w) {
		g_slist_free (cols);
		return gel_copynode (a[0]);
	} else if (cnt == 0) {
		return gel_makenum_null ();
	}

	nm = gel_matrix_new ();
	gel_matrix_set_size (nm, cnt, h, FALSE /* padding */);

	tj = cnt-1;
	for (li = cols; li != NULL; li = li->next) {
		i = GPOINTER_TO_INT (li->data);
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if (t != NULL)
				gel_matrix_index (nm, tj, j) =
					gel_copynode (t);
		}
		tj--;
	}

	g_slist_free (cols);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (nm);
	n->mat.quoted = a[0]->mat.quoted;

	return n;
}

/*ComplexConjugate function*/
static GelETree *
ComplexConjugate_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (conj_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], ComplexConjugate_op, "ComplexConjugate", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (sin_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],sin_op,"sin", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "sin"))
		return NULL;

	mpw_init(fr);

	mpw_sin(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*sinc function*/
static GelETree *
sinc_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (sinc_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],sinc_op,"sinc", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "sinc"))
		return NULL;

	if (mpw_zero_p (a[0]->val.value))
		return gel_makenum_ui(1);

	mpw_init(fr);

	mpw_sin(fr,a[0]->val.value);
	mpw_div(fr,fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*sinh function*/
static GelETree *
sinh_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (sinh_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],sinh_op,"sinh", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (cos_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],cos_op,"cos", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (cosh_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],cosh_op,"cosh", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (tan_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],tan_op,"tan", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (atan_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],atan_op,"atan", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "atan"))
		return NULL;

	mpw_init(fr);

	mpw_arctan(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*atan2 (arctan2) function*/
static GelETree *
atan2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],atan2_op,"atan2", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "atan2") ||
			! check_argument_number (a, 1, "atan2"))
		return NULL;

	mpw_init (fr);
	mpw_arctan2 (fr,
		     a[0]->val.value,
		     a[1]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear (fr);
		return NULL;
	}

	return gel_makenum_use (fr);
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

/* Free fall acceleration */
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

/* CatalanConstant */
static GelETree *
CatalanConstant_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t cc;
	mpw_init (cc);
	mpw_catalan_constant (cc);
	return gel_makenum_use (cc);
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
	if G_LIKELY (golden_ratio_iscached)
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
ErrorFunction_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (ErrorFunction_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], ErrorFunction_op, "ErrorFunction", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "ErrorFunction"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		/* FIXME: this is evil! */
		if G_UNLIKELY (_internal_erf_function == NULL) {
			_internal_erf_function = d_makeufunc(d_intern("<internal>exp"),
							     gel_parseexp
							     ("twosqrtpi = 2/sqrt(pi); "
							      "a = 1; "
							      "s = 0; "
							      "n = 0; "
							      "f = 1; "
							      "xx = x; "
							      "xsq = x^2; "
							      "do ( "
							      " t = xx * a * twosqrtpi; "
							      " s = s + t; "
							      " increment n; "
							      " f = f * n; "
							      " a = ((-1)^n) / (((2*n)+1) * f); "
							      " xx = xx * xsq "
							      ") while (|t| > ErrorFunctionTolerance); "
							      "s ",
							      NULL, FALSE, FALSE,
							      NULL, NULL),
							     g_slist_append(NULL,d_intern("x")),1,
							     NULL);
		}
		return gel_funccall(ctx,_internal_erf_function,a,1);

		return NULL;
	} else {
		MPW_MPF_REAL (num, a[0]->val.value, tmp);

		mpfr_init (ret);
		mpfr_erf (ret, num, GMP_RNDN);

		MPW_MPF_KILL (num, tmp);

		mpw_init (retw);
		mpw_set_mpf_use (retw, ret);

		return gel_makenum (retw);
	}
}

static GelETree *
RiemannZeta_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (RiemannZeta_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], RiemannZeta_op, "RiemannZeta", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "RiemannZeta"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "RiemannZeta");
		return NULL;
	}

	MPW_MPF_REAL (num, a[0]->val.value, tmp);

	mpfr_init (ret);
	mpfr_zeta (ret, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

static GelETree *
GammaFunction_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (GammaFunction_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], GammaFunction_op, "GammaFunction", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "GammaFunction"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "GammaFunction");
		return NULL;
	}

	MPW_MPF_REAL (num, a[0]->val.value, tmp);

	mpfr_init (ret);
	mpfr_gamma (ret, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

static GelETree *
BesselJ0_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (BesselJ0_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], BesselJ0_op, "BesselJ0", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "BesselJ0"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "BesselJ0");
		return NULL;
	}

	MPW_MPF_REAL (num, a[0]->val.value, tmp);

	mpfr_init (ret);
	mpfr_j0 (ret, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

static GelETree *
BesselJ1_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (BesselJ1_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], BesselJ1_op, "BesselJ1", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "BesselJ1"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "BesselJ1");
		return NULL;
	}

	MPW_MPF_REAL (num, a[0]->val.value, tmp);

	mpfr_init (ret);
	mpfr_j1 (ret, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

/* FIXME: implement over matrices / functions */
static GelETree *
BesselJn_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;
	long n;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], BesselJn_op, "BesselJn", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "BesselJn"))
		return NULL;
	n = mpw_get_long(a[0]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}

	if G_UNLIKELY ( ! check_argument_number (a, 1, "BesselJn"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[1]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "BesselJn");
		return NULL;
	}

	MPW_MPF_REAL (num, a[1]->val.value, tmp);

	mpfr_init (ret);
	mpfr_jn (ret, n, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

static GelETree *
BesselY0_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (BesselY0_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], BesselY0_op, "BesselY0", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "BesselY0"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "BesselY0");
		return NULL;
	}
	if G_UNLIKELY (mpw_sgn (a[0]->val.value) <= 0) {
		gel_errorout (_("%s: Bessel functions of second kind not defined for nonpositive real numbers"),
			      "BesselY0");
		return NULL;
	}

	MPW_MPF_REAL (num, a[0]->val.value, tmp);

	mpfr_init (ret);
	mpfr_y0 (ret, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

static GelETree *
BesselY1_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (BesselY1_function, a[0]);
	}

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], BesselY1_op, "BesselY1", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "BesselY1"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[0]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "BesselY1");
		return NULL;
	}
	if G_UNLIKELY (mpw_sgn (a[0]->val.value) <= 0) {
		gel_errorout (_("%s: Bessel functions of second kind not defined for nonpositive real numbers"),
			      "BesselY1");
		return NULL;
	}

	MPW_MPF_REAL (num, a[0]->val.value, tmp);

	mpfr_init (ret);
	mpfr_y1 (ret, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}

/* FIXME: implement over matrices / functions */
static GelETree *
BesselYn_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpfr_ptr num;
	mpfr_t tmp;
	mpfr_t ret;
	mpw_t retw;
	long n;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], BesselYn_op, "BesselYn", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "BesselYn"))
		return NULL;
	n = mpw_get_long(a[0]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}

	if G_UNLIKELY ( ! check_argument_number (a, 1, "BesselYn"))
		return NULL;
	if G_UNLIKELY (mpw_is_complex (a[1]->val.value)) {
		gel_errorout (_("%s: Not implemented (yet) for complex values"),
			      "BesselYn");
		return NULL;
	}
	if G_UNLIKELY (mpw_sgn (a[1]->val.value) <= 0) {
		gel_errorout (_("%s: Bessel functions of second kind not defined for nonpositive real numbers"),
			      "BesselYn");
		return NULL;
	}

	MPW_MPF_REAL (num, a[1]->val.value, tmp);

	mpfr_init (ret);
	mpfr_yn (ret, n, num, GMP_RNDN);

	MPW_MPF_KILL (num, tmp);

	mpw_init (retw);
	mpw_set_mpf_use (retw, ret);

	return gel_makenum (retw);
}


static GelETree *
IsNull_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_NULL_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsValue_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_VALUE_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsBoolean_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_BOOL_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsString_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_STRING_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsMatrix_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsVector_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_MATRIX_NODE &&
	    (gel_matrixw_width(a[0]->mat.matrix) == 1 ||
	     gel_matrixw_height(a[0]->mat.matrix) == 1))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsFunction_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_FUNCTION_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsFunctionOrIdentifier_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_FUNCTION_NODE || a[0]->type == GEL_IDENTIFIER_NODE)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsFunctionRef_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_OPERATOR_NODE &&
	   a[0]->op.oper == GEL_E_REFERENCE) {
		GelETree *arg = a[0]->op.args;
		g_assert(arg);
		if(arg->type==GEL_IDENTIFIER_NODE &&
		   d_lookup_global(arg->id.id))
			return gel_makenum_bool (1);
	}
	return gel_makenum_bool (0);
}
static GelETree *
IsComplex_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE)
		return gel_makenum_bool (0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsReal_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE)
		return gel_makenum_bool (0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (0);
	else
		return gel_makenum_bool (1);
}
static GelETree *
IsMatrixReal_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (1);

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixReal"))
		return NULL;

	if (gel_is_matrix_value_only_real (a[0]->mat.matrix))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (0);
	else if(mpw_is_integer(a[0]->val.value))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsPositiveInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (0);
	else if(mpw_is_integer(a[0]->val.value) &&
		mpw_sgn (a[0]->val.value) > 0)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsNonNegativeInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (0);
	else if(mpw_is_integer(a[0]->val.value) &&
		mpw_sgn (a[0]->val.value) >= 0)
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsGaussInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE)
		return gel_makenum_bool (0);
	else if(mpw_is_complex_integer(a[0]->val.value))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsMatrixInteger_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (1);

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixInteger"))
		return NULL;

	if (gel_is_matrix_value_only_integer (a[0]->mat.matrix))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsRational_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (0);
	else if(mpw_is_rational(a[0]->val.value) ||
		mpw_is_integer(a[0]->val.value))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsComplexRational_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE)
		return gel_makenum_bool (0);
	else if (mpw_is_complex_rational_or_integer (a[0]->val.value))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsMatrixRational_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (1);

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixRational"))
		return NULL;

	if (gel_is_matrix_value_only_rational (a[0]->mat.matrix))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}
static GelETree *
IsFloat_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type!=GEL_VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_bool (0);
	else if(mpw_is_real_part_float(a[0]->val.value))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}

static GelETree *
trunc_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (trunc_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],trunc_op,"trunc", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (floor_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],floor_op,"floor", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (ceil_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],ceil_op,"ceil", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (round_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],round_op,"round", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (float_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],float_op,"float", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (Numerator_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],Numerator_op,"Numerator", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Numerator"))
		return NULL;
	mpw_init(fr);
	mpw_numerator(fr,a[0]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
Denominator_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (Denominator_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],Denominator_op,"Denominator", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Denominator"))
		return NULL;
	mpw_init(fr);
	mpw_denominator(fr,a[0]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
Re_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (Re_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],Re_op,"Re", exception);

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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (Im_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],Im_op,"Im", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "Im"))
		return NULL;
	mpw_init(fr);
	mpw_im(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
sqrt_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (sqrt_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],sqrt_op,"sqrt", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "sqrt"))
		return NULL;
	if (ctx->modulo != NULL) {
		GelETree *arg[3];
		GelETree *ret;
		gboolean is_prime;
		mpz_ptr num;
		GelEFunc *SqrtModPrime;
		static GelToken *SqrtModPrime_id = NULL;

		if G_UNLIKELY ( ! check_argument_integer (a, 0, "sqrt"))
			return NULL;

		num = mpw_peek_real_mpz (ctx->modulo);
		is_prime = mympz_is_prime (num, -1);

		if G_UNLIKELY ( ! is_prime) {
			gel_errorout (_("%s: square root for composite moduli "
					"is not yet implemented"), "sqrt");
			return NULL;
		}
		if G_UNLIKELY (SqrtModPrime_id == NULL)
			SqrtModPrime_id = d_intern ("SqrtModPrime");
		SqrtModPrime = d_lookup_only_global (SqrtModPrime_id);
		if G_UNLIKELY (SqrtModPrime == NULL) {
			gel_errorout (_("%s: Cannot find square root function "
					"for prime moduli"), "sqrt");
			return NULL;
		}
		arg[0] = a[0];
		arg[1] = gel_makenum (ctx->modulo);
		arg[2] = NULL;
		ret = gel_funccall (ctx, SqrtModPrime, arg, 2);
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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (exp_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE) {
		if G_UNLIKELY (gel_matrixw_width(a[0]->mat.matrix) !=
			       gel_matrixw_height(a[0]->mat.matrix)) {
			gel_errorout (_("%s: matrix argument is not square"),
				      "exp");
			return NULL;
		}
		if G_UNLIKELY (_internal_exp_function == NULL) {
			/* FIXME: this really is not good*/
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
		}
		return gel_funccall(ctx,_internal_exp_function,a,1);
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

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (ln_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],ln_op,"ln", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "ln"))
		return NULL;
	mpw_init(fr);
	mpw_ln(fr,a[0]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
log2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (log2_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],log2_op,"log2", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "log2"))
		return NULL;
	mpw_init(fr);
	mpw_log2(fr,a[0]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
log10_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t fr;

	if (a[0]->type == GEL_FUNCTION_NODE ||
	    a[0]->type == GEL_IDENTIFIER_NODE) {
		return gel_function_from_function (log10_function, a[0]);
	}

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],log10_op,"log10", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "log10"))
		return NULL;
	mpw_init(fr);
	mpw_log10(fr,a[0]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
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

	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],gcd2_op,"gcd", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "gcd") ||
			! check_argument_integer (a, 1, "gcd"))
		return NULL;

	mpw_init(tmp);
	mpw_gcd(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*gcd function*/
static GelETree *
gcd_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int i;

	if (a[1] == NULL) {
		if (a[0]->type == GEL_MATRIX_NODE) {
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
		} else if (a[0]->type == GEL_VALUE_NODE) {
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

	{
		GelETree *gcd;
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
}

/*lcm function*/
static GelETree *
lcm2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],lcm2_op,"lcm", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "lcm") ||
			! check_argument_integer (a, 1, "lcm"))
		return NULL;

	mpw_init(tmp);
	mpw_lcm(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*lcm function*/
static GelETree *
lcm_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int i;

	if (a[1] == NULL) {
		if (a[0]->type == GEL_MATRIX_NODE) {
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
		} else if (a[0]->type == GEL_VALUE_NODE) {
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

	{
		GelETree *lcm;
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
}

/*jacobi function*/
static GelETree *
Jacobi_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],Jacobi_op,"Jacobi", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Jacobi") ||
			! check_argument_integer (a, 1, "Jacobi"))
		return NULL;

	mpw_init(tmp);
	mpw_jacobi(tmp,
		   a[0]->val.value,
		   a[1]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

static GelETree *
IntegerQuotient_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpz_ptr num1, num2;
	mpz_t quo;
	mpw_t numw;

	if (a[0]->type == GEL_MATRIX_NODE ||
	    a[1]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1], IntegerQuotient_op, "IntegerQuotient", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IntegerQuotient") ||
			! check_argument_integer (a, 1, "IntegerQuotient"))
		return NULL;

	num1 = mpw_peek_real_mpz (a[0]->val.value);
	num2 = mpw_peek_real_mpz (a[1]->val.value);

	if (mpz_sgn (num2) == 0) {
		gel_errorout (_("Division by zero!"));

		return NULL;
	}

	mpz_init (quo);
	mpz_fdiv_q (quo, num1, num2);
	mpw_init (numw);
	mpw_set_mpz_use (numw, quo);
	return gel_makenum_use (numw);
}

/*kronecker function*/
static GelETree *
JacobiKronecker_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t tmp;

	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1], JacobiKronecker_op, "JacobiKronecker", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "JacobiKronecker") ||
			! check_argument_integer (a, 1, "JacobiKronecker"))
		return NULL;

	mpw_init(tmp);
	mpw_kronecker(tmp,
		      a[0]->val.value,
		      a[1]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
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

	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],Legendre_op,"Legendre", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Legendere") ||
			! check_argument_integer (a, 1, "Legendere"))
		return NULL;

	mpw_init(tmp);
	mpw_legendre(tmp,
		     a[0]->val.value,
		     a[1]->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*perfect square testing function*/
static GelETree *
IsPerfectSquare_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],IsPerfectSquare_op,"IsPerfectSquare", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "IsPerfectSquare"))
		return NULL;

	if(mpw_perfect_square(a[0]->val.value)) {
		return gel_makenum_bool (1);
	} else {
		if G_UNLIKELY (gel_error_num) {
			gel_error_num = 0;
			return NULL;
		}
		return gel_makenum_bool (0);
	}
}


/*perfect square testing function*/
static GelETree *
IsPerfectPower_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],IsPerfectPower_op,"IsPerfectPower", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsPerfectPower"))
		return NULL;

	if(mpw_perfect_power(a[0]->val.value)) {
		return gel_makenum_bool (1);
	} else {
		if G_UNLIKELY (gel_error_num) {
			gel_error_num = 0;
			return NULL;
		}
		return gel_makenum_bool (0);
	}
}

static GelETree *
IsEven_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],IsEven_op,"IsEven", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsEven"))
		return NULL;

	if(mpw_even_p(a[0]->val.value)) {
		return gel_makenum_bool (1);
	} else {
		if G_UNLIKELY (gel_error_num) {
			gel_error_num = 0;
			return NULL;
		}
		return gel_makenum_bool (0);
	}
}

static GelETree *
IsOdd_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],IsOdd_op,"IsOdd", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsOdd"))
		return NULL;

	if(mpw_odd_p(a[0]->val.value)) {
		return gel_makenum_bool (1);
	} else {
		if G_UNLIKELY (gel_error_num) {
			gel_error_num = 0;
			return NULL;
		}
		return gel_makenum_bool (0);
	}
}

/*max function for two elements */
static GelETree *
max2_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],max2_op,"max", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "max") ||
			! check_argument_number (a, 1, "max"))
		return NULL;

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)<0)
		return gel_copynode (a[1]);
	else {
		if G_UNLIKELY (gel_error_num) {
			gel_error_num = 0;
			return NULL;
		}
		return gel_copynode (a[0]);
	}
}

/*max function*/
static GelETree *
max_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *max = NULL;
	int i;
	if (a[1] == NULL) {
		if (a[0]->type == GEL_MATRIX_NODE) {
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
			return gel_copynode (max);
		} else if (a[0]->type == GEL_VALUE_NODE) {
			if (mpw_is_complex (a[0]->val.value)) {
				gel_errorout (_("%s: Cannot compare complex numbers"),
					      "max");
				return NULL;
			}

			/*
			 * Evil optimization to avoid copying the node from the argument
			 */
			return gel_stealnode (a[0]);
		} else {
			gel_errorout (_("%s: Input not a number or a matrix of numbers."),
				      "max");
			return NULL;
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
		/*
		 * Evil optimization to avoid copying the node from the argument
		 */
		return gel_stealnode (a[0]);
	else
		return max;
}

/*min function*/
static GelETree *
min2_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(a[0]->type==GEL_MATRIX_NODE ||
	   a[1]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen(ctx,a[0],a[1],min2_op,"min", exception);

	if G_UNLIKELY ( ! check_argument_number (a, 0, "min") ||
			! check_argument_number (a, 1, "min"))
		return NULL;

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)>0)
		return gel_copynode (a[1]);
	else {
		if G_UNLIKELY (gel_error_num) {
			gel_error_num = 0;
			return NULL;
		}
		return gel_copynode (a[0]);
	}
}

/*min function*/
static GelETree *
min_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *min = NULL;
	int i;
	if (a[1] == NULL) {
		if (a[0]->type == GEL_MATRIX_NODE) {
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
			return gel_copynode (min);
		} else if (a[0]->type == GEL_VALUE_NODE) {
			if (mpw_is_complex (a[0]->val.value)) {
				gel_errorout (_("%s: Cannot compare complex numbers"),
					      "min");
				return NULL;
			}
			/*
			 * Evil optimization to avoid copying the node from the argument
			 */
			return gel_stealnode (a[0]);
		} else {
			gel_errorout (_("%s: Input not a number or a matrix of numbers."),
				      "min");
			return NULL;
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
		/*
		 * Evil optimization to avoid copying the node from the argument
		 */
		return gel_stealnode (a[0]);
	else
		return min;
}

static GelETree *
IsValueOnly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (1);

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsValueOnly"))
		return NULL;
	
	if(gel_is_matrix_value_only(a[0]->mat.matrix))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}

static GelETree *
IsMatrixPositive_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m;
	int i,j,w,h;

	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "IsMatrixPositive"))
		return NULL;

	m = a[0]->mat.matrix;

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if (t == NULL ||
			    t->type != GEL_VALUE_NODE ||
			    mpw_is_complex (t->val.value) ||
			    mpw_sgn (t->val.value) <= 0)
				return gel_makenum_bool (0);
		}
	}
	return gel_makenum_bool (1);
}

static GelETree *
IsMatrixNonnegative_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m;
	int i,j,w,h;

	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "IsMatrixNonnegative"))
		return NULL;

	m = a[0]->mat.matrix;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if (t != NULL) {
				if (t->type != GEL_VALUE_NODE ||
				    mpw_is_complex (t->val.value) ||
				    mpw_sgn (t->val.value) < 0)
					return gel_makenum_bool (0);
			}
		}
	}
	return gel_makenum_bool (1);
}

static GelETree *
IsZero_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_null_or_number_or_matrix (a, 0, "IsZero"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (1);
	else if (a[0]->type == GEL_VALUE_NODE)
		return gel_makenum_bool (mpw_zero_p (a[0]->val.value));
	else {
		GelMatrixW *m = a[0]->mat.matrix;
		int i,j,w,h;
		w = gel_matrixw_width (m);
		h = gel_matrixw_height (m);
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				GelETree *t = gel_matrixw_get_index (m, i, j);
				if ( ! ( t == NULL ||
					 (t->type == GEL_VALUE_NODE &&
					  mpw_zero_p (t->val.value)))) {
					return gel_makenum_bool (0);
				}
			}
		}
		return gel_makenum_bool (1);
	}
}

static GelETree *
IsIdentity_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_null_or_number_or_matrix (a, 0, "IsIdentity"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (0);
	else if (a[0]->type == GEL_VALUE_NODE)
		return gel_makenum_bool (mpw_eql_ui (a[0]->val.value, 1));
	else {
		GelMatrixW *m = a[0]->mat.matrix;
		int i,j,w,h;
		w = gel_matrixw_width (m);
		h = gel_matrixw_height (m);
		if (w != h)
			return gel_makenum_bool (0);
		for (j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				GelETree *t = gel_matrixw_get_index (m, i, j);
				if (i == j) {
					if (t == NULL ||
					    t->type != GEL_VALUE_NODE ||
					    ! mpw_eql_ui (t->val.value, 1)) {
						return gel_makenum_bool (0);
					}
				} else if ( ! ( t == NULL ||
					 (t->type == GEL_VALUE_NODE &&
					  mpw_zero_p (t->val.value)))) {
					return gel_makenum_bool (0);
				}
			}
		}
		return gel_makenum_bool (1);
	}
}

static GelETree *
I_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long size;
	int i;
	static int cached_size = -1;
	static GelMatrixW *cached_m = NULL;

	if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 0, "I"))
		return NULL;

	size = gel_get_nonnegative_integer (a[0]->val.value, "I");
	if G_UNLIKELY (size < 0)
		return NULL;

	if (size == 0)
		return gel_makenum_null ();

	/*make us a new empty node*/
	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.quoted = FALSE;

	if (cached_size == size) {
		n->mat.matrix = gel_matrixw_copy (cached_m);
	} else {
		GelMatrixW *m;

		if (cached_m != NULL)
			gel_matrixw_free (cached_m);
		n->mat.matrix = m = gel_matrixw_new();
		gel_matrixw_set_size (m, size, size);

		for (i = 0; i < size; i++)
			gel_matrixw_set_indexii (m, i) =
				gel_makenum_ui(1);
		/* This is in row reduced form, duh! */
		m->rref = 1;

		m->cached_value_only = 1;
		m->value_only = 1;
		m->cached_value_only_real = 1;
		m->value_only_real = 1;
		m->cached_value_only_rational = 1;
		m->value_only_rational = 1;
		m->cached_value_only_integer = 1;
		m->value_only_integer = 1;
		m->cached_value_or_bool_only = 1;
		m->value_or_bool_only = 1;

		cached_m = gel_matrixw_copy (m);
		cached_size = size;
	}

	return n;
}

static GelETree *
zeros_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	long rows, cols;

	if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 0, "zeros") ||
			(a[1] != NULL &&
			 ! check_argument_nonnegative_integer (a, 1, "zeros")))
		return NULL;

	if G_UNLIKELY (a[1] != NULL && a[2] != NULL) {
		gel_errorout (_("%s: too many arguments"), "zeros");
		return NULL;
	}

	rows = gel_get_nonnegative_integer (a[0]->val.value, "zeros");
	if G_UNLIKELY (rows < 0)
		return NULL;
	if (a[1] != NULL) {
		cols = gel_get_nonnegative_integer (a[1]->val.value, "zeros");
		if G_UNLIKELY (cols < 0)
			return NULL;
	} else {
		/* In this case we want a row vector */
		cols = rows;
		rows = 1;
	}

	if (cols == 0 || rows == 0)
		return gel_makenum_null ();

	/*make us a new empty node*/
	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = m = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, cols, rows);

	/* trivially rref */
	m->rref = 1;

	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_real = 1;
	m->value_only_real = 1;
	m->cached_value_only_rational = 1;
	m->value_only_rational = 1;
	m->cached_value_only_integer = 1;
	m->value_only_integer = 1;
	m->cached_value_or_bool_only = 1;
	m->value_or_bool_only = 1;
	
	return n;
}

static GelETree *
ones_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	long rows, cols;
	int i, j;

	if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 0, "ones") ||
			(a[1] != NULL &&
			 ! check_argument_nonnegative_integer (a, 1, "ones")))
		return NULL;

	if G_UNLIKELY (a[1] != NULL && a[2] != NULL) {
		gel_errorout (_("%s: too many arguments"), "ones");
		return NULL;
	}

	rows = gel_get_nonnegative_integer (a[0]->val.value, "ones");
	if (rows < 0)
		return NULL;
	if (a[1] != NULL) {
		cols = gel_get_nonnegative_integer (a[1]->val.value, "ones");
		if (cols < 0)
			return NULL;
	} else {
		/* In this case we want a row vector */
		cols = rows;
		rows = 1;
	}

	if (cols == 0 || rows == 0)
		return gel_makenum_null ();

	/*make us a new empty node*/
	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = m = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, cols, rows);

	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_real = 1;
	m->value_only_real = 1;
	m->cached_value_only_rational = 1;
	m->value_only_rational = 1;
	m->cached_value_only_integer = 1;
	m->value_only_integer = 1;
	m->cached_value_or_bool_only = 1;
	m->value_or_bool_only = 1;
	
	for(j=0;j<rows;j++)
		for(i=0;i<cols;i++)
			gel_matrixw_set_index (m, i, j) =
				gel_makenum_ui (1);

	return n;
}

static GelETree *
rows_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_ui (0);
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "rows"))
		return NULL;
	return gel_makenum_ui(gel_matrixw_height(a[0]->mat.matrix));
}
static GelETree *
columns_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_ui (0);
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "columns"))
		return NULL;
	return gel_makenum_ui(gel_matrixw_width(a[0]->mat.matrix));
}
static GelETree *
elements_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_ui (0);
	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "elements"))
		return NULL;
	return gel_makenum_ui (gel_matrixw_width (a[0]->mat.matrix) *
			       gel_matrixw_height (a[0]->mat.matrix));
}
static GelETree *
IsMatrixSquare_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (1);

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "IsMatrixSquare"))
		return NULL;
	if (gel_matrixw_width (a[0]->mat.matrix) == gel_matrixw_height (a[0]->mat.matrix))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}

static GelETree *
IsLowerTriangular_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int i,j,w;
	GelMatrixW *m;

	if G_UNLIKELY ( ! check_argument_square_matrix (a, 0, "IsLowerTriangular"))
		return NULL;

	m = a[0]->mat.matrix;

	w = gel_matrixw_width (m);
	for (i = 1; i < w; i++) {
		for (j = 0; j < i; j++) {
			GelETree *node = gel_matrixw_get_index (m, i, j);
			if (node != NULL &&
			    (node->type != GEL_VALUE_NODE ||
			     /* FIXME: perhaps use some zero tolerance */
			     ! mpw_zero_p (node->val.value))) {
				return gel_makenum_bool (0);
			}
		}
	}
	return gel_makenum_bool (1);
}

static GelETree *
IsUpperTriangular_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int i,j,w;
	GelMatrixW *m;

	if G_UNLIKELY ( ! check_argument_square_matrix (a, 0, "IsUpperTriangular"))
		return NULL;

	m = a[0]->mat.matrix;

	w = gel_matrixw_width (m);
	for (j = 1; j < w; j++) {
		for (i = 0; i < j; i++) {
			GelETree *node = gel_matrixw_get_index (m, i, j);
			if (node != NULL &&
			    (node->type != GEL_VALUE_NODE ||
			     /* FIXME: perhaps use some zero tolerance */
			     ! mpw_zero_p (node->val.value))) {
				return gel_makenum_bool (0);
			}
		}
	}
	return gel_makenum_bool (1);
}

static GelETree *
IsDiagonal_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int i,j,w;
	GelMatrixW *m;

	if G_UNLIKELY ( ! check_argument_square_matrix (a, 0, "IsDiagonal"))
		return NULL;

	m = a[0]->mat.matrix;

	w = gel_matrixw_width (m);
	for (j = 0; j < w; j++) {
		for (i = 0; i < w; i++) {
			GelETree *node = gel_matrixw_get_index (m, i, j);
			if (i != j &&
			    node != NULL &&
			    (node->type != GEL_VALUE_NODE ||
			     /* FIXME: perhaps use some zero tolerance */
			     ! mpw_zero_p (node->val.value))) {
				return gel_makenum_bool (0);
			}
		}
	}
	return gel_makenum_bool (1);
}

static GelETree *
SetMatrixSize_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	long w,h;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "SetMatrixSize"))
		return NULL;
	if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 1, "SetMatrixSize"))
		return NULL;
	if G_UNLIKELY ( ! check_argument_nonnegative_integer (a, 2, "SetMatrixSize"))
		return NULL;

	w = gel_get_nonnegative_integer (a[1]->val.value, "SetMatrixSize");
	if G_UNLIKELY (w < 0)
		return NULL;
	h = gel_get_nonnegative_integer (a[2]->val.value, "SetMatrixSize");
	if G_UNLIKELY (h < 0)
		return NULL;

	if (w == 0 || h == 0)
		return gel_makenum_null ();

	n = gel_stealnode (a[0]);
	if (n->type == GEL_NULL_NODE) {
		GelMatrixW *m;

		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = m = gel_matrixw_new();
		n->mat.quoted = FALSE;
		gel_matrixw_set_size (m, h, w);

		/* trivially rref */
		m->rref = 1;

		m->cached_value_only = 1;
		m->value_only = 1;
		m->cached_value_only_real = 1;
		m->value_only_real = 1;
		m->cached_value_only_rational = 1;
		m->value_only_rational = 1;
		m->cached_value_only_integer = 1;
		m->value_only_integer = 1;
		m->cached_value_or_bool_only = 1;
		m->value_or_bool_only = 1;
	} else {
		gel_matrixw_set_size (n->mat.matrix, h, w);
	}
	return n;
}

static GelETree *
AppendElement_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	int eltnum;

	if G_UNLIKELY ( ! check_argument_vector_or_null (a, 0, "AppendElement"))
		return NULL;

	/*
	 * Evil optimization to avoid copying the node from the argument
	 */
	n = gel_stealnode (a[0]);
	if (n->type == GEL_NULL_NODE) {
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = m = gel_matrixw_new();
		n->mat.quoted = FALSE;
		gel_matrixw_set_size (m, 1, 1);

		eltnum = 0;

		/* trivially rref */
		m->rref = 1;

		m->cached_value_only = 1;
		m->value_only = 1;
		m->cached_value_only_real = 1;
		m->value_only_real = 1;
		m->cached_value_only_rational = 1;
		m->value_only_rational = 1;
		m->cached_value_only_integer = 1;
		m->value_only_integer = 1;
		m->cached_value_or_bool_only = 1;
		m->value_or_bool_only = 1;
	} else {
		m = n->mat.matrix;
		eltnum = gel_matrixw_elements(m);
	}

	gel_matrixw_set_velement (m,
				  eltnum,
				  /*
				   * Evil optimization to avoid copying the node from the argument
				   */
				  gel_stealnode(a[1]));
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
			! check_argument_nonnegative_integer (a, 1, "IndexComplement"))
		return NULL;

	len = gel_get_nonnegative_integer (a[1]->val.value, "IndexComplement");
	if G_UNLIKELY (len < 0)
		return NULL;

	if G_UNLIKELY (len == 0)
		return gel_makenum_null ();

	if (a[0]->type == GEL_MATRIX_NODE) {
		index = g_new0 (char, len);

		m = a[0]->mat.matrix;
		ml = gel_matrixw_elements (m);
		nml = len;
		for (i = 0; i < ml; i++) {
			GelETree *t = gel_matrixw_vindex (m, i);
			int elt;
			if G_UNLIKELY (t->type != GEL_VALUE_NODE) {
				gel_errorout (_("%s: vector argument not value only"), "IndexComplement");
				g_free (index);
				return NULL;
			}
			elt = gel_get_nonnegative_integer (t->val.value, "IndexComplement");
			if G_UNLIKELY (elt < 0) {
				g_free (index);
				return NULL;
			}

			if G_UNLIKELY (elt == 0) {
				gel_errorout (_("%s: argument can't be negative or 0"),
					      "IndexComplement");
				g_free (index);
				return NULL;
			}

			elt--;
			if G_UNLIKELY (elt >= len) {
				gel_errorout (_("%s: vector argument has too large entries"), "IndexComplement");
				g_free (index);
				return NULL;
			}

			if (index[elt] == 0) {
				nml --;
				index[elt] = 1;
			}
		}

		if (nml <= 0) {
			g_free (index);
			return gel_makenum_null ();
		}

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
		int elt = gel_get_nonnegative_integer (a[0]->val.value, "IndexComplement");
		if G_UNLIKELY (elt < 0)
			return NULL;
		if G_UNLIKELY (elt == 0) {
			gel_errorout (_("%s: argument can't be negative or 0"),
				      "IndexComplement");
			return NULL;
		}
		if G_UNLIKELY (elt > len) {
			gel_errorout (_("%s: vector argument has too large entries"), "IndexComplement");
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

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix_value_only_integer (nm);
	if (a[0]->type == GEL_MATRIX_NODE)
		n->mat.quoted = a[0]->mat.quoted;
	else
		n->mat.quoted = TRUE;

	return n;
}

static GelETree *
HermitianProduct_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m1, *m2;
	int i, len;
	mpw_t res;
	mpw_t trm;

	if G_UNLIKELY ( ! check_argument_value_only_vector (a, 0, "HermitianProduct") ||
			! check_argument_value_only_vector (a, 1, "HermitianProduct"))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;
	len = gel_matrixw_elements (m1);
	if G_UNLIKELY (gel_matrixw_elements (m2) != len) {
		gel_errorout (_("%s: arguments must be vectors of equal size"), "HermitianProduct");
		return NULL;
	}

	mpw_init (res);
	mpw_init (trm);
	mpw_set_ui (res, 0);
	for (i = 0; i < len; i++) {
		GelETree *t1 = gel_matrixw_vindex (m1, i);
		GelETree *t2 = gel_matrixw_vindex (m2, i);
		/* (t1 and t2 must be value only nodes! checked above!) */
		mpw_conj (trm, t2->val.value);
		mpw_mul (trm, trm, t1->val.value);
		mpw_add (res, res, trm);
	}

	mpw_clear (trm);

	return gel_makenum_use (res);
}

static gboolean
symbolic_isinmatrix (GelETree *n, GelMatrixW *m)
{
	int w, h, i, j;

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);

	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_index (m, i, j);
			if (gel_is_tree_same (t, n)) {
				return TRUE;
			}
		}
	}
	/*int elts, i;

	elts = gel_matrixw_elements (m);

	for (i = 0; i < elts; i++) {
		GelETree *t = gel_matrixw_vindex (m, i);
		if (gel_is_tree_same (t, n)) {
			return TRUE;
		}
	}*/

	return FALSE;
}

static GelETree *
IsIn_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 1, "IsIn"))
		return NULL;

	if (a[1]->type == GEL_NULL_NODE)
		return gel_makenum_bool (FALSE);

	return gel_makenum_bool (symbolic_isinmatrix (a[0], a[1]->mat.matrix));
}

static GelETree *
IsSubset_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *mX, *mY;
	int w, h, i, j;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "IsSubset") ||
			! check_argument_matrix_or_null (a, 1, "IsSubset"))
		return NULL;

	/* emptyset is a subset of everything */
	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_bool (TRUE);

	/* now we know that X is not empty so if Y is empty
	 * then answer is no */
	if (a[1]->type == GEL_NULL_NODE)
		return gel_makenum_bool (FALSE);

	mX = a[0]->mat.matrix;
	mY = a[1]->mat.matrix;

	w = gel_matrixw_width (mX);
	h = gel_matrixw_height (mX);

	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_index (mX, i, j);
			if ( ! symbolic_isinmatrix (t, mY)) {
				return gel_makenum_bool (FALSE);
			}
		}
	}
	return gel_makenum_bool (TRUE);

}

static GelETree *
SetMinus_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m1, *m2;
	int w, h, i, j;
	int len;
	GSList *list, *li;
	GelETree *n;
	GelMatrix *nm;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "SetMinus") ||
			! check_argument_matrix_or_null (a, 1, "SetMinus"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	} else if (a[1]->type == GEL_NULL_NODE) {
		return gel_copynode (a[0]);
	}

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	list = NULL;
	len = 0;

	w = gel_matrixw_width (m1);
	h = gel_matrixw_height (m1);

	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_index (m1, i, j);
			if ( ! symbolic_isinmatrix (t, m2)) {
				if (t == the_zero)
					list = g_slist_prepend (list, NULL);
				else
					list = g_slist_prepend (list, gel_copynode (t));
				len ++;
			}
		}
	}
	if (list == NULL) {
		return gel_makenum_null ();
	}

	nm = gel_matrix_new ();
	gel_matrix_set_size (nm, len, 1, FALSE /* padding */);
	/* go backwards to "preserver order" */
	li = list;
	for (i = len-1; i >= 0; i--) {
		gel_matrix_index (nm, i, 0) = li->data;
		li = li->next;
	}
	g_slist_free (list);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (nm);
	n->mat.quoted = a[0]->mat.quoted;

	return n;
}

static GelETree *
Intersection_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m1, *m2;
	int w, h, i, j;
	int len;
	GSList *list, *li;
	GelETree *n;
	GelMatrix *nm;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "Intersection") ||
			! check_argument_matrix_or_null (a, 1, "Intersection"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	} else if (a[1]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	}

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	list = NULL;
	len = 0;

	w = gel_matrixw_width (m1);
	h = gel_matrixw_height (m1);

	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_index (m1, i, j);
			if (symbolic_isinmatrix (t, m2)) {
				if (t == the_zero)
					list = g_slist_prepend (list, NULL);
				else
					list = g_slist_prepend (list, gel_copynode (t));
				len ++;
			}
		}
	}
	if (list == NULL) {
		return gel_makenum_null ();
	}

	nm = gel_matrix_new ();
	gel_matrix_set_size (nm, len, 1, FALSE /* padding */);
	/* go backwards to "preserver order" */
	li = list;
	for (i = len-1; i >= 0; i--) {
		gel_matrix_index (nm, i, 0) = li->data;
		li = li->next;
	}
	g_slist_free (list);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (nm);
	n->mat.quoted = a[0]->mat.quoted;

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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss (ctx, n->mat.matrix, FALSE, FALSE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = FALSE;
	return n;
}
static GelETree *
rref_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "rref"))
		return NULL;

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	if ( ! n->mat.matrix->rref) {
		gel_value_matrix_gauss (ctx, n->mat.matrix, TRUE, FALSE, FALSE, FALSE, NULL, NULL);
	}
	n->mat.quoted = FALSE;
	return n;
}

/* cols and rows should have enough space (min(cols,rows) of m)
 * and m should be in at least ref (if not rref) form) and value only,
 * returns the count.  The values returned are zero based! */
static int
get_pivot_cols (GelMatrixW *m, int *cols, int *rows)
{
	int i, j, w, h, mwh;
	int cnt = 0;

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	mwh = MIN (w, h);
	
	for (j = 0; j < mwh; j++) {
		for (i = j; i < w; i++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if (t != NULL &&
			    ! mpw_zero_p (t->val.value)) {
				cols[cnt] = i;
				rows[cnt] = j;
				cnt++;
				break;
			}
		}
	}
	return cnt;
}

/* PivotColumns
 * Given a matrix in rref form, the columns which have a leading 1
 * in some row are the pivot columns.
 * (also returns in which row they occur) */
static GelETree *
PivotColumns_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	GelMatrix *nm;
	gboolean copied_m = FALSE;
	int *cols, *rows;
	int cnt, mwh;
	int i;

	if G_UNLIKELY (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "PivotColumns"))
		return NULL;

	m = a[0]->mat.matrix;
	if ( ! m->rref) {
		m = gel_matrixw_copy (m);
		/* only do ref, not rref for speed */
		gel_value_matrix_gauss (ctx, m, FALSE, FALSE, FALSE, FALSE, NULL, NULL);
		copied_m = TRUE;
	}

	mwh = MIN (gel_matrixw_width (m), gel_matrixw_height (m));

	cols = g_new (int, mwh);
	rows = g_new (int, mwh);

	cnt = get_pivot_cols (m, cols, rows);

	if (copied_m)
		gel_matrixw_free (m);

	if (cnt == 0) {
		g_free (cols);
		g_free (rows);
		return gel_makenum_null ();
	}

	nm = gel_matrix_new ();
	gel_matrix_set_size (nm, cnt, 2, FALSE /* padding */);
	for (i = 0; i < cnt; i++) {
		gel_matrix_index (nm, i, 0) = gel_makenum_ui (cols[i]+1);
		gel_matrix_index (nm, i, 1) = gel_makenum_ui (rows[i]+1);
	}

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix_value_only_integer (nm);
	n->mat.quoted = FALSE;

	g_free (cols);
	g_free (rows);

	return n;
}

/*
# Null space/kernel of a linear transform
# Okay, here's the idea:
# Row reduce a matrix. Then the non-pivot columns are basically
# the independent variables, and the pivot columns are the dependent ones.
# So if your row reduced matrix looks like this:
# [1 0 0  2 4]
# [0 0 1 -3 5]
# then to find a basis for the kernel, look at your non-pivot columns
# (4, 5)
# and for each non-pivot column, you get one vector.
# So take the fourth column, and start off with the vector [0,0,0,-1,0].'
# (so a -1 in the fourth place)
# Now in each pivot entry, you need to put a value to cancel what this
# -1 gives you -- so the pivot column entries are 2 and -3 (the entries
# of the fourth column that have a pivot to the left of them).
# So the first vector is [2,0,-3,-1,0], and the second is
# [4,0,5,0,-1]
# This is poorly explained (FIXME), but some examples should make it
# clear (find a good reference for this!)
*/

static GelETree *
NullSpace_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	GelMatrix *nm;
	gboolean copied_m = FALSE;
	int *pivot_cols, *pivot_rows;
	int dim_image;
	int number_of_pivots, mwh;
	int i, ii, j, pi;

	if G_UNLIKELY (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "NullSpace"))
		return NULL;

	m = a[0]->mat.matrix;
	if ( ! m->rref) {
		m = gel_matrixw_copy (m);
		if (gel_value_matrix_gauss (ctx, m,
					    TRUE /* reduce */,
					    FALSE /* uppertriang */,
					    FALSE /* stopsing */,
					    TRUE /* stopnonsing */,
					    NULL /* detop */,
					    NULL /* simul */)) {
			gel_matrixw_free (m);
			return gel_makenum_null ();
		}
		copied_m = TRUE;
	}

	dim_image = gel_matrixw_width (m);

	mwh = MIN (dim_image, gel_matrixw_height (m));

	pivot_cols = g_new (int, mwh);
	pivot_rows = g_new (int, mwh);

	number_of_pivots = get_pivot_cols (m, pivot_cols, pivot_rows);

	if (dim_image == number_of_pivots) {
		if (copied_m)
			gel_matrixw_free (m);

		g_free (pivot_cols);
		g_free (pivot_rows);

		return gel_makenum_null ();
	}

	nm = gel_matrix_new ();
	gel_matrix_set_size (nm, dim_image - number_of_pivots, dim_image,
			     FALSE /* padding */);

	j = 0;

	/* Loop over nonpivots */
	ii = 0;
	for (i = 0; i < dim_image; i++) { 
		/* skip pivots */
		if (ii < number_of_pivots &&
		    i == pivot_cols[ii]) {
			ii++;
			continue;
		}

		gel_matrix_index (nm, j, i) = gel_makenum_si (-1);

		for (pi = 0; pi < number_of_pivots; pi++) {
			if (pivot_cols[pi] < i) {
				GelETree *t = gel_matrixw_get_index
					(m, i, pivot_rows[pi]);
				if (t != NULL)
					gel_matrix_index (nm, j, pivot_cols[pi])
						= gel_copynode (t);
			} else {
				break;
			}
		}
		j++;
	}

	if (copied_m)
		gel_matrixw_free (m);

	g_free (pivot_cols);
	g_free (pivot_rows);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix_value_only (nm);
	n->mat.quoted = FALSE;

	return n;
}


static GelEFunc *
get_reference (GelETree *a, const char *argname, const char *func)
{
	if G_LIKELY (a->type == GEL_OPERATOR_NODE &&
		     a->op.oper == GEL_E_REFERENCE) {
		GelETree *arg = a->op.args;
		g_assert(arg);
		if G_UNLIKELY (arg->type != GEL_IDENTIFIER_NODE ||
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
		GelETree *node = gel_matrixw_get_index (m, i, r);
		if (node != NULL &&
		    (node->type != GEL_VALUE_NODE ||
		     /* FIXME: perhaps use some zero tolerance */
		     ! mpw_zero_p (node->val.value))) {
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

	ret = gel_value_matrix_gauss (ctx, RM, TRUE, FALSE, FALSE, FALSE, NULL, RV);

	if (retm != NULL) {
		GEL_GET_NEW_NODE(n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = RM;
		n->mat.quoted = FALSE;
		d_set_value (retm, n);
	} else {
		gel_matrixw_free (RM);
	}

	if (retv != NULL) {
		GEL_GET_NEW_NODE(n);
		n->type = GEL_MATRIX_NODE;
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
		GEL_GET_NEW_NODE(n);
		n->type = GEL_MATRIX_NODE;
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

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],Prime_op,"prime", exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "Prime"))
		return NULL;

	num = gel_get_nonnegative_integer (a[0]->val.value, "Prime");
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
	    numprimes<=num-1 && numprimes <= MAXPRIMES && i<=G_MAXUINT-1;i+=2) {
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

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],NextPrime_op,"NextPrime", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "NextPrime"))
		return NULL;

	mpw_init (ret);
	mpw_nextprime (ret, a[0]->val.value);
	if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
		mpw_clear (ret);
		/* eek! should not happen */
		gel_error_num = GEL_NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
LucasNumber_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t ret;

	if(a[0]->type==GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix(ctx,a[0],LucasNumber_op,"LucasNumber", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "LucasNumber"))
		return NULL;

	mpw_init (ret);
	mpw_lucnum (ret, a[0]->val.value);
	if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
		mpw_clear (ret);
		gel_error_num = GEL_NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
IsPrime_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr num;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0], IsPrime_op, "IsPrime", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "IsPrime"))
		return NULL;

	num = mpw_peek_real_mpz (a[0]->val.value);

	ret = mympz_is_prime (num, -1);

	return gel_makenum_bool (ret);
}

static GelETree *
StrongPseudoprimeTest_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr num;
	mpz_ptr b;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1],
						   StrongPseudoprimeTest_op,
						   "StrongPseudoprimeTest",
						   exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "StrongPseudoprimeTest") ||
			! check_argument_positive_integer (a, 1, "StrongPseudoprimeTest"))
		return NULL;

	num = mpw_peek_real_mpz (a[0]->val.value);
	b = mpw_peek_real_mpz (a[1]->val.value);

	ret = mympz_strong_pseudoprime_test (num, b);

	return gel_makenum_bool (ret);
}

static GelETree *
MillerRabinTest_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	int reps;
	mpz_ptr num;

	if (a[0]->type == GEL_MATRIX_NODE ||
	    a[1]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1],
						   MillerRabinTest_op,
						   "MillerRabinTest",
						   exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "MillerRabinTest") ||
			! check_argument_positive_integer (a, 1, "MillerRabinTest"))
		return NULL;

	reps = gel_get_nonnegative_integer (a[1]->val.value, "MillerRabinTest");
	if (reps < 0)
		return NULL;

	num = mpw_peek_real_mpz (a[0]->val.value);

	ret = mpz_millerrabin (num, reps);

	return gel_makenum_bool (ret);
}

static GelETree *
MillerRabinTestSure_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr num;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0],
						 MillerRabinTestSure_op,
						 "MillerRabinTestSure", exception);

	if G_UNLIKELY ( ! check_argument_positive_integer (a, 0, "MillerRabinTestSure"))
		return NULL;
	if G_UNLIKELY (mpw_cmp_ui (a[0]->val.value, 2) <= 0) {
		gel_errorout (_("%s: argument must be greater "
				"than 2"), "MillerRabinTestSure");
		return NULL;
	}

	num = mpw_peek_real_mpz (a[0]->val.value);

	ret = mympz_miller_rabin_test_sure (num);

	return gel_makenum_bool (ret);
}

static GelETree *
Factorize_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpz_ptr numz;
	GArray *fact;
	GelETree *n;
	GelMatrixW *mn;
	guint i;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix (ctx, a[0],
						 Factorize_op,
						 "Factorize", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Factorize"))
		return NULL;

	numz = mpw_peek_real_mpz (a[0]->val.value);

	fact = mympz_pollard_rho_factorize (numz);

	/* error or interrupt or whatnot */
	if G_UNLIKELY (fact == NULL) {
		RAISE_EXCEPTION (exception);
		return NULL;
	}

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (mn, fact->len, 2);
	
	for (i = 0; i < fact->len; i++) {
		GelFactor f = g_array_index (fact, GelFactor, i);
		mpw_t num;
		mpw_init (num);
		mpw_set_mpz_use (num, f.num);
		gel_matrixw_set_index (mn, (int)i, 0) = gel_makenum_use (num);
		gel_matrixw_set_index (mn, (int)i, 1) = gel_makenum_ui (f.exp);
	}

	g_array_free (fact, TRUE /*free segment */);

	return n;
}

static GelETree *
ModInvert_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpw_t ret;

	if (a[0]->type == GEL_MATRIX_NODE ||
	    a[1]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1],
						   ModInvert_op,
						   "ModInvert",
						   exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "ModInvert") ||
			! check_argument_integer (a, 1, "ModInvert"))
		return NULL;

	mpw_init (ret);
	mpw_invert (ret, a[0]->val.value, a[1]->val.value);
	if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
		mpw_clear (ret);
		gel_error_num = GEL_NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
Divides_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	int ret;
	mpz_ptr numa, numb;

	if (a[0]->type == GEL_MATRIX_NODE ||
	    a[1]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1], Divides_op, "Divides", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Divides") ||
			! check_argument_integer (a, 1, "Divides"))
		return NULL;

	numa = mpw_peek_real_mpz (a[0]->val.value);
	numb = mpw_peek_real_mpz (a[1]->val.value);

	if (mpz_sgn (numa) == 0) {
		gel_errorout (_("Division by zero!"));

		return NULL;
	}

	ret = mpz_divisible_p (numb, numa);

	return gel_makenum_bool (ret);
}

static GelETree *
ExactDivision_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	mpz_ptr numa, numb;
	mpz_t ret;
	mpw_t retw;

	if (a[0]->type == GEL_MATRIX_NODE ||
	    a[1]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1], ExactDivision_op, "ExactDivision", exception);

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "ExactDivision") ||
			! check_argument_integer (a, 1, "ExactDivision"))
		return NULL;

	numa = mpw_peek_real_mpz (a[0]->val.value);
	numb = mpw_peek_real_mpz (a[1]->val.value);

	if (mpz_sgn (numb) == 0) {
		gel_errorout (_("Division by zero!"));

		return NULL;
	}

	mpz_init (ret);
	mpz_divexact (ret, numa, numb);

	mpw_init (retw);
	mpw_set_mpz_use (retw, ret);

	return gel_makenum (retw);
}

/* this can return 0! unlike what poly_cut_zeros does */
static int
poly_find_cutoff_size (GelMatrixW *m)
{
	int i;
	int cutoff;
	for(i = gel_matrixw_width(m)-1; i >= 0; i--) {
		GelETree *t = gel_matrixw_get_index(m,i,0);
	       	if (t != NULL &&
		    ! mpw_zero_p (t->val.value))
			break;
	}
	cutoff = i+1;
	return cutoff;
}

static void
poly_cut_zeros(GelMatrixW *m)
{
	int i;
	int cutoff;
	for(i=gel_matrixw_width(m)-1;i>=1;i--) {
		GelETree *t = gel_matrixw_get_index(m,i,0);
	       	if (t != NULL &&
		    ! mpw_zero_p (t->val.value))
			break;
	}
	cutoff = i+1;
	if(cutoff==gel_matrixw_width(m))
		return;
	gel_matrixw_set_size(m,cutoff,1);
}

static gboolean
check_poly(GelETree * *a, int args, const char *func, gboolean complain)
{
	int i,j;

	for (j = 0; j < args; j++) {
		if (a[j]->type != GEL_MATRIX_NODE ||
		    gel_matrixw_height (a[j]->mat.matrix) != 1) {
			if G_UNLIKELY (complain)
				gel_errorout (_("%s: arguments not horizontal vectors"),
					      func);
			return FALSE;
		}

		for(i=0;i<gel_matrixw_width(a[j]->mat.matrix);i++) {
			GelETree *t = gel_matrixw_index(a[j]->mat.matrix,i,0);
			if (t->type != GEL_VALUE_NODE) {
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
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
				gel_copynode(gel_matrixw_get_index(m1,i,0));
		} else /*if(i<gel_matrixw_width(m2)*/ {
			gel_matrixw_set_index(mn,i,0) =
				gel_copynode(gel_matrixw_set_index(m2,i,0));
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
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
				gel_copynode(gel_matrixw_set_index(m1,i,0));
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = FALSE;
	size = gel_matrixw_width(m1) + gel_matrixw_width(m2);
	gel_matrixw_set_size(mn,size,1);
	
	mpw_init(accu);
		
	for(i=0;i<gel_matrixw_width(m1);i++) {
		for(j=0;j<gel_matrixw_width(m2);j++) {
			GelETree *l,*r,*nn;
			l = gel_matrixw_get_index(m1,i,0);
			r = gel_matrixw_get_index(m2,j,0);
			if (l == NULL ||
			    r == NULL ||
			    mpw_zero_p (l->val.value) ||
			    mpw_zero_p (r->val.value))
				continue;
			mpw_mul(accu,l->val.value,r->val.value);
			nn = gel_matrixw_get_index(mn,i+j,0);
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
DividePoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n, *rn, *ql;
	long size, sizeq;
	int i, j;
	GelMatrixW *pm, *qm, *mn, *rem;
	GelEFunc *retrem = NULL;
	mpw_t tmp;
	
	if G_UNLIKELY ( ! check_poly (a, 2, "DividePoly", TRUE))
		return NULL;
	if (a[2] != NULL) {
		if (a[2]->type != GEL_NULL_NODE) {
			retrem = get_reference (a[2], _("third argument"),
						"DividePoly");
			if G_UNLIKELY (retrem == NULL)
				return NULL;
		}
		if G_UNLIKELY (a[3] != NULL) {
			gel_errorout (_("%s: too many arguments"),
				      "DividePoly");
			return NULL;
		}
	}

	pm = a[0]->mat.matrix;
	qm = a[1]->mat.matrix;

	size = poly_find_cutoff_size (pm);
	sizeq = poly_find_cutoff_size (qm);

	if (sizeq <= 0) {
		gel_errorout ("%s: %s",
			      "DividePoly",
			      _("Division by zero!"));
		return NULL;
	}

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new ();
	n->mat.quoted = FALSE;

	/* nothing to do */
	if (size < sizeq) {
		gel_matrixw_set_size (mn, 1, 1);

		if (retrem != NULL) {
			GEL_GET_NEW_NODE(rn);
			rn->type = GEL_MATRIX_NODE;
			rn->mat.matrix = gel_matrixw_copy (pm);
			poly_cut_zeros (rn->mat.matrix);
			rn->mat.quoted = FALSE;
			d_set_value (retrem, rn);
		}

		return n;
	}

	gel_matrixw_set_size (mn, size, 1);

	rem = gel_matrixw_copy (pm);
	gel_matrixw_make_private (rem, TRUE /* kill_type_caches */);

	/* we know ql can't be zero */
	ql = gel_matrixw_index (qm, sizeq-1, 0);

	mpw_init (tmp);

	for (i = size-sizeq; i >= 0; i--) {
		GelETree *pt;
		pt = gel_matrixw_get_index (rem, i+sizeq-1, 0);
		if (pt == NULL || mpw_zero_p (pt->val.value)) {
			/* Leave mn[i,0] at NULL (zero) */
			continue;
		}
		gel_matrixw_set_index (mn, i, 0) = pt;
		gel_matrixw_set_index (rem, i+sizeq-1, 0) = NULL;
		mpw_div (pt->val.value,
			 pt->val.value, ql->val.value);

		for (j = 0; j < sizeq-1; j++) {
			GelETree *rv, *qt;
			rv = gel_matrixw_get_index (rem, i+j, 0);
			if (rv == NULL)
				rv = gel_matrixw_set_index (rem, i+j, 0)
					= gel_makenum_ui (0);
			qt = gel_matrixw_index (qm, j, 0);
			mpw_mul (tmp, pt->val.value, qt->val.value);
			mpw_sub (rv->val.value, rv->val.value, tmp);
		}
	}

	mpw_clear (tmp);

	poly_cut_zeros (mn);

	if (retrem != NULL) {
		poly_cut_zeros (rem);

		GEL_GET_NEW_NODE (rn);
		rn->type = GEL_MATRIX_NODE;
		rn->mat.matrix = rem;
		rn->mat.quoted = FALSE;
		d_set_value (retrem, rn);
	} else {
		gel_matrixw_free (rem);
	}

	return n;
}

static GelETree *
PolyDerivative_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if G_UNLIKELY ( ! check_poly(a,1,"PolyDerivative",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	n->mat.quoted = FALSE;
	
	poly_cut_zeros(n->mat.matrix);

	return n;
}

static GelETree *
IsPoly_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	if(check_poly(a,1,"IsPoly",FALSE))
		return gel_makenum_bool (1);
	else
		return gel_makenum_bool (0);
}

static GelETree *
QuadraticFormula_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	GelMatrixW *m;
	GelMatrix *nm;

	mpw_ptr aa, bb, cc;
	mpw_t r1, r2;

	if G_UNLIKELY (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_poly(a,1,"QuadraticFormula",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	if G_UNLIKELY (gel_matrixw_elements (m) != 3 ||
		       mpw_zero_p (gel_matrixw_index(m,2,0)->val.value)) {
		gel_errorout (_("%s: argument 1 must be a quadratic polynomial"),
			      "QuadraticFormula");
		return NULL;
	}

	aa = gel_matrixw_index(m,2,0)->val.value;
	bb = gel_matrixw_index(m,1,0)->val.value;
	cc = gel_matrixw_index(m,0,0)->val.value;

	mpw_init (r1);
	mpw_init (r2);

	if (mpw_zero_p (cc)) {
		mpw_div (r1, bb, aa);
		mpw_neg (r1, r1);
		mpw_set_ui (r2, 0);
	} else if (mpw_zero_p (bb)) {
		mpw_mul (r1, aa, cc);
		mpw_neg (r1, r1);
		mpw_sqrt (r1, r1);
		mpw_div (r1, r1, aa);
		mpw_neg (r2, r1);
	} else {
		mpw_mul (r1, bb, bb);
		mpw_mul (r2, aa, cc);
		mpw_mul_ui (r2, r2, 4);
		mpw_sub (r1, r1, r2);
		mpw_sqrt (r1, r1);
		/* r1 is now the sqrt of the discriminant */

		/* try to avoid instability */
		if (mpw_re_sgn (r1) != mpw_re_sgn (bb)) {
			mpw_neg (r1, r1);
		}

		mpw_add (r1, r1, bb);
		mpw_div_ui (r1, r1, 2);
		mpw_neg (r1, r1);

		/* r1 = (bb + s * sqrt(bb^2 - 4*aa*cc)) / (-2); */

		/* set r2 first */
		mpw_div (r2, cc, r1);

		mpw_div (r1, r1, aa);
	}

	nm = gel_matrix_new ();
	gel_matrix_set_size (nm, 1, 2, FALSE /* padding */);
	gel_matrix_index (nm, 0, 0) = gel_makenum_use (r1);
	gel_matrix_index (nm, 0, 1) = gel_makenum_use (r2);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix_value_only (nm);
	n->mat.quoted = FALSE;

	return n;
}


static GelETree *
PolyToString_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;
	int i;
	GString *gs;
	gboolean any = FALSE;
	GelMatrixW *m;
	const char *var;
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
		if (mpw_zero_p (t->val.value))
			continue;
		/*positive (or complex) */
		if (mpw_is_complex (t->val.value) ||
		    mpw_sgn (t->val.value) > 0) {
			if(any) g_string_append(gs," + ");
			if (MPW_IS_COMPLEX (t->val.value)) {
				g_string_append_c (gs, '(');
				if (i==0) {
					gel_print_etree (gelo, t, FALSE);
					g_string_append_c (gs, ')');
				} else if ( ! mpw_eql_ui(t->val.value,1)) {
					gel_print_etree (gelo, t, FALSE);
					g_string_append_c (gs, ')');
					g_string_append_c(gs,'*');
				}
			} else {
				if (i == 0) {
					gel_print_etree (gelo, t, FALSE);
				} else if ( ! mpw_eql_ui (t->val.value, 1)) {
					gel_print_etree (gelo, t, FALSE);
					g_string_append_c(gs,'*');
				}
			}
			/*negative*/
		} else {
			if(any) g_string_append(gs," - ");
			else g_string_append_c(gs,'-');
			mpw_neg(t->val.value,t->val.value);
			if (i == 0) {
				gel_print_etree (gelo, t, FALSE);
			} else if ( ! mpw_eql_ui (t->val.value, 1)) {
				gel_print_etree (gelo, t, FALSE);
				g_string_append_c(gs,'*');
			}
			mpw_neg(t->val.value,t->val.value);
		}
		if(i==1)
			g_string_append_printf (gs, "%s", var);
		else if(i>1)
			g_string_append_printf (gs, "%s^%d", var, i);
		any = TRUE;
	}
	if(!any)
		g_string_append(gs,"0");

	r = gel_output_snarf_string (gelo);
	gel_output_unref (gelo);

	GEL_GET_NEW_NODE(n);
	n->type = GEL_STRING_NODE;
	n->str.str = r;
	n->str.constant = FALSE;
	
	return n;
}

static GelETree *
ptf_makenew_power(GelToken *id, int power)
{
	GelETree *n;
	GelETree *tokn;
	GEL_GET_NEW_NODE(tokn);
	tokn->type = GEL_IDENTIFIER_NODE;
	tokn->id.id = id;
	tokn->id.uninitialized = FALSE;

	if(power == 1)
		return tokn;

	GEL_GET_NEW_NODE(n);
	n->type = GEL_OPERATOR_NODE;
	n->op.oper = GEL_E_EXP;
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
	
	if (power == 0) {
		return gel_makenum (mul);
	} else if (mpw_eql_ui (mul, 1)) {
		n = ptf_makenew_power(id,power);
	} else {
		GEL_GET_NEW_NODE(n);
		n->type = GEL_OPERATOR_NODE;
		n->op.oper = GEL_E_MUL;
		n->op.args = gel_makenum (mul);
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
		if (mpw_zero_p (t->val.value))
			continue;
		
		if(!nn)
			nn = ptf_makenew_term(t->val.value,var,i);
		else {
			GelETree *nnn;
			GEL_GET_NEW_NODE(nnn);
			nnn->type = GEL_OPERATOR_NODE;
			nnn->op.oper = GEL_E_PLUS;
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_FUNCTION_NODE;
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
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

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_string ("");

	if G_UNLIKELY ( ! check_argument_matrix (a, 0, "ASCIIToString"))
		return NULL;

	m = a[0]->mat.matrix;

	size = gel_matrixw_elements (m);

	s = g_new0 (char, size+1);

	for (i = 0; i < size; i++) {
		GelETree *t;
		t = gel_matrixw_vindex (m, i);
		if (t->type != GEL_VALUE_NODE ||
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

	GEL_GET_NEW_NODE(n);
	n->type = GEL_MATRIX_NODE;
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

	if (a[0]->type == GEL_NULL_NODE)
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
		if G_UNLIKELY (t->type != GEL_VALUE_NODE ||
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

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		fname = a[0]->id.id->token;
	} else /* GEL_STRING_NODE */ {
		fname = a[0]->str.str;
	}
	
	gel_add_category (fname, a[1]->str.str);
	gel_add_description (fname, a[2]->str.str);

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

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		fname1 = a[0]->id.id->token;
	} else /* GEL_STRING_NODE */ {
		fname1 = a[0]->str.str;
	}

	if (a[1]->type == GEL_IDENTIFIER_NODE) {
		fname2 = a[1]->id.id->token;
	} else /* GEL_STRING_NODE */ {
		fname2 = a[1]->str.str;
	}
	
	gel_add_alias (fname1, fname2);

	return gel_makenum_null();
}

static GelETree *
Identity_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	/*
	 * Evil optimization to avoid copying the node from the argument
	 */
	return gel_stealnode (a[0]);
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

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix_value_only_integer (mm);
	n->mat.quoted = FALSE;

	return n;
}

/* eats the glist and frees it */
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
	g_slist_free (list);

	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
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

static int
nPr (unsigned int n, unsigned int k)
{
	/* assume k+1 <= n */
	guint64 m = 1;
	guint s = n-k+1;
	while (s <= n) {
		m *= (guint64)s;
		if (m > G_MAXINT32) return -1;
		s += 1;
	}
	return (int)m;
}

static int
nCr (unsigned int n, unsigned int k)
{
	mpz_t ret;
	int r;
	mpz_init (ret);

	mpz_bin_uiui (ret, n, k);
	if (mpz_fits_sint_p (ret)) {
		r = mpz_get_si (ret);
	} else {
		r = -1;
	}
	mpz_clear (ret);
	return r;
}

static GelETree *
Combinations_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	long k, n;
	int *comb;
	int i;
	int len;
	GelMatrix *mm;
	GelETree *r;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Combinations") ||
			! check_argument_integer (a, 1, "Combinations"))
		return NULL;

	gel_error_num = 0;
	k = mpw_get_long(a[0]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}
	n = mpw_get_long(a[1]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (n < 1 || n > G_MAXINT || k < 1 || k > n) {
		gel_errorout (_("%s: value out of range"),
			      "Combinations");
		return NULL;
	}

	len = nCr (n, k);
	if (len < 0) {
		gel_errorout (_("%s: value out of range"),
			      "Combinations");
		return NULL;
	}

	comb = g_new (int, k);
	for (i = 0; i < k; i++)
		comb[i] = i+1;

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, len, 1, FALSE /* padding */);

	GEL_GET_NEW_NODE (r);
	r->type = GEL_MATRIX_NODE;
	r->mat.matrix = gel_matrixw_new_with_matrix (mm);
	r->mat.quoted = FALSE;

	i = 0;
	do {
		gel_matrix_index (mm, i, 0) = etree_out_of_int_vector (comb, k);
		i++;
	} while (comb_get_next_combination (comb, k, n));

	g_free (comb);

	return r;
}

static GelETree *
Permutations_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *r;
	long k, n, len;
	int *comb;
	int *perm;
	char *arrow;
	int i, j;
	GelMatrix *mm;

	if G_UNLIKELY ( ! check_argument_integer (a, 0, "Permutations") ||
			! check_argument_integer (a, 1, "Permutations"))
		return NULL;

	gel_error_num = 0;
	k = mpw_get_long(a[0]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}
	n = mpw_get_long(a[1]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (n < 1 || n > G_MAXINT || k < 1 || k > n) {
		gel_errorout (_("%s: value out of range"),
			      "Permutations");
		return NULL;
	}

	len = nPr (n, k);
	if (len < 0) {
		gel_errorout (_("%s: value out of range"),
			      "Permutations");
		return NULL;
	}

	arrow = g_new (char, k);
	perm = g_new (int, k);
	comb = g_new (int, k);

	for (i = 0; i < k; i++)
		comb[i] = i+1;

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, len, 1, FALSE /* padding */);

	GEL_GET_NEW_NODE (r);
	r->type = GEL_MATRIX_NODE;
	r->mat.matrix = gel_matrixw_new_with_matrix (mm);
	r->mat.quoted = FALSE;

	j = 0;
	do {
		for (i = 0; i < k; i++)
			perm[i] = comb[i];
		for (i = 0; i < k; i++)
			arrow[i] = 'L';
		for (;;) {
			int m;

			gel_matrix_index (mm, j, 0) =
				etree_out_of_int_vector (perm, k);
			j++;

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

	return r;
}

static GelETree *
NextCombination_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	long k, n;
	int *comb;
	int i;
	GelETree *r;
	GelMatrixW *m;

	if G_UNLIKELY ( ! check_argument_value_only_matrix (a, 0, "NextCombination") ||
			! check_argument_integer (a, 1, "NextCombination"))
		return NULL;

	m = a[0]->mat.matrix;
	k = gel_matrixw_elements (m);

	gel_error_num = 0;
	n = mpw_get_long(a[1]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (n < 1 || n > G_MAXINT || k < 1 || k > n) {
		gel_errorout (_("%s: value out of range"),
			      "NextCombination");
		return NULL;
	}

	comb = g_new (int, k);
	for (i = 0; i < k; i++) {
		int j = mpw_get_long (gel_matrixw_vindex (m, i)->val.value);
		if G_UNLIKELY (gel_error_num != 0) {
			gel_error_num = 0;
			g_free (comb);
			return NULL;
		} else if G_UNLIKELY (j < 1 || j > n) {
			g_free (comb);
			gel_errorout (_("%s: value out of range"),
				      "NextCombination");
			return NULL;
		}
		comb[i] = j;
	}
	if (comb_get_next_combination (comb, k, n))
		r = etree_out_of_int_vector (comb, k);
	else
		r = gel_makenum_null ();
	g_free (comb);

	return r;
}


static GelETree *
nCr_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	unsigned long r;

	if (a[0]->type == GEL_MATRIX_NODE ||
	    a[1]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrixen (ctx, a[0], a[1],
						   nCr_op,
						   "nCr",
						   exception);

	if G_UNLIKELY ( ! check_argument_real_number (a, 0, "nCr") ||
			! check_argument_nonnegative_integer (a, 1, "nCr"))
		return NULL;

	gel_error_num = 0;
	r = mpw_get_ulong(a[1]->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return NULL;
	}

	if (mpw_is_integer (a[0]->val.value)) {
		mpw_t num;
		mpw_init (num);
		mpw_bin_ui (num, a[0]->val.value, r);
		return gel_makenum_use(num);
	} else {
		unsigned long i;
		mpw_t num, nm;
		mpw_init (num);
		mpw_set_ui (num, 1);
		mpw_init_set (nm, a[0]->val.value);
		for (i=0;i<=r-1;i++) {
			mpw_mul (num, num, nm);
			mpw_sub_ui (nm, nm, 1);
		}
		mpw_fac_ui (nm, r);
		mpw_div (num, num, nm);
		mpw_clear (nm);
		return gel_makenum_use(num);
	}
}

static GelETree *
NonzeroColumns_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m;
	int i, j, w, h;
	int cnt;
	GSList *cols;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "NonzeroColumns"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	m = a[0]->mat.matrix;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	cnt = 0;
	cols = NULL;
	/* Must be done in this order and not rowise as is usual for genius! */
	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
			if ( ! ( t == NULL ||
				 t->type == GEL_NULL_NODE ||
				 (t->type == GEL_VALUE_NODE &&
				  mpw_zero_p (t->val.value)))) {
				cols = g_slist_prepend (cols,
							gel_makenum_ui(i+1));
				cnt++;
				break;
			}
		}
	}

	if (cnt == 0) {
		return gel_makenum_null ();
	} else {
		cols = g_slist_reverse (cols);
		return etree_out_of_etree_list (cols, cnt);
	}
}

static GelETree *
NonzeroElements_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelMatrixW *m;
	int i, e;
	int cnt;
	GSList *elts;

	if G_UNLIKELY ( ! check_argument_matrix_or_null (a, 0, "NonzeroElements"))
		return NULL;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	m = a[0]->mat.matrix;
	e = gel_matrixw_elements (m);
	cnt = 0;
	elts = NULL;
	/* Must be done in this order and not rowise as is usual for genius! */
	for (i = 0; i < e; i++) {
		GelETree *t = gel_matrixw_get_vindex (m, i);
		if ( ! (t == NULL ||
			t->type == GEL_NULL_NODE ||
			(t->type == GEL_VALUE_NODE &&
			 mpw_zero_p (t->val.value)))) {
			elts = g_slist_prepend (elts,
						gel_makenum_ui(i+1));
			cnt++;
		}
	}

	if (cnt == 0) {
		return gel_makenum_null ();
	} else {
		elts = g_slist_reverse (elts);
		return etree_out_of_etree_list (elts, cnt);
	}
}

static GelETree *
protect_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *tok;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix
			(ctx, a[0], protect_op, "protect",
			 exception);

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "protect"))
		return NULL;

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}
	
	tok->protected_ = 1;

	return gel_makenum_null();
}

static GelETree *
unprotect_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelToken *tok;

	if (a[0]->type == GEL_MATRIX_NODE)
		return gel_apply_func_to_matrix
			(ctx, a[0], unprotect_op, "unprotect",
			 exception);

	if G_UNLIKELY ( ! check_argument_string_or_identifier (a, 0, "unprotect"))
		return NULL;

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}
	
	tok->protected_ = 0;

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

	if (a[0]->type == GEL_IDENTIFIER_NODE) {
		tok = a[0]->id.id;
	} else /* GEL_STRING_NODE */ {
		tok = d_intern (a[0]->str.str);
	}

	f = d_lookup_global (tok);
	if G_UNLIKELY (f == NULL) {
		gel_errorout (_("%s: undefined function"), "SetFunctionFlags");
		return NULL;
	}

	for (i = 1; a[i] != NULL; i++) {
		if G_UNLIKELY (a[i]->type != GEL_STRING_NODE) {
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

static gboolean
call_func (GelCtx *ctx,
	   mpw_ptr retn,
	   GelEFunc *func,
	   mpw_ptr argnum)
{
	GelETree arg;
	GelETree *ret;
	GelETree *args[2];

	arg.type = GEL_VALUE_NODE;
	arg.val.next = NULL;
	mpw_init_set (arg.val.value, argnum);

	args[0] = &arg;
	args[1] = NULL;

	ret = gel_funccall (ctx, func, args, 1);

	mpw_clear (arg.val.value);

	if G_UNLIKELY (gel_error_num != 0 ||
		       ret == NULL ||
		       ret->type != GEL_VALUE_NODE) {
		gel_freetree (ret);
		return FALSE;
	}

	mpw_set (retn, ret->val.value);
	
	gel_freetree (ret);
	return TRUE;
}

/*
# The algorithms are described in:
# Numerical Analysis, 5th edition
# by Richard L. Burden and J. Douglas Faires
# PWS Publishing Company, Boston, 1993.
# Library of congress: QA 297 B84 1993

# In the below, f indicates the function whose integral we wish to determine,
# a,b indicate the left and right endpoints of the interval over which
# we wish to integrate, and n is the number of intervals into which we
# divide [a,b]

# These methods all return one value, the value of the integral

# Currently only works for real functions of a real variable

# Composite Simpson's Rule, Section 4.4, Algorithm 4.1, p. 186
# Note that this has error term = max(f'''')*h^4*(b-a)/180,
# where h=(b-a)/n
# If we can get maximums and derivatives, this would allow us to determine
# automatically what n should be.
*/

/* ported from the GEL version for speed */
static GelETree *
CompositeSimpsonsRule_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelEFunc *f;
	mpw_ptr ia, ib, in;
	long n, i;
	mpw_t X, XI0, XI1, XI2, h, fret;
	GelETree *ret = NULL;
	gboolean do_neg = FALSE;

	if G_UNLIKELY ( ! check_argument_function_or_identifier (a, 0, "CompositeSimpsonsRule") ||
			! check_argument_real_number (a, 1, "CompositeSimpsonsRule") ||
			! check_argument_real_number (a, 2, "CompositeSimpsonsRule") ||
			! check_argument_positive_integer (a, 3, "CompositeSimpsonsRule"))
		return NULL;

	ia = a[1]->val.value;
	ib = a[2]->val.value;
	in = a[3]->val.value;
	if (mpw_odd_p (in))
		mpw_add_ui (in, in, 1);

	n = mpw_get_long (in);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}

	if (mpw_cmp (ia, ib) == 0) {
		return gel_makenum_ui (0);
	}

	if (mpw_cmp (ia, ib) > 0) {
		mpw_ptr tmp = ia;
		ia = ib;
		ib = tmp;
		do_neg = TRUE;
	}

	if (a[0]->type == GEL_FUNCTION_NODE) {
		f = a[0]->func.func;
	} else /* (a[0]->type == GEL_IDENTIFIER_NODE) */ {
		f = d_lookup_global (a[0]->id.id);
	}

	if G_UNLIKELY (f == NULL ||
		       f->nargs != 1) {
		gel_errorout (_("%s: argument not a function of one variable"),
			      "CompositeSimpsonsRule");
		return NULL;
	}

	mpw_init (fret);
	mpw_init (X);
	mpw_init (XI0);
	mpw_init (XI1);
	mpw_init (XI2);
	mpw_init (h);

	/*
	h=(b-a)/n;       # Subdivision interval
	*/
	mpw_sub (h, ib, ia);
	mpw_div (h, h, in);
	mpw_make_float (h);

	/*
	XI0=f(a)+f(b);   # End points
	*/
	if ( ! call_func (ctx, XI0, f, ia))
		goto end_of_simpson;
	if ( ! call_func (ctx, fret, f, ib))
		goto end_of_simpson;
	mpw_add (XI0, XI0, fret);

	/*
	XI1=0;           # odd points
	XI2=0;           # even points
        X=a;             # current position
	*/
	mpw_set_d (XI1, 0);
	mpw_set_d (XI2, 0);
	mpw_set (X, ia);
	mpw_make_float (X);

	/* FIXME: */
	for (i = 1; i < n; i++) {
		/*
		   X=X+h;
		   if i%2 == 0
		   then XI2=XI2+f(X)
		   else XI1=XI1+f(X)
		   */
		mpw_add (X, X, h);
		if ( ! call_func (ctx, fret, f, X))
			goto end_of_simpson;
		if (i & 0x1 /* odd */) {
			mpw_add (XI1, XI1, fret);
		} else /* even */ {
			mpw_add (XI2, XI2, fret);
		}

		if (gel_evalnode_hook) {
			if G_UNLIKELY ((i & 0x3FF) == 0x3FF) {
				(*gel_evalnode_hook)();
			}
		}
	}

	/*
        h*(XI0+2*XI2+4*XI1)/3
	*/
	mpw_mul_ui (XI1, XI1, 4);
	mpw_mul_ui (XI2, XI2, 2);
	mpw_add (fret, XI0, XI1);
	mpw_add (fret, fret, XI2);
	mpw_mul (fret, fret, h);
	mpw_div_ui (fret, fret, 3);

	if (do_neg)
		mpw_neg (fret, fret);

	ret = gel_makenum (fret);

end_of_simpson:
	mpw_clear (X);
	mpw_clear (fret);
	mpw_clear (XI0);
	mpw_clear (XI1);
	mpw_clear (XI2);
	mpw_clear (h);

	return ret;
}

static GelETree *
Parse_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *r;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_string (a, 0, "Parse"))
		return NULL;

	r = gel_parseexp (a[0]->str.str,
			  NULL /* infile */,
			  FALSE /* exec_commands */,
			  FALSE /* testparse */,
			  NULL /* finished */,
			  NULL /* dirprefix */);

	/* Have to reset the error here, else we may die */
	gel_error_num = GEL_NO_ERROR;
	gel_got_eof = FALSE;

	return r;
}

static GelETree *
Evaluate_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *et;

	if (a[0]->type == GEL_NULL_NODE)
		return gel_makenum_null ();

	if G_UNLIKELY ( ! check_argument_string (a, 0, "Evaluate"))
		return NULL;

	et = gel_parseexp (a[0]->str.str,
			   NULL /* infile */,
			   FALSE /* exec_commands */,
			   FALSE /* testparse */,
			   NULL /* finished */,
			   NULL /* dirprefix */);

	/* Have to reset the error here, else we may die */
	gel_error_num = GEL_NO_ERROR;
	gel_got_eof = FALSE;

	if (et == NULL)
		return NULL;


	return gel_eval_etree (ctx, et);
}

static GelETree *
AskString_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	char *txt;
	const char *def = NULL;

	if G_UNLIKELY ( ! check_argument_string (a, 0, "AskString"))
		return NULL;

	if (a[1] != NULL) {
		if G_UNLIKELY ( ! check_argument_string (a, 1, "AskString"))
			return NULL;
		def = a[1]->str.str;
	}

	txt = gel_ask_string (a[0]->str.str, def);

	if (txt == NULL)
		return gel_makenum_null ();
	else
		return gel_makenum_string_use (txt);
}

static GelETree *
AskButtons_op (GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GSList *buttons = NULL;
	int i;
	int ret;

	if G_UNLIKELY ( ! check_argument_string (a, 0, "AskButtons"))
		return NULL;

	i = 1;
	while (a != NULL && a[i] != NULL) {
		if G_UNLIKELY ( ! check_argument_string (a, i, "AskButtons")) {
			g_slist_free_full (buttons, g_free);
			return NULL;
		}
		buttons = g_slist_append (buttons, g_strdup (a[i]->str.str));
		i++;
	}

	ret = gel_ask_buttons (a[0]->str.str, buttons);

	g_slist_free_full (buttons, g_free);
	
	if (ret < 0)
		return gel_makenum_null ();
	else
		return gel_makenum_ui (ret);
}


static GelETree *
set_FloatPrecision (GelETree * a)
{
	long bits;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_FloatPrecision"))
		return NULL;

	bits = mpw_get_long(a->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (bits < 60 || bits > 16384) {
		gel_errorout (_("%s: argument should be between %d and %d"),
			      "set_FloatPrecision", 60, 16384);
		return NULL;
	}
	
	if(gel_calcstate.float_prec != bits) {
		gel_calcstate.float_prec = bits;
		mpw_set_default_prec (gel_calcstate.float_prec);
		gel_break_fp_caches ();
		gel_set_state (gel_calcstate);
	}

	return gel_makenum_ui(gel_calcstate.float_prec);
}

static GelETree *
get_FloatPrecision (void)
{
	return gel_makenum_ui(gel_calcstate.float_prec);
}

static GelETree *
set_MaxDigits (GelETree * a)
{
	long digits;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_MaxDigits"))
		return NULL;

	digits = mpw_get_long(a->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (digits < 0 || digits > 256) {
		gel_errorout (_("%s: argument should be between %d and %d"),
			      "set_MaxDigits", 0, 256);
		return NULL;
	}
	
	if(gel_calcstate.max_digits != digits) {
		gel_calcstate.max_digits = digits;
		gel_set_state (gel_calcstate);
	}

	return gel_makenum_ui(gel_calcstate.max_digits);
}

static GelETree *
get_MaxDigits (void)
{
	return gel_makenum_ui(gel_calcstate.max_digits);
}

static GelETree *
set_OutputChopExponent (GelETree * a)
{
	long e;

	if G_UNLIKELY ( ! check_argument_nonnegative_integer (&a, 0, "set_OutputChopExponent"))
		return NULL;

	e = mpw_get_long(a->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}
	
	if(gel_calcstate.chop != e) {
		gel_calcstate.chop = e;
		gel_set_state (gel_calcstate);
	}

	return gel_makenum_ui(gel_calcstate.chop);
}

static GelETree *
get_OutputChopExponent (void)
{
	return gel_makenum_ui(gel_calcstate.chop);
}

static GelETree *
set_OutputChopWhenExponent (GelETree * a)
{
	long e;

	if G_UNLIKELY ( ! check_argument_nonnegative_integer (&a, 0, "set_OutputChopWhenExponent"))
		return NULL;

	e = mpw_get_long(a->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}
	
	if(gel_calcstate.chop_when != e) {
		gel_calcstate.chop_when = e;
		gel_set_state (gel_calcstate);
	}

	return gel_makenum_ui(gel_calcstate.chop_when);
}

static GelETree *
get_OutputChopWhenExponent (void)
{
	return gel_makenum_ui(gel_calcstate.chop_when);
}

static GelETree *
set_ResultsAsFloats (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_ResultsAsFloats"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		gel_calcstate.results_as_floats = ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		gel_calcstate.results_as_floats = a->bool_.bool_;
	gel_set_state (gel_calcstate);

	return gel_makenum_bool (gel_calcstate.results_as_floats);
}
static GelETree *
get_ResultsAsFloats (void)
{
	return gel_makenum_bool (gel_calcstate.results_as_floats);
}
static GelETree *
set_ScientificNotation (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_ScientificNotation"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		gel_calcstate.scientific_notation = ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		gel_calcstate.scientific_notation = a->bool_.bool_;
	gel_set_state (gel_calcstate);

	return gel_makenum_bool (gel_calcstate.scientific_notation);
}
static GelETree *
get_ScientificNotation (void)
{
	return gel_makenum_bool (gel_calcstate.scientific_notation);
}
static GelETree *
set_FullExpressions (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_FullExpressions"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		gel_calcstate.full_expressions = ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		gel_calcstate.full_expressions = a->bool_.bool_;
	gel_set_state (gel_calcstate);

	return gel_makenum_bool (gel_calcstate.full_expressions);
}
static GelETree *
get_FullExpressions (void)
{
	return gel_makenum_bool (gel_calcstate.full_expressions);
}

static GelETree *
set_OutputStyle (GelETree * a)
{
	const char *token;
	GelOutputStyle output_style = GEL_OUTPUT_NORMAL;

	if G_UNLIKELY ( ! check_argument_string_or_identifier (&a, 0, "set_OutputStyle"))
		return NULL;

	if (a->type == GEL_STRING_NODE)
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
		gel_errorout (_("OutputStyle must be one of normal, troff, latex or mathml"));
		return NULL;
	}

	gel_calcstate.output_style = output_style;
	gel_set_state (gel_calcstate);

	return gel_makenum_string (token);
}

static GelETree *
get_OutputStyle (void)
{
	const char *token;

	token = "normal";
	if (gel_calcstate.output_style == GEL_OUTPUT_TROFF)
		token = "troff";
	else if (gel_calcstate.output_style == GEL_OUTPUT_LATEX)
		token = "latex";
	else if (gel_calcstate.output_style == GEL_OUTPUT_MATHML)
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
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (errors < 0) {
		gel_errorout (_("%s: argument should be larger or equal to 0"),
			      "MaxErrors");
		return NULL;
	}
	
	if(gel_calcstate.max_errors != errors) {
		gel_calcstate.max_errors = errors;
		gel_set_state (gel_calcstate);
	}

	return gel_makenum_ui(gel_calcstate.max_errors);
}

static GelETree *
get_MaxErrors (void)
{
	return gel_makenum_ui(gel_calcstate.max_errors);
}

static GelETree *
set_MixedFractions (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_MixedFractions"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		gel_calcstate.mixed_fractions = ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		gel_calcstate.mixed_fractions = a->bool_.bool_;
	gel_set_state (gel_calcstate);

	return gel_makenum_bool (gel_calcstate.mixed_fractions);
}
static GelETree *
get_MixedFractions (void)
{
	return gel_makenum_bool (gel_calcstate.mixed_fractions);
}

static GelETree *
set_IntegerOutputBase (GelETree * a)
{
	long base;

	if G_UNLIKELY ( ! check_argument_integer (&a, 0, "set_IntegerOutputBase"))
		return NULL;

	base = mpw_get_long(a->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
		return NULL;
	}
	if G_UNLIKELY (base < 2 || base > 36) {
		gel_errorout (_("%s: argument should be between %d and %d"),
			      "IntegerOutputBase", 2, 36);
		return NULL;
	}
	
	if(gel_calcstate.integer_output_base != base) {
		gel_calcstate.integer_output_base = base;
		gel_set_state (gel_calcstate);
	}

	return gel_makenum_ui(gel_calcstate.integer_output_base);
}

static GelETree *
get_IntegerOutputBase (void)
{
	return gel_makenum_ui(gel_calcstate.integer_output_base);
}

static GelETree *
set_IsPrimeMillerRabinReps (GelETree * a)
{
	long reps;

	if G_UNLIKELY ( ! check_argument_positive_integer (&a, 0, "set_IsPrimeMillerRabinReps"))
		return NULL;

	reps = mpw_get_long (a->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = 0;
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

int
gel_count_arguments (GelETree **a)
{
	int args;

	args = 0;
	while (a != NULL && a[args] != NULL)
		args++;

	return args;
}

/*add the routines to the dictionary*/
void
gel_funclib_addall(void)
{
	GelEFunc *f;
	GelToken *id;

	gel_new_category ("basic", N_("Basic"), TRUE /* internal */);
	gel_new_category ("parameters", N_("Parameters"), TRUE /* internal */);
	gel_new_category ("constants", N_("Constants"), TRUE /* internal */);
	gel_new_category ("numeric", N_("Numeric"), TRUE /* internal */);
	gel_new_category ("trigonometry", N_("Trigonometry"), TRUE /* internal */);
	gel_new_category ("number_theory", N_("Number Theory"), TRUE /* internal */);
	gel_new_category ("matrix", N_("Matrix Manipulation"), TRUE /* internal */);
	gel_new_category ("linear_algebra", N_("Linear Algebra"), TRUE /* internal */);
	gel_new_category ("combinatorics", N_("Combinatorics"), TRUE /* internal */);
	gel_new_category ("calculus", N_("Calculus"), TRUE /* internal */);
	gel_new_category ("functions", N_("Functions"), TRUE /* internal */);
	gel_new_category ("equation_solving", N_("Equation Solving"), TRUE /* internal */);
	gel_new_category ("statistics", N_("Statistics"), TRUE /* internal */);
	gel_new_category ("polynomial", N_("Polynomials"), TRUE /* internal */);
	gel_new_category ("sets", N_("Set Theory"), TRUE /* internal */);
	gel_new_category ("commutative_algebra", N_("Commutative Algebra"), TRUE /* internal */);
	gel_new_category ("misc", N_("Miscellaneous"), TRUE /* internal */);

	FUNC (manual, 0, "", "basic", N_("Displays the user manual"));
	FUNC (warranty, 0, "", "basic", N_("Gives the warranty information"));
	FUNC (version, 0, "", "basic", N_("Return version as a 3-vector"));
	FUNC (exit, 0, "", "basic", N_("Exits the program"));
	ALIAS (quit, 0, exit);
	FUNC (error, 1, "str", "basic", N_("Prints a string to the error stream"));
	FUNC (wait, 1, "secs", "basic", N_("Waits a specified number of seconds"));
	FUNC (true, 0, "", "basic", N_("The true boolean value"));
	ALIAS (True, 0, true);
	FUNC (false, 0, "", "basic", N_("The false boolean value"));
	ALIAS (False, 0, false);

	FUNC (CurrentTime, 0, "", "basic", N_("Unix time in seconds as a floating point number"));

	/* FIXME: TRUE, FALSE aliases can't be done with the macros in funclibhelper.cP! */
	d_addfunc (d_makebifunc (d_intern ("TRUE"), true_op, 0));
	gel_add_alias ("true", "TRUE");
	d_addfunc (d_makebifunc (d_intern ("FALSE"), false_op, 0));
	gel_add_alias ("false", "FALSE");

	FUNC (IntegerFromBoolean, 1, "bval", "basic", N_("Make integer (0 or 1) from a boolean value"));

	FUNC (print, 1, "str", "basic", N_("Prints an expression"));
	FUNC (chdir, 1, "dir", "basic", N_("Changes current directory"));
	FUNC (printn, 1, "str", "basic", N_("Prints an expression without a trailing newline"));
	FUNC (display, 2, "str,expr", "basic", N_("Display a string and an expression"));
	FUNC (set, 2, "id,val", "basic", N_("Set a global variable"));
	FUNC (SetElement, 4, "id,row,col,val", "basic", N_("Set an element in a global variable which is a matrix"));
	FUNC (SetVElement, 3, "id,elt,val", "basic", N_("Set an element in a global variable which is a vector"));
	VFUNC (DisplayVariables, 1, "var", "basic", N_("Display values of variables, or all if called without arguments"));

	FUNC (SetHelp, 3, "id,category,desc", "basic", N_("Set the category and help description line for a function"));
	FUNC (SetHelpAlias, 2, "id,alias", "basic", N_("Sets up a help alias"));

	FUNC (Identity, 1, "x", "basic", N_("Identity function, returns its argument"));

	VFUNC (rand, 1, "size", "numeric", N_("Generate random float between 0 and 1, or if size given generate vector or matrix of random floats"));
	f->no_mod_all_args = 1;
	VFUNC (randint, 2, "max,size", "numeric", N_("Generate random integer between 0 and max-1 inclusive, or if size given generate vector or matrix of random integers"));
	f->no_mod_all_args = 1;

	PARAMETER (FloatPrecision, N_("Floating point precision"));
	PARAMETER (OutputChopExponent,
		   N_("Display 0.0 when floating point number is less than 10^-x "
		      "(0=never chop)"));
	PARAMETER (OutputChopWhenExponent,
		   N_("Only chop numbers when another number is greater than 10^-x"));
	PARAMETER (MaxDigits, N_("Maximum digits to display"));
	PARAMETER (MaxErrors, N_("Maximum errors to display"));
	PARAMETER (OutputStyle, N_("Output style: normal, latex, mathml or troff"));
	PARAMETER (IntegerOutputBase, N_("Integer output base"));
	PARAMETER (MixedFractions, N_("If true, mixed fractions are printed"));
	PARAMETER (FullExpressions, N_("Print full expressions, even if more than a line"));
	PARAMETER (ResultsAsFloats, N_("Convert all results to floats before printing"));
	PARAMETER (ScientificNotation, N_("Use scientific notation"));

	PARAMETER (IsPrimeMillerRabinReps, N_("Number of extra Miller-Rabin tests to run on a number before declaring it a prime in IsPrime"));

	/* secret functions */
	d_addfunc(d_makebifunc(d_intern("ninini"),ninini_op,0));
	d_addfunc(d_makebifunc(d_intern("shrubbery"),shrubbery_op,0));

	FUNC (ExpandMatrix, 1, "M", "matrix", N_("Expands a matrix just like we do on unquoted matrix input"));
	FUNC (RowsOf, 1, "M", "matrix", N_("Gets the rows of a matrix as a vertical vector"));
	FUNC (ColumnsOf, 1, "M", "matrix", N_("Gets the columns of a matrix as a horizontal vector"));
	FUNC (DiagonalOf, 1, "M", "matrix", N_("Gets the diagonal entries of a matrix as a column vector"));
	FUNC (CountZeroColumns, 1, "M", "matrix", N_("Count the number of zero columns in a matrix"));
	FUNC (StripZeroColumns, 1, "M", "matrix", N_("Removes any all-zero columns of M"));
	FUNC (NonzeroColumns, 1, "M", "matrix", N_("Return a vector with the indices of the nonzero columns in a matrix"));
	FUNC (NonzeroElements, 1, "v", "matrix", N_("Return a vector with the indices of the nonzero elements in a vector"));

	FUNC (ComplexConjugate, 1, "M", "numeric", N_("Calculates the conjugate"));
	conj_function = f;
	ALIAS (conj, 1, ComplexConjugate);
	ALIAS (Conj, 1, ComplexConjugate);

	FUNC (sin, 1, "x", "trigonometry", N_("Calculates the sine function"));
	f->no_mod_all_args = 1;
	sin_function = f;
	FUNC (cos, 1, "x", "trigonometry", N_("Calculates the cosine function"));
	f->no_mod_all_args = 1;
	cos_function = f;
	FUNC (sinh, 1, "x", "trigonometry", N_("Calculates the hyperbolic sine function"));
	f->no_mod_all_args = 1;
	sinh_function = f;
	FUNC (cosh, 1, "x", "trigonometry", N_("Calculates the hyperbolic cosine function"));
	f->no_mod_all_args = 1;
	cosh_function = f;
	FUNC (tan, 1, "x", "trigonometry", N_("Calculates the tan function"));
	f->no_mod_all_args = 1;
	tan_function = f;
	FUNC (atan, 1, "x", "trigonometry", N_("Calculates the arctan function"));
	f->no_mod_all_args = 1;
	atan_function = f;
	ALIAS (arctan, 1, atan);

	FUNC (sinc, 1, "x", "functions", N_("Calculates the sinc function, that is sin(x)/x"));
	f->no_mod_all_args = 1;
	sinc_function = f;

	FUNC (atan2, 2, "y,x", "trigonometry", N_("Calculates the arctan2 function (arctan(y/x) if x>0)"));
	f->no_mod_all_args = 1;
	ALIAS (arctan2, 1, atan2);

	FUNC (pi, 0, "", "constants", N_("The number pi"));
	pi_function = f;
	FUNC (e, 0, "", "constants", N_("The natural number e"));
	e_function = f;
	FUNC (GoldenRatio, 0, "", "constants", N_("The Golden Ratio"));
	GoldenRatio_function = f;
	FUNC (Gravity, 0, "", "constants", N_("Free fall acceleration"));
	Gravity_function = f;
	FUNC (EulerConstant, 0, "", "constants",
	      N_("Euler's Constant gamma"));
	ALIAS (gamma, 0, EulerConstant);
	EulerConstant_function = f;
	FUNC (CatalanConstant, 0, "", "constants",
	      N_("Catalan's Constant (0.915...)"));

	FUNC (ErrorFunction, 1, "x", "functions", N_("The error function, 2/sqrt(pi) * int_0^x e^(-t^2) dt"));
	ErrorFunction_function = f;
	ALIAS (erf, 1, ErrorFunction);
	FUNC (RiemannZeta, 1, "x", "functions", N_("The Riemann zeta function (only real values implemented)"));
	f->no_mod_all_args = 1;
	RiemannZeta_function = f;
	ALIAS (zeta, 1, RiemannZeta);
	FUNC (GammaFunction, 1, "x", "functions", N_("The Gamma function (only real values implemented)"));
	f->no_mod_all_args = 1;
	GammaFunction_function = f;
	ALIAS (Gamma, 1, GammaFunction);

	FUNC (BesselJ0, 1, "x", "functions", N_("The Bessel function of first kind of order 0"));
	f->no_mod_all_args = 1;
	BesselJ0_function = f;
	FUNC (BesselJ1, 1, "x", "functions", N_("The Bessel function of first kind of order 1"));
	f->no_mod_all_args = 1;
	BesselJ1_function = f;
	FUNC (BesselJn, 2, "n,x", "functions", N_("The Bessel function of first kind of order n"));
	f->no_mod_all_args = 1;

	FUNC (BesselY0, 1, "x", "functions", N_("The Bessel function of second kind of order 0"));
	f->no_mod_all_args = 1;
	BesselJ0_function = f;
	FUNC (BesselY1, 1, "x", "functions", N_("The Bessel function of second kind of order 1"));
	f->no_mod_all_args = 1;
	BesselJ1_function = f;
	FUNC (BesselYn, 2, "n,x", "functions", N_("The Bessel function of second kind of integer order n"));
	f->no_mod_all_args = 1;

	FUNC (sqrt, 1, "x", "numeric", N_("The square root"));
	f->propagate_mod = 1;
	sqrt_function = f;
	ALIAS (SquareRoot, 1, sqrt);
	FUNC (exp, 1, "x", "numeric", N_("The exponential function"));
	f->no_mod_all_args = 1;
	exp_function = f;
	FUNC (ln, 1, "x", "numeric", N_("The natural logarithm"));
	f->no_mod_all_args = 1;
	ln_function = f;
	FUNC (log2, 1, "x", "numeric", N_("Logarithm of x base 2"));
	f->no_mod_all_args = 1;
	log2_function = f;
	ALIAS (lg, 1, log2);
	f->no_mod_all_args = 1;
	FUNC (log10, 1, "x", "numeric", N_("Logarithm of x base 10"));
	f->no_mod_all_args = 1;
	log10_function = f;
	FUNC (round, 1, "x", "numeric", N_("Round a number"));
	f->no_mod_all_args = 1;
	round_function = f;
	ALIAS (Round, 1, round);
	FUNC (floor, 1, "x", "numeric", N_("Get the highest integer less than or equal to n"));
	f->no_mod_all_args = 1;
	floor_function = f;
	ALIAS (Floor, 1, floor);
	FUNC (ceil, 1, "x", "numeric", N_("Get the lowest integer more than or equal to n"));
	f->no_mod_all_args = 1;
	ceil_function = f;
	ALIAS (Ceiling, 1, ceil);
	FUNC (trunc, 1, "x", "numeric", N_("Truncate number to an integer (return the integer part)"));
	f->no_mod_all_args = 1;
	trunc_function = f;
	ALIAS (Truncate, 1, trunc);
	ALIAS (IntegerPart, 1, trunc);
	FUNC (float, 1, "x", "numeric", N_("Make number a float"));
	f->no_mod_all_args = 1;
	float_function = f;
	FUNC (Numerator, 1, "x", "numeric", N_("Get the numerator of a rational number"));
	Numerator_function = f;
	FUNC (Denominator, 1, "x", "numeric", N_("Get the denominator of a rational number"));
	Denominator_function = f;

	VFUNC (gcd, 2, "a,args", "number_theory", N_("Greatest common divisor"));
	VALIAS (GCD, 2, gcd);
	VFUNC (lcm, 2, "a,args", "number_theory", N_("Least common multiplier"));
	VALIAS (LCM, 2, lcm);
	FUNC (IsPerfectSquare, 1, "n", "number_theory", N_("Check a number for being a perfect square"));
	FUNC (IsPerfectPower, 1, "n", "number_theory", N_("Check a number for being any perfect power (a^b)"));
	FUNC (Prime, 1, "n", "number_theory", N_("Return the nth prime (up to a limit)"));
	ALIAS (prime, 1, Prime);
	FUNC (IsEven, 1, "n", "number_theory", N_("Tests if an integer is even"));
	FUNC (IsOdd, 1, "n", "number_theory", N_("Tests if an integer is odd"));

	FUNC (NextPrime, 1, "n", "number_theory", N_("Returns the least prime greater than n (if n is positive)"));
	FUNC (LucasNumber, 1, "n", "number_theory", N_("Returns the nth Lucas number"));
	FUNC (ModInvert, 2, "n,m", "number_theory", N_("Returns inverse of n mod m"));
	FUNC (Divides, 2, "m,n", "number_theory", N_("Checks divisibility (if m divides n)"));
	FUNC (ExactDivision, 2, "n,d", "number_theory", N_("Return n/d but only if d divides n else returns garbage (this is faster than writing n/d)"));
	FUNC (IsPrime, 1, "n", "number_theory", N_("Tests primality of integers, for numbers greater than 25*10^9 false positive is with low probability depending on IsPrimeMillerRabinReps"));
	FUNC (StrongPseudoprimeTest, 2, "n,b", "number_theory", N_("Run the strong pseudoprime test base b on n"));
	FUNC (MillerRabinTest, 2, "n,reps", "number_theory", N_("Use the Miller-Rabin primality test on n, reps number of times.  The probability of false positive is (1/4)^reps"));
	FUNC (MillerRabinTestSure, 1, "n", "number_theory", N_("Use the Miller-Rabin primality test on n with enough bases that assuming the Generalized Riemann Hypothesis the result is deterministic"));
	FUNC (Factorize, 1, "n", "number_theory", N_("Return factorization of a number as a matrix"));

	VFUNC (max, 2, "a,args", "numeric", N_("Returns the maximum of arguments or matrix"));
	VALIAS (Max, 2, max);
	VALIAS (Maximum, 2, max);
	VFUNC (min, 2, "a,args", "numeric", N_("Returns the minimum of arguments or matrix"));
	VALIAS (Min, 2, min);
	VALIAS (Minimum, 2, min);

	FUNC (IntegerQuotient, 2, "a,b", "numeric", N_("Division w/o remainder, equivalent to floor(a/b)"));

	FUNC (Jacobi, 2, "a,b", "number_theory", N_("Calculate the Jacobi symbol (a/b) (b should be odd)"));
	ALIAS (JacobiSymbol, 2, Jacobi);
	FUNC (JacobiKronecker, 2, "a,b", "number_theory", N_("Calculate the Jacobi symbol (a/b) with the Kronecker extension (a/2)=(2/a) when a odd, or (a/2)=0 when a even"));
	ALIAS (JacobiKroneckerSymbol, 2, JacobiKronecker);
	FUNC (Legendre, 2, "a,p", "number_theory", N_("Calculate the Legendre symbol (a/p)"));
	ALIAS (LegendreSymbol, 2, Legendre);

	FUNC (Re, 1, "z", "numeric", N_("Get the real part of a complex number"));
	Re_function = f;
	ALIAS (RealPart, 1, Re);
	FUNC (Im, 1, "z", "numeric", N_("Get the imaginary part of a complex number"));
	Im_function = f;
	ALIAS (ImaginaryPart, 1, Im);

	FUNC (I, 1, "n", "matrix", N_("Make an identity matrix of a given size"));
	f->no_mod_all_args = 1;
	ALIAS (eye, 1, I);
	VFUNC (zeros, 2, "rows,columns", "matrix", N_("Make an matrix of all zeros (or a row vector)"));
	f->no_mod_all_args = 1;
	VFUNC (ones, 2, "rows,columns", "matrix", N_("Make an matrix of all ones (or a row vector)"));
	f->no_mod_all_args = 1;

	FUNC (AppendElement, 2, "v,e", "matrix", N_("Append an element to a vector (treating 1x1 matrix as a row vector)"));

	FUNC (rows, 1, "M", "matrix", N_("Get the number of rows of a matrix"));
	FUNC (columns, 1, "M", "matrix", N_("Get the number of columns of a matrix"));
	FUNC (IsMatrixSquare, 1, "M", "matrix", N_("Is a matrix square"));
	FUNC (IsVector, 1, "v", "matrix", N_("Is argument a horizontal or a vertical vector"));
	FUNC (IsUpperTriangular, 1, "M", "matrix", N_("Is a matrix upper triangular"));
	FUNC (IsLowerTriangular, 1, "M", "matrix", N_("Is a matrix lower triangular"));
	FUNC (IsDiagonal, 1, "M", "matrix", N_("Is a matrix diagonal"));
	FUNC (elements, 1, "M", "matrix", N_("Get the number of elements of a matrix"));

	FUNC (ref, 1, "M", "linear_algebra", N_("Get the row echelon form of a matrix"));
	f->propagate_mod = 1;
	ALIAS (REF, 1, ref);
	ALIAS (RowEchelonForm, 1, ref);
	FUNC (rref, 1, "M", "linear_algebra", N_("Get the reduced row echelon form of a matrix"));
	f->propagate_mod = 1;
	ALIAS (RREF, 1, rref);
	ALIAS (ReducedRowEchelonForm, 1, rref);
	VFUNC (SolveLinearSystem, 3, "M,V,args", "linear_algebra", N_("Solve linear system Mx=V, return solution V if there is a unique solution, null otherwise.  Extra two reference parameters can optionally be used to get the reduced M and V."));
	f->propagate_mod = 1;

	FUNC (det, 1, "M", "linear_algebra", N_("Get the determinant of a matrix"));
	ALIAS (Determinant, 1, det);

	FUNC (PivotColumns, 1, "M", "linear_algebra", N_("Return pivot columns of a matrix, that is columns which have a leading 1 in rref form, also returns the row where they occur"));

	FUNC (NullSpace, 1, "M", "linear_algebra", N_("Get the nullspace of a matrix"))

	FUNC (SetMatrixSize, 3, "M,rows,columns", "matrix", N_("Make new matrix of given size from old one"));
	FUNC (IndexComplement, 2, "vec,msize", "matrix", N_("Return the index complement of a vector of indexes"));
	FUNC (HermitianProduct, 2, "u,v", "matrix", N_("Get the Hermitian product of two vectors"));
	ALIAS (InnerProduct, 2, HermitianProduct);

	FUNC (IsValueOnly, 1, "M", "matrix", N_("Check if a matrix is a matrix of numbers"));
	FUNC (IsMatrixInteger, 1, "M", "matrix", N_("Check if a matrix is an integer (non-complex) matrix"));
	FUNC (IsMatrixRational, 1, "M", "matrix", N_("Check if a matrix is a rational (non-complex) matrix"));
	FUNC (IsMatrixReal, 1, "M", "matrix", N_("Check if a matrix is a real (non-complex) matrix"));
	FUNC (IsMatrixPositive, 1, "M", "matrix", N_("Check if a matrix is positive, that is if each element is positive"));
	FUNC (IsMatrixNonnegative, 1, "M", "matrix", N_("Check if a matrix is nonnegative, that is if each element is nonnegative"));

	FUNC (IsZero, 1, "x", "matrix", N_("Check if a number or a matrix is all zeros"));
	FUNC (IsIdentity, 1, "x", "matrix", N_("Check if a number or a matrix is 1 or identity respectively"));

	FUNC (IsIn, 2, "x,X", "sets", N_("Returns true if the element x is in the set X (where X is a vector pretending to be a set)"));
	FUNC (IsSubset, 2, "X,Y", "sets", N_("Returns true if X is a subset of Y"));
	FUNC (SetMinus, 2, "X,Y", "sets", N_("Returns a set theoretic difference X-Y (X and Y are vectors pretending to be sets)"));
	FUNC (Intersection, 2, "X,Y", "sets", N_("Returns a set theoretic intersection of X and Y (X and Y are vectors pretending to be sets)"));

	FUNC (IsNull, 1, "arg", "basic", N_("Check if argument is a null"));
	FUNC (IsValue, 1, "arg", "basic", N_("Check if argument is a number"));
	FUNC (IsBoolean, 1, "arg", "basic", N_("Check if argument is a boolean (and not a number)"));
	FUNC (IsString, 1, "arg", "basic", N_("Check if argument is a text string"));
	FUNC (IsMatrix, 1, "arg", "basic", N_("Check if argument is a matrix"));
	FUNC (IsFunction, 1, "arg", "basic", N_("Check if argument is a function"));
	FUNC (IsFunctionOrIdentifier, 1, "arg", "basic", N_("Check if argument is a function or an identifier"));
	FUNC (IsFunctionRef, 1, "arg", "basic", N_("Check if argument is a function reference"));

	FUNC (IsComplex, 1, "num", "numeric", N_("Check if argument is a complex (non-real) number"));
	FUNC (IsReal, 1, "num", "numeric", N_("Check if argument is a real number"));
	FUNC (IsInteger, 1, "num", "numeric", N_("Check if argument is an integer (non-complex)"));
	FUNC (IsPositiveInteger, 1, "num", "numeric", N_("Check if argument is a positive real integer"));
	ALIAS (IsNaturalNumber, 1, IsPositiveInteger);
	FUNC (IsNonNegativeInteger, 1, "num", "numeric", N_("Check if argument is a non-negative real integer"));
	FUNC (IsGaussInteger, 1, "num", "numeric", N_("Check if argument is a possibly complex integer"));
	ALIAS (IsComplexInteger, 1, IsGaussInteger);
	FUNC (IsRational, 1, "num", "numeric", N_("Check if argument is a rational number (non-complex)"));
	FUNC (IsComplexRational, 1, "num", "numeric", N_("Check if argument is a possibly complex rational number"));
	FUNC (IsFloat, 1, "num", "numeric", N_("Check if argument is a floating point number (non-complex)"));

	FUNC (AddPoly, 2, "p1,p2", "polynomial", N_("Add two polynomials (vectors)"));
	FUNC (SubtractPoly, 2, "p1,p2", "polynomial", N_("Subtract two polynomials (as vectors)"));
	FUNC (MultiplyPoly, 2, "p1,p2", "polynomial", N_("Multiply two polynomials (as vectors)"));
	VFUNC (DividePoly, 3, "p,q,r", "polynomial", N_("Divide polynomial p by q, return the remainder in r"));
	FUNC (PolyDerivative, 1, "p", "polynomial", N_("Take polynomial (as vector) derivative"));
	FUNC (Poly2ndDerivative, 1, "p", "polynomial", N_("Take second polynomial (as vector) derivative"));
	FUNC (TrimPoly, 1, "p", "polynomial", N_("Trim zeros from a vector pretending to be a polynomial, that is trim trailing zero elements"));
	FUNC (IsPoly, 1, "p", "polynomial", N_("Check if a vector is usable as a polynomial"));
	VFUNC (PolyToString, 2, "p,var", "polynomial", N_("Make string out of a polynomial (as vector)"));
	FUNC (PolyToFunction, 1, "p", "polynomial", N_("Make function out of a polynomial (as vector)"));

	FUNC (QuadraticFormula, 1, "p", "equation_solving", N_("Find roots of a quadratic polynomial (given as vector of coefficients)"));

	FUNC (Combinations, 2, "k,n", "combinatorics", N_("Get all combinations of k numbers from 1 to n as a vector of vectors"));
	FUNC (NextCombination, 2, "v,n", "combinatorics", N_("Get combination that would come after v in call to combinations, first combination should be [1:k]."));
	FUNC (Permutations, 2, "k,n", "combinatorics", N_("Get all permutations of k numbers from 1 to n as a vector of vectors"));

	FUNC (nCr, 2, "n,r", "combinatorics", N_("Calculate combinations (binomial coefficient)"));
	ALIAS (Binomial, 2, nCr);

	FUNC (StringToASCII, 1, "str", "misc", N_("Convert a string to a vector of ASCII values"));
	FUNC (ASCIIToString, 1, "vec", "misc", N_("Convert a vector of ASCII values to a string"));

	FUNC (StringToAlphabet, 2, "str,alphabet", "misc", N_("Convert a string to a vector of 0-based alphabet values (positions in the alphabet string), -1's for unknown letters"));
	FUNC (AlphabetToString, 2, "vec,alphabet", "misc", N_("Convert a vector of 0-based alphabet values (positions in the alphabet string) to a string"));

	FUNC (protect, 1, "id", "basic", N_("Protect a variable from being modified.  It will be treated as a system defined variable from now on.  Protected parameters can still be modified."));
	FUNC (unprotect, 1, "id", "basic", N_("Unprotect a variable from being modified.  It will be treated as a user defined variable from now on."));
	VFUNC (SetFunctionFlags, 2, "id,flags", "basic", N_("Set flags for a function, currently \"PropagateMod\" and \"NoModuloArguments\""));
	FUNC (GetCurrentModulo, 0, "", "basic", N_("Get current modulo from the context outside the function"));
	FUNC (IsDefined, 1, "id", "basic", N_("Check if a variable or function is defined"));
	FUNC (undefine, 1, "id", "basic", N_("Undefine a variable (including all locals and globals of the same name)"));
	ALIAS (Undefine, 1, undefine);
	FUNC (UndefineAll, 0, "", "basic", N_("Undefine all unprotected (user defined) global variables and parameters.  Does not reset or change protected (system) parameters."));
	FUNC (ProtectAll, 0, "", "basic", N_("Mark all currently defined variables as protected.  They will be treated as system defined variables from now on."));
	FUNC (UserVariables, 0, "", "basic", N_("Return a vector of all global unprotected (user defined) variable names."));

	FUNC (Parse, 1, "str", "basic", N_("Parse a string (but do not execute)"));
	FUNC (Evaluate, 1, "str", "basic", N_("Parse and evaluate a string"));

	VFUNC (AskString, 2, "query,...", "basic", N_("Ask a question and return a string.  Optionally pass in a default."));
	VFUNC (AskButtons, 3, "query,button1,...", "basic", N_("Ask a question and present a list of buttons.  Returns the 1-based index of the button pressed (or null on failure)."));

	FUNC (CompositeSimpsonsRule, 4, "f,a,b,n", "calculus", N_("Integration of f by Composite Simpson's Rule on the interval [a,b] with n subintervals with error of max(f'''')*h^4*(b-a)/180, note that n should be even"));
	f->no_mod_all_args = 1;

	/*temporary until well done internal functions are done*/
	/* Search also for _internal_exp_function above, it's done on
	 * demand */
#if 0
	_internal_ln_function = d_makeufunc(d_intern("<internal>ln"),
					    /*FIXME:this is not the correct 
					      function*/
					    gel_parseexp("error(\"ln not finished\")",
							 NULL, FALSE, FALSE,
							 NULL, NULL),
					    g_slist_append(NULL,d_intern("x")),1,
					    NULL);
#endif

	gel_add_symbolic_functions ();

	/*protect EVERYthing up to this point*/
	d_protect_all ();
}
