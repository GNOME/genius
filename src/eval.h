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

#ifndef _EVAL_H_
#define _EVAL_H_

#include "mpwrap.h"

/*declarations of structures*/
#include "structs.h"

/* builtin primitives */
enum {
	E_SEPAR = 0,
	E_EQUALS,
	E_ABS,
	E_PLUS,
	E_MINUS,
	E_MUL,
	E_DIV,
	E_BACK_DIV,
	E_MOD,
	E_NEG,
	E_EXP,
	E_FACT,
	E_TRANSPOSE,
	E_CONJUGATE_TRANSPOSE,
	E_IF_CONS,
	E_IFELSE_CONS,
	E_WHILE_CONS,
	E_UNTIL_CONS,
	E_DOWHILE_CONS,
	E_DOUNTIL_CONS,
	E_FOR_CONS,
	E_FORBY_CONS,
	E_FORIN_CONS,
	E_SUM_CONS,
	E_SUMBY_CONS,
	E_SUMIN_CONS,
	E_PROD_CONS,
	E_PRODBY_CONS,
	E_PRODIN_CONS,
	E_EQ_CMP,
	E_NE_CMP,
	E_CMP_CMP,
	E_LT_CMP,
	E_GT_CMP,
	E_LE_CMP,
	E_GE_CMP,
	E_LOGICAL_AND,
	E_LOGICAL_OR,
	E_LOGICAL_XOR,
	E_LOGICAL_NOT,
	E_REGION_SEP,
	E_GET_VELEMENT,
	E_GET_ELEMENT,
	E_GET_REGION,
	E_GET_ROW_REGION,
	E_GET_COL_REGION,
	E_QUOTE,
	E_REFERENCE,
	E_DEREFERENCE,
	E_DIRECTCALL,
	E_CALL,
	E_RETURN,
	E_BAILOUT,
	E_EXCEPTION,
	E_CONTINUE,
	E_BREAK,
	E_MOD_CALC,
	E_OPER_LAST
};



/*table of operators, at least the primitive types*/
enum {
	GO_VALUE=1<<0,
	GO_MATRIX=1<<1,
	GO_STRING=1<<2,
	GO_FUNCTION=1<<3,
};
typedef gboolean (*GelEvalFunc)(GelCtx *ctx, GelETree *n, ...);
/*primitive operations can be like this*/
typedef struct _GelOperPrim GelOperPrim;
struct _GelOperPrim {
	guint32 arg[3]; /*bitmap of allowable types for arguments*/
	GelEvalFunc evalfunc;
};
#define OP_TABLE_LEN 10
typedef struct _GelOper GelOper;
struct _GelOper {
	GelOperPrim prim[OP_TABLE_LEN];
};

/*functions for manipulating a tree*/
GelETree * gel_makenum(mpw_t num);
GelETree * gel_makenum_use(mpw_t num); /*don't create a new number*/
GelETree * gel_makenum_ui(unsigned long num);
GelETree * gel_makenum_si(long num);
GelETree * gel_makenum_null(void);
GelETree * gel_makenum_identifier (GelToken *id);
GelETree * gel_makenum_string (const char *str);
GelETree * makeoperator(int oper, GSList **stack);

/*make new node, but don't actually get a new GelETree, just stick it
  into an already allocated but unused structure*/
void gel_makenum_from(GelETree *n, mpw_t num);
void gel_makenum_use_from(GelETree *n, mpw_t num); /*don't create a new number*/
void gel_makenum_ui_from(GelETree *n, unsigned long num);
void gel_makenum_si_from(GelETree *n, long num);
void gel_makenum_null_from(GelETree *n);

/*returns the number of args for an operator, or -1 if it takes up till
  exprlist marker -2 if it takes 1 past the marker for the first argument*/
int branches(int op) G_GNUC_CONST;

/*copy a node*/
GelETree * copynode(GelETree *o);

/*functions for reclaiming memory*/
void gel_freetree(GelETree *n);

/* you need to get, then free an evaluation context*/
GelCtx * eval_get_context(void);
void eval_free_context(GelCtx *ctx);
/* evaluate tree*/
GelETree * eval_etree(GelCtx *ctx, GelETree *etree);

/*return TRUE if node is true (a number node !=0, or nonempty string),
  false otherwise*/
int isnodetrue(GelETree *n, int *bad_node);

/*call a function (arguments should have already been evaluated)*/
GelETree * funccall(GelCtx *ctx, GelEFunc *func, GelETree **args, int nargs);
void gel_expandmatrix (GelETree *n);

GelETree * gather_comparisons(GelETree *n);
GelETree * replace_parameters(GelETree *n);
void replace_equals (GelETree *n, gboolean in_expression);
void try_to_do_precalc(GelETree *n);


#define GET_ABCDE(n,a,b,c,d,e) { \
	(a) = (n)->op.args; \
	(b) = (n)->op.args->any.next; \
	(c) = (n)->op.args->any.next->any.next; \
	(d) = (n)->op.args->any.next->any.next->any.next; \
	(e) = (n)->op.args->any.next->any.next->any.next->any.next; \
}
#define GET_ABCD(n,a,b,c,d) { \
	(a) = (n)->op.args; \
	(b) = (n)->op.args->any.next; \
	(c) = (n)->op.args->any.next->any.next; \
	(d) = (n)->op.args->any.next->any.next->any.next; \
}
#define GET_LRR(n,l,r,rr) { \
	(l) = (n)->op.args; \
	(r) = (n)->op.args->any.next; \
	(rr) = (n)->op.args->any.next->any.next; \
}
#define GET_LR(n,l,r) { (l) = (n)->op.args; (r) = (n)->op.args->any.next; }
#define GET_L(n,l) { (l) = (n)->op.args; }

extern GelETree *free_trees;

#define GET_NEW_NODE(n) {				\
	if(!free_trees)					\
		n = g_new(GelETree,1);			\
	else {						\
		n = free_trees;				\
		free_trees = free_trees->any.next;	\
	}						\
}

extern GelEFunc *_internal_ln_function;
extern GelEFunc *_internal_exp_function;

#endif
