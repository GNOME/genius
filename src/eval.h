/* GENIUS Calculator
 * Copyright (C) 1997-2004 Jiri (George) Lebl
 *
 * Author: Jiri (George) Lebl
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

#ifndef EVAL_H
#define EVAL_H

/* #define EVAL_DEBUG 1 */
/* Note: this won't be completely mem-debug friendly, but only mostly */
/* #define MEM_DEBUG_FRIENDLY 1 */
/* #define MEM_DEBUG_SUPER_FRIENDLY 1 */

#include "mpwrap.h"

/*declarations of structures*/
#include "structs.h"

/* builtin primitives */
enum {
	E_SEPAR = 0,
	E_EQUALS, /* see E_DEFEQUALS (on the end not to break bincompat) */
	E_PARAMETER,
	E_ABS,
	E_PLUS,
	E_MINUS,
	E_MUL,
	E_ELTMUL,
	E_DIV,
	E_ELTDIV,
	E_BACK_DIV,
	E_ELT_BACK_DIV,
	E_MOD,
	E_ELTMOD,
	E_NEG,
	E_EXP,
	E_ELTEXP,
	E_FACT,
	E_DBLFACT,
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
	E_REGION_SEP_BY,
	E_GET_VELEMENT,
	E_GET_ELEMENT,
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
	E_DEFEQUALS,
	E_OPER_LAST
};


/*table of operators, at least the primitive types*/
enum {
	GO_VALUE=1<<0,
	GO_MATRIX=1<<1,
	GO_STRING=1<<2,
	GO_FUNCTION=1<<3,
	GO_POLYNOMIAL=1<<4,
	GO_IDENTIFIER=1<<5,
	GO_BOOL=1<<6,
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
GelETree * gel_makenum_d (double num);
GelETree * gel_makenum_bool (gboolean bool_);
GelETree * gel_makenum_null(void);
GelETree * gel_makenum_identifier (GelToken *id);
GelETree * gel_makenum_string (const char *str);
GelETree * gel_makenum_string_use (char *str);
GelETree * gel_makenum_string_constant (const char *str);
/* FIXME: implement GelETree * gel_makenum_polynomial (...); */
GelETree * makeoperator(int oper, GSList **stack);

/*make new node, but don't actually get a new GelETree, just stick it
  into an already allocated but unused structure*/
void gel_makenum_from(GelETree *n, mpw_t num);
void gel_makenum_use_from(GelETree *n, mpw_t num); /*don't create a new number*/
void gel_makenum_ui_from(GelETree *n, unsigned long num);
void gel_makenum_si_from(GelETree *n, long num);
void gel_makenum_bool_from (GelETree *n, gboolean bool_);
void gel_makenum_null_from(GelETree *n);

/*copy a node*/
GelETree * copynode(GelETree *o);

/*functions for reclaiming memory*/
void gel_freetree(GelETree *n);
void gel_emptytree(GelETree *n);

/* Makes a new node and replaces the old one with NULL_NODE */
GelETree * gel_stealnode (GelETree *n);

/* you need to get, then free an evaluation context*/
GelCtx * eval_get_context(void);
void eval_free_context(GelCtx *ctx);
/* evaluate tree*/
GelETree * eval_etree(GelCtx *ctx, GelETree *etree);

/*return TRUE if node is true (a number node !=0, or nonempty string),
  false otherwise*/
int gel_isnodetrue(GelETree *n, int *bad_node);

/*call a function (arguments should have already been evaluated)*/
GelETree * funccall(GelCtx *ctx, GelEFunc *func, GelETree **args, int nargs);
void gel_expandmatrix (GelETree *n);

/* func is a function taking one argument and l is a function/identifier */
/* Note: a copy of the function is made */
GelETree * function_from_function (GelEFunc *func, GelETree *l);

/* Functions to fixup the parsed tree */
GelETree * gather_comparisons(GelETree *n);
void replace_equals (GelETree *n, gboolean in_expression);
void replace_exp (GelETree *n);
void fixup_num_neg (GelETree *n);
void try_to_do_precalc(GelETree *n);

/* find an identifier */
gboolean eval_find_identifier (GelETree *n, GelToken *tok);

char * gel_similar_possible_ids (const char *id);


/* return a list of used local functions (copies of) */
GSList * gel_subst_local_vars (GSList *, GelETree *n);

void gel_mod_node (GelCtx *ctx, GelETree *n);
gboolean gel_mod_integer_rational (mpw_t num, mpw_t mod);

mpw_ptr gel_find_pre_function_modulo (GelCtx *ctx);

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


#ifdef MEM_DEBUG_FRIENDLY

# ifdef EVAL_DEBUG
void register_new_tree (GelETree *n);
void deregister_tree (GelETree *n);
void print_live_trees (void);
void deregister_all_trees (void);
#  define GET_NEW_NODE(n) {				\
	n = g_new0 (GelETree, 1);			\
	printf ("%s NEW NODE %p\n", G_STRLOC, n);	\
	register_new_tree (n);				\
}
# else /* EVAL_DEBUG */

#  define GET_NEW_NODE(n) {				\
	n = g_new0 (GelETree, 1);			\
}
# endif /* EVAL_DEBUG */

#else /* MEM_DEBUG_FRIENDLY */

void _gel_make_free_trees (void);
# define GET_NEW_NODE(n) {				\
	if G_UNLIKELY (free_trees == NULL)		\
		_gel_make_free_trees ();		\
	n = free_trees;					\
	free_trees = free_trees->any.next;		\
}
#endif

extern GelEFunc *_internal_ln_function;
extern GelEFunc *_internal_exp_function;

#endif /* EVAL_H */
