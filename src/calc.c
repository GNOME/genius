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
#include "config.h"

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef HAVE_WORDEXP
#include <wordexp.h>
#else
#include <glob.h>
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

extern const char *genius_toplevels[];

gboolean genius_is_gui = FALSE;

GelOutput *main_out = NULL;

void (*evalnode_hook)(void) = NULL;
int run_hook_every = 1000;
void (*statechange_hook)(calcstate_t) = NULL;

typedef struct {
	char *category;
	char *name;
	gboolean internal;
	GSList *funcs;
} HelpCategory;
static GSList *categories = NULL;
static GHashTable *helphash = NULL;

/*these two are used for test parses so that we know when we have a complete
  expression toevaluate*/
static gboolean ignore_end_parse_errors = FALSE;
static gboolean got_end_too_soon = FALSE;

GHashTable *uncompiled = NULL;

/* stack ... has to be global:-( */
GSList *evalstack=NULL;

/*error .. global as well*/
GeniusError error_num = NO_ERROR;
gboolean gel_got_eof = FALSE;

/*the current state of the calculator*/
calcstate_t calcstate = {0};

/*error reporting function*/
void (*errorout)(const char *) = NULL;
void (*infoout)(const char *) = NULL;

GelCommand gel_command = GEL_NO_COMMAND;
char *gel_command_arg = NULL;

gboolean interrupted = FALSE;

static GSList *curfile = NULL;
static GSList *curline = NULL;

/*from lexer.l*/
int my_yyinput(void);

static int
function_sort (gconstpointer data1, gconstpointer data2)
{
	return strcmp (data1, data2);
}

static int
help_sort (gconstpointer data1, gconstpointer data2)
{
	const GelHelp *h1 = data1;
	const GelHelp *h2 = data2;
	return strcmp (h1->func, h2->func);
}

static HelpCategory *
get_category (const char *category, gboolean insert)
{
	GSList *li;
	for (li = categories; li != NULL; li = li->next) {
		HelpCategory *cat = li->data;
		if (strcmp (cat->category, category) == 0)
			return cat;
	}

	if (insert) {
		HelpCategory *cat = g_new0 (HelpCategory, 1);
		cat->category = g_strdup (category);
		categories = g_slist_append (categories, cat);
		return cat;
	} else {
		return NULL;
	}
}

GelHelp *
get_help (const char *func, gboolean insert)
{
	GelHelp *help;

	if (helphash == NULL)
		helphash = g_hash_table_new (g_str_hash, g_str_equal);

	help = g_hash_table_lookup (helphash, func);

	if (help == NULL && insert) {
		help = g_new0 (GelHelp, 1);
		help->func = g_strdup (func);
		g_hash_table_insert (helphash, help->func, help);
	}

	return help;
}

/* well sorted */
GSList *
get_categories (void)
{
	GSList *li, *list = NULL;
	for (li = categories; li != NULL; li = li->next) {
		HelpCategory *cat = li->data;
		list = g_slist_prepend (list, g_strdup (cat->category));
	}
	return g_slist_reverse (list);
}

const char *
get_category_name (const char *category)
{
	HelpCategory *cat;

	if (category == NULL)
		return _("Uncategorized");

	cat = get_category (category, FALSE /* insert */);
	if (cat == NULL || cat->name == NULL)
		return category;
	else
		return _(cat->name);
}

/* gets undocumented functions */
static GSList *
get_uncategorized_documented (void)
{
	GSList *funcs;
	GSList *li;
	GSList *list;
	
	funcs = d_getcontext();
	if (funcs == NULL)
		return NULL;

	list = NULL;
	
	for (li = funcs; li != NULL; li = li->next) {
		GelEFunc *f = li->data;
		GelHelp *help;
		if (f->id == NULL ||
		    f->id->token == NULL)
			continue;
		help = get_help (f->id->token, FALSE /* insert */);
		if (help != NULL &&
		    help->category == NULL &&
		    help->aliasfor == NULL)
			list = g_slist_insert_sorted (list,
						      help,
						      help_sort);
	}
	return list;
}

/* null for uncategorized */
GSList *
get_helps (const char *category)
{
	HelpCategory *cat;

	if (category == NULL) {
		return get_uncategorized_documented ();
	}

	cat = get_category (category, FALSE /* insert */);
	if (cat == NULL) {
		return NULL;
	} else {
		GSList *li, *list = NULL;
		for (li = cat->funcs; li != NULL; li = li->next) {
			const char *func = li->data;
			GelHelp *help = get_help (func, FALSE /* insert */);
			if (help != NULL)
				list = g_slist_prepend (list, help);
		}
		return g_slist_reverse (list);
	}
}

/* gets undocumented functions */
GSList *
get_undocumented (void)
{
	GSList *funcs;
	GSList *li;
	GSList *list;
	
	funcs = d_getcontext();
	if (funcs == NULL)
		return NULL;

	list = NULL;
	
	for (li = funcs; li != NULL; li = li->next) {
		GelEFunc *f = li->data;
		GelHelp *help;
		if(f->id == NULL ||
		   f->id->token == NULL ||
		   strcmp (f->id->token, "Ans") == 0 ||
		   strcmp (f->id->token, "ninini") == 0 ||
		   strcmp (f->id->token, "shrubbery") == 0)
			continue;
		help = get_help (f->id->token, FALSE /* insert */);
		if (help == NULL)
			list = g_slist_insert_sorted (list,
						      g_strdup (f->id->token),
						      function_sort);
	}
	return list;
}

void
new_category (const char *category, const char *name, gboolean internal)
{
	HelpCategory *cat = get_category (category, TRUE /* insert */);
	g_free (cat->name);
	cat->name = g_strdup (name);
	cat->internal = internal;
}

static void
remove_from_category (const char *func, const char *category)
{
	GSList *li;
	HelpCategory *cat = get_category (category, TRUE /* insert */);

	for (li = cat->funcs; li != NULL; li = li->next) {
		char *f = li->data;
		if (strcmp (f, func) == 0) {
			g_free (f);
			cat->funcs = g_slist_delete_link (cat->funcs, li);
			return;
		}
	}
}

void
add_category (const char *func, const char *category)
{
	GelHelp *help = get_help (func, TRUE /* insert */);
	HelpCategory *cat = get_category (category, TRUE /* insert */);

	if (help->category != NULL) {
		if (strcmp (help->category, category) == 0)
			return;
		remove_from_category (func, help->category);
		g_free (help->category);
	}
	help->category = g_strdup (category);

	cat->funcs = g_slist_insert_sorted (cat->funcs,
					    g_strdup (func),
					    function_sort);
}

static void
remove_alias (const char *func, const char *alias)
{
	GelHelp *help;
	GSList *li;

	help = get_help (func, TRUE /* insert */);
	for (li = help->aliases; li != NULL; li = li->next) {
		char *f = li->data;
		if (strcmp (f, alias) == 0) {
			g_free (f);
			help->aliases = g_slist_delete_link
				(help->aliases, li);
			return;
		}
	}
}

void
add_alias (const char *func, const char *alias)
{
	GelHelp *help, *ahelp;

	help = get_help (func, TRUE /* insert */);
	if (help->aliasfor != NULL) {
		gel_errorout (_("Trying to set an alias for an alias"));
		return;
	}

	ahelp = get_help (alias, TRUE /* insert */);
	if (ahelp->aliasfor != NULL) {
		remove_alias (ahelp->aliasfor, alias);
		g_free (ahelp->aliasfor);
	}
	ahelp->aliasfor = g_strdup (func);

	help->aliases = g_slist_append
		(help->aliases, g_strdup (alias));
}

void
add_help_link (const char *func, const char *link)
{
	GelHelp *help;

	help = get_help (func, TRUE /* insert */);
	g_free (help->help_html);
	help->help_link = g_strdup (link);
}

void
add_help_html (const char *func, const char *html)
{
	GelHelp *help;

	help = get_help (func, TRUE /* insert */);
	g_free (help->help_html);
	help->help_html = g_strdup (html);
}

void
add_description (const char *func, const char *desc)
{
	GelHelp *help;
	char *p;
	char *d;
	
	/*kill \n's \r's and ;'s (for compiled parsing purposes) */
	d = g_strdup(desc);
	if((p=strchr(d,'\n')))
		*p = '\0';
	if((p=strchr(d,'\r')))
		*p = '\0';

	help = get_help (func, TRUE /* insert */);
	g_free (help->description);
	help->description = g_strdup (d);
	g_free (d);
}

void
whack_help (const char *func)
{
	GelHelp *help;

	if (helphash == NULL)
		return;

	help = g_hash_table_lookup (helphash, func);
	if (help != NULL) {
		GSList *li, *list;

		list = g_slist_copy (help->aliases);
		for (li = list; li != NULL; li = li->next) {
			whack_help (li->data);
			g_free (li->data);
		}
		g_slist_free (list);

		if (help->aliasfor != NULL)
			remove_alias (help->aliasfor, func);
		if (help->category != NULL)
			remove_from_category (func, help->category);

		g_hash_table_remove (helphash, func);

		g_slist_free (help->aliases);
		g_free (help->aliasfor);
		g_free (help->category);
		g_free (help->help_link);
		g_free (help->help_html);
		g_free (help->description);
		g_free (help->func);
		g_free (help);
	}
}

void
gel_push_file_info(const char *file,int line)
{
	curfile = g_slist_prepend (curfile, file?g_strdup(file):NULL);
	curline = g_slist_prepend (curline, GINT_TO_POINTER (line));
}

void
gel_pop_file_info(void)
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
gel_incr_file_info(void)
{
	int i;
	
	if (curline == NULL)
		return;
	
	i = GPOINTER_TO_INT (curline->data);
	curline->data = GINT_TO_POINTER ((i+1));
}

void
gel_rewind_file_info(void)
{
	if(!curline)
		return;
	
	curline->data = GINT_TO_POINTER(1);
}

void
gel_get_file_info (char **file, int *line)
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
append_anal_binaryoper(GelOutput *gelo, char *p, GelETree *n)
{
	gboolean extra_param1 = FALSE;
	gboolean extra_param2 = FALSE;
	GelETree *l,*r;
	GET_LR(n,l,r);
	if (l->type == VALUE_NODE &&
	    (mpw_is_complex (l->val.value) ||
	     mpw_sgn (l->val.value) < 0 ||
	     mpw_is_rational (l->val.value)))
		extra_param1 = TRUE;
	if (r->type == VALUE_NODE &&
	    (mpw_is_complex (r->val.value) ||
	     mpw_sgn (r->val.value) < 0 ||
	     mpw_is_rational (r->val.value)))
		extra_param2 = TRUE;
	if (extra_param1)
		gel_output_string(gelo, "((");
	else
		gel_output_string(gelo, "(");
	gel_print_etree (gelo, l, FALSE);
	if (extra_param1)
		gel_output_string(gelo, ")");
	gel_output_string(gelo, p);
	if (extra_param2)
		gel_output_string(gelo, "(");
	gel_print_etree (gelo, r, FALSE);
	if (extra_param2)
		gel_output_string(gelo, "))");
	else
		gel_output_string(gelo, ")");
}

static void
append_binaryoper(GelOutput *gelo, char *p, GelETree *n)
{
	GelETree *l,*r;
	GET_LR(n,l,r);
	gel_output_string(gelo, "(");
	gel_print_etree (gelo, l, FALSE);
	gel_output_string(gelo, p);
	gel_print_etree (gelo, r, FALSE);
	gel_output_string(gelo, ")");
}

static void
append_unaryoper(GelOutput *gelo, char *p, GelETree *n)
{
	GelETree *l;
	GET_L (n, l);
	gel_output_string (gelo, "(");
	gel_output_string (gelo, p);
	gel_print_etree (gelo, l, FALSE);
	gel_output_string (gelo, ")");
}

static void
appendoper(GelOutput *gelo, GelETree *n)
{
	GelETree *l, *r, *rr;
	GelETree *a, *b, *c, *d, *e;
	GelETree *li;

	switch(n->op.oper) {
		case E_SEPAR:
			gel_output_string (gelo,"(");
			r = n->op.args;
			while (r != NULL) {
				gel_print_etree (gelo, r, FALSE);
				r = r->any.next;
				if (r != NULL)
					gel_output_string (gelo, ";");
			}
			gel_output_string (gelo,")");
			break;
		case E_EQUALS:
			append_binaryoper(gelo,"=",n); break;
		case E_PARAMETER:
			GET_LRR(n,l,r,rr);
			if (l->type != NULL_NODE) {
				gel_output_string(gelo,"(parameter (");
				gel_print_etree (gelo, l, FALSE);
				gel_output_string(gelo,") ");
				gel_print_etree (gelo, r, FALSE);
				gel_output_string(gelo," = ");
				gel_print_etree (gelo, rr, FALSE);
				gel_output_string(gelo,")");
			} else {
				gel_output_string(gelo,"(parameter ");
				gel_print_etree (gelo, r, FALSE);
				gel_output_string(gelo," = ");
				gel_print_etree (gelo, rr, FALSE);
				gel_output_string(gelo,")");
			}
			break;
		case E_ABS:
			GET_L(n,l);
			gel_output_string(gelo,"|");
			gel_print_etree(gelo, l, FALSE);
			gel_output_string(gelo,"|");
			break;
		case E_PLUS:
			append_binaryoper(gelo,"+",n); break;
		case E_MINUS:
			append_binaryoper(gelo,"-",n); break;
		case E_MUL:
			append_binaryoper(gelo,"*",n); break;
		case E_ELTMUL:
			append_binaryoper(gelo,".*",n); break;
		case E_DIV:
			append_binaryoper(gelo,"/",n); break;
		case E_ELTDIV:
			append_binaryoper(gelo,"./",n); break;
		case E_BACK_DIV:
			if(n->op.args->type == VALUE_NODE &&
			   n->op.args->any.next->type == VALUE_NODE)
				append_binaryoper(gelo," \\ ",n);
			else
				append_binaryoper(gelo,"\\",n);
			break;
		case E_ELT_BACK_DIV:
			if(n->op.args->type == VALUE_NODE &&
			   n->op.args->any.next->type == VALUE_NODE)
				append_binaryoper(gelo," .\\ ",n);
			else
				append_binaryoper(gelo,".\\",n);
			break;
		case E_MOD:
			append_binaryoper(gelo,"%",n); break;
		case E_ELTMOD:
			append_binaryoper(gelo,".%",n); break;
		case E_NEG:
			append_unaryoper(gelo,"-",n); break;
		case E_EXP:
			append_anal_binaryoper(gelo,"^",n); break;
		case E_ELTEXP:
			append_anal_binaryoper(gelo,".^",n); break;
		case E_FACT:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			if (l->type == VALUE_NODE &&
			    (mpw_is_complex (l->val.value) ||
			     mpw_sgn (l->val.value) < 0 ||
			     mpw_is_rational (l->val.value))) {
				gel_output_string(gelo, "(");
				gel_print_etree (gelo, l, FALSE);
				gel_output_string(gelo, ")");
			} else {
				gel_print_etree (gelo, l, FALSE);
			}
			gel_output_string(gelo, "!)");
			break;
		case E_DBLFACT:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			if (l->type == VALUE_NODE &&
			    (mpw_is_complex (l->val.value) ||
			     mpw_sgn (l->val.value) < 0 ||
			     mpw_is_rational (l->val.value))) {
				gel_output_string(gelo, "(");
				gel_print_etree (gelo, l, FALSE);
				gel_output_string(gelo, ")");
			} else {
				gel_print_etree (gelo, l, FALSE);
			}
			gel_output_string(gelo, "!!)");
			break;

		case E_TRANSPOSE:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo, ".')");
			break;
			
		case E_CONJUGATE_TRANSPOSE:
			GET_L(n,l);
			gel_output_string(gelo, "(");
			gel_print_etree (gelo, l, FALSE);
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
			append_binaryoper(gelo,":",n); break;

		case E_REGION_SEP_BY:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo,":");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,":");
			gel_print_etree (gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;

		case E_GET_VELEMENT:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo,"@(");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,"))");
			break;

		case E_GET_ELEMENT:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo,"@(");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,",");
			gel_print_etree (gelo, rr, FALSE);
			gel_output_string(gelo,"))");
			break;
		case E_GET_ROW_REGION:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo,"@[");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,",])");
			break;
		case E_GET_COL_REGION:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo,"@[,");
			gel_print_etree (gelo, r, FALSE);
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
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo," then ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_IFELSE_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(if ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo," then ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo," else ");
			gel_print_etree (gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_WHILE_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(while ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string (gelo," do ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_UNTIL_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo, "(until ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo, " do ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo, ")");
			break;
		case E_DOWHILE_CONS:
			GET_LR(n,l,r);
			gel_output_string (gelo, "(do ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string (gelo, " while ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_DOUNTIL_CONS:
			GET_LR(n,l,r);
			gel_output_string(gelo,"(do ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo," until ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_FOR_CONS:
			GET_ABCD (n, a, b, c, d);
			gel_output_string (gelo, "(for ");
			gel_print_etree (gelo, a, FALSE);
			gel_output_string (gelo, " = ");
			gel_print_etree (gelo, b, FALSE);
			gel_output_string (gelo, " to ");
			gel_print_etree (gelo, c, FALSE);
			gel_output_string (gelo, " do ");
			gel_print_etree (gelo, d, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_FORBY_CONS:
			GET_ABCDE (n, a, b, c, d, e);
			gel_output_string (gelo, "(for ");
			gel_print_etree (gelo, a, FALSE);
			gel_output_string (gelo, " = ");
			gel_print_etree (gelo, b, FALSE);
			gel_output_string (gelo, " to ");
			gel_print_etree (gelo, c, FALSE);
			gel_output_string (gelo, " by ");
			gel_print_etree (gelo, d, FALSE);
			gel_output_string (gelo, " do ");
			gel_print_etree (gelo, e, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_FORIN_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(for ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo," in ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo," do ");
			gel_print_etree (gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_SUM_CONS:
			GET_ABCD (n, a, b, c, d);
			gel_output_string (gelo, "(sum ");
			gel_print_etree (gelo, a, FALSE);
			gel_output_string (gelo, " = ");
			gel_print_etree (gelo, b, FALSE);
			gel_output_string (gelo, " to ");
			gel_print_etree (gelo, c, FALSE);
			gel_output_string (gelo, " do ");
			gel_print_etree (gelo, d, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_SUMBY_CONS:
			GET_ABCDE (n, a, b, c, d, e);
			gel_output_string (gelo, "(sum ");
			gel_print_etree (gelo, a, FALSE);
			gel_output_string (gelo, " = ");
			gel_print_etree (gelo, b, FALSE);
			gel_output_string (gelo, " to ");
			gel_print_etree (gelo, c, FALSE);
			gel_output_string (gelo, " by ");
			gel_print_etree (gelo, d, FALSE);
			gel_output_string (gelo, " do ");
			gel_print_etree (gelo, e, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_SUMIN_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(sum ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo," in ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo," do ");
			gel_print_etree (gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;
		case E_PROD_CONS:
			GET_ABCD(n,a,b,c,d);
			gel_output_string (gelo, "(prod ");
			gel_print_etree (gelo, a, FALSE);
			gel_output_string (gelo, " = ");
			gel_print_etree (gelo, b, FALSE);
			gel_output_string (gelo, " to ");
			gel_print_etree (gelo, c, FALSE);
			gel_output_string (gelo, " do ");
			gel_print_etree (gelo, d, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_PRODBY_CONS:
			GET_ABCDE (n, a, b, c, d, e);
			gel_output_string (gelo, "(prod ");
			gel_print_etree (gelo, a, FALSE);
			gel_output_string (gelo, " = ");
			gel_print_etree (gelo, b, FALSE);
			gel_output_string (gelo, " to ");
			gel_print_etree (gelo, c, FALSE);
			gel_output_string (gelo, " by ");
			gel_print_etree (gelo, d, FALSE);
			gel_output_string (gelo, " do ");
			gel_print_etree (gelo, e, FALSE);
			gel_output_string (gelo, ")");
			break;
		case E_PRODIN_CONS:
			GET_LRR(n,l,r,rr);
			gel_output_string(gelo,"(prod ");
			gel_print_etree (gelo, l, FALSE);
			gel_output_string(gelo," in ");
			gel_print_etree (gelo, r, FALSE);
			gel_output_string(gelo," do ");
			gel_print_etree (gelo, rr, FALSE);
			gel_output_string(gelo,")");
			break;

		case E_DIRECTCALL:
		case E_CALL:
			GET_L(n,l);
			if (l->type==IDENTIFIER_NODE) {
				gel_output_string (gelo, l->id.id->token);
			} else if (l->type == FUNCTION_NODE &&
				   l->func.func->id != NULL) {
				gel_output_string (gelo, l->func.func->id->token);
			} else if(l->type == OPERATOR_NODE && l->op.oper == E_DEREFERENCE) {
				GelETree *t;
				GET_L(l,t);
				if G_UNLIKELY (t->type!=IDENTIFIER_NODE) {
					gel_errorout (_("Bad identifier for function node!"));
					gel_output_string(gelo,"?)");
					break;
				}
				gel_output_string(gelo,"*");
				gel_output_string(gelo,t->id.id->token);
			} else {
				gel_print_etree (gelo, l, FALSE);
				gel_output_string (gelo, " call ");
			}
			gel_output_string(gelo,"(");
			li = n->op.args->any.next;
			if(li) {
				gel_print_etree (gelo, li, FALSE);
				li=li->any.next;
			}
			for(;li!=NULL;li=li->any.next) {
				gel_output_string(gelo,",");
				gel_print_etree (gelo, li, FALSE);
			}
			gel_output_string(gelo,")");
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
		case E_DEFEQUALS:
			append_binaryoper (gelo, ":=", n); break;

		default:
			gel_errorout (_("Unexpected operator!"));
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
		gel_print_etree (gelo, li, FALSE);
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
	gel_print_etree (gelo, li, FALSE);

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
		gel_print_etree (gelo,
				 gel_matrixw_index (m, i, 0),
				 FALSE);
		for(j = 1; j < gel_matrixw_height (m); j++) {
			gel_output_string (gelo, " above ");
			gel_print_etree (gelo,
					 gel_matrixw_index (m, i, j),
					 FALSE);
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
	gel_output_string (gelo, "\\left[ \\begin{array}{");
	for (i = 0; i < gel_matrixw_width (m); i++)
		gel_output_string (gelo, "r");
	gel_output_string (gelo, "}");
	if (nice)
		gel_output_string (gelo, "\n");
	else
		gel_output_string (gelo, " ");
	
	for (j = 0; j < gel_matrixw_height (m); j++) {
		gel_print_etree (gelo,
				 gel_matrixw_index (m, 0, j),
				 FALSE);
		for(i = 1; i < gel_matrixw_width (m); i++) {
			gel_output_string (gelo, " & ");
			gel_print_etree (gelo,
					 gel_matrixw_index (m, i, j),
					 FALSE);
		}
		gel_output_string (gelo, " \\\\");
		if (nice)
			gel_output_string (gelo, "\n");
		else
			gel_output_string (gelo, " ");
	}
	
	gel_output_string (gelo, "\\end{array} \\right]");
}

static void
appendmatrix_mathml (GelOutput *gelo, GelMatrixW *m, gboolean nice)
{
	/* FIXME: This produces content MathML with all expressions marked
	 * as content numbers, that is wrong */
	int i, j;
	if (nice)
		gel_output_string (gelo, "\n<matrix>\n");
	else
		gel_output_string (gelo, "<matrix>");
	
	for (j = 0; j < gel_matrixw_height (m); j++) {
		if (nice)
			gel_output_string (gelo, " ");
		gel_output_string (gelo, "<matrixrow>");
		gel_print_etree (gelo,
				 gel_matrixw_index (m, 0, j),
				 FALSE);
		for(i = 1; i < gel_matrixw_width (m); i++) {
			//gel_output_string (gelo, "</cn><cn>");
			gel_print_etree (gelo,
					 gel_matrixw_index (m, i, j),
					 FALSE);
		}
		gel_output_string (gelo, "</matrixrow>");
		if (nice)
			gel_output_string (gelo, "\n");
	}
	
	if (nice)
		gel_output_string (gelo, "</matrix>");
	else
		gel_output_string (gelo, "</matrix>");
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
	} else if (calcstate.output_style == GEL_OUTPUT_MATHML) {
		appendmatrix_mathml (gelo, m, FALSE /* nice */);
		return;
	}

	gel_output_string(gelo,"[");
	
	gel_print_etree (gelo, gel_matrixw_index (m, 0, 0), FALSE);
	
	for(i=1;i<gel_matrixw_width(m);i++) {
		gel_output_string(gelo, ",");
		gel_print_etree (gelo, gel_matrixw_index (m, i, 0), FALSE);
	}
	for(j=1;j<gel_matrixw_height(m);j++) {
		gel_output_string(gelo, ";");
		gel_print_etree (gelo,
				 gel_matrixw_index(m,0,j),
				 FALSE);
		for(i=1;i<gel_matrixw_width(m);i++) {
			gel_output_string(gelo, ",");
			gel_print_etree (gelo,
					 gel_matrixw_index (m, i, j),
					 FALSE);
		}
	}

	gel_output_string(gelo,"]");
}

static void
appendpolynomial (GelOutput *gelo, GelETree *n)
{
	char vars[] = "xyz";
	int index[3] = {0,0,0};
	int stride;
	gboolean first = TRUE;

	if G_UNLIKELY (n->poly.vars > 3) {
		/* FIXME: */
		gel_errorout (_("Cannot currently print polynomials of "
				"more than 3 vars"));
		gel_output_string (gelo, "(?)");
		return;
	}

	stride = n->poly.largest+1;

	for (;;) {
		int i = index[0] + (index[1]*stride) + (index[2]*stride*stride);
		int j;
		if (n->poly.indexes[i] != NULL &&
		    mpw_sgn (n->poly.indexes[i]) != 0) {
			char *p;
			gboolean firstvar;

			if ( ! first)
				gel_output_string (gelo, " + ");
			first = FALSE;

			p = mpw_getstring (n->poly.indexes[i],
					   calcstate.max_digits,
					   calcstate.scientific_notation,
					   calcstate.results_as_floats,
					   calcstate.mixed_fractions,
					   calcstate.output_style,
					   calcstate.integer_output_base,
					   TRUE /* add parenths */);
			gel_output_string (gelo, p);
			g_free (p);

			if (i != 0) {
				gel_output_string (gelo, " * ");

				firstvar = TRUE;
				for (j = 0; j < n->poly.vars; j++) {
					if (index[j] == 0)
						continue;

					if ( ! firstvar)
						gel_output_string (gelo, "*");
					firstvar = FALSE;

					if (index[j] == 1)
						gel_output_printf (gelo,
								   "%c",
								   vars[j]);
					else
						gel_output_printf (gelo,
								   "%c^%d",
								   vars[j],
								   index[j]);
				}
			}
		}
		j = 0;
		while (index[j] >= stride) {
			index[j] = 0;
			j++;
			if (j >= n->poly.vars)
				break;
		}
		index[j]++;
	}
}

/*make a string representation of an expression*/
void
gel_print_etree (GelOutput *gelo,
		 GelETree *n,
		 gboolean toplevel)
{
	char *p;

	if G_UNLIKELY (n == NULL) {
		gel_errorout (_("NULL tree!"));
		gel_output_string (gelo, "(?)");
		return;
	}

	gel_output_push_nonotify (gelo);

	/* all non-value nodes printed as <ci></ci> and
	 * value nodes as <cn></cn> */
	if (calcstate.output_style == GEL_OUTPUT_MATHML &&
	    n->type != VALUE_NODE)
		gel_output_string (gelo, "<ci>");


	switch(n->type) {
	case NULL_NODE:
		gel_output_string (gelo, "(null)");
		break;
	case VALUE_NODE:
		p=mpw_getstring(n->val.value,calcstate.max_digits,
				calcstate.scientific_notation,
				calcstate.results_as_floats,
				calcstate.mixed_fractions,
				calcstate.output_style,
				calcstate.integer_output_base,
				! toplevel /* add parenths */);
#if 0
		/* should we print the full number at toplevel ...??? no,
		 * I don't think so .... */
		/*if at toplevel, then always print the full number*/
		if(toplevel)
			gel_output_full_string(gelo,p);
#endif
		if (calcstate.output_style == GEL_OUTPUT_MATHML)
			gel_output_string (gelo, "<cn>");
		gel_output_string(gelo,p);
		if (calcstate.output_style == GEL_OUTPUT_MATHML)
			gel_output_string (gelo, "</cn>");
		g_free(p);
		break;
	case MATRIX_NODE:
		if (calcstate.output_style != GEL_OUTPUT_TROFF &&
		    calcstate.output_style != GEL_OUTPUT_LATEX &&
		    calcstate.output_style != GEL_OUTPUT_MATHML &&
		    n->mat.quoted)
			gel_output_string (gelo, "`");
		appendmatrix (gelo, n->mat.matrix);
		break;
	case SET_NODE:
		/* FIXME: not implemented */
		g_assert_not_reached ();
		break;
	case POLYNOMIAL_NODE:
		appendpolynomial (gelo, n);
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
			if G_UNLIKELY (f == NULL) {
				gel_errorout (_("NULL function!"));
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

			if (f->vararg)
				gel_output_string (gelo, "...");

			if G_LIKELY (f->type==GEL_USER_FUNC) {
				gel_output_string(gelo,")=(");
				D_ENSURE_USER_BODY (f);
				gel_print_etree (gelo, f->data.user, FALSE);
				gel_output_string(gelo,"))");
			} else {
				/*variable and reference functions should
				  never be in the etree*/
				gel_errorout (_("Unexpected function type!"));
				gel_output_string(gelo,")(?))");
			}
			break;
		}
	case COMPARISON_NODE:
		appendcomp(gelo,n);
		break;
	case BOOL_NODE:
		if (n->bool_.bool_)
			gel_output_string (gelo, "true");
		else
			gel_output_string (gelo, "false");
		break;
	default:
		gel_errorout (_("Unexpected node!"));
		gel_output_string(gelo,"(?)");
	       break;
	}
	/* all non-value nodes printed as <ci></ci> and
	 * value nodes as <cn></cn> */
	if (calcstate.output_style == GEL_OUTPUT_MATHML &&
	    n->type != VALUE_NODE)
		gel_output_string (gelo, "</ci>");

	gel_output_pop_nonotify (gelo);
}

static int
strlen_max (const char *s, int m)
{
	int i;
	for (i = 0; i < m && s[i] != '\0'; i++)
		;
	return i;
}

static char *
make_sep (int len)
{
	char sep[] = "----------------------------------------------------------------------------------";
	sep[MIN(80,len)] = '\0';
	return g_strdup (sep);
}

static char *
make_empty (int len)
{
	char sep[] = "                                                                                 ";
	sep[MIN(80,len)] = '\0';
	return g_strdup (sep);
}

static void
pretty_print_value_normal (GelOutput *gelo, GelETree *n)
{
	int columns = gel_output_get_columns (gelo);

	/* FIXME: what about mixed_fractions, what about rational
	   complex values, etc... */
	if ( ! mpw_is_complex (n->val.value) &&
	    mpw_is_rational (n->val.value)) {
		int lend, lenn, lenw;
		mpw_t num, den, whole;
		mpz_ptr z;
		char *nums, *dens, *wholes, *sep;

		mpw_init (num);
		mpw_init (den);
		mpw_init (whole);

		mpw_numerator (num, n->val.value);
		mpw_denominator (den, n->val.value);

		z = mpw_peek_real_mpz (den);
		if (z == NULL ||
		    mpz_sizeinbase (z, calcstate.integer_output_base) - 1
		      > calcstate.max_digits) {
			mpw_clear (den);
			mpw_clear (num);
			mpw_clear (whole);
			goto just_print_a_number;
		}

		if (calcstate.mixed_fractions &&
		    mpw_cmp (num, den) > 0) {
			mpz_t quot;
			mpz_t rem;
			mpz_ptr denz;

			mpz_init (quot);
			mpz_init (rem);

			z = mpw_peek_real_mpz (num);
			denz = mpw_peek_real_mpz (den);

			mpz_tdiv_qr (quot, rem, z, denz);

			mpw_set_mpz_use (whole, quot);
			mpw_set_mpz_use (num, rem);

			z = mpw_peek_real_mpz (whole);
			if (z == NULL ||
			    mpz_sizeinbase (z, calcstate.integer_output_base) - 1
			    > calcstate.max_digits) {
				mpw_clear (den);
				mpw_clear (num);
				mpw_clear (whole);
				goto just_print_a_number;
			}

			wholes = mpw_getstring (whole,
						0 /* calcstate.max_digits */,
						calcstate.scientific_notation,
						calcstate.results_as_floats,
						calcstate.mixed_fractions,
						calcstate.output_style,
						calcstate.integer_output_base,
						FALSE);
			lenw = strlen_max (wholes, columns);
		} else {
			wholes = NULL;
			lenw = -1;
		}

		z = mpw_peek_real_mpz (num);
		if (z == NULL ||
		    mpz_sizeinbase (z, calcstate.integer_output_base) - 1
		      > calcstate.max_digits) {
			mpw_clear (den);
			mpw_clear (num);
			mpw_clear (whole);
			goto just_print_a_number;
		}

		/* we can possibly be one off with max_digits,
		   since sizeinbase can be 1 off, but that's ok */

		nums = mpw_getstring (num,
				      0 /* calcstate.max_digits */,
				      calcstate.scientific_notation,
				      calcstate.results_as_floats,
				      calcstate.mixed_fractions,
				      calcstate.output_style,
				      calcstate.integer_output_base,
				      FALSE);
		lenn = strlen_max (nums, columns);
		dens = mpw_getstring (den,
				      0 /* calcstate.max_digits */,
				      calcstate.scientific_notation,
				      calcstate.results_as_floats,
				      calcstate.mixed_fractions,
				      calcstate.output_style,
				      calcstate.integer_output_base,
				      FALSE);
		lend = strlen_max (dens, columns);

		if ((lenn + 1 + lenw) >= columns ||
		    (lend + 1 + lenw) >= columns ||
		    (lenn <= 2 && lend <= 2)) {
			mpw_clear (den);
			mpw_clear (num);
			mpw_clear (whole);
			g_free (dens);
			g_free (nums);
			g_free (wholes);
			goto just_print_a_number;
		}

		gel_output_string (gelo, "\n");

		sep = make_sep (MAX (lend, lenn));

		if (lenw > 0) {
			char *spacer = make_empty (lenw+1);
			gel_output_string (gelo, spacer);
			gel_output_string (gelo, nums);
			gel_output_string (gelo, "\n");
			gel_output_string (gelo, wholes);
			gel_output_string (gelo, " ");
			gel_output_string (gelo, sep);
			gel_output_string (gelo, "\n");
			gel_output_string (gelo, spacer);
			gel_output_string (gelo, dens);
			g_free (spacer);
		} else {
			gel_output_string (gelo, "\n");
			gel_output_string (gelo, nums);
			gel_output_string (gelo, "\n");
			gel_output_string (gelo, sep);
			gel_output_string (gelo, "\n");
			gel_output_string (gelo, dens);
		}
		g_free (sep);
		g_free (nums);
		g_free (dens);
		g_free (wholes);

		mpw_clear (den);
		mpw_clear (num);
		mpw_clear (whole);

		return;
	}

just_print_a_number:
	gel_print_etree (gelo, n, TRUE);
}

void
gel_pretty_print_etree (GelOutput *gelo, GelETree *n)
{
	/*do a nice printout of matrices if that's the
	  top node*/
	gel_output_push_nonotify (gelo);
	if (n->type == VALUE_NODE &&
	    calcstate.output_style == GEL_OUTPUT_NORMAL) {
		pretty_print_value_normal (gelo, n);
	} else if (n->type == MATRIX_NODE) {
		int i,j;

		if (calcstate.output_style == GEL_OUTPUT_TROFF) {
			appendmatrix_troff (gelo, n->mat.matrix, TRUE /* nice */);
			gel_output_pop_nonotify (gelo);
			return;
		} else if (calcstate.output_style == GEL_OUTPUT_LATEX) {
			appendmatrix_latex (gelo, n->mat.matrix, TRUE /* nice */);
			gel_output_pop_nonotify (gelo);
			return;
		} else if (calcstate.output_style == GEL_OUTPUT_MATHML) {
			gel_output_string (gelo, "\n<math>");
			appendmatrix_mathml (gelo, n->mat.matrix, TRUE /* nice */);
			gel_output_string (gelo, "\n</math>");
			gel_output_pop_nonotify (gelo);
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
				if (i > 0)
					gel_output_string(gelo, "\t");
				gel_print_etree (gelo,
						 gel_matrixw_index
						    (n->mat.matrix, i, j),
						 FALSE);
			}
		}
		gel_output_string(gelo, "]");
	} else {
		if (calcstate.output_style == GEL_OUTPUT_MATHML)
			gel_output_string (gelo, "\n<math>\n ");
		gel_print_etree (gelo, n, TRUE);
		if (calcstate.output_style == GEL_OUTPUT_MATHML)
			gel_output_string (gelo, "\n</math>");

	}
	gel_output_pop_nonotify (gelo);
}

/*make a string representation of an expression*/
char *
gel_string_print_etree (GelETree *n)
{
	GelOutput *gelo = gel_output_new();
	char *ret;
	gel_output_setup_string(gelo, 0, NULL);

	gel_print_etree (gelo, n, TRUE);

	ret = gel_output_snarf_string(gelo);

	gel_output_unref(gelo);

	if(!ret)
		return g_strdup("");
	else
		return ret;
}

static void
compile_funcs_in_dict (FILE *outfile, GSList *dict, gboolean is_extra_dict)
{
	GSList *li, *l;
	char fs, vs;

	if (is_extra_dict) {
		fs = 'f';
		vs = 'v';
	} else {
		fs = 'F';
		vs = 'V';
	}

	for (li = dict; li != NULL; li = li->next) {
		GelEFunc *func = li->data;
		GelHelp *help;
		char *body;

		if ((func->type != GEL_USER_FUNC &&
		     func->type != GEL_VARIABLE_FUNC) ||
		    func->id == NULL ||
		    func->id->token == NULL ||
		    (func->id->parameter &&
		     func->id->built_in_parameter) ||
		    ( ! is_extra_dict && strcmp (func->id->token, "Ans") == 0))
			continue;

		if (func->data.user) {
			body = gel_compile_tree(func->data.user);
		} else {
			body = g_strdup (g_hash_table_lookup (uncompiled,
							      func->id));
			g_assert (body != NULL);
		}
		if (func->type == GEL_USER_FUNC) {
			fprintf (outfile,
				 "%c;%d;%s;%d;%d",
				 fs,
				 (int)strlen (body),
				 func->id->token,
				 (int)func->nargs,
				 (int)func->vararg);
			for (l = func->named_args; l != NULL; l = l->next) {
				GelToken *tok = l->data;
				fprintf (outfile, ";%s", tok->token);
			}
		} else /*GEL_VARIABLE_FUNC*/ {
			int param;
			if (is_extra_dict)
				param = 0;
			else
				param = (int)func->id->parameter;
			fprintf (outfile, "%c;%d;%s;%d",
				 vs,
				 (int)strlen(body),
				 func->id->token,
				 param);
		}

		fprintf (outfile, "\n%s\n", body);
		g_free (body);

		/* extra dict only does this so far */
		if (is_extra_dict)
			continue;

		compile_funcs_in_dict (outfile,
				       func->extra_dict,
				       TRUE /* is_extra_dict */);

		help = get_help (func->id->token, FALSE /* insert */);
		if (help != NULL && help->aliasfor != NULL) {
			fprintf (outfile, "A;%s;%s\n",
				 help->aliasfor,
				 func->id->token);
		} else if (help != NULL) {
			if (help->category != NULL)
				fprintf (outfile, "C;%s;%s\n",
					 func->id->token,
					 help->category);
			if (help->description != NULL)
				fprintf (outfile, "D;%s;%s\n",
					 func->id->token,
					 help->description);
			if (help->help_link != NULL) {
				char *s = gel_encode_string (help->help_link);
				fprintf (outfile, "L;%s;%s\n",
					 func->id->token, s);
				g_free (s);
			}
			/* FIXME: This may be too demanding */
			if (help->help_html != NULL) {
				char *s = gel_encode_string (help->help_html);
				fprintf (outfile, "H;%s;%s\n",
					 func->id->token, s);
				g_free (s);
			}
		}
		if (func->id->protected_)
			fprintf (outfile,"P;%s\n",func->id->token);
	}
}

void
gel_compile_all_user_funcs (FILE *outfile)
{
	GSList *funcs;
	fprintf (outfile, "CGEL "VERSION"\n");
	funcs = d_getcontext();
	if (funcs == NULL)
		return;
	funcs = g_slist_reverse (g_slist_copy (funcs));
	compile_funcs_in_dict (outfile, funcs, FALSE /* is_extra_dict */);
	g_slist_free (funcs);
}

static void
load_compiled_fp (const char *file, FILE *fp)
{
	char *buf;
	int buf_size = 4096;
	gboolean break_on_next = FALSE;
	GelEFunc *last_func = NULL;

	buf = g_new (char, buf_size);

	if G_UNLIKELY (fgets (buf, buf_size, fp) == NULL) {
		g_free (buf);
		return;
	}
	/* compiled files are not in general compatible accross versions */
	if G_UNLIKELY (strcmp (buf, "CGEL "VERSION"\n") != 0) {
		g_free (buf);
		gel_errorout (_("File '%s' is a wrong version of GEL"), file);
		return;
	}

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext ();

	error_num = NO_ERROR;

	/*if we this was set, then the mp library was initialized for
	  sure*/
	g_assert (calcstate.float_prec > 0);

	while ( ! break_on_next && fgets (buf, buf_size, fp) != NULL) {
		char *p;
		char *b2;
		GelToken *tok;
		int size, nargs, vararg;
		gboolean extra_dict = FALSE;
		gboolean parameter = FALSE;
		int i;
		GSList *li = NULL;
		int type;

		gel_incr_file_info();

		for (;;) {
			int len;
			p = strchr (buf,'\n');
			if (p != NULL) {
				*p = '\0';
				break;
			}
			buf_size *= 2;
			len = strlen (buf);
			buf = g_realloc (buf, buf_size);
			if (fgets (buf+len, buf_size-len, fp) == NULL) {
				break_on_next = TRUE;
				break;
			}
		}

		p = strtok(buf,";");
		if G_UNLIKELY (!p) {
			gel_errorout (_("Badly formed record"));
			continue;
		} else if G_UNLIKELY (*p == 'T') {
			gel_errorout (_("Record out of place"));
			continue;
		} else if (*p == 'A') {
			char *d;
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			d = strtok(NULL,";");
			if G_UNLIKELY (!d) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			add_alias(p,d);
			continue;
		} else if (*p == 'C') {
			char *d;
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			d = strtok(NULL,";");
			if G_UNLIKELY (!d) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			add_category(p,d);
			continue;
		} else if (*p == 'D') {
			char *d;
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			d = strtok(NULL,";");
			if G_UNLIKELY (!d) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			add_description(p,d);
			continue;
		} else if (*p == 'L') {
			char *d, *h;
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			d = strtok(NULL,";");
			if G_UNLIKELY (!d) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			h = gel_decode_string (d);
			add_help_link (p, h);
			g_free (h);
			continue;
		} else if (*p == 'H') {
			char *d, *h;
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			d = strtok(NULL,";");
			if G_UNLIKELY (!d) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			h = gel_decode_string (d);
			add_help_html (p, h);
			g_free (h);
			continue;
		} else if (*p == 'P') {
			GelToken *tok;
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			tok = d_intern(p);
			tok->protected_ = 1;
			continue;
		} else if G_UNLIKELY (*p != 'F' && *p != 'V' && *p != 'f' && *p != 'v') {
			gel_errorout (_("Badly formed record"));
			continue;
		}
		type = (*p == 'F' || *p == 'f') ? GEL_USER_FUNC : GEL_VARIABLE_FUNC;

		if (*p == 'f' || *p == 'v')
			extra_dict = TRUE;
		else
			extra_dict = FALSE;

		/*size*/
		p = strtok(NULL,";");
		if G_UNLIKELY (!p) {
			gel_errorout (_("Badly formed record"));
			continue;
		}
		size = -1;
		sscanf(p,"%d",&size);
		if G_UNLIKELY (size==-1) {
			gel_errorout (_("Badly formed record"));
			continue;
		}

		/*id*/
		p = strtok(NULL,";");
		if G_UNLIKELY (!p) {
			gel_errorout (_("Badly formed record"));
			continue;
		}
		tok = d_intern(p);

		if (type == GEL_USER_FUNC) {
			/*nargs*/
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			nargs = -1;
			sscanf(p,"%d",&nargs);
			if G_UNLIKELY (nargs == -1) {
				gel_errorout (_("Badly formed record"));
				continue;
			}

			/*vararg*/
			p = strtok(NULL,";");
			if (p == NULL) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			vararg = -1;
			sscanf(p,"%d",&vararg);
			if G_UNLIKELY (vararg == -1) {
				gel_errorout (_("Badly formed record"));
				continue;
			}

			/*argument names*/
			li = NULL;
			for(i=0;i<nargs;i++) {
				p = strtok(NULL,";");
				if G_UNLIKELY (p == NULL) {
					gel_errorout (_("Badly formed record"));
					g_slist_free(li);
					goto continue_reading;
				}
				li = g_slist_append(li,d_intern(p));
			}
		} else {
			/*parameter*/
			p = strtok(NULL,";");
			if G_UNLIKELY (!p) {
				gel_errorout (_("Badly formed record"));
				continue;
			}
			parameter = -1;
			sscanf(p,"%d",&parameter);

			/* FIXME: should this be an iff? */
			if (parameter)
				tok->parameter = 1;
		}


		/*the value*/
		b2 = g_new(char,size+2);
		if G_UNLIKELY (!fgets(b2,size+2,fp)) {
			gel_errorout (_("Missing value for function"));
			g_free(b2);
			g_slist_free(li);
			goto continue_reading;
		}
		gel_incr_file_info();
		p=strchr(b2,'\n');
		if(p) *p='\0';

		if (extra_dict) {
			GelEFunc *func;
			if (type == GEL_USER_FUNC) {
				func = d_makeufunc (tok, NULL, li, nargs, NULL);
				func->vararg = vararg ? 1 : 0;
			} else /*GEL_VARIABLE_FUNC*/ {
				func = d_makevfunc (tok, NULL);
			}
			func->context = -1;
			if G_UNLIKELY (last_func == NULL)
				gel_errorout (_("Extra dictionary for NULL function"));
			else
				last_func->extra_dict = g_slist_append
					(last_func->extra_dict, func);
		} else {
			GelEFunc *func;
			if(!uncompiled)
				uncompiled = g_hash_table_new(NULL,NULL);
			g_hash_table_insert(uncompiled,tok,b2);
			if(type == GEL_USER_FUNC) {
				func = d_makeufunc (tok, NULL, li, nargs, NULL);
				func->vararg = vararg ? 1 : 0;
			} else /*GEL_VARIABLE_FUNC*/ {
				func = d_makevfunc(tok,NULL);
			}
			last_func = func;
			d_addfunc (func);
		}
continue_reading:	;
	}
	fclose(fp);
	g_free (buf);
}

void
gel_load_compiled_file (const char *dirprefix, const char *file, gboolean warn)
{
	FILE *fp;
	char *newfile;
	if(dirprefix && file[0]!='/')
		newfile = g_strconcat(dirprefix, "/", file, NULL);
	else
		newfile = g_strdup (file);

	fp = fopen (newfile, "r");
	if (fp != NULL) {
		gel_push_file_info (newfile, 1);
		load_compiled_fp (newfile, fp);
		gel_pop_file_info ();
	} else if G_UNLIKELY (warn) {
		gel_errorout (_("Can't open file: '%s'"), newfile);
	}
	g_free (newfile);
}

static void
do_cyan (void)
{
	if (genius_is_gui) {
		gel_output_full_string (main_out, "\e[1;36m");
	}
}

static void
do_blue (void)
{
	if (genius_is_gui) {
		gel_output_full_string (main_out, "\e[1;34m");
	}
}

static void
do_green (void)
{
	if (genius_is_gui) {
		gel_output_full_string (main_out, "\e[0;32m");
	}
}

/*
static void
do_red (void)
{
	if (genius_is_gui) {
		gel_output_full_string (main_out, "\e[01;31m");
	}
}
*/

static void
do_black (void)
{
	if (genius_is_gui) {
		gel_output_full_string (main_out, "\e[0m");
	}
}

static char *
make_function_with_aliases (const char *func, GSList *aliases)
{
	GSList *li;
	GString *gs = g_string_new (func);
	for (li = aliases; li != NULL; li = li->next) {
		g_string_append (gs, ",");
		g_string_append (gs, li->data);
	}
	return g_string_free (gs, FALSE);
}

static void
print_description (int start, const char *desc)
{
	int ll = gel_output_get_columns (main_out) - start - 3;
	char **words;
	int i;
	int cur;

	if (ll <= 5) {
		gel_output_printf_full (main_out, FALSE,
					"%s\n", desc);
		return;
	}

	words = g_strsplit (desc, " ", -1);
	cur = 0;
	for (i = 0; words[i] != NULL; i++) {
		int len = strlen (words[i]);
		if (cur != 0 && cur + len >= ll) {
			cur = 0;
			gel_output_full_string
				(main_out, "\n                       ");
		} else if (cur != 0) {
			gel_output_full_string (main_out, " ");
			cur++;
		}
		gel_output_full_string (main_out, words[i]);
		cur += len;
	}
	g_strfreev (words);

	gel_output_full_string (main_out, "\n");
}

static void
print_function_help (GelHelp *help)
{
	if (help->aliasfor == NULL) {
		char *f;
		int len;
		f = make_function_with_aliases (help->func, help->aliases);
		len = strlen (f);
		do_cyan ();
		/*if (len <= 20)*/
			gel_output_printf_full (main_out, FALSE,
						"%-20s", f);
		/*else
			gel_output_printf_full (main_out, FALSE,
						"%-20s", help->func);*/
		g_free (f);
		do_black ();
		gel_output_full_string (main_out, " - ");
		do_green ();
		if (help->description != NULL)
			print_description (MAX (20, len),
					   _(help->description));
		else
			gel_output_full_string (main_out, "\n");
		/* if we didn't fit aliases on one line */
		/*
		if (len > 20 && help->aliases != NULL) {
			GSList *li;
			GString *gs = g_string_new (_("Aliases for "));
			g_string_append (gs, help->func);
			g_string_append (gs, ":");
			for (li = help->aliases; li != NULL; li = li->next) {
				g_string_append (gs, " ");
				g_string_append (gs, li->data);
			}
			gel_output_printf_full (main_out, FALSE,
						"%s\n", gs->str);
			g_string_free (gs, TRUE);
		}
		*/
	}
}

static void
print_command_help (const char *cmd)
{
	do_cyan ();
	gel_output_printf_full (main_out, FALSE, "%-20s", cmd);
	do_black ();
	gel_output_full_string (main_out, " - ");
	do_green ();

	if (strcmp (cmd, "load") == 0) {
		gel_output_full_string (main_out,
					_("Load a file into the interpretor"));
	} else if (strcmp (cmd, "plugin") == 0) {
		gel_output_full_string (main_out,
					_("Load a plugin"));
	} else if (strcmp (cmd, "ls") == 0) {
		gel_output_full_string (main_out,
					_("List files in the current directory"));
	} else if (strcmp (cmd, "cd") == 0) {
		gel_output_full_string (main_out,
					_("Change directory"));
	} else if (strcmp (cmd, "pwd") == 0) {
		gel_output_full_string (main_out,
					_("Print current directory"));
	} else if (strcmp (cmd, "help") == 0) {
		gel_output_full_string (main_out,
					_("Print help (or help on a function/command)"));
	}
	gel_output_full_string (main_out, "\n");
}

static void
full_help (void)
{
	GSList *categories = get_categories ();
	GSList *functions;
	GSList *cli, *fli;
	int i;

	gel_output_push_nonotify (main_out);

	do_green ();
	gel_output_full_string (main_out,
				_("\nFor a manual on using Genius and the GEL language type:\n"));
	do_black ();
	gel_output_full_string (main_out, _("  manual\n"));

	do_green ();
	gel_output_full_string (main_out,
				_("\nFor help on a specific function type:\n"));
	do_black ();
	gel_output_full_string (main_out, _("  help FunctionName\n"));

	do_black ();
	gel_output_full_string (main_out,
				_("\nCommands:\n"));
	for (i = 0; genius_toplevels[i] != NULL; i++)
		print_command_help (genius_toplevels[i]);

	for (cli = categories; cli != NULL; cli = cli->next) {
		char *cat = cli->data;
		functions = get_helps (cat);

		if (functions != NULL) {
			do_black ();
			gel_output_printf_full (main_out, FALSE, "\n%s:\n",
						get_category_name (cat));

			for (fli = functions; fli != NULL; fli = fli->next) {
				GelHelp *help = fli->data;
				print_function_help (help);
			}

			g_slist_free (functions);
		}

		g_free (cat);
	}
	g_slist_free (categories);

	functions = get_helps (NULL);
	if (functions != NULL) {
		do_black ();
		gel_output_printf_full (main_out, FALSE, "\n%s:\n",
					get_category_name (NULL));

		for (fli = functions; fli != NULL; fli = fli->next) {
			GelHelp *help = fli->data;
			print_function_help (help);
		}

		g_slist_free (functions);
	}

	functions = get_undocumented ();
	if (functions != NULL) {
		GString *gs = g_string_new (NULL);
		int len = 0;
		int line_len = gel_output_get_columns (main_out);

		do_black ();

		gel_output_full_string (main_out,
					_("\nUndocumented:\n"));
		do_cyan ();

		for (fli = functions; fli != NULL; fli = fli->next) {
			char *f = fli->data;
			int flen = strlen (f);

			if (len + flen + 1 > line_len-2 && len > 0) {
				gel_output_printf_full (main_out, FALSE, "%s\n",
							gs->str);
				g_string_truncate (gs, 0);
				len = 0;
			}
			g_string_append (gs, f);
			len += flen;
			if (fli->next != NULL) {
				g_string_append_c (gs, ',');
				len++;
			}

			g_free (f);
		}
		if (len > 0) {
			gel_output_printf_full (main_out, FALSE, "%s\n",
						gs->str);
		}
		g_string_free (gs, TRUE);

		g_slist_free (functions);
	}

	do_black ();

	gel_output_pop_nonotify (main_out);
}

static void
help_on (const char *text)
{
	GelHelp *help;
	GelHelp not_documented = { NULL /* func */,
			           NULL /* aliasfor */,
				   NULL /* category */,
				   _("Not documented"),
				   NULL /* aliases */,
				   NULL /* help_link */,
				   NULL /* help_html */ };
	GelEFunc *f;
	int i;

	gel_output_push_nonotify (main_out);

	for (i = 0; genius_toplevels[i] != NULL; i++)
		if (strcmp (text, genius_toplevels[i]) == 0) {
			print_command_help (text);
			do_black ();
			gel_output_pop_nonotify (main_out);
			return;
		}

	help = get_help (text, FALSE /*insert*/);
	if (help == NULL) {
		gel_errorout (_("'%s' is not documented"), text);
		not_documented.func = (char *)text;
		help = &not_documented;
	}

	if (help->aliasfor) {
		gel_output_printf_full (main_out, FALSE,
					_("%s is an alias for %s\n"),
					text, help->aliasfor);
		help_on (help->aliasfor);
		do_black ();
		gel_output_pop_nonotify (main_out);
		return;
	}

	do_cyan ();

	f = d_lookup_global (d_intern (text));
	if (f == NULL) {
		gel_output_printf_full (main_out, FALSE, "%s\n", text);
	} else {
		GSList *li;
		gel_output_printf_full (main_out, FALSE, "%s (", text);

		for (li = f->named_args; li != NULL; li = li->next) {
			GelToken *id = li->data;
			if (li != f->named_args)
				gel_output_full_string (main_out, ",");
			gel_output_full_string (main_out, id->token);
		}

		if (f->vararg)
			gel_output_full_string (main_out, "...");
		gel_output_full_string (main_out, ")\n");
	}
	do_green ();

	if (help->aliases != NULL) {
		GSList *li;
		GString *gs = g_string_new (_("Aliases:"));
		for (li = help->aliases; li != NULL; li = li->next) {
			g_string_append (gs, " ");
			g_string_append (gs, li->data);
		}
		gel_output_printf_full (main_out, FALSE,
					"%s\n", gs->str);
		g_string_free (gs, TRUE);
	}

	if (help->description != NULL) {
		gel_output_printf_full (main_out, FALSE,
					_("Description: %s\n"),
					_(help->description));
	}

	do_black ();
	gel_output_pop_nonotify (main_out);
}

static void
dump_a_string (FILE *outfile, const char *s)
{
	fprintf (outfile, "char *fake = N_(\"%s\");\n", s);
}

void
gel_dump_strings_from_user_funcs (FILE *outfile)
{
	/* FIXME: implement */
}

static void
dump_cat (FILE *outfile, const char *cat)
{
	GSList *functions;
	GSList *fli;

	functions = get_helps (cat);

	if (functions != NULL) {
		for (fli = functions; fli != NULL; fli = fli->next) {
			GelHelp *help = fli->data;
			GelToken *id = d_intern (help->func);
			GelEFunc *f = d_lookup_global (id);

			if (help->description != NULL &&
			    (f == NULL ||
			     f->type != GEL_BUILTIN_FUNC) &&
			    ! id->built_in_parameter) {
				dump_a_string (outfile, help->description);
			}

			fli->data = NULL;
		}

		g_slist_free (functions);
	}
}

void
gel_dump_strings_from_help (FILE *outfile)
{
	GSList *categories = get_categories ();
	GSList *cli;

	for (cli = categories; cli != NULL; cli = cli->next) {
		char *cat = cli->data;
		HelpCategory *cats;

		cats = get_category (cat, FALSE /* insert */);
		if (cats != NULL &&
		    cats->name != NULL &&
		    ! cats->internal) {
			dump_a_string (outfile, cats->name);
		}

		dump_cat (outfile, cat);

		cli->data = NULL;
		g_free (cat);
	}
	g_slist_free (categories);

	dump_cat (outfile, NULL);
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
	gel_lexer_open(fp);
	while(1) {
		gel_evalexp(NULL, fp, NULL, NULL, FALSE, dirprefix);
		if (gel_got_eof) {
			gel_got_eof = FALSE;
			break;
		}
		if(interrupted)
			break;
	}
	gel_lexer_close(fp);
	/*fclose(fp);*/
}

void
gel_load_file (const char *dirprefix, const char *file, gboolean warn)
{
	FILE *fp;
	char *newfile;
	gboolean oldgeof = gel_got_eof;
	gel_got_eof = FALSE;
	if(dirprefix && file[0]!='/')
		newfile = g_strconcat(dirprefix, "/", file, NULL);
	else
		newfile = g_strdup (file);

	if G_LIKELY ((fp = fopen(newfile,"r"))) {
		char *dir = g_path_get_dirname (newfile);
		gel_push_file_info(newfile,1);
		load_fp(fp, dir);
		gel_pop_file_info();
		g_free(dir);
		gel_got_eof = oldgeof;
	} else if G_UNLIKELY (warn) {
		gel_errorout (_("Can't open file: '%s'"),newfile);
		gel_got_eof = oldgeof;
	}
	g_free(newfile);
}

void
gel_load_guess_file (const char *dirprefix, const char *file, gboolean warn)
{
	FILE *fp;
	char *newfile;
	gboolean oldgeof = gel_got_eof;
	gel_got_eof = FALSE;
	if(dirprefix && file[0]!='/')
		newfile = g_strconcat(dirprefix, "/", file, NULL);
	else
		newfile = g_strdup (file);

	if G_LIKELY ((fp = fopen(newfile,"r"))) {
		char buf[6];
		gel_push_file_info(newfile,1);
		if(fgets(buf,6,fp) &&
		   strncmp(buf,"CGEL ",5)==0) {
			rewind(fp);
			load_compiled_fp(newfile,fp);
		} else {
			char *dir = g_path_get_dirname(newfile);
			rewind(fp);
			load_fp(fp, dir);
			g_free(dir);
		}
		gel_pop_file_info();
		gel_got_eof = oldgeof;
	} else if G_UNLIKELY (warn) {
		gel_errorout (_("Can't open file: '%s'"), newfile);
		gel_got_eof = oldgeof;
	}
	g_free (newfile);
}

void
set_new_errorout(void (*func)(const char *))
{
	errorout = func;
}

void
set_new_infoout(void (*func)(const char *))
{
	infoout = func;
}

static void
our_chdir (const char *dirprefix, const char *dir)
{
	if (dirprefix == NULL ||
	    dir[0] == '/') {
		chdir (dir);
	} else {
		char *d = g_strconcat (dirprefix, "/", dir, NULL);
		chdir (d);
		g_free (d);
	}
}

static GSList *
get_wordlist (const char *lst)
{
	GSList *list = NULL;
#if HAVE_WORDEXP
	wordexp_t we;
	int i;
	if G_UNLIKELY (wordexp (lst, &we, WRDE_NOCMD) != 0) {
		gel_errorout (_("Can't expand '%s'"), lst);
		return NULL;
	}
	for (i = 0; i < we.we_wordc; i++) {
		list = g_slist_prepend (list, g_strdup (we.we_wordv[i]));
	}
	wordfree (&we);
#else
	glob_t gl;
	int i;
	if G_UNLIKELY (glob (lst, 0, NULL, &gl) != 0) {
		gel_errorout (_("Can't expand '%s'"), lst);
		return NULL;
	}
	for (i = 0; i < gl.gl_pathc; i++) {
		list = g_slist_prepend (list, g_strdup (gl.gl_pathv[i]));
	}
	globfree (&gl);
#endif
	return list;
}


static gboolean
do_exec_commands (const char *dirprefix)
{
	GelCommand cmd = gel_command;
	char *arg = gel_command_arg;
	gboolean ret = FALSE;
	GSList *list, *li;
	DIR *dir;
	char buf[4096] = "";

	gel_command = GEL_NO_COMMAND;
	gel_command_arg = NULL;

	switch (cmd) {
	case GEL_NO_COMMAND:
		ret = FALSE;
		break;
	case GEL_LOADFILE:
		while (evalstack)
			gel_freetree (stack_pop (&evalstack));
		gel_load_file (dirprefix, arg, TRUE);
		ret = TRUE;
		break;
	case GEL_LOADFILE_GLOB:
		list = get_wordlist (arg);
		while (evalstack)
			gel_freetree (stack_pop (&evalstack));
		for (li = list; li != NULL; li = li->next) {
			gel_load_guess_file (dirprefix, li->data, TRUE);
			if (interrupted)
				break;
		}
		g_slist_foreach (list, (GFunc)g_free, NULL);
		g_slist_free (list);
		ret = TRUE;
		break;
	case GEL_CHANGEDIR:
		list = get_wordlist (arg);
		while (evalstack)
			gel_freetree (stack_pop (&evalstack));
		for (li = list; li != NULL; li = li->next) {
			our_chdir (dirprefix, li->data);
		}
		g_slist_foreach (list, (GFunc)g_free, NULL);
		g_slist_free (list);
		ret = TRUE;
		break;
	case GEL_LOADPLUGIN:
		g_strstrip (arg);

		for (li = gel_plugin_list; li != NULL; li = li->next) {
			GelPlugin *plg = li->data;
			if (strcmp (plg->base, arg)==0) {
				gel_open_plugin (plg);
				break;
			}
		}
		if G_UNLIKELY (li == NULL) {
			gel_errorout (_("Cannot open plugin '%s'!"),
				      arg);
		}
		ret = TRUE;
		break;
	case GEL_LS:
		dir = opendir (".");
		if (dir != NULL) {
			struct dirent *de;
			while ((de = readdir (dir)) != NULL) {
				struct stat s;
				if (strcmp (de->d_name, ".") == 0 ||
				    strcmp (de->d_name, "..") == 0)
					continue;
				if (stat (de->d_name, &s) == 0 &&
				    S_ISDIR (s.st_mode)) {
					if (genius_is_gui)
						do_blue ();
					gel_output_string (main_out, de->d_name);
					if (genius_is_gui)
						do_black ();

					gel_output_string (main_out, "/\n");
				}
			}
			rewinddir (dir);
			while ((de = readdir (dir)) != NULL) {
				struct stat s;
				if (strcmp (de->d_name, ".") == 0 ||
				    strcmp (de->d_name, "..") == 0)
					continue;
				if (stat (de->d_name, &s) == 0 &&
				     ! S_ISDIR (s.st_mode)) {
					char *ext = strchr (de->d_name, '.');
					if (genius_is_gui &&
					    ext != NULL &&
					    strcmp (ext, ".gel") == 0) {
						do_green ();
					}
					gel_output_string (main_out, de->d_name);
					if (genius_is_gui)
						do_black ();
					gel_output_string (main_out, "\n");
				}
			}

			closedir (dir);
		}
		ret = TRUE;
		break;
	case GEL_LS_ARG:
		list = get_wordlist (arg);

		for (li = list; li != NULL; li = li->next) {
			struct stat s;
			if (stat (li->data, &s) == 0 &&
			    S_ISDIR (s.st_mode)) {
				if (genius_is_gui)
					do_blue ();
				gel_output_string (main_out, li->data);
				if (genius_is_gui)
					do_black ();
				gel_output_string (main_out, "/\n");
			}
		}

		for (li = list; li != NULL; li = li->next) {
			struct stat s;
			if (stat (li->data, &s) == 0 &&
			    ! S_ISDIR (s.st_mode)) {
				char *ext = strchr (li->data, '.');
				if (genius_is_gui &&
				    ext != NULL &&
				    strcmp (ext, ".gel") == 0) {
					do_green ();
				}
				gel_output_string (main_out, li->data);
				if (genius_is_gui)
					do_black ();
				gel_output_string (main_out, "\n");
			}
		}

		g_slist_foreach (list, (GFunc)g_free, NULL);
		g_slist_free (list);
		ret = TRUE;
		break;
	case GEL_PWD:
		getcwd (buf, sizeof (buf));

		gel_output_string (main_out, buf);
		gel_output_string (main_out, "\n");
		ret = TRUE;
		break;
	case GEL_HELP:
		full_help ();
		ret = TRUE;
		break;
	case GEL_HELP_ARG:
		g_strstrip (arg);
		help_on (arg);
		ret = TRUE;
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	g_free (arg);
	return ret;
}

GelETree *
gel_parseexp(const char *str, FILE *infile, gboolean exec_commands, gboolean testparse,
	 gboolean *finished, const char *dirprefix)
{
	int stacklen;

	interrupted = FALSE;

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext();

	error_num = NO_ERROR;

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
		gel_lexer_open(infile);
	}

	gel_command = GEL_NO_COMMAND;
	g_free (gel_command_arg); gel_command_arg = NULL;

	lex_init = TRUE;
	/*yydebug=TRUE;*/  /*turn debugging of parsing on here!*/
	if(testparse) ignore_end_parse_errors = TRUE;
	got_end_too_soon = FALSE;
	gel_lexer_parse(infile);
	ignore_end_parse_errors = FALSE;

	/*while(yyparse() && !feof(yyin))
		;*/
	
	if(str) {
		while(my_yyinput()!=EOF)
			;
		close(lex_fd[0]);
		fflush(infile);
		gel_lexer_close(infile);
		/*fclose(infile);*/
	}
	
	if ( ! exec_commands) {
		gel_command = GEL_NO_COMMAND;
		g_free(gel_command_arg); gel_command_arg = NULL;
	} else if (do_exec_commands (dirprefix)) {
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
		if G_UNLIKELY (!testparse)
			gel_errorout (_("ERROR: Probably corrupt stack!"));
		if(finished) *finished = FALSE;
		return NULL;
	}
	replace_equals (evalstack->data, FALSE /* in_expression */);
	replace_exp (evalstack->data);
	fixup_num_neg (evalstack->data);
	evalstack->data = gather_comparisons (evalstack->data);
	try_to_do_precalc (evalstack->data);
	
	if (finished != NULL)
		*finished = TRUE;
	return stack_pop (&evalstack);
}

GelETree *
gel_runexp (GelETree *exp)
{
	static int busy = FALSE;
	GelETree *ret;
	GelCtx *ctx;
	
	if G_UNLIKELY (busy) {
		gel_errorout (_("ERROR: Can't execute more things at once!"));
		return NULL;
	}
	
	busy = TRUE;

	error_num = NO_ERROR;
	
	gel_push_file_info(NULL,0);

	ctx = eval_get_context();
	ret = eval_etree (ctx, exp);
	eval_free_context(ctx);

	gel_pop_file_info();

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

/* 'parsed' is eaten */
void
gel_evalexp_parsed (GelETree *parsed,
		    GelOutput *gelo,
		    const char *prefix,
		    gboolean pretty)
{
	GelETree *ret;
	
	if (parsed == NULL)
		return;
	/* gel_runexp eats the 'parsed' */
	ret = gel_runexp (parsed);
	if (ret == NULL)
		return;

	if(ret->type != NULL_NODE && gelo) {
		if(prefix) {
			gel_output_string(gelo, prefix);
		}
		if(pretty)
			gel_pretty_print_etree (gelo, ret);
		else
			gel_print_etree (gelo, ret, TRUE);
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
gel_evalexp (const char *str,
	     FILE *infile,
	     GelOutput *gelo,
	     const char *prefix,
	     gboolean pretty,
	     const char *dirprefix)
{
	GelETree *parsed;
	parsed = gel_parseexp (str, infile, TRUE, FALSE, NULL, dirprefix);
	gel_evalexp_parsed (parsed, gelo, prefix, pretty);
}

/*just to make the compiler happy*/
void yyerror(char *s);

void
yyerror (char *s)
{
	char *p;
	
	if (ignore_end_parse_errors && yytext[0] == '\0') {
		got_end_too_soon = TRUE;
		return;
	}
	
	if (strcmp (yytext, "\n") == 0) {
		gel_errorout (_("ERROR: %s before newline"), s);
	} else if (yytext[0] == '\0') {
		gel_errorout (_("ERROR: %s at end of input"), s);
	} else {
		char *tmp = g_strdup (yytext);
		p = tmp;
		while ( (p = strchr (p, '\n')) != NULL)
			*p='.';

		gel_errorout (_("ERROR: %s before '%s'"), s, tmp);
		g_free (tmp);
	}

	error_num = PARSE_ERROR;
}

void 
gel_errorout (const char *format, ...)
{
    va_list args;
    char *s;

    va_start (args, format);
    s = g_strdup_vprintf (format, args);
    va_end (args);

    (*errorout) (s);
    
    g_free (s);
}

void 
gel_infoout (const char *format, ...)
{
    va_list args;
    char *s;

    va_start (args, format);
    s = g_strdup_vprintf (format, args);
    va_end (args);

    (*infoout) (s);
    
    g_free (s);
}
