/* GENIUS Calculator
 * Copyright (C) 1997-2012 Jiri (George) Lebl
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

#ifndef _GEL_STRUCTS_H
#define _GEL_STRUCTS_H

#include <stdio.h>
#include "mpwrap.h"

/*dictionary function structure*/
typedef enum {
	GEL_BUILTIN_FUNC = 0, /*function internal to genius*/
	GEL_USER_FUNC, /*function that points to an GelETree for evaluation*/
	GEL_VARIABLE_FUNC, /*function that points to an GelETree result */
	GEL_REFERENCE_FUNC /*a function that points to some other GelEFunc*/
} GelEFuncType; /* should fit in 3 bits (it does) */

typedef struct _GelEFunc GelEFunc;
typedef union _GelETree GelETree;
typedef struct _GelETreeAny GelETreeAny;
/*typedef struct _GelETreeNull GelETreeNull;*/
typedef struct _GelETreeValue GelETreeValue;
typedef struct _GelETreeMatrix GelETreeMatrix;
typedef struct _GelETreeSet GelETreeSet;
typedef struct _GelETreePolynomial GelETreePolynomial;
typedef struct _GelETreeOperator GelETreeOperator;
typedef struct _GelETreeIdentifier GelETreeIdentifier;
typedef struct _GelETreeString GelETreeString;
typedef struct _GelETreeFunction GelETreeFunction;
typedef struct _GelETreeComparison GelETreeComparison;
typedef struct _GelETreeUsertype GelETreeUsertype;
typedef struct _GelETreeBool GelETreeBool;

typedef struct _GelETreeMatrixRow GelETreeMatrixRow;
/*typedef struct _GelETreeMatrixStart GelETreeMatrixStart;*/
/*typedef struct _GelETreeExprlistStart GelETreeExprlistStart;*/
typedef struct _GelETreeSpacer GelETreeSpacer;
typedef struct _GelETreeLocal GelETreeLocal;
typedef struct _GelToken GelToken;

typedef struct _GelEvalStack GelEvalStack;
typedef struct _GelEvalLoop GelEvalLoop;
typedef struct _GelEvalFor GelEvalFor;
typedef struct _GelEvalForIn GelEvalForIn;
typedef struct _GelCtx GelCtx;

typedef struct _GelExecSetup GelExecSetup;
typedef struct _GelOutput GelOutput;

/*not defined here, but needed and we can't include matrixw.h, but
  matrixw.h includes structs.h*/
typedef struct _GelMatrixW GelMatrixW;

typedef GelETree *(* ParameterSetFunc) (GelETree *val);
typedef GelETree *(* ParameterGetFunc) (void);

typedef	GelETree *(* GelBIFunction) (GelCtx *ctx, GelETree * *, gboolean *); /*the gboolean is exception*/


/* tokens point to this structure globaly, there is
   one such structure for each token.  */
struct _GelToken {
	char *token;
	GSList *refs;

	/* For built-in parameters this is the get and set function
	 * of type ParameterGetFunc and ParameterSetFunc. */
	gpointer data1;
	gpointer data2;

	char *uncompiled;

	guint8 protected_:1;
	guint8 parameter:1;
	guint8 built_in_parameter:1;
};

struct _GelEFunc {
	GelToken *id;
	GelToken *symbolic_id; /* id for symbolic math, preserved under
				  assignment, if NULL we use the id */
	int context; /*the context number this is used for .. if we pop this
		       context, we will destroy the function*/
	GSList *named_args; /*names of arguments*/

	GSList *extra_dict;

	GSList *local_idents;
	GSList *subst_dict;

	union {
		GelETree *user;
		GelBIFunction func;
		GelEFunc *ref;
		GelEFunc *next; /*this is for keeping a free list*/
	} data;

	guint32 nargs:16; /*number of arguments*/

	/* GelEFuncType type; */
	guint32 type:3;

	/* if true, we must take this off the subst list for a context pop,
	 * before we free the function */
	guint32 on_subst_list:1;

	/* never put on a substlist, have a set extradict */
	guint32 never_on_subst_list:1;

	guint32 vararg:1;
	guint32 propagate_mod:1;
	guint32 no_mod_all_args:1;

	/* this is true if all local vars of this function
	   will be local-scoped */
	guint32 local_all:1;

	/* is this variable local */
	guint32 is_local:1;

	/* did we already build the subst_list */
	guint32 built_subst_dict:1;

	/* NOTE: Make sure! to update d_setrealfunc and others */
};

typedef enum {
	GEL_NULL_NODE=0,
	GEL_VALUE_NODE,
	GEL_MATRIX_NODE,
	GEL_SET_NODE, /* FIXME: Not implemented */
	GEL_POLYNOMIAL_NODE, /* FIXME: Not implemented */
	GEL_OPERATOR_NODE,
	GEL_IDENTIFIER_NODE,
	GEL_STRING_NODE,
	GEL_FUNCTION_NODE, /*stores an anonymous function*/
	GEL_COMPARISON_NODE,
	GEL_USERTYPE_NODE, /*for user types, FIXME: not finished*/
	GEL_BOOL_NODE, /*boolean*/
	
	/*marker nodes*/
	GEL_MATRIX_ROW_NODE=1000,
	GEL_MATRIX_START_NODE,
	GEL_EXPRLIST_START_NODE,
	GEL_SPACER_NODE,
	GEL_LOCAL_NODE
} GelETreeType;

struct _GelETreeAny {
	GelETreeType type;
	GelETree *next;
};

/*struct _GelETreeNull {
	GelETreeType type;
	GelETree *next;
};*/

struct _GelETreeValue {
	GelETreeType type;
	GelETree *next;
	mpw_t value;
};

struct _GelETreeMatrix {
	GelETreeType type;
	GelETree *next;
	GelMatrixW *matrix;
	gboolean quoted;
	
	/* gboolean is faster, then a bitfield and we right now
	   don't gain anything */
	/*
	guint quoted:1;
	*/
};

/* FIXME: Not implemented */
struct _GelETreeSet {
	GelETreeType type;
	GelETree *next;
	GelETree *items;
	gboolean multiset;

	/* gboolean is faster, then a bitfield and we right now
	   don't gain anything */
	/*
	guint multiset:1;
	*/
};

struct _GelETreePolynomial {
	GelETreeType type;
	GelETree *next;
	guint16 largest; /* largest exponent */
	guint8 vars; /* number of variables */

	/* This needs redoing.  No need to store
	   this in the ETree struct since we want to
	   conserve memory.  Probably need an array
	   type for this. */
	/*
	guint16 arraysize;
	*/

	mpw_ptr *indexes; /* indexes when written out in standard form
			     from smallest to largest.  If more then one
			     variable then this is '(largest+1)^vars'
			     size array */
};

struct _GelETreeOperator {
	GelETreeType type;
	GelETree *next;
	gint8 oper;
	guint16 nargs;
	GelETree *args;
};

struct _GelETreeIdentifier {
	GelETreeType type;
	GelETree *next;
	GelToken *id;
	gboolean uninitialized;
	
	/* gboolean is faster, then a bitfield and we right now
	   don't gain anything */
	/*
	guint uninitialized:1;
	*/
};

struct _GelETreeString {
	GelETreeType type;
	GelETree *next;
	char *str;

	gboolean constant;
	/* gboolean is faster, then a bitfield and we right now
	   don't gain anything */
	/* guint constant:1; */
};

struct _GelETreeFunction {
	GelETreeType type;
	GelETree *next;
	GelEFunc *func; /*anon function*/
};

struct _GelETreeComparison {
	GelETreeType type;
	GelETree *next;
	gint16 nargs;
	GelETree *args;
	GSList *comp;
};

struct _GelETreeUsertype {
	GelETreeType type;
	GelETree *next;
	gint16 ttype;
	gpointer data;
};

struct _GelETreeBool {
	GelETreeType type;
	GelETree *next;
	gboolean bool_;

	/* gboolean is faster, then a bitfield and we right now
	   don't gain anything */
	/* guint bool_:1; */
};


struct _GelETreeMatrixRow {
	GelETreeType type;
	GelETree *next;
	guint32 nargs;
	GelETree *args;
};

/*struct _GelETreeMatrixStart {
	GelETreeType type;
	GelETree *next;
};*/

/*struct _GelETreeExprlistStart {
	GelETreeType type;
	GelETree *next;
};*/

struct _GelETreeSpacer {
	GelETreeType type;
	GelETree *next;
	GelETree *arg;
};

struct _GelETreeLocal {
	GelETreeType type;
	GelETree *next;
	GSList *idents; /* NULL means all */
	GelETree *arg;
};

union _GelETree {
	GelETreeType type;
	GelETreeAny any; /*for allocation purposes only*/
	/*GelETreeNull null;*/
	GelETreeValue val;
	GelETreeMatrix mat;
	GelETreeSet set;
	GelETreePolynomial poly;
	GelETreeOperator op;
	GelETreeIdentifier id;
	GelETreeString str;
	GelETreeFunction func;
	GelETreeComparison comp;
	GelETreeUsertype ut;
	GelETreeBool bool_;
	GelETreeMatrixRow row;
	/*GelETreeMatrixStart mats;*/
	/*GelETreeExprlistStart exps;*/
	GelETreeSpacer sp;
	GelETreeLocal loc;
};

/* builtin primitives (operator node type) */
enum {
	GEL_E_SEPAR = 0,
	GEL_E_EQUALS, /* see E_DEFEQUALS (on the end not to break bincompat) */
	GEL_E_PARAMETER,
	GEL_E_ABS,
	GEL_E_PLUS,
	GEL_E_ELTPLUS,
	GEL_E_MINUS,
	GEL_E_ELTMINUS,
	GEL_E_MUL,
	GEL_E_ELTMUL,
	GEL_E_DIV,
	GEL_E_ELTDIV,
	GEL_E_BACK_DIV,
	GEL_E_ELT_BACK_DIV,
	GEL_E_MOD,
	GEL_E_ELTMOD,
	GEL_E_NEG,
	GEL_E_EXP,
	GEL_E_ELTEXP,
	GEL_E_FACT,
	GEL_E_DBLFACT,
	GEL_E_TRANSPOSE,
	GEL_E_CONJUGATE_TRANSPOSE,
	GEL_E_IF_CONS,
	GEL_E_IFELSE_CONS,
	GEL_E_WHILE_CONS,
	GEL_E_UNTIL_CONS,
	GEL_E_DOWHILE_CONS,
	GEL_E_DOUNTIL_CONS,
	GEL_E_FOR_CONS,
	GEL_E_FORBY_CONS,
	GEL_E_FORIN_CONS,
	GEL_E_SUM_CONS,
	GEL_E_SUMBY_CONS,
	GEL_E_SUMIN_CONS,
	GEL_E_PROD_CONS,
	GEL_E_PRODBY_CONS,
	GEL_E_PRODIN_CONS,
	GEL_E_EQ_CMP,
	GEL_E_NE_CMP,
	GEL_E_CMP_CMP,
	GEL_E_LT_CMP,
	GEL_E_GT_CMP,
	GEL_E_LE_CMP,
	GEL_E_GE_CMP,
	GEL_E_LOGICAL_AND,
	GEL_E_LOGICAL_OR,
	GEL_E_LOGICAL_XOR,
	GEL_E_LOGICAL_NOT,
	GEL_E_REGION_SEP,
	GEL_E_REGION_SEP_BY,
	GEL_E_GET_VELEMENT,
	GEL_E_GET_ELEMENT,
	GEL_E_GET_ROW_REGION,
	GEL_E_GET_COL_REGION,
	GEL_E_QUOTE,
	GEL_E_REFERENCE,
	GEL_E_DEREFERENCE,
	GEL_E_DIRECTCALL,
	GEL_E_CALL,
	GEL_E_RETURN,
	GEL_E_BAILOUT,
	GEL_E_EXCEPTION,
	GEL_E_CONTINUE,
	GEL_E_BREAK,
	GEL_E_MOD_CALC,
	GEL_E_DEFEQUALS,
	GEL_E_SWAPWITH,
	GEL_E_INCREMENT,
	GEL_E_INCREMENT_BY,
	GEL_E_OPER_LAST
};

/* Evaluation stack */

/* The flag for the stack */
enum {
	GE_EMPTY_STACK = 0,
	GE_RESULT      = 1, /*used for recursive evluation with the same ctx*/
	GE_PRE         = 2,
	GE_POST        = 3,
	GE_FUNCCALL    = 4,
	GE_AND         = 5,
	GE_OR          = 6,
	GE_LOOP_LOOP   = 7,
	GE_LOOP_COND   = 8,
	GE_FOR         = 9,
	GE_FORIN       = 10,
	GE_MODULOOP    = 11,
	GE_SETMODULO   = 12,

	/* flag mask */
	GE_MASK        = 0xff,

	/* bool flags */
	GE_WHACKARG    = 1<<16 /* only on GE_PRE, GE_POST, GE_MODULOOP */
};

#define GE_ADDWHACKARG(flag,whackarg) \
	((whackarg) ? ((flag) | GE_WHACKARG) : (flag))

/*should take up about a page
  we will use a single pointer for data and the next pointer for post/pre flag
  we can use next as a marker that we have gone too far, and current as a marker
  that we have underpopped*/
#define STACK_SIZE 1022
struct _GelEvalStack {
	gpointer stack[STACK_SIZE];
	GelEvalStack *next;
};

typedef enum {
	GEL_EVAL_FOR,
	GEL_EVAL_SUM,
	GEL_EVAL_PROD
} GelEvalForType;

/*data structure for while/until loops*/
struct _GelEvalLoop {
	GelETree * condition;
	GelETree * body;
	guint8 is_while:1; /*if false, this is an until loop*/
	guint8 body_first:1; /*if true body is the first argument*/
};
/*data structure for 'for' loops*/
struct _GelEvalFor {
	GelEvalForType type;
	mpw_ptr x;
	mpw_ptr by;
	mpw_ptr to;
	gint8 init_cmp;
	GelETree * result;
	GelETree * body;
	GelETree * orig_body;
	GelToken * id;
};

/*data structure for 'forin' loops*/
struct _GelEvalForIn {
	GelEvalForType type;
	int i,j;
	GelMatrixW * mat;
	GelETree * result;
	GelETree * body;
	GelETree * orig_body;
	GelToken * id;
};

/* evaluation context structure */
struct _GelCtx {
	GelEvalStack *stack;
	gpointer *topstack;
	GelETree *res;
	gboolean post;
	gboolean whackarg;
	GelETree *current;
	mpw_ptr modulo;
};

enum {
	GEL_OUTPUT_FILE,
	GEL_OUTPUT_STRING,
	GEL_OUTPUT_BLACK_HOLE
};

typedef int (*GelOutputLineFunc)(GelOutput *);
typedef void (*GelOutputNotifyFunc)(GelOutput *);

struct _GelOutput {
	int ref_count;

	int output_type;

	gboolean length_limit; /* if TRUE limit by below, else don't */
	int line_length; /* limit the output to this number of characters,
			    if <=0 then always print normally */
	GelOutputLineFunc line_length_get;
			 /* if this is set, this function is used instead of
			    above */
	int cur_line_pos; /* position on current line */
	gboolean inside_escape; /* inside a shell escape, don't increment the
				   above */

	FILE *outfp;
	GString *outs;

	/* new data notifier */
	GelOutputNotifyFunc notify;
	int no_notify;
	
	gpointer data;

	int force_chop;
};

typedef void (*GelHookFunc) (void);

#endif /* _GEL_STRUCTS_H */
