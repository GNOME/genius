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
#ifndef _PARSEUTIL_H_
#define _PARSEUTIL_H_

#include "extra.h"

gboolean gp_push_func (gboolean vararg) GEL_WEAK_FUNC;
gboolean gp_prepare_push_param (gboolean setfunc) GEL_WEAK_FUNC;
gboolean gp_prepare_push_region_sep (void) GEL_WEAK_FUNC;

/*pops the last expression, pushes a marker
  entry and puts the last expression back*/
int gp_push_marker(GelETreeType markertype) GEL_WEAK_FUNC;

/*pushes a marker*/
void gp_push_marker_simple(GelETreeType markertype) GEL_WEAK_FUNC;

/*puts a spacer into the tree, spacers are just useless nodes to be removed
  before evaluation, they just signify where there were parenthesis*/
gboolean gp_push_spacer(void) GEL_WEAK_FUNC;

/*gather all expressions up until a row start marker and push the
  result as a MATRIX_ROW_NODE*/
gboolean gp_push_matrix_row(void) GEL_WEAK_FUNC;

/*gather all expressions up until a row start marker and push the
  result as a matrix*/
gboolean gp_push_matrix(gboolean quoted) GEL_WEAK_FUNC;

/*pushes a NULL onto the stack, null cannot be evaluated, it will be
  read as ""*/
void gp_push_null(void) GEL_WEAK_FUNC;

#define SYNTAX_ERROR {yyerror("syntax error"); YYERROR;}

#define PUSH_ACT(ACT) { \
	GelETree *tree = makeoperator((ACT),&evalstack); \
	if(!tree) {SYNTAX_ERROR;} \
	stack_push(&evalstack,tree); \
}

#define PUSH_IDENTIFIER(ID) { \
	GelETree * tree; \
	GET_NEW_NODE(tree); \
	tree->type = IDENTIFIER_NODE; \
	tree->id.id = d_intern(ID); \
	stack_push(&evalstack,tree); \
}

#define PUSH_STRING(ID) { \
	GelETree * tree; \
	GET_NEW_NODE(tree); \
	tree->type = STRING_NODE; \
	tree->str.str = (ID); \
	stack_push(&evalstack,tree); \
}

#endif
