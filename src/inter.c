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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "calc.h"
#include "dict.h"
#include "plugin.h"

#include "inter.h"

static int toplevelokg = TRUE;

static int
ok_for_top(char *s)
{
	char *t = g_strstrip(g_strdup(s));
	if(strncmp(t,"plugin",strlen(t))==0 ||
	   strncmp(t,"load",strlen(t))==0 ||
	   strncmp(t,"cd",strlen(t))==0 ||
	   strncmp(t,"pwd",strlen(t))==0 ||
	   strncmp(t,"ls",strlen(t))==0) {
		g_free(t);
		return TRUE;
	} else {
		g_free(t);
		return FALSE;
	}
}

GelETree *
get_p_expression(void)
{
	GString *gs;
	const char *prompt = "genius> ";
	
	gel_interrupted = FALSE;
	
	gs = g_string_new("");
	
	for(;;) {
		int finished;
		char *s;
		GelETree *ret;
		int oldtop = toplevelokg;

		toplevelokg = ok_for_top(gs->str);
		s = readline(prompt);
		toplevelokg = oldtop;
		
		if(gel_interrupted) {
			g_string_free(gs,TRUE);
			if(s) free(s);
			return NULL;
		}

		prompt = "      > ";
		if(!s) {
			gel_got_eof = TRUE;
			g_string_append_c(gs,'\n');
			ret = gel_parseexp(gs->str, NULL, TRUE, FALSE, NULL, NULL);
			g_string_free(gs,TRUE);
			return ret;
		}
		if(!*s)	{
			free(s);
			continue;
		}
		add_history(s);
		g_string_append(gs,s);
		free(s);
		g_string_append_c(gs,'\n');
		
		ret = gel_parseexp(gs->str, NULL, TRUE, TRUE, &finished, NULL);
		if (gel_got_eof)
			gel_got_eof = FALSE;
		if(finished) {
			g_string_free(gs,TRUE);
			return ret;
		}
	}
}

static GString *p_expr = NULL;
static int old_toplevelokg = FALSE;
void (*got_expr_func)(GelETree *) = NULL;

static void
write_all_state_to_rl(FILE *fp)
{
	GSList *li;
	int count;
	char *s;

	li = d_getcontext();
	count = 0;
	for(li=d_getcontext();li;li=li->next) {
		GelEFunc *f = li->data;
		if(!f->id || !f->id->token)
			continue;
		count++;
	}
	fprintf(fp,"FUNCTIONS %d\n",count);
	for(li=d_getcontext();li;li=li->next) {
		GelEFunc *f = li->data;
		if(!f->id || !f->id->token)
			continue;
		fprintf(fp,"%s\n",f->id->token);
	}

	count = 0;
	for(li = gel_plugin_list;li;li=li->next) {
		GelPlugin *plg = li->data;
		if(!plg->base)
			continue;
		count++;
	}
	fprintf(fp,"PLUGINS %d\n",count);
	for(li = gel_plugin_list;li;li=li->next) {
		GelPlugin *plg = li->data;
		if(!plg->base)
			continue;
		fprintf(fp,"%s\n",plg->base);
	}

	s = g_get_current_dir ();
	fprintf (fp, "CWD %s\n", s);
	g_free (s);

	if(toplevelokg)
		fprintf(fp,"TOPLEVEL OK\n");
	else
		fprintf(fp,"TOPLEVEL NOT OK\n");

	fflush(fp);
}

void
get_cb_p_expression(const char *s, FILE *torlfp)
{
	int finished;
	GelETree *ret;
	/*                   "genius> "*/
	const char *prompt = "      > ";
	toplevelokg = old_toplevelokg;

	if(gel_interrupted) {
		prompt = "\001\e[1m\002genius>\001\e[0m\002 ";
		gel_interrupted = FALSE;
		if(p_expr) g_string_free(p_expr,TRUE);
		p_expr = NULL;
		goto done_with_get;
	}

	if(!s) {
		gel_got_eof = TRUE;
		g_string_append_c(p_expr, '\n');
		ret = gel_parseexp(p_expr->str, NULL, TRUE, FALSE, NULL, NULL);
		g_string_free(p_expr, TRUE);
		p_expr = NULL;
		(*got_expr_func)(ret);
		gel_interrupted = FALSE;
		prompt = "\001\e[1m\002genius>\001\e[0m\002 ";
		goto done_with_get;
	}
	if(!*s)	{
		goto done_with_get;
	}
	add_history(s);
	g_string_append(p_expr,s);
	g_string_append_c(p_expr,'\n');

	ret = gel_parseexp(p_expr->str, NULL, TRUE, TRUE, &finished, NULL);
	if (gel_got_eof)
		gel_got_eof = FALSE;
	if(finished) {
		g_string_free(p_expr,TRUE);
		p_expr = NULL;
		(*got_expr_func)(ret);
		gel_interrupted = FALSE;
		prompt = "\001\e[1m\002genius>\001\e[0m\002 ";
	}
done_with_get:
	if(!p_expr) p_expr = g_string_new("");
	old_toplevelokg = toplevelokg;
	toplevelokg = ok_for_top(p_expr->str);

	write_all_state_to_rl(torlfp);
	fprintf(torlfp,"READLINE %s\n",prompt);
	fflush(torlfp);
}

void
start_cb_p_expression(void (*get_func)(GelETree *), FILE *torlfp)
{
	gel_interrupted = FALSE;
	
	gel_rewind_file_info();
	
	if(p_expr) g_string_free(p_expr,TRUE);
	p_expr = g_string_new("");
	
	got_expr_func = get_func;
	
	old_toplevelokg = toplevelokg;
	toplevelokg = ok_for_top(p_expr->str);

	write_all_state_to_rl(torlfp);
	fprintf(torlfp,"READLINE \001\e[1m\002genius>\001\e[0m\002 \n");
	fflush(torlfp);
}

void
stop_cb_p_expression(void)
{
	if(p_expr) g_string_free(p_expr,TRUE);
	p_expr = NULL;

	toplevelokg = old_toplevelokg;
	
	got_expr_func = NULL;
}



static int addtoplevels = TRUE;
extern const char *genius_toplevels[];
extern const char *genius_operators[];


static char *
command_generator (const char *text, int state)
{
	static int oi,ti,len;
	static GSList *fli;

	if(!state) {
		oi = 0;
		if(addtoplevels)
			ti = 0;
		else
			ti = -1;
		len = strlen (text);
		fli = d_getcontext();
	}
	
	while(ti>=0 && genius_toplevels[ti]) {
		const char *s = genius_toplevels[ti++];
		if(strncmp(s,text,len)==0)
			return strdup(s);
	}

	while(genius_operators[oi]) {
		const char *s = genius_operators[oi++];
		if(strncmp(s,text,len)==0)
			return strdup(s);
	}

	while(fli) {
		GelEFunc *f = fli->data;
		fli = g_slist_next(fli);
		if(!f->id || !f->id->token)
			continue;
		if(strncmp(f->id->token,text,len)==0)
			return strdup(f->id->token);
	}

	return NULL;
}

static char *
plugin_generator (const char *text, int state)
{
	static int len;
	static GSList *li;

	if(!state) {
		len = strlen (text);
		li = gel_plugin_list;
	}

	while(li) {
		GelPlugin *plg = li->data;
		li = g_slist_next(li);
		if(!plg->base)
			continue;
		if(strncmp(plg->base,text,len)==0)
			return strdup(plg->base);
	}

	return NULL;
}

/* Note: keep in sync with genius-readline-helper.c */
/* FIXME: make this common */
static char **
tab_completion (const char *text, int start, int end)
{
	char *p;
	for(p=rl_line_buffer;*p==' ' || *p=='\t';p++)
		;
	if(toplevelokg &&
	   (strncmp(p,"load ",5)==0 ||
	    strncmp(p,"load\t",5)==0)) {
		return NULL;
	}
	if(toplevelokg &&
	   (strncmp(p,"ls ",3)==0 ||
	    strncmp(p,"ls\t",3)==0)) {
		return NULL;
	}
	if(toplevelokg &&
	   (strncmp(p,"cd ",3)==0 ||
	    strncmp(p,"cd\t",3)==0)) {
		return NULL;
	}

	if(toplevelokg &&
	   (strncmp(p,"plugin ",7)==0 ||
	    strncmp(p,"plugin\t",7)==0)) {
		return rl_completion_matches (text, plugin_generator);
	}
	
	if(toplevelokg &&
	   (!*p ||
	    strncmp(p,"load",strlen(p))==0 ||
	    strncmp(p,"cd",strlen(p))==0 ||
	    strncmp(p,"ls",strlen(p))==0 ||
	    strncmp(p,"pwd",strlen(p))==0 ||
	    strncmp(p,"help",strlen(p))==0 ||
	    strncmp(p,"plugin",strlen(p))==0))
		addtoplevels = TRUE;
	else
		addtoplevels = FALSE;

	return rl_completion_matches (text, command_generator);
}

void
init_inter(void)
{
	rl_readline_name = "Genius";
	rl_attempted_completion_function =
		(rl_completion_func_t *)tab_completion;
}
