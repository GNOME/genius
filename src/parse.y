/* GENIUS Calculator
 * Copyright (C) 1997-2011 Jiri (George) Lebl
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

extern GSList *gel_parsestack;

extern gboolean gel_return_ret; /*should the lexer return on \n*/

/* prototype for yylex */
int yylex(void);
void yyerror(const char *);

%}

%union {
	mpw_t val;
	char *id;
}

%token STARTTOK

%token LOADFILE LOADFILE_GLOB LOAD_PLUGIN CHANGEDIR PWD LS LS_ARG HELP HELP_ARG

%token <val> NUMBER
%token <id> STRING
%token <id> FUNCID

%token FUNCTION CALL THREEDOTS PARAMETER

%token RETURNTOK BAILOUT EXCEPTION CONTINUE BREAK LOCAL

%token WHILE UNTIL FOR SUM PROD DO IF THEN ELSE TO BY IN

%token AT MAKEIMAGPARENTH

%token SEPAR NEXTROW EQUALS DEFEQUALS SWAPWITH

%token TRANSPOSE

%token ELTELTDIV ELTELTMUL ELTELTPLUS ELTELTMINUS ELTELTEXP ELTELTMOD DOUBLEFACT

%token EQ_CMP NE_CMP CMP_CMP LT_CMP GT_CMP LE_CMP GE_CMP

%token LOGICAL_XOR LOGICAL_OR LOGICAL_AND LOGICAL_NOT

%token INCREMENT

%left NEXTROW

%left SEPAR


%nonassoc FUNCTION PARAMETER

%nonassoc LOWER_THAN_ELSE
%nonassoc LOWER_THAN_INCREMENT
%nonassoc INCREMENT
%nonassoc WHILE UNTIL DO IF FOR SUM PROD TO BY IN THEN ELSE RETURNTOK LOCAL

%left LOGICAL_XOR LOGICAL_OR
%left LOGICAL_AND
%right LOGICAL_NOT

%left MOD

%right EQUALS DEFEQUALS 
%nonassoc SWAPWITH

%nonassoc CMP_CMP
%right EQ_CMP NE_CMP LT_CMP GT_CMP LE_CMP GE_CMP

%nonassoc CALL

%right ':'

%left '+' ELTELTPLUS '-' ELTELTMINUS
%left '*' ELTELTMUL '/' ELTELTDIV '\\' ELTELTBACKDIV '%' ELTELTMOD

%right '\'' TRANSPOSE

%right UMINUS UPLUS
%right '^' ELTELTEXP
%right '!' DOUBLEFACT

%left AT

%%

fullexpr:	STARTTOK expr '\n' { YYACCEPT; }
	|	STARTTOK LOADFILE '\n' { gel_command = GEL_LOADFILE; gel_command_arg = $<id>2; YYACCEPT; }
	|	STARTTOK LOADFILE_GLOB '\n' { gel_command = GEL_LOADFILE_GLOB; gel_command_arg = $<id>2; YYACCEPT; }
	|	STARTTOK CHANGEDIR '\n' { gel_command = GEL_CHANGEDIR; gel_command_arg = $<id>2; YYACCEPT; }
	|	STARTTOK LS '\n' { gel_command = GEL_LS; YYACCEPT; }
	|	STARTTOK LS_ARG '\n' { gel_command = GEL_LS_ARG; gel_command_arg = $<id>2; YYACCEPT; }
	|	STARTTOK HELP '\n' { gel_command = GEL_HELP; YYACCEPT; }
	|	STARTTOK HELP_ARG '\n' { gel_command = GEL_HELP_ARG; gel_command_arg = $<id>2; YYACCEPT; }
	|	STARTTOK PWD '\n' { gel_command = GEL_PWD; YYACCEPT; }
	|	STARTTOK LOAD_PLUGIN '\n' { gel_command = GEL_LOADPLUGIN; gel_command_arg = $<id>2; YYACCEPT; }
	|	STARTTOK '\n' { YYACCEPT; }
	|	STARTTOK expr SEPAR '\n' { gp_push_null(); PUSH_ACT(GEL_E_SEPAR); YYACCEPT; }
	|	error '\n' { gel_return_ret = TRUE; yyclearin; YYABORT; }
	|	error { gel_return_ret = TRUE; }
	;

expr:		expr SEPAR expr		{ PUSH_ACT(GEL_E_SEPAR); }
	|	LOCAL '*' SEPAR expr	{ if ( ! gp_push_local_all ()) {
						SYNTAX_ERROR;
					  }
       					}
	|	LOCAL identlist SEPAR expr { if ( ! gp_push_local_idents ()) {
						SYNTAX_ERROR;
					  }
					}
	|	expr MOD expr		{ PUSH_ACT(GEL_E_MOD_CALC); }
	|	'(' expr SEPAR ')'	{ gp_push_null(); PUSH_ACT(GEL_E_SEPAR);
					  gp_push_spacer(); }
	|	'(' expr ')'		{ gp_push_spacer(); }
	|	'(' expr MAKEIMAGPARENTH { mpw_t i;
					  mpw_init (i);
					  mpw_i (i);
					  gp_push_spacer();
					  gel_stack_push(&gel_parsestack,
							 gel_makenum_use(i));
					  PUSH_ACT(GEL_E_MUL); }
	|	expr EQUALS expr	{ PUSH_ACT(GEL_E_EQUALS); }
	|	expr DEFEQUALS expr	{ PUSH_ACT(GEL_E_DEFEQUALS); }
	|	INCREMENT expr %prec LOWER_THAN_INCREMENT { PUSH_ACT(GEL_E_INCREMENT); }
	|	INCREMENT expr BY expr %prec INCREMENT { PUSH_ACT(GEL_E_INCREMENT_BY); }
	|	expr SWAPWITH expr	{ PUSH_ACT(GEL_E_SWAPWITH); }
	|	'|' expr '|'		{ PUSH_ACT(GEL_E_ABS); }
	|	expr '+' expr		{ PUSH_ACT(GEL_E_PLUS); }
	|	expr ELTELTPLUS expr	{ PUSH_ACT(GEL_E_ELTPLUS); }
	|	expr '-' expr		{ PUSH_ACT(GEL_E_MINUS); }
	|	expr ELTELTMINUS expr	{ PUSH_ACT(GEL_E_ELTMINUS); }
	|	expr '*' expr		{ PUSH_ACT(GEL_E_MUL); }
	|	expr ELTELTMUL expr	{ PUSH_ACT(GEL_E_ELTMUL); }
	|	expr '/' expr		{ PUSH_ACT(GEL_E_DIV); }
	|	expr ELTELTDIV expr	{ PUSH_ACT(GEL_E_ELTDIV); }
	|	expr '\\' expr		{ PUSH_ACT(GEL_E_BACK_DIV); }
	|	expr ELTELTBACKDIV expr	{ PUSH_ACT(GEL_E_ELT_BACK_DIV); }
	|	expr '%' expr		{ PUSH_ACT(GEL_E_MOD); }
	|	expr ELTELTMOD expr	{ PUSH_ACT(GEL_E_ELTMOD); }
	|	expr CMP_CMP expr	{ PUSH_ACT(GEL_E_CMP_CMP); }

	|	expr EQ_CMP expr	{ PUSH_ACT(GEL_E_EQ_CMP); }
	|	expr NE_CMP expr	{ PUSH_ACT(GEL_E_NE_CMP); }
	|	expr LT_CMP expr	{ PUSH_ACT(GEL_E_LT_CMP); }
	|	expr GT_CMP expr	{ PUSH_ACT(GEL_E_GT_CMP); }
	|	expr LE_CMP expr	{ PUSH_ACT(GEL_E_LE_CMP); }
	|	expr GE_CMP expr	{ PUSH_ACT(GEL_E_GE_CMP); }

	|	expr LOGICAL_AND expr	{ PUSH_ACT(GEL_E_LOGICAL_AND); }
	|	expr LOGICAL_OR expr	{ PUSH_ACT(GEL_E_LOGICAL_OR); }
	|	expr LOGICAL_XOR expr	{ PUSH_ACT(GEL_E_LOGICAL_XOR); }
	|	LOGICAL_NOT expr	{ PUSH_ACT(GEL_E_LOGICAL_NOT); }
	
	/*|	expr IN expr		{ PUSH_ACT(GEL_E_EXISTS_IN); }
	|	expr LOGICAL_NOT IN expr	{ PUSH_ACT(GEL_E_NOT_EXISTS_IN); }*/

	|	expr '!'		{ PUSH_ACT(GEL_E_FACT); }
	|	expr DOUBLEFACT		{ PUSH_ACT(GEL_E_DBLFACT); }
	|	expr '\''		{ PUSH_ACT(GEL_E_CONJUGATE_TRANSPOSE); }
	|	expr TRANSPOSE		{ PUSH_ACT(GEL_E_TRANSPOSE); }
	|	'-' expr %prec UMINUS	{ PUSH_ACT(GEL_E_NEG); }
	|	'+' expr %prec UPLUS
	| 	expr '^' expr		{ PUSH_ACT(GEL_E_EXP); }
	| 	expr ELTELTEXP expr	{ PUSH_ACT(GEL_E_ELTEXP); }

	|	expr ':' expr	{
				if (gp_prepare_push_region_sep ()) {
					PUSH_ACT(GEL_E_REGION_SEP_BY);
				} else {
					PUSH_ACT(GEL_E_REGION_SEP);
				}
					}
	
	|	expr AT ':' ')'		{ /* FIXME: do nothing?, this is just a 
					     get all */ }
	|	expr AT expr ')'	{ PUSH_ACT(GEL_E_GET_VELEMENT); }
	|	expr AT expr ',' expr ')' { PUSH_ACT(GEL_E_GET_ELEMENT); }
	|	expr AT expr ',' ')'	{ PUSH_ACT(GEL_E_GET_ROW_REGION); }
	|	expr AT expr ',' ':' ')'	{ PUSH_ACT(GEL_E_GET_ROW_REGION); }
	|	expr AT ',' expr ')'	{ PUSH_ACT(GEL_E_GET_COL_REGION); }
	|	expr AT ':' ',' expr ')'	{ PUSH_ACT(GEL_E_GET_COL_REGION); }
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
			if(!gp_push_marker(GEL_MATRIX_START_NODE)) {SYNTAX_ERROR;}
			if(!gp_push_matrix(TRUE)) {SYNTAX_ERROR;}
					}
	|	'{' exprlist '}'	{SYNTAX_ERROR;}/*FIXME: set*/
	|	'`' '{' exprlist '}'	{SYNTAX_ERROR;}/*FIXME: nonordered multiset*/
	|	WHILE expr DO expr	{ PUSH_ACT(GEL_E_WHILE_CONS); }
	|	UNTIL expr DO expr	{ PUSH_ACT(GEL_E_UNTIL_CONS); }
	|	DO expr WHILE expr	{ PUSH_ACT(GEL_E_DOWHILE_CONS); }
	|	DO expr UNTIL expr	{ PUSH_ACT(GEL_E_DOUNTIL_CONS); }
	|	FOR ident anyequals expr TO expr DO expr { PUSH_ACT(GEL_E_FOR_CONS); }
	|	FOR ident anyequals expr TO expr BY expr DO expr { PUSH_ACT(GEL_E_FORBY_CONS); }
	|	FOR ident IN expr DO expr { PUSH_ACT(GEL_E_FORIN_CONS); }
	|	SUM ident anyequals expr TO expr DO expr { PUSH_ACT(GEL_E_SUM_CONS); }
	|	SUM ident anyequals expr TO expr BY expr DO expr { PUSH_ACT(GEL_E_SUMBY_CONS); }
	|	SUM ident IN expr DO expr { PUSH_ACT(GEL_E_SUMIN_CONS); }
	|	PROD ident anyequals expr TO expr DO expr { PUSH_ACT(GEL_E_PROD_CONS); }
	|	PROD ident anyequals expr TO expr BY expr DO expr { PUSH_ACT(GEL_E_PRODBY_CONS); }
	|	PROD ident IN expr DO expr { PUSH_ACT(GEL_E_PRODIN_CONS); }
	|	IF expr THEN expr %prec LOWER_THAN_ELSE	{ PUSH_ACT(GEL_E_IF_CONS); }
	|	IF expr THEN expr ELSE expr { PUSH_ACT(GEL_E_IFELSE_CONS); }
	|	ident			{ gp_convert_identifier_to_bool ();
					  /* convert true/false to bool */}
	|	'`' ident		{ PUSH_ACT(GEL_E_QUOTE); }
	|	'&' ident		{ PUSH_ACT(GEL_E_REFERENCE); }
	|	deref
	|	ident '(' ')'		{ gp_push_marker_simple(GEL_EXPRLIST_START_NODE);
					  PUSH_ACT(GEL_E_DIRECTCALL); }
	|	ident '(' exprlist ')'	{ PUSH_ACT(GEL_E_DIRECTCALL); }
	|	deref '(' ')'		{ gp_push_marker_simple(GEL_EXPRLIST_START_NODE);
					  PUSH_ACT(GEL_E_DIRECTCALL); }
	|	deref '(' exprlist ')'	{ PUSH_ACT(GEL_E_DIRECTCALL); }
	|	expr CALL '(' exprlist ')' { PUSH_ACT(GEL_E_CALL); }
	|	expr CALL '(' ')'	{ gp_push_marker_simple(GEL_EXPRLIST_START_NODE);
					  PUSH_ACT(GEL_E_CALL); }
	|	FUNCTION ident funcdef	{ PUSH_ACT(GEL_E_DEFEQUALS); }
	|	PARAMETER paramdef
	|	FUNCTION funcdef
	|	'`' funcdef
	|	RETURNTOK expr		{ PUSH_ACT(GEL_E_RETURN); }
	|	BAILOUT			{ PUSH_ACT(GEL_E_BAILOUT); }
	|	EXCEPTION		{ PUSH_ACT(GEL_E_EXCEPTION); }
	|	CONTINUE		{ PUSH_ACT(GEL_E_CONTINUE); }
	|	BREAK			{ PUSH_ACT(GEL_E_BREAK); }
	|	NUMBER			{ gel_stack_push(&gel_parsestack,
							 gel_makenum_use($<val>1)); }
	|	STRING			{ PUSH_CONST_STRING($<id>1); }
	|	'.'			{ gp_push_null(); }
	;

deref:		'*' ident		{ PUSH_ACT(GEL_E_DEREFERENCE); }
	;

ident:		FUNCID			{ PUSH_IDENTIFIER($<id>1); }
	;

paramdef: 	ident anyequals expr %prec EQUALS {
			gp_prepare_push_param (FALSE);
			PUSH_ACT (GEL_E_PARAMETER);
		}
	/*|	'(' expr ')' ident EQUALS expr {
			gp_prepare_push_param (TRUE);
			PUSH_ACT (GEL_E_PARAMETER);
		}*/
	;

anyequals:	EQUALS
	|	DEFEQUALS
	;

funcdef:	'(' identlist ')' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (FALSE /* vararg */,
					     TRUE /* arguments */,
					     FALSE /* extradict */,
					     FALSE /* never_subst */)) {
				SYNTAX_ERROR;
			}
						}
	|	'(' identlist THREEDOTS ')' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (TRUE /* vararg */,
					     TRUE /* arguments */,
					     FALSE /* extradict */,
					     FALSE /* never_subst */)) {
				SYNTAX_ERROR;
			}
							}
	|	'(' ')' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (FALSE /* vararg */,
					     FALSE /* arguments */,
					     FALSE /* extradict */,
					     FALSE /* never_subst */)) {
				SYNTAX_ERROR;
			}
					}
	|	'(' identlist ')' '[' identlist ']' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (FALSE /* vararg */,
					     TRUE /* arguments */,
					     TRUE /* extradict */,
					     TRUE /* never_subst */)) {
				SYNTAX_ERROR;
			}
						}
	|	'(' identlist THREEDOTS ')' '[' identlist ']' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (TRUE /* vararg */,
					     TRUE /* arguments */,
					     TRUE /* extradict */,
					     TRUE /* never_subst */)) {
				SYNTAX_ERROR;
			}
							}
	|	'(' ')' '[' identlist ']' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (FALSE /* vararg */,
					     FALSE /* arguments */,
					     TRUE /* extradict */,
					     TRUE /* never_subst */)) {
				SYNTAX_ERROR;
			}
					}
	;
	|	'(' identlist ')' '[' ']' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (FALSE /* vararg */,
					     TRUE /* arguments */,
					     FALSE /* extradict */,
					     TRUE /* never_subst */)) {
				SYNTAX_ERROR;
			}
						}
	|	'(' identlist THREEDOTS ')' '[' ']' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (TRUE /* vararg */,
					     TRUE /* arguments */,
					     FALSE /* extradict */,
					     TRUE /* never_subst */)) {
				SYNTAX_ERROR;
			}
							}
	|	'(' ')' '[' ']' anyequals expr %prec FUNCTION {
			if ( ! gp_push_func (FALSE /* vararg */,
					     FALSE /* arguments */,
					     FALSE /* extradict */,
					     TRUE /* never_subst */)) {
				SYNTAX_ERROR;
			}
					}
	;
	
identlist:	identlist ',' ident
	|	ident { if(!gp_push_marker(GEL_EXPRLIST_START_NODE)) {SYNTAX_ERROR;} }
	;

exprlist:	exprlist ',' expr
	/* We ignore the NEXTROW mark after a comma as it's just a return
	 * breaking a long vector */
	|	exprlist ',' NEXTROW expr
	|	expr { if(!gp_push_marker(GEL_EXPRLIST_START_NODE)) {SYNTAX_ERROR;} }
	;
	
matrixrows:	matrixrows NEXTROW exprlist { if(!gp_push_matrix_row()) {SYNTAX_ERROR;} }
	|	exprlist { if(!gp_push_matrix_row()) {SYNTAX_ERROR;} if(!gp_push_marker(GEL_MATRIX_START_NODE)) {SYNTAX_ERROR;} }
	;
	
%%
