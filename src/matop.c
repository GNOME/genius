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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "mpwrap.h"
#include "eval.h"
#include "calc.h"
#include "dict.h"
#include "util.h"
#include "funclib.h"
#include "matrix.h"
#include "matrixw.h"

#include "matop.h"

extern calcstate_t calcstate;

gboolean
gel_is_matrix_value_only (GelMatrixW *m)
{
	int i,j;
	if (m->cached_value_only)
		return m->value_only;
	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *n = gel_matrixw_set_index(m,i,j);
			if(n && n->type != VALUE_NODE) {
				m->cached_value_only = 1;
				m->value_only = 0;
				return FALSE;
			}
		}
	}
	m->cached_value_only = 1;
	m->value_only = 1;
	return TRUE;
}

gboolean
gel_is_matrix_value_only_real (GelMatrixW *m)
{
	int i,j;
	if (m->cached_value_only_real)
		return m->value_only_real;
	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *n = gel_matrixw_set_index(m,i,j);
			if (n != NULL &&
			    (n->type != VALUE_NODE ||
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
	int i,j;
	if (m->cached_value_only_rational)
		return m->value_only_rational;
	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *n = gel_matrixw_set_index(m,i,j);
			if (n != NULL &&
			    (n->type != VALUE_NODE ||
			     mpw_is_complex (n->val.value) ||
			     mpw_is_float (n->val.value))) {
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
	int i,j;
	if (m->cached_value_only_integer)
		return m->value_only_integer;
	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *n = gel_matrixw_set_index(m,i,j);
			if (n != NULL &&
			    (n->type != VALUE_NODE ||
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
	int i,j;
	gel_matrixw_make_private (m);
	m->tr = !m->tr;
	for (i = 0; i < gel_matrixw_width (m); i++) {
		for (j = 0; j < gel_matrixw_height (m); j++) {
			GelETree *n = gel_matrixw_set_index (m, i, j);
			if (n == NULL)
				continue;
			if (n->type == VALUE_NODE) {
				if (mpw_is_complex (n->val.value))
					mpw_conj (n->val.value, n->val.value);
			} else {
				GelETree *nn;
				GET_NEW_NODE (nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_DIRECTCALL;

				GET_NEW_NODE (nn->op.args);
				nn->op.args->type = IDENTIFIER_NODE;
				nn->op.args->id.id = d_intern ("conj");

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
	int i,j,k;
	mpw_t tmp;
	mpw_init(tmp);
	gel_matrixw_make_private(res);
	for(i=0;i<gel_matrixw_width(res);i++) {
		for(j=0;j<gel_matrixw_height(res);j++) {
			mpw_t accu;
			mpw_init(accu);
			for(k=0;k<gel_matrixw_width(m1);k++) {
				GelETree *l = gel_matrixw_index(m1,k,j);
				GelETree *r = gel_matrixw_index(m2,i,k);
				mpw_mul(tmp,l->val.value,r->val.value);
				mpw_add(accu,accu,tmp);
				if (modulo != NULL) {
					mpw_mod (accu, accu, modulo);
					if (error_num != 0) { /*FIXME: for now ignore errors in moding*/
						error_num = 0;
					}
					if (mpw_sgn (accu) < 0)
						mpw_add (accu, modulo, accu);
				}
				/*XXX: are there any problems that could occur
				  here? ... I don't seem to see any, if there
				  are catch them here*/
			}
			gel_matrixw_set_index(res,i,j) = gel_makenum_use(accu);
		}
	}
	mpw_clear(tmp);
}

static void
swap_rows(GelMatrixW *m, int row, int row2)
{
	int i;
	if(row==row2) return;
	
	gel_matrixw_make_private(m);
	
	for(i=0;i<gel_matrixw_width(m);i++) {
		GelETree *t = gel_matrixw_set_index(m,i,row);
		gel_matrixw_set_index(m,i,row) = gel_matrixw_set_index(m,i,row2);
		gel_matrixw_set_index(m,i,row2) = t;
	}
}

static void
div_row(GelMatrixW *m, int row, mpw_t div)
{
	int i;
	
	if(mpw_cmp_ui(div,1)==0)
		return;

	gel_matrixw_make_private(m);
	
	for(i=0;i<gel_matrixw_width(m);i++) {
		GelETree *t = gel_matrixw_set_index(m,i,row);
		if(t)
			mpw_div(t->val.value,t->val.value,div);
	}
}

static void
mul_sub_row(GelMatrixW *m, int row, mpw_t mul, int row2)
{
	int i;
	mpw_t tmp;
	
	gel_matrixw_make_private(m);
	
	mpw_init(tmp);
	for(i=0;i<gel_matrixw_width(m);i++) {
		GelETree *t = gel_matrixw_set_index(m,i,row);
		if(t) {
			GelETree *t2 = gel_matrixw_set_index(m,i,row2);
			mpw_mul(tmp,t->val.value,mul);
			if(!t2) {
				mpw_neg(tmp,tmp);
				gel_matrixw_set_index(m,i,row2) = gel_makenum_use(tmp);
				mpw_init(tmp);
			} else {
				mpw_sub(t2->val.value,t2->val.value,tmp);
			}
		}
	}
	mpw_clear(tmp);
}

/*NOTE: if simul is passed then we assume that it's the same size as m*/
/* return FALSE if singular */
/* FIXME: if modular arithmetic is on, work over the modulo!!!! */
gboolean
gel_value_matrix_gauss(GelMatrixW *m, gboolean reduce, gboolean uppertriang, gboolean stopsing, mpw_ptr detop, GelMatrixW *simul)
{
	int i,j,d,ii;
	GelETree *piv;
	mpw_t tmp;
	
	if(detop)
		mpw_set_ui(detop,1);

	mpw_init(tmp);
	d = 0;
	for(i=0;i<gel_matrixw_width(m) && d<gel_matrixw_height(m);i++) {
		for(j=d;j<gel_matrixw_height(m);j++) {
			GelETree *t = gel_matrixw_set_index(m,i,j);
			if(t && mpw_sgn(t->val.value)!=0)
				break;
		}
		if(j==gel_matrixw_height(m)) {
			if(stopsing) {
				mpw_clear(tmp);
				return FALSE;
			}
			continue;
		} else if(j>d) {
			swap_rows(m,j,d);
			if(simul)
				swap_rows(simul,j,d);
			if(detop)
				mpw_neg(detop,detop);
		}

		gel_matrixw_make_private(m);
		piv = gel_matrixw_index(m,i,d);
			
		for(j=d+1;j<gel_matrixw_height(m);j++) {
			GelETree *t = gel_matrixw_set_index(m,i,j);
			if(t && mpw_sgn(t->val.value)!=0) {
				mpw_div(tmp,t->val.value,piv->val.value);
				mul_sub_row(m,d,tmp,j);
				if(simul)
					mul_sub_row(simul,d,tmp,j);
			}
		}


		/*we just want to do an upper triangular matrix*/
		if(uppertriang) {
			d++;
			continue;
		}

		if(reduce) {
			for(j=0;j<d;j++) {
				GelETree *t = gel_matrixw_set_index(m,i,j);
				if(t && mpw_sgn(t->val.value)!=0) {
					mpw_div(tmp,t->val.value,piv->val.value);
					mul_sub_row(m,d,tmp,j);
					if(simul)
						mul_sub_row(simul,d,tmp,j);
				}
			}
		}

		for(ii=i+1;ii<gel_matrixw_width(m);ii++) {
			GelETree *t = gel_matrixw_set_index(m,ii,d);
			if(t) {
				mpw_div(t->val.value,
					t->val.value,
					piv->val.value);
			}
		}
		if(detop)
			mpw_div(detop,detop,piv->val.value);
		if(simul)
			div_row(simul,d,piv->val.value);

		mpw_set_ui(piv->val.value,1);
		d++;
	}
	
	mpw_clear(tmp);
	return TRUE;
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
gel_value_matrix_det (mpw_t rop, GelMatrixW *m)
{
	int w = gel_matrixw_width(m);
        int h = gel_matrixw_height(m);
	GelMatrixW *mm;
	mpw_t tmp;
	int i;

	if(w != h) {
		(*errorout)(_("Determinant of a non-square matrix is undefined"));
		return FALSE;
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
		gel_value_matrix_gauss(mm,FALSE,TRUE,FALSE,tmp,NULL);
		mpw_mul(rop,tmp,gel_matrixw_index(mm,0,0)->val.value);
		mpw_clear(tmp);
		for(i=1;i<gel_matrixw_width(mm);i++) {
			GelETree *t = gel_matrixw_set_index(mm,i,i);
			if(!t) {
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
