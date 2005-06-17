/* GENIUS Calculator
 * Copyright (C) 1997-2004 George Lebl
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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "calc.h"
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "util.h"
#include "matrix.h"
#include "matrixw.h"

#include "compil.h"

/*sort of weird encoding, use 'a'+upper 4 bits and 'a'+lower 4 bits*/
static void
append_string (GString *gs,const char *s)
{
	const char *p;
	char out[3]="aa";
	for(p=s;*p;p++) {
		out[0]='a'+((*p)&0xF);
		out[1]='a'+((*p)>>4);
		g_string_append(gs,out);
	}
}

/*sort of weird encoding, use 'a'+upper 4 bits and 'a'+lower 4 bits*/
char *
gel_decode_string (const char *s)
{
	int len = strlen(s);
	const char *ps;
	char *p,*pp;
	if(len%2 == 1)
		return NULL;
	
	/*the 0 takes care of the termination*/
	p = g_new0(char,(len/2)+1);
	
	for(ps=s,pp=p;*ps;ps+=2,pp++) {
		if(*ps<'a' || *ps >'a'+0xF ||
		   *(ps+1)<'a' || *(ps+1) >'a'+0xF) {
			g_free(p);
			return NULL;
		}
		*pp = (*ps-'a') + ((*(ps+1)-'a')<<4);
	}
	return p;
}

char *
gel_encode_string (const char *s)
{
	GString *gs = g_string_new (NULL);
	append_string (gs, s);
	return g_string_free (gs, FALSE);
}

static void
gel_compile_node(GelETree *t,GString *gs)
{
	char *s;
	int i,j;
	GSList *li;
	GelETree *ali;
	g_string_append_printf (gs, ";%d", t->type);
	switch(t->type) {
	case NULL_NODE:
		break;
	case VALUE_NODE:
		s = mpw_getstring(t->val.value,0,FALSE,FALSE,FALSE,GEL_OUTPUT_NORMAL,10, TRUE);
		g_string_append_c(gs,';');
		g_string_append(gs,s);
		g_free(s);
		break;
	case MATRIX_NODE:
		g_string_append_printf (gs, ";%dx%d;%d",
					gel_matrixw_width (t->mat.matrix),
					gel_matrixw_height (t->mat.matrix),
					t->mat.quoted);
		for(i=0;i<gel_matrixw_width(t->mat.matrix);i++) {
			for(j=0;j<gel_matrixw_height(t->mat.matrix);j++) {
				GelETree *tt = gel_matrixw_set_index(t->mat.matrix,i,j);
				if(!tt) g_string_append(gs,";0");
				else {
					g_string_append(gs,";N");
					gel_compile_node(tt,gs);
				}
			}
		}
		break;
	case OPERATOR_NODE:
		g_string_append_printf (gs, ";%d;%d",
					t->op.oper,
					t->op.nargs);
		for(ali=t->op.args;ali;ali=ali->any.next) {
			gel_compile_node(ali,gs);
		}
		break;
	case IDENTIFIER_NODE:
		g_string_append_printf (gs, ";%s", t->id.id->token);
		break;
	case STRING_NODE:
		if(*t->str.str) {
			g_string_append_c(gs,';');
			append_string(gs,t->str.str);
		} else {
			g_string_append(gs,";E");
		}
		break;
	case FUNCTION_NODE:
		g_assert(t->func.func->type==GEL_USER_FUNC);
		/*g_assert(t->func.func->id==NULL);*/
		g_string_append_printf (gs, ";%s;%s;%d;%d;%d;%d",
					t->func.func->id ? t->func.func->id->token : "*",
					t->func.func->symbolic_id ? t->func.func->symbolic_id->token : "*",
					t->func.func->nargs,
					t->func.func->vararg,
					t->func.func->propagate_mod,
					t->func.func->no_mod_all_args);
		for(li=t->func.func->named_args;li;li=g_slist_next(li)) {
			GelToken *tok = li->data;
			g_string_append_printf (gs, ";%s", tok->token);
		}
		gel_compile_node(t->func.func->data.user,gs);
		break;
	case COMPARISON_NODE:
		g_string_append_printf (gs, ";%d", t->comp.nargs);
		for(li=t->comp.comp;li;li=g_slist_next(li)) {
			int oper = GPOINTER_TO_INT(li->data);
			g_string_append_printf (gs, ";%d", oper);
		}
		for(ali=t->comp.args;ali;ali=ali->any.next) {
			gel_compile_node(ali,gs);
		}
		break;
	case BOOL_NODE:
		g_string_append_printf (gs, ";%c", 
					t->bool_.bool_ ? 't' : 'f');
		break;
	default:
		g_assert_not_reached(); break;
	}
}

char *
gel_compile_tree(GelETree *t)
{
	GString *gs;
	char *s;
	
	gs = g_string_new("T");
	
	gel_compile_node(t,gs);
	
	s = gs->str;
	g_string_free(gs,FALSE);
	return s;
}

static GelETree *
gel_decompile_node(char **ptrptr)
{
	GelETree *n;
	const char *id, *symbolic_id;
	char *p;
	int type = -1;
	int nargs = -1;
	int vararg = -1;
	int propagate_mod = -1;
	int no_mod_all_args = -1;
	int quote;
	int oper;
	int i,j;
	int w,h;
	GelMatrixW *m;
	GelETree *li = NULL;
	GelETree *args;
	GSList *oli;
	GelEFunc *func;
	mpw_t tmp;

	p = strtok_r(NULL,";", ptrptr);
	if G_UNLIKELY (!p) return NULL;
	sscanf(p,"%d",&type);
	if G_UNLIKELY (type==-1) return NULL;

	switch(type) {
	case NULL_NODE:
		return gel_makenum_null();
	case VALUE_NODE:
		p = strtok_r(NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		mpw_init(tmp);
		mpw_set_str(tmp,p,10);
		return gel_makenum_use(tmp);
	case MATRIX_NODE:
		p = strtok_r(NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		h = w = -1;
		sscanf(p,"%dx%d",&w,&h);
		if G_UNLIKELY (h==-1 || w ==-1)
			return NULL;

		p = strtok_r(NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		quote = -1;
		sscanf(p,"%d",&quote);
		if G_UNLIKELY (quote==-1) return NULL;

		m = gel_matrixw_new();
		gel_matrixw_set_size(m,w,h);
		for(i=0;i<w;i++) {
			for(j=0;j<h;j++) {
				p = strtok_r (NULL,";", ptrptr);
				if G_UNLIKELY (!p) {
					gel_matrixw_free(m);
					return NULL;
				}
				if(*p=='N') {
					GelETree *tt = gel_decompile_node (ptrptr);
					if G_UNLIKELY (!tt) {
						gel_matrixw_free(m);
						return NULL;
					}
					gel_matrixw_set_index(m,i,j)=tt;
				} else if G_UNLIKELY (*p!='0') {
					gel_matrixw_free(m);
					return NULL;
				}
			}
		}
		GET_NEW_NODE(n);
		n->type = MATRIX_NODE;
		n->mat.matrix = m;
		n->mat.quoted = quote;
		return n;
	case OPERATOR_NODE:
		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		oper = -1;
		sscanf(p,"%d",&oper);
		if G_UNLIKELY (oper==-1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"%d",&nargs);
		if G_UNLIKELY (nargs==-1) return NULL;

		args = li = NULL;
		for(i=0;i<nargs;i++) {
			GelETree *tt = gel_decompile_node (ptrptr);
			if G_UNLIKELY (tt == NULL) {
				while(args) {
					li = args->any.next;
					gel_freetree(args);
					args = li;
				}
				return NULL;
			}
			if(!args) {
				args = li = tt;
			} else {
				li = li->any.next = tt;
			}
			li->any.next = NULL;
		}

		GET_NEW_NODE(n);
		n->type = OPERATOR_NODE;
		n->op.args = args;
		n->op.nargs = nargs;
		n->op.oper = oper;
		return n;
	case IDENTIFIER_NODE:
		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		GET_NEW_NODE(n);
		n->type = IDENTIFIER_NODE;
		n->id.id = d_intern(p);
		return n;
	case STRING_NODE:
		p = strtok_r (NULL, ";", ptrptr);
		if G_UNLIKELY (p == NULL)
			return NULL;
		
		if (*p=='E') {
			n = gel_makenum_string_constant ("");
		} else {
			p = gel_decode_string (p);
			if G_UNLIKELY (p == NULL)
				return NULL;
			n = gel_makenum_string_constant (p);
			g_free (p);
		}
		return n;
	case FUNCTION_NODE:
		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		if (strcmp (p, "*") == 0)
			id = NULL;
		else
			id = p;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		if (strcmp (p, "*") == 0)
			symbolic_id = NULL;
		else
			symbolic_id = p;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		nargs = -1;
		sscanf(p,"%d",&nargs);
		if G_UNLIKELY (nargs==-1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"%d",&vararg);
		if G_UNLIKELY (vararg == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"%d",&propagate_mod);
		if G_UNLIKELY (propagate_mod == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"%d",&no_mod_all_args);
		if G_UNLIKELY (no_mod_all_args == -1) return NULL;

		oli = NULL;
		for(i=0;i<nargs;i++) {
			p = strtok_r (NULL,";", ptrptr);
			if G_UNLIKELY (!p) {
				g_slist_free(oli);
				return NULL;
			}
			oli = g_slist_append(oli,d_intern(p));
		}
		
		n = gel_decompile_node (ptrptr);
		if G_UNLIKELY (!n) {
			g_slist_free(oli);
			return NULL;
		}

		/* Note that d_intern handles NULL properly */
		func = d_makeufunc (d_intern (id), n, oli, nargs, NULL);
		func->symbolic_id = d_intern (symbolic_id);
		func->context = -1;
		func->vararg = vararg ? 1 : 0;
		func->propagate_mod = propagate_mod ? 1 : 0;
		func->no_mod_all_args = no_mod_all_args ? 1 : 0;

		GET_NEW_NODE(n);
		n->type = FUNCTION_NODE;
		n->func.func = func;
		return n;
	case COMPARISON_NODE:
		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"%d",&nargs);
		if G_UNLIKELY (nargs==-1) return NULL;
	
		oli = NULL;
		for(i=0;i<nargs-1;i++) {
			p = strtok_r (NULL,";", ptrptr);
			if G_UNLIKELY (!p) {
				g_slist_free(oli);
				return NULL;
			}
			j = -1;
			sscanf(p,"%d",&j);
			if G_UNLIKELY (j==-1) {
				g_slist_free(oli);
				return NULL;
			}
			oli = g_slist_append(oli,GINT_TO_POINTER(j));
		}

		args = li = NULL;
		for(i=0;i<nargs;i++) {
			GelETree *tt = gel_decompile_node (ptrptr);
			if G_UNLIKELY (!tt) {
				while(args) {
					li = args->any.next;
					gel_freetree(args);
					args = li;
				}
				g_slist_free(oli);
				return NULL;
			}
			if(!args) {
				args = li = tt;
			} else {
				li = li->any.next = tt;
			}
			li->any.next = NULL;
		}

		GET_NEW_NODE(n);
		n->type = COMPARISON_NODE;
		n->comp.args = args;
		n->comp.nargs = nargs;
		n->comp.comp = oli;
		return n;
	case BOOL_NODE:
		p = strtok_r (NULL, ";", ptrptr);
		if G_UNLIKELY (p == NULL)
			return NULL;
		if (*p == 't')
			return gel_makenum_bool (TRUE);
		else if (*p == 'f')
			return gel_makenum_bool (FALSE);
		else
			return NULL;
	default:
		return NULL;
	}
}

GelETree *
gel_decompile_tree (char *s)
{
	GelETree *t;
	char *p;
	char *ptrptr;
	
	if G_UNLIKELY (s == NULL) return NULL;

	p = strtok_r (s, ";", &ptrptr);

	if G_UNLIKELY (strcmp (p, "T") != 0) {
		gel_errorout (_("Bad tree record when decompiling"));
		return NULL;
	}
	
	t = gel_decompile_node (&ptrptr);
	if G_UNLIKELY (t == NULL) {
		printf ("FOO!\n");
		gel_errorout (_("Bad tree record when decompiling"));
		return NULL;
	}

	g_free (s);
	
	return t;
}
