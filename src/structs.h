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

#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include <stdio.h>
#include "mpwrap.h"

/*dictionary function structure*/
typedef enum {
	GEL_BUILTIN_FUNC = 0, /*function internal to genius*/
	GEL_USER_FUNC, /*function that points to an GelETree for evaluation*/
	GEL_VARIABLE_FUNC, /*function that points to an GelETree result */
	GEL_REFERENCE_FUNC /*a function that points to some other GelEFunc*/
} GelEFuncType;

typedef struct _GelEFunc GelEFunc;
typedef union _GelETree GelETree;
typedef struct _GelETreeAny GelETreeAny;
/*typedef struct _GelETreeNull GelETreeNull;*/
typedef struct _GelETreeValue GelETreeValue;
typedef struct _GelETreeMatrix GelETreeMatrix;
typedef struct _GelETreeSet GelETreeSet;
typedef struct _GelETreeOperator GelETreeOperator;
typedef struct _GelETreeIdentifier GelETreeIdentifier;
typedef struct _GelETreeString GelETreeString;
typedef struct _GelETreeFunction GelETreeFunction;
typedef struct _GelETreeComparison GelETreeComparison;
typedef struct _GelETreeUsertype GelETreeUsertype;
typedef struct _GelETreeMatrixRow GelETreeMatrixRow;
/*typedef struct _GelETreeMatrixStart GelETreeMatrixStart;*/
/*typedef struct _GelETreeExprlistStart GelETreeExprlistStart;*/
typedef struct _GelETreeSpacer GelETreeSpacer;
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

/* tokens point to this structure globaly, there is
   one such structure for each token.  */
struct _GelToken {
	char *token;
	GelEFunc *curref;
	GSList *refs;

	guint32 protected:1;
};

struct _GelEFunc {
	GelToken *id;
	GelEFuncType type;
	int context; /*the context number this is used for .. if we pop this
		       context, we will destroy the function*/
	int nargs; /*number of arguments*/
	GSList *named_args; /*names of arguments*/
	union {
		GelETree *user;
		GelETree *(*func)(GelCtx *ctx, GelETree * *, int *); /*the integer is exception*/
		GelEFunc *ref;
		GelEFunc *next; /*this is for keeping a free list*/
	} data;
};

typedef enum {
	NULL_NODE=0,
	VALUE_NODE,
	MATRIX_NODE,
	SET_NODE,
	OPERATOR_NODE,
	IDENTIFIER_NODE,
	STRING_NODE,
	FUNCTION_NODE, /*stores an anonymous function*/
	COMPARISON_NODE,
	USERTYPE_NODE, /*for user types*/
	
	/*marker nodes*/
	MATRIX_ROW_NODE=1000,
	MATRIX_START_NODE,
	EXPRLIST_START_NODE,
	SPACER_NODE
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
	guint quoted:1;
};

struct _GelETreeSet {
	GelETreeType type;
	GelETree *next;
	GelETree *items;
	guint multiset:1;
};

struct _GelETreeOperator {
	GelETreeType type;
	GelETree *next;
	int oper;
	int nargs;
	GelETree *args;
};

struct _GelETreeIdentifier {
	GelETreeType type;
	GelETree *next;
	GelToken *id;
};

struct _GelETreeString {
	GelETreeType type;
	GelETree *next;
	char *str;
};

struct _GelETreeFunction {
	GelETreeType type;
	GelETree *next;
	GelEFunc *func; /*anon function*/
};

struct _GelETreeComparison {
	GelETreeType type;
	GelETree *next;
	int nargs;
	GelETree *args;
	GSList *comp;
};

struct _GelETreeUsertype {
	GelETreeType type;
	GelETree *next;
	int ttype;
	gpointer data;
};

struct _GelETreeMatrixRow {
	GelETreeType type;
	GelETree *next;
	int nargs;
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

union _GelETree {
	GelETreeType type;
	GelETreeAny any; /*for allocation purposes only*/
	/*GelETreeNull null;*/
	GelETreeValue val;
	GelETreeMatrix mat;
	GelETreeSet set;
	GelETreeOperator op;
	GelETreeIdentifier id;
	GelETreeString str;
	GelETreeFunction func;
	GelETreeComparison comp;
	GelETreeUsertype ut;
	GelETreeMatrixRow row;
	/*GelETreeMatrixStart mats;*/
	/*GelETreeExprlistStart exps;*/
	GelETreeSpacer sp;
};

/* Evaluation stack */

/* The flag for the stack */
enum {
	GE_EMPTY_STACK,
	GE_RESULT, /*used for recursive evluation with the same ctx*/
	GE_PRE,
	GE_POST,
	GE_FUNCCALL,
	GE_AND,
	GE_OR,
	GE_LOOP_LOOP,
	GE_LOOP_COND,
	GE_FOR,
	GE_FORIN,
};

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
	guint32 is_while:1; /*if false, this is an until loop*/
	guint32 body_first:1; /*if true body is the first argument*/
};
/*data structure for 'for' loops*/
struct _GelEvalFor {
	GelEvalForType type;
	mpw_ptr x;
	mpw_ptr by;
	mpw_ptr to;
	int init_cmp;
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
	int post;
	GelETree *current;
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
	
	gpointer data;
};

#endif
