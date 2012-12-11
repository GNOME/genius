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

#ifndef _MATRIX_H_
#define _MATRIX_H_

#include <glib.h>

typedef struct _GelMatrix GelMatrix;
struct _GelMatrix {
	int width;
	int height;

	gpointer *thedata;

	/*private data*/
	int realwidth;
	int fullsize;
	
	/*how many times has this been used, do we need
	  a copy?*/
	int use;
};
typedef gpointer (*GelElementCopyFunc)(gpointer,gpointer);

/*make new matrix*/
GelMatrix * gel_matrix_new(void);

/*set size of a matrix*/
void gel_matrix_set_size(GelMatrix *matrix, int width, int height, gboolean padding);
/*set the size of the matrix to be at least this*/
void gel_matrix_set_at_least_size(GelMatrix *matrix, int width, int height);

/*set element*/
void gel_matrix_set_element(GelMatrix *matrix, int x, int y, gpointer data);

/*copy a matrix*/
GelMatrix * gel_matrix_copy(GelMatrix *source, GelElementCopyFunc el_copy, gpointer func_data);

/*transpose a matrix*/
GelMatrix * gel_matrix_transpose(GelMatrix *matrix);

/*run a GFunc for each non-null element*/
void gel_matrix_foreach(GelMatrix *matrix, GFunc func, gpointer func_data);

/*free a matrix*/
void gel_matrix_free(GelMatrix *matrix);

/*get the value at*/
#define gel_matrix_index(m,x,y) ((m)->thedata[(x)+(y)*((m)->realwidth)])

#endif
