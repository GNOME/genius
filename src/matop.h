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

#ifndef _MATOP_H_
#define _MATOP_H_

#include "mpwrap.h"

/*declarations of structures*/
#include "structs.h"

#include "matrixw.h"

gboolean gel_is_matrix_value_only (GelMatrixW *m);
gboolean gel_is_matrix_value_only_real (GelMatrixW *m);
gboolean gel_is_matrix_value_only_rational (GelMatrixW *m);
gboolean gel_is_matrix_value_only_integer (GelMatrixW *m);
void gel_matrix_conjugate_transpose (GelMatrixW *m);
void gel_value_matrix_multiply (GelMatrixW *res, GelMatrixW *m1, GelMatrixW *m2, mpw_ptr modulo);
gboolean gel_value_matrix_det (GelCtx *ctx, mpw_t rop, GelMatrixW *m);
/*NOTE: if simul is passed then we assume that it's the same size as m*/
/* return FALSE if singular */
gboolean gel_value_matrix_gauss (GelCtx *ctx, GelMatrixW *m, gboolean reduce, gboolean uppertriang, gboolean stopsing, mpw_ptr detop, GelMatrixW *simul);

#endif
