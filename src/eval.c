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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "calc.h"
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "util.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "compil.h"
#include "utype.h"

#ifdef EVAL_DEBUG
#define EDEBUG(x) puts(x)
#else
#define EDEBUG(x) ;
#endif

GelETree *gel_free_trees = NULL;
static GelEvalStack *free_stack = NULL;

#ifndef MEM_DEBUG_FRIENDLY
static GelEvalLoop *free_evl = NULL;
static GelEvalFor *free_evf = NULL;
static GelEvalForIn *free_evfi = NULL;

static void _gel_make_free_evl (void);
static void _gel_make_free_evf (void);
static void _gel_make_free_evfi (void);
#endif /* ! MEM_DEBUG_FRIENDLY */

#ifdef MEM_DEBUG_FRIENDLY
static GelCtx *most_recent_ctx = NULL;
#endif

static inline void
ge_add_stack_array(GelCtx *ctx)
{
	GelEvalStack *newstack;
#ifdef MEM_DEBUG_FRIENDLY
	newstack = g_new0 (GelEvalStack, 1);
#else
	if (free_stack == NULL) {
		newstack = g_new (GelEvalStack, 1);
	} else {
		newstack = free_stack;
		free_stack = free_stack->next;
	}
#endif
	
	newstack->next = ctx->stack;
	ctx->stack = newstack;
	/*the array is at the beginning of the structure*/
	ctx->topstack = (gpointer *)newstack;
	EDEBUG("ADDING STACK ARRAY");
}

/*we assume that a stack always exists*/
#define GE_PUSH_STACK(thectx,pointer,flag) { \
	if G_UNLIKELY ((thectx)->topstack == &((thectx)->stack->stack[STACK_SIZE]))	\
	 	ge_add_stack_array(thectx);				\
	*((thectx)->topstack ++) = (pointer);				\
	*((thectx)->topstack ++) = GINT_TO_POINTER(flag);		\
}

static inline gboolean
ge_remove_stack_array(GelCtx *ctx)
{
	GelEvalStack *next = ctx->stack->next;
	if G_UNLIKELY (!next) return FALSE;

	/*push it onto the list of free stack entries*/
#ifdef MEM_DEBUG_FRIENDLY
	memset (ctx->stack, 0xaa, sizeof (GelEvalStack));
# ifndef MEM_DEBUG_SUPER_FRIENDLY
	g_free (ctx->stack);
# endif /* !MEM_DEBUG_SUPER_FRIENDLY */
#else /* MEM_DEBUG_FRIENDLY */
	ctx->stack->next = free_stack;
	free_stack = ctx->stack;
#endif /* MEM_DEBUG_FRIENDLY */

	ctx->stack = next;
	ctx->topstack = &((ctx)->stack->stack[STACK_SIZE]);
	EDEBUG("REMOVING STACK ARRAY");
	return TRUE;
}

#ifdef MEM_DEBUG_FRIENDLY
#define GE_POP_STACK(thectx,pointer,flag) { \
	if((thectx)->topstack != (gpointer *)(thectx)->stack ||		\
	   ge_remove_stack_array(ctx)) {				\
		(flag) = GPOINTER_TO_INT(*(-- (thectx)->topstack));	\
		*((thectx)->topstack) = NULL;	 			\
		(pointer) = *(-- (thectx)->topstack);			\
		*((thectx)->topstack) = NULL;	 			\
	} else {							\
		(flag) = GE_EMPTY_STACK;				\
		(pointer) = NULL;					\
	}								\
}
#else /* MEM_DEBUG_FRIENDLY */
#define GE_POP_STACK(thectx,pointer,flag) { \
	if G_LIKELY ((thectx)->topstack != (gpointer *)(thectx)->stack ||	\
	   ge_remove_stack_array(ctx)) {					\
		(flag) = GPOINTER_TO_INT(*(-- (thectx)->topstack));	\
		(pointer) = *(-- (thectx)->topstack);			\
	} else {							\
		(flag) = GE_EMPTY_STACK;				\
		(pointer) = NULL;					\
	}								\
}
#endif /* MEM_DEBUG_FRIENDLY */

#define GE_PEEK_STACK(thectx,pointer,flag) { \
	if G_LIKELY ((thectx)->topstack != (gpointer *)(thectx)->stack) {	\
		(flag) = GPOINTER_TO_INT(*((thectx)->topstack - 1));	\
		(pointer) = *((thectx)->topstack - 2);			\
	} else if((thectx)->stack->next) {				\
		gpointer *a = (gpointer) &((thectx)->stack->next->next);\
		(flag) = GPOINTER_TO_INT(*(--a));			\
		(pointer) = *(--a);					\
	} else {							\
		(flag) = GE_EMPTY_STACK;				\
		(pointer) = NULL;					\
	}								\
}

#ifdef MEM_DEBUG_FRIENDLY
#define GE_BLIND_POP_STACK(thectx) { \
	if((thectx)->topstack != (gpointer *)(thectx)->stack ||	\
	   ge_remove_stack_array(thectx)) {			\
		*(-- (thectx)->topstack) = NULL;		\
		*(-- (thectx)->topstack) = NULL;		\
	}							\
}
#else /* MEM_DEBUG_FRIENDLY */
#define GE_BLIND_POP_STACK(thectx) { \
	if G_LIKELY ((thectx)->topstack != (gpointer *)(thectx)->stack ||	\
		     ge_remove_stack_array(thectx)) {		\
		(thectx)->topstack -= 2;			\
	}							\
}
#endif /* MEM_DEBUG_FRIENDLY */

static void mod_node(GelETree *n, mpw_ptr mod);
static void mod_matrix (GelMatrixW *m, mpw_ptr mod);
static inline GelEFunc * get_func_from (GelETree *l, gboolean silent);
static int branches (int op) G_GNUC_CONST;


/*returns the number of args for an operator, or -1 if it takes up till
  exprlist marker or -2 if it takes one more for the first argument*/
static int
branches (int op)
{
	switch(op) {
		case E_SEPAR: return 2;
		case E_EQUALS: return 2;
		case E_PARAMETER: return 3;
		case E_ABS: return 1;
		case E_PLUS: return 2;
		case E_ELTPLUS: return 2;
		case E_MINUS: return 2;
		case E_ELTMINUS: return 2;
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
		case E_DEFEQUALS: return 2;
	}
	return 0;
}

void
gel_init (void)
{
	if (the_zero == NULL)
		the_zero = gel_makenum_ui (0);
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

/*
static gboolean
find_on_stack (GelCtx *ctx, GelETree *etree, int *flag)
{
	GelEvalStack *stack = ctx->stack;
	gpointer *iter = ctx->topstack;
	gpointer *last = NULL;
	if ((gpointer)iter == (gpointer)stack) {
		if (stack->next == NULL)
			return FALSE;
		stack = stack->next;
		iter = &(stack->stack[STACK_SIZE]);
	}
	while (TRUE) {
		last = iter;
		iter -= 2;
		if (*iter == etree) {
			*flag = (int)(*(iter+1));
			return TRUE;
		}
		if ((gpointer)iter == (gpointer)stack) {
			if (stack->next == NULL)
				return FALSE;
			stack = stack->next;
			iter = &(stack->stack[STACK_SIZE]);
		}
	}
}
*/

GelETree *
gel_makenum_null (void)
{
	GelETree *n;
	GEL_GET_NEW_NODE (n);
	n->type = NULL_NODE;
	n->any.next = NULL;
	return n;
}

GelETree *
gel_makenum_identifier (GelToken *id)
{
	GelETree *n;
	GEL_GET_NEW_NODE (n);
	n->type = IDENTIFIER_NODE;
	n->id.id = id; 
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_string (const char *str)
{
	GelETree *n;
	GEL_GET_NEW_NODE (n);
	n->type = STRING_NODE;
	n->str.str = g_strdup (str); 
	n->str.constant = FALSE;
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_string_use (char *str)
{
	GelETree *n;
	GEL_GET_NEW_NODE (n);
	n->type = STRING_NODE;
	n->str.str = str; 
	n->str.constant = FALSE;
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_string_constant (const char *str)
{
	GelETree *n;
	char *hstr;
	static GHashTable *constant_strings = NULL;

	if G_UNLIKELY (constant_strings == NULL)
		constant_strings = g_hash_table_new (g_str_hash, g_str_equal);

	hstr = g_hash_table_lookup (constant_strings, str);

	if (hstr == NULL) {
		hstr = g_strdup (str);
		g_hash_table_insert (constant_strings,
				     hstr, hstr);
	}

	GEL_GET_NEW_NODE (n);
	n->type = STRING_NODE;
	n->str.str = hstr; 
	n->str.constant = TRUE;
	n->any.next = NULL;

	return n;
}

GelETree *
gel_makenum_ui(unsigned long num)
{
	GelETree *n;
	GEL_GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	mpw_init(n->val.value);
	mpw_set_ui(n->val.value,num);
	n->any.next = NULL;
	return n;
}

GelETree *
gel_makenum_si(long num)
{
	GelETree *n;
	GEL_GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	mpw_init(n->val.value);
	mpw_set_si(n->val.value,num);
	n->any.next = NULL;
	return n;
}

GelETree *
gel_makenum_d (double num)
{
	GelETree *n;
	GEL_GET_NEW_NODE (n);
	n->type = VALUE_NODE;
	mpw_init (n->val.value);
	mpw_set_d (n->val.value, num);
	n->any.next = NULL;
	return n;
}

GelETree *
gel_makenum_bool (gboolean bool_)
{
	GelETree *n;
	GEL_GET_NEW_NODE (n);
	n->type = BOOL_NODE;
	n->bool_.bool_ = bool_ ? 1 : 0;
	n->any.next = NULL;
	return n;
}

GelETree *
gel_makenum(mpw_t num)
{
	GelETree *n;
	GEL_GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	mpw_init_set(n->val.value,num);
	n->any.next = NULL;
	return n;
}

/*don't create a new number*/
GelETree *
gel_makenum_use(mpw_t num)
{
	GelETree *n;
	GEL_GET_NEW_NODE(n);
	n->type=VALUE_NODE;
	memcpy(n->val.value,num,sizeof(struct _mpw_t));
	n->any.next = NULL;
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

void
gel_makenum_bool_from (GelETree *n, gboolean bool_)
{
	n->type = BOOL_NODE;
	n->bool_.bool_ = bool_ ? 1 : 0;
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
		if ( ! n->str.constant)
			g_free (n->str.str);
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
		/*
		int flag;
		if (most_recent_ctx != NULL &&
		    find_on_stack (most_recent_ctx, n, &flag)) {
			printf ("FOUND ON STACK (%p)!!!! %d\n", n,
				   flag);
		}
		*/

#ifdef MEM_DEBUG_FRIENDLY
		if (most_recent_ctx != NULL &&
		    most_recent_ctx->current == n) {
			printf ("FOUND ON CURRENT (%p)!!!!\n", n);
		}

# ifdef EVAL_DEBUG
		printf ("%s WHACKING NODE %p\n", G_STRLOC, n);
		deregister_tree (n);
# endif /* EVAL_DEBUG */

		memset (n, 0xaa, sizeof (GelETree));
# ifndef MEM_DEBUG_SUPER_FRIENDLY
		g_free (n);
# endif /* ! MEM_DEBUG_SUPER_FRIENDLY */
#else /* ! MEM_DEBUG_FRIENDLY */
		/*put onto the free list*/
		n->any.next = gel_free_trees;
		gel_free_trees = n;
#endif

	}
#ifdef MEM_DEBUG_FRIENDLY
	else {
		GelETree *next = n->any.next;
		memset (n, 0, sizeof (GelETree));
		n->any.next = next;
	}
#endif /* MEM_DEBUG_FRIENDLY */
}

void
gel_freetree(GelETree *n)
{
	/*printf ("freeing: %p\n", n);*/
	freetree_full(n,TRUE,TRUE);
}

void
gel_emptytree(GelETree *n)
{
	/*printf ("freeing: %p\n", n);*/
	freetree_full(n,TRUE,FALSE);
}

/* Makes a new node and replaces the old one with NULL_NODE */
GelETree *
gel_stealnode (GelETree *n)
{
	GelETree *nn;

	if (n == NULL)
		return NULL;

	GEL_GET_NEW_NODE (nn);
	memcpy (nn, n, sizeof(GelETree));

#ifdef MEM_DEBUG_FRIENDLY
	{
		GelETree *next = n->any.next;
		memset (n, 0, sizeof (GelETree));
		n->any.next = next;
	}
#endif /* MEM_DEBUG_FRIENDLY */
	n->type = NULL_NODE;
	nn->any.next = NULL;
	
	return nn;
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
		mpw_init_set_no_uncomplex (empty->val.value,o->val.value);
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
			empty->op.args = gel_copynode(empty->op.args);
			for(li=empty->op.args;li->any.next;li=li->any.next) {
				li->any.next = gel_copynode(li->any.next);
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
		empty->str.constant = o->str.constant;
		if (o->str.constant)
			empty->str.str = o->str.str;
		else
			empty->str.str = g_strdup (o->str.str);
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
			empty->comp.args = gel_copynode(empty->comp.args);
			for(li=empty->comp.args;li->any.next;li=li->any.next) {
				li->any.next = gel_copynode(li->any.next);
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
	case BOOL_NODE:
		empty->type = BOOL_NODE;
		empty->any.next = o->any.next;
		empty->bool_.bool_ = o->bool_.bool_;
		break;
	case MATRIX_ROW_NODE:
		empty->type = MATRIX_ROW_NODE;
		empty->any.next = o->any.next;
		empty->row.nargs = o->row.nargs;
		empty->row.args = o->row.args;
		if(empty->row.args) {
			GelETree *li;
			empty->row.args = gel_copynode(empty->row.args);
			for(li=empty->row.args;li->any.next;li=li->any.next) {
				li->any.next = gel_copynode(li->any.next);
			}
		}
		break;
	case SPACER_NODE:
		empty->type = SPACER_NODE;
		empty->any.next = o->any.next;
		if(o->sp.arg)
			empty->sp.arg = gel_copynode(o->sp.arg);
		else
			empty->sp.arg = NULL;
		break;
	default:
		g_assert_not_reached();
		break;
	}
}

GelETree *
gel_copynode(GelETree *o)
{
	GelETree *n;

	if(!o)
		return NULL;

	GEL_GET_NEW_NODE(n);
	
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

# ifdef EVAL_DEBUG
	printf ("%s WHACKING NODE %p\n", G_STRLOC, from);
	deregister_tree (from);
# endif

	memset (from, 0xaa, sizeof (GelETree));
# ifndef MEM_DEBUG_SUPER_FRIENDLY
	g_free (from);
# endif
#else /* MEM_DEBUG_FRIENDLY */
	/*put onto the free list*/
	from->any.next = gel_free_trees;
	gel_free_trees = from;
#endif /* MEM_DEBUG_FRIENDLY */
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

void
gel_replacenode (GelETree *to, GelETree *from, gboolean copy)
{
	if (copy)
		copyreplacenode (to, from);
	else
		replacenode (to, from);
}

GelETree *
gel_makeoperator (int oper, GSList **stack)
{
	GelETree *n;
	int args;
	GelETree *list = NULL;
	args = branches(oper);
	if(args>=0) {
		int i;
		int popargs = args;
		for (i = 0; i < popargs; i++) {
			GelETree *tree = gel_stack_pop (stack);
			if(!tree)  {
				while(list) {
					GelETree *a = list->any.next;
					gel_freetree(list);
					list = a;
				}
				return NULL;
			}
			/* just reduce the list for separators */
			if (oper == E_SEPAR &&
			    tree->type == OPERATOR_NODE &&
			    tree->op.oper == E_SEPAR) {
				int extranum = 1;
				GelETree *last;

				/* there are at least two arguments */
				last = tree->op.args->any.next;
				while (last->any.next != NULL) {
					last = last->any.next;
					extranum ++;
				}

				args += extranum;

				last->any.next = list;
				list = tree->op.args;

				freenode (tree);
			} else {
				tree->any.next = list;
				list = tree;
			}
		}
	} else {
		int i=0;
		for(;;) {
			GelETree *tree;
			tree = gel_stack_pop(stack);
			/*we have gone all the way to the top and haven't
			  found a marker*/
			if G_UNLIKELY (!tree) {
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
					t = gel_stack_pop(stack);
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

	GEL_GET_NEW_NODE(n);
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
		if(!gel_matrixw_get_index(src,i,si)) continue;
		roww = i+1;
	}

	for(i=0;i<roww;i++) {
		GelETree *et = gel_matrixw_get_index(src,i,si);
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
		GelETree *et = gel_matrixw_get_index(src,i,si);
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
				gel_matrix_index(dest,i,di+x) = gel_copynode(et);
		/*single column matrix, convert to regular nodes*/
		} else if(gel_matrixw_width(et->mat.matrix) == 1) {
			int xx;
			int h = gel_matrixw_height(et->mat.matrix);
			gel_matrixw_make_private (et->mat.matrix, FALSE /* kill_type_caches */);
			for(x=0;x<h;x++) {
				gel_matrix_index(dest,i,di+x) =
					gel_matrixw_get_index(et->mat.matrix,0,x);
				gel_matrixw_set_index(et->mat.matrix,0,x) = NULL;
			}
			xx = 0;
			for(x=h;x<height;x++) {
				gel_matrix_index(dest,i,di+x) =
					gel_copynode(gel_matrix_index(dest,i,di+xx));
				if((++xx)>=h)
					xx=0;
			}
			gel_freetree(et);
		/*non-trivial matrix*/
		} else {
			int xx;
			int h = gel_matrixw_height(et->mat.matrix);
			int w = gel_matrixw_width(et->mat.matrix);

			gel_matrixw_make_private(et->mat.matrix,
						 FALSE /* kill_type_caches */);

			for(x=0;x<h;x++) {
				GelETree *n;
				GEL_GET_NEW_NODE(n);
				n->type = MATRIX_ROW_NODE;
				
				n->row.args = NULL;
				for(xx=w-1;xx>=0;xx--) {
					GelETree *t = gel_matrixw_get_index(et->mat.matrix,xx,x);
					if(!t)
						t = gel_makenum_ui(0);
					t->any.next = n->row.args;
					n->row.args = t;
					gel_matrixw_set_index(et->mat.matrix,xx,x) = NULL;
				}
				n->row.nargs = w;
				
				gel_matrix_index(dest,i,di+x) = n;

				*need_colwise = TRUE;
			}
			xx = 0;
			for(x=h;x<height;x++) {
				gel_matrix_index(dest,i,di+x) =
					gel_copynode(gel_matrix_index(dest,i,di+xx));
				if((++xx)>=h)
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
			;
		} else if (et->type == NULL_NODE) {
			/* Also here we just replace NULL_NODE's with 0's */
			if (et != &the_null)
				gel_freetree (et);
		} else if (et->type != MATRIX_ROW_NODE) {
			int x;
			gel_matrix_index (dest, di, i) = et;
			for (x = 1; x < w; x++)
				gel_matrix_index (dest, di+x, i) = gel_copynode (et);
		} else {
			int x;
			int xx;
			GelETree *iter;

			iter = et->row.args;
			for (iter = et->row.args, x=0; iter != NULL; x++) {
				if (iter->type == VALUE_NODE &&
				    MPW_IS_REAL (iter->val.value) &&
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
					gel_copynode (gel_matrix_index (dest, di+xx, i));
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
			GelETree *et = gel_matrixw_get_index (m, i, j);
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
		GelETree *t = gel_matrixw_get_index (nm, 0, 0);
		if (t != NULL &&
		    t->type == MATRIX_NODE) {
			if (nm->m->use == 1) {
				gel_matrixw_set_index (nm, 0, 0) = NULL;
			} else {
				t = gel_copynode (t);
			}
			replacenode (n, t);
			return;
		} else if (t != NULL &&
			   t->type == NULL_NODE) {
			freetree_full (n, TRUE, FALSE);
			n->type = NULL_NODE;
			return;
		}
		/* never should be reached */
	}

	gel_matrixw_make_private (nm, FALSE /* kill_type_caches */);

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
				if (colwidths[i] > 0) {
					expand_col (tm, m, i, ii, colwidths[i]);
				} else {
					int iii;
					for (iii = 0;
					     iii < m->height;
					     iii++) {
						GelETree *et = gel_matrix_index (m, i, iii);
						if (et != NULL) {
							if (et != &the_null)
								gel_freetree (et);
						}
					}
				}
			}
			gel_matrix_free (m);
			m = tm;
		}
		g_free (colwidths);
	}

	freetree_full (n, TRUE, FALSE);

	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (m);
	n->mat.quoted = FALSE;
}

static GelETree*
get_func_call_node(GelEFunc *func, GelETree **args, int nargs)
{
	int i;
	GelETree *l;
	GelETree *ret;
	GelETree *li = NULL;

	GEL_GET_NEW_NODE(l);
	l->type = FUNCTION_NODE;
	l->func.func = d_copyfunc(func);
	l->any.next = NULL;

	GEL_GET_NEW_NODE(ret);
	ret->type = OPERATOR_NODE;
	ret->op.oper = E_DIRECTCALL;
	ret->op.args = l;
	
	li = l;

	for(i=0;i<nargs;i++) {
		li = li->any.next = gel_copynode(args[i]);
	}
	li->any.next = NULL;
	ret->op.nargs = nargs+1;
	return ret;
}

GelETree *
gel_funccall(GelCtx *ctx, GelEFunc *func, GelETree **args, int nargs)
{
	GelETree *ret = NULL;
	
	g_return_val_if_fail(func!=NULL,NULL);

	ret = get_func_call_node(func,args,nargs);
	return gel_eval_etree(ctx,ret);
}

/*compare nodes, return TRUE if equal */
static gboolean
eqlnodes (GelETree *l, GelETree *r)
{
	if (l->type == BOOL_NODE ||
	    r->type == BOOL_NODE) {
		gboolean lt = gel_isnodetrue (l, NULL);
		gboolean rt = gel_isnodetrue (r, NULL);
		if ((lt && ! rt) ||
		    ( ! lt && rt)) {
			return 0;
		} else {
			return 1;
		}
	} else {
		gboolean n = mpw_eql(l->val.value,r->val.value);
		if G_UNLIKELY (gel_error_num) return 0;
		return n;
	}
}

/*compare nodes, return -1 if first one is smaller, 0 if they are
  equal, 1 if the first one is greater
  makes them the same type as a side effect*/
static int
cmpnodes(GelETree *l, GelETree *r)
{
	int n=0;

	n=mpw_cmp(l->val.value,r->val.value);

	if G_UNLIKELY (gel_error_num) return 0;

	if(n>0) n=1;
	else if(n<0) n=-1;
	return n;
}


static int
cmpcmpop(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int ret = cmpnodes(l,r);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = GEL_NO_ERROR;
		return TRUE;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_si_from(n,ret);
	return TRUE;
}

static int
logicalxorop(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	gboolean bad_node = FALSE;
	gboolean ret = gel_isnodetrue (l, &bad_node) != gel_isnodetrue (r,& bad_node);

	if G_UNLIKELY (bad_node || gel_error_num) {
		gel_error_num = GEL_NO_ERROR;
		return TRUE;
	}
	freetree_full (n, TRUE, FALSE);

	gel_makenum_bool_from (n, ret);

	return TRUE;
}

static int
logicalnotop(GelCtx *ctx, GelETree *n, GelETree *l)
{
	gboolean bad_node = FALSE;
	gboolean ret = !gel_isnodetrue(l,&bad_node);
	if G_UNLIKELY (bad_node || gel_error_num) {
		gel_error_num = GEL_NO_ERROR;
		return TRUE;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_bool_from (n, ret);
	return TRUE;
}

static gboolean
eqstring(GelETree *a, GelETree *b)
{
	int r = 0;
	if (a->type == STRING_NODE &&
	    b->type == STRING_NODE) {
		r = (strcmp (a->str.str, b->str.str) == 0);
	} else if (a->type == STRING_NODE) {
		char *s = gel_string_print_etree (b);
		r = (strcmp (a->str.str, s) == 0);
		g_free (s);
	} else if (b->type == STRING_NODE) {
		char *s = gel_string_print_etree (a);
		r = (strcmp (b->str.str, s) == 0);
		g_free (s);
	} else {
		g_assert_not_reached();
	}

	return r;
}

static gboolean
eqmatrix(GelETree *a, GelETree *b, int *error)
{
	gboolean r = FALSE;
	int i,j;
	if(a->type == MATRIX_NODE &&
	   b->type == MATRIX_NODE) {
		if G_UNLIKELY (!gel_is_matrix_value_or_bool_only(a->mat.matrix) ||
			       !gel_is_matrix_value_or_bool_only(b->mat.matrix)) {
			gel_errorout (_("Cannot compare non value or bool only matrixes"));
			*error = TRUE;
			return 0;
		}

		if G_UNLIKELY (gel_matrixw_width(a->mat.matrix)!=
			       gel_matrixw_width(b->mat.matrix) ||
			       gel_matrixw_height(a->mat.matrix)!=
			       gel_matrixw_height(b->mat.matrix)) {
			r = FALSE;
		} else {
			GelMatrixW *m1 = a->mat.matrix;
			GelMatrixW *m2 = b->mat.matrix;
			gboolean pure_values
				= (gel_is_matrix_value_only (a->mat.matrix) ||
				   gel_is_matrix_value_only (b->mat.matrix));
			
			r = TRUE;

			for(i=0;i<gel_matrixw_width(m1);i++) {
				for(j=0;j<gel_matrixw_height(m1);j++) {
					GelETree *t1,*t2;
					t1 = gel_matrixw_index(m1,i,j);
					t2 = gel_matrixw_index(m2,i,j);

					if (pure_values) {
						if ( ! mpw_eql (t1->val.value,
								t2->val.value)) {
							r = FALSE;
							break;
						}
					} else {
						gboolean t1t = gel_isnodetrue (t1, NULL);
						gboolean t2t = gel_isnodetrue (t2, NULL);
						if ((t1t && ! t2t) ||
						    ( ! t1t && t2t)) {
							r = FALSE;
							break;
						}
					}
				}
				if ( ! r)
					break;
			}
		}
	} else if(a->type == MATRIX_NODE) {
		GelMatrixW *m = a->mat.matrix;
		if G_UNLIKELY (gel_matrixw_width(m)>1 ||
			       gel_matrixw_height(m)>1) {
			r = FALSE;
		} else {
			GelETree *t = gel_matrixw_index(m,0,0);
			if G_UNLIKELY (t->type != VALUE_NODE &&
				       t->type != BOOL_NODE) {
				gel_errorout (_("Cannot compare non value or bool only matrixes"));
				*error = TRUE;
				return 0;
			}
			r = eqlnodes (t, b);
		}
	} else if(b->type == MATRIX_NODE) {
		GelMatrixW *m = b->mat.matrix;
		if G_UNLIKELY (gel_matrixw_width(m)>1 ||
			       gel_matrixw_height(m)>1) {
			r = FALSE;
		} else {
			GelETree *t = gel_matrixw_index(m,0,0);
			if G_UNLIKELY (t->type != VALUE_NODE &&
				       t->type != BOOL_NODE) {
				gel_errorout (_("Cannot compare non value or bool only matrixes"));
				*error = TRUE;
				return 0;
			}
			r = eqlnodes (t, a);
		}
	} else
		g_assert_not_reached();

	return r;
}

static int
cmpstring(GelETree *a, GelETree *b)
{
	int r = 0;
	if (a->type == STRING_NODE &&
	    b->type == STRING_NODE) {
		r = strcmp (a->str.str, b->str.str);
	} else if (a->type == STRING_NODE) {
		char *s = gel_string_print_etree (b);
		r = strcmp (a->str.str, s);
		g_free (s);
	} else if (b->type == STRING_NODE) {
		char *s = gel_string_print_etree (a);
		r = strcmp (s, b->str.str);
		g_free (s);
	} else {
		g_assert_not_reached();
	}

	return r;
}

static int
cmpstringop (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int ret;
	ret = cmpstring (l, r);

	freetree_full (n, TRUE, FALSE);

	if (ret > 0)
		gel_makenum_ui_from (n, 1);
	else if (ret <0 )
		gel_makenum_si_from (n, -1);
	else
		gel_makenum_ui_from (n, 0);
	return TRUE;
}

gboolean
gel_mod_integer_rational (mpw_t num, mpw_t mod)
{
	if G_UNLIKELY (mpw_is_complex (num)) {
		/* also on rationals but as integers */
		gel_errorout (_("Modulo arithmetic only works on integers"));
		return FALSE;
	} else if (mpw_is_integer (num)) {
		mpw_mod (num, num, mod);
		if (mpw_sgn (num) < 0)
			mpw_add (num, mod, num);
		if  G_UNLIKELY (gel_error_num != GEL_NO_ERROR)
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

		if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
			mpw_clear (n);
			mpw_clear (d);
			return FALSE;
		}

		mpw_invert (num, d, mod);
		if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
			mpw_clear (n);
			mpw_clear (d);
			return FALSE;
		}
		mpw_mul (num, num, n);
		mpw_mod (num, num, mod);

		if G_UNLIKELY (gel_error_num != GEL_NO_ERROR)
			return FALSE;
		else
			return TRUE;
	} else {
		/* also on rationals but as integers */
		gel_errorout (_("Modulo arithmetic only works on integers"));
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
		case E_ELTPLUS:
			mpw_add(res,ll->val.value,rr->val.value);
			break;
		case E_MINUS:
		case E_ELTMINUS:
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
			if  G_UNLIKELY ( ! gel_mod_integer_rational (res, ctx->modulo)) {
				gel_error_num = GEL_NUMERICAL_MPW_ERROR;
			}
		}
		if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
			GEL_GET_NEW_NODE(n);
			n->type = OPERATOR_NODE;
			n->op.oper = oper;
			n->op.args = gel_copynode(ll);
			n->op.args->any.next = gel_copynode(rr);
			n->op.args->any.next->any.next = NULL;
			n->op.nargs = 2;
			mpw_clear(res);
			gel_error_num = GEL_NO_ERROR;
			return n;
		}
		return gel_makenum_use(res);
	} else if ((rr->type == VALUE_NODE || rr->type == BOOL_NODE) &&
		   (ll->type == VALUE_NODE || ll->type == BOOL_NODE)) {
		gboolean lt = gel_isnodetrue (ll, NULL);
		gboolean rt = gel_isnodetrue (rr, NULL);
		gboolean res;
		gboolean got_res = FALSE;

		switch (oper) {
		case E_PLUS:
		case E_ELTPLUS:
			res = lt || rt;
			got_res = TRUE;
			break;
		case E_MINUS:
		case E_ELTMINUS:
			res = lt || ! rt;
			got_res = TRUE;
			break;
		case E_MUL:
		case E_ELTMUL:
			res = lt && rt;
			got_res = TRUE;
			break;
		default: 
			got_res = FALSE;
			res = FALSE;
			break;
		}
		if G_UNLIKELY ( ! got_res ||
			       gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
			GEL_GET_NEW_NODE(n);
			n->type = OPERATOR_NODE;
			n->op.oper = oper;
			n->op.args = gel_copynode(ll);
			n->op.args->any.next = gel_copynode(rr);
			n->op.args->any.next->any.next = NULL;
			n->op.nargs = 2;
			gel_error_num = GEL_NO_ERROR;
			return n;
		}
		return gel_makenum_bool (res);
	} else {
		/*this is the less common case so we can get around with a
		  wierd thing, we'll just make a new fake node and pretend
		  we want to evaluate that*/
		GEL_GET_NEW_NODE(n);
		n->type = OPERATOR_NODE;
		n->op.oper = oper;

		n->op.args = gel_copynode(ll);
		n->op.args->any.next = gel_copynode(rr);
		n->op.args->any.next->any.next = NULL;
		n->op.nargs = 2;

		if ( ! no_push) {
			GE_PUSH_STACK (ctx, n, GE_PRE);
		}

		return n;
	}
}


/*eltadd, eltsub, mul, div*/
static gboolean
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

	gel_matrixw_make_private(m, TRUE /* kill_type_caches */);

	for(j=0;j<gel_matrixw_height(m);j++) {
		for(i=0;i<gel_matrixw_width(m);i++) {
			GelETree *t = gel_matrixw_get_index(m,i,j);
			if(order == 0) {
				gel_matrixw_set_index(m,i,j) =
					op_two_nodes(ctx,
						     t ? t : the_zero,
						     node, n->op.oper,
						     FALSE /* no_push */);
			} else {
				gel_matrixw_set_index(m,i,j) =
					op_two_nodes(ctx,node,
						     t ? t : the_zero,
						     n->op.oper,
						     FALSE /* no_push */);
			}
			if (t != NULL)
				gel_freetree (t);
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

/* add and sub using identity for square matrices and eltbyelt for vectors */
static gboolean
matrix_addsub_scalar_matrix_op (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i;
	GelMatrixW *m;
	GelETree *node;
	int quote = 0;

	if (l->type == MATRIX_NODE) {
		m = l->mat.matrix;
		quote = l->mat.quoted;
		node = r;
	} else {
		m = r->mat.matrix;
		quote = r->mat.quoted;
		node = l;
	}

	/* If vector do the normal (element by element) scalar matrix operation */
	if (gel_matrixw_width (m) == 1 || gel_matrixw_height (m) == 1)
		return matrix_scalar_matrix_op (ctx, n, l, r);

	if G_UNLIKELY (gel_matrixw_width (m) != gel_matrixw_height (m)) {
		gel_errorout (_("Can't add/subtract a scalar to non-square matrix (A + x is defined as A + x*I)"));
		return TRUE;
	}

	gel_matrixw_make_private(m, TRUE /* kill_type_caches */);

	for (i = 0; i < gel_matrixw_width (m); i++) {
		GelETree *t = gel_matrixw_get_index(m,i,i);
		/* Only for ADD/SUB so order is unimportant */
		gel_matrixw_set_index (m, i, i) =
			op_two_nodes (ctx,
				      t ? t : the_zero,
				      node, n->op.oper,
				      FALSE /* no_push */);
		if (t != NULL)
			gel_freetree (t);
	}
	n->op.args = NULL;

	if (l->type == MATRIX_NODE) {
		replacenode (n, l);
		gel_freetree (r);
	} else {
		replacenode (n, r);
		gel_freetree (l);
	}
	return TRUE;
}

static gboolean
matrix_absnegfac_op(GelCtx *ctx, GelETree *n, GelETree *l)
{
	int i,j;
	GelMatrixW *m = l->mat.matrix;

	gel_matrixw_make_private(m, TRUE /* kill_type_caches */);

	for(j=0;j<gel_matrixw_height(m);j++) {
		for(i=0;i<gel_matrixw_width(m);i++) {
			GelETree *t = gel_matrixw_get_index(m,i,j);
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
			} else if (t->type == BOOL_NODE &&
				   n->op.oper == E_NEG) {
				t->bool_.bool_ = ! t->bool_.bool_;
			} else {
				GelETree *nn;
				GEL_GET_NEW_NODE(nn);
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

static gboolean
pure_matrix_eltbyelt_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	GelMatrixW *m1,*m2;
	m1 = l->mat.matrix;
	m2 = r->mat.matrix;
	if G_UNLIKELY ((gel_matrixw_width(m1) != gel_matrixw_width(m2)) ||
		       (gel_matrixw_height(m1) != gel_matrixw_height(m2))) {
		if (n->op.oper == E_PLUS ||
		    n->op.oper == E_ELTPLUS ||
		    n->op.oper == E_MINUS ||
		    n->op.oper == E_ELTMINUS)
			gel_errorout (_("Can't add/subtract two matricies of different sizes"));
		else
			gel_errorout (_("Can't do element by element operations on two matricies of different sizes"));
		return TRUE;
	}
	l->mat.quoted = l->mat.quoted || r->mat.quoted;
	gel_matrixw_make_private(m1, TRUE /* kill_type_caches */);
	for(j=0;j<gel_matrixw_height(m1);j++) {
		for(i=0;i<gel_matrixw_width(m1);i++) {
			GelETree *t = gel_matrixw_get_index (m1, i, j);
			gel_matrixw_set_index (m1, i, j)
				= op_two_nodes (ctx, t ? t : the_zero,
						gel_matrixw_index (m2, i, j),
						n->op.oper,
						FALSE /* no_push */);
			if (t != NULL)
				freetree_full (t, TRUE, TRUE);
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
	for(i=0;i<gel_matrixw_width(res);i++) { /* columns M2 */
		for(j=0;j<gel_matrixw_height(res);j++) { /* rows M1 */
			GelETree *a = NULL;
			for(k=0;k<gel_matrixw_width(m1);k++) { /* columns M1,
							          rows M2 */
				GelETree *t;
				GelETree *t2;
				t = op_two_nodes (ctx,
						  gel_matrixw_index (m1, k, j),
						  gel_matrixw_index (m2, i, k),
						  E_MUL,
						  TRUE /* no_push */);
				if (a == NULL) {
					a = t;
				} else {
					t2 = op_two_nodes (ctx, a, t, E_PLUS,
							   TRUE /* no_push */);
					gel_freetree (t);
					gel_freetree (a);
					a = t2;
				}
			}
			gel_matrixw_set_index (res, i, j) = a;
			if (a->type == OPERATOR_NODE) {
				GE_PUSH_STACK (ctx, a, GE_PRE);
			}
		}
	}
}

static gboolean
pure_matrix_mul_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	GelMatrixW *m, *m1,*m2;
	gboolean quote;
	m1 = l->mat.matrix;
	m2 = r->mat.matrix;
	if G_UNLIKELY ((gel_matrixw_width(m1) != gel_matrixw_height(m2))) {
		gel_errorout (_("Can't multiply matricies of wrong sizes"));
		return TRUE;
	}
	m = gel_matrixw_new();
	quote = l->mat.quoted || r->mat.quoted;
	gel_matrixw_set_size(m,gel_matrixw_width(m2),gel_matrixw_height(m1));

	/* for the puproses of cache optimization, it is more likely that
	 * we are not in modulo mode and have a value only matrix */
	if G_UNLIKELY (ctx->modulo != NULL) {
		    if (gel_is_matrix_value_only_integer (m1) &&
			gel_is_matrix_value_only_integer (m2)) {
			    gel_value_matrix_multiply (m, m1, m2, ctx->modulo);
		    } else {
			    expensive_matrix_multiply (ctx, m, m1, m2);
		    }
	} else {
		if G_LIKELY (gel_is_matrix_value_only(m1) &&
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

static gboolean
matrix_pow_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i;
	long power;
	gboolean quote;
	GelMatrixW *res = NULL;
	GelMatrixW *m;
	gboolean free_m = FALSE;
	mpw_ptr old_modulo;

	m = l->mat.matrix;
	quote = l->mat.quoted;

	if G_UNLIKELY (r->type != VALUE_NODE ||
		       mpw_is_complex(r->val.value) ||
		       !mpw_is_integer(r->val.value) ||
		       (gel_matrixw_width(m) !=
			gel_matrixw_height(m)) ||
		       !gel_is_matrix_value_only(m)) {
		gel_errorout (_("Powers are defined on (square matrix)^(integer) only"));
		return TRUE;
	}

	if G_UNLIKELY (ctx->modulo != NULL &&
		       ! gel_is_matrix_value_only_integer (m)) {
		gel_errorout (_("Powers on matrices in modulo mode are defined on integer matrices only"));
		return TRUE;
	}
	
	gel_error_num = GEL_NO_ERROR;
	power = mpw_get_long(r->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = GEL_NO_ERROR;
		gel_errorout (_("Exponent too large"));
		return TRUE;
	}
	
	if(power<=0) {
		GelMatrixW *mi;
		mi = gel_matrixw_new();
		gel_matrixw_set_size(mi,gel_matrixw_width(m),
				     gel_matrixw_height(m));
		/* width == height */
		for(i=0;i<gel_matrixw_width(m);i++)
			gel_matrixw_set_index(mi,i,i) =
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
		if G_UNLIKELY (!gel_value_matrix_gauss(ctx,m,TRUE,FALSE,TRUE,FALSE,NULL,mi)) {
			ctx->modulo = old_modulo;
			gel_errorout (_("Matrix appears singular and can't be inverted"));
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

static gboolean
pure_matrix_div_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i;
	gboolean quote;
	GelMatrixW *m1,*m2;
	GelMatrixW *mi,*toinvert;
	GelMatrixW *res;
	mpw_ptr old_modulo;

	m1 = l->mat.matrix;
	m2 = r->mat.matrix;
	quote = l->mat.quoted || r->mat.quoted;

	if G_UNLIKELY ((gel_matrixw_width(m1) !=
			gel_matrixw_height(m1)) ||
		       (gel_matrixw_width(m2) !=
			gel_matrixw_height(m2)) ||
		       (gel_matrixw_width(m1) !=
			gel_matrixw_width(m2)) ||
		       !gel_is_matrix_value_only(m1) ||
		       !gel_is_matrix_value_only(m2)) {
		gel_errorout (_("Can't divide matrices of different sizes or non-square matrices"));
		return TRUE;
	}

	mi = gel_matrixw_new();
	gel_matrixw_set_size(mi,gel_matrixw_width(m1),
			 gel_matrixw_height(m1));
	/* width == height */
	for(i=0;i<gel_matrixw_width(m1);i++)
		gel_matrixw_set_index(mi,i,i) =
			gel_makenum_ui(1);

	if(n->op.oper == E_BACK_DIV)
		toinvert = m1;
	else
		toinvert = m2;

	toinvert = gel_matrixw_copy(toinvert);
	/* FIXME: unfortunately the modulo logic of gauss is fucked */
	old_modulo = ctx->modulo;
	ctx->modulo = NULL;
	if G_UNLIKELY (!gel_value_matrix_gauss(ctx,toinvert,TRUE,FALSE,TRUE,FALSE,NULL,mi)) {
		ctx->modulo = old_modulo;
		gel_errorout (_("Matrix appears singular and can't be inverted"));
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

static gboolean
value_matrix_div_op(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	int i,j;
	gboolean quote;
	GelMatrixW *m;
	GelMatrixW *mi;
	mpw_ptr old_modulo;

	m = r->mat.matrix;
	quote = r->mat.quoted;

	if G_UNLIKELY ((gel_matrixw_width(m) !=
			gel_matrixw_height(m)) ||
		       !gel_is_matrix_value_only(m)) {
		gel_errorout (_("Can't divide by a non-square matrix"));
		return TRUE;
	}

	mi = gel_matrixw_new();
	gel_matrixw_set_size(mi,gel_matrixw_width(m),
			 gel_matrixw_height(m));
	/* width == height */
	for(i=0;i<gel_matrixw_width(m);i++)
		gel_matrixw_set_index(mi,i,i) =
			gel_makenum_ui(1);

	m = gel_matrixw_copy(m);
	/* FIXME: unfortunately the modulo logic of gauss is fucked */
	old_modulo = ctx->modulo;
	ctx->modulo = NULL;
	if G_UNLIKELY (!gel_value_matrix_gauss(ctx,m,TRUE,FALSE,TRUE,FALSE,NULL,mi)) {
		ctx->modulo = old_modulo;
		gel_errorout (_("Matrix appears singular and can't be inverted"));
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

	for(j=0;j<gel_matrixw_width(m);j++) {
		for(i=0;i<gel_matrixw_width(m);i++) {
			GelETree *t = gel_matrixw_get_index(m,i,j);
			if(t)
				mpw_mul(t->val.value,t->val.value,
					l->val.value);
		}
	}

	freetree_full(n,TRUE,FALSE);
	n->type = MATRIX_NODE;
	n->mat.matrix = m;
	n->mat.quoted = quote;
	return TRUE;
}

/*add, sub */
static gboolean
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
	gel_matrixw_make_private(m, TRUE /* kill_type_caches */);

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			GelETree *t = gel_matrixw_get_index (m, i, j);
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
			GEL_GET_NEW_NODE(nn);
			nn->type = OPERATOR_NODE;
			nn->op.oper = E_MOD_CALC;
			nn->op.args = gel_copynode (n);
			nn->op.args->any.next = gel_makenum (mod);
			nn->op.args->any.next->any.next = NULL;
			nn->op.nargs = 2;
			gel_error_num = GEL_NO_ERROR;
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
gboolean
gel_isnodetrue (GelETree *n, gboolean *bad_node)
{
	switch (n->type) {
	case NULL_NODE:
		return FALSE;
	case VALUE_NODE:
		return ! mpw_zero_p (n->val.value);
	case STRING_NODE:
		if(n->str.str && *n->str.str)
			return TRUE;
		else 
			return FALSE;
	case BOOL_NODE:
		return n->bool_.bool_;
	default:
		if (bad_node)
			*bad_node = TRUE;
		return FALSE;
	} 
}

static gboolean
transpose_matrix (GelCtx *ctx, GelETree *n, GelETree *l)
{
	l->mat.matrix->tr = !(l->mat.matrix->tr);
	/*remove from arglist*/
	n->op.args = NULL;
	replacenode(n,l);
	return TRUE;
}

static gboolean
conjugate_transpose_matrix (GelCtx *ctx, GelETree *n, GelETree *l)
{
	/* handles real case nicely */
	gel_matrix_conjugate_transpose (l->mat.matrix);

	/*remove from arglist*/
	n->op.args = NULL;
	replacenode(n,l);
	return TRUE;
}

static gboolean
string_concat (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	char *s = NULL;
	
	if (l->type == STRING_NODE &&
	    r->type == STRING_NODE) {
		s = g_strconcat (l->str.str, r->str.str, NULL);
	} else if (l->type == STRING_NODE &&
		   r->type == IDENTIFIER_NODE) {
		s = g_strconcat (l->str.str, r->id.id->token, NULL);
	} else if (r->type == STRING_NODE &&
		   l->type == IDENTIFIER_NODE) {
		s = g_strconcat (l->id.id->token, r->str.str, NULL);
	} else if (l->type == STRING_NODE) {
		char *t = gel_string_print_etree (r);
		s = g_strconcat (l->str.str, t, NULL);
		g_free (t);
	} else if (r->type == STRING_NODE) {
		char *t = gel_string_print_etree (l);
		s = g_strconcat (t, r->str.str, NULL);
		g_free (t);
	} else {
		g_assert_not_reached();
	}
	
	freetree_full (n, TRUE, FALSE);
	n->type = STRING_NODE;
	n->str.str = s;
	n->str.constant = FALSE;

	return TRUE;
}


/*for numbers*/
static void
my_mpw_back_div (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	mpw_div (rop, op2, op1);
}


#define PRIM_NUM_FUNC_1(funcname,mpwfunc) \
static gboolean							\
funcname(GelCtx *ctx, GelETree *n, GelETree *l)			\
{								\
	mpw_t res;						\
								\
	mpw_init(res);						\
	mpwfunc(res,l->val.value);				\
	if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {	\
		mpw_clear(res);					\
		gel_error_num = GEL_NO_ERROR;			\
		return TRUE;					\
	}							\
								\
	freetree_full(n,TRUE,FALSE);				\
	gel_makenum_use_from(n,res);				\
	return TRUE;						\
}
#define PRIM_NUM_FUNC_2(funcname,mpwfunc) \
static gboolean							\
funcname(GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)	\
{								\
	mpw_t res;						\
								\
	mpw_init(res);						\
	mpwfunc(res,l->val.value,r->val.value);			\
	if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {	\
		mpw_clear(res);					\
		gel_error_num = GEL_NO_ERROR;			\
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

static gboolean
numerical_pow (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	mpw_t res;

	mpw_init(res);
	if (ctx->modulo != NULL)
		mpw_powm (res, l->val.value, r->val.value, ctx->modulo);
	else
		mpw_pow (res, l->val.value, r->val.value);
	if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
		mpw_clear (res);
		gel_error_num = GEL_NO_ERROR;
		return TRUE;
	}

	freetree_full (n, TRUE, FALSE);
	gel_makenum_use_from (n, res);
	return TRUE;
}

static gboolean
boolean_add (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	gboolean lt = gel_isnodetrue (l, NULL);
	gboolean rt = gel_isnodetrue (r, NULL);

	freetree_full (n, TRUE, FALSE);
	gel_makenum_bool_from (n, lt || rt);
	return TRUE;
}

static gboolean
boolean_sub (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	gboolean lt = gel_isnodetrue (l, NULL);
	gboolean rt = gel_isnodetrue (r, NULL);

	freetree_full (n, TRUE, FALSE);
	gel_makenum_bool_from (n, lt || ! rt);
	return TRUE;
}

static gboolean
boolean_mul (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	gboolean lt = gel_isnodetrue (l, NULL);
	gboolean rt = gel_isnodetrue (r, NULL);

	freetree_full (n, TRUE, FALSE);
	gel_makenum_bool_from (n, lt && rt);
	return TRUE;
}

static gboolean
boolean_neg (GelCtx *ctx, GelETree *n, GelETree *l)
{
	gboolean lt = gel_isnodetrue (l, NULL);

	freetree_full (n, TRUE, FALSE);
	gel_makenum_bool_from (n, ! lt);
	return TRUE;
}

static GelToken *
get_fake_token (int i)
{
	static GelToken *ids[10] = { NULL, };
	if G_UNLIKELY (i >= 10) {
		GelToken *id;
		char *s = g_strdup_printf ("_x%d", i);
		id = d_intern (s);
		g_free (s);
		return id;
	}

	if G_UNLIKELY (ids[i] == NULL) {
		char *s = g_strdup_printf ("_x%d", i);
		ids[i] = d_intern (s);
		g_free (s);
	}

	return ids[i];
}

static GelETree *
make_funccall (GelEFunc *a)
{
	int i;
	GelETree *n;
	GelETree *nn;

	GEL_GET_NEW_NODE (n);
	n->type = OPERATOR_NODE;
	n->op.oper = E_DIRECTCALL;
	n->op.nargs = a->nargs+1;

	GEL_GET_NEW_NODE (nn);
	nn->type = FUNCTION_NODE;
	nn->func.func = d_copyfunc (a);
	nn->func.func->context = -1;

	n->op.args = nn;

	for (i = 0; i < a->nargs; i++) {
		GelETree *nnn;
		nnn = gel_makenum_identifier (get_fake_token (i));
		nn->any.next = nnn;
		nn = nnn;
	}
	nn->any.next = NULL;

	return n;
}

static gboolean
function_finish_bin_op (GelCtx *ctx, GelETree *n, int nargs, GelETree *la, GelETree *lb)
{
	int i;
	GSList *args;
	GelETree *nn;
	GelEFunc *f;

	GEL_GET_NEW_NODE (nn);
	nn->type = OPERATOR_NODE;
	nn->op.oper = n->op.oper;
	nn->op.args = la;
	nn->op.args->any.next = lb;
	nn->op.args->any.next->any.next = NULL;
	nn->op.nargs = 2;

	args = NULL;
	for (i = nargs -1; i >= 0; i--) {
		args = g_slist_prepend (args, get_fake_token (i));
	}

	f = d_makeufunc (NULL /* id */,
			 nn /* value */,
			 args, nargs,
			 NULL /* extra_dict */);
	freetree_full (n, TRUE /* free args */, FALSE /* kill */);
	n->type = FUNCTION_NODE;
	n->func.func = f;
	n->func.func->context = -1;

	return TRUE;
}

static gboolean
function_bin_op (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	GelETree *la, *lb;
	GelEFunc *a, *b;

	a = get_func_from (l, FALSE /* silent */);
	b = get_func_from (r, FALSE /* silent */);
	if (a == NULL || b == NULL) {
		return TRUE;
	}

	if G_UNLIKELY (a->vararg || b->vararg) {
		gel_errorout (_("Operations on functions with variable argument list not supported"));
		return TRUE;
	}

	if G_UNLIKELY (a->nargs != b->nargs) {
		gel_errorout (_("Operations on functions with different number of arguments not supported"));
		return TRUE;
	}

	la = make_funccall (a);
	lb = make_funccall (b);

	return function_finish_bin_op (ctx, n, a->nargs, la, lb);
}

static gboolean
function_something_bin_op (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	GelETree *la;
	GelEFunc *a;

	a = get_func_from (l, FALSE /* silent */);
	if (a == NULL) {
		return TRUE;
	}

	if G_UNLIKELY (a->vararg) {
		gel_errorout (_("Operations on functions with variable argument list not supported"));
		return TRUE;
	}

	la = make_funccall (a);

	return function_finish_bin_op (ctx, n, a->nargs, la, gel_copynode (r));
}

static gboolean
something_function_bin_op (GelCtx *ctx, GelETree *n, GelETree *l, GelETree *r)
{
	GelETree *lb;
	GelEFunc *b;

	b = get_func_from (r, FALSE /* silent */);
	if (b == NULL) {
		return TRUE;
	}

	if G_UNLIKELY (b->vararg) {
		gel_errorout (_("Operations on functions with variable argument list not supported"));
		return TRUE;
	}

	lb = make_funccall (b);

	return function_finish_bin_op (ctx, n, b->nargs, gel_copynode (l), lb);
}

static gboolean
function_uni_op (GelCtx *ctx, GelETree *n, GelETree *l)
{
	int i;
	GSList *args;
	GelETree *la;
	GelETree *nn;
	GelEFunc *f, *a;

	a = get_func_from (l, FALSE /* silent */);
	if (a == NULL) {
		return TRUE;
	}

	if G_UNLIKELY (a->vararg) {
		gel_errorout (_("Operations on functions with variable argument list not supported"));
		return TRUE;
	}

	la = make_funccall (a);

	GEL_GET_NEW_NODE (nn);
	nn->type = OPERATOR_NODE;
	nn->op.oper = n->op.oper;
	nn->op.args = la;
	nn->op.args->any.next = NULL;
	nn->op.nargs = 1;

	args = NULL;
	for (i = a->nargs -1; i >= 0; i--) {
		args = g_slist_prepend (args, get_fake_token (i));
	}

	f = d_makeufunc (NULL /* id */,
			 nn /* value */,
			 args, a->nargs,
			 NULL /* extra_dict */);
	freetree_full (n, TRUE /* free args */, FALSE /* kill */);
	n->type = FUNCTION_NODE;
	n->func.func = f;
	n->func.func->context = -1;

	return TRUE;
}

GelETree *
gel_function_from_function (GelEFunc *func, GelETree *l)
{
	int i;
	GSList *args;
	GelETree *la;
	GelETree *n;
	GelETree *nn;
	GelEFunc *f, *a;

	a = get_func_from (l, FALSE /* silent */);
	if (a == NULL) {
		return NULL;
	}

	if G_UNLIKELY (a->vararg) {
		gel_errorout (_("Operations on functions with variable argument list not supported"));
		return NULL;
	}

	if G_UNLIKELY (func->nargs != 1) {
		gel_errorout (_("Function creation with wrong number of arguments"));
		return NULL;
	}

	la = make_funccall (a);

	GEL_GET_NEW_NODE (n);
	n->type = FUNCTION_NODE;
	n->func.func = d_copyfunc (func);
	n->func.func->context = -1;

	GEL_GET_NEW_NODE (nn);
	nn->type = OPERATOR_NODE;
	nn->op.oper = E_DIRECTCALL;
	nn->op.args = n;
	nn->op.args->any.next = la;
	nn->op.args->any.next->any.next = NULL;
	nn->op.nargs = 2;

	args = NULL;
	for (i = a->nargs -1; i >= 0; i--) {
		args = g_slist_prepend (args, get_fake_token (i));
	}

	f = d_makeufunc (NULL /* id */,
			 nn /* value */,
			 args, a->nargs,
			 NULL /* extra_dict */);
	GEL_GET_NEW_NODE (n);
	n->type = FUNCTION_NODE;
	n->func.func = f;
	n->func.func->context = -1;

	return n;
}

	
#define EMPTY_PRIM {{{{0}}}}
/* May have to raise OP_TABLE_LEN in eval.h if you add entries below */

static const GelOper prim_table[E_OPER_LAST] = {
	/*E_SEPAR*/ EMPTY_PRIM,
	/*E_EQUALS*/ EMPTY_PRIM,
	/*E_PARAMETER*/ EMPTY_PRIM,
	/*E_ABS*/ 
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_abs},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
	 }},
	/*E_PLUS*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_add},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_addsub_scalar_matrix_op},
		 {{GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_IDENTIFIER|GO_STRING,GO_STRING,0},
			 (GelEvalFunc)string_concat},
		 {{GO_STRING,GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_IDENTIFIER|GO_STRING,0},
			 (GelEvalFunc)string_concat},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
		 {{GO_VALUE|GO_POLYNOMIAL,GO_VALUE|GO_POLYNOMIAL,0},
			 (GelEvalFunc)polynomial_add_sub_op},
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},(GelEvalFunc)boolean_add},
	 }},
	/*E_ELTPLUS*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_add},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_STRING,GO_STRING,0},
			 (GelEvalFunc)string_concat},
		 {{GO_STRING,GO_VALUE|GO_MATRIX|GO_FUNCTION|GO_STRING,0},
			 (GelEvalFunc)string_concat},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
		 {{GO_VALUE|GO_POLYNOMIAL,GO_VALUE|GO_POLYNOMIAL,0},
			 (GelEvalFunc)polynomial_add_sub_op},
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},(GelEvalFunc)boolean_add},
	 }},
	/*E_MINUS*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_sub},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_addsub_scalar_matrix_op},
		 {{GO_VALUE|GO_POLYNOMIAL,GO_VALUE|GO_POLYNOMIAL,0},
			 (GelEvalFunc)polynomial_add_sub_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},(GelEvalFunc)boolean_sub},
	 }},
	/*E_ELTMINUS*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_sub},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_VALUE|GO_POLYNOMIAL,GO_VALUE|GO_POLYNOMIAL,0},
			 (GelEvalFunc)polynomial_add_sub_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},(GelEvalFunc)boolean_sub},
	 }},
	/*E_MUL*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mul},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_mul_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},(GelEvalFunc)boolean_mul},
	 }},
	/*E_ELTMUL*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mul},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},(GelEvalFunc)boolean_mul},
	 }},
	/*E_DIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_div},
		 {{GO_MATRIX,GO_VALUE,0}, (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_VALUE,GO_MATRIX,0}, (GelEvalFunc)value_matrix_div_op},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_div_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_ELTDIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_div},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_BACK_DIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_back_div},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_div_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_ELT_BACK_DIV*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_back_div},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_MOD*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mod},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_ELTMOD*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_mod},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_NEG*/
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_neg},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
		 {{GO_BOOL,0,0},(GelEvalFunc)boolean_neg},
	 }},
	/*E_EXP*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_pow},
		 {{GO_MATRIX,GO_VALUE,0},(GelEvalFunc)matrix_pow_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_ELTEXP*/
	{{
		 {{GO_VALUE,GO_VALUE,0},(GelEvalFunc)numerical_pow},
		 {{GO_MATRIX,GO_MATRIX,0},(GelEvalFunc)pure_matrix_eltbyelt_op},
		 {{GO_VALUE|GO_MATRIX,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)matrix_scalar_matrix_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)function_bin_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,GO_VALUE|GO_MATRIX,0},
			 (GelEvalFunc)function_something_bin_op},
		 {{GO_VALUE|GO_MATRIX,GO_FUNCTION|GO_IDENTIFIER,0},
			 (GelEvalFunc)something_function_bin_op},
	 }},
	/*E_FACT*/
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_fac},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
	 }},
	/*E_DBLFACT*/
	{{
		 {{GO_VALUE,0,0},(GelEvalFunc)numerical_dblfac},
		 {{GO_MATRIX,0,0},(GelEvalFunc)matrix_absnegfac_op},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
	 }},
	/*E_TRANSPOSE*/
	{{
		 {{GO_MATRIX,0,0},(GelEvalFunc)transpose_matrix},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
	 }},
	/*E_CONJUGATE_TRANSPOSE*/
	{{
		 {{GO_MATRIX,0,0},(GelEvalFunc)conjugate_transpose_matrix},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
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
		 {{GO_VALUE|GO_STRING|GO_BOOL,GO_VALUE|GO_STRING|GO_BOOL,0},
			 (GelEvalFunc)logicalxorop},
	 }},
	/*E_LOGICAL_NOT*/
	{{
		 {{GO_VALUE|GO_STRING|GO_BOOL,0,0},(GelEvalFunc)logicalnotop},
		 {{GO_FUNCTION|GO_IDENTIFIER,0,0},
			 (GelEvalFunc)function_uni_op},
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
	/*E_DEFEQUALS*/ EMPTY_PRIM,
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
	/* FIXME: we should have some sort of compression stuff, but
	   we allocate these in chunks, so normally we can never free
	   them again.  We could use the type field to mark things
	   and then do some compression. */
#if 0
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
	while(gel_free_trees) {
		GelETree *et = gel_free_trees;
		gel_free_trees = gel_free_trees->any.next;
		g_free(et);
	}
#endif
}

static inline GelEvalLoop *
evl_new (GelETree *cond, GelETree *body, gboolean is_while, gboolean body_first)
{
	GelEvalLoop *evl;
#ifdef MEM_DEBUG_FRIENDLY
	evl = g_new0 (GelEvalLoop, 1);
#else
	if G_UNLIKELY (free_evl == NULL)
		_gel_make_free_evl ();
	evl = free_evl;
	free_evl = (GelEvalLoop *)free_evl->condition;
#endif
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
	memset (evl, 0xaa, sizeof (GelEvalLoop));
# ifndef MEM_DEBUG_SUPER_FRIENDLY
	g_free (evl);
# endif
#else
	evl->condition = (gpointer)free_evl;
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
	 mpw_ptr x,
	 mpw_ptr to,
	 mpw_ptr by,
	 gint8 init_cmp,
	 GelETree *body,
	 GelETree *orig_body,
	 GelToken *id)
{
	GelEvalFor *evf;
#ifdef MEM_DEBUG_FRIENDLY
	evf = g_new0 (GelEvalFor, 1);
#else
	if G_UNLIKELY (free_evf == NULL)
		_gel_make_free_evf ();
	evf = free_evf;
	free_evf = (GelEvalFor *)free_evf->body;
#endif
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
	memset (evf, 0xaa, sizeof (GelEvalFor));
# ifndef MEM_DEBUG_SUPER_FRIENDLY
	g_free (evf);
# endif
#else
	evf->body = (gpointer)free_evf;
	free_evf = evf;
#endif
}

static inline GelEvalForIn *
evfi_new (GelEvalForType type, GelMatrixW *mat, GelETree *body, GelETree *orig_body, GelToken *id)
{
	GelEvalForIn *evfi;
#ifdef MEM_DEBUG_FRIENDLY
	evfi = g_new0 (GelEvalForIn, 1);
#else
	if G_UNLIKELY (free_evfi == NULL)
		_gel_make_free_evfi ();
	evfi = free_evfi;
	free_evfi = (GelEvalForIn *)free_evfi->body;
#endif
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
	memset (evfi, 0xaa, sizeof (GelEvalForIn));
# ifndef MEM_DEBUG_SUPER_FRIENDLY
	g_free (evfi);
# endif
#else
	evfi->body = (gpointer)free_evfi;
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
					    gel_copynode (f->data.user),
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
		gboolean exception = FALSE;

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
		/* interruption happened during the function, which
		   means an exception */
		if G_UNLIKELY (gel_interrupted) {
			exception = TRUE;
		}
		if G_UNLIKELY (exception) {
			if(ret)
				gel_freetree(ret);
			return FALSE;
		} else if(ret) {
			replacenode(n,ret);
		}
	} else if(f->type == GEL_REFERENCE_FUNC) {
		GelETree *i;
		f = f->data.ref;
		
		GEL_GET_NEW_NODE(i);
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
		gel_errorout (_("Unevaluatable function type encountered!"));
	return TRUE;
}

char *
gel_similar_possible_ids (const char *id)
{
	GSList *similar, *li;
	GString *sim;

	similar = d_find_similar_globals (id);

	if (similar == NULL)
		return NULL;

	sim = g_string_new ("'");

	for (li = similar; li != NULL; li = li->next) {
		const char *id = li->data;

		if (li->next == NULL &&
		    li != similar)
			g_string_append (sim, _("' or '"));
		else if (li != similar)
			g_string_append (sim, "', '");

		g_string_append (sim, id);

		li->data = NULL; /* paranoia */
	}
	g_slist_free (similar);

	g_string_append (sim, "'");

	return g_string_free (sim, FALSE);
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
	if G_UNLIKELY (f == NULL) {
		char *similar;
		if (strcmp (n->id.id->token, "i") == 0) {
			gel_errorout (_("Variable 'i' used uninitialized.  "
					"Perhaps you meant to write '1i' for "
					"the imaginary number (square root of "
					"-1)."));
		} else if ((similar = gel_similar_possible_ids (n->id.id->token))
			       != NULL) {
			gel_errorout (_("Variable '%s' used uninitialized, "
					"perhaps you meant %s."),
				      n->id.id->token,
				      similar);

			g_free (similar);
		} else {
			gel_errorout (_("Variable '%s' used uninitialized"),
				      n->id.id->token);
		}
		return TRUE;
	} else {
		return iter_do_var(ctx,n,f);
	}
}

static inline gboolean
iter_derefvarop(GelCtx *ctx, GelETree *n)
{
	GelEFunc *f;
	GelETree *l;
	
	GEL_GET_L(n,l);
	
	f = d_lookup_global(l->id.id);
	if G_UNLIKELY (f == NULL) {
		char *similar = gel_similar_possible_ids (l->id.id->token);
		if (similar != NULL) {
			gel_errorout (_("Variable '%s' used uninitialized, "
					"perhaps you meant %s."),
				      l->id.id->token,
				      similar);

			g_free (similar);
		} else {
			gel_errorout (_("Variable '%s' used uninitialized"),
				      l->id.id->token);
		}
	} else if G_UNLIKELY (f->nargs != 0) {
		gel_errorout (_("Call of '%s' with the wrong number of arguments!\n"
				"(should be %d)"), f->id ? f->id->token : "anonymous", f->nargs);
	} else if G_UNLIKELY (f->type != GEL_REFERENCE_FUNC) {
		gel_errorout (_("Trying to dereference '%s' which is not a reference!\n"),
			      f->id ? f->id->token : "anonymous");
	} else /*if(f->type == GEL_REFERENCE_FUNC)*/ {
		f = f->data.ref;
		if G_UNLIKELY (f == NULL)
			gel_errorout (_("NULL reference encountered!"));
		else
			return iter_do_var(ctx,n,f);
	}
	return TRUE;
}

#define RET_RES(x) \
	freetree_full(n,TRUE,FALSE);	\
	gel_makenum_bool_from(n,x);	\
	return;

/*returns 0 if all numeric, 1 if numeric/matrix, 2 if contains string, 3 otherwise*/
static int
arglevel (GelETree *r, int cnt, gboolean bool_ok)
{
	int i;
	int level = 0;
	for(i=0;i<cnt;i++,r = r->any.next) {
		if (r->type == VALUE_NODE)
			continue;
		if (bool_ok && r->type == BOOL_NODE)
			continue;

		if(r->type==MATRIX_NODE)
			level = level<1?1:level;
		else if(r->type==STRING_NODE)
			level = 2;
		else
			return 3;
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
		gboolean err = FALSE;
		GelETree *l = ali,*r = ali->any.next;
		gboolean bool_ok = (oper == E_EQ_CMP ||
				    oper == E_NE_CMP);

		switch (arglevel (ali,
				  2,
				  bool_ok)) {
		case 0:
			switch(oper) {
			case E_EQ_CMP:
				if ( ! eqlnodes (l, r)) {
					if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
						gel_error_num = GEL_NO_ERROR;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_NE_CMP:
				if (eqlnodes (l, r)) {
					RET_RES(0)
				} else if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
					gel_error_num = GEL_NO_ERROR;
					return;
				}
				break;
			case E_LT_CMP:
				if(cmpnodes(l,r)>=0) {
					if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
						gel_error_num = GEL_NO_ERROR;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_GT_CMP:
				if(cmpnodes(l,r)<=0) {
					if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
						gel_error_num = GEL_NO_ERROR;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_LE_CMP:
				if(cmpnodes(l,r)>0) {
					RET_RES(0)
				} else if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
					gel_error_num = GEL_NO_ERROR;
					return;
				}
				break;
			case E_GE_CMP:
				if(cmpnodes(l,r)<0) {
					RET_RES(0)
				} else if G_UNLIKELY (gel_error_num != GEL_NO_ERROR) {
					gel_error_num = GEL_NO_ERROR;
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
					if G_UNLIKELY (err) {
						gel_error_num = GEL_NO_ERROR;
						return;
					}
					RET_RES(0)
				}
				break;
			case E_NE_CMP:
				if(eqmatrix(l,r,&err)) {
					RET_RES(0)
				} else if G_UNLIKELY (err) {
					gel_error_num = GEL_NO_ERROR;
					return;
				}
				break;
			default:
				gel_errorout (_("Cannot compare matrixes"));
				gel_error_num = GEL_NO_ERROR;
				return;
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
			gel_errorout (_("Primitives must get numeric/matrix/string arguments"));
			gel_error_num = GEL_NO_ERROR;
			return;
		}
	}
	RET_RES(1)
}

#undef RET_RES

static inline void
pop_stack_with_whack (GelCtx *ctx)
{
	gpointer data;
	int flag;

	GE_POP_STACK (ctx, data, flag);
	if (flag == (GE_POST | GE_WHACKARG) ||
	    flag == (GE_PRE | GE_WHACKARG)) {
		gel_freetree (data);
	}
}


/* free a special stack entry */
static inline void
ev_free_special_data(GelCtx *ctx, gpointer data, int flag)
{
	switch(flag) {
	case (GE_POST | GE_WHACKARG):
	case (GE_PRE | GE_WHACKARG):
		/* WHACKWHACK */
		gel_freetree (data);
		break;
	case GE_FUNCCALL:
		/*we are crossing a boundary, we need to free a context*/
		d_popcontext ();
		gel_freetree (data);
		pop_stack_with_whack (ctx);
		break;
	case GE_LOOP_COND:
	case GE_LOOP_LOOP:
		{
			GelEvalLoop *evl = data;
			gel_freetree (evl->condition);
			gel_freetree (evl->body);
			evl_free (evl);
			pop_stack_with_whack (ctx);
		}
		break;
	case GE_FOR:
		{
			GelEvalFor *evf = data;
			gel_freetree(evf->body);
			gel_freetree(evf->result);
			evf_free(evf);
			pop_stack_with_whack (ctx);
		}
		break;
	case GE_FORIN:
		{
			GelEvalForIn *evfi = data;
			gel_freetree(evfi->body);
			gel_freetree(evfi->result);
			evfi_free(evfi);
			pop_stack_with_whack (ctx);
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
push_setmod (GelCtx *ctx, GelETree *n, gboolean whackarg)
{
	GelETree *l, *r;

	GEL_GET_LR (n, l, r);

	if G_UNLIKELY (r->type != VALUE_NODE ||
		       mpw_is_complex (r->val.value) ||
		       ! mpw_is_integer (r->val.value) ||
		       mpw_sgn (r->val.value) <= 0) {
		gel_errorout (_("Bad argument to modular operation"));
		return FALSE;
	}

	GE_PUSH_STACK (ctx, n, GE_ADDWHACKARG (GE_POST, whackarg));
	GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);

	ctx->modulo = g_new (struct _mpw_t, 1);
	mpw_init_set_no_uncomplex (ctx->modulo, r->val.value);

	ctx->post = FALSE;
	ctx->current = l;
	ctx->whackarg = FALSE;

	return TRUE;
}

static void
iter_pop_stack(GelCtx *ctx)
{
	gpointer data;
	int flag;
	EDEBUG("---- iter_pop_stack ----");

#ifdef MEM_DEBUG_FRIENDLY
	ctx->current = NULL;
	ctx->post = FALSE;
	ctx->whackarg = FALSE;
#endif
	
	for(;;) {
		GE_POP_STACK(ctx,data,flag);
#ifdef EVAL_DEBUG
		printf ("  ---- stack pop %p %d ----", data, flag);
#endif
		switch(flag & GE_MASK) {
		case GE_EMPTY_STACK:
			EDEBUG("   POPPED AN EMPTY STACK");
			ctx->current = NULL;
			ctx->whackarg = FALSE;
			return;
		case GE_PRE:
			ctx->post = FALSE;
			ctx->current = data;
			ctx->whackarg = (flag & GE_WHACKARG);
#ifdef EVAL_DEBUG
			printf("   POPPED A PRE NODE(%d) whack %d\n",
			       ctx->current->type, ctx->whackarg);
#endif
			return;
		case GE_POST:
			ctx->post = TRUE;
			ctx->current = data;
			ctx->whackarg = (flag & GE_WHACKARG);
#ifdef EVAL_DEBUG
			printf("   POPPED A POST NODE(%d) whack %d\n",
			       ctx->current->type, ctx->whackarg);
#endif
			return;
		case GE_AND:
		case GE_OR:
			{
				GelETree *li = data;
				gboolean ret;
				gboolean bad_node = FALSE;
				EDEBUG("    POPPED AN OR or AND");
				ret = gel_isnodetrue(li,&bad_node);
				if G_UNLIKELY (bad_node || gel_error_num) {
					int n_flag;
					EDEBUG("    AND/OR BAD BAD NODE");
					gel_error_num = GEL_NO_ERROR;

					GE_POP_STACK (ctx, data, n_flag);
					if (n_flag & GE_WHACKARG) {
						gel_freetree (data);
					}
					break;
				}
				if((flag==GE_AND && !ret) ||
				   (flag==GE_OR && ret)) {
					int n_flag;
					GE_POP_STACK(ctx,data,n_flag);
					g_assert((n_flag & GE_MASK) == GE_POST);
					if (n_flag & GE_WHACKARG) {
						gel_freetree (data);
					} else {
						freetree_full (data, TRUE, FALSE);
						if(flag==GE_AND)
							gel_makenum_bool_from(data,0);
						else
							gel_makenum_bool_from(data,1);
					}
					EDEBUG("    AND/OR EARLY DONE");
					break;
				}
				li = li->any.next;
				if(!li) {
					int n_flag;
					GE_POP_STACK(ctx,data,n_flag);
					g_assert((n_flag & GE_MASK) == GE_POST);
					if (n_flag & GE_WHACKARG) {
						gel_freetree (data);
					} else {
						freetree_full (data, TRUE, FALSE);
						if(flag==GE_AND)
							gel_makenum_bool_from(data,1);
						else
							gel_makenum_bool_from(data,0);
					}
					EDEBUG("    AND/OR ALL THE WAY DONE");
					break;
				}
				GE_PUSH_STACK(ctx,li,flag);
				ctx->post = FALSE;
				ctx->current = li;
				ctx->whackarg = FALSE;
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
				g_assert (call != NULL);
				if (flag & GE_WHACKARG) {
					/* WHACKWHACK */
					gel_freetree (call);
					gel_freetree (data);
				} else {
					if (ctx->modulo != NULL)
						mod_node (data, ctx->modulo);
					replacenode(call,data);
				}
			}
			break;
		case GE_LOOP_COND:
			/*this was the condition of a while or until loop*/
			{
				GelEvalLoop *evl = data;
				GelETree *n;
				gboolean ret, bad_node = FALSE;
				int n_flag;
				g_assert(evl->condition);

				/*next MUST be the original node*/
				GE_PEEK_STACK(ctx,n,n_flag);
				g_assert ((n_flag & GE_MASK) == GE_POST);

				EDEBUG("    LOOP CONDITION CHECK");
				ret = gel_isnodetrue(evl->condition,&bad_node);
				if G_UNLIKELY (bad_node || gel_error_num) {
					EDEBUG("    LOOP CONDITION BAD BAD NODE");
					gel_error_num = GEL_NO_ERROR;
					replacenode (n->op.args, evl->condition);
					gel_freetree (evl->body);
					evl_free (evl);
					GE_BLIND_POP_STACK(ctx);
					if (n_flag & GE_WHACKARG) {
						/* WHACKWHACK */
						gel_freetree (n);
					}
					break;
				}
				/*check if we should continue the loop*/
				if((evl->is_while && ret) ||
				   (!evl->is_while && !ret)) {
					GelETree *l,*r;
					EDEBUG("    LOOP CONDITION MET");
					GEL_GET_LR(n,l,r);
					gel_freetree (evl->condition);
					evl->condition = NULL;
					gel_freetree (evl->body);
					if (evl->body_first)
						evl->body = gel_copynode (l);
					else
						evl->body = gel_copynode (r);
					ctx->current = evl->body;
					ctx->post = FALSE;
					ctx->whackarg = FALSE;
					GE_PUSH_STACK(ctx,evl,GE_LOOP_LOOP);
					return;
				} else {
					GelETree *b;
					EDEBUG("    LOOP CONDITION NOT MET");
					/*condition not met, so return the body*/
					gel_freetree (evl->condition);
					b = evl->body;
					evl_free (evl);
					GE_BLIND_POP_STACK (ctx);
					if (n_flag & GE_WHACKARG) {
						/* WHACKWHACK */
						gel_freetree (n);
						gel_freetree (b);
					} else if (b == NULL) {
						EDEBUG("     NULL BODY");
						freetree_full (n, TRUE, FALSE);
						n->type = NULL_NODE;
					} else {
						replacenode (n, b);
					}
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
				g_assert ((n_flag & GE_MASK) == GE_POST);

				EDEBUG("    LOOP LOOP BODY FINISHED");

				GEL_GET_LR(n,l,r);
				gel_freetree (evl->condition);
				if (evl->body_first)
					evl->condition = gel_copynode (r);
				else
					evl->condition = gel_copynode (l);
				ctx->current = evl->condition;
				ctx->post = FALSE;
				ctx->whackarg = FALSE;
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
					g_assert ((flag & GE_MASK) == GE_POST);
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
					if (res->type == VALUE_NODE ||
					    res->type == NULL_NODE ||
					    res->type == BOOL_NODE ||
					    res->type == STRING_NODE) {
						if (flag & GE_WHACKARG) {
							/* WHACKWHACK */
							gel_freetree (data);
							gel_freetree (res);
						} else {
							replacenode (data, res);
						}
						evf_free (evf);
						break;
					} else {
						replacenode (data, res);
						ctx->current = data;
						ctx->post = FALSE;
						ctx->whackarg =
							(flag & GE_WHACKARG);
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
					if (evf->body != NULL) {
						gel_freetree (evf->body);
					}
					evf->body = gel_copynode (evf->orig_body);
					ctx->current = evf->body;
					ctx->post = FALSE;
					ctx->whackarg = FALSE;
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
					      gel_copynode(gel_matrixw_index(evfi->mat,
							     evfi->i,evfi->j))));
					gel_freetree(evfi->body);
					evfi->body = gel_copynode(evfi->orig_body);
					ctx->current = evfi->body;
					ctx->post = FALSE;
					ctx->whackarg = FALSE;
					return;
				/*if we are done*/
				} else {
					GelETree *res;
					GE_POP_STACK(ctx,data,flag);
					g_assert ((flag & GE_MASK) == GE_POST);
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
					if (res->type == VALUE_NODE ||
					    res->type == NULL_NODE ||
					    res->type == BOOL_NODE ||
					    res->type == STRING_NODE) {
						if (flag & GE_WHACKARG) {
							/* WHACKWHACK */
							gel_freetree (data);
							gel_freetree (res);
						} else {
							replacenode (data, res);
						}
						evfi_free (evfi);
						break;
					} else {
						replacenode (data, res);
						ctx->current = data;
						ctx->post = FALSE;
						ctx->whackarg = 
							(flag & GE_WHACKARG);
						evfi_free (evfi);
						return;
					}
				}
			}
		case GE_MODULOOP:
			if (push_setmod (ctx, data, flag & GE_WHACKARG))
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
  go into "pre" mode and push all other ones,
  and adds the GE_WHACKARG so that we free unused thingies
  earlier from separators, expects at least two arguments!!!!,
  else first argument will be whacked */
static inline GelETree *
iter_push_args_whack(GelCtx *ctx, GelETree *args, int n)
{
	GelETree *t = args;

	ctx->post = FALSE;
	ctx->current = args;
	ctx->whackarg = TRUE;

	switch (n) {
	case 0:
	case 1:
		g_assert_not_reached ();
	case 2:
		t = args->any.next;
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE);
		break;
	case 3:
		t = args->any.next->any.next;
		GE_PUSH_STACK (ctx, args->any.next->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE | GE_WHACKARG);
		break;
	case 4:
		t = args->any.next->any.next->any.next;
		GE_PUSH_STACK (ctx, args->any.next->any.next->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args->any.next->any.next,
			       GE_PRE | GE_WHACKARG);
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE | GE_WHACKARG);
		break;
	case 5:
		t = args->any.next->any.next->any.next->any.next;
		GE_PUSH_STACK (ctx, args->any.next->any.next->any.next->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args->any.next->any.next->any.next,
			       GE_PRE | GE_WHACKARG);
		GE_PUSH_STACK (ctx, args->any.next->any.next,
			       GE_PRE | GE_WHACKARG);
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE | GE_WHACKARG);
		break;
	default:
		{
			int i;
			GelETree *li;
			GSList *list = NULL, *sli;

			li = args->any.next;
			for (i = 1; i < n; i++) {
				list = g_slist_prepend (list, li);
				li = li->any.next;
			}

			t = list->data;
			GE_PUSH_STACK (ctx, t, GE_PRE);
#ifdef MEM_DEBUG_FRIENDLY
			list->data = NULL;
#endif

			for (sli = list->next; sli != NULL; sli = sli->next) {
				GE_PUSH_STACK (ctx, sli->data,
					       GE_PRE | GE_WHACKARG);
#ifdef MEM_DEBUG_FRIENDLY
				sli->data = NULL;
#endif
			}
			g_slist_free (list);
		}
		break;
	}

	return t;
}

/* push n of the arguments on the stack */
static inline void
pushstack_n_args (GelCtx *ctx, GelETree *args, int n)
{
	switch (n) {
	case 0: break;
	case 1:
		GE_PUSH_STACK (ctx, args, GE_PRE);
		break;
	case 2:
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args, GE_PRE);
		break;
	case 3:
		GE_PUSH_STACK (ctx, args->any.next->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args, GE_PRE);
		break;
	case 4:
		GE_PUSH_STACK (ctx, args->any.next->any.next->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args->any.next->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args->any.next, GE_PRE);
		GE_PUSH_STACK (ctx, args, GE_PRE);
		break;
	default:
		{
			int i;
			GelETree *li;
			GSList *list = NULL, *sli;

			li = args;
			for (i = 0; i < n; i++) {
				list = g_slist_prepend (list, li);
				li = li->any.next;
			}

			for (sli = list; sli != NULL; sli = sli->next) {
				GE_PUSH_STACK (ctx, sli->data, GE_PRE);
#ifdef MEM_DEBUG_FRIENDLY
				sli->data = NULL;
#endif
			}
			g_slist_free (list);
		}
		break;
	}
}


/*make first argument the "current",
  go into "pre" mode and push all other ones*/
static inline void
iter_push_args(GelCtx *ctx, GelETree *args, int n)
{
	ctx->post = FALSE;
	ctx->current = args;
	ctx->whackarg = FALSE;

	pushstack_n_args (ctx, args->any.next, n-1);
}

/*make first argument the "current",
 *and push all other args.  evaluate with no modulo. */
static inline void
iter_push_args_no_modulo (GelCtx *ctx, GelETree *args, int n)
{
	ctx->post = FALSE;
	ctx->current = args;
	ctx->whackarg = FALSE;

	if (ctx->modulo != NULL) {
		GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);

		/* Make modulo NULL */
		ctx->modulo = NULL;
	}

	pushstack_n_args (ctx, args->any.next, n-1);
}

/*make first argument the "current",
  push no modulo on the second argument */
static inline void
iter_push_two_args_no_modulo_on_2 (GelCtx *ctx, GelETree *args)
{
	ctx->post = FALSE;
	ctx->current = args;
	ctx->whackarg = FALSE;

	if (ctx->modulo != NULL) {
		mpw_ptr ptr = g_new (struct _mpw_t, 1);
		mpw_init_set_no_uncomplex (ptr, ctx->modulo);

		GE_PUSH_STACK (ctx, ptr, GE_SETMODULO);
	}
	GE_PUSH_STACK(ctx, args->any.next, GE_PRE);
	g_assert (args->any.next->any.next == NULL);
	if (ctx->modulo != NULL) {
		GE_PUSH_STACK (ctx, NULL, GE_SETMODULO);
	}
}

/*when a matrix contains other things than NULLs, VALUEs, and STRINGs,
  make a copy of it and evaluate it's nodes*/
static inline void
iter_push_matrix(GelCtx *ctx, GelETree *n, GelMatrixW *m)
{
	int x,y;
	int w,h;
	GelETree *t;
	gboolean pushed = FALSE;

	w = gel_matrixw_width(m);
	h = gel_matrixw_height(m);
	for (y = h-1; y >= 0; y--) {
		for (x = w-1; x >= 0; x--) {
			t = gel_matrixw_get_index (m, x, y);
			if (t != NULL &&
			    t->type != NULL_NODE &&
			    t->type != BOOL_NODE &&
			    t->type != VALUE_NODE &&
			    t->type != STRING_NODE &&
			    t->type != USERTYPE_NODE) {
				if ( ! pushed) {
					/*make us a private copy!*/
					gel_matrixw_make_private (m, TRUE /* kill_type_caches */);

					/* it will be a copy */
					t = gel_matrixw_get_index (m, x, y);

					GE_PUSH_STACK (ctx, n,
					   GE_ADDWHACKARG (GE_POST,
							   ctx->whackarg));
					pushed = TRUE;
				}
				GE_PUSH_STACK(ctx,t,GE_PRE);
			}
		}
	}
	if (pushed) {
		int flag;
		ctx->post = FALSE;
		/* will pop the last thing which was t in PRE mode */
		GE_POP_STACK (ctx, ctx->current, flag);
		ctx->whackarg = FALSE;
	} else {
		/*if we haven't pushed ourselves,
		 * then just put us in post mode*/
		ctx->post = TRUE;
	}
}

static GelEFunc *
get_func_from (GelETree *l, gboolean silent)
{
	GelEFunc *f;

	if(l->type == IDENTIFIER_NODE) {
		f = d_lookup_global(l->id.id);
		if (f == NULL) {
			if G_UNLIKELY ( ! silent) {
				char * similar =
					gel_similar_possible_ids (l->id.id->token);
				if (similar != NULL) {
					gel_errorout (_("Function '%s' used uninitialized, "
							"perhaps you meant %s."),
						      l->id.id->token,
						      similar);

					g_free (similar);
				} else {
					gel_errorout (_("Function '%s' used uninitialized"),
						      l->id.id->token);
				}
			}
			return NULL;
		}
	} else if(l->type == FUNCTION_NODE) {
		f = l->func.func;
	} else if(l->type == OPERATOR_NODE &&
		l->op.oper == E_DEREFERENCE) {
		GelETree *ll;
		GEL_GET_L(l,ll);
		f = d_lookup_global(ll->id.id);
		if (f == NULL) {
			if G_UNLIKELY ( ! silent) {
				gel_errorout (_("Variable '%s' used uninitialized"),
					      ll->id.id->token);
			}
			return NULL;
		} else if(f->type != GEL_REFERENCE_FUNC) {
			if G_UNLIKELY ( ! silent) {
				gel_errorout (_("Can't dereference '%s'!"),
					      ll->id.id->token);
			}
			return NULL;
		}
		f = f->data.ref;
	} else {
		if G_UNLIKELY ( ! silent)
			gel_errorout (_("Can't call a non-function!"));
		return NULL;
	}
	return f;
}

static GelEFunc *
get_func_from_arg (GelETree *n, gboolean silent)
{
	GelETree *l;

	GEL_GET_L (n,l);
	return get_func_from (l, silent);
}

static gboolean
iter_funccallop(GelCtx *ctx, GelETree *n, gboolean *repushed)
{
	GelEFunc *f;
	
	EDEBUG("    FUNCCALL");

	f = get_func_from_arg (n, FALSE /* silent */);
	if (f == NULL)
		goto funccall_done_ok;
	
	if G_UNLIKELY ((f->vararg && f->nargs > n->op.nargs) ||
		       (! f->vararg && f->nargs != n->op.nargs - 1)) {
		if ( ! f->vararg)
			gel_errorout (_("Call of '%s' with the wrong number of arguments!\n"
					"(should be %d)"),
				      f->id != NULL ? f->id->token : "anonymous",
				      f->nargs);
		else
			gel_errorout (_("Call of '%s' with the wrong number of arguments!\n"
				      "(should be greater than %d)"),
				    f->id != NULL ? f->id->token : "anonymous",
				    f->nargs-2);
		goto funccall_done_ok;
	}

	switch (f->type) {
	case GEL_USER_FUNC:
	case GEL_VARIABLE_FUNC:
	{
		GSList *li;
		GelETree *ali;
		GelToken *last_arg = NULL;

		EDEBUG("     USER FUNC PUSHING CONTEXT");

		d_addcontext_named (f->id);

		EDEBUG("     USER FUNC CONTEXT PUSHED TO ADD EXTRA DICT");

		/* add extra dictionary stuff */
		for (li = f->extra_dict; li != NULL; li = li->next) {
			GelEFunc *func = d_copyfunc (li->data);
			func->context = d_curcontext ();
			d_addfunc (func);
		}

		EDEBUG("     USER FUNC EXTRA DICT ADDED, TO PUSH ARGS ON CONTEXT STACK");

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
				if G_UNLIKELY (rf == NULL) {
					d_popcontext ();
					gel_errorout (_("Referencing an undefined variable %s!"), t->id.id->token);
					goto funccall_done_ok;
				}
				d_addfunc(d_makereffunc(li->data,rf));
			} else {
				d_addfunc(d_makevfunc(li->data,gel_copynode(ali)));
			}
			li = li->next;
			if (li == NULL)
				break;
		}

		EDEBUG("     USER FUNC ABOUT TO HANDLE VARARG");

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
					gel_matrix_index (m, i++, 0) = gel_copynode (ali);
				}

				GEL_GET_NEW_NODE (nn);
				nn->type = MATRIX_NODE;
				nn->mat.quoted = FALSE;
				nn->mat.matrix = gel_matrixw_new_with_matrix (m);

				d_addfunc (d_makevfunc (last_arg, nn));
			}
		}

		EDEBUG("     USER FUNC ABOUT TO ENSURE BODY");

		D_ENSURE_USER_BODY (f);
		
		/*push self as post AGAIN*/
		GE_PUSH_STACK (ctx, ctx->current,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		*repushed = TRUE;

		/*the next to be evaluated is the body*/
		ctx->post = FALSE;
		ctx->current = gel_copynode(f->data.user);
		ctx->whackarg = FALSE;
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
	}
	case GEL_BUILTIN_FUNC:
	{
		gboolean exception = FALSE;
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
			r = g_alloca (sizeof (GelETree *) * n->op.nargs);
			for(i=0,li=n->op.args->any.next;li;i++,li=li->any.next)
				r[i] = li;
			r[i] = NULL;

			/*
			 * Note that we ARE allowing for the function to modify
			 * the arguments.  This can be used for optimization
			 * such as the Identity function.  The function should
			 * however not just steal the GelETree, it should replace
			 * it with a NULL_NODE or some such.
			 */

			ret = (*f->data.func)(ctx,r,&exception);
		} else {
			ret = (*f->data.func)(ctx,NULL,&exception);
		}
		if ( ! f->propagate_mod) {
			g_assert (ctx->modulo == NULL);
			ctx->modulo = old_modulo;
		}
		/* interruption happened during the function, which
		   means an exception */
		if G_UNLIKELY (gel_interrupted) {
			exception = TRUE;
		}
		if G_UNLIKELY (exception) {
			if(ret)
				gel_freetree(ret);
			return FALSE;
		} else if(ret) {
			if (ctx->modulo != NULL)
				mod_node (ret, ctx->modulo);
			replacenode (n, ret);
		}
		break;
	}
	case GEL_REFERENCE_FUNC:
	{
		GelETree *id;
		if G_UNLIKELY (f->nargs > 0) {
			gel_errorout (_("Reference function with arguments encountered!"));
			goto funccall_done_ok;
		}
		f = f->data.ref;
		if G_UNLIKELY (f->id == NULL) {
			gel_errorout (_("Unnamed reference function encountered!"));
			goto funccall_done_ok;
		}
		
		GEL_GET_NEW_NODE(id);
		id->type = IDENTIFIER_NODE;
		id->id.id = f->id; /*this WILL have an id*/
		id->any.next = NULL;

		freetree_full(n,TRUE,FALSE);
		n->type = OPERATOR_NODE;
		n->op.oper = E_REFERENCE;

		n->op.args = id;
		n->op.nargs = 1;
		break;
	}
	default:
		gel_errorout (_("Unevaluatable function type encountered!"));
		break;
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
#ifdef MEM_DEBUG_FRIENDLY
	ctx->current = NULL;
#endif
	EDEBUG("  RETURN");
	for(;;) {
		int flag;
		gpointer data;
		GE_POP_STACK(ctx,data,flag);
		EDEBUG("   POPPED STACK");
		if((flag & GE_MASK) == GE_EMPTY_STACK) {
			EDEBUG("    EMPTY");
			break;
		} else if((flag & GE_MASK) == GE_FUNCCALL) {
			GelETree *fn;
			GE_POP_STACK(ctx,fn,flag);
			g_assert(fn);
			EDEBUG("    FOUND FUNCCCALL");
			gel_freetree(data);
			if (flag & GE_WHACKARG) {
				EDEBUG("     WHACKING RETURN STUFF");
				/* WHACKWHACK */
				gel_freetree (fn);
				gel_freetree (r);
			} else {
				if (ctx->modulo != NULL)
					mod_node (r, ctx->modulo);
				replacenode(fn,r);
			}

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
	ctx->whackarg = FALSE;
	replacenode(ctx->res,r);
}

static inline void
iter_forloop (GelCtx *ctx, GelETree *n, gboolean *repushed)
{
	GelEvalFor *evf;
	GelEvalForType type = GEL_EVAL_FOR;
	GelETree *from=NULL,*to=NULL,*by=NULL,*body=NULL,*ident=NULL;
	gint8 init_cmp;

	switch (n->op.oper) {
	case E_FOR_CONS:
		type = GEL_EVAL_FOR;
		GEL_GET_ABCD(n,ident,from,to,body);
		break;
	case E_SUM_CONS:
		type = GEL_EVAL_SUM;
		GEL_GET_ABCD(n,ident,from,to,body);
		break;
	case E_PROD_CONS:
		type = GEL_EVAL_PROD;
		GEL_GET_ABCD(n,ident,from,to,body);
		break;
	case E_FORBY_CONS:
		type = GEL_EVAL_FOR;
		GEL_GET_ABCDE(n,ident,from,to,by,body);
		break;
	case E_SUMBY_CONS:
		type = GEL_EVAL_SUM;
		GEL_GET_ABCDE(n,ident,from,to,by,body);
		break;
	case E_PRODBY_CONS:
		type = GEL_EVAL_PROD;
		GEL_GET_ABCDE(n,ident,from,to,by,body);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	EDEBUG("   ITER FOR LOOP");

	if G_UNLIKELY ((by && (by->type != VALUE_NODE ||
			       mpw_is_complex(by->val.value))) ||
		       from->type != VALUE_NODE || mpw_is_complex(from->val.value) ||
		       to->type != VALUE_NODE || mpw_is_complex(to->val.value)) {
		gel_errorout (_("Bad type for 'for/sum/prod' loop!"));
		iter_pop_stack(ctx);
		return;
	}
	if G_UNLIKELY (by && mpw_zero_p (by->val.value)) {
		gel_errorout (_("'for/sum/prod' loop increment can't be 0"));
		iter_pop_stack(ctx);
		return;
	}
	
	init_cmp = mpw_cmp(from->val.value,to->val.value);
	
	/*if no iterations*/
	if(!by) {
		if(init_cmp>0) {
			d_addfunc(d_makevfunc(ident->id.id,gel_copynode(from)));
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
			      gel_copynode(body),body,ident->id.id);
	} else {
		int sgn = mpw_sgn(by->val.value);
		if((sgn>0 && init_cmp>0) || (sgn<0 && init_cmp<0)) {
			d_addfunc(d_makevfunc(ident->id.id,gel_copynode(from)));
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
			      init_cmp,gel_copynode(body),body,ident->id.id);
	}

	d_addfunc(d_makevfunc(ident->id.id,gel_makenum(evf->x)));
	
	GE_PUSH_STACK (ctx, n,
		       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
	*repushed = TRUE;
	GE_PUSH_STACK (ctx, evf, GE_FOR);
	
	ctx->current = evf->body;
	ctx->post = FALSE;
	ctx->whackarg = FALSE;
}

static inline void
iter_forinloop(GelCtx *ctx, GelETree *n, gboolean *repushed)
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

	GEL_GET_LRR(n,ident,from,body);
	
	EDEBUG("   ITER FORIN LOOP");

	/* If there is nothing to sum */
	if (from->type == NULL_NODE) {
		/* replace n with the appropriate nothingness */
		freetree_full (n, TRUE, FALSE);
		switch (type) {
		case GEL_EVAL_FOR:
			n->type = NULL_NODE;
			break;
		case GEL_EVAL_SUM:
			gel_makenum_ui_from (n, 0);
			break;
		case GEL_EVAL_PROD:
			gel_makenum_ui_from (n, 1);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		iter_pop_stack (ctx);
		return;
	}

	/* FIXME: string should go through all the characters I suppose */
	if G_UNLIKELY (from->type != VALUE_NODE &&
		       from->type != BOOL_NODE &&
		       from->type != MATRIX_NODE) {
		gel_errorout (_("Bad type for 'for in' loop!"));
		iter_pop_stack(ctx);
		return;
	}
	
	if(from->type == MATRIX_NODE) {
		evfi = evfi_new (type, from->mat.matrix,
				 gel_copynode (body), body, ident->id.id);
		d_addfunc(d_makevfunc(ident->id.id,
				      gel_copynode(gel_matrixw_index(from->mat.matrix,
							     evfi->i,
							     evfi->j))));
	} else {
		evfi = evfi_new (type, NULL, gel_copynode(body), body, ident->id.id);
		d_addfunc(d_makevfunc(ident->id.id,gel_copynode(from)));
	}
	
	GE_PUSH_STACK (ctx, n,
		       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
	*repushed = TRUE;
	GE_PUSH_STACK(ctx,evfi,GE_FORIN);
	
	ctx->current = evfi->body;
	ctx->post = FALSE;
	ctx->whackarg = FALSE;
}

static inline void
iter_loop (GelCtx *ctx, GelETree *n, gboolean body_first, gboolean is_while)
{
	GelEvalLoop *evl;
	GelETree *l, *r;
	
	GEL_GET_LR(n,l,r);
	
	EDEBUG("   ITER LOOP");
	
	GE_PUSH_STACK (ctx, ctx->current,
		       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
	if (body_first) {
		EDEBUG ("    BODY FIRST");
		evl = evl_new (NULL, gel_copynode (l), is_while, body_first);
		GE_PUSH_STACK (ctx, evl, GE_LOOP_LOOP);
		ctx->current = evl->body;
		ctx->post = FALSE;
		ctx->whackarg = FALSE;
	} else {
		EDEBUG("    CHECK FIRST");
		evl = evl_new (gel_copynode(l), NULL, is_while, body_first);
		GE_PUSH_STACK (ctx, evl, GE_LOOP_COND);
		ctx->current = evl->condition;
		ctx->post = FALSE;
		ctx->whackarg = FALSE;
	}
}

static inline void
iter_ifop(GelCtx *ctx, GelETree *n, gboolean has_else, gboolean *repushed)
{
	GelETree *l,*r,*rr = NULL;
	gboolean ret;
	gboolean bad_node = FALSE;
	
	EDEBUG("    IF/IFELSE ITER OP");
	
	if(has_else) {
		GEL_GET_LRR(n,l,r,rr);
	} else {
		GEL_GET_LR(n,l,r);
	}
	
	ret = gel_isnodetrue(l,&bad_node);
	if G_UNLIKELY (bad_node || gel_error_num) {
		EDEBUG("    IF/IFELSE BAD BAD NODE");
		gel_error_num = GEL_NO_ERROR;
		iter_pop_stack(ctx);
		return;
	}
	
	if(ret) {
#ifdef EVAL_DEBUG
		printf ("    IF TRUE EVAL BODY n %p l %p r %p\n", n, l, r);
#endif
		/*remove from arglist so that it doesn't get freed on
		  replace node*/
		n->op.args->any.next = n->op.args->any.next->any.next;
		replacenode (n, r);
		ctx->post = FALSE;
		g_assert (ctx->current == n);
		/* whackarg stays the same */
		*repushed = TRUE;
	} else if(has_else) {
		EDEBUG("    IF FALSE EVAL ELSE BODY");
		/*remove from arglist so that it doesn't get freed on
		  replace node*/
		n->op.args->any.next->any.next = NULL;
		replacenode (n, rr);
		ctx->post = FALSE;
		g_assert (ctx->current == n);
		/* whackarg stays the same */
		*repushed = TRUE;
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
							\
		/* makes debugging happy */		\
		ctx->current = NULL;			\
							\
		gel_freetree(e->body);			\
		freefunc(e);				\
							\
		/*pop loop call tree*/			\
		GE_POP_STACK(ctx,n,flag);		\
							\
		if (flag & GE_WHACKARG) {		\
			gel_freetree (n);		\
		} else {				\
			/*null the tree*/		\
			freetree_full(n,TRUE,FALSE);	\
			n->type = NULL_NODE;		\
		}					\
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
		switch(flag & GE_MASK) {
		case GE_EMPTY_STACK:
			EDEBUG("    EMPTY");
			goto iter_continue_break_done;
		case GE_FUNCCALL:
			EDEBUG("    FOUND FUNCCCALL MAKE IT NULL THEN");
			gel_errorout (_("Continue or break outside a loop, "
					"assuming \"return null\""));
			gel_freetree(data);

			d_popcontext ();

			/*pop the function call*/
			GE_POP_STACK(ctx,data,flag);

			g_assert ((flag & GE_MASK) == GE_POST);
			if (flag & GE_WHACKARG) {
				/* WHACKWHACK */
				gel_freetree (data);
			} else {
				freetree_full(data,TRUE,FALSE);
				((GelETree *)data)->type = NULL_NODE;
			}

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
	gel_errorout (_("Continue or break outside a loop, "
			"assuming \"return null\""));
	/*we were at the top so substitute result for a NULL*/
	ctx->current = NULL;
	ctx->post = FALSE;
	ctx->whackarg = FALSE;
	freetree_full(ctx->res,TRUE,FALSE);
	ctx->res->type = NULL_NODE;
}

#undef LOOP_BREAK_CONT

static inline void
iter_bailout_op(GelCtx *ctx)
{
	EDEBUG("  BAILOUT");

#ifdef MEM_DEBUG_FRIENDLY
	/* Current will be changed and possibly whacked */
	ctx->current = NULL;
#endif
	for(;;) {
		int flag;
		gpointer data;
		GE_POP_STACK(ctx,data,flag);
		EDEBUG("   POPPED STACK");
		if ((flag & GE_MASK) == GE_EMPTY_STACK) {
			EDEBUG("    EMPTY");
			break;
		} else if ((flag & GE_MASK) == GE_FUNCCALL) {
			EDEBUG("    FOUND FUNCCCALL");
			gel_freetree(data);

			d_popcontext ();

			/*pop the function call off the stack*/
			GE_POP_STACK(ctx,data,flag);
			if (flag & GE_WHACKARG) {
				/* WHACKWHACK */
				gel_freetree (data);
			}

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
	ctx->whackarg = FALSE;
}

static int
iter_get_ui_index (GelETree *num)
{
	long i;
	if G_UNLIKELY (num->type != VALUE_NODE ||
		       !mpw_is_integer(num->val.value)) {
		gel_errorout (_("Wrong argument type as matrix index"));
		return -1;
	}

	i = mpw_get_long(num->val.value);
	if G_UNLIKELY (gel_error_num) {
		gel_error_num = GEL_NO_ERROR;
		return -1;
	}
	if G_UNLIKELY (i > INT_MAX) {
		gel_errorout (_("Matrix index too large"));
		return -1;
	} else if G_UNLIKELY (i <= 0) {
		gel_errorout (_("Matrix index less than 1"));
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
		} else if G_UNLIKELY (reg[i] >= maxsize) {
			g_free (reg);
			gel_errorout (_("Matrix index out of range"));
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
	} else if G_UNLIKELY (i >= maxsize) {
		gel_errorout (_("Matrix index out of range"));
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
		if G_UNLIKELY (d_curcontext()==0 &&
			       m->id.id->protected_) {
			gel_errorout (_("Trying to set a protected id '%s'"),
				      m->id.id->token);
			return NULL;
		}
		f = d_lookup_local(m->id.id);
		if(!f) {
			GelETree *t;
			GEL_GET_NEW_NODE(t);
			t->type = MATRIX_NODE;
			t->mat.matrix = gel_matrixw_new();
			t->mat.quoted = FALSE;
			gel_matrixw_set_size(t->mat.matrix,1,1);

			f = d_makevfunc(m->id.id,t);
			d_addfunc(f);
			if(new_matrix) *new_matrix = TRUE;
		} else if G_UNLIKELY (f->type != GEL_USER_FUNC &&
				      f->type != GEL_VARIABLE_FUNC) {
			gel_errorout (_("Indexed Lvalue not user function"));
			return NULL;
		}
		D_ENSURE_USER_BODY (f);
		if(f->data.user->type != MATRIX_NODE) {
			GelETree *t;
			GEL_GET_NEW_NODE(t);
			t->type = MATRIX_NODE;
			t->mat.matrix = gel_matrixw_new();
			t->mat.quoted = FALSE;
			gel_matrixw_set_size(t->mat.matrix,1,1);

			d_set_value(f,t);
			if(new_matrix) *new_matrix = TRUE;
		}
		mat = f->data.user->mat.matrix;
	} else if(m->type == OPERATOR_NODE ||
		  m->op.oper == E_DEREFERENCE) {
		GelETree *l;
		GelEFunc *f;
		GEL_GET_L(m,l);

		if G_UNLIKELY (l->type != IDENTIFIER_NODE) {
			gel_errorout (_("Dereference of non-identifier!"));
			return NULL;
		}

		f = d_lookup_local(l->id.id);
		if G_UNLIKELY (f == NULL) {
			gel_errorout (_("Dereference of undefined variable!"));
			return NULL;
		}
		if G_UNLIKELY (f->type != GEL_REFERENCE_FUNC) {
			gel_errorout (_("Dereference of non-reference!"));
			return NULL;
		}

		if G_UNLIKELY (f->data.ref->type != GEL_USER_FUNC &&
			       f->data.ref->type != GEL_VARIABLE_FUNC) {
			gel_errorout (_("Indexed Lvalue not user function"));
			return NULL;
		}
		if G_UNLIKELY (f->data.ref->context == 0 &&
			       f->data.ref->id->protected_) {
			gel_errorout (_("Trying to set a protected id '%s'"),
				      f->data.ref->id->token);
			return NULL;
		}
		D_ENSURE_USER_BODY (f->data.ref);
		if(f->data.ref->data.user->type != MATRIX_NODE) {
			GelETree *t;
			GEL_GET_NEW_NODE(t);
			t->type = MATRIX_NODE;
			t->mat.matrix = gel_matrixw_new();
			t->mat.quoted = FALSE;
			gel_matrixw_set_size(t->mat.matrix,1,1);

			d_set_value(f->data.ref,t);
			if(new_matrix) *new_matrix = TRUE;
		}
		mat = f->data.ref->data.user->mat.matrix;
	} else {
		gel_errorout (_("Indexed Lvalue not an identifier or a dereference"));
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
		func = d_makevfunc (token, gel_copynode (val));
		/* make function global */
		func->context = 0;
		d_addfunc_global (func);
		return gel_copynode (val);
	}
}

static void
iter_equalsop(GelETree *n)
{
	GelETree *l,*r;

	GEL_GET_LR(n,l,r);
	
	if G_UNLIKELY (l->type != IDENTIFIER_NODE &&
		       !(l->type == OPERATOR_NODE && l->op.oper == E_GET_VELEMENT) &&
		       !(l->type == OPERATOR_NODE && l->op.oper == E_GET_ELEMENT) &&
		       !(l->type == OPERATOR_NODE && l->op.oper == E_GET_COL_REGION) &&
		       !(l->type == OPERATOR_NODE && l->op.oper == E_GET_ROW_REGION) &&
		       !(l->type == OPERATOR_NODE && l->op.oper == E_DEREFERENCE)) {
		gel_errorout (_("Lvalue not an identifier/dereference/matrix location!"));
		return;
	}

	if(l->type == IDENTIFIER_NODE) {
		if G_UNLIKELY (d_curcontext() == 0 &&
			       l->id.id->protected_) {
			gel_errorout (_("Trying to set a protected id '%s'"),
				      l->id.id->token);
			return;
		}
		if (l->id.id->parameter) {
			GelETree *ret = set_parameter (l->id.id, r);
			if (ret != NULL)
				replacenode (n, ret);
			return;
		} else if(r->type == FUNCTION_NODE) {
			d_addfunc (d_makerealfunc (r->func.func,
						   l->id.id,
						   FALSE));
		} else if(r->type == OPERATOR_NODE &&
			  r->op.oper == E_REFERENCE) {
			GelETree *t = r->op.args;
			GelEFunc *rf = d_lookup_global(t->id.id);
			if G_UNLIKELY (rf == NULL) {
				gel_errorout (_("Referencing an undefined variable!"));
				return;
			}
			d_addfunc(d_makereffunc(l->id.id,rf));
		} else {
			d_addfunc(d_makevfunc(l->id.id,gel_copynode(r)));
		}
	} else if(l->op.oper == E_DEREFERENCE) {
		GelEFunc *f;
		GelETree *ll;
		GEL_GET_L(l,ll);

		if G_UNLIKELY (ll->type != IDENTIFIER_NODE) {
			gel_errorout (_("Dereference of non-identifier!"));
			return;
		}
		
		f = d_lookup_local(ll->id.id);
		if G_UNLIKELY (f == NULL) {
			gel_errorout (_("Dereference of undefined variable!"));
			return;
		}
		if G_UNLIKELY (f->type!=GEL_REFERENCE_FUNC) {
			gel_errorout (_("Dereference of non-reference!"));
			return;
		}

		if G_UNLIKELY (f->data.ref->context == 0 &&
			       f->data.ref->id->protected_) {
			gel_errorout (_("Trying to set a protected id '%s'"),
				      f->data.ref->id->token);
			return;
		}
		
		if(r->type == FUNCTION_NODE) {
			d_setrealfunc(f->data.ref,r->func.func,FALSE);
		} else if(r->type == OPERATOR_NODE &&
			  r->op.oper == E_REFERENCE) {
			GelETree *t = r->op.args;
			GelEFunc *rf = d_lookup_global(t->id.id);
			if G_UNLIKELY (rf == NULL) {
				gel_errorout (_("Referencing an undefined variable!"));
				return;
			}
			d_set_ref(f->data.ref,rf);
		} else {
			d_set_value(f->data.ref,gel_copynode(r));
		}
	} else if(l->op.oper == E_GET_ELEMENT) {
		GelMatrixW *mat;
		GelETree *m, *index1, *index2;
		GEL_GET_LRR (l, m, index1, index2);

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

			gel_matrixw_set_element (mat, x, y, gel_copynode (r));
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

			if G_UNLIKELY (r->type == MATRIX_NODE &&
				       (gel_matrixw_width (r->mat.matrix) != lx ||
					gel_matrixw_height (r->mat.matrix) != ly)) {
				g_free (regx);
				g_free (regy);
				gel_errorout (_("Wrong matrix dimensions when setting"));
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
			gel_errorout (_("Matrix index not an integer or a vector"));
			return;
		}
	} else if(l->op.oper == E_GET_VELEMENT) {
		GelMatrixW *mat;
		GelETree *m, *index;
		GEL_GET_LR (l, m, index);

		if (index->type == VALUE_NODE) {
			int i;

			i = iter_get_matrix_index_num (index, INT_MAX);
			if (i < 0)
				return;

			mat = iter_get_matrix_p (l->op.args, NULL);
			if (mat == NULL)
				return;

			gel_matrixw_set_velement (mat, i, gel_copynode (r));
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
			gel_errorout (_("Matrix index not an integer or a vector"));
			return;
		}
	} else /*l->data.oper == E_GET_COL_REGION E_GET_ROW_REGION*/ {
		GelMatrixW *mat;
		GelETree *m, *index;
		GEL_GET_LR (l, m, index);

		if (index->type == VALUE_NODE ||
		    index->type == MATRIX_NODE) {
			int *regx, *regy;
			int lx, ly;
			int i;

			if (l->op.oper == E_GET_COL_REGION) {
				if ( ! iter_get_index_region (index, INT_MAX, &regx, &lx))
					return;
				if G_UNLIKELY (r->type == MATRIX_NODE &&
					       gel_matrixw_width (r->mat.matrix) != lx) {
					g_free (regx);
					gel_errorout (_("Wrong matrix dimensions when setting"));
					return;
				}
			} else {
				if ( ! iter_get_index_region (index, INT_MAX, &regy, &ly))
					return;
				if G_UNLIKELY (r->type == MATRIX_NODE &&
					       gel_matrixw_height (r->mat.matrix) != ly) {
					g_free (regy);
					gel_errorout (_("Wrong matrix dimensions when setting"));
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
			gel_errorout (_("Matrix index not an integer or a vector"));
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

	GEL_GET_LRR (n, l, r, rr);

	/* FIXME: l should be the set func */
	
	g_assert (r->type == IDENTIFIER_NODE);

	if G_UNLIKELY (d_curcontext() != 0) {
		gel_errorout (_("Parameters can only be created in the global context"));
		return;
	}
	
	d_addfunc (d_makevfunc (r->id.id, gel_copynode (rr)));
	r->id.id->parameter = 1;

	/*remove from arglist so that it doesn't get freed on replacenode*/
	n->op.args->any.next->any.next = NULL;
	replacenode (n, rr);
}

static inline void
iter_push_indexes_and_arg(GelCtx *ctx, GelETree *n)
{
	GelETree *l,*ident;

	GEL_GET_L(n,l);
	
	if (l->op.oper == E_GET_ELEMENT) {
		GelETree *ll,*rr;
		
		GEL_GET_LRR(l,ident,ll,rr);

		GE_PUSH_STACK(ctx,n->op.args->any.next,GE_PRE);
		GE_PUSH_STACK(ctx,rr,GE_PRE);
		ctx->post = FALSE;
		ctx->current = ll;
		ctx->whackarg = FALSE;
	} else if(l->op.oper == E_GET_VELEMENT ||
		  l->op.oper == E_GET_COL_REGION ||
		  l->op.oper == E_GET_ROW_REGION) {
		GelETree *ll;
		
		GEL_GET_LR(l,ident,ll);

		GE_PUSH_STACK(ctx,n->op.args->any.next,GE_PRE);
		ctx->post = FALSE;
		ctx->current = ll;
		ctx->whackarg = FALSE;
	} else {
		ctx->post = FALSE;
		ctx->current = n->op.args->any.next;
		ctx->whackarg = FALSE;
	}
}

static void
iter_get_velement (GelETree *n)
{
	GelETree *m;
	GelETree *index;

	GEL_GET_LR (n, m, index);

	if G_UNLIKELY (m->type != MATRIX_NODE) {
		gel_errorout (_("Index works only on matricies"));
		return;
	}

	if (index->type == VALUE_NODE) {
		GelETree *t;
		int i = iter_get_matrix_index_num (index, gel_matrixw_elements (m->mat.matrix));
		if (i < 0)
			return;
		t = gel_copynode (gel_matrixw_vindex (m->mat.matrix, i));
		replacenode (n, t);
	} else if (index->type == MATRIX_NODE) {
		GelMatrixW *vec;
		int matsize = gel_matrixw_elements (m->mat.matrix);
		gboolean quoted = m->mat.quoted;
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
	} else if (index->type == NULL_NODE) {
		freetree_full (n, TRUE, FALSE);
		gel_makenum_null_from (n);
	} else {
		gel_errorout (_("Vector index not an integer or a vector"));
	}
}

static void
iter_get_element (GelETree *n)
{
	GelETree *m, *index1, *index2;

	GEL_GET_LRR (n, m, index1, index2);

	if G_UNLIKELY (m->type != MATRIX_NODE) {
		gel_errorout (_("Index works only on matricies"));
		return;
	} else if G_UNLIKELY (index1->type != NULL_NODE &&
		       index1->type != MATRIX_NODE &&
		       index1->type != VALUE_NODE &&
		       index2->type != NULL_NODE &&
		       index2->type != MATRIX_NODE &&
		       index2->type != VALUE_NODE) {
		gel_errorout (_("Matrix index not an integer or a vector"));
		return;
	} else if G_UNLIKELY (index1->type == NULL_NODE ||
			      index2->type == NULL_NODE) {
		/* This is rather unlikely, most of the time we don't
		 * want NULLs */
		freetree_full (n, TRUE, FALSE);
		gel_makenum_null_from (n);
		return;


	/* this is where we get to the real code */
	} else if (index1->type == VALUE_NODE &&
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
		t = gel_matrixw_get_index (m->mat.matrix, x, y);
		if (m->mat.matrix->m->use == 1 && t != NULL) {
			replacenode (n, t);
			gel_matrixw_set_index (m->mat.matrix, x, y) = NULL;
		} else if (t == NULL) {
			freetree_full (n, FALSE /* freeargs */, FALSE /* kill */);
			gel_makenum_ui_from (n, 0);
		} else {
			replacenode (n, gel_copynode (t));
		}

		/* free the args now */
		gel_freetree (m);
		gel_freetree (index1);
		gel_freetree (index2);
	/* Now at least one is a matrix and the other is a value */
	/*} else if ((index1->type == VALUE_NODE ||
		    index1->type == MATRIX_NODE) &&
		   (index2->type == VALUE_NODE ||
		    index2->type == MATRIX_NODE)) {*/
	} else {
		GelMatrixW *mat;
		int *regx, *regy;
		int lx, ly;
		int maxx, maxy;
		gboolean quoted = m->mat.quoted;

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
	}
}

static void
iter_get_region (GelETree *n, gboolean col)
{
	GelETree *m, *index;

	GEL_GET_LR (n, m, index);

	if G_UNLIKELY (m->type != MATRIX_NODE) {
		gel_errorout (_("Index works only on matricies"));
		return;
	} else if G_LIKELY (index->type == VALUE_NODE ||
			    index->type == MATRIX_NODE) {
		GelMatrixW *mat;
		int *regx, *regy;
		int lx, ly;
		int i;
		int maxx, maxy;
		gboolean quoted = m->mat.quoted;

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
	} else if (index->type == NULL_NODE) {
		freetree_full (n, TRUE, FALSE);
		gel_makenum_null_from (n);
	} else {
		gel_errorout (_("Matrix index not an integer or a vector"));
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
	case IDENTIFIER_NODE: return GO_IDENTIFIER;
	case POLYNOMIAL_NODE: return GO_POLYNOMIAL;
	case BOOL_NODE: return GO_BOOL;
	default: return 0;
	}
}

static char *
iter_get_arg_name(guint32 arg)
{
	switch(arg) {
	case GO_VALUE: return _("number");
	case GO_MATRIX: return _("matrix");
	case GO_STRING: return _("string");
	case GO_FUNCTION: return _("function");
	case GO_IDENTIFIER: return _("identifier");
	case GO_POLYNOMIAL: return _("polynomial");
	case GO_BOOL: return _("boolean");
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
	case E_DEFEQUALS:
	case E_PARAMETER: break;
	case E_ABS: name = g_strdup(_("Absolute value")); break;
	case E_PLUS: name = g_strdup(_("Addition")); break;
	case E_ELTPLUS: name = g_strdup(_("Element by element addition")); break;
	case E_MINUS: name = g_strdup(_("Subtraction")); break;
	case E_ELTMINUS: name = g_strdup(_("Element by element subtraction")); break;
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
	int i;

	GEL_GET_LR(n,l,r);
	
	arg1 = iter_get_arg(l);
	arg2 = iter_get_arg(r);
	
	if G_UNLIKELY (arg1 == 0 || arg2 == 0) {
		gel_errorout (_("Bad types for '%s'"),
			      iter_get_op_name(n->op.oper));
		return TRUE;
	}

	for(i=0;i<OP_TABLE_LEN;i++) {
		if(op->prim[i].arg[0]&arg1 &&
		   op->prim[i].arg[1]&arg2) {
			return op->prim[i].evalfunc(ctx,n,l,r);
		}
	}
	gel_errorout (_("%s not defined on <%s> and <%s>"),
		      iter_get_op_name(n->op.oper),
		      iter_get_arg_name(arg1),
		      iter_get_arg_name(arg2));
	return TRUE;
}

static inline gboolean
iter_call1(GelCtx *ctx, const GelOper *op, GelETree *n)
{
	GelETree *l;
	guint32 arg1;
	int i;

	GEL_GET_L(n,l);
	
	arg1 = iter_get_arg(l);
	
	if G_UNLIKELY (arg1 == 0) {
		gel_errorout (_("Bad type for '%s'"),
			      iter_get_op_name(n->op.oper));
		return TRUE;
	}

	for(i=0;i<OP_TABLE_LEN;i++) {
		if(op->prim[i].arg[0]&arg1) {
			return op->prim[i].evalfunc(ctx,n,l);
		}
	}
	gel_errorout (_("%s not defined on <%s>"),
		      iter_get_op_name(n->op.oper),
		      iter_get_arg_name(arg1));
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
		GEL_GET_LRR (n, from, by, to);
		if G_UNLIKELY (from->type != VALUE_NODE ||
			       to->type != VALUE_NODE ||
			       by->type != VALUE_NODE) {
			gel_errorout (_("Vector building only works on numbers"));
			return;
		}
		initcmp = cmp = mpw_cmp (from->val.value, to->val.value);
		bysgn = mpw_sgn (by->val.value);

		if G_UNLIKELY ((cmp > 0 && bysgn > 0) ||
			       (cmp != 0 && bysgn == 0) ||
			       (cmp < 0 && bysgn < 0)) {
			/* FIXME: perhaps we should just return null like octave? */
			gel_errorout (_("Impossible arguments to vector building operator"));
			return;
		}	
	} else {
		GEL_GET_LR (n, from, to);
		if G_UNLIKELY (from->type != VALUE_NODE ||
			       to->type != VALUE_NODE) {
			gel_errorout (_("Vector building only works on numbers"));
			return;
		}
		initcmp = cmp = mpw_cmp (from->val.value, to->val.value);
		if (cmp > 0)
			bysgn = -1;
	}

	count = 0;
	mpw_init_set (tmp, from->val.value);
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
	n->mat.quoted = TRUE;
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
	case E_DEFEQUALS:
		EDEBUG("  EQUALS PRE");
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST,
					       ctx->whackarg));
		iter_push_indexes_and_arg(ctx,n);
		break;

	case E_PARAMETER:
		EDEBUG("  PARAMETER PRE");
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST,
					       ctx->whackarg));
		/* Push third parameter (the value) */
		ctx->post = FALSE;
		ctx->current = n->op.args->any.next->any.next;
		ctx->whackarg = FALSE;
		break;

	case E_EXP:
	case E_ELTEXP:
		EDEBUG("  PUSH US AS POST AND ALL ARGUMENTS AS PRE (no modulo on second)");
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST,
					       ctx->whackarg));
		iter_push_two_args_no_modulo_on_2 (ctx, n->op.args);
		break;

	case E_SEPAR:
		EDEBUG("  PUSH US AS POST AND ALL ARGUMENTS AS PRE WITH "
		       " WHACKARGS");
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST,
					       ctx->whackarg));
		n->op.args = iter_push_args_whack (ctx, n->op.args, n->op.nargs);
		break;

	case E_ABS:
	case E_PLUS:
	case E_ELTPLUS:
	case E_MINUS:
	case E_ELTMINUS:
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
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST,
					       ctx->whackarg));
		iter_push_args (ctx, n->op.args, n->op.nargs);
		break;

	case E_CALL:
		EDEBUG("  CHANGE CALL TO DIRECTCALL AND EVAL THE FIRST ARGUMENT");
		n->op.oper = E_DIRECTCALL;
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_PRE,
					       ctx->whackarg));
		/* eval first argument */
		ctx->current = n->op.args;
		ctx->post = FALSE;
		ctx->whackarg = FALSE;
		break;

	/*in case of DIRECTCALL we don't evaluate the first argument*/
	case E_DIRECTCALL:
		/*if there are arguments to evaluate*/
		if(n->op.args->any.next) {
			GelEFunc *f;
			EDEBUG("  DIRECT:PUSH US AS POST AND 2nd AND HIGHER ARGS AS PRE");
			GE_PUSH_STACK (ctx, n,
				       GE_ADDWHACKARG (GE_POST,
						       ctx->whackarg));
			f = get_func_from_arg (n, TRUE /* silent */);
			if (f != NULL && f->no_mod_all_args)
				iter_push_args_no_modulo (ctx,
							  n->op.args->any.next,
							  n->op.nargs - 1);
			else
				iter_push_args (ctx,
						n->op.args->any.next,
						n->op.nargs - 1);
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
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		GE_PUSH_STACK(ctx,n->op.args,GE_AND);
		ctx->post = FALSE;
		ctx->current = n->op.args;
		ctx->whackarg = FALSE;
		break;
	case E_LOGICAL_OR:
		EDEBUG("  LOGICAL OR");
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		GE_PUSH_STACK(ctx,n->op.args,GE_OR);
		ctx->post = FALSE;
		ctx->current = n->op.args;
		ctx->whackarg = FALSE;
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
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		ctx->post = FALSE;
		ctx->current = n->op.args;
		ctx->whackarg = FALSE;
		break;

	case E_DEREFERENCE:
		if(!iter_derefvarop(ctx,n))
			return FALSE;
		if (ctx->whackarg) {
			ctx->current = NULL;
			gel_freetree (n);
		} else {
			if ((n->type == VALUE_NODE ||
			     n->type == MATRIX_NODE) &&
			    ctx->modulo != NULL)
				mod_node (n, ctx->modulo);
		}
		iter_pop_stack(ctx);
		break;

	case E_FOR_CONS:
	case E_FORBY_CONS:
	case E_SUM_CONS:
	case E_SUMBY_CONS:
	case E_PROD_CONS:
	case E_PRODBY_CONS:
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		iter_push_args (ctx, n->op.args->any.next, n->op.nargs - 2);
		break;

	case E_FORIN_CONS:
	case E_SUMIN_CONS:
	case E_PRODIN_CONS:
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		ctx->current = n->op.args->any.next;
		ctx->post = FALSE;
		ctx->whackarg = FALSE;
		break;

	case E_EXCEPTION:
		if (ctx->whackarg) {
			ctx->current = NULL;
			gel_freetree (n);
		}
		return FALSE;

	case E_BAILOUT:
		if (ctx->whackarg) {
			ctx->current = NULL;
			gel_freetree (n);
		}
		iter_bailout_op(ctx);
		break;

	case E_CONTINUE:
		if (ctx->whackarg) {
			ctx->current = NULL;
			gel_freetree (n);
		}
		iter_continue_break_op(ctx,TRUE);
		break;

	case E_BREAK:
		if (ctx->whackarg) {
			ctx->current = NULL;
			gel_freetree (n);
		}
		iter_continue_break_op(ctx,FALSE);
		break;

	case E_QUOTE:
		if (ctx->whackarg) {
			ctx->current = NULL;
			gel_freetree (n);
		} else {
			/* Just replace us with the quoted thing */
			GelETree *arg = n->op.args;
			n->op.args = NULL;
			replacenode (n, arg);
		}
		iter_pop_stack(ctx);
		break;

	case E_REFERENCE:
		{
			GelETree *t;
			GelEFunc *rf;

			if (ctx->whackarg) {
				ctx->current = NULL;
				gel_freetree (n);
			}

			/* If doesn't exist, make it and set it to null */
			t = n->op.args;
			rf = d_lookup_global (t->id.id);
			if (rf == NULL) {
				d_addfunc (d_makevfunc (t->id.id, 
							gel_makenum_null ()));
			}

			iter_pop_stack(ctx);
			break;
		}

	case E_MOD_CALC:
		/* Push modulo op, so that we may push the
		 * first argument once we have gotten a modulo */
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_MODULOOP, ctx->whackarg));
		ctx->post = FALSE;
		ctx->current = n->op.args->any.next;
		ctx->whackarg = FALSE;
		break;

	default:
		gel_errorout (_("Unexpected operator!"));
#ifdef EVAL_DEBUG
		printf ("!!!!!!!!!!!!!!!UNEXPECTED_OPERATOR PRE (%p) (%d)\n", n, n->op.oper);
#endif
		GE_PUSH_STACK (ctx, n,
			       GE_ADDWHACKARG (GE_POST, ctx->whackarg));
		break;
	}
	return TRUE;
}

static gboolean
iter_operator_post (GelCtx *ctx, gboolean *repushed)
{
	GelETree *n = ctx->current;
	GelETree *r;
	EDEBUG(" OPERATOR POST");
	switch(n->op.oper) {
	case E_SEPAR:
		/* By now there is only one argument and that
		   is the last one */
		r = n->op.args;
		n->op.args = NULL;
		replacenode (n, r);
		iter_pop_stack (ctx);
		break;

	case E_EQUALS:
	case E_DEFEQUALS:
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
	case E_ELTPLUS:
	case E_MINUS:
	case E_ELTMINUS:
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
		     n->type == MATRIX_NODE) &&
		    ! ctx->whackarg)
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
		     n->type == MATRIX_NODE) &&
		    ! ctx->whackarg)
			mod_node (n, ctx->modulo);
		iter_pop_stack(ctx);
		break;

	case E_MOD_CALC:
		/* FIXME: maybe we should always replace things here,
		 * not just for values and matrices */
		if (n->op.args->type == BOOL_NODE ||
		    n->op.args->type == VALUE_NODE ||
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
		iter_forloop (ctx, n, repushed);
		break;

	case E_FORIN_CONS:
	case E_SUMIN_CONS:
	case E_PRODIN_CONS:
		iter_forinloop (ctx, n, repushed);
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
		iter_ifop (ctx, n, FALSE, repushed);
		break;
	case E_IFELSE_CONS:
		iter_ifop (ctx, n, TRUE, repushed);
		break;

	case E_DIRECTCALL:
	case E_CALL:
		if ( ! iter_funccallop(ctx, n, repushed))
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
		gel_errorout (_("Unexpected operator!"));
#ifdef EVAL_DEBUG
		printf ("!!!!!!!!!!!!!!!UNEXPECTED_OPERATOR POST (%p) (%d)\n", n, n->op.oper);
#endif
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
		 * than needed */
		if ((n->op.oper == E_EQUALS || n->op.oper == E_DEFEQUALS) &&
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
		   n->mat.matrix != NULL &&
		   ! gel_is_matrix_value_only (n->mat.matrix)) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix, TRUE /* kill_type_caches */);
		for (i = 0; i < w; i++) {
			for(j = 0; j < h; j++) {
				GelETree *t = gel_matrixw_get_index
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
	gboolean whack_saved;

#define WHACK_SAVEDN_POP \
	{						\
		ctx->current = NULL;			\
		if (whack_saved) {			\
			/* WHACKWHACK */		\
			gel_freetree (n);		\
		}					\
		iter_pop_stack (ctx);			\
	}

	while((n = ctx->current)) {
		EDEBUG("ITER");
		if (gel_evalnode_hook != NULL) {
			static int i = 0;
			if G_UNLIKELY ((i++ & GEL_RUN_HOOK_EVERY_MASK) == GEL_RUN_HOOK_EVERY_MASK) {
				(*gel_evalnode_hook)();
				i = 0;
			}
		}
		whack_saved = ctx->whackarg;

		if G_UNLIKELY (gel_interrupted) {
			if (whack_saved) {
				ctx->current = NULL;
				gel_freetree (n);
			}
			return FALSE;
		}

		switch(n->type) {
		case NULL_NODE:
			EDEBUG(" NULL NODE");
			WHACK_SAVEDN_POP;
			break;

		case VALUE_NODE:
			EDEBUG(" VALUE NODE");

			if (ctx->modulo != NULL)
				mod_node (n, ctx->modulo);

			WHACK_SAVEDN_POP;
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
				WHACK_SAVEDN_POP;
			}
			break;
		case OPERATOR_NODE:
			EDEBUG(" OPERATOR NODE");
			if(!ctx->post) {
				if G_UNLIKELY (!iter_operator_pre(ctx)) {
					/* WHACKWHACK */
					/* FIXME: is this needed?
					 * check if it's possible */
					if (n == ctx->current &&
					    whack_saved) {
						ctx->current = NULL;
						gel_freetree (n);
					}
					return FALSE;
				}
				/* pre either pushes n again or whacks it
				   itself, in either case we can assume we
				   are rid of it if we were to whack it */
			} else {
				gboolean repushed = FALSE;
				if G_UNLIKELY ( ! iter_operator_post
						    (ctx, &repushed)) {
					/* WHACKWHACK */
					if (whack_saved && ! repushed) {
						/* FIXME: is this needed? */
						if (ctx->current == n)
							ctx->current = NULL;
						gel_freetree (n);
					}
					return FALSE;
				}
				if (whack_saved && ! repushed) {
					gel_freetree (n);
				}
			}
			break;
		case IDENTIFIER_NODE:
			EDEBUG(" IDENTIFIER NODE");
			if G_UNLIKELY (!iter_variableop(ctx, n)) {
				/* WHACKWHACK */
				if (whack_saved)
					gel_freetree (n);
				return FALSE;
			}
			if ((n->type == VALUE_NODE ||
			     n->type == MATRIX_NODE) &&
			    ctx->modulo != NULL &&
			    ! whack_saved)
				mod_node (n, ctx->modulo);
			WHACK_SAVEDN_POP;
			break;
		case STRING_NODE:
			EDEBUG(" STRING NODE");
			WHACK_SAVEDN_POP;
			break;

		case FUNCTION_NODE:
			EDEBUG(" FUNCTION NODE");
			if (n->func.func != NULL &&
			    (n->func.func->type == GEL_USER_FUNC ||
			     n->func.func->type == GEL_VARIABLE_FUNC) &&
			    d_curcontext () != 0) {
				d_put_on_subst_list (n->func.func);
			}
			WHACK_SAVEDN_POP;
			break;

		case COMPARISON_NODE:
			EDEBUG(" COMPARISON NODE");
			if(!ctx->post) {
				/*if in pre mode, push arguments onto stack*/
				GE_PUSH_STACK (ctx, n, 
					       GE_ADDWHACKARG (GE_POST,
							       ctx->whackarg));
				iter_push_args(ctx,
					       n->comp.args,
					       n->comp.nargs);
			} else {
				/*if in post mode evaluate */
				evalcomp(n);
				WHACK_SAVEDN_POP;
			}
			break;
		case USERTYPE_NODE:
			EDEBUG(" USERTYPE NODE");
			WHACK_SAVEDN_POP;
			break;
		case BOOL_NODE:
#ifdef EVAL_DEBUG
			printf (" BOOL NODE -- %p %s\n", n, n->bool_.bool_ ? "true" : "false");
#endif
			WHACK_SAVEDN_POP;
			break;
		default:
			gel_errorout (_("Unexpected node!"));
#ifdef EVAL_DEBUG
			{
				char *s = gel_string_print_etree (n);
				printf ("!!!!!!!!!!!!!!!UNEXPECTED_NODE (%p) (%d)\t-> %s\n", n, n->type, s);
				g_free (s);
			}
#endif
			WHACK_SAVEDN_POP;
			break;
		}
	}
	return TRUE;
}

GelCtx *
gel_eval_get_context(void)
{
	GelCtx *ctx = g_new0(GelCtx,1);
	ge_add_stack_array(ctx);
#ifdef MEM_DEBUG_FRIENDLY
	most_recent_ctx = ctx;
#endif
	return ctx;
}

void
gel_eval_free_context(GelCtx *ctx)
{
#ifdef MEM_DEBUG_FRIENDLY
	if (most_recent_ctx == ctx)
		most_recent_ctx = NULL;
#endif
	g_free(ctx->stack);
	g_free(ctx);
}

GelETree *
gel_eval_etree (GelCtx *ctx, GelETree *etree)
{
	/*level measures any recursion into here such as from
	  external functions etc, so that we can purge free lists,
	  but not during calculation*/
	static int level = 0;
	int flag;
	gpointer data;

#ifdef MEM_DEBUG_FRIENDLY
# ifdef EVAL_DEBUG
	if (level == 0) {
		deregister_all_trees ();
	}
# endif
#endif

	if (ctx->modulo != NULL) {
		GE_PUSH_STACK (ctx, ctx->modulo, GE_SETMODULO);
		ctx->modulo = NULL;
	}
	
	GE_PUSH_STACK(ctx,ctx->res,GE_RESULT);
	if(ctx->post) {
		GE_PUSH_STACK(ctx,ctx->current,
			      GE_ADDWHACKARG (GE_POST, ctx->whackarg));
	} else {
		GE_PUSH_STACK (ctx, ctx->current,
			       GE_ADDWHACKARG (GE_PRE, ctx->whackarg));
	}
	GE_PUSH_STACK (ctx, NULL, GE_EMPTY_STACK);
	ctx->res = etree;
	ctx->current = etree;
	ctx->post = FALSE;
	ctx->whackarg = FALSE;
	
	level++;

	if G_UNLIKELY (!iter_eval_etree(ctx)) {
		gpointer data;
		/*an exception happened*/
		ctx->current = NULL;
		gel_freetree (ctx->res);
		etree = ctx->res = NULL;
		do {
			GE_POP_STACK(ctx,data,flag);
			ev_free_special_data(ctx,data,flag);
		} while(flag != GE_EMPTY_STACK);
	}
	if G_UNLIKELY (--level == 0)
		purge_free_lists();
	
	GE_POP_STACK(ctx,ctx->current,flag);
	g_assert ((flag & GE_MASK) == GE_POST || (flag & GE_MASK) == GE_PRE);
	ctx->post = ((flag & GE_MASK) == GE_POST);
	ctx->whackarg = (flag & GE_WHACKARG);
	GE_POP_STACK(ctx,ctx->res,flag);
	flag = (flag & GE_MASK);
	g_assert(flag == GE_RESULT);

	GE_PEEK_STACK (ctx, data, flag);
	flag = (flag & GE_MASK);
	if (flag == GE_SETMODULO) {
		if (ctx->modulo != NULL) {
			mpw_clear (ctx->modulo);
			g_free (ctx->modulo);
		}
		ctx->modulo = data;
		GE_BLIND_POP_STACK (ctx);
	}

#ifdef MEM_DEBUG_FRIENDLY
# ifdef EVAL_DEBUG
	if (level == 0) {
		print_live_trees ();
	}
# endif
#endif

	return etree;
}

GelETree *
gel_gather_comparisons(GelETree *n)
{
	GelETree *next,*ret;
	if(!n) return NULL;

	ret = n;
	next = n->any.next;

	if(n->type == SPACER_NODE) {
		GelETree *t = n->sp.arg;
		freenode(n);
		ret = gel_gather_comparisons(t);
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
			GEL_GET_NEW_NODE(nn);
			nn->type = COMPARISON_NODE;
			nn->comp.nargs = 0;
			nn->comp.args = NULL;
			nn->comp.comp = NULL;
			
			for(;;) {
				GelETree *t;
				t = n->op.args->any.next;
				if(!ali) {
					ali = nn->comp.args =
						gel_gather_comparisons(n->op.args);
				} else {
					ali = ali->any.next = 
						gel_gather_comparisons(n->op.args);
				}
				ali->any.next = NULL;
				nn->comp.nargs++;
				nn->comp.comp =
					g_slist_append (nn->comp.comp,
							GINT_TO_POINTER((int)n->op.oper));

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
						gel_gather_comparisons(n);
					ali->any.next = NULL;
					nn->comp.nargs++;
					break;
				}
			}
			ret = nn;
			break;
		default:
			if(n->op.args) {
				n->op.args = gel_gather_comparisons(n->op.args);
				for(ali=n->op.args;ali->any.next;ali=ali->any.next)
					ali->any.next =
						gel_gather_comparisons(ali->any.next);
			}
		}
	} else if(n->type==MATRIX_NODE) {
		int i,j;
		int w,h;
		if(!n->mat.matrix ||
		   gel_is_matrix_value_only (n->mat.matrix)) {
			goto gather_comparisons_end;
		}
		w = gel_matrixw_width(n->mat.matrix);
		h = gel_matrixw_height(n->mat.matrix);
		gel_matrixw_make_private(n->mat.matrix, TRUE /* kill_type_caches */);
		for(j=0;j<h;j++) {
			for(i=0;i<w;i++) {
				GelETree *t = gel_matrixw_get_index(n->mat.matrix,i,j);
				if (t != NULL) {
					gel_matrixw_set_index(n->mat.matrix,i,j) =
						gel_gather_comparisons(t);
				}
			}
		}
	} else if(n->type==SET_NODE) {
		GelETree *ali;
		if(n->set.items) {
			n->set.items = gel_gather_comparisons(n->set.items);
			for(ali=n->set.items;ali->any.next;ali=ali->any.next)
				ali->any.next =
					gel_gather_comparisons(ali->any.next);
		}
	} else if(n->type==FUNCTION_NODE) {
		if ((n->func.func->type == GEL_USER_FUNC ||
		     n->func.func->type == GEL_VARIABLE_FUNC) &&
		    n->func.func->data.user) {
			n->func.func->data.user =
				gel_gather_comparisons(n->func.func->data.user);
		}
	}
gather_comparisons_end:
	ret->any.next = next;
	return ret;
}

void
gel_replace_equals (GelETree *n, gboolean in_expression)
{
	if (n == NULL)
		return;

	if (n->type == SPACER_NODE) {
		gel_replace_equals (n->sp.arg, in_expression);
	} else if(n->type == OPERATOR_NODE) {
		gboolean run_through_args = TRUE;
		if (n->op.oper == E_EQUALS &&
		    in_expression) {
			n->op.oper = E_EQ_CMP;
		} else if (n->op.oper == E_WHILE_CONS ||
			   n->op.oper == E_UNTIL_CONS ||
			   n->op.oper == E_IF_CONS) {
			run_through_args = FALSE;
			gel_replace_equals (n->op.args, TRUE);
			gel_replace_equals (n->op.args->any.next, in_expression);
		} else if (n->op.oper == E_DOWHILE_CONS ||
			   n->op.oper == E_DOUNTIL_CONS) {
			run_through_args = FALSE;
			gel_replace_equals (n->op.args, in_expression);
			gel_replace_equals (n->op.args->any.next, TRUE);
		} else if (n->op.oper == E_IFELSE_CONS) {
			run_through_args = FALSE;
			gel_replace_equals (n->op.args, TRUE);
			gel_replace_equals (n->op.args->any.next, in_expression);
			gel_replace_equals (n->op.args->any.next->any.next, in_expression);
		}

		if (run_through_args) {
			GelETree *args = n->op.args;
			while (args != NULL) {
				gel_replace_equals (args, in_expression);
				args = args->any.next;
			}
		}
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL &&
		   ! gel_is_matrix_value_only (n->mat.matrix)) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix, TRUE /* kill_type_caches */);
		for(j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				GelETree *t = gel_matrixw_get_index
					(n->mat.matrix, i, j);
				if (t != NULL)
					gel_replace_equals (t, in_expression);
			}
		}
	} else if (n->type == SET_NODE ) {
		GelETree *ali;
		for(ali = n->set.items; ali != NULL; ali = ali->any.next)
			gel_replace_equals (ali, in_expression);
	} else if (n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC) &&
		   n->func.func->data.user != NULL) {
		/* function bodies are a completely new thing */
		gel_replace_equals (n->func.func->data.user, FALSE);
	}
}

void
gel_replace_exp (GelETree *n)
{
	if (n == NULL)
		return;

	if (n->type == SPACER_NODE) {
		gel_replace_exp (n->sp.arg);
	} else if(n->type == OPERATOR_NODE) {
		GelETree *args;
		if (n->op.oper == E_EXP &&
		    n->op.args->type == IDENTIFIER_NODE &&
		    n->op.args->id.id->token != NULL &&
		    strcmp (n->op.args->id.id->token, "e") == 0) {
			n->op.oper = E_DIRECTCALL;
			n->op.args->id.id = d_intern ("exp");
		}

		args = n->op.args;
		while (args != NULL) {
			gel_replace_exp (args);
			args = args->any.next;
		}
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL &&
		   ! gel_is_matrix_value_only (n->mat.matrix)) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix, TRUE /* kill_type_caches */);
		for(j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				GelETree *t = gel_matrixw_get_index
					(n->mat.matrix, i, j);
				if (t != NULL)
					gel_replace_exp (t);
			}
		}
	} else if (n->type == SET_NODE ) {
		GelETree *ali;
		for(ali = n->set.items; ali != NULL; ali = ali->any.next)
			gel_replace_exp (ali);
	} else if (n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC) &&
		   n->func.func->data.user != NULL) {
		gel_replace_exp (n->func.func->data.user);
	}
}

/* Fixup number negation */
void
gel_fixup_num_neg (GelETree *n)
{
	if (n == NULL)
		return;

	if (n->type == SPACER_NODE) {
		gel_fixup_num_neg (n->sp.arg);
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
			gel_fixup_num_neg (n->op.args->any.next);
		} else {
			GelETree *args = n->op.args;
			while (args != NULL) {
				gel_fixup_num_neg (args);
				args = args->any.next;
			}
		}
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL &&
		   ! gel_is_matrix_value_only (n->mat.matrix)) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix, TRUE /* kill_type_caches */);
		for(j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				GelETree *t = gel_matrixw_get_index
					(n->mat.matrix, i, j);
				if (t != NULL)
					gel_fixup_num_neg (t);
			}
		}
	} else if (n->type == SET_NODE ) {
		GelETree *ali;
		for(ali = n->set.items; ali != NULL; ali = ali->any.next)
			gel_fixup_num_neg (ali);
	} else if (n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC) &&
		   n->func.func->data.user != NULL) {
		gel_fixup_num_neg (n->func.func->data.user);
	}
}

/* IMPORTANT: There's also a tree traversal function in symbolic.c */

/* find an identifier */
gboolean
gel_eval_find_identifier (GelETree *n, GelToken *tok, gboolean funcbody)
{
	if (n == NULL)
		return FALSE;

	if (n->type == SPACER_NODE) {
		return gel_eval_find_identifier (n->sp.arg, tok, funcbody);
	} else if (n->type == IDENTIFIER_NODE ) {
		if (n->id.id == tok)
			return TRUE;
		else
			return FALSE;
	} else if(n->type == OPERATOR_NODE) {
		GelETree *args = n->op.args;
		while (args != NULL) {
			if (gel_eval_find_identifier (args, tok, funcbody))
				return TRUE;
			args = args->any.next;
		}
		return FALSE;
	} else if (n->type == MATRIX_NODE &&
		   n->mat.matrix != NULL) {
		int i,j;
		int w,h;
		w = gel_matrixw_width (n->mat.matrix);
		h = gel_matrixw_height (n->mat.matrix);
		for(j = 0; j < h; j++) {
			for (i = 0; i < w; i++) {
				GelETree *t = gel_matrixw_get_index
					(n->mat.matrix, i, j);
				if (t != NULL &&
				    gel_eval_find_identifier (t, tok, funcbody))
					return TRUE;
			}
		}
		return FALSE;
	} else if (n->type == SET_NODE ) {
		GelETree *ali;
		for (ali = n->set.items; ali != NULL; ali = ali->any.next) {
			if (gel_eval_find_identifier (ali, tok, funcbody))
				return TRUE;
		}
		return FALSE;
	} else if (funcbody &&
		   n->type == FUNCTION_NODE &&
		   (n->func.func->type == GEL_USER_FUNC ||
		    n->func.func->type == GEL_VARIABLE_FUNC)) {
		D_ENSURE_USER_BODY (n->func.func);
		return gel_eval_find_identifier (n->func.func->data.user, tok,
					     funcbody);
	}
	return FALSE;
}

/*this means that it will precalc even complex and float
  numbers*/
static void
op_precalc_all_1 (GelETree *n, void (*func)(mpw_ptr,mpw_ptr))
{
	GelETree *l;
	mpw_t res;
	GEL_GET_L(n,l);
	if(l->type != VALUE_NODE)
		return;
	mpw_init(res);
	(*func)(res,l->val.value);
	if G_UNLIKELY (gel_error_num) {
		mpw_clear(res);
		gel_error_num = GEL_NO_ERROR;
		return;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_use_from(n,res);
}

static void
op_precalc_1 (GelETree *n,
	      void (*func)(mpw_ptr,mpw_ptr),
	      gboolean respect_type)
{
	GelETree *l;
	mpw_t res;
	GEL_GET_L(n,l);
	if (l->type != VALUE_NODE ||
	    (respect_type &&
	     (mpw_is_complex (l->val.value) ||
	      mpw_is_float (l->val.value))))
		return;
	mpw_init(res);
	(*func)(res,l->val.value);
	if G_UNLIKELY (gel_error_num) {
		mpw_clear(res);
		gel_error_num = GEL_NO_ERROR;
		return;
	}
	freetree_full(n,TRUE,FALSE);
	gel_makenum_use_from(n,res);
}

static void
op_precalc_2 (GelETree *n,
	      void (*func)(mpw_ptr,mpw_ptr,mpw_ptr),
	      gboolean respect_type)
{
	GelETree *l,*r,*next;
	mpw_t res;
	GEL_GET_LR(n,l,r);
	if (l->type != VALUE_NODE ||
	    r->type != VALUE_NODE ||
	    (respect_type &&
	     (mpw_is_complex (l->val.value) ||
	      mpw_is_complex (r->val.value) ||
	      mpw_is_float (l->val.value) ||
	      mpw_is_float (r->val.value))))
		return;
	mpw_init(res);
	(*func)(res,l->val.value,r->val.value);
	if G_UNLIKELY (gel_error_num) {
		mpw_clear(res);
		gel_error_num = GEL_NO_ERROR;
		return;
	}
	next = n->any.next;
	freetree_full(n,TRUE,FALSE);
	gel_makenum_use_from(n,res);
	n->any.next = next;
}

static void
try_to_precalc_op (GelETree *n, gboolean respect_type)
{
	switch(n->op.oper) {
	case E_NEG:
		op_precalc_all_1 (n, mpw_neg);
		return;
	case E_ABS:
		op_precalc_1 (n, mpw_abs, respect_type);
		return;
	case E_FACT:
		op_precalc_1 (n, mpw_fac, respect_type);
		return;
	case E_DBLFACT:
		op_precalc_1 (n, mpw_dblfac, respect_type);
		return;
	case E_PLUS:
		op_precalc_2 (n, mpw_add, respect_type);
		return;
	case E_ELTPLUS:
		op_precalc_2 (n, mpw_add, respect_type);
		return;
	case E_MINUS:
		op_precalc_2 (n, mpw_sub, respect_type);
		return;
	case E_ELTMINUS:
		op_precalc_2 (n, mpw_sub, respect_type);
		return;
	case E_MUL:
		op_precalc_2 (n, mpw_mul, respect_type);
		return;
	case E_ELTMUL:
		op_precalc_2 (n, mpw_mul, respect_type);
		return;
	case E_DIV:
		op_precalc_2 (n, mpw_div, respect_type);
		return;
	case E_ELTDIV:
		op_precalc_2 (n, mpw_div, respect_type);
		return;
	case E_MOD:
		op_precalc_2 (n, mpw_mod, respect_type);
		return;
	/* FIXME: this could be time consuming, somehow catch that */
	case E_EXP:
		op_precalc_2 (n, mpw_pow, respect_type);
		return;
	case E_ELTEXP:
		op_precalc_2 (n, mpw_pow, respect_type);
		return;
	default:
		return;
	}
}

void
gel_try_to_do_precalc(GelETree *n)
{
	if(!n) return;

	if(n->type==OPERATOR_NODE) {
		GelETree *ali;

		/* double negation is always positive no matter what */
		if (n->op.oper == E_NEG &&
		    n->op.args->type == OPERATOR_NODE &&
		    n->op.args->op.oper == E_NEG) {
			GelETree *nn;
			nn = n->op.args->op.args;
			n->op.args->op.args = NULL;
			replacenode (n, nn);
			gel_try_to_do_precalc (n);
		} else if(n->op.oper == E_MOD_CALC) {
			/* in case of modular calculation, only do
			   precalc on the second argument (don't descend
			   at all into the first one) */
			/* FIXME: precalc might be broken in case of mod */
			/* gel_try_to_do_precalc(n->op.args->any.next); */;
		} else {
			if(n->op.args) {
				for(ali=n->op.args;ali;ali=ali->any.next)
					gel_try_to_do_precalc(ali);
			}
			if(n->type==OPERATOR_NODE)
				try_to_precalc_op (n,
						   TRUE /* respect_type */);
		}
	} else if(n->type==MATRIX_NODE) {
		int i,j;
		int w,h;
		if (n->mat.matrix == NULL ||
		    gel_is_matrix_value_only (n->mat.matrix))
			return;
		w = gel_matrixw_width(n->mat.matrix);
		h = gel_matrixw_height(n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix, TRUE /* kill_type_caches */);
		for(j=0;j<h;j++) {
			for(i=0;i<w;i++) {
				GelETree *t = gel_matrixw_get_index(n->mat.matrix,i,j);
				if(t)
					gel_try_to_do_precalc(t);
			}
		}
	} else if(n->type==SET_NODE) {
		GelETree *ali;
		if(n->set.items) {
			for(ali=n->set.items;ali;ali=ali->any.next)
				gel_try_to_do_precalc(ali);
		}
	} else if(n->type==FUNCTION_NODE) {
		if ((n->func.func->type == GEL_USER_FUNC ||
		     n->func.func->type == GEL_VARIABLE_FUNC) &&
		    n->func.func->data.user)
			gel_try_to_do_precalc(n->func.func->data.user);
	}
}

gboolean
gel_is_tree_same (GelETree *l, GelETree *r)
{
	if (l == NULL && r == NULL)
		return TRUE;
	if (l == NULL || r == NULL)
		return FALSE;

	if (l->type != r->type)
		return FALSE;

	if (l->type == NULL_NODE) {
		return TRUE;
	} else if (l->type == VALUE_NODE) {
		return mpw_symbolic_eql (l->val.value, r->val.value);
	} else if (l->type == OPERATOR_NODE) {
		GelETree *ali, *bli;
		if (l->op.oper != r->op.oper || l->op.nargs != r->op.nargs)
			return FALSE;
		for (ali = l->op.args, bli = r->op.args;
		     ali != NULL && bli != NULL;
		     ali = ali->any.next, bli = bli->any.next) {
			if ( ! gel_is_tree_same (ali, bli))
				return FALSE;
		}
		return TRUE;
	} else if (l->type == IDENTIFIER_NODE) {
		if (l->id.id == r->id.id)
			return TRUE;
		else
			return FALSE;
	} else if (l->type == STRING_NODE) {
		if (l->str.str != NULL && /* sanity only! */
		    r->str.str != NULL &&
		    strcmp (l->str.str, r->str.str) == 0)
			return TRUE;
		else
			return FALSE;
	} else if (l->type == BOOL_NODE) {
		if ((l->bool_.bool_ && r->bool_.bool_) ||
		    ( ! l->bool_.bool_ && ! r->bool_.bool_))
			return TRUE;
		else
			return FALSE;
	} else if (l->type == MATRIX_NODE) {
		int i, j;
		int w, h;
		if G_UNLIKELY (l->mat.matrix == NULL ||
			       r->mat.matrix == NULL)
			return FALSE /* possible? */;
		w = gel_matrixw_width (l->mat.matrix);
		if (w != gel_matrixw_width (r->mat.matrix))
			return FALSE;
		h = gel_matrixw_height (l->mat.matrix);
		if (h != gel_matrixw_height (r->mat.matrix))
			return FALSE;
		for (i = 0; i < w; i++) {
			for (j = 0; j < h; j++) {
				GelETree *lt = gel_matrixw_index (l->mat.matrix, i, j);
				GelETree *rt = gel_matrixw_index (r->mat.matrix, i, j);
				if ( ! gel_is_tree_same (lt, rt))
					return FALSE;
			}
		}
		return TRUE;
	/* FIXME: SET_NODE */
	/* FIXME: POLYNOMIAL_NODE */
	/* FIXME: FUNCTION_NODE */
	/* FIXME: COMPARISON_NODE */
	/* FIXME: USERTYPE_NODE */
	}
	return FALSE;
}

/* FIXME: this is incomplete and stupid! */
static gboolean
oper_reshufle (GelETree *n, int oper)
{
	gboolean shuffled = FALSE;

	/* First sort out multiplications or addi */
	if (n->op.oper == oper) {
		GelETree *l, *r;

		GEL_GET_LR (n, l, r);

		/* always swap values to go first */
		if (r->type == VALUE_NODE &&
		    l->type != VALUE_NODE) {
			n->op.args = r;
			r->any.next = l;
			l->any.next = NULL;

			shuffled = TRUE;

			GEL_GET_LR (n, l, r);
		}

		/* make into (a*b)*c, "*" is * or + (oper) */
		/* unless a is a value and b and c are not */
		if (r->type == OPERATOR_NODE &&
		    r->op.oper == oper) {
			GelETree *a, *b, *c;
			a = l;
			b = r->op.args;
			c = r->op.args->any.next;

			if ( ! (a->type == VALUE_NODE &&
				b->type != VALUE_NODE &&
				c->type != VALUE_NODE)) {
				r->op.args = NULL;
				gel_freetree (r);

				GEL_GET_NEW_NODE (l);
				l->type = OPERATOR_NODE;
				l->op.oper = oper;
				l->op.nargs = 2;
				l->op.args = a;
				a->any.next = b;
				b->any.next = NULL;

				n->op.args = l;
				l->any.next = c;
				c->any.next = NULL;

				shuffled = TRUE;

				GEL_GET_LR (n, l, r);
			}
		}

		/* if (a*b)*c and a is a value and b and c are not
		   make into a*(b*c) */
		if (l->type == OPERATOR_NODE &&
		    l->op.oper == oper) {
			GelETree *a, *b, *c;
			a = l->op.args;
			b = l->op.args->any.next;
			c = r;

			if (a->type == VALUE_NODE &&
			    b->type != VALUE_NODE &&
			    c->type != VALUE_NODE) {
				l->op.args = NULL;
				gel_freetree (l);

				GEL_GET_NEW_NODE (r);
				r->type = OPERATOR_NODE;
				r->op.oper = oper;
				r->op.nargs = 2;
				r->op.args = b;
				b->any.next = c;
				c->any.next = NULL;

				n->op.args = a;
				a->any.next = r;
				r->any.next = NULL;

				shuffled = TRUE;

				/* GEL_GET_LR (n, l, r); */
			}
		}
	}
	return shuffled;
}

void
gel_simplify (GelETree *n)
{
resimplify:
	if (n == NULL)
		return;

	if (n->type == OPERATOR_NODE) {
		GelETree *ali;

		/* double negation is always positive no matter what */
		if (n->op.oper == E_NEG &&
		    n->op.args->type == OPERATOR_NODE &&
		    n->op.args->op.oper == E_NEG) {
			GelETree *nn;
			nn = n->op.args->op.args;
			n->op.args->op.args = NULL;
			replacenode (n, nn);
			goto resimplify;
		} else if(n->op.oper == E_MOD_CALC) {
			/* in case of modular calculation, only do
			   precalc on the second argument (don't descend
			   at all into the first one) */
			/* FIXME: precalc might be broken in case of mod */
			/* gel_try_to_do_precalc(n->op.args->any.next); */;
			/* double negation is always positive no matter what */
			return;
		}

		if(n->op.args) {
			for(ali=n->op.args;ali;ali=ali->any.next)
				gel_simplify (ali);
		}

		/* be aggressive! */
		try_to_precalc_op (n, FALSE /* respect_type */);
		if (n->type != OPERATOR_NODE)
			return;

		/* FIXME: we want to assume addition ALWAYS comutes and
		   multiplication sometimes commutes (must get some type
		   info!).  We can always at least move all numbers through
		   and sort them by type and then precompute them */
		/* We can always assume associativity anyway! */

		/* sort out multiplications and additions,
		   putting all values first */
		if (oper_reshufle (n, E_MUL)) {
			goto resimplify;
		}
		if (oper_reshufle (n, E_PLUS)) {
			goto resimplify;
		}

		/* Now try to put together multiplications and exponents */
		/* FIXME: this is too specific be more general!, though maybe if we sort out all
		   multiplication and addition as above, things will work nicely */
		if (n->op.oper == E_MUL) {
			GelETree *l, *r;
			GelETree *ll, *rr;
			GelETree *le = NULL, *re = NULL;
			GEL_GET_LR (n, l, r);
			ll = l;
			rr = r;
			if (l->type == OPERATOR_NODE &&
			    l->op.oper == E_EXP) {
				ll = l->op.args;
				le = l->op.args->any.next;
			}
			if (r->type == OPERATOR_NODE &&
			    r->op.oper == E_EXP) {
				rr = r->op.args;
				re = r->op.args->any.next;
			}
			/* we can put this together! */
			if (gel_is_tree_same (ll, rr)) {
				GelETree *nn, *e;

				n->op.args = NULL;
				gel_freetree (rr);
				if (re != NULL) {
					r->op.args = NULL;
					gel_freetree (r);
				}
				if (l != ll) {
					l->op.args = NULL;
					gel_freetree (l);
				}

				GEL_GET_NEW_NODE (e);
				e->type = OPERATOR_NODE;
				e->op.oper = E_PLUS;
				e->op.nargs = 2;
				if (le == NULL) {
					e->op.args = gel_makenum_ui (1);
				} else {
					e->op.args = le;
				}
				if (re == NULL) {
					e->op.args->any.next = gel_makenum_ui (1);
				} else {
					e->op.args->any.next = re;
				}
				e->op.args->any.next->any.next = NULL;

				GEL_GET_NEW_NODE (nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_EXP;
				nn->op.nargs = 2;

				nn->op.args = ll;
				ll->any.next = e;
				e->any.next = NULL;

				replacenode (n, nn);

				goto resimplify;
			}
		}

		/* FIXME: this is just like for E_MUL except re and le
		   are on the other side si there are some changes */
		if (n->op.oper == E_PLUS) {
			GelETree *l, *r;
			GelETree *ll, *rr;
			GelETree *le = NULL, *re = NULL;
			GEL_GET_LR (n, l, r);
			ll = l;
			rr = r;
			if (l->type == OPERATOR_NODE &&
			    l->op.oper == E_MUL) {
				le = l->op.args;
				ll = l->op.args->any.next;
			}
			if (r->type == OPERATOR_NODE &&
			    r->op.oper == E_MUL) {
				re = r->op.args;
				rr = r->op.args->any.next;
			}
			/* we can put this together! */
			if (gel_is_tree_same (ll, rr)) {
				GelETree *nn, *e;

				n->op.args = NULL;
				gel_freetree (rr);
				if (re != NULL) {
					r->op.args = NULL;
					gel_freetree (r);
				}
				if (l != ll) {
					l->op.args = NULL;
					gel_freetree (l);
				}

				GEL_GET_NEW_NODE (e);
				e->type = OPERATOR_NODE;
				e->op.oper = E_PLUS;
				e->op.nargs = 2;
				if (le == NULL) {
					e->op.args = gel_makenum_ui (1);
				} else {
					e->op.args = le;
				}
				if (re == NULL) {
					e->op.args->any.next = gel_makenum_ui (1);
				} else {
					e->op.args->any.next = re;
				}
				e->op.args->any.next->any.next = NULL;

				GEL_GET_NEW_NODE (nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_MUL;
				nn->op.nargs = 2;

				nn->op.args = e;
				e->any.next = ll;
				ll->any.next = NULL;

				replacenode (n, nn);

				goto resimplify;
			}
		}

		if (n->op.oper == E_MUL &&
		    (n->op.args->type == VALUE_NODE ||
		     n->op.args->any.next->type == VALUE_NODE)) {
			GelETree *l, *r;
			GEL_GET_LR (n, l, r);

			/* multiply by 0, so nothing */
			if ((l->type == VALUE_NODE &&
			     mpw_zero_p (l->val.value)) ||
			    (r->type == VALUE_NODE &&
			     mpw_zero_p (r->val.value))) {
				freetree_full (n, TRUE, FALSE);
				gel_makenum_ui_from (n, 0);
			} else if (l->type == VALUE_NODE &&
				   mpw_eql_ui (l->val.value, 1)) {
				/* multiply by 1, so identity */
				n->op.args = NULL;
				gel_freetree (l);
				replacenode (n, r);
			} else if (r->type == VALUE_NODE &&
				   mpw_eql_ui (r->val.value, 1)) {
				/* multiply by 1, so identity */
				n->op.args = NULL;
				gel_freetree (r);
				replacenode (n, l);
			}
		} else if (n->op.oper == E_DIV &&
			   (n->op.args->type == VALUE_NODE ||
			    n->op.args->any.next->type == VALUE_NODE)) {
			GelETree *l, *r;
			GEL_GET_LR (n, l, r);

			/* divide 0 by something so nothing
			   (unless the bottom is 0) */
			if ((l->type == VALUE_NODE &&
			     mpw_zero_p (l->val.value)) &&
			    (r->type != VALUE_NODE ||
			     ! mpw_zero_p (r->val.value))) {
				freetree_full (n, TRUE, FALSE);
				gel_makenum_ui_from (n, 0);
			} else if (r->type == VALUE_NODE &&
				   mpw_eql_ui (r->val.value, 1)) {
				/* divide by 1, so identity */
				n->op.args = NULL;
				gel_freetree (r);
				replacenode (n, l);
			}
		} else if (n->op.oper == E_PLUS &&
			   (n->op.args->type == VALUE_NODE ||
			    n->op.args->any.next->type == VALUE_NODE)) {
			GelETree *l, *r;
			GEL_GET_LR (n, l, r);

			if (l->type == VALUE_NODE &&
			    mpw_zero_p (l->val.value)) {
				/* add 0, so identity */
				n->op.args = NULL;
				gel_freetree (l);
				replacenode (n, r);
			} else if (r->type == VALUE_NODE &&
				   mpw_zero_p (r->val.value)) {
				/* add 0, so identity */
				n->op.args = NULL;
				gel_freetree (r);
				replacenode (n, l);
			}
		} else if (n->op.oper == E_EXP) {
			GelETree *l, *r;
			GEL_GET_LR (n, l, r);

			if (r->type == VALUE_NODE &&
			    mpw_zero_p (r->val.value)) {
				/* something^0 so we get 1 */
				freetree_full (n, TRUE, FALSE);
				gel_makenum_ui_from (n, 1);
			} else if (l->type == OPERATOR_NODE &&
				   l->op.oper == E_EXP) {
				/* (x^v)^w => x^(v*w);
				   and then simplify again */
				GelETree *nn;
				GelETree *x, *v, *w;

				x = l->op.args;
				v = l->op.args->any.next;
				w = r;
				l->op.args = NULL;
				gel_freetree (l);

				GEL_GET_NEW_NODE (nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_MUL;
				nn->op.nargs = 2;

				nn->op.args = v;
				v->any.next = w;
				w->any.next = NULL;

				n->op.args = x;
				x->any.next = nn;
				nn->any.next = NULL;

				goto resimplify;
			}
		}
	} else if(n->type==MATRIX_NODE) {
		int i,j;
		int w,h;
		if (n->mat.matrix == NULL ||
		    gel_is_matrix_value_only (n->mat.matrix))
			return;
		w = gel_matrixw_width(n->mat.matrix);
		h = gel_matrixw_height(n->mat.matrix);
		gel_matrixw_make_private (n->mat.matrix, TRUE /* kill_type_caches */);
		for(j=0;j<h;j++) {
			for(i=0;i<w;i++) {
				GelETree *t = gel_matrixw_get_index(n->mat.matrix,i,j);
				if(t)
					gel_simplify (t);
			}
		}
	} else if(n->type==SET_NODE) {
		GelETree *ali;
		if(n->set.items) {
			for(ali=n->set.items;ali;ali=ali->any.next)
				gel_simplify (ali);
		}
	} else if(n->type==FUNCTION_NODE) {
		if ((n->func.func->type == GEL_USER_FUNC ||
		     n->func.func->type == GEL_VARIABLE_FUNC) &&
		    n->func.func->data.user)
			gel_simplify (n->func.func->data.user);
	}
}

#ifndef MEM_DEBUG_FRIENDLY
/* In tests it seems that this achieves better then 4096 */
#define GEL_CHUNK_SIZE 4048
#define ALIGNED_SIZE(t) (sizeof(t) + sizeof (t) % G_MEM_ALIGN)

static long _gel_tree_num = 0;
static gboolean _gel_max_nodes_check = TRUE;

/* Will get to the warning another page later, but that's OK
 * we don't expect this to be happening often */
void
gel_test_max_nodes_again (void)
{
	_gel_max_nodes_check = TRUE;
}

void
_gel_make_free_trees (void)
{
	int i;
	char *p;

	if G_UNLIKELY (_gel_max_nodes_check &&
		       gel_calcstate.max_nodes > 0 &&
		       _gel_tree_num > gel_calcstate.max_nodes) {
		if (_gel_tree_limit_hook != NULL) {
			(*_gel_tree_limit_hook) ();
		}
		_gel_max_nodes_check = FALSE;
	}

	p = g_malloc ((GEL_CHUNK_SIZE / ALIGNED_SIZE (GelETree)) *
		      ALIGNED_SIZE (GelETree));
	for (i = 0; i < (GEL_CHUNK_SIZE / ALIGNED_SIZE (GelETree)); i++) {
		GelETree *t = (GelETree *)p;
		/*put onto the free list*/
		t->any.next = gel_free_trees;
		gel_free_trees = t;
		p += ALIGNED_SIZE (GelETree);
		_gel_tree_num ++;
	}
}

static void
_gel_make_free_evl (void)
{
	int i;
	char *p;

	p = g_malloc ((GEL_CHUNK_SIZE / ALIGNED_SIZE (GelEvalLoop)) *
		      ALIGNED_SIZE (GelEvalLoop));
	for (i = 0; i < (GEL_CHUNK_SIZE / ALIGNED_SIZE (GelEvalLoop)); i++) {
		GelEvalLoop *t = (GelEvalLoop *)p;
		/*put onto the free list*/
		t->condition = (gpointer)free_evl;
		free_evl = t;
		p += ALIGNED_SIZE (GelEvalLoop);
	}
}

static void
_gel_make_free_evf (void)
{
	int i;
	char *p;

	p = g_malloc ((GEL_CHUNK_SIZE / ALIGNED_SIZE (GelEvalFor)) *
		      ALIGNED_SIZE (GelEvalFor));
	for (i = 0; i < (GEL_CHUNK_SIZE / ALIGNED_SIZE (GelEvalFor)); i++) {
		GelEvalFor *t = (GelEvalFor *)p;
		/*put onto the free list*/
		t->body = (gpointer)free_evf;
		free_evf = t;
		p += ALIGNED_SIZE (GelEvalFor);
	}
}

static void
_gel_make_free_evfi (void)
{
	int i;
	char *p;

	p = g_malloc ((GEL_CHUNK_SIZE / ALIGNED_SIZE (GelEvalForIn)) *
		      ALIGNED_SIZE (GelEvalForIn));
	for (i = 0; i < (GEL_CHUNK_SIZE / ALIGNED_SIZE (GelEvalForIn)); i++) {
		GelEvalForIn *t = (GelEvalForIn *)p;
		/*put onto the free list*/
		t->body = (gpointer)free_evfi;
		free_evfi = t;
		p += ALIGNED_SIZE (GelEvalForIn);
	}
}
#endif /* ! MEM_DEBUG_FRIENDLY */

#ifdef MEM_DEBUG_FRIENDLY
# ifdef EVAL_DEBUG
static GSList *trees_list = NULL;
void
register_new_tree (GelETree *n)
{
	trees_list = g_slist_prepend (trees_list, n);
}
void
deregister_tree (GelETree *n)
{
	trees_list = g_slist_remove (trees_list, n);
}
void
print_live_trees (void)
{
	GSList *li;
	int count = 0;
	for (li = trees_list; li != NULL; li = li->next) {
		char *s;
		GelETree *n = li->data;
		s = gel_string_print_etree (n);
		printf ("TREE %p:\t%s\n", n, s);
		g_free (s);
		count ++;
	}
	printf ("count %d:\n", count);
}
void
deregister_all_trees (void)
{
	g_slist_free (trees_list);
	trees_list = NULL;
}
# endif /* EVAL_DEBUG */
#endif /* MEM_DEBUG_FRIENDLY */
