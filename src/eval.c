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
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "compil.h"
#include "utype.h"

/*#define EVAL_DEBUG 1*/

#ifdef EVAL_DEBUG
#define EDEBUG(x) puts(x)
#else
#define EDEBUG(x) ;
#endif

/* Note: this won't be completely mem-debug friendly, but only mostly */
/* #define MEM_DEBUG_FRIENDLY 1 */

extern calcstate_t calcstate;

GelETree *free_trees = NULL;
static GelEvalStack *free_stack = NULL;
static GelEvalLoop *free_evl = NULL;
static GelEvalFor *free_evf = NULL;
static GelEvalForIn *free_evfi = NULL;

extern GHashTable *uncompiled;

extern int interrupted;

extern char *genius_params[];

static inline void
ge_add_stack_array(GelCtx *ctx)
{
	GelEvalStack *newstack;
	if(!free_stack) {
		newstack = g_new(GelEvalStack,1);
	} else {
		newstack = free_stack;
		free_stack = free_stack->next;
	}
	
	newstack->next = ctx->stack;
	ctx->stack = newstack;
	/*the array is at the beginning of the structure*/
	ctx->topstack = (gpointer *)newstack;
	EDEBUG("ADDING STACK ARRAY");
}

/*we assume that a stack always exists*/
#define GE_PUSH_STACK(thectx,pointer,flag) { \
	if((thectx)->topstack == &((thectx)->stack->stack[STACK_SIZE]))	\
	 	ge_add_stack_array(thectx);				\
	*((thectx)->topstack ++) = (pointer);				\
	*((thectx)->topstack ++) = GINT_TO_POINTER(flag);		\
}

static inline gboolean
ge_remove_stack_array(GelCtx *ctx)
{
	GelEvalStack *next = ctx->stack->next;
	if(!next) return FALSE;

	/*push it onto the list of free stack entries*/
#ifdef MEM_DEBUG_FRIENDLY
	g_free (ctx->stack);
#else /* MEM_DEBUG_FRIENDLY */
	ctx->stack->next = free_stack;
	free_stack = ctx->stack;
#endif /* MEM_DEBUG_FRIENDLY */

	ctx->stack = next;
	ctx->topstack = &((ctx)->stack->stack[STACK_SIZE]);
	EDEBUG("REMOVING STACK ARRAY");
	return TRUE;
}

#define GE_POP_STACK(thectx,pointer,flag) { \
	if((thectx)->topstack != (gpointer *)(thectx)->stack ||		\
	   ge_remove_stack_array(ctx)) {				\
		(flag) = GPOINTER_TO_INT(*(-- (thectx)->topstack));	\
		(pointer) = *(-- (thectx)->topstack);			\
	} else {							\
		(flag) = GE_EMPTY_STACK;				\
		(pointer) = NULL;					\
	}								\
}

#define GE_PEEK_STACK(ctx,pointer,flag) { \
	if((ctx)->topstack != (gpointer *)(ctx)->stack) {		\
		(flag) = GPOINTER_TO_INT(*((ctx)->topstack - 1));	\
		(pointer) = *((ctx)->topstack - 2);			\
	} else if((ctx)->stack->next) {					\
		gpointer *a = (gpointer) &((ctx)->stack->next->next);	\
		(flag) = GPOINTER_TO_INT(*(--a));			\
		(pointer) = *(--a);					\
	} else {							\
		(flag) = GE_EMPTY_STACK;				\
		(pointer) = NULL;					\
	}								\
}

#define GE_BLIND_POP_STACK(ctx) { \
	if((ctx)->topstack != (gpointer *)(ctx)->stack ||	\
	   ge_remove_stack_array(ctx)) {			\
		(ctx)->topstack -= 2;				\
	}							\
}

static void mod_node(GelETree *n, mpw_ptr mod);
static void mod_matrix (GelMatrixW *m, mpw_ptr mod);


/*returns the number of args for an operator, or -1 if it takes up till
  exprlist marker or -2 if it takes one more for the first argument*/
int
branches(int op)
{
	switch(op) {
		case E_SEPAR: return 2;
		case E_EQUALS: return 2;
		case E_PARAMETER: return 3;
		case E_ABS: return 1;
		case E_PLUS: return 2;
		case E_MINUS: return 2;
		case E_MUL: return 2;
		case E_ELTMUL: return 2;
		case E_DIV: return 2;
		case E_ELTDIV: return 2;
		case E_BACK_DIV: return 2;
		case E_ELT_BACK_DIV: return 2;
		case E_MOD: return 2;
		case E_ELTMOD: return 2;
		case E_NEG: return 1;
		case E_EXP: return 2;
		case E_ELTEXP: return 2;
		case E_FACT: return 1;
		case E_DBLFACT: return 1;
		case E_TRANSPOSE: return 1;
		case E_CONJUGATE_TRANSPOSE: return 1;
		case E_IF_CONS: return 2;
		case E_IFELSE_CONS: return 3;
		case E_WHILE_CONS: return 2;
		case E_UNTIL_CONS: return 2;
		case E_DOWHILE_CONS: return 2;
		case E_DOUNTIL_CONS: return 2;
		case E_FOR_CONS: return 4;
		case E_FORBY_CONS: return 5;
		case E_FORIN_CONS: return 3;
		case E_SUM_CONS: return 4;
		case E_SUMBY_CONS: return 5;
		case E_SUMIN_CONS: return 3;
		case E_PROD_CONS: return 4;
		case E_PRODBY_CONS: return 5;
		case E_PRODIN_CONS: return 3;
		case E_EQ_CMP: return 2;
		case E_NE_CMP: return 2;
		case E_CMP_CMP: return 2;
		case E_LT_CMP: return 2;
		case E_GT_CMP: return 2;
		case E_LE_CMP: return 2;
		case E_GE_CMP: return 2;
		case E_LOGICAL_AND: return 2;
		case E_LOGICAL_OR: return 2;
		case E_LOGICAL_XOR: return 2;
		case E_LOGICAL_NOT: return 1;
		case E_REGION_SEP: return 2;
		case E_REGION_SEP_BY: return 3;
		case E_GET_VELEMENT: return 2;
		case E_GET_ELEMENT: return 3;
		case E_GET_ROW_REGION: return 2;
		case E_GET_COL_REGION: return 2;
		case E_QUOTE: return 1;
		case E_REFERENCE: return 1;
		case E_DEREFERENCE: return 1;
		case E_DIRECTCALL: return -2;
		case E_CALL: return -2;
		case E_RETURN: return 1;
		case E_BAILOUT: return 0;
		case E_EXCEPTION: return 0;
		case E_CONTINUE: return 0;
		case E_BREAK: return 0;
		case E_MOD_CALC: return 2;
	}
	return 0;
}

mpw_ptr
gel_find_pre_function_modulo (GelCtx *ctx)
{
	GelEvalStack *stack = ctx->stack;
	gpointer *iter = ctx->topstack;
	gpointer *last = NULL;
	if ((gpointer)iter == (gpointer)stack) {
		if (stack->next == NULL)
			return NULL;
		stack = stack->next;
		iter = &(stack->stack[STACK_SIZE]);
	}
	while ((int)(*(iter-1)) != GE_FUNCCALL) {
		last = iter;
		iter -= 2;
		if ((gpointer)iter == (gpointer)stack) {
			if (stack->next == NULL)
				return NULL;
			stack = stack->next;
			iter = &(stack->stack[STACK_SIZE]);
		}
	}

	if (last == NULL || (int)(*(last-1)) != GE_SETMODULO) {
		return NULL;
	} else {
		return *(last-2);
	}
}

GelETree *
gel_makenum_null (void)
{
	GelETree *n;
	GET_NEW_NODE (n);
	n->type = NULL_NODE;
	return n;
}

GelETree *
gel_makenum_identifier (GelToken *id)
{
	GelETree *n;
	GET_NEW_NODE (n);
	n->type = IDENTIFIER_NODE;
	n->id.id = id; 
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_string (const char *str)
{
	GelETree *n;
	GET_NEW_NODE (n);
	n->type = STRING_NODE;
	n->str.str = g_strdup (str); 
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_string_use (char *str)
{
	GelETree *n;
	GET_NEW_NODE (n);
	n->type = STRING_NODE;
	n->str.str = str; 
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_ui(unsigned long num)
{
	GelETree *n;
	GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	mpw_init(n->val.value);
	mpw_set_ui(n->val.value,num);
	return n;
}

GelETree *
gel_makenum_si(long num)
{
	GelETree *n;
	GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	mpw_init(n->val.value);
	mpw_set_si(n->val.value,num);
	return n;
}

GelETree *
gel_makenum_d (double num)
{
	GelETree *n;
	GET_NEW_NODE (n);
	n->type = VALUE_NODE;
	mpw_init (n->val.value);
	mpw_set_d (n->val.value, num);
	return n;
}

GelETree *
gel_makenum(mpw_t num)
{
	GelETree *n;
	GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	mpw_init_set(n->val.value,num);
	return n;
}

/*don't create a new number*/
GelETree *
gel_makenum_use(mpw_t num)
{
	GelETree *n;
	GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	memcpy(n->val.value,num,sizeof(struct _mpw_t));
	return n;
}

void
gel_makenum_null_from(GelETree *n)
{
	n->type = NULL_NODE;
}

void
gel_makenum_ui_from(GelETree *n, unsigned long num)
{
	n->type=VALUE_NODE;
	mpw_init(n->val.value);
	mpw_set_ui(n->val.value,num);
}

void
gel_makenum_si_from(GelETree *n, long num)
{
	n->type=VALUE_NODE;
	mpw_init(n->val.value);
	mpw_set_si(n->val.value,num);
}

void
gel_makenum_from(GelETree *n, mpw_t num)
{
	n->type=VALUE_NODE;
	mpw_init_set(n->val.value,num);
}

/*don't create a new number*/
void
gel_makenum_use_from(GelETree *n, mpw_t num)
{
	n->type=VALUE_NODE;
	memcpy(n->val.value,num,sizeof(struct _mpw_t));
}

static inline void
freetree_full(GelETree *n, gboolean freeargs, gboolean kill)
{
	if(!n)
		return;
	switch(n->type) {
	case NULL_NODE: break;
	case VALUE_NODE:
		mpw_clear(n->val.value);
		break;
	case MATRIX_NODE:
		if(n->mat.matrix)
			gel_matrixw_free(n->mat.matrix);
		break;
	case OPERATOR_NODE:
		if(freeargs) {
			while(n->op.args) {
				GelETree *a = n->op.args;
				n->op.args = a->any.next;
				freetree_full(a,TRUE,TRUE);
			}
		}
		break;
	case IDENTIFIER_NODE:
		/*was this a fake token, to an anonymous function*/
		if(!n->id.id->token) {
			/*XXX:where does the function go?*/
			g_slist_free(n->id.id->refs);
			g_free(n->id.id);
		}
		break;
	case STRING_NODE:
		g_free(n->str.str);
		break;
	case FUNCTION_NODE:
		d_freefunc(n->func.func);
		break;
	case COMPARISON_NODE:
		if(freeargs) {
			while(n->comp.args) {
				GelETree *a = n->comp.args;
				n->comp.args = a->any.next;
				freetree_full(a,TRUE,TRUE);
			}
		}
		g_slist_free(n->comp.comp);
		break;
	case USERTYPE_NODE:
		gel_free_user_variable_data(n->ut.ttype,n->ut.data);
		break;
	case MATRIX_ROW_NODE:
		if(freeargs) {
			while(n->row.args) {
				GelETree *a = n->row.args;
				n->row.args = a->any.next;
				freetree_full(a,TRUE,TRUE);
			}
		}
		break;
	case SPACER_NODE:
		if(freeargs && n->sp.arg)
			gel_freetree(n->sp.arg);
		break;
	default: break;
	}
	if(kill) {
#ifdef MEM_DEBUG_FRIENDLY
		g_free (n);
#else
		/*put onto the free list*/
		n->any.next = free_trees;
		free_trees = n;
#endif
	}
}

void
gel_freetree(GelETree *n)
{
	/*printf ("freeing: %p\n", n);*/
	freetree_full(n,TRUE,TRUE);
}

static inline void
freenode(GelETree *n)
{
	freetree_full(n,FALSE,TRUE);
}

static inline void
copynode_to(GelETree *empty, GelETree *o)
{
	switch(o->type) {
	case NULL_NODE:
		empty->type = NULL_NODE;
		empty->any.next = o->any.next;
		break;
	case VALUE_NODE:
		empty->type = VALUE_NODE;
		empty->any.next = o->any.next;
		mpw_init_set(empty->val.value,o->val.value);
		break;
	case MATRIX_NODE:
		empty->type = MATRIX_NODE;
		empty->any.next = o->any.next;
		empty->mat.matrix = gel_matrixw_copy(o->mat.matrix);
		empty->mat.quoted = o->mat.quoted;
		break;
	case OPERATOR_NODE:
		empty->type = OPERATOR_NODE;
		empty->any.next = o->any.next;
		empty->op.oper = o->op.oper;
		empty->op.nargs = o->op.nargs;
		empty->op.args = o->op.args;
		if(empty->op.args) {
			GelETree *li;
			empty->op.args = copynode(empty->op.args);
			for(li=empty->op.args;li->any.next;li=li->any.next) {
				li->any.next = copynode(li->any.next);
			}
		}
		break;
	case IDENTIFIER_NODE:
		empty->type = IDENTIFIER_NODE;
		empty->any.next = o->any.next;
		empty->id.id = o->id.id;
		break;
	case STRING_NODE:
		empty->type = STRING_NODE;
		empty->any.next = o->any.next;
		empty->str.str = g_strdup(o->str.str);
		break;
	case FUNCTION_NODE:
		empty->type = FUNCTION_NODE;
		empty->any.next = o->any.next;
		empty->func.func = d_copyfunc(o->func.func);
		break;
	case COMPARISON_NODE:
		empty->type = COMPARISON_NODE;
		empty->any.next = o->any.next;
		empty->comp.nargs = o->comp.nargs;
		empty->comp.args = o->comp.args;
		if(empty->comp.args) {
			GelETree *li;
			empty->comp.args = copynode(empty->comp.args);
			for(li=empty->comp.args;li->any.next;li=li->any.next) {
				li->any.next = copynode(li->any.next);
			}
		}
		empty->comp.comp = g_slist_copy(o->comp.comp);
		break;
	case USERTYPE_NODE:
		empty->type = USERTYPE_NODE;
		empty->any.next = o->any.next;
		empty->ut.ttype = o->ut.ttype;
		empty->ut.data = gel_copy_user_variable_data(o->ut.ttype,
								o->ut.data);
		break;
	case MATRIX_ROW_NODE:
		empty->type = MATRIX_ROW_NODE;
		empty->any.next = o->any.next;
		empty->row.nargs = o->row.nargs;
		empty->row.args = o->row.args;
		if(empty->row.args) {
			GelETree *li;
			empty->row.args = copynode(empty->row.args);
			for(li=empty->row.args;li->any.next;li=li->any.next) {
				li->any.next = copynode(li->any.next);
			}
		}
		break;
	case SPACER_NODE:
		empty->type = SPACER_NODE;
		empty->any.next = o->any.next;
		if(o->sp.arg)
			empty->sp.arg = copynode(o->sp.arg);
		else
			empty->sp.arg = NULL;
		break;
	default:
		g_assert_not_reached();
		break;
	}
}

GelETree *
copynode(GelETree *o)
{
	GelETree *n;

	if(!o)
		return NULL;

	GET_NEW_NODE(n);
	
	copynode_to(n,o);
	
	return n;
}

static inline void
replacenode(GelETree *to, GelETree *from)
{
	GelETree *next = to->any.next;
	freetree_full(to,TRUE,FALSE);
	memcpy(to,from,sizeof(GelETree));

#ifdef MEM_DEBUG_FRIENDLY
	g_free (from);
#else
	/*put onto the free list*/
	from->any.next = free_trees;
	free_trees = from;
#endif
	to->any.next = next;

	/*printf ("replaced from: %p\n", from);*/
}
static inline void
copyreplacenode(GelETree *to, GelETree *from)
{
	GelETree *next = to->any.next;
	freetree_full(to,TRUE,FALSE);
	copynode_to(to,from);
	to->any.next = next;
}

GelETree *
makeoperator(int oper, GSList **stack)
{
	GelETree *n;
	int args;
	GelETree *list = NULL;
	args = branches(oper);
	if(args>=0) {
		int i;
		for(i=0;i<args;i++) {
			GelETree *tree = stack_pop(stack);
			if(!tree)  {
				while(list) {
					GelETree *a = list->any.next;
					gel_freetree(list);
					list = a;
				}
				return NULL;
			}
			tree->any.next = list;
			list = tree;
		}
	} else {
		int i=0;
		for(;;) {
			GelETree *tree;
			tree = stack_pop(stack);
			/*we have gone all the way to the top and haven't
			  found a marker*/
			if(!tree) {
				while(list) {
					GelETree *a = list->any.next;
					gel_freetree(list);
					list = a;
				}
				return NULL;
			}
			if(tree->type==EXPRLIST_START_NODE) {
				gel_freetree(tree);
				/*pop one more in case of -2*/
				if(args==-2) {
					GelETree *t;
					t = stack_pop(stack);
					/*we have gone all the way to the top
					  whoops!*/
					if(!t) {
						while(list) {
							GelETree *a = list->any.next;
							gel_freetree(list);
							list = a;
						}
						return NULL;
					}
					t->any.next = list;
					list = t;
					i++;
				}
				break;
			}
			tree->any.next = list;
			list = tree;
			i++;
		}
		args = i;
	}

	GET_NEW_NODE(n);
	n->type = OPERATOR_NODE;
	n->op.oper = oper;
	
	n->op.args = list;
	n->op.nargs = args;
	
	/*try_to_precalc_op(n);*/

	return n;
}

/* kind of a hack */
static GelETree the_null = {NULL_NODE};

/*need_colwise will return if we need column wise expansion*/
static int
expand_row (GelMatrix *dest, GelMatrixW *src, int di, int si, gboolean *need_colwise)
{
	int i;
	int height = 0;
	int roww;
	
	roww = 0;
	for(i=0;i<gel_matrixw_width(src);i++) {
		if(!gel_matrixw_set_index(src,i,si)) continue;
		roww = i+1;
	}

	for(i=0;i<roww;i++) {
		GelETree *et = gel_matrixw_set_index(src,i,si);
		if (et == NULL ||
		    (et->type != NULL_NODE &&
		     et->type != MATRIX_NODE)) {
			if (height == 0)
				height = 1;
		} else if (et != NULL &&
			   et->type == MATRIX_NODE &&
			   gel_matrixw_height(et->mat.matrix)>height) {
			height = gel_matrixw_height(et->mat.matrix);
		}
	}

	if (height == 0) {
		return 0;
	}
	
	gel_matrix_set_at_least_size(dest,1,di+height);
	
	for(i=roww-1;i>=0;i--) {
		int x;
		GelETree *et = gel_matrixw_set_index(src,i,si);
		gel_matrixw_set_index(src,i,si) = NULL;
		
		/*0 node*/
		if(!et) {
			for(x=0;x<height;x++)
				gel_matrix_index(dest,i,di+x) = NULL;
		/*null node*/
		} else if (et->type == NULL_NODE) {
			*need_colwise = TRUE;
			gel_matrix_index(dest,i,di) = et;
			for(x=1;x<height;x++)
				gel_matrix_index(dest,i,di+x) = &the_null;
		/*non-matrix node*/
		} else if(et->type!=MATRIX_NODE) {
			gel_matrix_index(dest,i,di) = et;
			for(x=1;x<height;x++)
				gel_matrix_index(dest,i,di+x) = copynode(et);
		/*single column matrix, convert to regular nodes*/
		} else if(gel_matrixw_width(et->mat.matrix) == 1) {
			int xx;
			gel_matrixw_make_private(et->mat.matrix);
			for(x=0;x<gel_matrixw_height(et->mat.matrix);x++) {
				gel_matrix_index(dest,i,di+x) =
					gel_matrixw_set_index(et->mat.matrix,0,x);
				gel_matrixw_set_index(et->mat.matrix,0,x) = NULL;
			}
			xx = 0;
			for(x=gel_matrixw_height(et->mat.matrix);x<height;x++) {
				gel_matrix_index(dest,i,di+x) =
					copynode(gel_matrix_index(dest,i,di+xx));
				if((++xx)>=gel_matrixw_height(et->mat.matrix))
					xx=0;
			}
			gel_freetree(et);
		/*non-trivial matrix*/
		} else {
			int xx;

			gel_matrixw_make_private(et->mat.matrix);

			for(x=0;x<gel_matrixw_height(et->mat.matrix);x++) {
				GelETree *n;
				GET_NEW_NODE(n);
				n->type = MATRIX_ROW_NODE;
				
				n->row.args = NULL;
				for(xx=gel_matrixw_width(et->mat.matrix)-1;xx>=0;xx--) {
					GelETree *t = gel_matrixw_set_index(et->mat.matrix,xx,x);
					if(!t)
						t = gel_makenum_ui(0);
					t->any.next = n->row.args;
					n->row.args = t;
					gel_matrixw_set_index(et->mat.matrix,xx,x) = NULL;
				}
				n->row.nargs = gel_matrixw_width(et->mat.matrix);
				
				gel_matrix_index(dest,i,di+x) = n;

				*need_colwise = TRUE;
			}
			xx = 0;
			for(x=gel_matrixw_height(et->mat.matrix);x<height;x++) {
				gel_matrix_index(dest,i,di+x) =
					copynode(gel_matrix_index(dest,i,di+xx));
				if((++xx)>=gel_matrixw_height(et->mat.matrix))
					xx=0;
			}
			gel_freetree(et);
		}
	}
	
	return height;
}


static int
expand_col (GelMatrix *dest, GelMatrix *src, int si, int di, int w)
{
	int i;

	for (i = 0; i < src->height; i++) {
		GelETree *et = gel_matrix_index (src, si, i);
		if (et == NULL) {
			int x;
			for (x = 0; x < w; x++)
				gel_matrix_index (dest, di+x, i) = NULL;
		} else if (et->type == NULL_NODE) {
			/* Also here we just replace NULL_NODE's with 0's */
			int x;
			if (et != &the_null)
				gel_freetree (et);
			for (x = 0; x < w; x++)
				gel_matrix_index (dest, di+x, i) = NULL;
		} else if (et->type != MATRIX_ROW_NODE) {
			int x;
			gel_matrix_index (dest, di, i) = et;
			for (x = 1; x < w; x++)
				gel_matrix_index (dest, di+x, i) = copynode (et);
		} else {
			int x;
			int xx;
			GelETree *iter;

			iter = et->row.args;
			for (iter = et->row.args, x=0; iter != NULL; x++) {
				if (iter->type == VALUE_NODE &&
				    mpw_is_integer (iter->val.value) &&
				    mpw_sgn (iter->val.value) == 0) {
					GelETree *next = iter->any.next;
					gel_matrix_index (dest, di+x, i) = NULL;
					iter->any.next = NULL;
					gel_freetree (iter);
					iter = next;
				} else {
					GelETree *old = iter;
					gel_matrix_index (dest, di+x, i) = iter;
					iter = iter->any.next;
					old->any.next = NULL;
				}
			}

			xx = 0;
			for (; x < w; x++) {
				gel_matrix_index (dest, di+x, i) =
					copynode (gel_matrix_index (dest, di+xx, i));
				xx++;
				if (xx >= et->row.nargs)
					xx = 0;
			}
			freenode (et);
		}
	}
	
	return w;
}

static int
get_cols (GelMatrix *m, int *colwidths, gboolean *just_denull)
{
	int i,j;
	int maxcol;
	int cols = 0;

	*just_denull = TRUE;

	for (i = 0; i < m->width; i++) {
		maxcol = 0;
		for (j = 0; j < m->height; j++) {
			GelETree *et = gel_matrix_index (m, i, j);
			if (et == NULL ||
			    (et->type != MATRIX_ROW_NODE &&
			     et->type != NULL_NODE)) {
				if (maxcol == 0)
					maxcol = 1;
			} else if (et->type != NULL_NODE) {
				/* Must be MATRIX_ROW_NODE then */
				if (et->row.nargs > maxcol)
					maxcol = et->row.nargs;
			}
		}
		if (maxcol != 1)
			*just_denull = FALSE;
		colwidths[i] = maxcol;
		cols += maxcol;
	}
	
	return cols;
}

static gboolean
mat_need_expand (GelMatrixW *m)
{
	int i, j;
	for (i = 0; i < gel_matrixw_width (m); i++) {
		for (j = 0; j < gel_matrixw_height (m); j++) {
			GelETree *et = gel_matrixw_set_index (m, i, j);
			if (et != NULL &&
			    (et->type == MATRIX_NODE ||
			     et->type == NULL_NODE))
				return TRUE;
		}
	}
	return FALSE;
}

/*evaluate a matrix (or try to), it will try to expand the matrix and
  put 0's into the empty, undefined, spots. For example, a matrix such
  as if b = [8,7]; a = [1,2:3,b]  should expand to, [1,2,2:3,8,7] */
void
gel_expandmatrix (GelETree *n)
{
	int i;
	int k;
	int cols;
	GelMatrix *m;
	gboolean need_colwise = FALSE;
	GelMatrixW *nm;
	int h,w;

	/* An empty matrix really */
	if (n->type == NULL_NODE)
		return;

	nm = n->mat.matrix;

	g_return_if_fail (n->type == MATRIX_NODE);

	if ( ! mat_need_expand (nm))
		return;

	w = gel_matrixw_width (nm);
	h = gel_matrixw_height (nm);

	if (w == 1 && h == 1) {
		GelETree *t = gel_matrixw_set_index (nm, 0, 0);
		if (t != NULL &&
		    t->type == MATRIX_NODE) {
			if (nm->m->use == 1) {
				gel_matrixw_set_index (nm, 0, 0) = NULL;
			} else {
				t = copynode (t);
			}
			replacenode (n, t);
			return;
		} else if (t != NULL &&
			   t->type == NULL_NODE) {
			freetree_full (n, TRUE, FALSE);
			n->type = NULL_NODE;
			return;
		}
	}

	gel_matrixw_make_private (nm);

	m = gel_matrix_new();
	gel_matrix_set_size(m, w, h, TRUE /* padding */);
	
	cols = gel_matrixw_width (nm);
	
	for (i = 0, k = 0; i < h; i++) {
		int w;
		w = expand_row (m, nm, k, i, &need_colwise);
		k += w;
	}

	if (k == 0) {
		gel_matrix_free (m);
		freetree_full (n, TRUE, FALSE);
		n->type = NULL_NODE;
		return;
	}

	/* If we whacked some rows completely shorten
	 * the matrix */
	if (k < h)
		gel_matrix_set_size (m, w, k, TRUE /* padding */);

	if (need_colwise) {
		gboolean just_denull;
		int *colwidths = g_new (int, m->width);

		cols = get_cols (m, colwidths, &just_denull);

		/* empty matrix, return null */
		if (cols == 0) {
			gel_matrix_free (m);
			g_free (colwidths);
			freetree_full (n, TRUE, FALSE);
			n->type = NULL_NODE;
			return;
		}

		if (just_denull) {
			int j;
			for (i = 0; i < m->width; i++) {
				for (j = 0; j < m->height; j++) {
					GelETree *et
						= gel_matrix_index (m, i, j);
					if (et != NULL &&
					    et->type == NULL_NODE) {
						if (et != &the_null)
							gel_freetree (et);
						gel_matrix_index (m, i, j)
							= NULL;
					}
				}
			}
		} else {
			int ii;
			GelMatrix *tm;

			tm = gel_matrix_new ();

			gel_matrix_set_size (tm,cols,m->height, TRUE /* padding */);
			for (i = 0, ii = 0; i < m->width; ii += colwidths[i], i++) {
				if (colwidths[i] > 0)
					expand_col (tm, m, i, ii, colwidths[i]);
			}
			gel_matrix_free (m);
			m = tm;

			g_free (colwidths);
		}
	}

	freetree_full (n, TRUE, FALSE);

	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (m);
	n->mat.quoted = 0;
}

static GelETree*
get_func_call_node(GelEFunc *func, GelETree **args, int nargs)
{
	int i;
	GelETree *l;
	GelETree *ret;
	GelETree *li = NULL;

	GET_NEW_NODE(l);
	l->type = FUNCTION_NODE;
	l->func.func = d_copyfunc(func);
	l->any.next = NULL;

	GET_NEW_NODE(ret);
	ret->type = OPERATOR_NODE;
	ret->op.oper = E_DIRECTCALL;
	ret->op.args = l;
	
	li = l;

	for(i=0;i<nargs;i++) {
		li = li->any.next = copynode(args[i]);
	}
	li->any.next = NULL;
	ret->op.nargs = nargs+1;
	return ret;
}

GelETree *
funccall(GelCtx *ctx, GelEFunc *func, GelETree **args, int nargs)
{
	GelETree *ret = NULL;
	
	g_return_val_if_fail(func!=NULL,NULL);

	ret = get_func_call_node(func,args,nargs);
	return eval_etree(ctx,ret);
}

/*compare nodes, return TRUE if equal
  makes them the same type as a side effect*/
static int
eqlnodes(GelETree *l, GelETree *r)
{
	int n = mpw_eql(l->val.value,r->val.value);
	if(error_num) return 0;
	return n;
}

/*compare nodes, return -1 if first one is smaller, 0 if they are
  equal, 1 if the first one is greater
  makes them the same type as a side effect*/
static int
cmpnodes(GelETree *l, GelETree *r)
{
	int n=0;

	n=mpw_cmp(l->val.value,r->val.value);

	if(error_num) return 0;

	if(n>0) n=1;
	else if(n<0) n=-1;
	return n;
}


static int
cmpcmpop(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int ret = cmpnodes(l,r);
	if(error_num) {
		error_num = 0;
		return TRUE;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_si_from(n,ret);
	return TRUE;
}

static int
logicalxorop(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int bad_node = FALSE;
	int ret = isnodetrue(l,&bad_node) != isnodetrue(r,&bad_node);
	if(bad_node || error_num) {
		error_num = 0;
		return TRUE;
	}
	freetree_full(n,TRUE,FALSE);
	if(ret) gel_makenum_ui_from(n,1);
	else gel_makenum_ui_from(n,0);
	return TRUE;
}

static int
logicalnotop(GelCtx *ctx, GelETree *n, GelETree *l)
{
	int bad_node = FALSE;
	int ret = !isnodetrue(l,&bad_node);
	if(bad_node || error_num) {
		error_num = 0;
		return TRUE;
	}
	freetree_full(n,TRUE,FALSE);
	if(ret) gel_makenum_ui_from(n,1);
	else gel_makenum_ui_from(n,0);
	return TRUE;
}

static int
eqstring(GelETree *a, GelETree *b)
{
	int r=0;
	if(a->type == STRING_NODE &&
	   b->type == STRING_NODE) {
		r=strcmp(a->str.str, b->str.str)==0;
	} else if(a->type == STRING_NODE) {
		char *s = string_print_etree(b);
		r = strcmp(a->str.str, s)==0;
		g_free(s);
	} else if(b->type == STRING_NODE) {
		char *s = string_print_etree(a);
		r = strcmp(b->str.str, s)==0;
		g_free(s);
	} else
		g_assert_not_reached();

	return r;
}

static int
eqmatrix(GelETree *a, GelETree *b, int *error)
{
	int r=FALSE;
	int i,j;
	if(a->type == MATRIX_NODE &&
	   b->type == MATRIX_NODE) {
		if(!gel_is_matrix_value_only(a->mat.matrix) ||
		   !gel_is_matrix_value_only(b->mat.matrix)) {
			(*errorout)(_("Cannot compare non value-only matrixes"));
			*error = TRUE;
			return 0;
		}
		if(gel_matrixw_width(a->mat.matrix)!=
		   gel_matrixw_width(b->mat.matrix) ||
		   gel_matrixw_height(a->mat.matrix)!=
		   gel_matrixw_height(b->mat.matrix))
			r = FALSE;
		else {
			GelMatrixW *m1 = a->mat.matrix;
			GelMatrixW *m2 = b->mat.matrix;
			
			r = TRUE;

			for(i=0;i<gel_matrixw_width(m1);i++) {
				for(j=0;j<gel_matrixw_height(m1);j++) {
					GelETree *t1,*t2;
					t1 = gel_matrixw_index(m1,i,j);
					t2 = gel_matrixw_index(m2,i,j);
					if ( ! mpw_eql (t1->val.value,
							t2->val.value)) {
						r = FALSE;
						break;
					}
				}
				if(!r) break;
			}
		}
	} else if(a->type == MATRIX_NODE) {
		GelMatrixW *m = a->mat.matrix;
		if(gel_matrixw_width(m)>1 ||
		   gel_matrixw_height(m)>1) {
			r = FALSE;
		} else {
			GelETree *t = gel_matrixw_index(m,0,0);
			if(t->type != VALUE_NODE) {
				(*errorout)(_("Cannot compare non value-only matrixes"));
				*error = TRUE;
				return 0;
			}
			r = mpw_eql(t->val.value,b->val.value);
		}
	} else if(b->type == MATRIX_NODE) {
		GelMatrixW *m = b->mat.matrix;
		if(gel_matrixw_width(m)>1 ||
		   gel_matrixw_height(m)>1) {
			r = FALSE;
		} else {
			GelETree *t = gel_matrixw_index(m,0,0);
			if(t->type != VALUE_NODE) {
				(*errorout)(_("Cannot compare non value-only matrixes"));
				*error = TRUE;
				return 0;
			}
			r = mpw_eql(t->val.value,a->val.value);
		}
	} else
		g_assert_not_reached();

	return r;
}

static int
cmpstring(GelETree *a, GelETree *b)
{
	int r=0;
	if(a->type == STRING_NODE &&
	   b->type == STRING_NODE) {
		r=strcmp(a->str.str, b->str.str);
	} else if(a->type == STRING_NODE) {
		char *s = string_print_etree(b);
		r = strcmp(a->str.str, s);
		g_free(s);
	} else if(b->type == STRING_NODE) {
		char *s = string_print_etree(b);
		r = strcmp(b->str.str, s);
		g_free(s);
	} else
		g_assert_not_reached();

	return r;
}

static int
cmpstringop(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int ret;
	ret = cmpstring(l,r);
	freetree_full(n,TRUE,FALSE);
	if(ret>0) gel_makenum_ui_from(n,1);
	else if(ret<0) gel_makenum_si_from(n,-1);
	else gel_makenum_ui_from(n,0);
	return TRUE;
}

gboolean
gel_mod_integer_rational (mpw_t num, mpw_t mod)
{
	if (mpw_is_complex (num)) {
		/* also on rationals but as integers */
		(*errorout)(_("Modulo arithmetic only works on integers"));
		return FALSE;
	} else if (mpw_is_integer (num)) {
		mpw_mod (num, num, mod);
		if (mpw_sgn (num) < 0)
			mpw_add (num, mod, num);
		if (error_num != 0)
			return FALSE;
		else
			return TRUE;
	} else if (mpw_is_rational (num)) {
		mpw_t n, d;
		mpw_init (n);
		mpw_init (d);
		mpw_numerator (n, num);
		mpw_denominator (d, num);

		mpw_mod (n, n, mod);
		if (mpw_sgn (n) < 0)
			mpw_add (n, mod, n);

		mpw_mod (d, d, mod);
		if (mpw_sgn (d) < 0)
			mpw_add (d, mod, d);

		if (error_num != 0) {
			mpw_clear (n);
			mpw_clear (d);
			return FALSE;
		}

		mpw_invert (num, d, mod);
		if (error_num != 0) {
			mpw_clear (n);
			mpw_clear (d);
			return FALSE;
		}
		mpw_mul (num, num, n);
		mpw_mod (num, num, mod);

		if (error_num != 0)
			return FALSE;
		else
			return TRUE;
	} else {
		/* also on rationals but as integers */
		(*errorout)(_("Modulo arithmetic only works on integers"));
		return FALSE;
	}
}

static GelETree *
op_two_nodes (GelCtx *ctx, GelETree *ll, GelETree *rr, int oper,
	      gboolean no_push)
{
	GelETree *n;
	mpw_t res;
	
	if(rr->type == VALUE_NODE &&
	   ll->type == VALUE_NODE) {
		gboolean skipmod = FALSE;
		mpw_init(res);
		switch(oper) {
		case E_PLUS:
			mpw_add(res,ll->val.value,rr->val.value);
			break;
		case E_MINUS:
			mpw_sub(res,ll->val.value,rr->val.value);
			break;
		case E_MUL:
		case E_ELTMUL:
			mpw_mul(res,ll->val.value,rr->val.value);
			break;
		case E_DIV:
		case E_ELTDIV:
			mpw_div(res,ll->val.value,rr->val.value);
			break;
		case E_BACK_DIV:
		case E_ELT_BACK_DIV:
			mpw_div(res,rr->val.value,ll->val.value);
			break;
		case E_MOD:
		case E_ELTMOD:
			mpw_mod(res,ll->val.value,rr->val.value);
			break;
		case E_EXP:
		case E_ELTEXP:
			if (ctx->modulo != NULL) {
				mpw_powm (res, ll->val.value, rr->val.value,
					  ctx->modulo);
				skipmod = TRUE;
			} else {
				mpw_pow (res, ll->val.value, rr->val.value);
			}
			break;
		default: g_assert_not_reached();
		}
		if (!skipmod && ctx->modulo != NULL) {
			if ( ! gel_mod_integer_rational (res, ctx->modulo)) {
				error_num = NUMERICAL_MPW_ERROR;
			}
		}
		if(error_num==NUMERICAL_MPW_ERROR) {
			GET_NEW_NODE(n);
			n->type = OPERATOR_NODE;
			n->op.oper = oper;
			n->op.args = copynode(ll);
			n->op.args->any.next = copynode(rr);
			n->op.args->any.next->any.next = NULL;
			n->op.nargs = 2;
			mpw_clear(res);
			error_num = NO_ERROR;
			return n;
		}
		return gel_makenum_use(res);
	} else {
		/*this is the less common case so we can get around with a
		  wierd thing, we'll just make a new fake node and pretend
		  we want to evaluate that*/
		GET_NEW_NODE(n);
		n->type = OPERATOR_NODE;
		n->op.oper = oper;

		n->op.args = copynode(ll);
		n->op.args->any.next = copynode(rr);
		n->op.args->any.next->any.next = NULL;
		n->op.nargs = 2;
		
		if ( ! no_push) {
			GE_PUSH_STACK (ctx, n, GE_PRE);
		}

		return n;
	}
}


/*add, sub, mul, div*/
static int
matrix_scalar_matrix_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	GelMatrixW *m;
	GelETree *node;
	int order = 0;
	int quote = 0;
	if(l->type == MATRIX_NODE) {
		m = l->mat.matrix;
		quote = l->mat.quoted;
		node = r;
	} else {
		order = 1;
		m = r->mat.matrix;
		quote = r->mat.quoted;
		node = l;
	}

	gel_matrixw_make_private(m);

	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			if(order == 0) {
				gel_matrixw_set_index(m,i,j) =
					op_two_nodes(ctx,
						     gel_matrixw_index(m,i,j),
						     node, n->op.oper,
						     FALSE /* no_push */);
			} else {
				gel_matrixw_set_index(m,i,j) =
					op_two_nodes(ctx,node,
						     gel_matrixw_index(m,i,j),
						     n->op.oper,
						     FALSE /* no_push */);
			}
		}
	}
	n->op.args = NULL;

	if(l->type == MATRIX_NODE) {
		replacenode(n,l);
		gel_freetree(r);
	} else {
		replacenode(n,r);
		gel_freetree(l);
	}
	return TRUE;
}

static int
matrix_absnegfac_op(GelCtx *ctx, GelETree *n, GelETree *l)
{
	int i,j;
	GelMatrixW *m = l->mat.matrix;

	gel_matrixw_make_private(m);

	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *t = gel_matrixw_set_index(m,i,j);
			if(t == NULL) {
				if(n->op.oper == E_FACT ||
				   n->op.oper == E_DBLFACT)
					gel_matrixw_set_index(m,i,j) = gel_makenum_ui(1);
			} else if(t->type == VALUE_NODE) {
				switch(n->op.oper) {
				case E_ABS:
					mpw_abs(t->val.value,t->val.value);
					break;
				case E_NEG:
					mpw_neg(t->val.value,t->val.value);
					break;
				case E_FACT:
					mpw_fac(t->val.value,t->val.value);
					break;
				case E_DBLFACT:
					mpw_dblfac(t->val.value,t->val.value);
					break;
				default:
					g_assert_not_reached();
				}
			} else {
				GelETree *nn;
				GET_NEW_NODE(nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = n->op.oper;
				nn->op.args = t;
				t->any.next = NULL;
				nn->op.nargs = 1;
				gel_matrixw_set_index(m,i,j) = nn;
				GE_PUSH_STACK(ctx,nn,GE_PRE);
			}
		}
	}
	/*remove l from argument list*/
	n->op.args = NULL;
	replacenode(n,l);
	return TRUE;
}

static int
pure_matrix_eltbyelt_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	GelMatrixW *m1,*m2;
	m1 = l->mat.matrix;
	m2 = r->mat.matrix;
	if((gel_matrixw_width(m1) != gel_matrixw_width(m2)) ||
	   (gel_matrixw_height(m1) != gel_matrixw_height(m2))) {
		if (n->op.oper == E_PLUS ||
		    n->op.oper == E_MINUS)
			(*errorout)(_("Can't add/subtract two matricies of different sizes"));
		else
			(*errorout)(_("Can't do element by element operations on two matricies of different sizes"));
		return TRUE;
	}
	l->mat.quoted = l->mat.quoted || r->mat.quoted;
	gel_matrixw_make_private(m1);
	for(i=0;i<gel_matrixw_width(m1);i++) {
		for(j=0;j<gel_matrixw_height(m1);j++) {
			gel_matrixw_set_index(m1,i,j) =
				op_two_nodes(ctx,gel_matrixw_index(m1,i,j),
					     gel_matrixw_index(m2,i,j),
					     n->op.oper,
					     FALSE /* no_push */);
		}
	}
	/*remove l from arglist*/
	n->op.args = n->op.args->any.next;
	/*replace n with l*/
	replacenode(n,l);
	return TRUE;
}

static void
expensive_matrix_multiply(GelCtx *ctx, GelMatrixW *res, GelMatrixW *m1, GelMatrixW *m2)
{
	int i,j,k;
	for(i=0;i<gel_matrixw_width(res);i++) {
		for(j=0;j<gel_matrixw_height(res);j++) {
			GelETree *a = NULL;
			for(k=0;k<gel_matrixw_width(m1);k++) {
				GelETree *t;
				GelETree *t2;
				t = op_two_nodes(ctx,gel_matrixw_index(m1,j,k),
						 gel_matrixw_index(m2,k,i),
						 E_MUL,
						 FALSE /* no_push */);
				if(!a) {
					a=t;
				} else {
					t2 = op_two_nodes(ctx,a,t,E_PLUS,
							  FALSE /* no_push */);
					gel_freetree(t);
					gel_freetree(a);
					a = t2;
				}
			}
			gel_matrixw_set_index(res,i,j) = a;
		}
	}
}

static int
pure_matrix_mul_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	GelMatrixW *m, *m1,*m2;
	int quote;
	m1 = l->mat.matrix;
	m2 = r->mat.matrix;
	if((gel_matrixw_width(m1) != gel_matrixw_height(m2))) {
		(*errorout)(_("Can't multiply matricies of wrong sizes"));
		return TRUE;
	}
	m = gel_matrixw_new();
	quote = l->mat.quoted || r->mat.quoted;
	gel_matrixw_set_size(m,gel_matrixw_width(m2),gel_matrixw_height(m1));

	if (ctx->modulo != NULL) {
		    if (gel_is_matrix_value_only_integer (m1) &&
			gel_is_matrix_value_only_integer (m2)) {
			    gel_value_matrix_multiply (m, m1, m2, ctx->modulo);
		    } else {
			    expensive_matrix_multiply (ctx, m, m1, m2);
		    }
	} else {
		if(gel_is_matrix_value_only(m1) &&
		   gel_is_matrix_value_only(m2)) {
			gel_value_matrix_multiply (m, m1, m2, NULL);
		} else {
			expensive_matrix_multiply(ctx,m,m1,m2);
		}
	}
	freetree_full(n,TRUE,FALSE);
	n->type = MATRIX_NODE;
	n->mat.matrix = m;
	n->mat.quoted = quote;
	return TRUE;
}

static int
matrix_pow_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	long power;
	int quote;
	GelMatrixW *res = NULL;
	GelMatrixW *m;
	int free_m = FALSE;
	mpw_ptr old_modulo;

	m = l->mat.matrix;
	quote = l->mat.quoted;

	if(r->type != VALUE_NODE ||
	   mpw_is_complex(r->val.value) ||
	   !mpw_is_integer(r->val.value) ||
	   (gel_matrixw_width(m) !=
	    gel_matrixw_height(m)) ||
	   !gel_is_matrix_value_only(m)) {
		(*errorout)(_("Powers are defined on (square matrix)^(integer) only"));
		return TRUE;
	}

	if (ctx->modulo != NULL &&
	    ! gel_is_matrix_value_only_integer (m)) {
		(*errorout)(_("Powers on matrices in modulo mode are defined on integer matrices only"));
		return TRUE;
	}
	
	error_num = 0;
	power = mpw_get_long(r->val.value);
	if(error_num) {
		error_num = 0;
		(*errorout)(_("Exponent too large"));
		return TRUE;
	}
	
	if(power<=0) {
		GelMatrixW *mi;
		mi = gel_matrixw_new();
		gel_matrixw_set_size(mi,gel_matrixw_width(m),
				     gel_matrixw_height(m));
		for(i=0;i<gel_matrixw_width(m);i++)
			for(j=0;j<gel_matrixw_width(m);j++)
				if(i==j)
					gel_matrixw_set_index(mi,i,j) =
						gel_makenum_ui(1);
		if(power==0) {
			freetree_full(n,TRUE,FALSE);
			n->type = MATRIX_NODE;
			n->mat.matrix = mi;
			n->mat.quoted = quote;
			return TRUE;
		}

		m = gel_matrixw_copy(m);
		/* FIXME: unfortunately the modulo logic of gauss is fucked */
		old_modulo = ctx->modulo;
		ctx->modulo = NULL;
		if(!gel_value_matrix_gauss(ctx,m,TRUE,FALSE,TRUE,NULL,mi)) {
			ctx->modulo = old_modulo;
			(*errorout)(_("Matrix appears singular and can't be inverted"));
			gel_matrixw_free(m);
			gel_matrixw_free(mi);
			return TRUE;
		}
		ctx->modulo = old_modulo;
		gel_matrixw_free(m);
		m = mi;
		free_m = TRUE;

		/* Mod if in modulo mode */
		if (ctx->modulo != NULL)
			mod_matrix (m, ctx->modulo);

		power = -power;
	}
	
	if(power==1) {
		if(!free_m)
			l->mat.matrix = NULL;
		freetree_full(n,TRUE,FALSE);
		n->type = MATRIX_NODE;
		n->mat.matrix = m;
		n->mat.quoted = quote;
		return TRUE;
	}

	while(power>0) {
		/*if odd*/
		if(power & 0x1) {
			if(res) {
				GelMatrixW *ml = gel_matrixw_new();
				gel_matrixw_set_size(ml,gel_matrixw_width(m),
						     gel_matrixw_height(m));
				gel_value_matrix_multiply(ml,res,m,ctx->modulo);
				gel_matrixw_free(res);
				res = ml;
			} else
				res = gel_matrixw_copy(m);
			power--;
		} else { /*even*/
			GelMatrixW *ml = gel_matrixw_new();
			gel_matrixw_set_size(ml,gel_matrixw_width(m),
					 gel_matrixw_height(m));
			gel_value_matrix_multiply(ml,m,m,ctx->modulo);
			if(free_m)
				gel_matrixw_free(m);
			m = ml;
			free_m = TRUE;

			power >>= 1; /*divide by two*/
		}
	}
	
	freetree_full(n,TRUE,FALSE);
	n->type = MATRIX_NODE;
	if(!res) {
		if(free_m)
			n->mat.matrix = m;
		else
			n->mat.matrix = gel_matrixw_copy(m);
	} else {
		n->mat.matrix = res;
		if(free_m)
			gel_matrixw_free(m);
	}
	n->mat.quoted = quote;
	return TRUE;
}

static int
pure_matrix_div_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	int quote;
	GelMatrixW *m1,*m2;
	GelMatrixW *mi,*toinvert;
	GelMatrixW *res;
	mpw_ptr old_modulo;

	m1 = l->mat.matrix;
	m2 = r->mat.matrix;
	quote = l->mat.quoted || r->mat.quoted;

	if((gel_matrixw_width(m1) !=
	    gel_matrixw_height(m1)) ||
	   (gel_matrixw_width(m2) !=
	    gel_matrixw_height(m2)) ||
	   (gel_matrixw_width(m1) !=
	    gel_matrixw_width(m2)) ||
	   !gel_is_matrix_value_only(m1) ||
	   !gel_is_matrix_value_only(m2)) {
		(*errorout)(_("Can't divide matrices of different sizes or non-square matrices"));
		return TRUE;
	}

	mi = gel_matrixw_new();
	gel_matrixw_set_size(mi,gel_matrixw_width(m1),
			 gel_matrixw_height(m1));
	for(i=0;i<gel_matrixw_width(m1);i++)
		for(j=0;j<gel_matrixw_width(m1);j++)
			if(i==j)
				gel_matrixw_set_index(mi,i,j) =
					gel_makenum_ui(1);

	if(n->op.oper == E_BACK_DIV)
		toinvert = m1;
	else
		toinvert = m2;

	toinvert = gel_matrixw_copy(toinvert);
	/* FIXME: unfortunately the modulo logic of gauss is fucked */
	old_modulo = ctx->modulo;
	ctx->modulo = NULL;
	if(!gel_value_matrix_gauss(ctx,toinvert,TRUE,FALSE,TRUE,NULL,mi)) {
		ctx->modulo = old_modulo;
		(*errorout)(_("Matrix appears singular and can't be inverted"));
		gel_matrixw_free(mi);
		gel_matrixw_free(toinvert);
		return TRUE;
	}
	ctx->modulo = old_modulo;
	gel_matrixw_free(toinvert);

	/* Mod if in modulo mode */
	if (ctx->modulo != NULL)
		mod_matrix (mi, ctx->modulo);

	if(n->op.oper == E_BACK_DIV)
		m1 = mi;
	else
		m2 = mi;

	res = gel_matrixw_new();
	gel_matrixw_set_size(res,gel_matrixw_width(m1),
			 gel_matrixw_height(m1));
	gel_value_matrix_multiply(res,m1,m2,ctx->modulo);
	if(n->op.oper == E_BACK_DIV)
		gel_matrixw_free(m1);
	else
		gel_matrixw_free(m2);

	freetree_full(n,TRUE,FALSE);
	n->type = MATRIX_NODE;
	n->mat.matrix = res;
	n->mat.quoted = quote;
	return TRUE;
}

static int
value_matrix_div_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	int quote;
	GelMatrixW *m;
	GelMatrixW *mi;
	mpw_ptr old_modulo;

	m = r->mat.matrix;
	quote = r->mat.quoted;

	if((gel_matrixw_width(m) !=
	    gel_matrixw_height(m)) ||
	   !gel_is_matrix_value_only(m)) {
		(*errorout)(_("Can't divide by a non-square matrix"));
		return TRUE;
	}

	mi = gel_matrixw_new();
	gel_matrixw_set_size(mi,gel_matrixw_width(m),
			 gel_matrixw_height(m));
	for(i=0;i<gel_matrixw_width(m);i++)
		for(j=0;j<gel_matrixw_width(m);j++)
			if(i==j)
				gel_matrixw_set_index(mi,i,j) =
					gel_makenum_ui(1);

	m = gel_matrixw_copy(m);
	/* FIXME: unfortunately the modulo logic of gauss is fucked */
	old_modulo = ctx->modulo;
	ctx->modulo = NULL;
	if(!gel_value_matrix_gauss(ctx,m,TRUE,FALSE,TRUE,NULL,mi)) {
		ctx->modulo = old_modulo;
		(*errorout)(_("Matrix appears singular and can't be inverted"));
		gel_matrixw_free(mi);
		gel_matrixw_free(m);
		return TRUE;
	}
	ctx->modulo = old_modulo;
	gel_matrixw_free(m);
	m = mi;

	/* Mod if in modulo mode */
	if (ctx->modulo != NULL)
		mod_matrix (mi, ctx->modulo);

	for(i=0;i<gel_matrixw_width(m);i++)
		for(j=0;j<gel_matrixw_width(m);j++) {
			GelETree *t = gel_matrixw_set_index(m,i,j);
			if(t)
				mpw_mul(t->val.value,t->val.value,
					l->val.value);
		}

	freetree_full(n,TRUE,FALSE);
	n->type = MATRIX_NODE;
	n->mat.matrix = m;
	n->mat.quoted = quote;
	return TRUE;
}

/*add, sub */
static int
polynomial_add_sub_op (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	if (l->type == VALUE_NODE) {
		/* r->type == POLYNOMIAL_NODE */
		/* FIXME implement */
	} else if (r->type == VALUE_NODE) {
		/* l->type == POLYNOMIAL_NODE */
		/* FIXME implement */
	} else {
		/* FIXME implement */
	}

	return TRUE;
}

static void
mod_matrix (GelMatrixW *m, mpw_ptr mod)
{
	int i,j;
	int w,h;

	/*make us a private copy!*/
	gel_matrixw_make_private(m);

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			GelETree *t = gel_matrixw_set_index (m, i, j);
			if (t != NULL) {
				mod_node (t, mod);
			}
		}
	}
}

static void
mod_node (GelETree *n, mpw_ptr mod)
{
	if(n->type == VALUE_NODE) {
		if ( ! gel_mod_integer_rational (n->val.value, mod)) {
			GelETree *nn;
			GET_NEW_NODE(nn);
			nn->type = OPERATOR_NODE;
			nn->op.oper = E_MOD_CALC;
			nn->op.args = copynode (n);
			nn->op.args->any.next = gel_makenum (mod);
			nn->op.args->any.next->any.next = NULL;
			nn->op.nargs = 2;
			error_num = NO_ERROR;
			replacenode (n, nn);
		}
	} else if(n->type == MATRIX_NODE) {
		if (n->mat.matrix != NULL)
			mod_matrix (n->mat.matrix, mod);
	}
}

void
gel_mod_node (GelCtx *ctx, GelETree *n)
{
	if (ctx->modulo != NULL)
		mod_node (n, ctx->modulo);
}

/*return TRUE if node is true (a number node !=0), false otherwise*/
int
isnodetrue(GelETree *n, int *bad_node)
{
	if(n->type==STRING_NODE) {
		if(n->str.str && *n->str.str)
			return TRUE;
		else 
			return FALSE;
	}
	if(n->type!=VALUE_NODE) {
		if(bad_node) *bad_node = TRUE;
		return FALSE;
	}
	if(mpw_sgn(n->val.value)!=0)
		return TRUE;
	else
		return FALSE;
}

static int
transpose_matrix (GelCtx *ctx, GelETree *n, GelETree *l)
{
	l->mat.matrix->tr = !(l->mat.matrix->tr);
	/*remove from arglist*/
	n->op.args = NULL;
	replacenode(n,l);
	return TRUE;
}

static int
conjugate_transpose_matrix (GelCtx *ctx, GelETree *n, GelETree *l)
{
	if (gel_is_matrix_value_only_real (l->mat.matrix)) {
		l->mat.matrix->tr = !(l->mat.matrix->tr);
	} else {
		gel_matrix_conjugate_transpose (l->mat.matrix);
	}
	/*remove from arglist*/
	n->op.args = NULL;
	replacenode(n,l);
	return TRUE;
}

static int
string_concat(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	char *s = NULL;
	
	if(l->type == STRING_NODE &&
	   r->type == STRING_NODE) {
		s = g_strconcat(l->str.str, r->str.str, NULL);
	} else if(l->type == STRING_NODE) {
		char *t = string_print_etree(r);
		s = g_strconcat(l->str.str, t, NULL);
		g_free(t);
	} else if(r->type == STRING_NODE) {
		char *t = string_print_etree(l);
		s = g_strconcat(t,r->str.str,NULL);
		g_free(t);
	} else
		g_assert_not_reached();
	
	freetree_full(n, TRUE, FALSE);
	n->type = STRING_NODE;
	n->str.str = s;

	return TRUE;
}


/*for numbers*/
static void
my_mpw_back_div(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	mpw_div(rop,op2,op1);
}


#define PRIM_NUM_FUNC_1(funcname,mpwfunc) \
static int							\
funcname(GelCtx *ctx, GelETree *n, GelETree *l)			\
{								\
	mpw_t res;						\
								\
	mpw_init(res);						\
	mpwfunc(res,l->val.value);				\
	if(error_num==NUMERICAL_MPW_ERROR) {			\
		mpw_clear(res);					\
		error_num=NO_ERROR;				\
		return TRUE;					\
	}							\
								\
	freetree_full(n,TRUE,FALSE);				\
	gel_makenum_use_from(n,res);				\
	return TRUE;						\
}
#define PRIM_NUM_FUNC_2(funcname,mpwfunc) \
static int							\
funcname(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)	\
{								\
	mpw_t res;						\
								\
	mpw_init(res);						\
	mpwfunc(res,l->val.value,r->val.value);			\
	if(error_num==NUMERICAL_MPW_ERROR) {			\
		mpw_clear(res);					\
		error_num=NO_ERROR;				\
		return TRUE;					\
	}							\
								\
	freetree_full(n,TRUE,FALSE);				\
	gel_makenum_use_from(n,res);				\
	return TRUE;						\
}

PRIM_NUM_FUNC_1(numerical_abs,mpw_abs)
PRIM_NUM_FUNC_1(numerical_neg,mpw_neg)
PRIM_NUM_FUNC_1(numerical_fac,mpw_fac)
PRIM_NUM_FUNC_1(numerical_dblfac,mpw_dblfac)
PRIM_NUM_FUNC_2(numerical_add,mpw_add)
PRIM_NUM_FUNC_2(numerical_sub,mpw_sub)
PRIM_NUM_FUNC_2(numerical_mul,mpw_mul)
PRIM_NUM_FUNC_2(numerical_div,mpw_div)
PRIM_NUM_FUNC_2(numerical_mod,mpw_mod)
PRIM_NUM_FUNC_2(numerical_back_div,my_mpw_back_div)

static int
numerical_pow (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	mpw_t res;

	mpw_init(res);
	if (ctx->modulo != NULL)
		mpw_powm (res, l->val.value, r->val.value, ctx->modulo);
	else
		mpw_pow (res, l->val.value, r->val.value);
	if (error_num == NUMERICAL_MPW_ERROR) {
		mpw_clear (res);
		error_num = NO_ERROR;
		return TRUE;
	}

	freetree_full (n, TRUE, FALSE);
	gel_makenum_use_from (n, res);
	return TRUE;
}
	
#define EMPTY_PRIM {{{{0}}}}

static const GelOper prim_table[E_OPER_LAST] = {
	/*E_SEPAR*/ EMPTY_PRIM,
	/*E_EQUALS*/ EMPTY_PRIM,
	/*E_PARAMETER*/ EMPTY_PRIM,
	/*E_ABS*/ 
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_abs},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
	 }},
	/*E_PLUS*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_add},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_STRING,GO_STRING,0},
			 (GelEvalFunc)string_concat},
		 {{GO_STRING,GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_STRING,0},
			 (GelEvalFunc)string_concat},
		 {{GO_VALUE|GO_POLYNOMIAL,GO_VALUE|GO_POLYNOMIAL,0},
			 (GelEvalFunc)polynomial_add_sub_op},
	 }},
	/*E_MINUS*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_sub},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_VALUE|GO_POLYNOMIAL,GO_VALUE|GO_POLYNOMIAL,0},
			 (GelEvalFunc)polynomial_add_sub_op},
	 }},
	/*E_MUL*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mul},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_mul_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
	 }},
	/*E_ELTMUL*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mul},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
	 }},
	/*E_DIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_div},
		 {{GO_MATRIX,GO_VALUE,0}, (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_VALUE,GO_MATRIX,0}, (GelEvalFunc)value_matrix_div_op},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_div_op},
	 }},
	/*E_ELTDIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_div},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
	 }},
	/*E_BACK_DIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_back_div},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_div_op},
	 }},
	/*E_ELT_BACK_DIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_back_div},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
	 }},
	/*E_MOD*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mod},
	 }},
	/*E_ELTMOD*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mod},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
	 }},
	/*E_NEG*/
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_neg},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
	 }},
	/*E_EXP*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_pow},
		 {{GO_MATRIX,GO_VALUE,0},(GelEvalFunc)matrix_pow_op},
	 }},
	/*E_ELTEXP*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_pow},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
	 }},
	/*E_FACT*/
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_fac},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
	 }},
	/*E_DBLFACT*/
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_dblfac},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
	 }},
	/*E_TRANSPOSE*/
	{{
		 {{GO_MATRIX,0,0},(GelEvalFunc)transpose_matrix},
	 }},
	/*E_CONJUGATE_TRANSPOSE*/
	{{
		 {{GO_MATRIX,0,0},(GelEvalFunc)conjugate_transpose_matrix},
	 }},
	/*E_IF_CONS*/ EMPTY_PRIM,
	/*E_IFELSE_CONS*/ EMPTY_PRIM,
	/*E_WHILE_CONS*/ EMPTY_PRIM,
	/*E_UNTIL_CONS*/ EMPTY_PRIM,
	/*E_DOWHILE_CONS*/ EMPTY_PRIM,
	/*E_DOUNTIL_CONS*/ EMPTY_PRIM,
	/*E_FOR_CONS*/ EMPTY_PRIM,
	/*E_FORBY_CONS*/ EMPTY_PRIM,
	/*E_FORIN_CONS*/ EMPTY_PRIM,
	/*E_SUM_CONS*/ EMPTY_PRIM,
	/*E_SUMBY_CONS*/ EMPTY_PRIM,
	/*E_SUMIN_CONS*/ EMPTY_PRIM,
	/*E_PROD_CONS*/ EMPTY_PRIM,
	/*E_PRODBY_CONS*/ EMPTY_PRIM,
	/*E_PRODIN_CONS*/ EMPTY_PRIM,
	/*E_EQ_CMP*/ EMPTY_PRIM,
	/*E_NE_CMP*/ EMPTY_PRIM,
	/*E_CMP_CMP*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)cmpcmpop},
		 {{GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_STRING,GO_STRING,0},
			 (GelEvalFunc)cmpstringop},
		 {{GO_STRING,GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_STRING,0},
			 (GelEvalFunc)cmpstringop},
	 }},
	/*E_LT_CMP*/ EMPTY_PRIM,
	/*E_GT_CMP*/ EMPTY_PRIM,
	/*E_LE_CMP*/ EMPTY_PRIM,
	/*E_GE_CMP*/ EMPTY_PRIM,
	/*E_LOGICAL_AND*/ EMPTY_PRIM,
	/*E_LOGICAL_OR*/ EMPTY_PRIM,
	/*E_LOGICAL_XOR*/
	{{
		 {{GO_VALUE|GO_STRING,GO_VALUE|GO_STRING,0},
			 (GelEvalFunc)logicalxorop},
	 }},
	/*E_LOGICAL_NOT*/
	{{
		 {{GO_VALUE|GO_STRING,0,0},(GelEvalFunc)logicalnotop},
	 }},
	/*E_REGION_SEP*/ EMPTY_PRIM,
	/*E_REGION_SEP_BY*/ EMPTY_PRIM,
	/*E_GET_VELEMENT*/ EMPTY_PRIM,
	/*E_GET_ELEMENT*/ EMPTY_PRIM,
	/*E_GET_ROW_REGION*/ EMPTY_PRIM,
	/*E_GET_COL_REGION*/ EMPTY_PRIM,
	/*E_QUOTE*/ EMPTY_PRIM,
	/*E_REFERENCE*/ EMPTY_PRIM,
	/*E_DEREFERENCE*/ EMPTY_PRIM,
	/*E_DIRECTCALL*/ EMPTY_PRIM,
	/*E_CALL*/ EMPTY_PRIM,
	/*E_RETURN*/ EMPTY_PRIM,
	/*E_BAILOUT*/ EMPTY_PRIM,
	/*E_EXCEPTION*/ EMPTY_PRIM,
	/*E_CONTINUE*/ EMPTY_PRIM,
	/*E_BREAK*/ EMPTY_PRIM,
	/*E_MOD_CALC*/ EMPTY_PRIM,
	/*E_OPER_LAST*/
};

#undef EMPTY_PRIM


/*pure free lists*/
static void
purge_free_lists(void)
{
	while(free_stack) {
		GelEvalStack *evs = free_stack;
		free_stack = free_stack->next;
		g_free(evs);
	}
	while(free_evl) {
		GelEvalLoop *evl = free_evl;
		free_evl = (GelEvalLoop *)free_evl->condition;
		g_free(evl);
	}
	while(free_evf) {
		GelEvalFor *evf = free_evf;
		free_evf = (GelEvalFor *)free_evf->body;
		g_free(evf);
	}
	while(free_evfi) {
		GelEvalForIn *evfi = free_evfi;
		free_evfi = (GelEvalForIn *)free_evfi->body;
		g_free(evfi);
	}
	while(free_trees) {
		GelETree *et = free_trees;
		free_trees = free_trees->any.next;
		g_free(et);
	}
}

static inline GelEvalLoop *
evl_new (GelETree *cond, GelETree *body, gboolean is_while, gboolean body_first)
{
	GelEvalLoop *evl;
	if(!free_evl) {
		evl = g_new(GelEvalLoop,1);
	} else {
		evl = free_evl;
		free_evl = (GelEvalLoop *)free_evl->condition;
	}
	evl->condition = cond;
	evl->body = body;
	evl->is_while = is_while ? 1 : 0;
	evl->body_first = body_first ? 1 : 0;
	return evl;
}

static inline void
evl_free(GelEvalLoop *evl)
{
#ifdef MEM_DEBUG_FRIENDLY
	memset (evl, 0, sizeof (GelEvalLoop));
	g_free (evl);
#else
	(GelEvalLoop *)evl->condition = free_evl;
	free_evl = evl;
#endif
}

static void
evl_free_with_cond(GelEvalLoop *evl)
{
	gel_freetree(evl->condition);
	evl_free (evl);
}

static inline GelEvalFor *
evf_new (GelEvalForType type,
	 mpw_ptr x, mpw_ptr to, mpw_ptr by, int init_cmp,
	 GelETree *body, GelETree *orig_body, GelToken *id)
{
	GelEvalFor *evf;
	if(!free_evf) {
		evf = g_new(GelEvalFor,1);
	} else {
		evf = free_evf;
		free_evf = (GelEvalFor *)free_evf->body;
	}
	evf->type = type;
	evf->x = x;
	evf->to = to;
	evf->by = by;
	evf->init_cmp = init_cmp;
	evf->result = NULL;
	evf->body = body;
	evf->orig_body = orig_body;
	evf->id = id;
	return evf;
}

static inline void
evf_free(GelEvalFor *evf)
{
#ifdef MEM_DEBUG_FRIENDLY
	g_free (evf);
#else
	(GelEvalFor *)evf->body = free_evf;
	free_evf = evf;
#endif
}

static inline GelEvalForIn *
evfi_new (GelEvalForType type, GelMatrixW *mat, GelETree *body, GelETree *orig_body, GelToken *id)
{
	GelEvalForIn *evfi;
	if(!free_evfi) {
		evfi = g_new(GelEvalForIn,1);
	} else {
		evfi = free_evfi;
		free_evfi = (GelEvalForIn *)free_evfi->body;
	}
	evfi->type = type;
	evfi->i = evfi->j = 0;
	evfi->mat = mat;
	evfi->result = NULL;
	evfi->body = body;
	evfi->orig_body = orig_body;
	evfi->id = id;
	return evfi;
}

static inline void
evfi_free(GelEvalForIn *evfi)
{
#ifdef MEM_DEBUG_FRIENDLY
	g_free (evfi);
#else
	(GelEvalForIn *)evfi->body = free_evfi;
	free_evfi = evfi;
#endif
}

static gboolean
iter_do_var(GelCtx *ctx, GelETree *n, GelEFunc *f)
{
	if(f->type == GEL_VARIABLE_FUNC) {
		D_ENSURE_USER_BODY (f);
		copyreplacenode(n,f->data.user);
	} else if(f->type == GEL_USER_FUNC) {
		D_ENSURE_USER_BODY (f);
		freetree_full(n,TRUE,FALSE);

		n->type = FUNCTION_NODE;
		/* FIXME: are we ok with passing the token as well? */
		n->func.func = d_makeufunc (f->id /* FIXME: does this need to be NULL */,
					    copynode (f->data.user),
					    g_slist_copy (f->named_args),
					    f->nargs,
					    f->extra_dict);
		n->func.func->context = -1;
		n->func.func->vararg = f->vararg;
		if (f->on_subst_list &&
		    d_curcontext () != 0)
			d_put_on_subst_list (n->func.func);
	} else if(f->type == GEL_BUILTIN_FUNC) {
		GelETree *ret;
		int exception = FALSE;

		if(f->nargs != 0) {
			freetree_full(n,TRUE,FALSE);
			n->type = FUNCTION_NODE;
			/* FIXME: are we ok with passing the token (f->id) as well? */
			n->func.func = d_makerealfunc(f,f->id,FALSE);
			n->func.func->context = -1;
			n->func.func->vararg = f->vararg;
			/* FIXME: no need for extra_dict right? */
			return TRUE;
		}
		ret = (*f->data.func)(ctx,NULL,&exception);
		if(exception) {
			if(ret)
				gel_freetree(ret);
			return FALSE;
		} else if(ret) {
			replacenode(n,ret);
		}
	} else if(f->type == GEL_REFERENCE_FUNC) {
		GelETree *i;
		f = f->data.ref;
		
		GET_NEW_NODE(i);
		i->type = IDENTIFIER_NODE;
		if(f->id) {
			i->id.id = f->id;
		} else {
			/*make up a new fake id*/
			GelToken *tok = g_new0(GelToken,1);
			tok->refs = g_slist_append(NULL,f);
			tok->curref = f;
			i->id.id = tok;
		}
		i->any.next = NULL;

		freetree_full(n,TRUE,FALSE);
		n->type = OPERATOR_NODE;
		n->op.oper = E_REFERENCE;

		n->op.args = i;
		n->op.nargs = 1;
	} else
		(*errorout)(_("Unevaluatable function type encountered!"));
	return TRUE;
}

static inline gboolean
iter_variableop(GelCtx *ctx, GelETree *n)
{
	GelEFunc *f;

	if (n->id.id->built_in_parameter) {
		GelETree *r = NULL;
		ParameterGetFunc getfunc = n->id.id->data2;
		if (getfunc != NULL)
			r = getfunc ();
		else
			r = gel_makenum_null ();
		replacenode (n, r);
		return TRUE;
	}
	
	f = d_lookup_global(n->id.id);
	if(!f) {
		char buf[256];
		g_snprintf(buf,256,_("Variable '%s' used uninitialized"),n->id.id->token);
		(*errorout)(buf);
		return TRUE;
	} else
		return iter_do_var(ctx,n,f);
}

static inline gboolean
iter_derefvarop(GelCtx *ctx, GelETree *n)
{
	GelEFunc *f;
	GelETree *l;
	
	GET_L(n,l);
	
	f = d_lookup_global(l->id.id);
	if(!f) {
		char buf[256];
		g_snprintf(buf,256,_("Variable '%s' used uninitialized"),
			   l->id.id->token);
		(*errorout)(buf);
	} else if(f->nargs != 0) {
		char buf[256];
		g_snprintf(buf,256,_("Call of '%s' with the wrong number of arguments!\n"
				     "(should be %d)"),f->id?f->id->token:"anonymous",f->nargs);
		(*errorout)(buf);
	} else if(f->type != GEL_REFERENCE_FUNC) {
		char buf[256];
		g_snprintf(buf,256,_("Trying to dereference '%s' which is not a reference!\n"),
			   f->id?f->id->token:"anonymous");
		(*errorout)(buf);
	} else /*if(f->type == GEL_REFERENCE_FUNC)*/ {
		f = f->data.ref;
		if(!f)
			(*errorout)(_("NULL reference encountered!"));
		else
			return iter_do_var(ctx,n,f);
	}
	return TRUE;
}

#define RET_RES(x) \
	freetree_full(n,TRUE,FALSE);	\
	gel_makenum_ui_from(n,x);		\
	return;

/*returns 0 if all numeric, 1 if numeric/matrix, 2 if contains string, 3 otherwise*/
static int
arglevel(GelETree *r,int cnt)
{
	int i;
	int level = 0;
	for(i=0;i<cnt;i++,r = r->any.next) {
		if(r->type!=VALUE_NODE) {
			if(r->type==MATRIX_NODE)
				level = level<1?1:level;
			else if(r->type==STRING_NODE)
				level = 2;
			else
				return 3;
		}
	}
	return level;
}

static void
evalcomp(GelETree *n)
{
	GSList *oli;
	GelETree *ali;

	for(ali=n->comp.args,oli=n->comp.comp;oli;ali=ali->any.next,oli=oli->next) {
		int oper = GPOINTER_TO_INT(oli->data);
		int err = FALSE;
		GelETree *l = ali,*r = ali->any.next;

		switch(arglevel(ali,2)) {
		case 0:
			switch(oper) {
			case E_EQ_CMP:
				if(!eqlnodes(l,r)) {
					if(error_num) {
						error_num=0;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_NE_CMP:
				if(eqlnodes(l,r)) {
					RET_RES(0)
				} else if(error_num) {
					error_num=0;
					return;
				}
				break;
			case E_LT_CMP:
				if(cmpnodes(l,r)>=0) {
					if(error_num) {
						error_num=0;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_GT_CMP:
				if(cmpnodes(l,r)<=0) {
					if(error_num) {
						error_num=0;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_LE_CMP:
				if(cmpnodes(l,r)>0) {
					RET_RES(0)
				} else if(error_num) {
					error_num=0;
					return;
				}
				break;
			case E_GE_CMP:
				if(cmpnodes(l,r)<0) {
					RET_RES(0)
				} else if(error_num) {
					error_num=0;
					return;
				}
				break;
			default:
				g_assert_not_reached();
			}
			break;
		case 1:
			switch(oper) {
			case E_EQ_CMP:
				if(!eqmatrix(l,r,&err)) {
					if(err) {
						error_num=0;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_NE_CMP:
				if(eqmatrix(l,r,&err)) {
					RET_RES(0)
				} else if(err) {
					error_num=0;
					return;
				}
				break;
			default:
				(*errorout)(_("Cannot compare matrixes"));
				{
					error_num=0;
					return;
				}
			}
			break;
		case 2:
			switch(oper) {
			case E_EQ_CMP:
				if(!eqstring(l,r)) {
					RET_RES(0)
				}
				break;
			case E_NE_CMP:
				if(eqstring(l,r)) {
					RET_RES(0)
				}
				break;
			case E_LT_CMP:
				if(cmpstring(l,r)>=0) {
					RET_RES(0)
				}
				break;
			case E_GT_CMP:
				if(cmpstring(l,r)<=0) {
					RET_RES(0)
				}
				break;
			case E_LE_CMP:
				if(cmpstring(l,r)>0) {
					RET_RES(0)
				}
				break;
			case E_GE_CMP:
				if(cmpstring(l,r)<0) {
					RET_RES(0)
				}
				break;
			default:
				g_assert_not_reached();
			}
			break;
		default:
			(*errorout)(_("Primitives must get numeric/matrix/string arguments"));
			{
				error_num=0;
				return;
			}
			break;
		}
	}
	RET_RES(1)
}

#undef RET_RES


/* free a special stack entry */
static inline void
ev_free_special_data(GelCtx *ctx, gpointer data, int flag)
{
	switch(flag) {
	case GE_FUNCCALL:
		/*we are crossing a boundary, we need to free a context*/
		d_popcontext ();
		gel_freetree(data);
		GE_BLIND_POP_STACK(ctx);
		break;
	case GE_LOOP_COND:
	case GE_LOOP_LOOP:
		{
			GelEvalLoop *evl = data;
			gel_freetree (evl->condition);
			gel_freetree (evl->body);
			evl_free (evl);
			GE_BLIND_POP_STACK (ctx);
		}
		break;
	case GE_FOR:
		{
			GelEvalFor *evf = data;
			gel_freetree(evf->body);
			gel_freetree(evf->result);
			evf_free(evf);
			GE_BLIND_POP_STACK(ctx);
		}
		break;
	case GE_FORIN:
		{
			GelEvalForIn *evfi = data;
			gel_freetree(evfi->body);
			gel_freetree(evfi->result);
			evfi_free(evfi);
			GE_BLIND_POP_STACK(ctx);
		}
		break;
	case GE_SETMODULO:
		if (ctx->modulo != NULL) {
			mpw_clear (ctx->modulo);
			g_free (ctx->modulo);
		}
		ctx->modulo = data;
		break;
	default:
		break;
	}
}

static gboolean
push_setmod (GelCtx *ctx, GelETree *n)
{
	GelETree *l, *r;

	GET_LR (n, l, r);

	if (r->type != VALUE_NODE ||
	    mpw_is_complex (r->val.value) ||
	    ! mpw_is_integer (r->val.value) ||
	    mpw_sgn (r->val.value) <= 0) {
		(*errorout)(_("Bad argument to modular operation"));
		return FALSE;
	}

	GE_PUSH_STACK (ctx, n, GE_POST);
	GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);

	ctx->modulo = g_new (struct _mpw_t, 1);
	mpw_init_set (ctx->modulo, r->val.value);

	ctx->post = FALSE;
	ctx->current = l;

	return TRUE;
}

static void
iter_pop_stack(GelCtx *ctx)
{
	gpointer data;
	int flag;
	
	for(;;) {
		GE_POP_STACK(ctx,data,flag);
		switch(flag) {
		case GE_EMPTY_STACK:
			EDEBUG("   POPPED AN EMPTY STACK");
			ctx->current = NULL;
			return;
		case GE_PRE:
			ctx->post = FALSE;
			ctx->current = data;
#ifdef EVAL_DEBUG
			printf("   POPPED A PRE NODE(%d)\n",ctx->current->type);
#endif
			return;
		case GE_POST:
			ctx->post = TRUE;
			ctx->current = data;
#ifdef EVAL_DEBUG
			printf("   POPPED A POST NODE(%d)\n",ctx->current->type);
#endif
			return;
		case GE_AND:
		case GE_OR:
			{
				GelETree *li = data;
				int ret;
				int bad_node = FALSE;
				EDEBUG("    POPPED AN OR or AND");
				ret = isnodetrue(li,&bad_node);
				if(bad_node || error_num) {
					EDEBUG("    AND/OR BAD BAD NODE");
					error_num = 0;
					GE_BLIND_POP_STACK(ctx);
					break;
				}
				if((flag==GE_AND && !ret) ||
				   (flag==GE_OR && ret)) {
					int n_flag;
					GE_PEEK_STACK(ctx,data,n_flag);
					g_assert(n_flag==GE_POST);
					freetree_full (data, TRUE, FALSE);
					if(flag==GE_AND)
						gel_makenum_ui_from(data,0);
					else
						gel_makenum_ui_from(data,1);
					EDEBUG("    AND/OR EARLY DONE");
					break;
				}
				li = li->any.next;
				if(!li) {
					int n_flag;
					GE_PEEK_STACK(ctx,data,n_flag);
					g_assert(n_flag==GE_POST);
					freetree_full (data, TRUE, FALSE);
					if(flag==GE_AND)
						gel_makenum_ui_from(data,1);
					else
						gel_makenum_ui_from(data,0);
					EDEBUG("    AND/OR ALL THE WAY DONE");
					break;
				}
				GE_PUSH_STACK(ctx,li,flag);
				ctx->post = FALSE;
				ctx->current = li;
				EDEBUG("    JUST PUT THE NEXT ONE");
				return;
			}
		case GE_FUNCCALL:
			{
				gpointer call;

				/*pop the context*/
				d_popcontext ();
				
				GE_POP_STACK(ctx,call,flag);

				/*replace the call with the result of
				  the function*/
				g_assert(call);
				if (ctx->modulo != NULL)
					mod_node (data, ctx->modulo);
				replacenode(call,data);
			}
			break;
		case GE_LOOP_COND:
			/*this was the condition of a while or until loop*/
			{
				GelEvalLoop *evl = data;
				GelETree *n;
				int ret,bad_node = FALSE;
				int n_flag;
				g_assert(evl->condition);

				/*next MUST be the original node*/
				GE_PEEK_STACK(ctx,n,n_flag);
				g_assert(n_flag==GE_POST);

				EDEBUG("    LOOP CONDITION CHECK");
				ret = isnodetrue(evl->condition,&bad_node);
				if(bad_node || error_num) {
					EDEBUG("    LOOP CONDITION BAD BAD NODE");
					error_num = 0;
					replacenode (n->op.args, evl->condition);
					gel_freetree (evl->body);
					evl_free (evl);
					GE_BLIND_POP_STACK(ctx);
					break;
				}
				/*check if we should continue the loop*/
				if((evl->is_while && ret) ||
				   (!evl->is_while && !ret)) {
					GelETree *l,*r;
					EDEBUG("    LOOP CONDITION MET");
					GET_LR(n,l,r);
					gel_freetree (evl->condition);
					evl->condition = NULL;
					gel_freetree (evl->body);
					if (evl->body_first)
						evl->body = copynode (l);
					else
						evl->body = copynode (r);
					ctx->current = evl->body;
					ctx->post = FALSE;
					GE_PUSH_STACK(ctx,evl,GE_LOOP_LOOP);
					return;
				} else {
					EDEBUG("    LOOP CONDITION NOT MET");
					/*condition not met, so return the body*/
					if (evl->body == NULL) {
						EDEBUG("     NULL BODY");
						freetree_full (n, TRUE, FALSE);
						n->type = NULL_NODE;
					} else {
						replacenode (n, evl->body);
					}
					gel_freetree (evl->condition);
					evl_free (evl);
					GE_BLIND_POP_STACK (ctx);
					break;
				}
			}
		case GE_LOOP_LOOP:
			{
				GelEvalLoop *evl = data;
				GelETree *n,*l,*r;
				int n_flag;
				g_assert(evl->body);

				/*next MUST be the original node*/
				GE_PEEK_STACK(ctx,n,n_flag);
				g_assert(n_flag==GE_POST);

				EDEBUG("    LOOP LOOP BODY FINISHED");

				GET_LR(n,l,r);
				gel_freetree (evl->condition);
				if (evl->body_first)
					evl->condition = copynode (r);
				else
					evl->condition = copynode (l);
				ctx->current = evl->condition;
				ctx->post = FALSE;
				GE_PUSH_STACK(ctx,evl,GE_LOOP_COND);
				return;
			}
		case GE_FOR:
			{
				GelEvalFor *evf = data;
				if(evf->by)
					mpw_add(evf->x,evf->x,evf->by);
				else
					mpw_add_ui(evf->x,evf->x,1);
				/*if done*/
				if(mpw_cmp(evf->x,evf->to) == -evf->init_cmp) {
					GelETree *res;
					GE_POP_STACK(ctx,data,flag);
					g_assert(flag==GE_POST);
					if (evf->type == GEL_EVAL_FOR) {
						res = evf->body;
						evf->body = NULL;
					} else if (evf->type == GEL_EVAL_SUM) {
						if (evf->result != NULL) {
							res = op_two_nodes (ctx,
									    evf->result,
									    evf->body,
									    E_PLUS,
									    TRUE /* no_push */);
							gel_freetree (evf->result);
							evf->result = NULL;
						} else {
							res = evf->body;
							evf->body = NULL;
						}
						gel_freetree (evf->body);
						evf->body = NULL;
					} else /* if (evf->type == GEL_EVAL_PROD) */ {
						if (evf->result != NULL) {
							res = op_two_nodes (ctx,
									    evf->result,
									    evf->body,
									    E_MUL,
									    TRUE /* no_push */);
							gel_freetree (evf->result);
							evf->result = NULL;
						} else {
							res = evf->body;
							evf->body = NULL;
						}
						gel_freetree (evf->body);
						evf->body = NULL;
					}
					if (res->type == VALUE_NODE) {
						replacenode (data, res);
						evf_free (evf);
						break;
					} else {
						replacenode (data, res);
						ctx->current = data;
						ctx->post = FALSE;
						evf_free (evf);
						return;
					}
				/*if we should continue*/
				} else {
					if (evf->type == GEL_EVAL_SUM) {
						if (evf->result != NULL) {
							GelETree *old = evf->result;
							evf->result =
								op_two_nodes (ctx,
									      old,
									      evf->body,
									      E_PLUS,
									      TRUE /* no_push */);
							gel_freetree (old);
						} else {
							evf->result = evf->body;
							evf->body = NULL;
						}
					} else if (evf->type == GEL_EVAL_PROD) {
						if (evf->result != NULL) {
							GelETree *old = evf->result;
							evf->result =
								op_two_nodes (ctx,
									      old,
									      evf->body,
									      E_MUL,
									      TRUE /* no_push */);
							gel_freetree (old);
						} else {
							evf->result = evf->body;
							evf->body = NULL;
						}
					}
					GE_PUSH_STACK (ctx, evf, GE_FOR);
					d_addfunc (d_makevfunc (evf->id,
								gel_makenum (evf->x)));
					if (evf->body != NULL)
						gel_freetree (evf->body);
					evf->body = copynode (evf->orig_body);
					ctx->current = evf->body;
					ctx->post = FALSE;
					return;
				}
			}
		case GE_FORIN:
			{
				GelEvalForIn *evfi = data;
				if(evfi->mat &&
				   (++evfi->i)>=gel_matrixw_width(evfi->mat)) {
					evfi->i=0;
					if((++evfi->j)>=gel_matrixw_height(evfi->mat))
						evfi->mat = NULL;
				}
				/*if we should continue*/
				if(evfi->mat) {
					if (evfi->type == GEL_EVAL_SUM) {
						if (evfi->result != NULL) {
							GelETree *old = evfi->result;
							evfi->result =
								op_two_nodes (ctx,
									      old,
									      evfi->body,
									      E_PLUS,
									      TRUE /* no_push */);
							gel_freetree (old);
						} else {
							evfi->result = evfi->body;
							evfi->body = NULL;
						}
					} else if (evfi->type == GEL_EVAL_PROD) {
						if (evfi->result != NULL) {
							GelETree *old = evfi->result;
							evfi->result =
								op_two_nodes (ctx,
									      old,
									      evfi->body,
									      E_MUL,
									      TRUE /* no_push */);
							gel_freetree (old);
						} else {
							evfi->result = evfi->body;
							evfi->body = NULL;
						}
					}
					GE_PUSH_STACK(ctx,evfi,GE_FORIN);
					d_addfunc(d_makevfunc(evfi->id,
					      copynode(gel_matrixw_index(evfi->mat,
							     evfi->i,evfi->j))));
					gel_freetree(evfi->body);
					evfi->body = copynode(evfi->orig_body);
					ctx->current = evfi->body;
					ctx->post = FALSE;
					return;
				/*if we are done*/
				} else {
					GelETree *res;
					GE_POP_STACK(ctx,data,flag);
					g_assert(flag==GE_POST);
					if (evfi->type == GEL_EVAL_FOR) {
						res = evfi->body;
						evfi->body = NULL;
					} else if (evfi->type == GEL_EVAL_SUM) {
						if (evfi->result != NULL) {
							res = op_two_nodes (ctx,
									    evfi->result,
									    evfi->body,
									    E_PLUS,
									    TRUE /* no_push */);
							gel_freetree (evfi->result);
							evfi->result = NULL;
						} else {
							res = evfi->body;
							evfi->body = NULL;
						}
						gel_freetree (evfi->body);
						evfi->body = NULL;
					} else /* if (evfi->type == GEL_EVAL_PROD) */ {
						if (evfi->result != NULL) {
							res = op_two_nodes (ctx,
									    evfi->result,
									    evfi->body,
									    E_MUL,
									    TRUE /* no_push */);
							gel_freetree (evfi->result);
							evfi->result = NULL;
						} else {
							res = evfi->body;
							evfi->body = NULL;
						}
						gel_freetree (evfi->body);
						evfi->body = NULL;
					}
					if (res->type == VALUE_NODE) {
						replacenode (data, res);
						evfi_free(evfi);
						break;
					} else {
						replacenode (data, res);
						ctx->current = data;
						ctx->post = FALSE;
						evfi_free(evfi);
						return;
					}
				}
			}
		case GE_MODULOOP:
			if (push_setmod (ctx, data))
				return;
			break;
		case GE_SETMODULO:
			if (ctx->modulo != NULL) {
				mpw_clear (ctx->modulo);
				g_free (ctx->modulo);
			}
			ctx->modulo = data;
			break;
		default:
			g_assert_not_reached();
			break;
		}
	}
}

/*make first argument the "current",
  go into "pre" mode and push all other ones, except
  that it doesn't push the last one, it assumes that
  there are at least two to push*/
static inline void
iter_push_args_no_last(GelCtx *ctx, GelETree *args)
{
	GelETree *li;
	ctx->post = FALSE;
	ctx->current = args;

	/* FIXME: this may (will!) be the wrong order I think */
	for(li=args->any.next;li->any.next;li=li->any.next) {
		GE_PUSH_STACK(ctx,li,GE_PRE);
	}
}

/*make first argument the "current",
  go into "pre" mode and push all other ones*/
static inline void
iter_push_args(GelCtx *ctx, GelETree *args)
{
	GelETree *li;
	ctx->post = FALSE;
	ctx->current = args;

	/* FIXME: this may (will!) be the wrong order I think */
	for(li=args->any.next;li;li=li->any.next) {
		GE_PUSH_STACK(ctx,li,GE_PRE);
	}
}

/*make first argument the "current",
 *and push all other args.  evaluate with no modulo. */
static inline void
iter_push_args_no_modulo (GelCtx *ctx, GelETree *args)
{
	GelETree *li;

	ctx->post = FALSE;
	ctx->current = args;
	if (ctx->modulo != NULL) {
		GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);

		/* Make modulo NULL */
		ctx->modulo = NULL;
	}

	/* FIXME: this may (will!) be the wrong order I think */
	for(li=args->any.next;li;li=li->any.next) {
		GE_PUSH_STACK(ctx,li,GE_PRE);
	}
}

/*make first argument the "current",
  push no modulo on the second argument
  go into "pre" mode and push all other ones*/
static inline void
iter_push_args_no_modulo_on_2 (GelCtx *ctx, GelETree *args)
{
	ctx->post = FALSE;
	ctx->current = args;
	if (ctx->modulo != NULL) {
		mpw_ptr ptr = g_new (struct _mpw_t, 1);
		mpw_init_set (ptr, ctx->modulo);

		GE_PUSH_STACK (ctx, ptr, GE_SETMODULO);
	}
	GE_PUSH_STACK(ctx, args->any.next, GE_PRE);
	g_assert (args->any.next->any.next == NULL);
	if (ctx->modulo != NULL) {
		GE_PUSH_STACK (ctx, NULL, GE_SETMODULO);
	}
}

static int
matrix_to_be_evaluated(GelMatrixW *m)
{
	int x,y;
	int w,h;
	GelETree *n;
	w = gel_matrixw_width(m);
	h = gel_matrixw_height(m);
	for(y=0;y<h;y++) {
		for(x=0;x<w;x++) {
			n = gel_matrixw_set_index(m,x,y);
			if(n &&
			   n->type != NULL_NODE &&
			   n->type != VALUE_NODE &&
			   n->type != STRING_NODE &&
			   n->type != USERTYPE_NODE)
				return TRUE;
		}
	}
	return FALSE;
}

/*when a matrix contains other things then NULLs, VALUEs, and STRINGs,
  make a copy of it and evaluate it's nodes*/
static inline void
iter_push_matrix(GelCtx *ctx, GelETree *n, GelMatrixW *m)
{
	int x,y;
	int w,h;
	int pushed = FALSE;
	GelETree *t;
	if(!matrix_to_be_evaluated(m)) {
		/*just put us in post mode, we will not eval anything*/
		ctx->post = TRUE;
		return;
	}

	/*make us a private copy!*/
	gel_matrixw_make_private(m);
	w = gel_matrixw_width(m);
	h = gel_matrixw_height(m);
	for(y=0;y<h;y++) {
		for(x=0;x<w;x++) {
			t = gel_matrixw_set_index(m,x,y);
			if(t) {
				if(!pushed) {
					GE_PUSH_STACK(ctx,n,GE_POST);
					ctx->post = FALSE;
					ctx->current = t;
					pushed = TRUE;
				} else {
					GE_PUSH_STACK(ctx,t,GE_PRE);
				}
			}
		}
	}
	/*if we haven't pushed ourselves, then just put us in post mode*/
	if(!pushed) {
		ctx->post = TRUE;
	}
}

static GelEFunc *
get_func_from_arg (GelCtx *ctx, GelETree *n, gboolean silent)
{
	GelEFunc *f;
	GelETree *l;

	GET_L(n,l);

	if(l->type == IDENTIFIER_NODE) {
		f = d_lookup_global(l->id.id);
		if(!f) {
			if ( ! silent) {
				char buf[256];
				g_snprintf(buf,256,_("Function '%s' used uninitialized"),
					   l->id.id->token);
				(*errorout)(buf);
			}
			return NULL;
		}
	} else if(l->type == FUNCTION_NODE) {
		f = l->func.func;
	} else if(l->type == OPERATOR_NODE &&
		l->op.oper == E_DEREFERENCE) {
		GelETree *ll;
		GET_L(l,ll);
		f = d_lookup_global(ll->id.id);
		if(!f) {
			if ( ! silent) {
				char buf[256];
				g_snprintf(buf,256,_("Variable '%s' used uninitialized"),
					   ll->id.id->token);
				(*errorout)(buf);
			}
			return NULL;
		} else if(f->type != GEL_REFERENCE_FUNC) {
			if ( ! silent) {
				char buf[256];
				g_snprintf(buf,256,_("Can't dereference '%s'!"),
					   ll->id.id->token);
				(*errorout)(buf);
			}
			return NULL;
		}
		f = f->data.ref;
	} else {
		if ( ! silent)
			(*errorout)(_("Can't call a non-function!"));
		return NULL;
	}
	return f;
}

static gboolean
iter_funccallop(GelCtx *ctx, GelETree *n)
{
	GelEFunc *f;
	
	EDEBUG("    FUNCCALL");

	f = get_func_from_arg (ctx, n, FALSE /* silent */);
	if (f == NULL)
		goto funccall_done_ok;
	
	g_assert(f);
	
	if ((f->vararg && f->nargs > n->op.nargs) ||
	    (! f->vararg && f->nargs != n->op.nargs - 1)) {
		char buf[256];
		if ( ! f->vararg)
			g_snprintf (buf, 256,
				    _("Call of '%s' with the wrong number of arguments!\n"
				      "(should be %d)"),
				    f->id != NULL ? f->id->token : "anonymous",
				    f->nargs);
		else
			g_snprintf (buf, 256,
				    _("Call of '%s' with the wrong number of arguments!\n"
				      "(should be greater then %d)"),
				    f->id != NULL ? f->id->token : "anonymous",
				    f->nargs-2);
		(*errorout)(buf);
	} else if(f->type == GEL_USER_FUNC ||
		  f->type == GEL_VARIABLE_FUNC) {
		GSList *li;
		GelETree *ali;
		GelToken *last_arg = NULL;

		EDEBUG("     USER FUNC PUSHING CONTEXT");

		d_addcontext();

		/* add extra dictionary stuff */
		for (li = f->extra_dict; li != NULL; li = li->next) {
			GelEFunc *func = d_copyfunc (li->data);
			func->context = d_curcontext ();
			d_addfunc (func);
		}

		/*push arguments on context stack*/
		li = f->named_args;
		for(ali = n->op.args->any.next;
		    ali != NULL;
		    ali = ali->any.next) {
			if (li->next == NULL) {
				last_arg = li->data;
				if (f->vararg)
					break;
			}
			if (ali->type == FUNCTION_NODE) {
				d_addfunc(d_makerealfunc(ali->func.func,li->data,FALSE));
			} else if(ali->type == OPERATOR_NODE &&
				  ali->op.oper == E_REFERENCE) {
				GelETree *t = ali->op.args;
				GelEFunc *rf = d_lookup_global_up1(t->id.id);
				if(!rf) {
					d_popcontext ();
					(*errorout)(_("Referencing an undefined variable!"));
					goto funccall_done_ok;
				}
				d_addfunc(d_makereffunc(li->data,rf));
			} else {
				d_addfunc(d_makevfunc(li->data,copynode(ali)));
			}
			li = li->next;
			if (li == NULL)
				break;
		}

		if (f->vararg) {
			if (last_arg == NULL) {
				li = g_slist_last (f->named_args);
				g_assert (li != NULL);
				last_arg = li->data;
			}
			/* no extra argument */
			if (n->op.nargs == f->nargs) {
				d_addfunc (d_makevfunc (last_arg, gel_makenum_null ()));
			} else {
				GelETree *nn;
				GelMatrix *m;
				int i;

				m = gel_matrix_new ();
				gel_matrix_set_size (m, n->op.nargs - f->nargs, 1, FALSE /* padding */);

				/* continue with ali */
				i = 0;
				for (; ali != NULL; ali = ali->any.next) {
					gel_matrix_index (m, i++, 0) = copynode (ali);
				}

				GET_NEW_NODE (nn);
				nn->type = MATRIX_NODE;
				nn->mat.quoted = FALSE;
				nn->mat.matrix = gel_matrixw_new_with_matrix (m);

				d_addfunc (d_makevfunc (last_arg, nn));
			}
		}

		D_ENSURE_USER_BODY (f);
		
		/*push self as post AGAIN*/
		GE_PUSH_STACK(ctx,ctx->current,GE_POST);

		/*the next to be evaluated is the body*/
		ctx->post = FALSE;
		ctx->current = copynode(f->data.user);
		/*printf("copying: %p\n", ctx->current);*/

		GE_PUSH_STACK(ctx,ctx->current,GE_FUNCCALL);

		/* push current modulo if we are not propagating it
		 * to the function */
		if ( ! f->propagate_mod &&
		    ctx->modulo != NULL) {
			GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);
			ctx->modulo = NULL;
		}

		/*exit without popping the stack as we don't want to do that*/
		return TRUE;
	} else if(f->type == GEL_BUILTIN_FUNC) {
		int exception = FALSE;
		GelETree *ret;
		mpw_ptr old_modulo;

		old_modulo = ctx->modulo;
		if ( ! f->propagate_mod) {
			ctx->modulo = NULL;
		}

		if (n->op.nargs > 1) {
			GelETree **r;
			GelETree *li;
			int i;
			r = g_new (GelETree *, n->op.nargs);
			for(i=0,li=n->op.args->any.next;li;i++,li=li->any.next)
				r[i] = li;
			r[i] = NULL;
			ret = (*f->data.func)(ctx,r,&exception);
			g_free (r);
		} else {
			ret = (*f->data.func)(ctx,NULL,&exception);
		}
		if ( ! f->propagate_mod) {
			g_assert (ctx->modulo == NULL);
			ctx->modulo = old_modulo;
		}
		if(exception) {
			if(ret)
				gel_freetree(ret);
			return FALSE;
		} else if(ret) {
			if (ctx->modulo != NULL)
				mod_node (ret, ctx->modulo);
			replacenode (n, ret);
		}
	} else if(f->type == GEL_REFERENCE_FUNC) {
		GelETree *id;
		if(f->nargs>0) {
			(*errorout)(_("Reference function with arguments encountered!"));
			goto funccall_done_ok;
		}
		f = f->data.ref;
		if(!f->id) {
			(*errorout)(_("Unnamed reference function encountered!"));
			goto funccall_done_ok;
		}
		
		GET_NEW_NODE(id);
		id->type = IDENTIFIER_NODE;
		id->id.id = f->id; /*this WILL have an id*/
		id->any.next = NULL;

		freetree_full(n,TRUE,FALSE);
		n->type = OPERATOR_NODE;
		n->op.oper = E_REFERENCE;

		n->op.args = id;
		n->op.nargs = 1;
	} else {
		(*errorout)(_("Unevaluatable function type encountered!"));
	}
funccall_done_ok:
	iter_pop_stack(ctx);
	return TRUE;
}

static inline void
iter_returnop(GelCtx *ctx, GelETree *n)
{
	GelETree *r;
	/*r was already evaluated*/
	/*now take it out of the argument list*/
	r = n->op.args;
	n->op.args = NULL;
	EDEBUG("  RETURN");
	for(;;) {
		int flag;
		gpointer data;
		GE_POP_STACK(ctx,data,flag);
		EDEBUG("   POPPED STACK");
		if(flag == GE_EMPTY_STACK) {
			EDEBUG("    EMPTY");
			break;
		} else if(flag == GE_FUNCCALL) {
			GelETree *fn;
			GE_POP_STACK(ctx,fn,flag);
			g_assert(fn);
			EDEBUG("    FOUND FUNCCCALL");
			gel_freetree(data);
			if (ctx->modulo != NULL)
				mod_node (r, ctx->modulo);
			replacenode(fn,r);

			d_popcontext ();

			iter_pop_stack(ctx);
			return;
		} else
			ev_free_special_data(ctx,data,flag);
	}
	EDEBUG("   GOT TO TOP OF THE STACK, SO JUST JUMP OUT OF GLOBAL CONTEXT");
	/*we were at the top so substitute result for
	  the return value*/
	ctx->current = NULL;
	ctx->post = FALSE;
	replacenode(ctx->res,r);
}

static inline void
iter_forloop (GelCtx *ctx, GelETree *n)
{
	GelEvalFor *evf;
	GelEvalForType type = GEL_EVAL_FOR;
	GelETree *from=NULL,*to=NULL,*by=NULL,*body=NULL,*ident=NULL;
	int init_cmp;

	switch (n->op.oper) {
	case E_FOR_CONS:
		type = GEL_EVAL_FOR;
		GET_ABCD(n,ident,from,to,body);
		break;
	case E_SUM_CONS:
		type = GEL_EVAL_SUM;
		GET_ABCD(n,ident,from,to,body);
		break;
	case E_PROD_CONS:
		type = GEL_EVAL_PROD;
		GET_ABCD(n,ident,from,to,body);
		break;
	case E_FORBY_CONS:
		type = GEL_EVAL_FOR;
		GET_ABCDE(n,ident,from,to,by,body);
		break;
	case E_SUMBY_CONS:
		type = GEL_EVAL_SUM;
		GET_ABCDE(n,ident,from,to,by,body);
		break;
	case E_PRODBY_CONS:
		type = GEL_EVAL_PROD;
		GET_ABCDE(n,ident,from,to,by,body);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	EDEBUG("   ITER FOR LOOP");

	if((by && (by->type != VALUE_NODE ||
		   mpw_is_complex(by->val.value))) ||
	   from->type != VALUE_NODE || mpw_is_complex(from->val.value) ||
	   to->type != VALUE_NODE || mpw_is_complex(to->val.value)) {
		(*errorout)(_("Bad type for 'for/sum/prod' loop!"));
		iter_pop_stack(ctx);
		return;
	}
	if(by && mpw_sgn(by->val.value)==0) {
		(*errorout)(_("'for/sum/prod' loop increment can't be 0"));
		iter_pop_stack(ctx);
		return;
	}
	
	init_cmp = mpw_cmp(from->val.value,to->val.value);
	
	/*if no iterations*/
	if(!by) {
		if(init_cmp>0) {
			d_addfunc(d_makevfunc(ident->id.id,copynode(from)));
			freetree_full(n,TRUE,FALSE);
			if (type == GEL_EVAL_FOR) {
				n->type = NULL_NODE;
			} else if (type == GEL_EVAL_SUM) {
				gel_makenum_ui_from (n, 0);
			} else /* if (type == GEL_EVAL_PROD) */ {
				gel_makenum_ui_from (n, 1);
			}
			iter_pop_stack(ctx);
			return;
		} else if(init_cmp==0) {
			init_cmp = -1;
		}
		evf = evf_new(type, from->val.value,to->val.value,NULL,init_cmp,
			      copynode(body),body,ident->id.id);
	} else {
		int sgn = mpw_sgn(by->val.value);
		if((sgn>0 && init_cmp>0) || (sgn<0 && init_cmp<0)) {
			d_addfunc(d_makevfunc(ident->id.id,copynode(from)));
			freetree_full(n,TRUE,FALSE);
			if (type == GEL_EVAL_FOR) {
				n->type = NULL_NODE;
			} else if (type == GEL_EVAL_SUM) {
				gel_makenum_ui_from (n, 0);
			} else /* if (type == GEL_EVAL_PROD) */ {
				gel_makenum_ui_from (n, 1);
			}
			iter_pop_stack(ctx);
			return;
		}
		if(init_cmp == 0)
			init_cmp = -sgn;
		evf = evf_new(type, from->val.value,to->val.value,by->val.value,
			      init_cmp,copynode(body),body,ident->id.id);
	}

	d_addfunc(d_makevfunc(ident->id.id,gel_makenum(evf->x)));
	
	GE_PUSH_STACK(ctx,n,GE_POST);
	GE_PUSH_STACK(ctx,evf,GE_FOR);
	
	ctx->current = evf->body;
	ctx->post = FALSE;
}

static inline void
iter_forinloop(GelCtx *ctx, GelETree *n)
{
	GelEvalForIn *evfi;
	GelEvalForType type = GEL_EVAL_FOR;
	GelETree *from,*body,*ident;

	switch (n->op.oper) {
	case E_FORIN_CONS:
		type = GEL_EVAL_FOR;
		break;
	case E_SUMIN_CONS:
		type = GEL_EVAL_SUM;
		break;
	case E_PRODIN_CONS:
		type = GEL_EVAL_PROD;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	GET_LRR(n,ident,from,body);
	
	EDEBUG("   ITER FORIN LOOP");

	/* If there is nothing to sum */
	if (from->type == NULL_NODE) {
		/* replace n with the appropriate nothingness */
		freetree_full (n, TRUE, FALSE);
		switch (n->op.oper) {
		case E_FORIN_CONS:
			n->type = NULL_NODE;
			break;
		case E_SUMIN_CONS:
			gel_makenum_ui_from (n, 0);
			break;
		case E_PRODIN_CONS:
			gel_makenum_ui_from (n, 1);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		iter_pop_stack (ctx);
		return;
	}

	if(from->type != VALUE_NODE &&
	   from->type != MATRIX_NODE) {
		(*errorout)(_("Bad type for 'for in' loop!"));
		iter_pop_stack(ctx);
		return;
	}
	
	if(from->type == MATRIX_NODE) {
		evfi = evfi_new (type, from->mat.matrix,
				 copynode (body), body, ident->id.id);
		d_addfunc(d_makevfunc(ident->id.id,
				      copynode(gel_matrixw_index(from->mat.matrix,
							     evfi->i,
							     evfi->j))));
	} else {
		evfi = evfi_new (type, NULL, copynode(body), body, ident->id.id);
		d_addfunc(d_makevfunc(ident->id.id,copynode(from)));
	}
	
	GE_PUSH_STACK(ctx,n,GE_POST);
	GE_PUSH_STACK(ctx,evfi,GE_FORIN);
	
	ctx->current = evfi->body;
	ctx->post = FALSE;
}

static inline void
iter_loop (GelCtx *ctx, GelETree *n, gboolean body_first, gboolean is_while)
{
	GelEvalLoop *evl;
	GelETree *l, *r;
	
	GET_LR(n,l,r);
	
	EDEBUG("   ITER LOOP");
	
	GE_PUSH_STACK (ctx, ctx->current, GE_POST);
	if (body_first) {
		EDEBUG ("    BODY FIRST");
		evl = evl_new (NULL, copynode (l), is_while, body_first);
		GE_PUSH_STACK (ctx, evl, GE_LOOP_LOOP);
		ctx->current = evl->body;
		ctx->post = FALSE;
	} else {
		EDEBUG("    CHECK FIRST");
		evl = evl_new (copynode(l), NULL, is_while, body_first);
		GE_PUSH_STACK (ctx, evl, GE_LOOP_COND);
		ctx->current = evl->condition;
		ctx->post = FALSE;
	}
}

static inline void
iter_ifop(GelCtx *ctx, GelETree *n, gboolean has_else)
{
	GelETree *l,*r,*rr = NULL;
	int ret;
	int bad_node = FALSE;
	
	EDEBUG("    IF/IFELSE ITER OP");
	
	if(has_else) {
		GET_LRR(n,l,r,rr);
	} else {
		GET_LR(n,l,r);
	}
	
	ret = isnodetrue(l,&bad_node);
	if(bad_node || error_num) {
		EDEBUG("    IF/IFELSE BAD BAD NODE");
		error_num = 0;
		iter_pop_stack(ctx);
		return;
	}
	
	if(ret) {
		EDEBUG("    IF TRUE EVAL BODY");
		/*remove from arglist so that it doesn't get freed on
		  replace node*/
		n->op.args->any.next = n->op.args->any.next->any.next;
		replacenode(n,r);
		ctx->post = FALSE;
		ctx->current = n;
	} else if(has_else) {
		EDEBUG("    IF FALSE EVAL ELSE BODY");
		/*remove from arglist so that it doesn't get freed on
		  replace node*/
		n->op.args->any.next->any.next = NULL;
		replacenode(n,rr);
		ctx->post = FALSE;
		ctx->current = n;
	} else {
		EDEBUG("    IF FALSE RETURN NULL");
		/*just return NULL*/
		freetree_full(n,TRUE,FALSE);
		n->type = NULL_NODE;
		iter_pop_stack(ctx);
	}
}

/*the breakout logic is almost identical for the different loops,
  but the code differs slightly so we just make a macro that subsitutes
  the right types, values and free functions*/
#define LOOP_BREAK_CONT(structtype,freefunc,pushflag) { \
	structtype *e = data;					\
	if(cont) {					\
		freetree_full(e->body,TRUE,FALSE);	\
		e->body->type = NULL_NODE;		\
		GE_PUSH_STACK(ctx,e,pushflag);		\
		/*we have already killed the body, so	\
		  this will continue as if the body	\
		  was evaluated to null*/		\
		iter_pop_stack(ctx);			\
	} else {					\
		GelETree *n;				\
		gel_freetree(e->body);			\
		freefunc(e);				\
							\
		/*pop loop call tree*/			\
		GE_POP_STACK(ctx,n,flag);		\
							\
		/*null the tree*/			\
		freetree_full(n,TRUE,FALSE);		\
		n->type = NULL_NODE;			\
							\
		/*go on with the computation*/		\
		iter_pop_stack(ctx);			\
	}						\
	return;						\
}

static inline void
iter_continue_break_op(GelCtx *ctx, gboolean cont)
{
	EDEBUG("  CONTINUE/BREAK");
	for(;;) {
		int flag;
		gpointer data;
		GE_POP_STACK(ctx,data,flag);
		EDEBUG("   POPPED STACK");
		switch(flag) {
		case GE_EMPTY_STACK:
			EDEBUG("    EMPTY");
			goto iter_continue_break_done;
		case GE_FUNCCALL:
			EDEBUG("    FOUND FUNCCCALL MAKE IT NULL THEN");
			(*errorout)(_("Continue or break outside a loop, "
				      "assuming \"return null\""));
			gel_freetree(data);

			d_popcontext ();

			/*pop the function call*/
			GE_POP_STACK(ctx,data,flag);

			g_assert(flag == GE_POST);
			freetree_full(data,TRUE,FALSE);
			((GelETree *)data)->type = NULL_NODE;

			iter_pop_stack(ctx);
			return;
		case GE_LOOP_LOOP:
			LOOP_BREAK_CONT (GelEvalLoop, evl_free_with_cond, GE_LOOP_LOOP);
		case GE_FOR:
			LOOP_BREAK_CONT (GelEvalFor, evf_free, GE_FOR);
		case GE_FORIN:
			LOOP_BREAK_CONT (GelEvalForIn, evfi_free, GE_FORIN);
		default:
			ev_free_special_data(ctx,data,flag);
			break;
		}
	}
iter_continue_break_done:
	EDEBUG("   GOT TO TOP OF THE STACK, SO JUST JUMP OUT OF GLOBAL CONTEXT");
	(*errorout)(_("Continue or break outside a loop, "
		      "assuming \"return null\""));
	/*we were at the top so substitute result for a NULL*/
	ctx->current = NULL;
	ctx->post = FALSE;
	freetree_full(ctx->res,TRUE,FALSE);
	ctx->res->type = NULL_NODE;
}

#undef LOOP_BREAK_CONT

static inline void
iter_bailout_op(GelCtx *ctx)
{
	EDEBUG("  BAILOUT");
	for(;;) {
		int flag;
		gpointer data;
		GE_POP_STACK(ctx,data,flag);
		EDEBUG("   POPPED STACK");
		if(flag == GE_EMPTY_STACK) {
			EDEBUG("    EMPTY");
			break;
		} else if(flag == GE_FUNCCALL) {
			EDEBUG("    FOUND FUNCCCALL");
			gel_freetree(data);

			d_popcontext ();

			/*pop the function call off the stack*/
			GE_BLIND_POP_STACK(ctx);

			iter_pop_stack(ctx);
			return;
		} else
			ev_free_special_data(ctx,data,flag);
	}
	EDEBUG("   GOT TO TOP OF THE STACK, SO JUST JUMP OUT OF GLOBAL CONTEXT");
	/*we were at the top so substitute result for
	  the return value*/
	ctx->current = NULL;
	ctx->post = FALSE;
}

static int
iter_get_ui_index (GelETree *num)
{
	long i;
	if(num->type != VALUE_NODE ||
	   !mpw_is_integer(num->val.value)) {
		(*errorout)(_("Wrong argument type as matrix index"));
		return -1;
	}

	i = mpw_get_long(num->val.value);
	if(error_num) {
		error_num = 0;
		return -1;
	}
	if(i>INT_MAX) {
		(*errorout)(_("Matrix index too large"));
		return -1;
	} else if(i<=0) {
		(*errorout)(_("Matrix index less than 1"));
		return -1;
	}
	return i;
}

static int *
iter_get_matrix_index_vector (GelETree *index, int maxsize, int *vlen)
{
	int i;
	int reglen = gel_matrixw_elements (index->mat.matrix);
	int *reg = g_new (int, reglen);

	*vlen = reglen;

	for (i = 0; i < reglen; i++) {
		GelETree *it = gel_matrixw_vindex (index->mat.matrix, i);
		reg[i] = iter_get_ui_index (it) - 1;
		if (reg[i] < 0) {
			g_free (reg);
			return NULL;
		} else if (reg[i] >= maxsize) {
			g_free (reg);
			(*errorout)(_("Matrix index out of range"));
			return NULL;
		}
	}
	return reg;
}

/* assumes index->type == VALUE_NODE */
static inline int
iter_get_matrix_index_num (GelETree *index, int maxsize)
{
	int i = iter_get_ui_index (index) - 1;
	if (i < 0) {
		return -1;
	} else if (i >= maxsize) {
		(*errorout)(_("Matrix index out of range"));
		return -1;
	}
	return i;
}

static gboolean
iter_get_index_region (GelETree *index, int maxsize, int **reg, int *l)
{
	if (index->type == VALUE_NODE) {
		int i = iter_get_matrix_index_num (index, maxsize);
		if (i < 0)
			return FALSE;
		*reg = g_new (int, 1);
		(*reg)[0] = i;
		*l = 1;
	} else /* MATRIX_NODE */ {
		*reg = iter_get_matrix_index_vector (index, maxsize, l);
		if (*reg == NULL)
			return FALSE;
	}
	return TRUE;
}

/* correct types already (value or matrix) */
static gboolean
iter_get_index_regions (GelETree *i1, GelETree *i2,
			int max1, int max2,
			int **reg1, int **reg2,
			int *l1, int *l2)
{
	if ( ! iter_get_index_region (i1, max1, reg1, l1))
		return FALSE;
	if ( ! iter_get_index_region (i2, max2, reg2, l2))
		return FALSE;
	return TRUE;
}

static GelMatrixW *
iter_get_matrix_p(GelETree *m, gboolean *new_matrix)
{
	GelMatrixW *mat = NULL;
	
	if(m->type == IDENTIFIER_NODE) {
		GelEFunc *f;
		if(d_curcontext()==0 &&
		   m->id.id->protected) {
			char *err = g_strdup_printf
				(_("Trying to set a protected id '%s'"),
				 m->id.id->token);
			(*errorout)(err);
			g_free (err);
			return NULL;
		}
		f = d_lookup_local(m->id.id);
		if(!f) {
			GelETree *t;
			GET_NEW_NODE(t);
			t->type = MATRIX_NODE;
			t->mat.matrix = gel_matrixw_new();
			t->mat.quoted = 0;
			gel_matrixw_set_size(t->mat.matrix,1,1);

			f = d_makevfunc(m->id.id,t);
			d_addfunc(f);
			if(new_matrix) *new_matrix = TRUE;
		} else if(f->type != GEL_USER_FUNC &&
			  f->type != GEL_VARIABLE_FUNC) {
			(*errorout)(_("Indexed Lvalue not user function"));
			return NULL;
		}
		D_ENSURE_USER_BODY (f);
		if(f->data.user->type != MATRIX_NODE) {
			GelETree *t;
			GET_NEW_NODE(t);
			t->type = MATRIX_NODE;
			t->mat.matrix = gel_matrixw_new();
			t->mat.quoted = 0;
			gel_matrixw_set_size(t->mat.matrix,1,1);

			d_set_value(f,t);
			if(new_matrix) *new_matrix = TRUE;
		}
		mat = f->data.user->mat.matrix;
	} else if(m->type == OPERATOR_NODE ||
		  m->op.oper == E_DEREFERENCE) {
		GelETree *l;
		GelEFunc *f;
		GET_L(m,l);

		if(l->type != IDENTIFIER_NODE) {
			(*errorout)(_("Dereference of non-identifier!"));
			return NULL;
		}

		f = d_lookup_local(l->id.id);
		if(!f) {
			(*errorout)(_("Dereference of undefined variable!"));
			return NULL;
		}
		if(f->type != GEL_REFERENCE_FUNC) {
			(*errorout)(_("Dereference of non-reference!"));
			return NULL;
		}

		if(f->data.ref->type != GEL_USER_FUNC &&
		   f->data.ref->type != GEL_VARIABLE_FUNC) {
			(*errorout)(_("Indexed Lvalue not user function"));
			return NULL;
		}
		if(f->data.ref->context==0 && f->data.ref->id->protected) {
			char *err = g_strdup_printf
				(_("Trying to set a protected id '%s'"),
				 f->data.ref->id->token);
			(*errorout)(err);
			g_free (err);
			return NULL;
		}
		D_ENSURE_USER_BODY (f->data.ref);
		if(f->data.ref->data.user->type != MATRIX_NODE) {
			GelETree *t;
			GET_NEW_NODE(t);
			t->type = MATRIX_NODE;
			t->mat.matrix = gel_matrixw_new();
			t->mat.quoted = 0;
			gel_matrixw_set_size(t->mat.matrix,1,1);

			d_set_value(f->data.ref,t);
			if(new_matrix) *new_matrix = TRUE;
		}
		mat = f->data.ref->data.user->mat.matrix;
	} else {
		(*errorout)(_("Indexed Lvalue not an identifier or a dereference"));
		return NULL;
	}
	return mat;
}

static GelETree *
set_parameter (GelToken *token, GelETree *val)
{
	GelEFunc *func;

	if (token->built_in_parameter) {
		ParameterSetFunc setfunc = token->data1;
		if (setfunc != NULL)
			return setfunc (val);
		return gel_makenum_null ();
	} else {
		func = d_makevfunc (token, copynode (val));
		/* make function global */
		func->context = 0;
		d_addfunc_global (func);
		return copynode (val);
	}
}

static void
iter_equalsop(GelETree *n)
{
	GelETree *l,*r;

	GET_LR(n,l,r);
	
	if(l->type != IDENTIFIER_NODE &&
	   !(l->type == OPERATOR_NODE && l->op.oper == E_GET_VELEMENT) &&
	   !(l->type == OPERATOR_NODE && l->op.oper == E_GET_ELEMENT) &&
	   !(l->type == OPERATOR_NODE && l->op.oper == E_GET_COL_REGION) &&
	   !(l->type == OPERATOR_NODE && l->op.oper == E_GET_ROW_REGION) &&
	   !(l->type == OPERATOR_NODE && l->op.oper == E_DEREFERENCE)) {
		(*errorout)(_("Lvalue not an identifier/dereference/matrix location!"));
		return;
	}

	if(l->type == IDENTIFIER_NODE) {
		if(d_curcontext()==0 && l->id.id->protected) {
			char *err = g_strdup_printf
				(_("Trying to set a protected id '%s'"),
				 l->id.id->token);
			(*errorout)(err);
			g_free (err);
			return;
		}
		if (l->id.id->parameter) {
			GelETree *ret = set_parameter (l->id.id, r);
			if (ret != NULL)
				replacenode (n, ret);
			return;
		} else if(r->type == FUNCTION_NODE) {
			d_addfunc(d_makerealfunc(r->func.func,l->id.id,FALSE));
		} else if(r->type == OPERATOR_NODE &&
			  r->op.oper == E_REFERENCE) {
			GelETree *t = r->op.args;
			GelEFunc *rf = d_lookup_global(t->id.id);
			if(!rf) {
				(*errorout)(_("Referencing an undefined variable!"));
				return;
			}
			d_addfunc(d_makereffunc(l->id.id,rf));
		} else {
			d_addfunc(d_makevfunc(l->id.id,copynode(r)));
		}
	} else if(l->op.oper == E_DEREFERENCE) {
		GelEFunc *f;
		GelETree *ll;
		GET_L(l,ll);

		if(ll->type != IDENTIFIER_NODE) {
			(*errorout)(_("Dereference of non-identifier!"));
			return;
		}
		
		f = d_lookup_local(ll->id.id);
		if(!f) {
			(*errorout)(_("Dereference of undefined variable!"));
			return;
		}
		if(f->type!=GEL_REFERENCE_FUNC) {
			(*errorout)(_("Dereference of non-reference!"));
			return;
		}

		if(f->data.ref->context==0 && f->data.ref->id->protected) {
			char *err = g_strdup_printf
				(_("Trying to set a protected id '%s'"),
				 f->data.ref->id->token);
			(*errorout)(err);
			g_free (err);
			return;
		}
		
		if(r->type == FUNCTION_NODE) {
			d_setrealfunc(f->data.ref,r->func.func,FALSE);
		} else if(r->type == OPERATOR_NODE &&
			  r->op.oper == E_REFERENCE) {
			GelETree *t = r->op.args;
			GelEFunc *rf = d_lookup_global(t->id.id);
			if(!rf) {
				(*errorout)(_("Referencing an undefined variable!"));
				return;
			}
			d_set_ref(f->data.ref,rf);
		} else {
			d_set_value(f->data.ref,copynode(r));
		}
	} else if(l->op.oper == E_GET_ELEMENT) {
		GelMatrixW *mat;
		GelETree *m, *index1, *index2;
		GET_LRR (l, m, index1, index2);

		if (index1->type == VALUE_NODE &&
		    index2->type == VALUE_NODE) {
			int x, y;

			x = iter_get_matrix_index_num (index2, INT_MAX);
			if (x < 0)
				return;
			y = iter_get_matrix_index_num (index1, INT_MAX);
			if (y < 0)
				return;

			mat = iter_get_matrix_p (l->op.args, NULL);
			if (mat == NULL)
				return;

			gel_matrixw_set_element (mat, x, y, copynode (r));
		} else if ((index1->type == VALUE_NODE ||
			    index1->type == MATRIX_NODE) &&
			   (index2->type == VALUE_NODE ||
			    index2->type == MATRIX_NODE)) {
			int *regx, *regy;
			int lx, ly;

			if ( ! iter_get_index_regions (index1, index2,
						       INT_MAX, INT_MAX,
						       &regy, &regx,
						       &ly, &lx))
				return;

			if (r->type == MATRIX_NODE &&
			    (gel_matrixw_width (r->mat.matrix) != lx ||
			     gel_matrixw_height (r->mat.matrix) != ly)) {
				g_free (regx);
				g_free (regy);
				(*errorout)(_("Wrong matrix dimensions when setting"));
				return;
			}

			mat = iter_get_matrix_p (l->op.args, NULL);
			if (mat == NULL) {
				g_free (regx);
				g_free (regy);
				return;
			}

			if (r->type == MATRIX_NODE)
				gel_matrixw_set_region (mat, r->mat.matrix, regx, regy, lx, ly);
			else
				gel_matrixw_set_region_etree (mat, r, regx, regy, lx, ly);
			g_free (regx);
			g_free (regy);
		} else {
			(*errorout)(_("Matrix index not an integer or a vector"));
			return;
		}
	} else if(l->op.oper == E_GET_VELEMENT) {
		GelMatrixW *mat;
		GelETree *m, *index;
		GET_LR (l, m, index);

		if (index->type == VALUE_NODE) {
			int i;

			i = iter_get_matrix_index_num (index, INT_MAX);
			if (i < 0)
				return;

			mat = iter_get_matrix_p (l->op.args, NULL);
			if (mat == NULL)
				return;

			gel_matrixw_set_velement (mat, i, copynode (r));
		} else if (index->type == MATRIX_NODE) {
			int *reg;
			int len;

			if ( ! iter_get_index_region (index, INT_MAX,
						      &reg, &len))
				return;

			mat = iter_get_matrix_p (l->op.args, NULL);
			if (mat == NULL) {
				g_free (reg);
				return;
			}

			if (r->type == MATRIX_NODE)
				gel_matrixw_set_vregion (mat, r->mat.matrix, reg, len);
			else
				gel_matrixw_set_vregion_etree (mat, r, reg, len);
			g_free (reg);
		} else {
			(*errorout)(_("Matrix index not an integer or a vector"));
			return;
		}
	} else /*l->data.oper == E_GET_COL_REGION E_GET_ROW_REGION*/ {
		GelMatrixW *mat;
		GelETree *m, *index;
		GET_LR (l, m, index);

		if (index->type == VALUE_NODE ||
		    index->type == MATRIX_NODE) {
			int *regx, *regy;
			int lx, ly;
			int i;

			if (l->op.oper == E_GET_COL_REGION) {
				if ( ! iter_get_index_region (index, INT_MAX, &regx, &lx))
					return;
				if (r->type == MATRIX_NODE &&
				    gel_matrixw_width (r->mat.matrix) != lx) {
					g_free (regx);
					(*errorout)(_("Wrong matrix dimensions when setting"));
					return;
				}
			} else {
				if ( ! iter_get_index_region (index, INT_MAX, &regy, &ly))
					return;
				if (r->type == MATRIX_NODE &&
				    gel_matrixw_height (r->mat.matrix) != ly) {
					g_free (regy);
					(*errorout)(_("Wrong matrix dimensions when setting"));
					return;
				}
			}

			mat = iter_get_matrix_p (l->op.args, NULL);
			if (mat == NULL) {
				g_free (regx);
				g_free (regy);
				return;
			}

			if (l->op.oper == E_GET_COL_REGION) {
				ly = gel_matrixw_height (mat);
				if (r->type == MATRIX_NODE &&
				    ly < gel_matrixw_height (r->mat.matrix))
					ly = gel_matrixw_height (r->mat.matrix);
				regy = g_new (int, ly);
				for (i = 0; i < ly; i++)
					regy[i] = i;
			} else {
				lx = gel_matrixw_width (mat);
				if (r->type == MATRIX_NODE &&
				    lx < gel_matrixw_width (r->mat.matrix))
					lx = gel_matrixw_width (r->mat.matrix);
				regx = g_new (int, lx);
				for (i = 0; i < lx; i++)
					regx[i] = i;
			}

			if (r->type == MATRIX_NODE)
				gel_matrixw_set_region (mat, r->mat.matrix, regx, regy, lx, ly);
			else
				gel_matrixw_set_region_etree (mat, r, regx, regy, lx, ly);
			g_free (regx);
			g_free (regy);
		} else {
			(*errorout)(_("Matrix index not an integer or a vector"));
			return;
		}
	}
	/*remove from arglist so that it doesn't get freed on replacenode*/
	n->op.args->any.next = NULL;
	replacenode(n,r);
}

static void
iter_parameterop (GelETree *n)
{
	GelETree *l,*r,*rr;

	GET_LRR (n, l, r, rr);

	/* FIXME: l should be the set func */
	
	g_assert (r->type == IDENTIFIER_NODE);

	if (d_curcontext() != 0) {
		(*errorout)(_("Parameters can only be created in the global context"));
		return;
	}
	
	d_addfunc (d_makevfunc (r->id.id, copynode (rr)));
	r->id.id->parameter = 1;

	/*remove from arglist so that it doesn't get freed on replacenode*/
	n->op.args->any.next->any.next = NULL;
	replacenode (n, rr);
}

static inline void
iter_push_indexes_and_arg(GelCtx *ctx, GelETree *n)
{
	GelETree *l,*ident;

	GET_L(n,l);
	
	if (l->op.oper == E_GET_ELEMENT) {
		GelETree *ll,*rr;
		
		GET_LRR(l,ident,ll,rr);

		GE_PUSH_STACK(ctx,n->op.args->any.next,GE_PRE);
		GE_PUSH_STACK(ctx,rr,GE_PRE);
		ctx->post = FALSE;
		ctx->current = ll;
	} else if(l->op.oper == E_GET_VELEMENT ||
		  l->op.oper == E_GET_COL_REGION ||
		  l->op.oper == E_GET_ROW_REGION) {
		GelETree *ll;
		
		GET_LR(l,ident,ll);

		GE_PUSH_STACK(ctx,n->op.args->any.next,GE_PRE);
		ctx->post = FALSE;
		ctx->current = ll;
	} else {
		ctx->post = FALSE;
		ctx->current = n->op.args->any.next;
	}
}

static void
iter_get_velement (GelETree *n)
{
	GelETree *m;
	GelETree *index;

	GET_LR (n, m, index);

	if (m->type != MATRIX_NODE) {
		(*errorout)(_("Index works only on matricies"));
		return;
	}

	if (index->type == VALUE_NODE) {
		GelETree *t;
		int i = iter_get_matrix_index_num (index, gel_matrixw_elements (m->mat.matrix));
		if (i < 0)
			return;
		t = copynode (gel_matrixw_vindex (m->mat.matrix, i));
		replacenode (n, t);
	} else if (index->type == MATRIX_NODE) {
		GelMatrixW *vec;
		int matsize = gel_matrixw_elements (m->mat.matrix);
		int quoted = m->mat.quoted;
		int *reg;
		int reglen;

		reg = iter_get_matrix_index_vector (index, matsize, &reglen);
		if (reg == NULL)
			return;

		vec = gel_matrixw_get_vregion (m->mat.matrix, reg, reglen);
		g_free (reg);

		freetree_full (n, TRUE /* freeargs */, FALSE /* kill */);
		n->type = MATRIX_NODE;
		n->mat.matrix = vec;
		n->mat.quoted = quoted;
	} else {
		(*errorout)(_("Vector index not an integer or a vector"));
	}
}

static void
iter_get_element (GelETree *n)
{
	GelETree *m, *index1, *index2;

	GET_LRR (n, m, index1, index2);

	if(m->type != MATRIX_NODE) {
		(*errorout)(_("Index works only on matricies"));
		return;
	}

	if (index1->type == VALUE_NODE &&
	    index2->type == VALUE_NODE) {
		int x, y;
		GelETree *t;

		x = iter_get_matrix_index_num (index2, gel_matrixw_width (m->mat.matrix));
		if (x < 0)
			return;
		y = iter_get_matrix_index_num (index1, gel_matrixw_height (m->mat.matrix));
		if (y < 0)
			return;

		/* make sure we don't free the args just yet */
		n->op.args = NULL;

		/* we will free this matrix in just a little bit */
		t = gel_matrixw_set_index (m->mat.matrix, x, y);
		if (m->mat.matrix->m->use == 1 && t != NULL) {
			replacenode (n, t);
			gel_matrixw_set_index (m->mat.matrix, x, y) = NULL;
		} else if (t == NULL) {
			freetree_full (n, FALSE /* freeargs */, FALSE /* kill */);
			gel_makenum_ui_from (n, 0);
		} else {
			replacenode (n, copynode (t));
		}

		/* free the args now */
		gel_freetree (m);
		gel_freetree (index1);
		gel_freetree (index2);
	} else if ((index1->type == VALUE_NODE ||
		    index1->type == MATRIX_NODE) &&
		   (index2->type == VALUE_NODE ||
		    index2->type == MATRIX_NODE)) {
		GelMatrixW *mat;
		int *regx, *regy;
		int lx, ly;
		int maxx, maxy;
		int quoted = m->mat.quoted;

		maxx = gel_matrixw_width (m->mat.matrix);
		maxy = gel_matrixw_height (m->mat.matrix);

		if ( ! iter_get_index_regions (index1, index2,
					       maxy, maxx,
					       &regy, &regx,
					       &ly, &lx))
			return;

		mat = gel_matrixw_get_region (m->mat.matrix, regx, regy, lx, ly);
		g_free (regx);
		g_free (regy);

		freetree_full (n, TRUE /* freeargs */, FALSE /* kill */);
		n->type = MATRIX_NODE;
		n->mat.matrix = mat;
		n->mat.quoted = quoted;
	} else {
		(*errorout)(_("Matrix index not an integer or a vector"));
	}
}

static void
iter_get_region (GelETree *n, gboolean col)
{
	GelETree *m, *index;

	GET_LR (n, m, index);

	if(m->type != MATRIX_NODE) {
		(*errorout)(_("Index works only on matricies"));
		return;
	}

	if (index->type == VALUE_NODE ||
	    index->type == MATRIX_NODE) {
		GelMatrixW *mat;
		int *regx, *regy;
		int lx, ly;
		int i;
		int maxx, maxy;
		int quoted = m->mat.quoted;

		maxx = gel_matrixw_width (m->mat.matrix);
		maxy = gel_matrixw_height (m->mat.matrix);

		if (col) {
			if ( ! iter_get_index_region (index, maxx, &regx, &lx))
				return;
			regy = g_new (int, maxy);
			for (i = 0; i < maxy; i++)
				regy[i] = i;
			ly = maxy;
		} else {
			if ( ! iter_get_index_region (index, maxy, &regy, &ly))
				return;
			regx = g_new (int, maxx);
			for (i = 0; i < maxx; i++)
				regx[i] = i;
			lx = maxx;
		}

		mat = gel_matrixw_get_region (m->mat.matrix, regx, regy, lx, ly);
		g_free (regx);
		g_free (regy);

		freetree_full (n, TRUE /* freeargs */, FALSE /* kill */);
		n->type = MATRIX_NODE;
		n->mat.matrix = mat;
		n->mat.quoted = quoted;
	} else {
		(*errorout)(_("Matrix index not an integer or a vector"));
	}
}

static inline guint32
iter_get_arg(GelETree *n)
{
	switch(n->type) {
	case VALUE_NODE: return GO_VALUE;
	case MATRIX_NODE: return GO_MATRIX;
	case STRING_NODE: return GO_STRING;
	case FUNCTION_NODE: return GO_FUNCTION;
	default: return 0;
	}
}

static char *
iter_get_arg_name(guint32 arg)
{
	static char *value = N_("number");
	static char *matrix = N_("matrix");
	static char *string = N_("string");
	static char *function = N_("function");
	switch(arg) {
	case GO_VALUE: return gettext(value);
	case GO_MATRIX: return gettext(matrix);
	case GO_STRING: return gettext(string);
	case GO_FUNCTION: return gettext(function);
	default:
		g_assert_not_reached();
		return NULL;
	}
}

static char *
iter_get_op_name(int oper)
{
	static char *name = NULL;
	g_free(name);
	name = NULL;

	switch(oper) {
	case E_SEPAR:
	case E_EQUALS:
	case E_PARAMETER: break;
	case E_ABS: name = g_strdup(_("Absolute value")); break;
	case E_PLUS: name = g_strdup(_("Addition")); break;
	case E_MINUS: name = g_strdup(_("Subtraction")); break;
	case E_MUL: name = g_strdup(_("Multiplication")); break;
	case E_ELTMUL: name = g_strdup(_("Element by element multiplication")); break;
	case E_DIV: name = g_strdup(_("Division")); break;
	case E_ELTDIV: name = g_strdup(_("Element by element division")); break;
	case E_BACK_DIV: name = g_strdup(_("Back division")); break;
	case E_ELT_BACK_DIV: name = g_strdup(_("Element by element back division")); break;
	case E_MOD: name = g_strdup(_("Modulo")); break;
	case E_ELTMOD: name = g_strdup(_("Element by element modulo")); break;
	case E_NEG: name = g_strdup(_("Negation")); break;
	case E_EXP: name = g_strdup(_("Power")); break;
	case E_ELTEXP: name = g_strdup(_("Element by element power")); break;
	case E_FACT: name = g_strdup(_("Factorial")); break;
	case E_DBLFACT: name = g_strdup(_("Double factorial")); break;
	case E_TRANSPOSE: name = g_strdup(_("Transpose")); break;
	case E_CONJUGATE_TRANSPOSE: name = g_strdup(_("ConjugateTranspose")); break;
	case E_CMP_CMP: name = g_strdup(_("Comparison (<=>)")); break;
	case E_LOGICAL_XOR: name = g_strdup(_("XOR")); break;
	case E_LOGICAL_NOT: name = g_strdup(_("NOT")); break;
	default: break;
	}
	
	return name;
}

static inline gboolean
iter_call2(GelCtx *ctx, const GelOper *op, GelETree *n)
{
	GelETree *l,*r;
	guint32 arg1,arg2;
	char *s;
	int i;

	GET_LR(n,l,r);
	
	arg1 = iter_get_arg(l);
	arg2 = iter_get_arg(r);
	
	if(arg1 == 0 || arg2 == 0) {
		s = g_strdup_printf(_("Bad types for '%s'"),
				    iter_get_op_name(n->op.oper));
		(*errorout)(s);
		return TRUE;
	}

	for(i=0;i<OP_TABLE_LEN;i++) {
		if(op->prim[i].arg[0]&arg1 &&
		   op->prim[i].arg[1]&arg2) {
			return op->prim[i].evalfunc(ctx,n,l,r);
		}
	}
	s = g_strdup_printf(_("%s not defined on <%s> and <%s>"),
			    iter_get_op_name(n->op.oper),
			    iter_get_arg_name(arg1),
			    iter_get_arg_name(arg2));
	(*errorout)(s);
	g_free(s);
	return TRUE;
}

static inline gboolean
iter_call1(GelCtx *ctx, const GelOper *op, GelETree *n)
{
	GelETree *l;
	guint32 arg1;
	char *s;
	int i;

	GET_L(n,l);
	
	arg1 = iter_get_arg(l);
	
	if(arg1 == 0) {
		s = g_strdup_printf(_("Bad type for '%s'"),
				    iter_get_op_name(n->op.oper));
		(*errorout)(s);
		return TRUE;
	}

	for(i=0;i<OP_TABLE_LEN;i++) {
		if(op->prim[i].arg[0]&arg1) {
			return op->prim[i].evalfunc(ctx,n,l);
		}
	}
	s = g_strdup_printf(_("%s not defined on <%s>"),
			    iter_get_op_name(n->op.oper),
			    iter_get_arg_name(arg1));
	(*errorout)(s);
	g_free(s);
	return TRUE;
}

static void
iter_region_sep_op (GelCtx *ctx, GelETree *n)
{
	GelETree *from, *to, *by = NULL;
	GelETree *vect = NULL;
	GelMatrix *mat;
	int bysgn = 1, cmp, initcmp, count, i;
	mpw_t tmp;
	if (n->op.oper == E_REGION_SEP_BY) {
		GET_LRR (n, from, by, to);
		if (from->type != VALUE_NODE ||
		    to->type != VALUE_NODE ||
		    by->type != VALUE_NODE) {
			(*errorout) (_("Vector building only works on numbers"));
			return;
		}
		initcmp = cmp = mpw_cmp (from->val.value, to->val.value);
		bysgn = mpw_sgn (by->val.value);

		if ((cmp > 0 && bysgn > 0) ||
		    (cmp != 0 && bysgn == 0) ||
		    (cmp < 0 && bysgn < 0)) {
			/* FIXME: perhaps we should just return null like octave? */
			(*errorout) (_("Impossible arguments to vector building operator"));
			return;
		}	
	} else {
		GET_LR (n, from, to);
		if (from->type != VALUE_NODE ||
		    to->type != VALUE_NODE) {
			(*errorout) (_("Vector building only works on numbers"));
			return;
		}
		initcmp = cmp = mpw_cmp (from->val.value, to->val.value);
		if (cmp > 0)
			bysgn = -1;
	}

	count = 0;
	mpw_init (tmp);
	mpw_set (tmp, from->val.value);
	for (;;) {
		GelETree *t = gel_makenum (tmp);

		t->any.next = vect;
		vect = t;
		count ++;

		if (cmp == 0 || cmp != initcmp)
			break;

		if (by != NULL)
			mpw_add (tmp, tmp, by->val.value);
		else if (bysgn == 1) 
			mpw_add_ui (tmp, tmp, 1);
		else
			mpw_sub_ui (tmp, tmp, 1);

		cmp = mpw_cmp (tmp, to->val.value);

		if (cmp != 0 && cmp != initcmp)
			break;
	}
	mpw_clear (tmp);

	mat = gel_matrix_new ();
	gel_matrix_set_size (mat, count, 1, FALSE /* padding */);

	for (i = count-1; i >= 0; i--) {
		GelETree *t = vect;
		gel_matrix_index (mat, i, 0) = t;
		vect = vect->any.next;
		t->any.next = NULL;
	}

	freetree_full (n, TRUE /* freeargs */, FALSE /* kill */);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (mat);
	n->mat.quoted = 1;
}

/*The first pass over an operator (sometimes it's enough and we don't go
  for a second pass*/
static gboolean
iter_operator_pre(GelCtx *ctx)
{
	GelETree *n = ctx->current;
	
	EDEBUG(" OPERATOR PRE");
	
	switch(n->op.oper) {
	case E_EQUALS:
		EDEBUG("  EQUALS PRE");
		GE_PUSH_STACK(ctx,n,GE_POST);
		iter_push_indexes_and_arg(ctx,n);
		break;

	case E_PARAMETER:
		EDEBUG("  PARAMETER PRE");
		GE_PUSH_STACK(ctx,n,GE_POST);
		/* Push third parameter (the value) */
		ctx->post = FALSE;
		ctx->current = n->op.args->any.next->any.next;
		break;

	case E_EXP:
	case E_ELTEXP:
		EDEBUG("  PUSH US AS POST AND ALL ARGUMENTS AS PRE (no modulo on second)");
		GE_PUSH_STACK(ctx,n,GE_POST);
		iter_push_args_no_modulo_on_2 (ctx, n->op.args);
		break;

	case E_SEPAR:
	case E_ABS:
	case E_PLUS:
	case E_MINUS:
	case E_MUL:
	case E_ELTMUL:
	case E_DIV:
	case E_ELTDIV:
	case E_BACK_DIV:
	case E_ELT_BACK_DIV:
	case E_MOD:
	case E_ELTMOD:
	case E_NEG:
	case E_FACT:
	case E_DBLFACT:
	case E_TRANSPOSE:
	case E_CONJUGATE_TRANSPOSE:
	case E_CMP_CMP:
	case E_LOGICAL_XOR:
	case E_LOGICAL_NOT:
	case E_RETURN:
	case E_GET_VELEMENT:
	case E_GET_ELEMENT:
	case E_GET_ROW_REGION:
	case E_GET_COL_REGION:
	case E_REGION_SEP:
	case E_REGION_SEP_BY:
		EDEBUG("  PUSH US AS POST AND ALL ARGUMENTS AS PRE");
		GE_PUSH_STACK(ctx,n,GE_POST);
		iter_push_args (ctx, n->op.args);
		break;

	case E_CALL:
		EDEBUG("  CHANGE CALL TO DIRECTCALL AND EVAL THE FIRST ARGUMENT");
		n->op.oper = E_DIRECTCALL;
		GE_PUSH_STACK (ctx, n, GE_PRE);
		/* eval first argument */
		ctx->current = n->op.args;
		ctx->post = FALSE;
		break;

	/*in case of DIRECTCALL we don't evaluate the first argument*/
	case E_DIRECTCALL:
		/*if there are arguments to evaluate*/
		if(n->op.args->any.next) {
			GelEFunc *f;
			EDEBUG("  DIRECT:PUSH US AS POST AND 2nd AND HIGHER ARGS AS PRE");
			GE_PUSH_STACK(ctx,n,GE_POST);
			f = get_func_from_arg (ctx, n, TRUE /* silent */);
			if (f != NULL && f->no_mod_all_args)
				iter_push_args_no_modulo (ctx, n->op.args->any.next);
			else
				iter_push_args (ctx, n->op.args->any.next);
		} else {
			EDEBUG("  DIRECT:JUST GO TO POST");
			/*just go to post immediately*/
			ctx->post = TRUE;
		}
		break;

	/*these should have been translated to COMPARE_NODEs*/
	case E_EQ_CMP:
	case E_NE_CMP:
	case E_LT_CMP:
	case E_GT_CMP:
	case E_LE_CMP: 
	case E_GE_CMP:
		g_assert_not_reached();

	case E_LOGICAL_AND:
		EDEBUG("  LOGICAL AND");
		GE_PUSH_STACK(ctx,n,GE_POST);
		GE_PUSH_STACK(ctx,n->op.args,GE_AND);
		ctx->post = FALSE;
		ctx->current = n->op.args;
		break;
	case E_LOGICAL_OR:
		EDEBUG("  LOGICAL OR");
		GE_PUSH_STACK(ctx,n,GE_POST);
		GE_PUSH_STACK(ctx,n->op.args,GE_OR);
		ctx->post = FALSE;
		ctx->current = n->op.args;
		break;

	case E_WHILE_CONS:
		iter_loop(ctx,n,FALSE,TRUE);
		break;
	case E_UNTIL_CONS:
		iter_loop(ctx,n,FALSE,FALSE);
		break;
	case E_DOWHILE_CONS:
		iter_loop(ctx,n,TRUE,TRUE);
		break;
	case E_DOUNTIL_CONS:
		iter_loop(ctx,n,TRUE,FALSE);
		break;

	case E_IF_CONS:
	case E_IFELSE_CONS:
		EDEBUG("  IF/IFELSE PRE");
		GE_PUSH_STACK(ctx,n,GE_POST);
		ctx->post = FALSE;
		ctx->current = n->op.args;
		break;

	case E_DEREFERENCE:
		if(!iter_derefvarop(ctx,n))
			return FALSE;
		if ((n->type == VALUE_NODE ||
		     n->type == MATRIX_NODE) &&
		    ctx->modulo != NULL)
			mod_node (n, ctx->modulo);
		iter_pop_stack(ctx);
		break;

	case E_FOR_CONS:
	case E_FORBY_CONS:
	case E_SUM_CONS:
	case E_SUMBY_CONS:
	case E_PROD_CONS:
	case E_PRODBY_CONS:
		GE_PUSH_STACK(ctx,n,GE_POST);
		iter_push_args_no_last(ctx,n->op.args->any.next);
		break;

	case E_FORIN_CONS:
	case E_SUMIN_CONS:
	case E_PRODIN_CONS:
		GE_PUSH_STACK(ctx,n,GE_POST);
		ctx->current = n->op.args->any.next;
		ctx->post = FALSE;
		break;

	case E_EXCEPTION:
		return FALSE;

	case E_BAILOUT:
		iter_bailout_op(ctx);
		break;

	case E_CONTINUE:
		iter_continue_break_op(ctx,TRUE);
		break;

	case E_BREAK:
		iter_continue_break_op(ctx,FALSE);
		break;

	case E_QUOTE:
		{
			/* Just replace us with the quoted thing */
			GelETree *arg = n->op.args;
			n->op.args = NULL;
			replacenode (n, arg);
			iter_pop_stack(ctx);
			break;
		}

	case E_REFERENCE:
		iter_pop_stack(ctx);
		break;

	case E_MOD_CALC:
		/* Push modulo op, so that we may push the
		 * first argument once we have gotten a modulo */
		GE_PUSH_STACK (ctx, n, GE_MODULOOP);
		ctx->post = FALSE;
		ctx->current = n->op.args->any.next;
		break;

	default:
		(*errorout)(_("Unexpected operator!"));
		GE_PUSH_STACK(ctx,n,GE_POST);
		break;
	}
	return TRUE;
}

static gboolean
iter_operator_post(GelCtx *ctx)
{
	GelETree *n = ctx->current;
	GelETree *r;
	EDEBUG(" OPERATOR POST");
	switch(n->op.oper) {
	case E_SEPAR:
		r = n->op.args->any.next;
		/*remove from arg list*/
		n->op.args->any.next = NULL;
		replacenode(n,r);
		iter_pop_stack(ctx);
		break;

	case E_EQUALS:
		EDEBUG("  EQUALS POST");
		iter_equalsop(n);
		iter_pop_stack(ctx);
		break;

	case E_PARAMETER:
		EDEBUG("  PARAMETER POST");
		iter_parameterop (n);
		iter_pop_stack (ctx);
		break;

	case E_PLUS:
	case E_MINUS:
	case E_MUL:
	case E_ELTMUL:
	case E_DIV:
	case E_ELTDIV:
	case E_BACK_DIV:
	case E_ELT_BACK_DIV:
	case E_MOD:
	case E_ELTMOD:
	case E_EXP:
	case E_ELTEXP:
	case E_CMP_CMP:
	case E_LOGICAL_XOR:
		if(!iter_call2(ctx,&prim_table[n->op.oper],n))
			return FALSE;
		if (ctx->modulo != NULL &&
		    (n->type == VALUE_NODE ||
		     /* FIXME: note, most matrix operations already
		      * mod, so this will just make things slower,
		      * but currently it is needed for correct
		      * behaviour */
		     n->type == MATRIX_NODE))
			mod_node (n, ctx->modulo);
		iter_pop_stack(ctx);
		break;

	case E_ABS:
	case E_NEG:
	case E_FACT:
	case E_DBLFACT:
	case E_TRANSPOSE:
	case E_CONJUGATE_TRANSPOSE:
	case E_LOGICAL_NOT:
		if(!iter_call1(ctx,&prim_table[n->op.oper],n))
			return FALSE;
		if (ctx->modulo != NULL &&
		    (n->type == VALUE_NODE ||
		     /* FIXME: note, most matrix operations already
		      * mod, so this will just make things slower,
		      * but currently it is needed for correct
		      * behaviour */
		     n->type == MATRIX_NODE))
			mod_node (n, ctx->modulo);
		iter_pop_stack(ctx);
		break;

	case E_MOD_CALC:
		/* FIXME: maybe we should always replace things here,
		 * not just for values and matrices */
		if (n->op.args->type == VALUE_NODE ||
		    n->op.args->type == MATRIX_NODE ||
		    /* also replace if we got a E_MOD_CALC oper since
		     * that can only mean an error occured, and we
		     * don't want to duplicate the mod */
		    (n->op.args->type == OPERATOR_NODE &&
		     n->op.args->op.oper == E_MOD_CALC)) {
			GelETree *t = n->op.args;
			gel_freetree (n->op.args->any.next);
			n->op.args = NULL;
			replacenode (n, t);
		}
		iter_pop_stack(ctx);
		break;

	case E_FOR_CONS:
	case E_FORBY_CONS:
	case E_SUM_CONS:
	case E_SUMBY_CONS:
	case E_PROD_CONS:
	case E_PRODBY_CONS:
		iter_forloop (ctx, n);
		break;

	case E_FORIN_CONS:
	case E_SUMIN_CONS:
	case E_PRODIN_CONS:
		iter_forinloop (ctx, n);
		break;

	case E_GET_VELEMENT:
		iter_get_velement (n);
		iter_pop_stack (ctx);
		break;

	case E_GET_ELEMENT:
		iter_get_element (n);
		iter_pop_stack (ctx);
		break;

	case E_GET_ROW_REGION:
		iter_get_region (n, FALSE /* col */);
		iter_pop_stack (ctx);
		break;

	case E_GET_COL_REGION:
		iter_get_region (n, TRUE /* col */);
		iter_pop_stack (ctx);
		break;

	case E_IF_CONS:
		iter_ifop(ctx,n,FALSE);
		break;
	case E_IFELSE_CONS:
		iter_ifop(ctx,n,TRUE);
		break;

	case E_DIRECTCALL:
	case E_CALL:
		if(!iter_funccallop(ctx,n))
			return FALSE;
		break;

	case E_RETURN:
		iter_returnop(ctx,n);
		break;

	case E_REGION_SEP:
	case E_REGION_SEP_BY:
		iter_region_sep_op (ctx, n);
		iter_pop_stack (ctx);
		break;

	/*these should have been translated to COMPARE_NODEs*/
	case E_EQ_CMP:
	case E_NE_CMP:
	case E_LT_CMP:
	case E_GT_CMP:
	case E_LE_CMP: 
	case E_GE_CMP:

	/*This operators should never reach post, they are evaluated in pre,
	  or dealt with through the pop_stack_special*/
	case E_QUOTE:
	case E_REFERENCE:
	case E_LOGICAL_AND:
	case E_LOGICAL_OR:
	case E_WHILE_CONS:
	case E_UNTIL_CONS:
	case E_DOWHILE_CONS:
	case E_DOUNTIL_CONS:
	case E_CONTINUE:
	case E_BREAK:
	case E_EXCEPTION:
	case E_BAILOUT:
	case E_DEREFERENCE:
		g_assert_not_reached();

	default:
		(*errorout)(_("Unexpected operator!"));
		iter_pop_stack(ctx);
		break;
	}
	return TRUE;
}

static gboolean
function_id_on_list (GSList *funclist, GelToken *id)
{
	GSList *li;
       
	for (li = funclist; li != NULL; li = li->next) {
		GelEFunc *func = li->data;
		if (func->id == id)
			return TRUE;
	}
	return FALSE;
}

GSList *
gel_subst_local_vars (GSList *funclist, GelETree *n)
{
	if (n == NULL)
		return funclist;

	if (n->type == IDENTIFIER_NODE) {
		GelEFunc *func = d_lookup_local (n->id.id);
		if (func != NULL &&
		    ! function_id_on_list (funclist, n->id.id)) {
			GelEFunc *f = d_copyfunc (func);
			f->context = -1;
			funclist = g_slist_prepend (funclist, f);
		}
	} else if (n->type == SPACER_NODE) {
		funclist = gel_subst_local_vars (funclist, n->sp.arg);
	} else if(n->type == OPERATOR_NODE) {
		/* special case to avoid more work
		 * then needed */
		if (n->op.oper == E_EQUALS &&
		    n->op.args->type == IDENTIFIER_NODE) {
			funclist = gel_subst_local_vars (funclist, n->op.args->any.next);
		} else {
			GelETree *args = n->op.args;
			while (args != NULL) {
				funclist = gel_subst_local_vars (funclist, args);
				args = args->any.next;
			}
		}
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix);
		for (i = 0; i < w; i++) {
			for(j = 0; j < h; j++) {
				GelETree *t = gel_matrixw_set_index
					(n->mat.matrix, i, j);
				if (t != NULL)
					funclist = gel_subst_local_vars (funclist, t);
			}
		}
	} else if (n->type == SET_NODE) {
		GelETree *ali;
		for(ali = n->set.items; ali != NULL; ali = ali->any.next)
			funclist = gel_subst_local_vars (funclist, ali);
	} else if (n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC)) {
		D_ENSURE_USER_BODY (n->func.func);
		funclist = gel_subst_local_vars (funclist, n->func.func->data.user);
	}
	return funclist;
}

static gboolean
iter_eval_etree(GelCtx *ctx)
{
	GelETree *n;
	while((n = ctx->current)) {
		EDEBUG("ITER");
		if(evalnode_hook) {
			static int i = 0;
			if(i++>run_hook_every) {
				(*evalnode_hook)();
				i=0;
			}
		}
		if(interrupted)
			return FALSE;

		switch(n->type) {
		case NULL_NODE:
			EDEBUG(" NULL NODE");
			iter_pop_stack(ctx);
			break;

		case VALUE_NODE:
			EDEBUG(" VALUE NODE");

			if (ctx->modulo != NULL)
				mod_node (n, ctx->modulo);

			iter_pop_stack(ctx);
			break;
		case MATRIX_NODE:
			EDEBUG(" MATRIX NODE");
			if(!ctx->post) {
				/*if in pre mode, push elements onto stack*/
				iter_push_matrix(ctx,n,n->mat.matrix);
			} else {
				/*if in post mode expand the matrix */
				if(!n->mat.quoted)
					gel_expandmatrix (n);
				if (ctx->modulo != NULL)
					mod_node (n, ctx->modulo);
				iter_pop_stack(ctx);
			}
			break;
		case OPERATOR_NODE:
			EDEBUG(" OPERATOR NODE");
			if(!ctx->post) {
				if(!iter_operator_pre(ctx))
					return FALSE;
			} else {
				if(!iter_operator_post(ctx))
					return FALSE;
			}
			break;
		case IDENTIFIER_NODE:
			EDEBUG(" IDENTIFIER NODE");
			if(!iter_variableop(ctx, n))
				return FALSE;
			if ((n->type == VALUE_NODE ||
			     n->type == MATRIX_NODE) &&
			    ctx->modulo != NULL)
				mod_node (n, ctx->modulo);
			iter_pop_stack(ctx);
			break;
		case STRING_NODE:
			EDEBUG(" STRING NODE");
			iter_pop_stack(ctx);
			break;

		case FUNCTION_NODE:
			EDEBUG(" FUNCTION NODE");
			if (n->func.func != NULL &&
			    (n->func.func->type == GEL_USER_FUNC ||
			     n->func.func->type == GEL_VARIABLE_FUNC) &&
			    d_curcontext () != 0) {
				d_put_on_subst_list (n->func.func);
			}
			iter_pop_stack(ctx);
			break;

		case COMPARISON_NODE:
			EDEBUG(" COMPARISON NODE");
			if(!ctx->post) {
				/*if in pre mode, push arguments onto stack*/
				GE_PUSH_STACK(ctx,n,GE_POST);
				iter_push_args(ctx,n->comp.args);
			} else {
				/*if in post mode evaluate */
				evalcomp(n);
				iter_pop_stack(ctx);
			}
			break;
		case USERTYPE_NODE:
			EDEBUG(" USERTYPE NODE");
			iter_pop_stack(ctx);
			break;
		default:
			(*errorout)(_("Unexpected node!"));
			iter_pop_stack(ctx);
			break;
		}
	}
	return TRUE;
}

GelCtx *
eval_get_context(void)
{
	GelCtx *ctx = g_new0(GelCtx,1);
	ge_add_stack_array(ctx);
	return ctx;
}

void
eval_free_context(GelCtx *ctx)
{
	g_free(ctx->stack);
	g_free(ctx);
}

GelETree *
eval_etree(GelCtx *ctx, GelETree *etree)
{
	/*level measures any recursion into here such as from
	  external functions etc, so that we can purge free lists,
	  but not during calculation*/
	static int level = 0;
	int flag;
	gpointer data;

	if (ctx->modulo != NULL) {
		GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);
		ctx->modulo = NULL;
	}
	
	GE_PUSH_STACK(ctx,ctx->res,GE_RESULT);
	if(ctx->post) {
		GE_PUSH_STACK(ctx,ctx->current,GE_POST);
	} else {
		GE_PUSH_STACK(ctx,ctx->current,GE_PRE);
	}
	GE_PUSH_STACK(ctx,NULL,GE_EMPTY_STACK);
	ctx->res = etree;
	ctx->current = etree;
	ctx->post = FALSE;
	
	level++;
	
	if(!iter_eval_etree(ctx)) {
		gpointer data;
		/*an exception happened*/
		gel_freetree(ctx->res);
		etree = ctx->res = NULL;
		do {
			GE_POP_STACK(ctx,data,flag);
			ev_free_special_data(ctx,data,flag);
		} while(flag != GE_EMPTY_STACK);
	}
	if(--level == 0)
		purge_free_lists();
	
	GE_POP_STACK(ctx,ctx->current,flag);
	g_assert(flag == GE_POST || flag == GE_PRE);
	ctx->post = (flag == GE_POST);
	GE_POP_STACK(ctx,ctx->res,flag);
	g_assert(flag == GE_RESULT);

	GE_PEEK_STACK (ctx, data, flag);
	if (flag == GE_SETMODULO) {
		if (ctx->modulo != NULL) {
			mpw_clear (ctx->modulo);
			g_free (ctx->modulo);
		}
		ctx->modulo = data;
	}

	return etree;
}

GelETree *
gather_comparisons(GelETree *n)
{
	GelETree *next,*ret;
	if(!n) return NULL;

	ret = n;
	next = n->any.next;

	if(n->type == SPACER_NODE) {
		GelETree *t = n->sp.arg;
		freenode(n);
		ret = gather_comparisons(t);
	} else if(n->type==OPERATOR_NODE) {
		GelETree *nn;
		GelETree *ali = NULL;
		switch(n->op.oper) {
		case E_EQ_CMP:
		case E_NE_CMP:
		case E_LT_CMP:
		case E_GT_CMP:
		case E_LE_CMP:
		case E_GE_CMP:
			GET_NEW_NODE(nn);
			nn->type = COMPARISON_NODE;
			nn->comp.nargs = 0;
			nn->comp.args = NULL;
			nn->comp.comp = NULL;
			
			for(;;) {
				GelETree *t;
				t = n->op.args->any.next;
				if(!ali) {
					ali = nn->comp.args =
						gather_comparisons(n->op.args);
				} else {
					ali = ali->any.next = 
						gather_comparisons(n->op.args);
				}
				ali->any.next = NULL;
				nn->comp.nargs++;
				nn->comp.comp =
					g_slist_append(nn->comp.comp,
						      GINT_TO_POINTER(n->op.oper));

				freenode(n);
				n = t;
				if(n->type != OPERATOR_NODE ||
				   (n->op.oper != E_EQ_CMP &&
				    n->op.oper != E_NE_CMP &&
				    n->op.oper != E_LT_CMP &&
				    n->op.oper != E_GT_CMP &&
				    n->op.oper != E_LE_CMP &&
				    n->op.oper != E_GE_CMP)) {
					ali = ali->any.next = 
						gather_comparisons(n);
					ali->any.next = NULL;
					nn->comp.nargs++;
					break;
				}
			}
			ret = nn;
			break;
		default:
			if(n->op.args) {
				n->op.args = gather_comparisons(n->op.args);
				for(ali=n->op.args;ali->any.next;ali=ali->any.next)
					ali->any.next =
						gather_comparisons(ali->any.next);
			}
		}
	} else if(n->type==MATRIX_NODE) {
		int i,j;
		int w,h;
		if(!n->mat.matrix)
			goto gather_comparisons_end;
		w = gel_matrixw_width(n->mat.matrix);
		h = gel_matrixw_height(n->mat.matrix);
		gel_matrixw_make_private(n->mat.matrix);
		for(i=0;i<w;i++) {
			for(j=0;j<h;j++) {
				GelETree *t = gel_matrixw_set_index(n->mat.matrix,i,j);
				if(t) {
					gel_matrixw_set_index(n->mat.matrix,i,j) =
						gather_comparisons(t);
				}
			}
		}
	} else if(n->type==SET_NODE) {
		GelETree *ali;
		if(n->set.items) {
			n->set.items = gather_comparisons(n->set.items);
			for(ali=n->set.items;ali->any.next;ali=ali->any.next)
				ali->any.next =
					gather_comparisons(ali->any.next);
		}
	} else if(n->type==FUNCTION_NODE) {
		if ((n->func.func->type == GEL_USER_FUNC ||
		     n->func.func->type == GEL_VARIABLE_FUNC) &&
		    n->func.func->data.user) {
			n->func.func->data.user =
				gather_comparisons(n->func.func->data.user);
		}
	}
gather_comparisons_end:
	ret->any.next = next;
	return ret;
}

void
replace_equals (GelETree *n, gboolean in_expression)
{
	if (n == NULL)
		return;

	if (n->type == SPACER_NODE) {
		replace_equals (n->sp.arg, in_expression);
	} else if(n->type == OPERATOR_NODE) {
		gboolean run_through_args = TRUE;
		if (n->op.oper == E_EQUALS &&
		    in_expression) {
			n->op.oper = E_EQ_CMP;
		} else if (n->op.oper == E_WHILE_CONS ||
			   n->op.oper == E_UNTIL_CONS ||
			   n->op.oper == E_IF_CONS) {
			run_through_args = FALSE;
			replace_equals (n->op.args, TRUE);
			replace_equals (n->op.args->any.next, in_expression);
		} else if (n->op.oper == E_DOWHILE_CONS ||
			   n->op.oper == E_DOUNTIL_CONS) {
			run_through_args = FALSE;
			replace_equals (n->op.args, in_expression);
			replace_equals (n->op.args->any.next, TRUE);
		} else if (n->op.oper == E_IFELSE_CONS) {
			run_through_args = FALSE;
			replace_equals (n->op.args, TRUE);
			replace_equals (n->op.args->any.next, in_expression);
			replace_equals (n->op.args->any.next->any.next, in_expression);
		}

		if (run_through_args) {
			GelETree *args = n->op.args;
			while (args != NULL) {
				replace_equals (args, in_expression);
				args = args->any.next;
			}
		}
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix);
		for (i = 0; i < w; i++) {
			for(j = 0; j < h; j++) {
				GelETree *t = gel_matrixw_set_index
					(n->mat.matrix, i, j);
				if (t != NULL)
					replace_equals (t, in_expression);
			}
		}
	} else if (n->type == SET_NODE ) {
		GelETree *ali;
		for(ali = n->set.items; ali != NULL; ali = ali->any.next)
			replace_equals (ali, in_expression);
	} else if (n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC) &&
		   n->func.func->data.user != NULL) {
		/* function bodies are a completely new thing */
		replace_equals (n->func.func->data.user, FALSE);
	}
}

/* Fixup number negation */
void
fixup_num_neg (GelETree *n)
{
	if (n == NULL)
		return;

	if (n->type == SPACER_NODE) {
		fixup_num_neg (n->sp.arg);
	} else if(n->type == OPERATOR_NODE) {
		/* replace -1^2 with something like (-1)^2, only
		 * for numbers.  If you typed parenthesis as in
		 * -(1)^2, there would be a spacer node present
		 * so the below would not happen */
		if (n->op.oper == E_NEG &&
		    n->op.args->type == OPERATOR_NODE &&
		    (n->op.args->op.oper == E_EXP ||
		     n->op.args->op.oper == E_ELTEXP) &&
		    n->op.args->op.args->type == VALUE_NODE) {
			GelETree *t = n->op.args;
			n->op.args = NULL;
			replacenode (n, t);
			mpw_neg (n->op.args->val.value,
				 n->op.args->val.value);
			fixup_num_neg (n->op.args->any.next);
		} else {
			GelETree *args = n->op.args;
			while (args != NULL) {
				fixup_num_neg (args);
				args = args->any.next;
			}
		}
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix);
		for (i = 0; i < w; i++) {
			for(j = 0; j < h; j++) {
				GelETree *t = gel_matrixw_set_index
					(n->mat.matrix, i, j);
				if (t != NULL)
					fixup_num_neg (t);
			}
		}
	} else if (n->type == SET_NODE ) {
		GelETree *ali;
		for(ali = n->set.items; ali != NULL; ali = ali->any.next)
			fixup_num_neg (ali);
	} else if (n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC) &&
		   n->func.func->data.user != NULL) {
		fixup_num_neg (n->func.func->data.user);
	}
}


/*this means that it will precalc even complex and float
  numbers*/
static void
op_precalc_all_1(GelETree *n, void (*func)(mpw_ptr,mpw_ptr))
{
	GelETree *l;
	mpw_t res;
	GET_L(n,l);
	if(l->type != VALUE_NODE)
		return;
	mpw_init(res);
	(*func)(res,l->val.value);
	if(error_num) {
		mpw_clear(res);
		error_num = 0;
		return;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_use_from(n,res);
}

static void
op_precalc_1(GelETree *n, void (*func)(mpw_ptr,mpw_ptr))
{
	GelETree *l;
	mpw_t res;
	GET_L(n,l);
	if(l->type != VALUE_NODE ||
	   mpw_is_complex(l->val.value) ||
	   mpw_is_float(l->val.value))
		return;
	mpw_init(res);
	(*func)(res,l->val.value);
	if(error_num) {
		mpw_clear(res);
		error_num = 0;
		return;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_use_from(n,res);
}

static void
op_precalc_2(GelETree *n, void (*func)(mpw_ptr,mpw_ptr,mpw_ptr))
{
	GelETree *l,*r,*next;
	mpw_t res;
	GET_LR(n,l,r);
	if(l->type != VALUE_NODE ||
	   r->type != VALUE_NODE ||
	   mpw_is_complex(l->val.value) ||
	   mpw_is_complex(r->val.value) ||
	   mpw_is_float(l->val.value) ||
	   mpw_is_float(r->val.value))
		return;
	mpw_init(res);
	(*func)(res,l->val.value,r->val.value);
	if(error_num) {
		mpw_clear(res);
		error_num = 0;
		return;
	}
	next = n->any.next;
	freetree_full(n,TRUE,FALSE);
	gel_makenum_use_from(n,res);
	n->any.next = next;
}

static void
try_to_precalc_op(GelETree *n)
{
	switch(n->op.oper) {
	case E_NEG: op_precalc_all_1(n,mpw_neg); return;
	case E_ABS: op_precalc_1(n,mpw_abs); return;
	case E_FACT: op_precalc_1(n,mpw_fac); return;
	case E_DBLFACT: op_precalc_1(n,mpw_dblfac); return;
	case E_PLUS: op_precalc_2(n,mpw_add); return;
	case E_MINUS: op_precalc_2(n,mpw_sub); return;
	case E_MUL: op_precalc_2(n,mpw_mul); return;
	case E_ELTMUL: op_precalc_2(n,mpw_mul); return;
	case E_DIV: op_precalc_2(n,mpw_div); return;
	case E_ELTDIV: op_precalc_2(n,mpw_div); return;
	case E_MOD: op_precalc_2(n,mpw_mod); return;
	/* FIXME: this could be time consuming, somehow catch that */
	case E_EXP: op_precalc_2(n,mpw_pow); return;
	case E_ELTEXP: op_precalc_2(n,mpw_pow); return;
	default: return;
	}
}

void
try_to_do_precalc(GelETree *n)
{
	if(!n) return;

	if(n->type==OPERATOR_NODE) {
		GelETree *ali;
		if(n->op.oper == E_MOD_CALC) {
			/* in case of modular calculation, only do
			   precalc on the second argument (don't descend
			   at all into the first one) */
			/* FIXME: precalc might be broken in case of mod */
			/* try_to_do_precalc(n->op.args->any.next); */;
		} else {
			if(n->op.args) {
				for(ali=n->op.args;ali;ali=ali->any.next)
					try_to_do_precalc(ali);
			}
			if(n->type==OPERATOR_NODE)
				try_to_precalc_op(n);
		}
	} else if(n->type==MATRIX_NODE) {
		int i,j;
		int w,h;
		if(!n->mat.matrix) return;
		w = gel_matrixw_width(n->mat.matrix);
		h = gel_matrixw_height(n->mat.matrix);
		gel_matrixw_make_private(n->mat.matrix);
		for(i=0;i<w;i++) {
			for(j=0;j<h;j++) {
				GelETree *t = gel_matrixw_set_index(n->mat.matrix,i,j);
				if(t)
					try_to_do_precalc(t);
			}
		}
	} else if(n->type==SET_NODE) {
		GelETree *ali;
		if(n->set.items) {
			for(ali=n->set.items;ali;ali=ali->any.next)
				try_to_do_precalc(ali);
		}
	} else if(n->type==FUNCTION_NODE) {
		if ((n->func.func->type == GEL_USER_FUNC ||
		     n->func.func->type == GEL_VARIABLE_FUNC) &&
		    n->func.func->data.user)
			try_to_do_precalc(n->func.func->data.user);
	}
}
