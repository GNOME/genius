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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "calc.h"
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "util.h"
#include "funclib.h"
#include "matrix.h"
#include "matrixw.h"

#include "matop.h"

gboolean
gel_is_matrix_value_only (GelMatrixW *m)
{
	int i, j, w, h;
	if (m->cached_value_only)
		return m->value_only;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *n = gel_matrixw_get_index(m,i,j);
			if(n && n->type != GEL_VALUE_NODE) {
				m->cached_value_only = 1;
				m->value_only = 0;
				m->cached_value_only_real = 1;
				m->value_only_real = 0;
				m->cached_value_only_rational = 1;
				m->value_only_rational = 0;
				m->cached_value_only_integer = 1;
				m->value_only_integer = 0;
				return FALSE;
			}
		}
	}
	m->cached_value_only = 1;
	m->value_only = 1;
	return TRUE;
}

gboolean
gel_is_matrix_value_or_bool_only (GelMatrixW *m)
{
	int i, j, w, h;
	gboolean got_bools = FALSE;
	if (m->cached_value_or_bool_only)
		return m->value_or_bool_only;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *n = gel_matrixw_get_index(m,i,j);
			if ( ! n)
				continue;
			if (n->type == GEL_BOOL_NODE) {
				got_bools = TRUE;
				continue;
			}

			if (n->type != GEL_VALUE_NODE) {
				m->cached_value_or_bool_only = 1;
				m->value_or_bool_only = 0;
				m->cached_value_only = 1;
				m->value_only = 0;
				m->cached_value_only_real = 1;
				m->value_only_real = 0;
				m->cached_value_only_rational = 1;
				m->value_only_rational = 0;
				m->cached_value_only_integer = 1;
				m->value_only_integer = 0;
				return FALSE;
			}
		}
	}
	m->cached_value_or_bool_only = 1;
	m->value_or_bool_only = 1;

	m->cached_value_only = 1;
	if (got_bools)
		m->value_only = 0;
	else
		m->value_only = 1;
	return TRUE;
}


gboolean
gel_is_matrix_value_only_real (GelMatrixW *m)
{
	int i, j, w, h;
	if (m->cached_value_only_real)
		return m->value_only_real;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *n = gel_matrixw_get_index(m,i,j);
			if (n != NULL &&
			    (n->type != GEL_VALUE_NODE ||
			     mpw_is_complex (n->val.value))) {
				m->cached_value_only_real = 1;
				m->value_only_real = 0;
				return FALSE;
			}
		}
	}
	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_real = 1;
	m->value_only_real = 1;
	return TRUE;
}

gboolean
gel_is_matrix_value_only_rational (GelMatrixW *m)
{
	int i, j, w, h;
	if (m->cached_value_only_rational)
		return m->value_only_rational;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *n = gel_matrixw_get_index(m,i,j);
			if (n != NULL &&
			    (n->type != GEL_VALUE_NODE ||
			     mpw_is_complex (n->val.value) ||
			     mpw_is_real_part_float (n->val.value))) {
				m->cached_value_only_rational = 1;
				m->value_only_rational = 0;
				return FALSE;
			}
		}
	}
	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_rational = 1;
	m->value_only_rational = 1;
	return TRUE;
}

gboolean
gel_is_matrix_value_only_integer (GelMatrixW *m)
{
	int i, j, w, h;
	if (m->cached_value_only_integer)
		return m->value_only_integer;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *n = gel_matrixw_get_index(m,i,j);
			if (n != NULL &&
			    (n->type != GEL_VALUE_NODE ||
			     mpw_is_complex (n->val.value) ||
			     ! mpw_is_integer (n->val.value))) {
				m->cached_value_only_integer = 1;
				m->value_only_integer = 0;
				return FALSE;
			}
		}
	}
	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_rational = 1;
	m->value_only_rational = 1;
	m->cached_value_only_integer = 1;
	m->value_only_integer = 1;
	return TRUE;
}

void
gel_matrix_conjugate_transpose (GelMatrixW *m)
{
	int i, j, w, h;

	if (gel_is_matrix_value_only_real (m)) {
		m->tr = !m->tr;
		return;
	}

	gel_matrixw_make_private (m, FALSE /* kill_type_caches */);
	m->tr = !m->tr;
	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *n = gel_matrixw_get_index (m, i, j);
			if (n == NULL)
				continue;
			if (n->type == GEL_VALUE_NODE) {
				if (mpw_is_complex (n->val.value))
					mpw_conj (n->val.value, n->val.value);
			} else {
				GelETree *nn;
				GEL_GET_NEW_NODE (nn);
				nn->type = GEL_OPERATOR_NODE;
				nn->op.oper = GEL_E_DIRECTCALL;
				nn->op.nargs = 2;

				GEL_GET_NEW_NODE (nn->op.args);
				nn->op.args->type = GEL_IDENTIFIER_NODE;
				nn->op.args->id.id = d_intern ("conj");
				nn->op.args->id.uninitialized = FALSE;

				nn->op.args->any.next = n;
				n->any.next = NULL;
				gel_matrixw_set_index (m, i, j) = nn;
			}
		}
	}
}

void
gel_value_matrix_multiply (GelMatrixW *res, GelMatrixW *m1, GelMatrixW *m2,
			   mpw_ptr modulo)
{
	int i, j, k, w, h, m1w;
	mpw_t tmp;
	mpw_init(tmp);
	gel_matrixw_make_private(res, TRUE /* kill_type_caches */);

	w = gel_matrixw_width (res);
	h = gel_matrixw_height (res);
	m1w = gel_matrixw_width (m1);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			gboolean got_something = FALSE;
			mpw_t accu;
			mpw_init(accu);
			for (k = 0; k < m1w; k++) {
				GelETree *l = gel_matrixw_get_index(m1,k,j);
				GelETree *r = gel_matrixw_get_index(m2,i,k);

				/* if both zero add nothing */
				if (l == NULL || r == NULL)
					continue;
				
				got_something = TRUE;

				mpw_mul(tmp,l->val.value,r->val.value);
				mpw_add(accu,accu,tmp);
				if (modulo != NULL) {
					mpw_mod (accu, accu, modulo);
					if G_UNLIKELY (gel_error_num != 0) { /*FIXME: for now ignore errors in moding*/
						gel_error_num = 0;
					}
					if (mpw_sgn (accu) < 0)
						mpw_add (accu, modulo, accu);
				}
				/*XXX: are there any problems that could occur
				  here? ... I don't seem to see any, if there
				  are catch them here*/
			}
			if (got_something) {
				gel_matrixw_set_index(res,i,j) = gel_makenum_use(accu);
			} else {
				gel_matrixw_set_index(res,i,j) = NULL;
				mpw_clear (accu);
			}
		}
	}
	mpw_clear(tmp);
}

/* m must be made private before */
static void
swap_rows(GelMatrixW *m, int row, int row2)
{
	int i, w;
	if(row==row2) return;
	
	/* no need for this, only used within gauss and matrix is already private
	gel_matrixw_make_private(m);*/
	
	w = gel_matrixw_width (m);
	for (i = 0; i < w; i++) {
		GelETree *t = gel_matrixw_get_index(m,i,row);
		gel_matrixw_set_index(m,i,row) = gel_matrixw_get_index(m,i,row2);
		gel_matrixw_set_index(m,i,row2) = t;
	}
}

/* m must be made private before */
static gboolean
div_row (GelCtx *ctx, GelMatrixW *m, int row, mpw_t div)
{
	int i, w;
	gboolean ret = TRUE;
	
	if (mpw_eql_ui (div, 1))
		return TRUE;

	/* no need for this, only used within gauss and matrix is already private
	gel_matrixw_make_private(m);*/
	
	w = gel_matrixw_width (m);
	for (i = 0; i < w; i++) {
		GelETree *t = gel_matrixw_get_index(m,i,row);
		if(t) {
			mpw_div(t->val.value,t->val.value,div);
			if (ctx->modulo != NULL) {
				gel_mod_node (ctx, t);
				/* can't mod so we have a singular matrix / system */
				if (t != NULL && t->type != GEL_VALUE_NODE)
					ret = FALSE;
			}
		}
	}
	return ret;
}

/* m must be made private before */
static gboolean
mul_sub_row (GelCtx *ctx, GelMatrixW *m, int row, mpw_t mul, int row2)
{
	int i, w;
	static mpw_t tmp;
	static gboolean tmp_inited = FALSE;
	gboolean ret = TRUE;
	
	/* no need for this, only used within gauss and matrix is already private
	gel_matrixw_make_private(m);*/
	
	if G_UNLIKELY ( ! tmp_inited) {
		mpw_init(tmp);
		tmp_inited = TRUE;
	}
	w = gel_matrixw_width(m);
	for (i = 0; i < w; i++) {
		GelETree *t = gel_matrixw_get_index(m,i,row);
		if (t && ! mpw_zero_p (t->val.value)) {
			GelETree *t2 = gel_matrixw_get_index(m,i,row2);
			mpw_mul(tmp,t->val.value,mul);
			if (t2 == NULL) {
				mpw_neg(tmp,tmp);
				t2 = gel_makenum_use(tmp);
				gel_matrixw_set_index(m,i,row2) = t2;
				mpw_init(tmp);
			} else if ( ! mpw_is_complex_float (tmp) &&
				   mpw_symbolic_eql (t2->val.value, tmp)) {
				gel_freetree (t2);
				gel_matrixw_set_index(m,i,row2) = NULL;
			} else {
				mpw_sub (t2->val.value,
					 t2->val.value, tmp);
			}
			if (ctx->modulo != NULL && t2 != NULL) {
				gel_mod_node (ctx, t2);
				/* can't mod so we have a singular matrix / system */
				if (t2 != NULL && t2->type != GEL_VALUE_NODE)
					ret = FALSE;
			}
		}
	}
	return ret;
}

/*NOTE: if simul is passed then we assume that it's the same size as m*/
/* return FALSE if singular */
/* FIXME: if modular arithmetic is on, work over the modulo properly!!!! */
gboolean
gel_value_matrix_gauss (GelCtx *ctx,
			GelMatrixW *m,
			gboolean reduce,
			gboolean uppertriang,
			gboolean stopsing,
			gboolean stopnonsing,
			mpw_ptr detop,
			GelMatrixW *simul)
{
	int i, j, d, ii, w, h;
	GelETree *piv;
	mpw_t tmp;
	gboolean ret = TRUE;
	gboolean made_private = FALSE;
	gboolean matrix_rational = FALSE;
	int *pivots = NULL;
	int pivots_max = -1;

	w = gel_matrixw_width (m);
        h = gel_matrixw_height (m);

	if(detop)
		mpw_set_ui(detop,1);

	if (m->rref) {
		/* test for singularity */
		if (w > h) {
			ret = FALSE;
		} else {
			GelETree *t = gel_matrixw_get_indexii(m,w-1);
			if (t == NULL ||
			    mpw_zero_p (t->val.value))
				ret = FALSE;
		}
		return ret;
	}
	
	matrix_rational = gel_is_matrix_value_only_rational (m);

	mpw_init(tmp);
	d = 0;

	if (reduce) {
		pivots = g_alloca (sizeof(int) * h);
	}

	for (i = 0; i < w && d < h; i++) {
		if (matrix_rational) {
			for (j = d; j < h; j++) {
				GelETree *t = gel_matrixw_get_index(m,i,j);
				if (t != NULL &&
				    ! mpw_zero_p (t->val.value))
					break;
			}
		} else {
			/* kind of a hack */
			int bestj = h;
			mpw_t best_abs_sq;
			GelETree *bestpiv = NULL;

			mpw_init (best_abs_sq);
			for (j = d; j < h; j++) {
				GelETree *t = gel_matrixw_get_index(m,i,j);
				if (t != NULL &&
				    ! mpw_zero_p (t->val.value)) {
					if (bestpiv == NULL) {
						bestpiv = t;
						bestj = j;
					} else {
						mpw_abs_sq (tmp, t->val.value);
						if (mpw_cmp (tmp, best_abs_sq) > 0) {
							bestpiv = t;
							bestj = j;
						}
					}
				}
			}
			mpw_clear (best_abs_sq);

			j = bestj;
		}

		if (j == h) {
			if(stopsing) {
				mpw_clear(tmp);
				return FALSE;
			}
			continue;
		}

		if ( ! made_private) {
			gel_matrixw_make_private (m, TRUE /* kill_type_caches */);
			if (simul)
				gel_matrixw_make_private (simul, TRUE /* kill_type_caches */);
			made_private = TRUE;

			/* the matrix will be value only */
			m->cached_value_only = 1;
			m->value_only = 1;

			if (matrix_rational) {
				/* the matrix will be value only rational */
				m->cached_value_only_rational = 1;
				m->value_only_rational = 1;
			}
		}
		
		if (j > d) {
			swap_rows(m,j,d);
			if(simul)
				swap_rows(simul,j,d);
			if(detop)
				mpw_neg(detop,detop);
		}

		piv = gel_matrixw_index(m,i,d);
			
		for (j = d+1; j < h; j++) {
			GelETree *t = gel_matrixw_get_index(m,i,j);
			/* Assume t is already reduced mod ctx->modulo
			 * if appropriate */
			if (t != NULL &&
			    ! mpw_zero_p (t->val.value)) {
				mpw_div(tmp,t->val.value,piv->val.value);
				if ( ! mul_sub_row (ctx, m, d, tmp, j) &&
				    stopsing) {
					mpw_clear(tmp);
					return FALSE;
				}
				if(simul) {
					if ( ! mul_sub_row (ctx, simul, d, tmp, j) &&
					     stopsing) {
						mpw_clear(tmp);
						return FALSE;
					}
				}
			}
		}


		/*we just want to do an upper triangular matrix*/
		if(uppertriang) {
			d++;
			continue;
		}

		if(reduce) {
			pivots[d] = i;
			pivots_max = d;
		}

		/* make pivot 1 */
		for (ii = i+1; ii < w; ii++) {
			GelETree *t = gel_matrixw_get_index(m,ii,d);
			if(t) {
				mpw_div(t->val.value,
					t->val.value,
					piv->val.value);
				if (ctx->modulo != NULL) {
					gel_mod_node (ctx, t);
					if (stopsing &&
					    t != NULL &&
					    t->type != GEL_VALUE_NODE) {
						mpw_clear(tmp);
						return FALSE;
					}
				}
			}
		}
		if(detop)
			mpw_div (detop, detop, piv->val.value);
		if(simul) {
			if ( ! div_row (ctx, simul, d, piv->val.value) &&
			    stopsing) {
				mpw_clear(tmp);
				return FALSE;
			}
		}
		mpw_set_ui(piv->val.value,1);
		d++;
	}

	if (d < w)
		ret = FALSE;

	if (stopnonsing && d == w) {
		mpw_clear(tmp);
		return TRUE;
	}

	if(reduce) {
		for(d = pivots_max; d >= 0; d--) {
			i = pivots[d];
			for(j=0;j<d;j++) {
				GelETree *t = gel_matrixw_get_index(m,i,j);
				if (t != NULL &&
				    ! mpw_zero_p (t->val.value)) {
					/* subtle: must copy t->val.value,
					 * else we wipe it out */
					mpw_set (tmp, t->val.value);
					if ( ! mul_sub_row (ctx, m, d, tmp, j) &&
					     stopsing) {
						mpw_clear(tmp);
						return FALSE;
					}
					if(simul) {
						if ( ! mul_sub_row (ctx, simul, d, tmp, j) &&
						     stopsing) {
							mpw_clear(tmp);
							return FALSE;
						}
					}
				}
			}
		}
	}

	if (detop && ctx->modulo != NULL) {
		/* FIXME: this may fail!!! */
		gel_mod_integer_rational (detop, ctx->modulo);
	}

	if (reduce && ! uppertriang)
		m->rref = 1;
	
	mpw_clear(tmp);
	return ret;
}


static void
det2(mpw_t rop, GelMatrixW *m)
{
	mpw_t tmp;
	mpw_init(tmp);
	mpw_mul(rop,gel_matrixw_index(m,0,0)->val.value,
		gel_matrixw_index(m,1,1)->val.value);
	mpw_mul(tmp,gel_matrixw_index(m,1,0)->val.value,
		gel_matrixw_index(m,0,1)->val.value);
	mpw_sub(rop,rop,tmp);
	mpw_clear(tmp);
}

static void
det3(mpw_t rop, GelMatrixW *m)
{
	mpw_t tmp;
	mpw_init(tmp);

	mpw_mul(rop,gel_matrixw_index(m,0,0)->val.value,
		gel_matrixw_index(m,1,1)->val.value);
	mpw_mul(rop,rop,
		gel_matrixw_index(m,2,2)->val.value);

	mpw_mul(tmp,gel_matrixw_index(m,1,0)->val.value,
		gel_matrixw_index(m,2,1)->val.value);
	mpw_mul(tmp,tmp,
		gel_matrixw_index(m,0,2)->val.value);
	mpw_add(rop,rop,tmp);

	mpw_mul(tmp,gel_matrixw_index(m,2,0)->val.value,
		gel_matrixw_index(m,0,1)->val.value);
	mpw_mul(tmp,tmp,
		gel_matrixw_index(m,1,2)->val.value);
	mpw_add(rop,rop,tmp);

	mpw_mul(tmp,gel_matrixw_index(m,2,0)->val.value,
		gel_matrixw_index(m,1,1)->val.value);
	mpw_mul(tmp,tmp,
		gel_matrixw_index(m,0,2)->val.value);
	mpw_sub(rop,rop,tmp);

	mpw_mul(tmp,gel_matrixw_index(m,1,0)->val.value,
		gel_matrixw_index(m,0,1)->val.value);
	mpw_mul(tmp,tmp,
		gel_matrixw_index(m,2,2)->val.value);
	mpw_sub(rop,rop,tmp);

	mpw_mul(tmp,gel_matrixw_index(m,0,0)->val.value,
		gel_matrixw_index(m,2,1)->val.value);
	mpw_mul(tmp,tmp,
		gel_matrixw_index(m,1,2)->val.value);
	mpw_sub(rop,rop,tmp);

	mpw_clear(tmp);
}

gboolean
gel_value_matrix_det (GelCtx *ctx, mpw_t rop, GelMatrixW *m)
{
	int w = gel_matrixw_width(m);
        int h = gel_matrixw_height(m);
	GelMatrixW *mm;
	mpw_t tmp;
	int i;

	/* only works properly without modulo, but modulo is taken
	 * care of after the det function is executed */
	g_assert (ctx->modulo == NULL);

	if(w != h) {
		gel_errorout (_("Determinant of a non-square matrix is undefined"));
		return FALSE;
	}

	/* If we already are in rref form just compute determinant */
	if (m->rref) {
		mpw_set_ui (rop, 1);
		for (i = 0; i < w; i++) {
			GelETree *t = gel_matrixw_get_indexii (m, i);
			if (t == NULL ||
			    mpw_zero_p (t->val.value)) {
				mpw_set_ui (rop, 0);
				return TRUE;
			}
			/* row reduced form has 1's on the diagonal! */
			/*mpw_mul(rop,rop,t->val.value);*/
		}
		return TRUE;
	}


	switch(w) {
	case 1:
		mpw_set(rop,gel_matrixw_index(m,0,0)->val.value);
		break;
	case 2:
		det2(rop,m);
		break;
	case 3:
		det3(rop,m);
		break;
	default:
		mpw_init(tmp);
		mm = gel_matrixw_copy(m);
		gel_value_matrix_gauss(ctx,mm,
				       FALSE /* reduce */,
				       TRUE /* uppertriang */,
				       FALSE /* stopsing */,
				       FALSE /* stopnonsing */,
				       tmp,
				       NULL);
		mpw_mul(rop,tmp,gel_matrixw_index(mm,0,0)->val.value);
		mpw_clear(tmp);
		for (i = 1; i < w; i++) {
			GelETree *t = gel_matrixw_get_indexii(mm,i);
			if (t == NULL) {
				gel_matrixw_free(mm);
				mpw_set_ui(rop,0);
				return TRUE;
			}
			mpw_mul(rop,rop,t->val.value);
		}
		gel_matrixw_free(mm);
		break;
	}
	return TRUE;
}
