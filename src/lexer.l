/* GENIUS Calculator
 * Copyright (C) 1997-2016 Jiri (George) Lebl
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "calc.h"
#include "parse.h"
#include "util.h"
#include "binreloc.h"

int gel_lex_fd[2];
int gel_first_tok = STARTTOK;
gboolean gel_return_ret = FALSE;
int gel_parenth_depth = 0;
gboolean gel_lex_init = TRUE;
extern GSList *gel_parsestack;
static int matrix_entry = 0;
static GSList *matrix_entry_stack = NULL;

/* Ugly warning avoiding */
#define yywrap() 1
#define YY_SKIP_YYWRAP
/*#ifdef FLEX_SCANNER
int yylex(void);
#endif*/

#define NO_RET {gel_return_ret = FALSE;}
#define DO_RET { \
	if(!gel_parenth_depth) \
		gel_return_ret = TRUE; \
	else \
		gel_return_ret = FALSE; \
}

static GHashTable *fp_hash = NULL;

void gel_lexer_open(FILE *fp);
void gel_lexer_parse(FILE *fp);
void gel_lexer_close(FILE *fp);

void
gel_lexer_open(FILE *fp)
{
	YY_BUFFER_STATE buf;
	if(!fp_hash)
		fp_hash = g_hash_table_new(NULL, NULL);

	buf = g_hash_table_lookup(fp_hash, fp);
	if(buf)
		return;

	buf = yy_create_buffer(fp, YY_BUF_SIZE);
	g_hash_table_insert(fp_hash, fp, buf);
}

void
gel_lexer_parse(FILE *fp)
{
	YY_BUFFER_STATE buf;

	g_assert(fp_hash);

	buf = g_hash_table_lookup(fp_hash, fp);

	yy_switch_to_buffer(buf);
	yyparse();
}

void
gel_lexer_close(FILE *fp)
{
	YY_BUFFER_STATE buf;

	/* don't close the stdin */
	if(fp == stdin)
		return;

	g_assert(fp_hash);

	buf = g_hash_table_lookup(fp_hash, fp);

	yy_delete_buffer(buf);

	 g_hash_table_remove(fp_hash, fp);
}

%}

%%

%{
	if(gel_lex_init) {
		gel_return_ret = FALSE;
		gel_parenth_depth = 0;
		matrix_entry = 0;
		matrix_entry_stack = NULL;
		gel_lex_init = FALSE;
	}
	
	if(gel_first_tok!=INT_MAX) {
		int i = gel_first_tok;
		gel_first_tok = INT_MAX;
		return i;
	}
%}


#.*$		; /*comment, ignore*/

^[ 	]*load[ 	]+<([^\n>]|\\>)*>[ 	]*$ {
			char *s;
			char *end;
			char *dir;
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			s=strchr(yytext,'<')+1; /*find the beginning*/
			end=strrchr(yytext,'>'); /*find the end*/
			s = gel_unescape_string(s,end,NULL);
			dir = gbr_find_data_dir (DATADIR);
			yylval.id = g_build_filename (dir,
						      "genius",
						      s,
						      NULL);
			g_free (dir);
			g_free(s);
			DO_RET;
			return LOADFILE;
		}

^[ 	]*load[ 	]+[^\n 	].*$ {
			char *s;
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			s=strchr(yytext,'d')+1; /*move after the load*/
			/*btw from the above RE we know this will work*/
			while(*s==' ' || *s=='\t')
				s++;
			yylval.id = g_strdup(s);
			DO_RET;
			return LOADFILE_GLOB;
		}

^[ 	]*cd[ 	]+[^\n 	].*$ {
			char *s;
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			s=strchr(yytext,'d')+1; /*move after the cd*/
			/*btw from the above RE we know this will work*/
			while(*s==' ' || *s=='\t')
				s++;
			yylval.id = g_strdup(s);
			DO_RET;
			return CHANGEDIR;
		}

^[ 	]*plugin[ 	]+[^\n 	].*$ {
			char *s;
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			s=strchr(yytext,'n')+1; /*move after the plugin*/
			/*btw from the above RE we know this will work*/
			while(*s==' ' || *s=='\t')
				s++;
			yylval.id = g_strdup(s);
			DO_RET;
			return LOAD_PLUGIN;
		}

^[ 	]*pwd[ 	]*$ {
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			DO_RET;
			return PWD;
		}

^[ 	]*ls[ 	]+[^\n 	].*$ {
			char *s;
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			s=strchr(yytext,'s')+1; /*move after the ls*/
			/*btw from the above RE we know this will work*/
			while(*s==' ' || *s=='\t')
				s++;
			yylval.id = g_strdup(s);
			DO_RET;
			return LS_ARG;
		}

^[ 	]*ls[ 	]*$ {
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			DO_RET;
			return LS;
		}

^[ 	]*help[ 	]+[^\n 	].*$ {
			char *s;
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			s=strchr(yytext,'p')+1; /*move after the help*/
			/*btw from the above RE we know this will work*/
			while(*s==' ' || *s=='\t')
				s++;
			yylval.id = g_strdup(s);
			DO_RET;
			return HELP_ARG;
		}

^[ 	]*help[ 	]*$ {
			if(gel_parsestack || gel_parenth_depth) {REJECT;}
			DO_RET;
			return HELP;
		}


";"		{
			if (matrix_entry &&
			    GPOINTER_TO_INT(matrix_entry_stack->data) == 0) {
				return NEXTROW;
			}
			DO_RET;
			return SEPAR;
		}

\"([^"]|\\\")*\"	{ 
			DO_RET;
			yylval.id = gel_unescape_string(&yytext[1],NULL,NULL);
			/*kill the trailing "*/
			yylval.id[strlen(yylval.id)-1]='\0';
			return STRING;
		}

==		{ NO_RET; return EQ_CMP; }
!=		{ NO_RET; return NE_CMP; }
"<>"		{ NO_RET; return NE_CMP; }
"<=>"		{ NO_RET; return CMP_CMP; }

"<="		{ NO_RET; return LE_CMP; }
">="		{ NO_RET; return GE_CMP; }

"<"		{ NO_RET; return LT_CMP; }
">"		{ NO_RET; return GT_CMP; }

=		{ NO_RET; return EQUALS; }
:=		{ NO_RET; return DEFEQUALS; }

swapwith	{ NO_RET; return SWAPWITH; }
increment	{ NO_RET; return INCREMENT; }

not		{ NO_RET; return LOGICAL_NOT; }
and		{ NO_RET; return LOGICAL_AND; }
xor		{ NO_RET; return LOGICAL_XOR; }
or		{ NO_RET; return LOGICAL_OR; }

while		{ NO_RET; return WHILE; }
until		{ NO_RET; return UNTIL; }
for		{ NO_RET; return FOR; }
sum		{ NO_RET; return SUM; }
prod		{ NO_RET; return PROD; }
do		{ NO_RET; return DO; }
to		{ NO_RET; return TO; }
by		{ NO_RET; return BY; }
in		{ NO_RET; return IN; }
if		{ NO_RET; return IF; }
then		{ NO_RET; return THEN; }
else		{ NO_RET; return ELSE; }

function	{ NO_RET; return FUNCTION; }
parameter	{ NO_RET; return PARAMETER; }
call		{ NO_RET; return CALL; }

return		{ NO_RET; return RETURNTOK; }
bailout		{ DO_RET; return BAILOUT; }
exception	{ DO_RET; return EXCEPTION; }
continue	{ DO_RET; return CONTINUE; }
break		{ DO_RET; return BREAK; }

mod		{ NO_RET; return MOD; }

null		{ DO_RET; return '.'; }

local		{ DO_RET; return LOCAL; }

"@("		{
			gel_parenth_depth++;
			if (matrix_entry_stack != NULL)
				matrix_entry_stack->data =
					GINT_TO_POINTER
					  (GPOINTER_TO_INT(matrix_entry_stack->data) + 1);
			NO_RET;
			return AT;
		}
\.\.\.		{
			NO_RET;
			return THREEDOTS;
		}

[[][\t ]*	{
			matrix_entry++;
			gel_parenth_depth++;
			matrix_entry_stack =
				g_slist_prepend (matrix_entry_stack,
						 GINT_TO_POINTER (0));
			NO_RET;
			return '[';
		}

[\t ]*[]]	{
			GSList *li;
			matrix_entry--;
			gel_parenth_depth--;
			li = matrix_entry_stack;
			matrix_entry_stack = g_slist_remove_link (matrix_entry_stack, li);
			g_slist_free_1 (li);
			DO_RET;
			return ']';
		}
			

[({]		{
			gel_parenth_depth++;
			if (matrix_entry_stack != NULL)
				matrix_entry_stack->data =
					GINT_TO_POINTER
					  (GPOINTER_TO_INT(matrix_entry_stack->data) + 1);
			NO_RET;
			return yytext[0];
		}

[)][ \t]*i		{
			gel_parenth_depth--;
			if (matrix_entry_stack != NULL)
				matrix_entry_stack->data =
					GINT_TO_POINTER
					  (GPOINTER_TO_INT(matrix_entry_stack->data) - 1);
			DO_RET;
			return MAKEIMAGPARENTH;
		}

[)}]		{
			gel_parenth_depth--;
			if (matrix_entry_stack != NULL)
				matrix_entry_stack->data =
					GINT_TO_POINTER
					  (GPOINTER_TO_INT(matrix_entry_stack->data) - 1);
			DO_RET;
			return yytext[0];
		}

[a-zA-Z_][a-zA-Z0-9_]*	{
				/* identifier*/
				yylval.id = g_strdup(yytext);
				DO_RET;
				return FUNCID;
			}

[1-9][0-9]*i?[ ][1-9][0-9]*i?[/][1-9][0-9]*i?	{
			mpw_init(yylval.val);
			mpw_set_str(yylval.val,yytext,10);
			if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
				gel_error_num = GEL_NO_ERROR;
				REJECT;
			}
			DO_RET;
			return NUMBER;
		}

[0-9]+i		{
			mpw_init(yylval.val);
			mpw_set_str_complex_int(yylval.val,yytext,10);
			if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
				gel_error_num = GEL_NO_ERROR;
				REJECT;
			}
			DO_RET;
			return NUMBER;
		}
			

[0-9]*\.[0-9]+[eE@][-+]?[0-9]+i	|
[0-9]+[eE@][-+]?[0-9]+i		|
[0-9]*\.[0-9]+i	{
			char *s;
			if(yytext[0] == '.')
				s = g_strconcat("0",&yytext[0],NULL);
			else
				s = g_strdup(yytext);
			mpw_init(yylval.val);
			mpw_set_str_complex(yylval.val,s,10);
			g_free(s);
			if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
				gel_error_num = GEL_NO_ERROR;
				REJECT;
			}
			DO_RET;
			return NUMBER;
		}


[0-9]*\.[0-9]+[eE@][-+]?[0-9]+	|
[0-9]+[eE@][-+]?[0-9]+		|
[0-9]*\.[0-9]+	{
			char *s;
			if(yytext[0] == '.')
				s = g_strconcat("0",&yytext[0],NULL);
			else
				s = g_strdup(yytext);
			mpw_init(yylval.val);
			mpw_set_str_float(yylval.val,s,10);
			g_free(s);
			if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
				gel_error_num = GEL_NO_ERROR;
				REJECT;
			}
			DO_RET;
			return NUMBER;
		}


([1-3][0-9]|[1-9])\\[0-9a-zA-Z]+	{
			char *s;
			int base;

			s=g_malloc(strlen(yytext)-2); /*minus 3 and plus 1*/
			sscanf(yytext,"%d\\%[0-9a-zA-Z]s",&base,s);
			if(base>36) { g_free(s); REJECT; }
			mpw_init(yylval.val);
			mpw_set_str_int(yylval.val,s,base);
			g_free(s);
			if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
				gel_error_num = GEL_NO_ERROR;
				REJECT;
			}
			DO_RET;
			return NUMBER;
		}

0x[0-9a-fA-F]+	|
0[0-7]+		|
[0-9]+		{
			mpw_init(yylval.val);
			mpw_set_str_int(yylval.val,yytext,0);
			if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
				gel_error_num = GEL_NO_ERROR;
				REJECT;
			}
			DO_RET;
			return NUMBER;
		}
		
\\\n		{ gel_incr_file_info(); }
		
[\t ]*\n	{ 
			gel_incr_file_info();
			if(matrix_entry) {
				return NEXTROW;
			} else if(gel_return_ret) {
				gel_first_tok = 0;
				return '\n';
			}
		}
		
[ \t\r]+	; /*ignore whitespace*/

\.\+		{ DO_RET; return ELTELTPLUS; }
\.\-		{ DO_RET; return ELTELTMINUS; }
\.\*		{ DO_RET; return ELTELTMUL; }
\.\/		{ DO_RET; return ELTELTDIV; }
\.\\		{ DO_RET; return ELTELTBACKDIV; }
\.\%		{ DO_RET; return ELTELTMOD; }
\.\^		{ DO_RET; return ELTELTEXP; }
\.\'		{ DO_RET; return TRANSPOSE; }

[!][!]		{ DO_RET; return DOUBLEFACT; }

[!']		{
			DO_RET;
			return yytext[0];
		}

[@^*/%+-`]	{ NO_RET; return yytext[0]; }

\.		{ DO_RET; return '.'; }

\|		{ DO_RET; return '|'; }

<<EOF>>		{
			gel_got_eof = TRUE;
			return '\n';
		}

.		{ NO_RET; return yytext[0]; }

%%

int my_yyinput(void);
int my_yyinput(void) { return input(); }
void my_yy_flush_buffer(void);
void my_yy_flush_buffer(void) { YY_FLUSH_BUFFER; }

/* avoid unused warnings, kind of bogus */
/* Apparently this no worky, so don't do it */
#if 0
#ifdef FLEX_SCANNER
void fake_fake_fake (void);
void fake_fake_fake (void) {
	 yy_flex_realloc (NULL, 0);
	 yyunput (0, NULL);
}
#endif
#endif

/* Dropping support for tabs as separators in matrix entry as of 1.0.1
   if we want to readd it, just add the following after "ignore whitespace"
   line above
^[\t ][\t ]*	;

[\t ]*,[\t ]*	{ return ','; }

[\t][\t ]*	{
			if(matrix_entry)
				return ',';
		}
		*/

