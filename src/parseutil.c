/* GENIUS Calculator
 * Copyright (C) 1997-2009 Jiri (George) Lebl
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
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "util.h"
#include "calc.h"
#include "matrix.h"
#include "matrixw.h"

#include "parseutil.h"

extern GSList *gel_parsestack;


gboolean
gp_push_func (gboolean vararg, gboolean arguments, gboolean extradict, gboolean never_subst)
{
	GelETree * tree;
	GelETree * val;
	GSList * list = NULL;
	GSList * elist = NULL;
	int argnum = 0;
	gboolean local_all = FALSE;
	GSList *local_idents = NULL;
	
	val = gel_stack_pop (&gel_parsestack);
	if G_UNLIKELY (val == NULL)
		return FALSE;

	if (gel_get_local_node (val, TRUE /*first_arg*/,
				&local_all, &local_idents) == 2) {
		gel_errorout ("%s", _("ERROR: local statement not the first statement in function definition"));
		gel_freetree (val);
		g_slist_free (local_idents);
		return FALSE;
	}

	if (extradict) {
		for (;;) {
			tree = gel_stack_pop (&gel_parsestack);
			if (tree != NULL &&
			    tree->type == GEL_EXPRLIST_START_NODE) {
				gel_freetree (tree);
				break;
			}
			/*we have gone all the way to the top and haven't found a
			  marker or tree is not an ident node*/
			if G_UNLIKELY (tree == NULL ||
				       tree->type != GEL_IDENTIFIER_NODE) {
				if (tree != NULL) gel_freetree(tree);
				g_slist_free (elist); 
				g_slist_free (local_idents);
				return FALSE;
			}
			elist = g_slist_prepend (elist, tree->id.id);
			gel_freetree (tree);
		}
	}

	if (arguments) {
		for (;;) {
			tree = gel_stack_pop (&gel_parsestack);
			if (tree != NULL &&
			    tree->type == GEL_EXPRLIST_START_NODE) {
				gel_freetree (tree);
				break;
			}
			/*we have gone all the way to the top and haven't found a
			  marker or tree is not an ident node*/
			if G_UNLIKELY (tree == NULL ||
				       tree->type != GEL_IDENTIFIER_NODE) {
				if (tree != NULL) gel_freetree(tree);
				g_slist_free (list); 
				g_slist_free (elist); 
				g_slist_free (local_idents);
				return FALSE;
			}
			list = g_slist_prepend (list, tree->id.id);
			gel_freetree (tree);
			argnum++;
		}
	}

	GEL_GET_NEW_NODE(tree);

	tree->type = GEL_FUNCTION_NODE;
	tree->func.func = d_makeufunc(NULL,val,list,argnum, NULL);
	tree->func.func->context = -1;
	tree->func.func->vararg = vararg;
	tree->func.func->local_all = local_all ? 1 : 0;
	tree->func.func->local_idents = local_idents;
	tree->func.func->never_on_subst_list = never_subst ? 1 : 0;

	if (never_subst) {
		tree->func.func->built_subst_dict = 1;
		tree->func.func->subst_dict = elist;
	}

	gel_stack_push(&gel_parsestack,tree);

	return TRUE;
}

gboolean
gp_prepare_push_param (gboolean setfunc)
{
	GelETree * ident;
	GelETree * val;
	GelETree * func;

	/* FIXME: setfunc not yet implemented */
	g_assert ( ! setfunc);

	val = gel_stack_pop (&gel_parsestack);
	if (val == NULL)
		return FALSE;

	ident = gel_stack_pop (&gel_parsestack);
	if (ident == NULL)
		return FALSE;

	func = gel_makenum_null ();

	gel_stack_push (&gel_parsestack, func);
	gel_stack_push (&gel_parsestack, ident);
	gel_stack_push (&gel_parsestack, val);

	return TRUE;
}

/* returns true if this is a 'by' sep */
gboolean
gp_prepare_push_region_sep (void)
{
	GelETree *e1, *e2;

	e2 = gel_stack_pop (&gel_parsestack);
	e1 = gel_stack_pop (&gel_parsestack);

	if (e2->type == GEL_OPERATOR_NODE &&
	    e2->op.oper == GEL_E_REGION_SEP) {
		GelETree *a1 = e2->op.args;
		GelETree *a2 = e2->op.args->any.next;
		a1->any.next = NULL;
		a2->any.next = NULL;
		e2->op.args = NULL;
		gel_freetree (e2);
		gel_stack_push (&gel_parsestack, e1);
		gel_stack_push (&gel_parsestack, a1);
		gel_stack_push (&gel_parsestack, a2);

		return TRUE;
	} else {
		gel_stack_push (&gel_parsestack, e1);
		gel_stack_push (&gel_parsestack, e2);

		return FALSE;
	}
}

/*pops the last expression, pushes a marker
  entry and puts the last expression back*/
gboolean
gp_push_marker(GelETreeType markertype)
{
	GelETree * last_expr = gel_stack_pop(&gel_parsestack);
	GelETree * tree;
	
	if(!last_expr)
		return FALSE;
       
	GEL_GET_NEW_NODE(tree);
	tree->type = markertype;
	gel_stack_push(&gel_parsestack,tree);
	gel_stack_push(&gel_parsestack,last_expr);
	return TRUE;
}

/*pushes a marker*/
void
gp_push_marker_simple(GelETreeType markertype)
{
	GelETree *tree;
	GEL_GET_NEW_NODE(tree);
	tree->type = markertype;
	gel_stack_push(&gel_parsestack,tree);
}



/*puts a spacer into the tree, spacers are just useless nodes to be removed
  before evaluation, they just signify where there were parenthesis*/
gboolean
gp_push_spacer(void)
{
	GelETree * last_expr = gel_stack_pop(&gel_parsestack);
	
	if G_UNLIKELY (last_expr == NULL)
		return FALSE;
	else if(last_expr->type == GEL_SPACER_NODE)
		gel_stack_push(&gel_parsestack,last_expr);
	else {
		GelETree * tree;
		GEL_GET_NEW_NODE(tree);
		tree->type = GEL_SPACER_NODE;
		tree->sp.arg = last_expr;
		gel_stack_push(&gel_parsestack,tree);
	}
	return TRUE;
}

/*puts a local node into the tree, locals are just useless nodes to be removed
  right after parsing */
gboolean
gp_push_local_all (void)
{
	GelETree * tree;
	GelETree * last_expr = gel_stack_pop (&gel_parsestack);
	
	if G_UNLIKELY (last_expr == NULL)
		return FALSE;

	GEL_GET_NEW_NODE (tree);
	tree->type = GEL_LOCAL_NODE;
	tree->loc.arg = last_expr;
	tree->loc.idents = NULL; /* all */
	gel_stack_push (&gel_parsestack, tree);

	return TRUE;
}

/*puts a local node into the tree, locals are just useless nodes to be removed
  right after parsing */
gboolean
gp_push_local_idents (void)
{
	GelETree * tree;
	GSList * list = NULL;
	int i = 0;
	GelETree * last_expr = gel_stack_pop (&gel_parsestack);
	
	if G_UNLIKELY (last_expr == NULL)
		return FALSE;

	for (;;) {
		tree = gel_stack_pop (&gel_parsestack);
		if (tree && tree->type == GEL_EXPRLIST_START_NODE) {
			gel_freetree (tree);
			break;
		}
		/*we have gone all the way to the top and haven't found a
		  marker or tree is not an ident node*/
		if G_UNLIKELY (tree == NULL ||
			       tree->type != GEL_IDENTIFIER_NODE) {
			if (tree != NULL)
				gel_freetree(tree);
			g_slist_free (list); 
			return FALSE;
		}
		list = g_slist_prepend (list, tree->id.id);
		gel_freetree (tree);
		i++;
	}

	GEL_GET_NEW_NODE (tree);
	tree->type = GEL_LOCAL_NODE;
	tree->loc.arg = last_expr;
	tree->loc.idents = list;
	gel_stack_push (&gel_parsestack, tree);

	return TRUE;
}
	
/*gather all expressions up until a row start marker and push the
  result as a GEL_MATRIX_ROW_NODE*/
gboolean
gp_push_matrix_row(void)
{
	GelETree *tree;
	GelETree *row = NULL;
	int i=0;
	for(;;) {
		tree = gel_stack_pop(&gel_parsestack);
		/*we have gone all the way to the top and haven't found a
		  marker*/
		if(!tree) {
			while(row) {
				GelETree *li = row->any.next;
				gel_freetree(row);
				row = li;
			}
			return FALSE;
		}
		if(tree->type==GEL_EXPRLIST_START_NODE) {
			gel_freetree(tree);
			break;
		}
		tree->any.next = row;
		row = tree;
		i++;
	}
	GEL_GET_NEW_NODE(tree);
	tree->type = GEL_MATRIX_ROW_NODE;
	tree->row.args = row;
	tree->row.nargs = i;

	gel_stack_push(&gel_parsestack,tree);
	
	return TRUE;
}
	
/*gather all expressions up until a row start marker and push the
  result as a matrix*/
gboolean
gp_push_matrix(gboolean quoted)
{
	GelETree *tree;
	int i,j;
	int cols,rows;
	GSList *rowl = NULL;
	GSList *liy;
	GelETree *lix;
	
	GelMatrix *matrix;
	
	rows=0;
	cols=0;
	for(;;) {
		tree = gel_stack_pop(&gel_parsestack);
		/*we have gone all the way to the top and haven't found a
		  marker*/
		if(!tree) {
			GSList *li;
			for(li=rowl;li;li=g_slist_next(li)) {
				GelETree *row = li->data;
				while(row) {
					GelETree *a = row->any.next;
					gel_freetree(row);
					row = a;
				}
			}
			g_slist_free(rowl);
			/**/g_warning("BAD MATRIX, NO START MARKER");
			return FALSE;
		} else if(tree->type==GEL_MATRIX_START_NODE) {
			gel_freetree(tree);
			break;
		} else if(tree->type==GEL_MATRIX_ROW_NODE) {
			if((int)tree->row.nargs > cols)
				cols = tree->row.nargs;
			rowl = g_slist_prepend(rowl,tree->row.args);
			tree->row.args = NULL;
			tree->row.nargs = 0;
			gel_freetree(tree);
			rows++;
			continue;
		} else {
			GSList *li;
			gel_freetree(tree);
			for(li=rowl;li;li=g_slist_next(li)) {
				GelETree *row = li->data;
				while(row) {
					GelETree *a = row->any.next;
					gel_freetree(row);
					row = a;
				}
			}
			g_slist_free(rowl);
			/**/g_warning("BAD MATRIX, A NON ROW ELEMENT FOUND");
			return FALSE;
		}
	}

	matrix = gel_matrix_new();
	gel_matrix_set_size(matrix, cols, rows, TRUE /* padding */);
	
	for(j=0,liy=rowl;liy;j++,liy=g_slist_next(liy)) {
		for(i=0,lix=liy->data;lix;i++,lix=lix->any.next) {
			gel_matrix_index(matrix,i,j) = lix;
		}
		/* On non-quoted matrices fill the matrix with nulls
		 * since those may be converted to zeros */
		if ( ! quoted) {
			for(;i < cols;i++) {
				gel_matrix_index (matrix, i, j) =
					gel_makenum_null ();
			}
		}
	}
	g_slist_free(rowl);
	
	GEL_GET_NEW_NODE(tree);
	tree->type = GEL_MATRIX_NODE;
	tree->mat.matrix = gel_matrixw_new_with_matrix(matrix);
	tree->mat.quoted = quoted ? 1 : 0;
	
	gel_stack_push(&gel_parsestack,tree);
	return TRUE;
}

/*pushes a NULL onto the stack, null cannot be evaluated, it will be
  read as ""*/
void
gp_push_null(void)
{
	GelETree *tree;
	GEL_GET_NEW_NODE(tree);
	tree->type = GEL_NULL_NODE;

	gel_stack_push(&gel_parsestack,tree);
}

void
gp_convert_identifier_to_bool (void)
{
	GelETree *val;

	val = gel_stack_peek (&gel_parsestack);
	if (val == NULL ||
	    val->type != GEL_IDENTIFIER_NODE) {
		/**/g_warning ("NO IDENTIFIER TO CONVERT TO TRY TO CONVERT BOOL");
		return;
	}
	if (val->id.id == NULL ||
	    val->id.id->token == NULL)
		return;

	if (strcmp (val->id.id->token, "true") == 0 ||
	     strcmp (val->id.id->token, "True") == 0 ||
	     strcmp (val->id.id->token, "TRUE") == 0) {
		gel_emptytree (val);
		gel_makenum_bool_from (val, TRUE);
	} else if (strcmp (val->id.id->token, "false") == 0 ||
		   strcmp (val->id.id->token, "False") == 0 ||
		   strcmp (val->id.id->token, "FALSE") == 0) {
		gel_emptytree (val);
		gel_makenum_bool_from (val, FALSE);
	}
}
