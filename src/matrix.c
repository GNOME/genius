/* GENIUS Calculator
 * Copyright (C) 1997-2011 Jiri (George) Lebl
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
#include "matrix.h"
#include "eval.h"
#include <string.h>

/* we cast matrix to this structure to stuff it onto
   the free matrix list, we could just cast it to a
   pointer but this gives the impression of being
   cleaner*/
typedef struct _GelMatrixFreeList GelMatrixFreeList;
struct _GelMatrixFreeList {
	GelMatrixFreeList *next;
};

#ifndef MEM_DEBUG_FRIENDLY
static GelMatrixFreeList *free_matrices = NULL;
#endif

/*make new matrix*/
GelMatrix *
gel_matrix_new(void)
{
	GelMatrix *m;
#ifdef MEM_DEBUG_FRIENDLY
	m = g_new0(GelMatrix,1);
#else
	if(!free_matrices)
		m = g_new(GelMatrix,1);
	else {
		m = (GelMatrix *)free_matrices;
		free_matrices = free_matrices->next;
	}
#endif
	
	m->width = 0;
	m->height = 0;
	
	m->thedata = NULL;
	
	m->realwidth = 0;
	m->fullsize = 0;
	
	m->use = 0;
	
	return m;
}

/*set size of a matrix*/
void
gel_matrix_set_size (GelMatrix *matrix, int width, int height, gboolean padding)
{
	gpointer *na;
	int i;
	int wpadding;
	int hpadding;

	g_return_if_fail(matrix != NULL);
	g_return_if_fail(width>0);
	g_return_if_fail(height>0);

	if (! padding) {
		wpadding = 0;
		hpadding = 0;
	} else {
		wpadding = width >> 4;
		hpadding = height >> 4;
		if(wpadding > 10) wpadding = 10;
		if(hpadding > 10) hpadding = 10;
	}
	
	if (matrix->thedata == NULL) {
		matrix->width = width;
		matrix->realwidth = width+wpadding;
		matrix->height = height;
		matrix->fullsize=(width+wpadding)*(height+hpadding);

		matrix->thedata = g_new0 (gpointer, matrix->fullsize);
		return;
	}
	
	if (width <= matrix->realwidth) {
		int newsize = matrix->realwidth*height;
		if (newsize > matrix->fullsize) {
			matrix->thedata = g_renew (gpointer, matrix->thedata, newsize);
			memset (matrix->thedata + matrix->fullsize, 0, (newsize - matrix->fullsize) * sizeof(void *));
			matrix->fullsize = newsize;
		}
		if (width < matrix->width) {
			for(i=0;i<matrix->height;i++)
				memset(matrix->thedata+((matrix->realwidth*i)+width),0,(matrix->width-width)*sizeof(void *));
		}
		if (height < matrix->height) {
			memset(matrix->thedata+(matrix->realwidth*height),0,
			       ((matrix->realwidth*matrix->height)-(matrix->realwidth*height))*sizeof(void *));
		}
		matrix->width = width;
		matrix->height = height;
		return;
	}

	matrix->fullsize = (width+wpadding)*(height+hpadding);
	na = g_new0 (gpointer, matrix->fullsize);
	
	for(i=0;i<matrix->height;i++) {
		memcpy(na+((width+wpadding)*i),
		       matrix->thedata+(matrix->realwidth*i),
		       matrix->width*sizeof(void *));
	}
	
	matrix->realwidth = width+wpadding;
	matrix->width = width;
	matrix->height = height;

	g_free (matrix->thedata);
	
	matrix->thedata = na;
}

/*set the size of the matrix to be at least this*/
void
gel_matrix_set_at_least_size(GelMatrix *matrix, int width, int height)
{
	g_return_if_fail(matrix != NULL);
	g_return_if_fail(width>=0);
	g_return_if_fail(height>=0);
	
	if(width>matrix->width || height>matrix->height)
		gel_matrix_set_size (matrix,
				     MAX(width,matrix->width),
				     MAX(height,matrix->height),
				     TRUE /* padding */);
}

/*set element*/
void
gel_matrix_set_element(GelMatrix *matrix, int x, int y, gpointer data)
{
	g_return_if_fail(matrix != NULL);
	g_return_if_fail(x>=0);
	g_return_if_fail(y>=0);
	
	if(x>=matrix->width || y>=matrix->height)
		gel_matrix_set_size (matrix,
				     MAX(x+1,matrix->width),
				     MAX(y+1,matrix->height),
				     TRUE /* padding */);
	g_return_if_fail(matrix->thedata!=NULL);
	
	matrix->thedata[x+y*matrix->realwidth]=data;
}

/*copy a matrix*/
GelMatrix *
gel_matrix_copy(GelMatrix *source, GelElementCopyFunc el_copy, gpointer func_data)
{
	GelMatrix *matrix;
	int i,j;

	g_return_val_if_fail(source != NULL,NULL);
	
	matrix = gel_matrix_new();
	
	/*copy over the structure*/
	*matrix = *source;
	
	matrix->thedata = NULL;
	
	if(source->thedata==NULL)
		return matrix;

	/*make us a new matrix data array*/
	gel_matrix_set_size (matrix, source->width,source->height, TRUE /* padding */);
	
	/*copy the data*/
	if(el_copy) {
		for(j=0;j<source->height;j++) {
			for(i=0;i<source->width;i++) {
				gpointer data = gel_matrix_index(source,i,j);
				if(data)
					gel_matrix_index(matrix,i,j) =
						(*el_copy)(data, func_data);
			}
		}
	} else {
		for(j=0;j<source->height;j++)
			for(i=0;i<source->width;i++)
				gel_matrix_index(matrix,i,j) =
					gel_matrix_index(source,i,j);
	}
	return matrix;
}

/*transpose a matrix*/
GelMatrix *
gel_matrix_transpose(GelMatrix *matrix)
{
	int i,j;
	GelMatrix *new;

	g_return_val_if_fail(matrix != NULL,NULL);
	
	new = gel_matrix_new();

	if (matrix->thedata == NULL)
		return new;

	gel_matrix_set_size (new, matrix->height, matrix->width, TRUE /* padding */);
	
	for(i=0;i<matrix->width;i++)
		for(j=0;j<matrix->height;j++)
			gel_matrix_index(new,j,i) = gel_matrix_index(matrix,i,j);

	return new;
}

/*run a GFunc for each non-null element*/
void
gel_matrix_foreach(GelMatrix *matrix, GFunc func, gpointer func_data)
{
	int i,j;

	g_return_if_fail(matrix != NULL);
	g_return_if_fail(func != NULL);
	
	if (matrix->thedata == NULL)
		return;

	for(j=0;j<matrix->height;j++) {
		for(i=0;i<matrix->width;i++) {
			gpointer data = gel_matrix_index(matrix,i,j);
			if(data)
				(*func)(data,func_data);
		}
	}
}

/*free a matrix*/
void
gel_matrix_free(GelMatrix *matrix)
{
	GelMatrixFreeList *mf;
	
	g_return_if_fail(matrix != NULL);
	
	mf = (GelMatrixFreeList *)matrix;

	if (matrix->thedata != NULL) {
		g_free (matrix->thedata);
		matrix->thedata = NULL;
	}
#ifdef MEM_DEBUG_FRIENDLY
	memset (matrix, 0xaa, sizeof (GelMatrix));
	g_free (matrix);
#else
	mf->next = free_matrices;
	free_matrices = mf;
#endif
}
