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

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#ifdef HAVE_WORDEXP
#include <wordexp.h>
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <readline/tilde.h>

#include "calc.h"
#include "eval.h"
#include "util.h"
#include "dict.h"
#include "funclib.h"
#include "matrixw.h"
#include "compil.h"
#include "plugin.h"

#include "geloutput.h"

#include "mpwrap.h"

#include "lexer.h"

#include "parse.h"

extern int lex_fd[2];
extern int first_tok;
extern int lex_init;

extern char *yytext;
extern int yydebug;

GelOutput *main_out = NULL;

void (*evalnode_hook)(void) = NULL;
int run_hook_every = 1000;
void (*statechange_hook)(calcstate_t) = NULL;

static GHashTable *funcdesc = NULL;

/*these two are used for test parses so that we know when we have a complete
  expression toevaluate*/
static gboolean ignore_end_parse_errors = FALSE;
static gboolean got_end_too_soon = FALSE;

GHashTable *uncompiled = NULL;

/* stack ... has to be global:-( */
GSList *evalstack=NULL;

/*error .. global as well*/
calc_error_t error_num = NO_ERROR;
int got_eof = FALSE;

/*the current state of the calculator*/
calcstate_t calcstate = {0};

/*error reporting function*/
void (*errorout)(char *)=NULL;
void (*infoout)(char *)=NULL;

char *loadfile = NULL;
char *loadfile_glob = NULL;
char *load_plugin = NULL;

int interrupted = FALSE;

static GSList *curfile = NULL;
static GSList *curline = NULL;

/*from lexer.l*/
int my_yyinput(void);

void
add_description (const char *func, const char *desc)
{
	char *origkey;
	char *origdesc;
	char *p;
	char *d;
	
	/*kill \n's \r's and ;'s (for compiled parsing purposes)
	  and cap the entry at 80*/
	d = g_strdup(desc);
	if((p=strchr(d,'\n')))
		*p = '\0';
	if((p=strchr(d,'\r')))
		*p = '\0';
	if(strlen(d)>80) 
		d[81]='\0';

	if(!funcdesc)
		funcdesc = g_hash_table_new(g_str_hash,g_str_equal);
	
	if(g_hash_table_lookup_extended(funcdesc,func,
					(gpointer *)&origkey,
					(gpointer *)&origdesc)) {
		g_free(origdesc);
		g_hash_table_insert(funcdesc,origkey,g_strdup(d));
	} else
		g_hash_table_insert(funcdesc,g_strdup(func),g_strdup(d));
	g_free(d);
}

const char *
get_description(const char *func)
{
	char *s;
	if(!funcdesc)
		return "";
	
	s = g_hash_table_lookup(funcdesc,func);
	if(s) return s;
	else return "";
}

void
push_file_info(char *file,int line)
{
	curfile = g_slist_prepend(curfile,file?g_strdup(file):NULL);
	curline = g_slist_prepend(curline,GINT_TO_POINTER(line));
}

void
pop_file_info(void)
{
	GSList *li;
	g_assert(curfile && curline);

	li = curfile;
	curfile = g_slist_remove_link(curfile,curfile);
	g_free(li->data);
	g_slist_free_1(li);
	li = curline;
	curline = g_slist_remove_link(curline,curline);
	g_slist_free_1(li);
}

void
incr_file_info(void)
{
	int i;
	
	if(!curline)
		return;
	
	i = GPOINTER_TO_INT(curline->data);
	curline->data = GINT_TO_POINTER((i+1));
}

void
rewind_file_info(void)
{
	if(!curline)
		return;
	
	curline->data = GINT_TO_POINTER(1);
}

void
get_file_info(char **file, int *line)
{
	if(!curline || !curfile) {
		*file = NULL;
		*line = 0;
		return;
	}
	
	*file = curfile->data;
	*line = GPOINTER_TO_INT(curline->data);
}

static void
append_binaryoper(GelOutput *gelo, char *p, GelETree *n)
{
	GelETree *l,*r;
	GET_LR(n,l,r);
	gel_output_string(gelo, "(");
	print_etree(gelo, l, FALSE);
	gel_output_string(gelo, p);
	print_etree(gelo, r, FALSE);
	gel_output_string(gelo, ")");
}

static void
append_unaryoper(GelOutput *gelo, char *p, GelETree *n)
{
	GelETree *l;
	GET_L(n,l);
	gel_output_string(gelo, "(");
	gel_output_string(gelo, p);
	print_etree(gelo, l, FALSE);
	gel_output_string(gelo, ")");
}

static void
appendoper(GelOutput *gelo, GelETree *n)
{
	GelETree *l,*r,*rr;
	GelETree *li;

	switch(n->op.oper) {
		case E_SEPAR:
			append_binaryoper(gelo,";",n); break;
		case E_EQUALS:
			append_binaryoper(gelo,"=",n); break;
		case E_ABS:
			GET_L(n,l);
			gel_output_string(gelo,"(|");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo,"|)");
			break;
		case E_PLUS:
			append_binaryoper(gelo,"+",n); break;
		case E_MINUS:
			append_binaryoper(gelo,"-",n); break;
		case E_MUL:
			append_binaryoper(gelo,"*",n); break;
		case E_DIV:
			append_binaryoper(gelo,"/",n); break;
		case E_BACK_DIV:
			if(n->op.args->type == VALUE_NODE &&
			   n->op.args->any.next->type == VALUE_NODE)
				append_binaryoper(gelo," \\ ",n);
			else
				append_binaryoper(gelo,"\\",n);
			break;
		case E_MOD:
			append_binaryoper(gelo,"%",n); break;
		case E_NEG:
			append_unaryoper(gelo,"-",n); break;
		case E_EXP:
			append_binaryoper(gelo,"^",n); break;
		case E_FACT:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo, "!)");
			break;

		case E_TRANSPOSE:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo, ".')");
			break;
			
		case E_CONJUGATE_TRANSPOSE:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo, "')");
			break;

		case E_EQ_CMP:
			append_binaryoper(gelo,"==",n); break;
		case E_NE_CMP:
			append_binaryoper(gelo,"!=",n); break;
		case E_CMP_CMP:
			append_binaryoper(gelo,"<=>",n); break;
		case E_LT_CMP:
			append_binaryoper(gelo,"<",n); break;
		case E_GT_CMP:
			append_binaryoper(gelo,">",n); break;
		case E_LE_CMP:
			append_binaryoper(gelo,"<=",n); break;
		case E_GE_CMP:
			append_binaryoper(gelo,">=",n); break;
		case E_LOGICAL_AND:
			append_binaryoper(gelo," and ",n); break;
		case E_LOGICAL_OR:
			append_binaryoper(gelo," or ",n); break;
		case E_LOGICAL_XOR:
			append_binaryoper(gelo," xor ",n); break;
		case E_LOGICAL_NOT:
			append_unaryoper(gelo,"not ",n); break;

		case E_REGION_SEP:
			append_binaryoper(gelo,"..",n); break;

		case E_GET_VELEMENT:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo,"@(");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,"))");
			break;

		case E_GET_ELEMENT:
		case E_GET_REGION:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo,"@(");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,",");
			print_etree(gelo, rr, FALSE);
			gel_output_string(gelo,"))");
			break;
		case E_GET_ROW_REGION:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo,"@[");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,",])");
			break;
		case E_GET_COL_REGION:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo,"@[,");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,"])");
			break;

		case E_QUOTE:
			append_unaryoper(gelo,"`",n); break;
		case E_REFERENCE:
			append_unaryoper(gelo,"&",n); break;
		case E_DEREFERENCE:
			append_unaryoper(gelo,"*",n); break;

		case E_IF_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(if ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," then ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_IFELSE_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(if ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," then ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo," else ");
			print_etree(gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_WHILE_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(while ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," do ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_UNTIL_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(until ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," do ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_DOWHILE_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(do ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," while ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_DOUNTIL_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(do ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," until ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_FOR_CONS:
			{
				GelETree *a,*b,*c,*d;
				GET_ABCD(n,a,b,c,d);
				gel_output_string(gelo,"(for ");
				print_etree(gelo, a, FALSE);
				gel_output_string(gelo," = ");
				print_etree(gelo, b, FALSE);
				gel_output_string(gelo," to ");
				print_etree(gelo, c, FALSE);
				gel_output_string(gelo," do ");
				print_etree(gelo, d, FALSE);
				gel_output_string(gelo,")");
				break;
			}
		case E_FORBY_CONS:
			{
				GelETree *a,*b,*c,*d,*e;
				GET_ABCDE(n,a,b,c,d,e);
				gel_output_string(gelo,"(for ");
				print_etree(gelo, a, FALSE);
				gel_output_string(gelo," = ");
				print_etree(gelo, b, FALSE);
				gel_output_string(gelo," to ");
				print_etree(gelo, c, FALSE);
				gel_output_string(gelo," by ");
				print_etree(gelo, d, FALSE);
				gel_output_string(gelo," do ");
				print_etree(gelo, e, FALSE);
				gel_output_string(gelo,")");
				break;
			}
		case E_FORIN_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(for ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," in ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo," do ");
			print_etree(gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_SUM_CONS:
			{
				GelETree *a,*b,*c,*d;
				GET_ABCD(n,a,b,c,d);
				gel_output_string(gelo,"(sum ");
				print_etree(gelo, a, FALSE);
				gel_output_string(gelo," = ");
				print_etree(gelo, b, FALSE);
				gel_output_string(gelo," to ");
				print_etree(gelo, c, FALSE);
				gel_output_string(gelo," do ");
				print_etree(gelo, d, FALSE);
				gel_output_string(gelo,")");
				break;
			}
		case E_SUMBY_CONS:
			{
				GelETree *a,*b,*c,*d,*e;
				GET_ABCDE(n,a,b,c,d,e);
				gel_output_string(gelo,"(sum ");
				print_etree(gelo, a, FALSE);
				gel_output_string(gelo," = ");
				print_etree(gelo, b, FALSE);
				gel_output_string(gelo," to ");
				print_etree(gelo, c, FALSE);
				gel_output_string(gelo," by ");
				print_etree(gelo, d, FALSE);
				gel_output_string(gelo," do ");
				print_etree(gelo, e, FALSE);
				gel_output_string(gelo,")");
				break;
			}
		case E_SUMIN_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(sum ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," in ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo," do ");
			print_etree(gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_PROD_CONS:
			{
				GelETree *a,*b,*c,*d;
				GET_ABCD(n,a,b,c,d);
				gel_output_string(gelo,"(prod ");
				print_etree(gelo, a, FALSE);
				gel_output_string(gelo," = ");
				print_etree(gelo, b, FALSE);
				gel_output_string(gelo," to ");
				print_etree(gelo, c, FALSE);
				gel_output_string(gelo," do ");
				print_etree(gelo, d, FALSE);
				gel_output_string(gelo,")");
				break;
			}
		case E_PRODBY_CONS:
			{
				GelETree *a,*b,*c,*d,*e;
				GET_ABCDE(n,a,b,c,d,e);
				gel_output_string(gelo,"(prod ");
				print_etree(gelo, a, FALSE);
				gel_output_string(gelo," = ");
				print_etree(gelo, b, FALSE);
				gel_output_string(gelo," to ");
				print_etree(gelo, c, FALSE);
				gel_output_string(gelo," by ");
				print_etree(gelo, d, FALSE);
				gel_output_string(gelo," do ");
				print_etree(gelo, e, FALSE);
				gel_output_string(gelo,")");
				break;
			}
		case E_PRODIN_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(prod ");
			print_etree(gelo, l, FALSE);
			gel_output_string(gelo," in ");
			print_etree(gelo, r, FALSE);
			gel_output_string(gelo," do ");
			print_etree(gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;

		case E_DIRECTCALL:
		case E_CALL:
			GET_L(n,l);
			gel_output_string(gelo,"(");
			if(l->type==IDENTIFIER_NODE) {
				gel_output_string(gelo,l->id.id->token);
			} else if(l->type == OPERATOR_NODE && l->op.oper == E_DEREFERENCE) {
				GelETree *t;
				GET_L(l,t);
				if(t->type!=IDENTIFIER_NODE) {
					(*errorout)(_("Bad identifier for function node!"));
					gel_output_string(gelo,"?)");
					break;
				}
				gel_output_string(gelo,"*");
				gel_output_string(gelo,t->id.id->token);
			} else {
				(*errorout)(_("Bad identifier for function node!"));
				gel_output_string(gelo,"?)");
				break;
			}
			gel_output_string(gelo,"(");
			li = n->op.args->any.next;
			if(li) {
				print_etree(gelo, li, FALSE);
				li=li->any.next;
			}
			for(;li!=NULL;li=li->any.next) {
				gel_output_string(gelo,",");
				print_etree(gelo, li, FALSE);
			}
			gel_output_string(gelo,"))");
			break;
		case E_RETURN:
			append_unaryoper(gelo,"return ",n); break;
		case E_BAILOUT:
			gel_output_string(gelo,"(bailout)"); break;
		case E_EXCEPTION:
			gel_output_string(gelo,"(exception)"); break;
		case E_CONTINUE:
			gel_output_string(gelo,"(continue)"); break;
		case E_BREAK:
			gel_output_string(gelo,"(break)"); break;
		case E_MOD_CALC:
			append_binaryoper(gelo," mod ",n); break;

		default:
			(*errorout)(_("Unexpected operator!"));
			gel_output_string(gelo,"(?)");
			break;
	}
}

static void
appendcomp(GelOutput *gelo, GelETree *n)
{
	GelETree *li;
	GSList *oli;
	
	gel_output_string(gelo,"(");
	
	for(oli=n->comp.comp,li=n->comp.args;oli;
	    li=li->any.next,oli=g_slist_next(oli)) {
		int oper= GPOINTER_TO_INT(oli->data);
		print_etree(gelo, li, FALSE);
		switch(oper) {
		case E_EQ_CMP:
			gel_output_string(gelo,"=="); break;
		case E_NE_CMP:
			gel_output_string(gelo,"!="); break;
		case E_LT_CMP:
			gel_output_string(gelo,"<"); break;
		case E_GT_CMP:
			gel_output_string(gelo,">"); break;
		case E_LE_CMP:
			gel_output_string(gelo,"<="); break;
		case E_GE_CMP:
			gel_output_string(gelo,">="); break;
		default:
			g_assert_not_reached();
		}
	}
	print_etree(gelo, li, FALSE);

	gel_output_string(gelo,")");
}

static void
appendmatrix_troff (GelOutput *gelo, GelMatrixW *m, gboolean nice)
{
	int i, j;
	if (nice)
		gel_output_string (gelo, "\n");
	gel_output_string (gelo, "left [ matrix { ");
	if (nice)
		gel_output_string (gelo, "\n");
	
	for (i = 0; i < gel_matrixw_width (m); i++) {
		gel_output_string (gelo, "rcol { ");
		print_etree (gelo, gel_matrixw_index (m, i, 0), FALSE);
		for(j = 1; j < gel_matrixw_height (m); j++) {
			gel_output_string (gelo, " above ");
			print_etree (gelo, gel_matrixw_index (m, i, j), FALSE);
		}
		gel_output_string (gelo, " } ");
		if (nice)
			gel_output_string (gelo, "\n");
	}
	
	gel_output_string (gelo, "} right ]");
}

static void
appendmatrix_latex (GelOutput *gelo, GelMatrixW *m, gboolean nice)
{
	int i, j;
	if (nice)
		gel_output_string (gelo, "\n");
	gel_output_string (gelo, "\\left( \\begin{array}{");
	for (i = 0; i < gel_matrixw_width (m); i++)
		gel_output_string (gelo, "r");
	gel_output_string (gelo, "}");
	if (nice)
		gel_output_string (gelo, "\n");
	else
		gel_output_string (gelo, " ");
	
	for (j = 0; j < gel_matrixw_height (m); j++) {
		print_etree (gelo, gel_matrixw_index (m, 0, j), FALSE);
		for(i = 1; i < gel_matrixw_width (m); i++) {
			gel_output_string (gelo, " & ");
			print_etree (gelo, gel_matrixw_index (m, i, j), FALSE);
		}
		gel_output_string (gelo, " \\\\");
		if (nice)
			gel_output_string (gelo, "\n");
		else
			gel_output_string (gelo, " ");
	}
	
	gel_output_string (gelo, "\\end{array} \\right)");
}

static void
appendmatrix (GelOutput *gelo, GelMatrixW *m)
{
	int i,j;

	if (calcstate.output_style == GEL_OUTPUT_TROFF) {
		appendmatrix_troff (gelo, m, FALSE /* nice */);
		return;
	} else if (calcstate.output_style == GEL_OUTPUT_LATEX) {
		appendmatrix_latex (gelo, m, FALSE /* nice */);
		return;
	}

	gel_output_string(gelo,"[");
	
	print_etree(gelo, gel_matrixw_index(m,0,0), FALSE);
	
	for(i=1;i<gel_matrixw_width(m);i++) {
		gel_output_string(gelo, ",");
		print_etree(gelo, gel_matrixw_index(m,i,0), FALSE);
	}
	for(j=1;j<gel_matrixw_height(m);j++) {
		gel_output_string(gelo, ";");
		print_etree(gelo, gel_matrixw_index(m,0,j), FALSE);
		for(i=1;i<gel_matrixw_width(m);i++) {
			gel_output_string(gelo, ",");
			print_etree(gelo, gel_matrixw_index(m,i,j), FALSE);
		}
	}

	gel_output_string(gelo,"]");
}


/*make a string representation of an expression*/
void
print_etree(GelOutput *gelo, GelETree *n, gboolean toplevel)
{
	char *p;

	if (n == NULL) {
		(*errorout)(_("NULL tree!"));
		gel_output_string (gelo, "(?)");
		return;
	}

	switch(n->type) {
	case NULL_NODE:
		gel_output_string (gelo, "(null)");
		break;
	case VALUE_NODE:
		p=mpw_getstring(n->val.value,calcstate.max_digits,
				calcstate.scientific_notation,
				calcstate.results_as_floats,
				calcstate.mixed_fractions,
				calcstate.integer_output_base);
		/*if at toplevel, then always print the full number*/
		if(toplevel)
			gel_output_full_string(gelo,p);
		else
			gel_output_string(gelo,p);
		g_free(p);
		break;
	case MATRIX_NODE:
		if (calcstate.output_style != GEL_OUTPUT_TROFF &&
		    calcstate.output_style != GEL_OUTPUT_LATEX &&
		    n->mat.quoted)
			gel_output_string (gelo, "`");
		appendmatrix (gelo, n->mat.matrix);
		break;
	case OPERATOR_NODE:
		appendoper(gelo,n);
		break;
	case IDENTIFIER_NODE:
		gel_output_string(gelo,n->id.id->token);
		break;
	case STRING_NODE:
		gel_output_string(gelo,"\"");
		p = escape_string(n->str.str);
		gel_output_string(gelo,p);
		g_free(p);
		gel_output_string(gelo,"\"");
		break;
	case FUNCTION_NODE:
		{
			GSList *li;
			GelEFunc *f;
			
			f = n->func.func;
			if(!f) {
				(*errorout)(_("NULL function!"));
				gel_output_string(gelo,"(?)");
				break;
			}
			if(f->type==GEL_BUILTIN_FUNC) {
				gel_output_string(gelo,"(<builtin function>)");
				break;
			}

			gel_output_string(gelo,"(`(");

			for(li=f->named_args; li!=NULL; li=g_slist_next(li)) {
				GelToken *id = li->data;
				if(li!=f->named_args)
					gel_output_string(gelo,",");
				gel_output_string(gelo,id->token);
			}

			if(f->type==GEL_USER_FUNC) {
				gel_output_string(gelo,")=(");
				D_ENSURE_USER_BODY (f);
				print_etree(gelo, f->data.user, FALSE);
				gel_output_string(gelo,"))");
			} else {
				/*variable and reference functions should
				  never be in the etree*/
				(*errorout)(_("Unexpected function type!"));
				gel_output_string(gelo,")(?)");
			}
			break;
		}
	case COMPARISON_NODE:
		appendcomp(gelo,n);
		break;
	default:
		(*errorout)(_("Unexpected node!"));
		gel_output_string(gelo,"(?)");
	       break;
	}
}

void
pretty_print_etree(GelOutput *gelo, GelETree *n)
{
	/*do a nice printout of matrices if that's the
	  top node*/
	if(n->type == MATRIX_NODE) {
		int i,j;

		if (calcstate.output_style == GEL_OUTPUT_TROFF) {
			appendmatrix_troff (gelo, n->mat.matrix, TRUE /* nice */);
			return;
		} else if (calcstate.output_style == GEL_OUTPUT_LATEX) {
			appendmatrix_latex (gelo, n->mat.matrix, TRUE /* nice */);
			return;
		}

		if(n->mat.quoted)
			gel_output_string(gelo,"\n`[");
		else
			gel_output_string(gelo,"\n[");
		for(j=0;j<gel_matrixw_height(n->mat.matrix);j++) {
			if(j>0) {
				if(n->mat.quoted)
					gel_output_string(gelo, "\n  ");
				else
					gel_output_string(gelo, "\n ");
			}
			for(i=0;i<gel_matrixw_width(n->mat.matrix);i++) {
				if(i>0) gel_output_string(gelo, "\t");
				print_etree(gelo,
					    gel_matrixw_index(n->mat.matrix,i,j),
					    FALSE);
			}
		}
		gel_output_string(gelo, "]");
	} else {
		print_etree (gelo, n, FALSE);
	}
}

/*make a string representation of an expression*/
char *
string_print_etree(GelETree *n)
{
	GelOutput *gelo = gel_output_new();
	char *ret;
	gel_output_setup_string(gelo, 0, NULL);

	print_etree (gelo, n, TRUE);

	ret = gel_output_snarf_string(gelo);

	gel_output_unref(gelo);

	if(!ret)
		return g_strdup("");
	else
		return ret;
}

/*add the right parenthesis and brackets to the end of the expression*/
char *
addparenth(char *s)
{
	GSList *stack = NULL;
	char *p;
	int l;

	for(p=s;*p!='\0';p++) {
		switch(*p) {
			case '[':
				stack_push(&stack,"]");
				break;
			case '(':
				stack_push(&stack,")");
				break;
			case '{':
				stack_push(&stack,"}");
				break;
			case ']':
				if(!stack)
					return s; /*too many trailing*/
				if(*(char *)stack_pop(&stack) != ']') {
					if(stack) g_slist_free(stack);
					return s; /*non matching*/
				}
				break;
			case ')':
				if(!stack)
					return s; /*too many trailing*/
				if(*(char *)stack_pop(&stack) != ')') {
					if(stack) g_slist_free(stack);
					return s; /*non matching*/
				}
				break;
			case '}':
				if(!stack)
					return s; /*too many trailing*/
				if(*(char *)stack_pop(&stack) != '}') {
					if(stack) g_slist_free(stack);
					return s; /*non matching*/
				}
				break;
		}
	}
	l = strlen(s);
	s = g_realloc(s,l+g_slist_length(stack)+1);
	while(stack)
		s[l++]=*((char *)stack_pop(&stack));
	s[l]='\0';
	return s;
}

void
compile_all_user_funcs(FILE *outfile)
{
	GSList *funcs;
	GSList *li;
	fprintf(outfile,"CGEL "VERSION"\n");
	funcs = d_getcontext();
	if(!funcs) return;
	funcs = g_slist_reverse(g_slist_copy(funcs));
	for(li=funcs;li;li=g_slist_next(li)) {
		GelEFunc *func = li->data;
		char *body;
		GSList *li;
		const char *d;

		if((func->type!=GEL_USER_FUNC &&
		    func->type!=GEL_VARIABLE_FUNC) ||
		   !func->id ||
		   !func->id->token ||
		   strcmp(func->id->token,"Ans")==0)
			continue;

		if(func->data.user) {
			body = gel_compile_tree(func->data.user);
		} else {
			body = g_strdup(g_hash_table_lookup(uncompiled,func->id));
			g_assert(body);
		}
		if(func->type==GEL_USER_FUNC) {
			fprintf(outfile,"F;%d;%s;%d",(int)strlen(body),func->id->token,(int)func->nargs);
			for(li=func->named_args;li;li=g_slist_next(li)) {
				GelToken *tok = li->data;
				fprintf(outfile,";%s",tok->token);
			}
		} else /*GEL_VARIABLE_FUNC*/ {
			fprintf(outfile,"V;%d;%s",(int)strlen(body),func->id->token);
		}

		fprintf(outfile,"\n%s\n",body);
		g_free(body);
		d = get_description(func->id->token);
		if(d && *d)
			fprintf(outfile,"D;%s;%s\n",func->id->token,d);
		if(func->id->protected)
			fprintf(outfile,"P;%s\n",func->id->token);
	}
	g_slist_free(funcs);
}

static void
load_compiled_fp(const char *file, FILE *fp)
{
	char buf[4096];

	if(!fgets(buf,4096,fp))
		return;
	if(strcmp(buf,"CGEL "VERSION"\n")!=0) {
		g_snprintf(buf,4096,_("File '%s' is a wrong version of GEL"),file);
		(*errorout)(buf);
		return;
	}

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext();

	error_num=NO_ERROR;

	/*if we this was set, then the mp library was initialized for
	  sure*/
	g_assert(calcstate.float_prec>0);

	while(fgets(buf,4096,fp)) {
		char *p;
		char *b2;
		GelToken *tok;
		int size,nargs;
		int i;
		GSList *li = NULL;
		int type;

		incr_file_info();

		p=strchr(buf,'\n');
		if(p) *p='\0';

		p = strtok(buf,";");
		if(!p) {
			(*errorout)(_("Badly formed record"));
			continue;
		} else if(*p == 'T') {
			(*errorout)(_("Record out of place"));
			continue;
		} else if(*p == 'D') {
			char *d;
			p = strtok(NULL,";");
			if(!p) {
				(*errorout)(_("Badly formed record"));
				continue;
			}
			d = strtok(NULL,";");
			if(!d) {
				(*errorout)(_("Badly formed record"));
				continue;
			}
			add_description(p,d);
			continue;
		} else if(*p == 'P') {
			GelToken *tok;
			p = strtok(NULL,";");
			if(!p) {
				(*errorout)(_("Badly formed record"));
				continue;
			}
			tok = d_intern(p);
			tok->protected = 1;
			continue;
		} else if(*p != 'F' && *p != 'V') {
			(*errorout)(_("Badly formed record"));
			continue;
		}
		type = *p=='F'?GEL_USER_FUNC:GEL_VARIABLE_FUNC;

		/*size*/
		p = strtok(NULL,";");
		if(!p) {
			(*errorout)(_("Badly formed record"));
			continue;
		}
		size = -1;
		sscanf(p,"%d",&size);
		if(size==-1) {
			(*errorout)(_("Badly formed record"));
			continue;
		}

		/*id*/
		p = strtok(NULL,";");
		if(!p) {
			(*errorout)(_("Badly formed record"));
			continue;
		}
		tok = d_intern(p);

		if(type == GEL_USER_FUNC) {
			/*nargs*/
			p = strtok(NULL,";");
			if(!p) {
				(*errorout)(_("Badly formed record"));
				continue;
			}
			nargs = -1;
			sscanf(p,"%d",&nargs);
			if(size==-1) {
				(*errorout)(_("Badly formed record"));
				continue;
			}

			/*argument names*/
			li = NULL;
			for(i=0;i<nargs;i++) {
				p = strtok(NULL,";");
				if(!p) {
					(*errorout)(_("Badly formed record"));
					g_slist_free(li);
					goto continue_reading;
				}
				li = g_slist_append(li,d_intern(p));
			}
		}

		/*the value*/
		b2 = g_new(char,size+2);
		if(!fgets(b2,size+2,fp)) {
			(*errorout)(_("Missing value for function"));
			g_free(b2);
			g_slist_free(li);
			goto continue_reading;
		}
		incr_file_info();
		p=strchr(b2,'\n');
		if(p) *p='\0';
		if(!uncompiled)
			uncompiled = g_hash_table_new(NULL,NULL);
		g_hash_table_insert(uncompiled,tok,b2);
		if(type == GEL_USER_FUNC)
			d_addfunc(d_makeufunc(tok,NULL,li,nargs));
		else /*GEL_VARIABLE_FUNC*/
			d_addfunc(d_makevfunc(tok,NULL));
continue_reading:	;
	}
	fclose(fp);
}

void
load_compiled_file (const char *dirprefix, const char *file, gboolean warn)
{
	FILE *fp;
	char *newfile;
	if(dirprefix && file[0]!='/')
		newfile = g_strconcat(dirprefix, "/", file, NULL);
	else
		newfile = g_strdup (file);
	if((fp = fopen(newfile,"r"))) {
		push_file_info(newfile,1);
		load_compiled_fp(newfile,fp);
		pop_file_info();
	} else if (warn) {
		char buf[256];
		g_snprintf(buf,256,_("Can't open file: '%s'"), newfile);
		(*errorout)(buf);
	}
	g_free (newfile);
}

void
set_new_calcstate(calcstate_t state)
{
	if(calcstate.float_prec != state.float_prec) {
		mpw_init_mp(); /*just in case we haven't yet*/
		mpw_set_default_prec(state.float_prec);
		gel_break_fp_caches();
	}
	/*set the state variable for calculator*/
	calcstate=state;

	/*if the calling process already knows, who cares,
	  but call it anyway*/
	if(statechange_hook)
		(*statechange_hook)(state);
}

static void
load_fp(FILE *fp, char *dirprefix)
{
	my_yy_open(fp);
	while(1) {
		evalexp(NULL, fp, NULL, NULL, FALSE, dirprefix);
		if(got_eof) {
			got_eof = FALSE;
			break;
		}
		if(interrupted)
			break;
	}
	my_yy_close(fp);
	/*fclose(fp);*/
}

void
load_file (const char *dirprefix, const char *file, gboolean warn)
{
	FILE *fp;
	char *newfile;
	int oldgeof = got_eof;
	got_eof = FALSE;
	if(dirprefix && file[0]!='/')
		newfile = g_strconcat(dirprefix, "/", file, NULL);
	else
		newfile = g_strdup (file);

	if((fp = fopen(newfile,"r"))) {
		char *dir = g_dirname(newfile);
		push_file_info(newfile,1);
		load_fp(fp, dir);
		pop_file_info();
		g_free(dir);
		got_eof = oldgeof;
	} else if (warn) {
		char buf[256];
		g_snprintf(buf,256,_("Can't open file: '%s'"),newfile);
		(*errorout)(buf);
		got_eof = oldgeof;
	}
	g_free(newfile);
}

void
load_guess_file (const char *dirprefix, const char *file, gboolean warn)
{
	FILE *fp;
	char *newfile;
	int oldgeof = got_eof;
	got_eof = FALSE;
	if(dirprefix && file[0]!='/')
		newfile = g_strconcat(dirprefix, "/", file, NULL);
	else
		newfile = g_strdup (file);

	if((fp = fopen(newfile,"r"))) {
		char buf[6];
		push_file_info(newfile,1);
		if(fgets(buf,6,fp) &&
		   strncmp(buf,"CGEL ",5)==0) {
			rewind(fp);
			load_compiled_fp(newfile,fp);
		} else {
			char *dir = g_dirname(newfile);
			rewind(fp);
			load_fp(fp, dir);
			g_free(dir);
		}
		pop_file_info();
		got_eof = oldgeof;
	} else if (warn) {
		char buf[256];
		g_snprintf(buf,256,_("Can't open file: '%s'"), newfile);
		(*errorout)(buf);
		got_eof = oldgeof;
	}
	g_free (newfile);
}

void
set_new_errorout(void (*func)(char *))
{
	errorout = func;
}

void
set_new_infoout(void (*func)(char *))
{
	infoout = func;
}

static void
do_load_files(const char *dirprefix)
{
	if(loadfile) {
		char *file = loadfile;
		loadfile = NULL;
		while(evalstack)
			gel_freetree(stack_pop(&evalstack));
		load_file(dirprefix, file, TRUE);
		g_free(file);
	}

	if(loadfile_glob) {
#if HAVE_WORDEXP
		wordexp_t we;
		char *flist = loadfile_glob;
		int i;
		loadfile_glob = NULL;
		while(evalstack)
			gel_freetree(stack_pop(&evalstack));
		wordexp(flist,&we,WRDE_NOCMD);
		for(i=0;i<we.we_wordc;i++) {
			
			load_guess_file(dirprefix,we.we_wordv[i],TRUE);
			if(interrupted) {
				wordfree(&we);
				free(flist);
				return;
			}
		}
		wordfree(&we);
		free(flist);
#else
		char *s;
		FILE *fp;
		char buf[258]; /*so that we fit 256 chars in there*/
		char *flist = loadfile_glob;

		loadfile_glob = NULL;
		while(evalstack)
			gel_freetree(stack_pop(&evalstack));
		
		s = g_strdup_printf("for n in %s ; do echo $n ; done",flist);
		fp = popen(s,"r");
		g_free(s);
		while(fgets(buf,258,fp)) {
			int len = strlen(buf);
			if(buf[len-1]=='\n')
				buf[len-1]='\0';
			load_guess_file(dirprefix,buf,TRUE);
			if(interrupted) {
				fclose(fp);
				free(flist);
				return;
			}
		}
		fclose(fp);
		free(flist);
#endif
	}
	
	if(load_plugin) {
		char *plugin = g_strstrip(load_plugin);
		GSList *li;

		load_plugin = NULL;
		
		for(li=plugin_list;li;li=g_slist_next(li)) {
			plugin_t *plg = li->data;
			if(strcmp(plg->base,plugin)==0) {
				open_plugin(plg);
				break;
			}
		}
		if(!li) {
			char *p = g_strdup_printf(_("Cannot open plugin '%s'!"),
						  plugin);
			(*errorout)(p);
			g_free(p);
		}

		g_free(plugin);
	}
}

GelETree *
parseexp(const char *str, FILE *infile, gboolean load_files, gboolean testparse,
	 gboolean *finished, const char *dirprefix)
{
	int stacklen;

	interrupted = FALSE;

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext();

	error_num=NO_ERROR;

	/*if we this was set, then the mp library was initialized for
	  sure*/
	g_assert(calcstate.float_prec>0);

	first_tok = STARTTOK;
	
	g_assert(str || infile);
	g_assert(!(str && infile));

	if(str) {
		int l = strlen(str);
		pipe(lex_fd);
		infile = fdopen(lex_fd[0], "r");
		write(lex_fd[1], str, l);
		if(str[l-1] != '\n')
			write(lex_fd[1], "\n", 1);
		close(lex_fd[1]);
		my_yy_open(infile);
	}

	g_free(loadfile); loadfile = NULL;
	g_free(loadfile_glob); loadfile_glob = NULL;
	g_free(load_plugin); load_plugin = NULL;

	lex_init = TRUE;
	/*yydebug=TRUE;*/  /*turn debugging of parsing on here!*/
	if(testparse) ignore_end_parse_errors = TRUE;
	got_end_too_soon = FALSE;
	my_yy_parse(infile);
	ignore_end_parse_errors = FALSE;

	/*while(yyparse() && !feof(yyin))
		;*/
	
	if(str) {
		while(my_yyinput()!=EOF)
			;
		close(lex_fd[0]);
		fflush(infile);
		my_yy_close(infile);
		/*fclose(infile);*/
	}
	
	if(!load_files) {
		g_free(loadfile); loadfile = NULL;
		g_free(loadfile_glob); loadfile_glob = NULL;
		g_free(load_plugin); load_plugin = NULL;
	} else if(loadfile || loadfile_glob || load_plugin) {
		do_load_files(dirprefix);
		if(finished) *finished = TRUE;
		return NULL;
	}

	/*if we are testing and got an unfinished expression just report that*/
	if(testparse && got_end_too_soon) {
		while(evalstack)
			gel_freetree(stack_pop(&evalstack));
		if(finished) *finished = FALSE;
		return NULL;
	}

	/*catch parsing errors*/
	if(error_num!=NO_ERROR) {
		while(evalstack)
			gel_freetree(stack_pop(&evalstack));
		if(finished) *finished = TRUE;
		return NULL;
	}
	
	stacklen = g_slist_length(evalstack);
	
	if(stacklen==0) {
		if(finished) *finished = FALSE;
		return NULL;
	}

	/*stack is supposed to have only ONE entry*/
	if(stacklen!=1) {
		while(evalstack)
			gel_freetree(stack_pop(&evalstack));
		if(!testparse)
			(*errorout)(_("ERROR: Probably corrupt stack!"));
		if(finished) *finished = FALSE;
		return NULL;
	}
	replace_equals (evalstack->data, FALSE /* in_expression */);
	evalstack->data = gather_comparisons (evalstack->data);
	evalstack->data = replace_parameters (evalstack->data);
	try_to_do_precalc (evalstack->data);
	
	if (finished != NULL)
		*finished = TRUE;
	return stack_pop (&evalstack);
}

GelETree *
runexp(GelETree *exp)
{
	static int busy = FALSE;
	GelETree *ret;
	GelCtx *ctx;
	
	if(busy) {
		(*errorout)(_("ERROR: Can't execute more things at once!"));
		return NULL;
	}
	
	busy = TRUE;

	error_num = NO_ERROR;
	
	push_file_info(NULL,0);

	ctx = eval_get_context();
	ret = eval_etree(ctx,copynode(exp));
	eval_free_context(ctx);

	pop_file_info();

	busy = FALSE;

	/*catch evaluation errors*/
	if(!ret)
		return NULL;
	if(error_num!=NO_ERROR) {
		gel_freetree(ret);
		return NULL;
	}
	return ret;
}

void
evalexp_parsed(GelETree *parsed, GelOutput *gelo,
	       const char *prefix, gboolean pretty)
{
	GelETree *ret;
	
	if(!parsed) return;
	ret = runexp(parsed);
	gel_freetree(parsed);
	if(!ret) return;

	if(ret->type != NULL_NODE && gelo) {
		if(prefix) {
			gel_output_string(gelo, prefix);
		}
		if(pretty)
			pretty_print_etree(gelo,ret);
		else
			print_etree(gelo, ret, TRUE);
		gel_output_string(gelo, "\n");
	}

	/*set ans to the last answer*/
	if(ret->type == FUNCTION_NODE) {
		if(ret->func.func)
			d_addfunc(d_makerealfunc(ret->func.func,d_intern("Ans"),TRUE));
		else
			d_addfunc(d_makevfunc(d_intern("Ans"),gel_makenum_ui(0)));
		gel_freetree(ret);
	} else if(ret->type == OPERATOR_NODE &&
		ret->op.oper == E_REFERENCE) {
		GelETree *t = ret->op.args;
		if(!t) {
			GelEFunc *rf = d_lookup_global(t->id.id);
			if(rf)
				d_addfunc(d_makereffunc(d_intern("Ans"),rf));
			else
				d_addfunc(d_makevfunc(d_intern("Ans"),gel_makenum_ui(0)));
		} else
				d_addfunc(d_makevfunc(d_intern("Ans"),gel_makenum_ui(0)));
		gel_freetree(ret);
	} else
		d_addfunc(d_makevfunc(d_intern("Ans"),ret));
}

void
evalexp (const char *str, FILE *infile, GelOutput *gelo,
	 const char *prefix, gboolean pretty, const char *dirprefix)
{
	GelETree *parsed;
	parsed = parseexp (str, infile, TRUE, FALSE, NULL, dirprefix);
	evalexp_parsed (parsed, gelo, prefix, pretty);
}

/*just to make the compiler happy*/
void yyerror(char *s);

void
yyerror(char *s)
{
	char *out=NULL;
	char *p;
	
	if(ignore_end_parse_errors && yytext[0]=='\0') {
		got_end_too_soon = TRUE;
		return;
	}
	
	if(strcmp(yytext,"\n")==0) {
		out=g_strconcat(_("ERROR: "),s,_(" before newline"),NULL);
	} else if(yytext[0]=='\0') {
		out=g_strconcat(_("ERROR: "),s,_(" at end of input"),NULL);
	} else {
		char *tmp = g_strdup(yytext);
		while((p=strchr(tmp,'\n')))
			*p='.';

		out=g_strconcat(_("ERROR: "),s,_(" before '"),tmp,"'",NULL);
		g_free(tmp);
	}

	(*errorout)(out);
	g_free(out);
	error_num=PARSE_ERROR;
}
