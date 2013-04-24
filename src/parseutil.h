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
#ifndef _PARSEUTIL_H_
#define _PARSEUTIL_H_

#include "extra.h"

gboolean gp_push_func (gboolean vararg,
		       gboolean arguments,
		       gboolean extradict,
		       gboolean never_subst) GEL_WEAK_FUNC;
gboolean gp_prepare_push_param (gboolean setfunc) GEL_WEAK_FUNC;
gboolean gp_prepare_push_region_sep (void) GEL_WEAK_FUNC;

gboolean gp_push_local_all (void) GEL_WEAK_FUNC;
gboolean gp_push_local_idents (void) GEL_WEAK_FUNC;

void gp_convert_identifier_to_bool (void) GEL_WEAK_FUNC;

/*pops the last expression, pushes a marker
  entry and puts the last expression back*/
gboolean gp_push_marker(GelETreeType markertype) GEL_WEAK_FUNC;

/*pushes a marker*/
void gp_push_marker_simple(GelETreeType markertype) GEL_WEAK_FUNC;

/*puts a spacer into the tree, spacers are just useless nodes to be removed
  before evaluation, they just signify where there were parenthesis*/
gboolean gp_push_spacer(void) GEL_WEAK_FUNC;

/*gather all expressions up until a row start marker and push the
  result as a GEL_MATRIX_ROW_NODE*/
gboolean gp_push_matrix_row(void) GEL_WEAK_FUNC;

/*gather all expressions up until a row start marker and push the
  result as a matrix*/
gboolean gp_push_matrix(gboolean quoted) GEL_WEAK_FUNC;

/*pushes a NULL onto the stack, null cannot be evaluated, it will be
  read as ""*/
void gp_push_null(void) GEL_WEAK_FUNC;

#define SYNTAX_ERROR {yyerror("syntax error"); YYERROR;}

#define PUSH_ACT(ACT) { \
	GelETree *tree = gel_makeoperator((ACT),&gel_parsestack); \
	if(!tree) {SYNTAX_ERROR;} \
	gel_stack_push(&gel_parsestack,tree); \
}

#define PUSH_IDENTIFIER(ID) { \
	GelETree * tree; \
	GEL_GET_NEW_NODE(tree); \
	tree->type = GEL_IDENTIFIER_NODE; \
	tree->id.id = d_intern(ID); \
	tree->id.uninitialized = FALSE; \
	gel_stack_push(&gel_parsestack,tree); \
	g_free (ID); \
}

#define PUSH_CONST_STRING(ID) { \
	GelETree * tree; \
	tree = gel_makenum_string_constant (ID); \
	gel_stack_push(&gel_parsestack,tree); \
	g_free (ID); \
}

#endif
