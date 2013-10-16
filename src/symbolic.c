/* GENIUS Calculator
 * Copyright (C) 1997-2013 Jiri (George) Lebl
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
#include "symbolic.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "geloutput.h"
#include "compil.h"

#include "funclibhelper.cP"

static GelETree * differentiate_expr (GelETree *expr, GelToken *xtok);
static GelETree * gel_differentiate_func1_expr (GelToken *tok);

static int gel_derivative_silent = 0;


#if 0
static GelEFunc *
zero_function (void)
{
	GelEFunc *f;
	f = d_makeufunc (NULL /* id */,
			 gel_makenum_ui (0),
			 g_slist_append (NULL, d_intern ("x")),
			 1 /* nargs */,
			 NULL /* extra_dict */);
	f->context = -1;
	return f;
}

static GelEFunc *
hack_parse_function (const char *expr)
{
	GelEFunc *f;
	/* FIXME: parseexp could set gel_error_num! */
	f = d_makeufunc (NULL /* id */,
			 gel_parseexp(expr,
				      NULL, FALSE, FALSE,
				      NULL, NULL),
			 g_slist_append (NULL, d_intern ("x")),
			 1 /* nargs */,
			 NULL /* extra_dict */);
	f->context = -1;
	return f;
}
#endif

#define PARSE(expr) (gel_parseexp (expr, NULL, FALSE, FALSE, NULL, NULL))

/* Differentiate 1-var function */
static GelETree *
gel_differentiate_func1_expr (GelToken *tok)
{
	GelETree *n = NULL;

#define DERIVATIVE_ENTRY(func,expr) \
	} else if (tok == d_intern (func)) {			\
		n = PARSE (expr);

#define DERIVATIVE_ENTRY_ALIAS(func1,func2,expr) \
	} else if (tok == d_intern (func1) ||			\
		   tok == d_intern (func2)) {			\
		n = PARSE (expr);

	if (tok == NULL) {
		return NULL;
	DERIVATIVE_ENTRY ("conj", "0");

	DERIVATIVE_ENTRY ("exp", "exp(x)");
	/* Better than 1/x, since doing multiple derivatives
	   on that sucks without simplification */
	DERIVATIVE_ENTRY ("ln", "x^-1");
	DERIVATIVE_ENTRY ("log", "x^-1");
	DERIVATIVE_ENTRY ("log2", "log2(e)*x^-1");
	DERIVATIVE_ENTRY ("log10", "log10(e)*x^-1");

	/* treat z and zbar separately, we are differentiating in z */
	DERIVATIVE_ENTRY ("Re", "(1/2)");
	DERIVATIVE_ENTRY ("Im", "(-1/2i)");

	DERIVATIVE_ENTRY ("sin", "cos(x)");
	DERIVATIVE_ENTRY ("sinh", "cosh(x)");
	DERIVATIVE_ENTRY ("cos", "-sin(x)");
	DERIVATIVE_ENTRY ("cosh", "sinh(x)");
	DERIVATIVE_ENTRY ("csc", "-csc(x)*cot(x)");
	DERIVATIVE_ENTRY ("csch", "-csch(x)*coth(x)");
	DERIVATIVE_ENTRY ("sec", "sec(x)*tan(x)");
	DERIVATIVE_ENTRY ("sech", "-sech(x)*tanh(x)");
	DERIVATIVE_ENTRY ("tan", "sec(x)^2");
	DERIVATIVE_ENTRY ("tanh", "sech(x)^2");
	DERIVATIVE_ENTRY ("cot", "-csc(x)^2");
	DERIVATIVE_ENTRY ("coth", "-csch(x)^2");

	DERIVATIVE_ENTRY ("sinc", "cos(x)*x^(-1)-sinc(x)*x^(-1)");

	DERIVATIVE_ENTRY ("BesselJ0", "-BesselJ1(x)");
	DERIVATIVE_ENTRY ("BesselJ1", "(1/2)*(BesselJ0(x)-BesselJn(2,x))");
	DERIVATIVE_ENTRY ("BesselY0", "-BesselY1(x)");
	DERIVATIVE_ENTRY ("BesselY1", "(1/2)*(BesselY0(x)-BesselYn(2,x))");

	DERIVATIVE_ENTRY ("LambertW", "LambertW(x)/(x*(1+LambertW(x)))");
	DERIVATIVE_ENTRY ("LambertWm1", "LambertWm1(x)/(x*(1+LambertWm1(x)))");

	/* FIXME: check these, I don't trust the CRC handbook */
	DERIVATIVE_ENTRY_ALIAS ("asin", "arcsin", "1/sqrt(1-x^2)");
	DERIVATIVE_ENTRY_ALIAS ("asinh", "arcsinh", "1/sqrt(1+x^2)");
	DERIVATIVE_ENTRY_ALIAS ("acos", "arccos", "-1/sqrt(1-x^2)");
	DERIVATIVE_ENTRY_ALIAS ("acosh", "arccosh", "1/sqrt(x^2-1)");
	DERIVATIVE_ENTRY_ALIAS ("acsc", "arccsc", "-1/(x*sqrt(x^2-1))");
	DERIVATIVE_ENTRY_ALIAS ("acsch", "arccsch", "-1/(x*sqrt(x^2+1))");
	DERIVATIVE_ENTRY_ALIAS ("asec", "arcsec", "1/(x*sqrt(x^2-1))");
	DERIVATIVE_ENTRY_ALIAS ("asech", "arcsech", "-1/(x*sqrt(1-x^2))");
	DERIVATIVE_ENTRY_ALIAS ("atan", "arctan", "1/(1+x^2)");
	DERIVATIVE_ENTRY_ALIAS ("atanh", "arctanh", "1/(1-x^2)");
	DERIVATIVE_ENTRY_ALIAS ("acot", "arccot", "-1/(x^2+1)");
	DERIVATIVE_ENTRY_ALIAS ("acoth", "arccoth", "-1/(x^2-1)");

	DERIVATIVE_ENTRY_ALIAS ("erf", "ErrorFunction",
				"2*exp(-(x^2))/sqrt(pi)");

	DERIVATIVE_ENTRY ("cis", "-sin(x)+1i*cos(x)");

	DERIVATIVE_ENTRY_ALIAS ("sqrt", "SquareRoot", "1/(2*sqrt(x))");

#undef DERIVATIVE_ENTRY
#undef DERIVATIVE_ENTRY_ALIAS

	}

	return n;
}

static void
substitute_x_y_z_w (GelETree *expr,
		    GelToken *xtok, GelETree *x, gboolean xcopy,
		    GelToken *ytok, GelETree *y, gboolean ycopy,
		    GelToken *ztok, GelETree *z, gboolean zcopy,
		    GelToken *wtok, GelETree *w, gboolean wcopy)
{
	if (expr == NULL)
		return;

	if (expr->type == GEL_SPACER_NODE) {
		substitute_x_y_z_w (expr->sp.arg,
				    xtok, x, xcopy,
				    ytok, y, ycopy,
				    ztok, z, zcopy,
				    wtok, w, wcopy);
	} else if (expr->type == GEL_IDENTIFIER_NODE ) {
		if (xtok != NULL && x != NULL && expr->id.id == xtok) {
			gel_replacenode (expr, x, xcopy);
		} else if (ytok != NULL && y != NULL && expr->id.id == ytok) {
			gel_replacenode (expr, y, ycopy);
		} else if (ztok != NULL && z != NULL && expr->id.id == ztok) {
			gel_replacenode (expr, z, zcopy);
		} else if (wtok != NULL && w != NULL && expr->id.id == wtok) {
			gel_replacenode (expr, w, wcopy);
		}
	} else if (expr->type == GEL_OPERATOR_NODE) {
		GelETree *args = expr->op.args;
		while (args != NULL) {
			substitute_x_y_z_w (args,
					    xtok, x, xcopy,
					    ytok, y, ycopy,
					    ztok, z, zcopy,
					    wtok, w, wcopy);
			args = args->any.next;
		}
	} else if (expr->type == GEL_MATRIX_NODE &&
		   expr->mat.matrix != NULL) {
		int i, j;
		int mw, mh;
		mw = gel_matrixw_width (expr->mat.matrix);
		mh = gel_matrixw_height (expr->mat.matrix);
		for(j = 0; j < mh; j++) {
			for (i = 0; i < mw; i++) {
				GelETree *t = gel_matrixw_get_index
					(expr->mat.matrix, i, j);
				if (t != NULL)
					substitute_x_y_z_w (t,
							    xtok, x, xcopy,
							    ytok, y, ycopy,
							    ztok, z, zcopy,
							    wtok, w, wcopy);
			}
		}
	} else if (expr->type == GEL_SET_NODE ) {
		GelETree *ali;
		for (ali = expr->set.items; ali != NULL; ali = ali->any.next) {
			substitute_x_y_z_w (ali,
					    xtok, x, xcopy,
					    ytok, y, ycopy,
					    ztok, z, zcopy,
					    wtok, w, wcopy);
		}
		/* Not inside function body I don't think
	} else if (expr->type == GEL_FUNCTION_NODE &&
		   (expr->func.func->type == GEL_USER_FUNC ||
		    expr->func.func->type == GEL_VARIABLE_FUNC) &&
		   ENSURE BODY! expr->func.func->data.user != NULL) {
		substitute_x_y_z_w (expr->func.func->data.user,
					   xtok, x, xcopy,
					   ytok, y, ycopy,
					   ztok, z, zcopy,
					   wtok, w, wcopy);
					   */
	}
}

static void
substitute_x (GelETree *expr, GelToken *xtok, GelETree *x, gboolean xcopy)
{
	substitute_x_y_z_w (expr, xtok, x, xcopy,
			    NULL, NULL, FALSE,
			    NULL, NULL, FALSE,
			    NULL, NULL, FALSE);
}

static void
substitute_x_y (GelETree *expr,
		GelToken *xtok, GelETree *x, gboolean xcopy,
		GelToken *ytok, GelETree *y, gboolean ycopy)
{
	substitute_x_y_z_w (expr, xtok, x, xcopy,
			    ytok, y, ycopy,
			    NULL, NULL, FALSE,
			    NULL, NULL, FALSE);
}

/* FIXME: is this always ok?  It won't catch things like x^0 !!! */
static gboolean
is_constant (GelETree *expr, GelToken *xtok)
{
	if (expr->type == GEL_VALUE_NODE ||
	    (expr->type == GEL_IDENTIFIER_NODE &&
	     expr->id.id != xtok)) {
		return TRUE;
	}
	if (gel_eval_find_identifier (expr, xtok, FALSE /* funcbody */))
		return FALSE;
	else
		return TRUE;
}

static GelToken *
get_symbolic_id (GelToken *tok)
{
	/* FIXME: if the token is reset INSIDE the function we
	   are differentiating, then we are getting the wrong token! */
	GelEFunc *f = d_lookup_global (tok);
	if (f == NULL || f->symbolic_id == NULL)
		return tok;
	else
		return f->symbolic_id;
}

static GelETree *
differentiate_oper (GelETree *expr, GelToken *xtok)
{
	GelToken *ftok;
	GelETree *n, *nn, *nnn;

	switch (expr->op.oper) {
	case GEL_E_EXP:
	/* FIXME: case GEL_E_ELTEXP: */
		if (is_constant (expr->op.args, xtok)) {
			nnn = differentiate_expr (expr->op.args->any.next, xtok);
			if (nnn == NULL) {
				/* FIXME: */
				return NULL;
			}
			if (expr->op.args->type == GEL_IDENTIFIER_NODE &&
			    expr->op.args->id.id == d_intern ("e")) {
				n = PARSE ("(x^y)*dy");
			} else {
				n = PARSE ("(ln(x)*x^y)*dy");
			}

			substitute_x_y_z_w (n,
					    d_intern ("x"), expr->op.args, TRUE,
					    d_intern ("y"), expr->op.args->any.next, TRUE,
					    d_intern ("dy"), nnn, FALSE,
					    NULL, NULL, FALSE);
			return n;
		} else if (is_constant (expr->op.args->any.next, xtok)) {
			nn = differentiate_expr (expr->op.args, xtok);
			if (nn == NULL) {
				/* FIXME: */
				return NULL;
			}
			if (expr->op.args->any.next->type == GEL_VALUE_NODE) {
				GelETree *ymo = NULL;
				mpw_t val;
				if (mpw_zero_p (expr->op.args->any.next->val.value)) {
					gel_freetree (nn);
					return gel_makenum_ui (0);
				}
				mpw_init (val);
				mpw_sub_ui (val,
					    expr->op.args->any.next->val.value, 1);
				if (nn->type == GEL_VALUE_NODE &&
				    mpw_eql_ui (nn->val.value, 1)) {
					if (mpw_zero_p (val)) {
						n = gel_makenum_ui (1);
					} else if (mpw_eql_ui (val, 1)) {
						n = gel_copynode (PARSE ("2*x"));
					} else {
						n = PARSE ("y*x^ymo");
						ymo = gel_makenum (val);
					}
					gel_freetree (nn);
					nn = NULL;
				} else {
					if (mpw_zero_p (val)) {
						n = gel_makenum_ui (1);
					} else if (mpw_eql_ui (val, 1)) {
						n = PARSE ("(2*x)*dx");
					} else {
						n = PARSE ("(y*x^ymo)*dx");
						ymo = gel_makenum (val);
					}
				}
				substitute_x_y_z_w (n,
						    d_intern ("x"), expr->op.args, TRUE,
						    d_intern ("y"), expr->op.args->any.next, TRUE,
						    d_intern ("dx"), nn, FALSE,
						    d_intern ("ymo"), ymo, FALSE);
				return n;
			}
			if (nn->type == GEL_VALUE_NODE &&
			    mpw_eql_ui (nn->val.value, 1)) {
				n = PARSE ("y*x^(y-1)");
				gel_freetree (nn);
				nn = NULL;
			} else {
				n = PARSE ("(y*x^(y-1))*dx");
			}

			substitute_x_y_z_w (n,
					    d_intern ("x"), expr->op.args, TRUE,
					    d_intern ("y"), expr->op.args->any.next, TRUE,
					    d_intern ("dx"), nn, FALSE,
					    NULL, NULL, FALSE);
			return n;
		}
		nn = differentiate_expr (expr->op.args, xtok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		nnn = differentiate_expr (expr->op.args->any.next, xtok);
		if (nnn == NULL) {
			gel_freetree (nn);
			/* FIXME: */
			return NULL;
		}
		n = PARSE ("(x*y^(y-1))*dx + (x^y)*ln(x)*dy");
		substitute_x_y_z_w (n,
				    d_intern ("x"), expr->op.args, TRUE,
				    d_intern ("y"), expr->op.args->any.next, TRUE,
				    d_intern ("dx"), nn, FALSE,
				    d_intern ("dy"), nnn, FALSE);
		return n;

	case GEL_E_ABS:
		if (is_constant (expr->op.args, xtok))
			return gel_makenum_ui (0);
		nn = differentiate_expr (expr->op.args, xtok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		n = PARSE ("sign(x)*y");
		substitute_x_y (n,
				d_intern ("x"), expr->op.args, TRUE,
				d_intern ("y"), nn, FALSE);
		return n;
	case GEL_E_PLUS:
	/* FIXME: case GEL_E_ELTPLUS: */
	case GEL_E_MINUS:
	/* FIXME: case GEL_E_ELTMINUS: */
		nn = differentiate_expr (expr->op.args, xtok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		nnn = differentiate_expr (expr->op.args->any.next, xtok);
		if (nnn == NULL) {
			gel_freetree (nn);
			/* FIXME: */
			return NULL;
		}
		GEL_GET_NEW_NODE(n);
		n->type = GEL_OPERATOR_NODE;
		n->op.oper = expr->op.oper;
		n->op.args = nn;
		n->op.args->any.next = nnn;
		n->op.args->any.next->any.next = NULL;
		n->op.nargs = 2;
		return n;
	case GEL_E_MUL:
	/* FIXME: case GEL_E_ELTMUL: */
		if (is_constant (expr->op.args, xtok)) {
			nnn = differentiate_expr (expr->op.args->any.next, xtok);
			if (nnn == NULL) {
				/* FIXME: */
				return NULL;
			}
			GEL_GET_NEW_NODE(n);
			n->type = GEL_OPERATOR_NODE;
			n->op.oper = expr->op.oper;
			n->op.args = gel_copynode (expr->op.args);
			n->op.args->any.next = nnn;
			n->op.args->any.next->any.next = NULL;
			n->op.nargs = 2;
			return n;
		} else if (is_constant (expr->op.args->any.next, xtok)) {
			nn = differentiate_expr (expr->op.args, xtok);
			if (nn == NULL) {
				/* FIXME: */
				return NULL;
			}
			GEL_GET_NEW_NODE(n);
			n->type = GEL_OPERATOR_NODE;
			n->op.oper = expr->op.oper;
			n->op.args = nn;
			n->op.args->any.next = gel_copynode (expr->op.args->any.next);
			n->op.args->any.next->any.next = NULL;
			n->op.nargs = 2;
			return n;
		}
		nn = differentiate_expr (expr->op.args, xtok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		nnn = differentiate_expr (expr->op.args->any.next, xtok);
		if (nnn == NULL) {
			gel_freetree (nn);
			/* FIXME: */
			return NULL;
		}
		n = PARSE ("x*dy+dx*y");
		substitute_x_y_z_w (n,
				    d_intern ("x"), expr->op.args, TRUE,
				    d_intern ("y"), expr->op.args->any.next, TRUE,
				    d_intern ("dx"), nn, FALSE,
				    d_intern ("dy"), nnn, FALSE);
		return n;
	case GEL_E_DIV:
	/* FIXME: case GEL_E_ELTDIV: */
		if (is_constant (expr->op.args->any.next, xtok)) {
			nn = differentiate_expr (expr->op.args, xtok);
			if (nn == NULL) {
				/* FIXME: */
				return NULL;
			}
			GEL_GET_NEW_NODE(n);
			n->type = GEL_OPERATOR_NODE;
			n->op.oper = expr->op.oper;
			n->op.args = nn;
			n->op.args->any.next = gel_copynode (expr->op.args->any.next);
			n->op.args->any.next->any.next = NULL;
			n->op.nargs = 2;
			return n;
		}
		nn = differentiate_expr (expr->op.args, xtok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		nnn = differentiate_expr (expr->op.args->any.next, xtok);
		if (nnn == NULL) {
			gel_freetree (nn);
			/* FIXME: */
			return NULL;
		}
		/* FIXME: is this better idea then is_constant? */
		if (nn->type == GEL_VALUE_NODE &&
		    mpw_zero_p (nn->val.value)) {
			gel_freetree (nn);
			nn = NULL;
			n = PARSE ("(-x*dy)/(y^2)");
		} else {
			n = PARSE ("(y*dx-x*dy)/(y^2)");
		}
		substitute_x_y_z_w (n,
				    d_intern ("x"), expr->op.args, TRUE,
				    d_intern ("y"), expr->op.args->any.next, TRUE,
				    d_intern ("dx"), nn, FALSE,
				    d_intern ("dy"), nnn, FALSE);
		return n;
	/* FIXME: case GEL_E_BACK_DIV: */
	/* FIXME: case GEL_E_ELT_BACK_DIV: */
	case GEL_E_NEG:
		nn = differentiate_expr (expr->op.args, xtok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		GEL_GET_NEW_NODE(n);
		n->type = GEL_OPERATOR_NODE;
		n->op.oper = expr->op.oper;
		n->op.args = nn;
		n->op.args->any.next = NULL;
		n->op.nargs = 1;
		return n;

	case GEL_E_CALL:
	case GEL_E_DIRECTCALL:
		if (expr->op.args->type != GEL_IDENTIFIER_NODE &&
		    expr->op.args->type != GEL_FUNCTION_NODE) {
			/* FIXME: */
			return NULL;
		}
		/* FIXME: What about multiple argument functions!!!! */
		if (expr->op.nargs != 2) {
			/* FIXME: */
			return NULL;
		}
		if (is_constant (expr->op.args->any.next, xtok)) {
			return gel_makenum_ui (0);
		}
		if (expr->op.args->type == GEL_IDENTIFIER_NODE) {
			ftok = get_symbolic_id (expr->op.args->id.id);
		} else /* if (expr->op.args->type == GEL_FUNCTION_NODE) */ {
			GelEFunc *f = expr->op.args->func.func;
			if (f->symbolic_id != NULL)
				ftok = f->symbolic_id;
			else
				ftok = f->id;
			if (ftok == NULL) {
				/* FIXME: */
				return NULL;
			}
		}
		nn = gel_differentiate_func1_expr (ftok);
		if (nn == NULL) {
			/* FIXME: */
			return NULL;
		}
		nnn = differentiate_expr (expr->op.args->any.next, xtok);
		if (nnn == NULL) {
			/* FIXME: */
			gel_freetree (nn);
			return NULL;
		}
		substitute_x (nn, d_intern ("x"), expr->op.args->any.next, TRUE);

		if (nnn->type == GEL_VALUE_NODE &&
		    mpw_eql_ui (nnn->val.value, 1)) {
			gel_freetree (nnn);
			return nn;
		} else if (nnn->type == GEL_VALUE_NODE &&
			   mpw_zero_p (nnn->val.value)) {
			gel_freetree (nnn);
			gel_freetree (nn);
			return gel_makenum_ui (0);
		}

		GEL_GET_NEW_NODE (n);
		n->type = GEL_OPERATOR_NODE;
		n->op.oper = GEL_E_MUL;
		n->op.args = nn;
		n->op.args->any.next = nnn;
		n->op.args->any.next->any.next = NULL;
		n->op.nargs = 2;
		return n;

	/* FIXME: could do sums and products term by term */

	default:
		/* FIXME: complain about this more finegraindly? */
		return NULL;
	}

	return NULL;
}

static GelETree *
differentiate_expr (GelETree *expr, GelToken *xtok)
{
	if (gel_evalnode_hook != NULL) {
		static int i = 0;
		if G_UNLIKELY ((i++ & GEL_RUN_HOOK_EVERY_MASK) == GEL_RUN_HOOK_EVERY_MASK) {
			(*gel_evalnode_hook)();
			i = 0;
			if G_UNLIKELY (gel_interrupted)
				return NULL;
		}
	}

	if (expr == NULL) {
		return NULL;
	} else if (expr->type == GEL_VALUE_NODE) {
		return gel_makenum_ui (0);
	} else if (expr->type == GEL_IDENTIFIER_NODE) {
		if (expr->id.id == xtok)
			return gel_makenum_ui (1);
		else
			return gel_makenum_ui (0);
	} else if (expr->type == GEL_OPERATOR_NODE) {
		return differentiate_oper (expr, xtok);
	} else {
		/* FIXME: complain about this more finegraindly? */
		return NULL;
	}
}

static GelEFunc *
gel_differentiate_func1_by_name (GelEFunc *f)
{
	GelEFunc *rf = NULL;
	GelETree *tree;

	if (f->id == NULL)
		return NULL;

	if (f->symbolic_id != NULL)
		tree = gel_differentiate_func1_expr (f->symbolic_id);
	else
		tree = gel_differentiate_func1_expr (f->id);

	if (tree != NULL) {
		rf = d_makeufunc (NULL /* id */,
				  tree,
				  g_slist_append (NULL, d_intern ("x")),
				  1 /* nargs */,
				  NULL /* extra_dict */);
		rf->context = -1;
		return rf;
	} else {
		return NULL;
	}
}

static GelEFunc *
gel_differentiate_func (GelEFunc *f)
{
	GelEFunc *rf;

	if G_UNLIKELY (f == NULL ||
		       f->nargs != 1) {
		gel_errorout (_("%s: '%s' not a function of one variable"),
			      "SymbolicDerivative",
			      f->id ? f->id->token : "anonymous");
		return NULL;
	}

	rf = gel_differentiate_func1_by_name (f);

	/* FIXME:
	   What if the function has a different name, this is pretty
	   bad SymbolicDerivative won't work inside other functions.
	   Note: Not just builtin functions */

	if (rf == NULL && f->type == GEL_USER_FUNC) {
		GelETree *n;
		GelToken *xtok;
		D_ENSURE_USER_BODY (f);
		xtok = f->named_args->data;
		n = differentiate_expr (f->data.user, xtok);
		if (n != NULL) {
			gel_simplify (n);
			rf = d_makeufunc (NULL /* id */,
					  n,
					  g_slist_append (NULL, xtok),
					  1 /* nargs */,
					  NULL /* extra_dict */);
			rf->context = -1;
		}
	}

	if (rf == NULL &&
	    gel_derivative_silent <= 0 &&
	    ! gel_interrupted) {
		gel_errorout (_("%s: Cannot differentiate the '%s' function"),
			      "SymbolicDerivative",
			      f->id ? f->id->token : "anonymous");
	}

	return rf;
}

static GelETree *
SymbolicDerivative_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelEFunc *f, *rf;
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_function_or_identifier (a, 0, "SymbolicDerivative"))
		return NULL;

	if (a[0]->type == GEL_FUNCTION_NODE) {
		f = a[0]->func.func;
	} else /* (a[0]->type == GEL_IDENTIFIER_NODE) */ {
		f = d_lookup_global (a[0]->id.id);
	}

	if G_UNLIKELY (f == NULL ||
		       f->nargs != 1) {
		gel_errorout (_("%s: argument not a function of one variable"),
			      "SymbolicDerivative");
		return NULL;
	}

	rf = gel_differentiate_func (f);
	if (rf != NULL) {
		GEL_GET_NEW_NODE (n);
		n->type = GEL_FUNCTION_NODE;
		n->func.func = rf;
		return n;
	} else {
		return NULL;
	}
}

static GelETree *
SymbolicDerivativeTry_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelETree *n;

	/* THREAD FIXME: not thread friendly */
	gel_derivative_silent ++;
	n = SymbolicDerivative_op (ctx, a, exception);
	gel_derivative_silent --;

	if (n == NULL)
		return gel_makenum_null ();
	else
		return n;
}

#if 0
static GelETree *
SimplifyFunction_op(GelCtx *ctx, GelETree * * a, gboolean *exception)
{
	GelEFunc *f, *rf;
	GelETree *n;

	if G_UNLIKELY ( ! check_argument_function_or_identifier (a, 0, "SimplifyFunction"))
		return NULL;

	if (a[0]->type == GEL_FUNCTION_NODE) {
		f = a[0]->func.func;
	} else /* (a[0]->type == GEL_IDENTIFIER_NODE) */ {
		f = d_lookup_global (a[0]->id.id);
	}

	/* FIXME: call gel_simplify on body of f */

}
#endif


/*add the routines to the dictionary*/
void
gel_add_symbolic_functions (void)
{
	GelEFunc *f;

	gel_new_category ("symbolic", N_("Symbolic Operations"), TRUE /* internal */);

	FUNC (SymbolicDerivative, 1, "f", "symbolic",
	      N_("Attempt to symbolically differentiate the function f, "
		 "where f is a function of one variable."));
	FUNC (SymbolicDerivativeTry, 1, "f", "symbolic",
	      N_("Attempt to symbolically differentiate the function f, "
		 "where f is a function of one variable, returns null if "
		 "unsuccessful but is silent."));
	/*
	FUNC (SimplifyFunction, 1, "f", "symbolic",
	      N_("Attempt to simplify the body of the function.  Very rudimentary at the moment"));
	      */
}
