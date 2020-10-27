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

#include <glib.h>
#include <string.h>
#include "structs.h"
#include "eval.h"
#include "calc.h"
#include "matrix.h"

#include "matrixw.h"

/* #define MATRIX_DEBUG 1 */

/* we cast matrix to this structure to stuff it onto
   the free matrix list, we could just cast it to a
   pointer but this gives the impression of being
   cleaner*/
typedef struct _GelMatrixWFreeList GelMatrixWFreeList;
struct _GelMatrixWFreeList {
	GelMatrixWFreeList *next;
};

static void internal_make_private (GelMatrixW *m, int w, int h,
				   gboolean kill_type_caches);

static GelMatrixWFreeList *free_matrices = NULL;

#define NEW_MATRIX(m) \
	if (free_matrices == NULL) { \
		m = g_new (GelMatrixW, 1); \
	} else { \
		m = (GelMatrixW *)free_matrices; \
		free_matrices = free_matrices->next; \
	}

GelETree *the_zero = NULL;

/*free a matrix*/
static void
internal_matrix_free (GelMatrix *m)
{
	int i, j;

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->use == 1) {
#ifdef MATRIX_DEBUG
		/*debug*/printf ("ACTUALLY FREE\n");
#endif
		for (j = 0; j < m->height; j++) {
			for (i = 0; i < m->width; i++) {
				GelETree *t = gel_matrix_index (m, i, j);
				if (t != NULL)
					gel_freetree (t);
			}
		}
		gel_matrix_free (m);
	} else {
		m->use--;
	}
}


static int
getmax (const int *reg, int len)
{
	int max = 0;
	int i;
	for (i = 0; i < len; i++)
		if (reg[i] > max)
			max = reg[i];
	return max;
}

/* This should be streamlined */
static gboolean
has_duplicates (const int *reg, int l)
{
	int i, ii;
	if (reg == NULL)
		return FALSE;
	for (i = 0; i < l; i++) {
		for (ii = i+1; ii < l; ii++) {
			if (reg[ii] == reg[i])
				return TRUE;
		}
	}
	return FALSE;
}

/*make new matrix*/
GelMatrixW *
gel_matrixw_new(void)
{
	GelMatrixW *m;
	NEW_MATRIX (m);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif
	
	m->m = gel_matrix_new();
	m->m->use = 1;

	/* clear caches as we're gonna mess with this matrix */
	m->cached_value_only = 0;
	m->cached_value_only_real = 0;
	m->cached_value_only_rational = 0;
	m->cached_value_only_integer = 0;
	m->cached_value_or_bool_only = 0;
	m->rref = 0;
	
	m->tr = 0;
	m->regx = NULL;
	m->regy = NULL;
	m->regw = m->m->width;
	m->regh = m->m->height;
	
	return m;
}
GelMatrixW *
gel_matrixw_new_with_matrix(GelMatrix *mat)
{
	GelMatrixW *m;
	NEW_MATRIX (m);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif
	
	m->m = mat;
	m->m->use++;

	m->cached_value_only = 0;
	m->cached_value_only_real = 0;
	m->cached_value_only_rational = 0;
	m->cached_value_only_integer = 0;
	m->cached_value_or_bool_only = 0;
	m->rref = 0;
	
	m->tr = 0;
	m->regx = NULL;
	m->regy = NULL;
	m->regw = m->m->width;
	m->regh = m->m->height;
	
	return m;
}

GelMatrixW *
gel_matrixw_new_with_matrix_value_only (GelMatrix *mat)
{
	GelMatrixW *m;
	NEW_MATRIX (m);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif
	
	m->m = mat;
	m->m->use++;

	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_real = 0;
	m->cached_value_only_rational = 0;
	m->cached_value_only_integer = 0;
	m->cached_value_or_bool_only = 0;
	m->rref = 0;
	
	m->tr = 0;
	m->regx = NULL;
	m->regy = NULL;
	m->regw = m->m->width;
	m->regh = m->m->height;
	
	return m;
}

GelMatrixW *
gel_matrixw_new_with_matrix_value_only_integer (GelMatrix *mat)
{
	GelMatrixW *m;
	NEW_MATRIX (m);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif
	
	m->m = mat;
	m->m->use++;

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
	m->rref = 0;
	
	m->tr = 0;
	m->regx = NULL;
	m->regy = NULL;
	m->regw = m->m->width;
	m->regh = m->m->height;
	
	return m;
}

GelMatrixW *
gel_matrixw_new_with_matrix_value_only_real_nonrational (GelMatrix *mat)
{
	GelMatrixW *m;
	NEW_MATRIX (m);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif
	
	m->m = mat;
	m->m->use++;

	m->cached_value_only = 1;
	m->value_only = 1;
	m->cached_value_only_real = 1;
	m->value_only_real = 1;
	m->cached_value_only_rational = 1;
	m->value_only_rational = 0;
	m->cached_value_only_integer = 1;
	m->value_only_integer = 0;
	m->cached_value_or_bool_only = 1;
	m->value_or_bool_only = 1;
	m->rref = 0;
	
	m->tr = 0;
	m->regx = NULL;
	m->regy = NULL;
	m->regw = m->m->width;
	m->regh = m->m->height;
	
	return m;
}

/* neww and newh do not actually guarantee that size,
 * also note that these sizes are after transpose
 * they just prevent useless copying*/
static void
make_us_a_copy (GelMatrixW *m, int neww, int newh)
{
	GelMatrix *old;
	int i,j;
	int w,h;
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->m->use == 1)
		return;
	
	old = m->m;
	
	m->m = gel_matrix_new();
	m->m->use = 1;
	gel_matrix_set_size (m->m, neww, newh, TRUE /* padding */);
	w = MIN (neww, m->regw);
	h = MIN (newh, m->regh);
	for(j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			int mi = m->regx ? m->regx[i] : i;
			int mj = m->regy ? m->regy[j] : j;
			GelETree *t = gel_matrix_index (old, mi, mj);
#ifdef MATRIX_DEBUG
			printf ("(%d,%d) = (%d,%d)\n", i, j,mi, mj);
#endif
			gel_matrix_index (m->m, i, j) = gel_copynode (t);
		}
	}

	g_free (m->regx);
	m->regx = NULL;
	g_free (m->regy);
	m->regy = NULL;
	
	m->regw = w;
	m->regh = h;
	old->use--;
}

/* This assumes that use > 1 and transpose has been dealt with */
static void
copy_with_region (GelMatrixW *m, int *regx, int *regy, int w, int h)
{
	GelMatrix *old;
	int i, j;
	int cw, ch;
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	g_assert (m->m->use > 1);

	old = m->m;
	
	m->m = gel_matrix_new();
	m->m->use = 1;
	gel_matrix_set_size (m->m, w, h, TRUE /* padding */);
	cw = MIN (w, m->regw);
	ch = MIN (h, m->regh);
	for(j = 0; j < ch; j++) {
		for (i = 0; i < cw; i++) {
			int mi = m->regx ? m->regx[regx[i]] : regx[i];
			int mj = m->regy ? m->regy[regy[j]] : regy[j];
			GelETree *t = gel_matrix_index (old, mi, mj);
			gel_matrix_index (m->m, i, j) = gel_copynode (t);
		}
	}

	g_free (m->regx);
	m->regx = NULL;
	g_free (m->regy);
	m->regy = NULL;
	
	m->regw = w;
	m->regh = h;
	old->use--;
}

/* This assumes that use > 1 and transpose has been dealt with */
static void
copy_internal_region (GelMatrixW *m, int w, int h)
{
	GelMatrix *old;
	int i, j;
	int cw, ch;
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	g_assert (m->m->use > 1);

	old = m->m;
	
	m->m = gel_matrix_new();
	m->m->use = 1;
	gel_matrix_set_size (m->m, w, h, TRUE /* padding */);
	cw = MIN (w, m->regw);
	ch = MIN (h, m->regh);
	for (j = 0; j < ch; j++) {
		for (i = 0; i < cw; i++) {
			int mi = m->regx ? m->regx[i] : i;
			int mj = m->regy ? m->regy[j] : j;
			GelETree *t = gel_matrix_index (old, mi, mj);
			if (t != NULL &&
			    (t->type != GEL_VALUE_NODE ||
			     ! mpw_exact_zero_p (t->val.value)))
				gel_matrix_index (m->m, i, j) = gel_copynode (t);
		}
	}

	g_free (m->regx);
	m->regx = NULL;
	g_free (m->regy);
	m->regy = NULL;
	
	m->regw = w;
	m->regh = h;
	old->use--;
}

/* This ensures the size of the matrix,
 * sizes are after transpose,
 * make sure to work with a copy,
 * note that this whacks regx and regy */
static void
ensure_at_least_size (GelMatrixW *m, int w, int h)
{
	g_assert (m->m->use == 1);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->regx == NULL && m->regy == NULL) {
		gel_matrix_set_at_least_size (m->m, w, h);
		if (w > m->regw)
			m->regw = w;
		if (h > m->regh)
			m->regh = h;
	} else {
		/* FIXME: somewhat evil, but I think this is a degenerate case,
		 * we will have regions when reffering to other matrices, not
		 * really when we own a matrix I think.  At least in casual use */
		GelMatrix *old;
		int i,j;
		int nw,nh;

		old = m->m;

		m->m = gel_matrix_new();
		m->m->use = 1;
		gel_matrix_set_size (m->m, w, h, TRUE /* padding */);
		nw = MIN (w, m->regw);
		nh = MIN (h, m->regh);
		for (j = 0; j < nh; j++) {
			for (i = 0; i < nw; i++) {
				int mi = m->regx ? m->regx[i] : i;
				int mj = m->regy ? m->regy[j] : j;
				GelETree *t = gel_matrix_index (old, mi, mj);
				if (t != NULL) {
					gel_matrix_index (m->m,i,j) = t;
					gel_matrix_index (old, mi, mj) = NULL;
				}
			}
		}
		for (j = 0; j < old->height; j++) {
			for (i = 0; i < old->width; i++) {
				GelETree *t = gel_matrix_index (old,i,j);
				if (t != NULL)
					gel_freetree (t);
			}
		}
		gel_matrix_free (old);

		g_free (m->regx);
		m->regx = NULL;
		g_free (m->regy);
		m->regy = NULL;
		m->regw = w;
		m->regh = h;
	}
}

/*set size of a matrix*/
void
gel_matrixw_set_size (GelMatrixW *m, int nwidth, int nheight)
{
	int width, height;

	g_return_if_fail (m != NULL);
	g_return_if_fail (nwidth >= 0);
	g_return_if_fail (nheight >= 0);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr) {
		width = nheight;
		height = nwidth;
	} else {
		width = nwidth;
		height = nheight;
	}

	/* we're changing things so make sure we don't consider it rref
	 * anymore */
	m->rref = 0;

	if (m->regw >= width &&
	    m->regh >= height) {
		/*if we're the sole owner, we'll have to zero out some things*/
		if (m->m->use == 1) {
			int i, j;
			/* if duplicates are found just do some
			 * copying */
			if (has_duplicates (m->regx, m->regw) ||
			    has_duplicates (m->regy, m->regh)) {
				GelMatrix *old = m->m;
#ifdef MATRIX_DEBUG
				/*debug*/printf ("HAS_DUPLICATES\n");
#endif
				/* evil */ old->use ++;
				copy_internal_region (m, width, height);
				internal_matrix_free (old);
				return;
			}
			for (j = 0; j < m->regh; j++) {
				for (i = width; i < m->regw; i++) {
					int mi = m->regx ? m->regx[i] : i;
					int mj = m->regy ? m->regy[j] : j;
					if (gel_matrix_index (m->m, mi, mj) != NULL) {
						gel_freetree(gel_matrix_index(m->m,mi,mj));
						gel_matrix_index(m->m,mi,mj)=NULL;
					}
				}
			}
			for (j = height; j < m->regh; j++) {
				for (i = 0; i < width; i++) {
					int mi = m->regx ? m->regx[i] : i;
					int mj = m->regy ? m->regy[j] : j;
					if (gel_matrix_index (m->m, mi, mj) != NULL) {
						gel_freetree(gel_matrix_index(m->m,mi,mj));
						gel_matrix_index(m->m,mi,mj)=NULL;
					}
				}
			}
		}
		m->regw = width;
		m->regh = height;
	} else if (m->m->use > 1) {
		/* since the use is greater than 1, we WILL get a copy of
		 * this matrix at the right size */
		/* it may seem we could leave caches alone, but changing size
		 * could make those values different */
		internal_make_private (m, width, height, TRUE /* kill_type_caches */);
		g_assert (m->m->use == 1);
	} else /* m->m->use == 1 */{
		ensure_at_least_size (m, width, height);
		gel_matrixw_set_size (m, nwidth, nheight);
	}
}

/*set the size of the matrix to be at least this*/
void
gel_matrixw_set_at_least_size (GelMatrixW *m, int width, int height)
{
	g_return_if_fail (m != NULL);
	g_return_if_fail (width >= 0);
	g_return_if_fail (height >= 0);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr) {
		int tmp = width;
		width = height;
		height = tmp;
	}

	if (width > m->regw || height > m->regh) {
		/* FIXME: this may be a bit inefficient */
		gel_matrixw_make_private (m, TRUE /* kill_type_caches */);
		make_us_a_copy (m, MAX (width, m->regw),MAX (height, m->regh));
		ensure_at_least_size (m, width, height);
	}
}

/*set element*/
void
gel_matrixw_set_element (GelMatrixW *m, int x, int y, gpointer data)
{
	GelETree *t;

	g_return_if_fail (m != NULL);
	g_return_if_fail (x >= 0);
	g_return_if_fail (y >= 0);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr)
		internal_make_private (m, MAX(m->regh, y+1), MAX(m->regw, x+1),
				       TRUE /* kill_type_caches */);
	else
		internal_make_private (m, MAX(m->regw, x+1), MAX(m->regh, y+1),
				       TRUE /* kill_type_caches */);
	gel_matrixw_set_at_least_size (m, x+1, y+1);
	t = gel_matrixw_get_index (m, x, y);
	if (t != NULL)
		gel_freetree (t);
	gel_matrixw_set_index (m, x, y) = data;
}

/*increment element*/
void
gel_matrixw_incr_element (GelMatrixW *m, int x, int y, mpw_ptr by)
{
	GelETree *t;

	g_return_if_fail (m != NULL);
	g_return_if_fail (x >= 0);
	g_return_if_fail (y >= 0);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr)
		internal_make_private (m, MAX(m->regw, y+1), MAX(m->regh, x+1),
				       TRUE /* kill_type_caches */);
	else
		internal_make_private (m, MAX(m->regw, x+1), MAX(m->regh, y+1),
				       TRUE /* kill_type_caches */);
	gel_matrixw_set_at_least_size (m, x+1, y+1);
	t = gel_matrixw_get_index (m, x, y);
	if (t == NULL) {
		if (by == NULL)
			gel_matrixw_set_index (m, x, y) = gel_makenum_ui (1);
		else
			gel_matrixw_set_index (m, x, y) = gel_makenum (by);
	} else if (t->type == GEL_VALUE_NODE) {
		if (by == NULL)
			mpw_add_ui (t->val.value, t->val.value, 1);
		else
			mpw_add (t->val.value, t->val.value, by);
	} else {
		gel_errorout (_("Trying to increment a nonvalue"));
	}
}

/*set vector element*/
void
gel_matrixw_set_velement (GelMatrixW *m, int i, gpointer data)
{
	GelETree *t;
	int x, y, w, h;

	g_return_if_fail (m != NULL);
	g_return_if_fail (i >= 0);

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);

	if (h == 1) {
		x = i;
		y = 0;
	} else {
		x = i % w;
		y = i / w;
	}
	if (m->tr) {
		internal_make_private (m, MAX(m->regw, y+1), MAX(m->regh, x+1),
				       TRUE /* kill_type_caches */);
	} else {
		internal_make_private (m, MAX(m->regw, x+1), MAX(m->regh, y+1),
				       TRUE /* kill_type_caches */);
	}
	gel_matrixw_set_at_least_size (m, x+1, y+1);
	t = gel_matrixw_get_index (m, x, y);
	if (t != NULL)
		gel_freetree (t);
	gel_matrixw_set_index (m, x, y) = data;
}

/*increment vector element*/
void
gel_matrixw_incr_velement (GelMatrixW *m, int i, mpw_ptr by)
{
	GelETree *t;
	int x, y, w, h;

	g_return_if_fail (m != NULL);
	g_return_if_fail (i >= 0);

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);

	if (h == 1) {
		x = i;
		y = 0;
	} else {
		x = i % w;
		y = i / w;
	}
	if (m->tr) {
		internal_make_private (m, MAX(m->regw, x+1), MAX(m->regh, y+1),
				       TRUE /* kill_type_caches */);
	} else {
		internal_make_private (m, MAX(m->regw, y+1), MAX(m->regh, x+1),
				       TRUE /* kill_type_caches */);
	}
	gel_matrixw_set_at_least_size (m, x+1, y+1);
	t = gel_matrixw_get_index (m, x, y);
	if (t == NULL) {
		if (by == NULL)
			gel_matrixw_set_index (m, x, y) = gel_makenum_ui (1);
		else
			gel_matrixw_set_index (m, x, y) = gel_makenum (by);
	} else if (t->type == GEL_VALUE_NODE) {
		if (by == NULL)
			mpw_add_ui (t->val.value, t->val.value, 1);
		else
			mpw_add (t->val.value, t->val.value, by);
	} else {
		gel_errorout (_("Trying to increment a nonvalue"));
	}
}

void
gel_matrixw_incr_region (GelMatrixW *m,
			 int *destx, int *desty,
			 int w, int h,
			 mpw_ptr by)
{
	int i, j;

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			gel_matrixw_incr_element (m, destx[i], desty[j], by);
		}
	}
}

void
gel_matrixw_incr (GelMatrixW *m, mpw_ptr by)
{
	int i, j, w, h;

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			gel_matrixw_incr_element (m, i, j, by);
		}
	}
}

void
gel_matrixw_incr_vregion (GelMatrixW *m,
			  int *desti, int len, mpw_ptr by)
{
	int i;

	for (i = 0; i < len; i++) {
		gel_matrixw_incr_velement (m, desti[i], by);
	}
}

/*make sure it's in range first!*/
GelMatrixW *
gel_matrixw_get_region (GelMatrixW *m, int *regx, int *regy, int w, int h)
{
	GelMatrixW *new;
	
	g_return_val_if_fail (m != NULL, NULL);
	g_return_val_if_fail (regx != NULL, NULL);
	g_return_val_if_fail (regy != NULL, NULL);
	g_return_val_if_fail (w >= 1, NULL);
	g_return_val_if_fail (h >= 1, NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr) {
		int t;
		int *tmp = regx;
		regx = regy;
		regy = tmp;
		t = w;
		w = h;
		h = t;
	}
	
	new = gel_matrixw_copy (m);

	/* we're changing things so make sure we don't consider it rref
	 * anymore */
	new->rref = 0;

	if (getmax (regx, w) >= new->regw ||
	    getmax (regy, h) >= new->regh) {
		/* FIXME: maybe we can whack a bit of region stuff here,
		 * instead of always copying, we sometimes don't have to
		 * I think */
		copy_with_region (new, regx, regy, w, h);
		return new;
	}

	if (new->regx == NULL) {
		new->regx = g_new (int, w);
		memcpy (new->regx, regx, sizeof(int)*w);
	} else {
		int i;
		int *nregx = g_new (int, w);
		for (i = 0; i < w; i++)
			nregx[i] = new->regx[regx[i]];
		g_free (new->regx);
		new->regx = nregx;
	}

	new->regw = w;

	if (new->regy == NULL) {
		new->regy = g_new (int, h);
		memcpy (new->regy, regy, sizeof(int)*h);
	} else {
		int i;
		int *nregy = g_new (int, h);
		for (i = 0; i < h; i++)
			nregy[i] = new->regy[regy[i]];
		g_free (new->regy);
		new->regy = nregy;
	}

	new->regh = h;

	return new;
}

/*make sure it's in range first!*/
GelMatrixW *
gel_matrixw_get_vregion (GelMatrixW *m, int *reg, int len)
{
	GelMatrix *vec;
	int i;
	
	g_return_val_if_fail (m != NULL, NULL);
	g_return_val_if_fail (reg != NULL, NULL);
	g_return_val_if_fail (len >= 1, NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	vec = gel_matrix_new ();
	gel_matrix_set_size (vec, len, 1, FALSE /* padding */);

	for (i = 0; i < len; i++) {
		GelETree *t = gel_matrixw_get_vindex (m, reg[i]);
		if (t != NULL)
			t = gel_copynode (t);
		gel_matrix_index (vec, i, 0) = t;
	}

	return gel_matrixw_new_with_matrix (vec);
}

void
gel_matrixw_set_region(GelMatrixW *m, GelMatrixW *src,
		       int *destx, int *desty,
		       int w, int h)
{
	int i, j;
	int xmax, ymax;

	g_return_if_fail (m != NULL);
	g_return_if_fail (src != NULL);
	g_return_if_fail (destx != NULL);
	g_return_if_fail (desty != NULL);
	g_return_if_fail (w >= 0);
	g_return_if_fail (h >= 0);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr) {
		int t;
		int *tmp = destx;
		destx = desty;
		desty = tmp;
		t = w;
		w = h;
		h = t;
	}
	
	xmax = getmax (destx, w);
	ymax = getmax (desty, h);

	/* FIXME: we will copy some nodes we don't need to copy
	 * as we will free them just below */
	internal_make_private (m, MAX (xmax+1, m->regw), MAX (ymax+1, m->regh),
			       TRUE /* kill_type_caches */);
	ensure_at_least_size (m, xmax+1, ymax+1);
	/* assume that's what ensure/make_us_a_copy does */
	g_assert (m->regx == NULL && m->regy == NULL);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			int si, sj;
			GelETree *t = gel_matrix_index (m->m, destx[i], desty[j]);
			if (m->tr == src->tr) {
				si = i;
				sj = j;
			} else {
				si = j;
				sj = i;
			}

			if (si >= src->regw ||
			    sj >= src->regh) {
				gel_matrix_index (m->m, destx[i], desty[j]) = NULL;
			} else {
				gel_matrix_index (m->m, destx[i], desty[j]) = gel_copynode
					(gel_matrix_index (src->m,
							   src->regx ? src->regx[si] : si,
							   src->regy ? src->regy[sj] : sj));
			}
			if (t != NULL) gel_freetree (t);
		}
	}
}

void
gel_matrixw_set_region_etree (GelMatrixW *m, GelETree *src,
			      int *destx, int *desty,
			      int w, int h)
{
	int i, j;
	int xmax, ymax;

	g_return_if_fail (m != NULL);
	g_return_if_fail (src != NULL);
	g_return_if_fail (destx != NULL);
	g_return_if_fail (desty != NULL);
	g_return_if_fail (w >= 0);
	g_return_if_fail (h >= 0);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (m->tr) {
		int t;
		int *tmp = destx;
		destx = desty;
		desty = tmp;
		t = w;
		w = h;
		h = t;
	}

	xmax = getmax (destx, w);
	ymax = getmax (desty, h);

	internal_make_private (m, MAX (xmax+1, m->regw), MAX (ymax+1, m->regh),
			       TRUE /* kill_type_caches */);
	ensure_at_least_size (m, xmax+1, ymax+1);
	/* assume that's what ensure/make_us_a_copy does */
	g_assert (m->regx == NULL && m->regy == NULL);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *t = gel_matrix_index (m->m, destx[i], desty[j]);
			gel_matrix_index (m->m, destx[i], desty[j]) = gel_copynode (src);
			if (t != NULL)
				gel_freetree (t);
		}
	}
}

/*copy a matrix*/
GelMatrixW *
gel_matrixw_copy (GelMatrixW *source)
{
	GelMatrixW *m;

	g_return_val_if_fail (source != NULL, NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	NEW_MATRIX (m);
	memcpy (m, source, sizeof (GelMatrixW));
	if (m->regx != NULL) {
		m->regx = g_new (int, m->regw);
		memcpy (m->regx, source->regx, m->regw * sizeof (int));
	}
	if (m->regy != NULL) {
		m->regy = g_new (int, m->regh);
		memcpy (m->regy, source->regy, m->regh * sizeof (int));
	}

	m->m->use++;
	return m;
}

GelMatrixW *
gel_matrixw_rowsof (GelMatrixW *source)
{
	GelMatrix *mm;
	int i, width, height;
	int *reg;

	g_return_val_if_fail (source != NULL, NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	width = gel_matrixw_width (source);
	height = gel_matrixw_height (source);

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, 1, height, FALSE /* padding */);

	reg = g_new (int, width);
	for (i = 0; i < width; i++)
		reg[i] = i;

	for (i = 0; i < height; i++) {
		GelETree *n;

		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = gel_matrixw_get_region (source, reg, &i, width, 1);

		gel_matrix_index (mm, 0, i) = n;
	}

	g_free (reg);
		
	return gel_matrixw_new_with_matrix (mm);
}

GelMatrixW *
gel_matrixw_columnsof (GelMatrixW *source)
{
	GelMatrix *mm;
	int i, width, height;
	int *reg;

	g_return_val_if_fail (source != NULL, NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	width = gel_matrixw_width (source);
	height = gel_matrixw_height (source);

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, width, 1, FALSE /* padding */);

	reg = g_new (int, height);
	for (i = 0; i < height; i++)
		reg[i] = i;

	for (i = 0; i < width; i++) {
		GelETree *n;

		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = gel_matrixw_get_region (source, &i, reg, 1, height);
		n->mat.quoted = 0;

		gel_matrix_index (mm, i, 0) = n;
	}

	g_free (reg);
		
	return gel_matrixw_new_with_matrix (mm);
}

GelMatrixW *
gel_matrixw_diagonalof (GelMatrixW *source)
{
	GelMatrix *mm;
	int i, width, height;
	int len;

	g_return_val_if_fail (source != NULL, NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	width = gel_matrixw_width (source);
	height = gel_matrixw_height (source);
	len = MIN(width,height);

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, 1, len, FALSE /* padding */);

	for (i = 0; i < len; i++) {
		GelETree *n = gel_matrixw_get_indexii (source, i);
		if (n != NULL)
			n = gel_copynode (n);

		gel_matrix_index (mm, 0, i) = n;
	}
		
	return gel_matrixw_new_with_matrix (mm);
}

/*transpose a matrix*/
GelMatrixW *
gel_matrixw_transpose(GelMatrixW *m)
{
	GelMatrixW *new;
	
	g_return_val_if_fail(m != NULL,NULL);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif
	
	new = gel_matrixw_copy(m);

	/* we're changing things so make sure we don't consider it rref
	 * anymore */
	new->rref = 0;

	new->tr = !new->tr;
	return new;
}

/*make private copy of the GelMatrix*/
static void
internal_make_private (GelMatrixW *m, int w, int h, gboolean kill_type_caches)
{
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	if (kill_type_caches) {
		/* clear caches as we're gonna mess with this matrix */
		m->cached_value_only = 0;
		m->cached_value_only_real = 0;
		m->cached_value_only_rational = 0;
		m->cached_value_only_integer = 0;
		m->cached_value_or_bool_only = 0;
	}

	m->rref = 0;

#ifdef MATRIX_DEBUG
	printf ("Make private %p %d %d\n", m, w, h);
#endif

	if (m->m->use > 1 ||
	    has_duplicates (m->regx, m->regw) ||
	    has_duplicates (m->regy, m->regh)) {
		GelMatrix *old = m->m;
#ifdef MATRIX_DEBUG
		/*debug*/printf ("HAS_DUPLICATES OR NEEDS COPY\n");
		printf ("Has duplicates/copy internal %p %d %d\n", m, w, h);
#endif
		/* evil */ old->use ++;
		copy_internal_region (m, w, h);
		internal_matrix_free (old);
		return;
	}
}

/*make private copy of the GelMatrix*/
void
gel_matrixw_make_private (GelMatrixW *m, gboolean kill_type_caches)
{
	g_return_if_fail(m != NULL);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	internal_make_private (m, m->regw, m->regh, kill_type_caches);
}

/*free a matrix*/
void
gel_matrixw_free(GelMatrixW *m)
{
	GelMatrixWFreeList *mf;
	
	g_return_if_fail(m != NULL);

#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	internal_matrix_free (m->m);

	if (m->regx != NULL)
		g_free (m->regx);
	if (m->regy != NULL)
		g_free (m->regy);
	
	mf = (GelMatrixWFreeList *)m;
	
#ifdef MEM_DEBUG_FRIENDLY

	memset (mf, 0xaa, sizeof (GelMatrixW));
	g_free (mf);
#else
	mf->next = free_matrices;
	free_matrices = mf;
#endif
}

void
gel_matrixw_set_vregion (GelMatrixW *m, GelMatrixW *src, int *desti, int len)
{
	int srcelts;
	int max;

	g_return_if_fail (m != NULL);
	g_return_if_fail (src != NULL);
	g_return_if_fail (desti != NULL);
	g_return_if_fail (len > 0);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	srcelts = src->regw * src->regh;
	max = getmax (desti, len);

	if (m->tr) {
		int i;
		if (m->regw == 1) {
			internal_make_private (m, m->regw, MAX (max+1, m->regh),
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, m->regw, max+1);
		} else {
			int minw = (max / m->regh) + 1;
			internal_make_private (m, MAX (minw, m->regw), m->regh,
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, minw, m->regh);
		}
		/* assume that's what ensure/make_us_a_copy does */
		g_assert (m->regx == NULL && m->regy == NULL);

		for (i = 0; i < len; i++) {
			int x, y;
			GelETree *t;
			if (m->regw == 1) {
				x = 0;
				y = desti[i];
			} else {
				x = desti[i] / m->regh;
				y = desti[i] % m->regh;
			}
			t = gel_matrix_index (m->m, x, y);

			if (i >= srcelts) {
				gel_matrix_index (m->m, x, y) = NULL;
			} else {
				GelETree *s = gel_matrixw_get_vindex (src, i);
				if (s != NULL)
					s = gel_copynode (s);
				gel_matrix_index (m->m, x, y) = s;
			}

			if (t != NULL)
				gel_freetree (t);
		}
	} else {
		int i;
		if (m->regh == 1) {
			internal_make_private (m, MAX (max+1, m->regw), m->regh,
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, max+1, m->regh);
		} else {
			int minh = (max / m->regw) + 1;
			internal_make_private (m, m->regw, MAX (minh, m->regh),
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, m->regw, minh);
		}
		/* assume that's what ensure/make_us_a_copy does */
		g_assert (m->regx == NULL && m->regy == NULL);

		for (i = 0; i < len; i++) {
			int x, y;
			GelETree *t;
			if (m->regh == 1) {
				x = desti[i];
				y = 0;
			} else {
				x = desti[i] % m->regw;
				y = desti[i] / m->regw;
			}
			t = gel_matrix_index (m->m, x, y);

			if (i >= srcelts) {
				gel_matrix_index (m->m, x, y) = NULL;
			} else {
				GelETree *s = gel_matrixw_get_vindex (src, i);
				if (s != NULL)
					s = gel_copynode (s);
				gel_matrix_index (m->m, x, y) = s;
			}

			if (t != NULL)
				gel_freetree (t);
		}
	}
}

void
gel_matrixw_set_vregion_etree (GelMatrixW *m, GelETree *src, int *desti, int len)
{
	int max;

	g_return_if_fail (m != NULL);
	g_return_if_fail (src != NULL);
	g_return_if_fail (desti != NULL);
	g_return_if_fail (len > 0);
#ifdef MATRIX_DEBUG
	/*debug*/printf ("%s\n", G_GNUC_PRETTY_FUNCTION);
#endif

	max = getmax (desti, len);

	if (m->tr) {
		int i;
		if (m->regw == 1) {
			internal_make_private (m, m->regw, MAX (max+1, m->regh),
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, m->regw, max+1);
		} else {
			int minw = (max / m->regh) + 1;
			internal_make_private (m, MAX (minw, m->regw), m->regh,
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, minw, m->regh);
		}
		/* assume that's what ensure/make_us_a_copy does */
		g_assert (m->regx == NULL && m->regy == NULL);

		for (i = 0; i < len; i++) {
			int x, y;
			GelETree *t;
			if (m->regw == 1) {
				x = 0;
				y = desti[i];
			} else {
				x = desti[i] / m->regh;
				y = desti[i] % m->regh;
			}
			t = gel_matrix_index (m->m, x, y);

			gel_matrix_index (m->m, x, y) = gel_copynode (src);

			if (t != NULL)
				gel_freetree (t);
		}
	} else {
		int i;
		if (m->regh == 1) {
			internal_make_private (m, MAX (max+1, m->regw), m->regh,
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, max+1, m->regh);
		} else {
			int minh = (max / m->regw) + 1;
			internal_make_private (m, m->regw, MAX (minh, m->regh),
					       TRUE /* kill_type_caches */);
			ensure_at_least_size (m, m->regw, minh);
		}
		/* assume that's what ensure/make_us_a_copy does */
		g_assert (m->regx == NULL && m->regy == NULL);

		for (i = 0; i < len; i++) {
			int x, y;
			GelETree *t;
			if (m->regh == 1) {
				x = desti[i];
				y = 0;
			} else {
				x = desti[i] % m->regw;
				y = desti[i] / m->regw;
			}
			t = gel_matrix_index (m->m, x, y);
			gel_matrix_index (m->m, x, y) = gel_copynode (src);
			if (t != NULL)
				gel_freetree (t);
		}
	}
}

GelETree *
gel_matrixw_index(GelMatrixW *m, int x, int y) {
	GelETree *t = gel_matrixw_get_index (m, x, y);
	return t?t:the_zero;
}

GelETree *
gel_matrixw_vindex(GelMatrixW *m, int i) {
	GelETree *t;
	int w = gel_matrixw_width(m);
	/* Avoid dividing things */
	if (w == 1)
		t = gel_matrixw_index (m, 0, i);
	else if (gel_matrixw_height(m) == 1)
		t = gel_matrixw_index (m, i, 0);
	else
		t = gel_matrixw_index (m, i % w, i / w);
	return t ? t : the_zero;
}

GelETree *
gel_matrixw_get_vindex(GelMatrixW *m, int i) {
	int w = gel_matrixw_width(m);
	/* Avoid dividing things */
	if (w == 1)
		return gel_matrixw_get_index (m, 0, i);
	else if (gel_matrixw_height(m) == 1)
		return gel_matrixw_get_index (m, i, 0);
	else
		return gel_matrixw_get_index (m, i % w, i / w);
}
