/* GENIUS Calculator
 * Copyright (C) 1997-2005 Jiri (George) Lebl
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

#ifndef FUNCLIB_H_
#define FUNCLIB_H_

/*add the routines to the dictionary*/
void gel_funclib_addall(void);
/*remove all cached floating point values*/
void gel_break_fp_caches(void);

GelETree * gel_apply_func_to_matrix (GelCtx *ctx,
				     GelETree *mat, 
				     GelBIFunction function,
				     const char *ident, 
				     gboolean *exception);
GelETree * gel_apply_func_to_matrixen (GelCtx *ctx,
				       GelETree *mat1,
				       GelETree *mat2,
				       GelBIFunction function,
				       const char *ident,
				       gboolean *exception);

int gel_get_nonnegative_integer (mpw_ptr z, const char *funcname);

int gel_count_arguments (GelETree **a);


#endif /* FUNCLIB_H_ */
