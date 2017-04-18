/* GENIUS Calculator
 * Copyright (C) 1997-2017 Jiri (George) Lebl
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

#ifndef _GEL_EVAL_H
#define _GEL_EVAL_H

/* #define EVAL_DEBUG 1 */
/* Note: this won't be completely mem-debug friendly, but only mostly */
/* #define MEM_DEBUG_FRIENDLY 1 */
/* #define MEM_DEBUG_SUPER_FRIENDLY 1 */

#include "mpwrap.h"

/*declarations of structures*/
#include "structs.h"

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

void gel_init (void);

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
GelETree * gel_makeoperator(int oper, GSList **stack);

/*make new node, but don't actually get a new GelETree, just stick it
  into an already allocated but unused structure*/
void gel_makenum_from(GelETree *n, mpw_t num);
void gel_makenum_use_from(GelETree *n, mpw_t num); /*don't create a new number*/
void gel_makenum_ui_from(GelETree *n, unsigned long num);
void gel_makenum_si_from(GelETree *n, long num);
void gel_makenum_bool_from (GelETree *n, gboolean bool_);
void gel_makenum_null_from(GelETree *n);

/*copy a node*/
GelETree * gel_copynode(GelETree *o);

void gel_replacenode (GelETree *to, GelETree *from, gboolean copy);

/*functions for reclaiming memory*/
void gel_freetree(GelETree *n);
void gel_emptytree(GelETree *n);

/* Makes a new node and replaces the old one with NULL_NODE */
GelETree * gel_stealnode (GelETree *n);

/* you need to get, then free an evaluation context*/
GelCtx * gel_eval_get_context(void);
void gel_eval_free_context(GelCtx *ctx);
/* evaluate tree*/
GelETree * gel_eval_etree(GelCtx *ctx, GelETree *etree);

/*return TRUE if node is true (a number node !=0, or nonempty string),
  false otherwise*/
int gel_isnodetrue(GelETree *n, int *bad_node);

/*call a function (arguments should have already been evaluated)*/
GelETree * gel_funccall(GelCtx *ctx, GelEFunc *func, GelETree **args, int nargs);
void gel_expandmatrix (GelETree *n);

/* func is a function taking one argument and l is a function/identifier */
/* Note: a copy of the function is made */
GelETree * gel_function_from_function (GelEFunc *func, GelETree *l);

/* Functions to fixup the parsed tree */
GelETree * gel_gather_comparisons(GelETree *n);

/* 0 not found
   1 found OK
   2 found not first */
gboolean gel_get_local_node (GelETree *n, gboolean first_arg,
			     gboolean *local_all, GSList **local_idents);

void gel_replace_equals (GelETree *n, gboolean in_expression);
void gel_replace_exp (GelETree *n);
void gel_fixup_num_neg (GelETree *n);
/* careful precalculation */
void gel_try_to_do_precalc (GelETree *n);
/* aggressive precalculation/simplification */
void gel_simplify (GelETree *n);
/* is the tree semantically the same? */
gboolean gel_is_tree_same (GelETree *l, GelETree *r);

/* find an identifier */
gboolean gel_eval_find_identifier (GelETree *n,
			       GelToken *tok,
			       gboolean funcbody);

char * gel_similar_possible_ids (const char *id);


GSList * gel_get_ids_for_extradict (GSList *toklist, GSList *args, GSList *locals, GelETree *n);

/* return a list of used local functions (copies of).
 * Modifies toklist to remove those already put on funclist */
GSList * gel_subst_local_vars (GSList *funclist, GSList **toklist);

void gel_mod_node (GelCtx *ctx, GelETree *n);
gboolean gel_mod_integer_rational (mpw_t num, mpw_t mod);

mpw_ptr gel_find_pre_function_modulo (GelCtx *ctx);

/* useful in funclib, but really internal */
gboolean _gel_iter_set_velement(GelMatrixW *mat, GelETree *r, GelETree *index);
gboolean _gel_iter_set_element(GelMatrixW *mat, GelETree *r, GelETree *index1, GelETree *index2);

/*compare nodes, return TRUE if equal
  makes them the same type as a side effect*/
gboolean gel_eqlnodes (GelETree *l, GelETree *r);

#define GEL_GET_ABCDE(n,a,b,c,d,e) { \
	(a) = (n)->op.args; \
	(b) = (n)->op.args->any.next; \
	(c) = (n)->op.args->any.next->any.next; \
	(d) = (n)->op.args->any.next->any.next->any.next; \
	(e) = (n)->op.args->any.next->any.next->any.next->any.next; \
}
#define GEL_GET_ABCD(n,a,b,c,d) { \
	(a) = (n)->op.args; \
	(b) = (n)->op.args->any.next; \
	(c) = (n)->op.args->any.next->any.next; \
	(d) = (n)->op.args->any.next->any.next->any.next; \
}
#define GEL_GET_LRR(n,l,r,rr) { \
	(l) = (n)->op.args; \
	(r) = (n)->op.args->any.next; \
	(rr) = (n)->op.args->any.next->any.next; \
}
#define GEL_GET_XRR(n,r,rr) { \
	(r) = (n)->op.args->any.next; \
	(rr) = (n)->op.args->any.next->any.next; \
}
#define GEL_GET_LR(n,l,r) { (l) = (n)->op.args; (r) = (n)->op.args->any.next; }
#define GEL_GET_XR(n,r) { (r) = (n)->op.args->any.next; }
#define GEL_GET_L(n,l) { (l) = (n)->op.args; }

extern GelETree *gel_free_trees;


#ifdef MEM_DEBUG_FRIENDLY

# ifdef EVAL_DEBUG
void register_new_tree (GelETree *n);
void deregister_tree (GelETree *n);
void print_live_trees (void);
void deregister_all_trees (void);
#  define GEL_GET_NEW_NODE(n) {				\
	n = g_new0 (GelETree, 1);			\
	printf ("%s NEW NODE %p\n", G_STRLOC, n);	\
	register_new_tree (n);				\
}
# else /* EVAL_DEBUG */

#  define GEL_GET_NEW_NODE(n) {				\
	n = g_new0 (GelETree, 1);			\
}
# endif /* EVAL_DEBUG */

#else /* MEM_DEBUG_FRIENDLY */

void _gel_make_free_trees (void);
# define GEL_GET_NEW_NODE(n) {				\
	if G_UNLIKELY (gel_free_trees == NULL)		\
		_gel_make_free_trees ();		\
	n = gel_free_trees;				\
	gel_free_trees = gel_free_trees->any.next;	\
}
#endif

extern const GelHookFunc _gel_tree_limit_hook;
extern const GelHookFunc _gel_finished_toplevel_exec_hook;
void gel_test_max_nodes_again (void);

extern GelEFunc *_gel_internal_ln_function;
extern GelEFunc *_gel_internal_exp_function;

#endif /* _GEL_EVAL_H */
