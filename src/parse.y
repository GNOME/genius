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
%{
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

extern GSList *evalstack;
extern GelETree *free_trees;

extern int return_ret; /*should the lexer return on \n*/
extern char *loadfile;
extern char *loadfile_glob;
extern char *load_plugin;

%}

%union {
	mpw_t val;
	char *id;
}

%token STARTTOK

%token LOADFILE LOADFILE_GLOB LOAD_PLUGIN

%token <val> NUMBER
%token <id> STRING
%token <id> FUNCID

%token FUNCTION CALL

%token RETURNTOK BAILOUT EXCEPTION CONTINUE BREAK

%token WHILE UNTIL FOR SUM PROD DO IF THEN ELSE TO BY IN

%token AT REGION_SEP

%token SEPAR NEXTROW EQUALS

%token EQ_CMP NE_CMP CMP_CMP LT_CMP GT_CMP LE_CMP GE_CMP

%token LOGICAL_XOR LOGICAL_OR LOGICAL_AND LOGICAL_NOT


%left NEXTROW

%left SEPAR

%left MOD

%nonassoc LOWER_THEN_ELSE
%nonassoc WHILE UNTIL DO IF FOR SUM PROD TO BY IN THEN ELSE FUNCTION CALL RETURNTOK

%left LOGICAL_XOR LOGICAL_OR
%left LOGICAL_AND
%right LOGICAL_NOT

%right EQUALS

%nonassoc CMP_CMP
%right EQ_CMP NE_CMP LT_CMP GT_CMP LE_CMP GE_CMP

%left '+' '-'
%left '*' '/' '\\' '%'

%right '\''
%right '!'
%right '^'
%right UMINUS UPLUS

%left AT

%%

fullexpr:	STARTTOK expr '\n' { YYACCEPT; }
	|	STARTTOK LOADFILE '\n' { loadfile = $<id>2; YYACCEPT; }
	|	STARTTOK LOADFILE_GLOB '\n' { loadfile_glob = $<id>2; YYACCEPT; }
	|	STARTTOK LOAD_PLUGIN '\n' { load_plugin = $<id>2; YYACCEPT; }
	|	STARTTOK '\n' { YYACCEPT; }
	|	STARTTOK expr SEPAR '\n' { gp_push_null(); PUSH_ACT(E_SEPAR); YYACCEPT; }
	|	error '\n' { return_ret = TRUE; yyclearin; YYABORT; }
	|	error { return_ret = TRUE; }
	;

expr:		expr SEPAR expr		{ PUSH_ACT(E_SEPAR); }
	|	expr MOD expr		{ PUSH_ACT(E_MOD_CALC); }
	|	'(' expr SEPAR ')'	{ gp_push_null(); PUSH_ACT(E_SEPAR);
					  gp_push_spacer(); }
	|	'(' expr ')'		{ gp_push_spacer(); }
	|	expr EQUALS expr	{ PUSH_ACT(E_EQUALS); }
	|	'|' expr '|'		{ PUSH_ACT(E_ABS); }
	|	expr '+' expr		{ PUSH_ACT(E_PLUS); }
	|	expr '-' expr		{ PUSH_ACT(E_MINUS); }
	|	expr '*' expr		{ PUSH_ACT(E_MUL); }
	|	expr '/' expr		{ PUSH_ACT(E_DIV); }
	|	expr '\\' expr		{ PUSH_ACT(E_BACK_DIV); }
	|	expr '%' expr		{ PUSH_ACT(E_MOD); }
	|	expr CMP_CMP expr	{ PUSH_ACT(E_CMP_CMP); }

	|	expr EQ_CMP expr	{ PUSH_ACT(E_EQ_CMP); }
	|	expr NE_CMP expr	{ PUSH_ACT(E_NE_CMP); }
	|	expr LT_CMP expr	{ PUSH_ACT(E_LT_CMP); }
	|	expr GT_CMP expr	{ PUSH_ACT(E_GT_CMP); }
	|	expr LE_CMP expr	{ PUSH_ACT(E_LE_CMP); }
	|	expr GE_CMP expr	{ PUSH_ACT(E_GE_CMP); }

	|	expr LOGICAL_AND expr	{ PUSH_ACT(E_LOGICAL_AND); }
	|	expr LOGICAL_OR expr	{ PUSH_ACT(E_LOGICAL_OR); }
	|	expr LOGICAL_XOR expr	{ PUSH_ACT(E_LOGICAL_XOR); }
	|	LOGICAL_NOT expr	{ PUSH_ACT(E_LOGICAL_NOT); }
	
	/*|	expr IN expr		{ PUSH_ACT(E_EXISTS_IN); }
	|	expr LOGICAL_NOT IN expr	{ PUSH_ACT(E_NOT_EXISTS_IN); }*/

	|	expr '!'		{ PUSH_ACT(E_FACT); }
	|	expr '\''		{ PUSH_ACT(E_TRANSPOSE); }
	|	'-' expr %prec UMINUS	{ PUSH_ACT(E_NEG); }
	|	'+' expr %prec UPLUS
	| 	expr '^' expr		{ PUSH_ACT(E_EXP); }
	
	|	expr AT expr ')'	{ PUSH_ACT(E_GET_VELEMENT); }
	|	expr AT expr ',' expr ')' { PUSH_ACT(E_GET_ELEMENT); }
	|	expr AT reg ',' expr ')' { PUSH_ACT(E_GET_REGION); }
	|	expr AT expr ',' reg ')' { PUSH_ACT(E_GET_REGION); }
	|	expr AT reg ',' reg ')' { PUSH_ACT(E_GET_REGION); }
	|	expr AT expr ',' ')'	{ PUSH_ACT(E_GET_ROW_REGION); }
	|	expr AT reg ',' ')'	{ PUSH_ACT(E_GET_ROW_REGION); }
	|	expr AT ',' expr ')'	{ PUSH_ACT(E_GET_COL_REGION); }
	|	expr AT ',' reg ')'	{ PUSH_ACT(E_GET_COL_REGION); }
	|	'[' matrixrows ']'	{ if(!gp_push_matrix(FALSE)) {SYNTAX_ERROR;} }
	|	'`' '[' matrixrows ']'	{ if(!gp_push_matrix(TRUE)) {SYNTAX_ERROR;} }
	/*This next rule DOESN'T work right, we need some sort of connection
	  to the lexer, OR write our own lexer/parser and kill this bison
	  crap
	|	LT_CMP exprlist GT_CMP 	{SYNTAX_ERROR;}
	*/
	/*FIXME: vector */
	|	'(' exprlist ',' expr ')'	{
			if(!gp_push_matrix_row()) {SYNTAX_ERROR;}
			if(!gp_push_marker(MATRIX_START_NODE)) {SYNTAX_ERROR;}
			if(!gp_push_matrix(TRUE)) {SYNTAX_ERROR;}
					}
	|	'{' exprlist '}'	{SYNTAX_ERROR;}/*FIXME: set*/
	|	'`' '{' exprlist '}'	{SYNTAX_ERROR;}/*FIXME: nonordered multiset*/
	|	WHILE expr DO expr	{ PUSH_ACT(E_WHILE_CONS); }
	|	UNTIL expr DO expr	{ PUSH_ACT(E_UNTIL_CONS); }
	|	DO expr WHILE expr	{ PUSH_ACT(E_DOWHILE_CONS); }
	|	DO expr UNTIL expr	{ PUSH_ACT(E_DOUNTIL_CONS); }
	|	FOR ident EQUALS expr TO expr DO expr { PUSH_ACT(E_FOR_CONS); }
	|	FOR ident EQUALS expr TO expr BY expr DO expr { PUSH_ACT(E_FORBY_CONS); }
	|	FOR ident IN expr DO expr { PUSH_ACT(E_FORIN_CONS); }
	|	SUM ident EQUALS expr TO expr DO expr { PUSH_ACT(E_SUM_CONS); }
	|	SUM ident EQUALS expr TO expr BY expr DO expr { PUSH_ACT(E_SUMBY_CONS); }
	|	SUM ident IN expr DO expr { PUSH_ACT(E_SUMIN_CONS); }
	|	PROD ident EQUALS expr TO expr DO expr { PUSH_ACT(E_PROD_CONS); }
	|	PROD ident EQUALS expr TO expr BY expr DO expr { PUSH_ACT(E_PRODBY_CONS); }
	|	PROD ident IN expr DO expr { PUSH_ACT(E_PRODIN_CONS); }
	|	IF expr THEN expr %prec LOWER_THEN_ELSE	{ PUSH_ACT(E_IF_CONS); }
	|	IF expr THEN expr ELSE expr { PUSH_ACT(E_IFELSE_CONS); }
	|	ident
	|	'`' ident		{ PUSH_ACT(E_QUOTE); }
	|	'&' ident		{ PUSH_ACT(E_REFERENCE); }
	|	deref
	|	ident '(' ')'		{ gp_push_marker_simple(EXPRLIST_START_NODE);
					  PUSH_ACT(E_DIRECTCALL); }
	|	ident '(' exprlist ')'	{ PUSH_ACT(E_DIRECTCALL); }
	|	deref '(' ')'		{ gp_push_marker_simple(EXPRLIST_START_NODE);
					  PUSH_ACT(E_DIRECTCALL); }
	|	deref '(' exprlist ')'	{ PUSH_ACT(E_DIRECTCALL); }
	|	expr CALL '(' exprlist ')' { PUSH_ACT(E_CALL); }
	|	expr CALL '(' ')'	{ gp_push_marker_simple(EXPRLIST_START_NODE);
					  PUSH_ACT(E_CALL); }
	|	FUNCTION ident funcdef	{ PUSH_ACT(E_EQUALS); }
	|	FUNCTION funcdef
	|	'`' funcdef
	|	RETURNTOK expr		{ PUSH_ACT(E_RETURN); }
	|	BAILOUT			{ PUSH_ACT(E_BAILOUT); }
	|	EXCEPTION		{ PUSH_ACT(E_EXCEPTION); }
	|	CONTINUE		{ PUSH_ACT(E_CONTINUE); }
	|	BREAK			{ PUSH_ACT(E_BREAK); }
	|	NUMBER			{ stack_push(&evalstack,
						     gel_makenum_use($<val>1)); }
	|	STRING			{ PUSH_STRING($<id>1); }
	|	'.'			{ gp_push_null(); }
	;
	
deref:		'*' ident		{ PUSH_ACT(E_DEREFERENCE); }
	;
	
ident:		FUNCID			{
				PUSH_IDENTIFIER($<id>1);
				g_free($<id>1);
					}
	;
	
funcdef:	'(' identlist')' EQUALS expr { if(!gp_push_func()) {SYNTAX_ERROR;} }
	|	'(' ')' EQUALS expr	{ if(!gp_push_marker(EXPRLIST_START_NODE))
						{SYNTAX_ERROR;}
					  if(!gp_push_func())
					  	{SYNTAX_ERROR;} }
	;
	
identlist:	identlist ',' ident
	|	ident { if(!gp_push_marker(EXPRLIST_START_NODE)) {SYNTAX_ERROR;} }
	;

exprlist:	exprlist ',' expr
	|	expr { if(!gp_push_marker(EXPRLIST_START_NODE)) {SYNTAX_ERROR;} }
	;
	
matrixrows:	matrixrows NEXTROW exprlist { if(!gp_push_matrix_row()) {SYNTAX_ERROR;} }
	|	exprlist { if(!gp_push_matrix_row()) {SYNTAX_ERROR;} if(!gp_push_marker(MATRIX_START_NODE)) {SYNTAX_ERROR;} }
	;
	
reg:		expr REGION_SEP expr	{ PUSH_ACT(E_REGION_SEP); }
	;
	
%%
