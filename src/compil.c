/* GENIUS Calculator
 * Copyright (C) 1997-2025 Jiri (George) Lebl
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
#include <string.h>
#include <glib.h>
#include "calc.h"
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "util.h"
#include "matrix.h"
#include "matrixw.h"

#include <vicious.h>

#include "compil.h"

/* first char '=' then rest Base64 */
/* else just ascii */
char *
gel_decode_string (const char *s)
{
	if (s == NULL) {
		return NULL;
	} else if (s[0] == '=') {
		gsize len;
		char *p = (char *)g_base64_decode (&(s[1]), &len);
		if (p == NULL) /* error was probably logged by now */
			return NULL;
		p = g_realloc (p, len+1);
		p[len] = '\0';
		return p;
	} else {
		return g_strdup (s);
	}
}

static int
is_ok_ascii (const char *s)
{
	const char *p;
	if (s[0] == '=')
		return FALSE;
	for (p = s; *p != '\0'; p++) {
		if ( ! ( (*p >= 'a' && *p <= 'z') ||
			 (*p >= 'A' && *p <= 'Z') ||
			 (*p >= '0' && *p <= '9') ||
			 strchr ("():,.[] !?~+-_{}/=><*^'\"@#$|\\&\t", *p) != NULL) ) {
			return FALSE;
		}
	}
	return TRUE;
}

char *
gel_encode_string (const char *s)
{
	if (ve_string_empty (s)) {
		return g_strdup ("=");
	} else if (is_ok_ascii (s)) {
		return g_strdup (s);
	} else {
		char *p = g_base64_encode ((const unsigned char *)s, strlen (s));
		char *ret = g_strconcat ("=", p, NULL);
		g_free (p);
		return ret;
	}
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
	case GEL_NULL_NODE:
		break;
	case GEL_VALUE_NODE:
		s = mpw_getstring(t->val.value,0,FALSE,FALSE,FALSE,GEL_OUTPUT_NORMAL,10, TRUE);
		g_string_append_c(gs,';');
		g_string_append(gs,s);
		g_free(s);
		break;
	case GEL_MATRIX_NODE:
		g_string_append_printf (gs, ";%dx%d;%d",
					gel_matrixw_width (t->mat.matrix),
					gel_matrixw_height (t->mat.matrix),
					t->mat.quoted);
		/* Note that compilation goes in opposite to usual order for genius,
		 * columnwise and not rowwise */
		for(i=0;i<gel_matrixw_width(t->mat.matrix);i++) {
			for(j=0;j<gel_matrixw_height(t->mat.matrix);j++) {
				GelETree *tt = gel_matrixw_get_index(t->mat.matrix,i,j);
				if(!tt) g_string_append(gs,";0");
				else {
					g_string_append(gs,";N");
					gel_compile_node(tt,gs);
				}
			}
		}
		break;
	case GEL_OPERATOR_NODE:
		g_string_append_printf (gs, ";%d;%d",
					t->op.oper,
					t->op.nargs);
		for(ali=t->op.args;ali;ali=ali->any.next) {
			gel_compile_node(ali,gs);
		}
		break;
	case GEL_IDENTIFIER_NODE:
		g_string_append_printf (gs, ";%s", t->id.id->token);
		break;
	case GEL_STRING_NODE:
		g_string_append_c (gs, ';');
		s = gel_encode_string (t->str.str);
		g_string_append (gs, s);
		g_free (s);
		break;
	case GEL_FUNCTION_NODE:
		g_assert(t->func.func->type==GEL_USER_FUNC);
		/*g_assert(t->func.func->id==NULL);*/
		g_string_append_printf (gs,
					";%s;%s;n%d;v%d;p%d;o%d;l%d;e%d;b%d",
					t->func.func->id ? t->func.func->id->token : "*",
					t->func.func->symbolic_id ? t->func.func->symbolic_id->token : "*",
					t->func.func->nargs,
					t->func.func->vararg,
					t->func.func->propagate_mod,
					t->func.func->no_mod_all_args,
					t->func.func->local_all,
					t->func.func->never_on_subst_list,
					t->func.func->built_subst_dict);
		/* Make sure to also update calc compile_funcs_in_dict
		 * and related! */

		if (t->func.func->local_idents == NULL)
			g_string_append (gs, ";-");
		for (li = t->func.func->local_idents; li != NULL; li = li->next) {
			GelToken *tok = li->data;
			if (li != t->func.func->local_idents)
				g_string_append_printf (gs, ",%s", tok->token);
			else
				g_string_append_printf (gs, ";%s", tok->token);
		}
		if (t->func.func->subst_dict == NULL)
			g_string_append (gs, ";-");
		for (li = t->func.func->subst_dict; li != NULL; li = li->next) {
			GelToken *tok = li->data;
			if (li != t->func.func->subst_dict)
				g_string_append_printf (gs, ",%s", tok->token);
			else
				g_string_append_printf (gs, ";%s", tok->token);
		}
		for (li = t->func.func->named_args; li != NULL; li = li->next) {
			GelToken *tok = li->data;
			g_string_append_printf (gs, ";%s", tok->token);
		}
		gel_compile_node(t->func.func->data.user,gs);
		break;
	case GEL_COMPARISON_NODE:
		g_string_append_printf (gs, ";%d", t->comp.nargs);
		for(li=t->comp.comp;li;li=g_slist_next(li)) {
			int oper = GPOINTER_TO_INT(li->data);
			g_string_append_printf (gs, ";%d", oper);
		}
		for(ali=t->comp.args;ali;ali=ali->any.next) {
			gel_compile_node(ali,gs);
		}
		break;
	case GEL_BOOL_NODE:
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
	
	gs = g_string_new("T");
	
	gel_compile_node(t,gs);
	
	return g_string_free(gs,FALSE);
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
	int local_all = -1;
	int never_on_subst_list = -1;
	int built_subst_dict = -1;
	int quote;
	int oper;
	int i,j;
	int w,h;
	GelMatrixW *m;
	GelETree *li = NULL;
	GelETree *args;
	GSList *oli, *local_idents, *subst_dict;
	GelEFunc *func;
	mpw_t tmp;

	p = strtok_r(NULL,";", ptrptr);
	if G_UNLIKELY (!p) return NULL;
	sscanf(p,"%d",&type);
	if G_UNLIKELY (type==-1) return NULL;

	switch(type) {
	case GEL_NULL_NODE:
		return gel_makenum_null();
	case GEL_VALUE_NODE:
		p = strtok_r(NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		mpw_init(tmp);
		mpw_set_str(tmp,p,10);
		if G_UNLIKELY (gel_error_num == GEL_NUMERICAL_MPW_ERROR) {
			gel_errorout (_("Bad value record when decompiling"));
			gel_error_num = GEL_NO_ERROR;
			mpw_set_ui(tmp,0);
		}
		return gel_makenum_use(tmp);
	case GEL_MATRIX_NODE:
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
		/* Note that compilation goes in opposite to usual order for genius,
		 * columnwise and not rowwise */
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
		GEL_GET_NEW_NODE(n);
		n->type = GEL_MATRIX_NODE;
		n->mat.matrix = m;
		n->mat.quoted = quote;
		return n;
	case GEL_OPERATOR_NODE:
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

		GEL_GET_NEW_NODE(n);
		n->type = GEL_OPERATOR_NODE;
		n->op.args = args;
		n->op.nargs = nargs;
		n->op.oper = oper;
		return n;
	case GEL_IDENTIFIER_NODE:
		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		GEL_GET_NEW_NODE(n);
		n->type = GEL_IDENTIFIER_NODE;
		n->id.id = d_intern(p);
		/* never compile the uninitialized state,
		 * we simply forget that we already gave an
		 * error here and will warn again */
		n->id.uninitialized = FALSE;
		return n;
	case GEL_STRING_NODE:
		p = strtok_r (NULL, ";", ptrptr);
		if G_UNLIKELY (p == NULL)
			return NULL;
		
		p = gel_decode_string (p);
		if G_UNLIKELY (p == NULL)
			return NULL;
		n = gel_makenum_string_constant (p);
		g_free (p);

		return n;
	case GEL_FUNCTION_NODE:
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
		sscanf(p,"n%d",&nargs);
		if G_UNLIKELY (nargs==-1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"v%d",&vararg);
		if G_UNLIKELY (vararg == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"p%d",&propagate_mod);
		if G_UNLIKELY (propagate_mod == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (!p) return NULL;
		sscanf(p,"o%d",&no_mod_all_args);
		if G_UNLIKELY (no_mod_all_args == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (p == NULL) return NULL;
		sscanf(p,"l%d",&local_all);
		if G_UNLIKELY (local_all == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (p == NULL) return NULL;
		sscanf(p,"e%d",&never_on_subst_list);
		if G_UNLIKELY (never_on_subst_list == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (p == NULL) return NULL;
		sscanf(p,"b%d",&built_subst_dict);
		if G_UNLIKELY (built_subst_dict == -1) return NULL;

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (p == NULL) return NULL;
		local_idents = NULL;
		if (strcmp (p, "-") != 0) {
			char **s;
			s = g_strsplit (p, ",", -1);
			for (i = 0; s[i] != NULL; i++) {
				local_idents = g_slist_append (local_idents, d_intern (s[i]));
			}
			g_strfreev (s);
		}

		p = strtok_r (NULL,";", ptrptr);
		if G_UNLIKELY (p == NULL) return NULL;
		subst_dict = NULL;
		if (strcmp (p, "-") != 0) {
			char **s;
			s = g_strsplit (p, ",", -1);
			for (i = 0; s[i] != NULL; i++) {
				subst_dict = g_slist_append (subst_dict, d_intern (s[i]));
			}
			g_strfreev (s);
		}

		oli = NULL;
		for(i=0;i<nargs;i++) {
			p = strtok_r (NULL,";", ptrptr);
			if G_UNLIKELY (!p) {
				g_slist_free (oli);
				g_slist_free (local_idents);
				g_slist_free (subst_dict);
				return NULL;
			}
			oli = g_slist_append(oli,d_intern(p));
		}
		
		n = gel_decompile_node (ptrptr);
		if G_UNLIKELY (!n) {
			g_slist_free (oli);
			g_slist_free (local_idents);
			g_slist_free (subst_dict);
			return NULL;
		}

		/* Note that d_intern handles NULL properly */
		func = d_makeufunc (d_intern (id), n, oli, nargs, NULL);
		func->symbolic_id = d_intern (symbolic_id);
		func->context = -1;
		func->vararg = vararg ? 1 : 0;
		func->propagate_mod = propagate_mod ? 1 : 0;
		func->no_mod_all_args = no_mod_all_args ? 1 : 0;
		func->local_all = local_all ? 1 : 0;
		func->never_on_subst_list = never_on_subst_list ? 1 : 0;
		func->built_subst_dict = built_subst_dict ? 1 : 0;
		func->local_idents = local_idents;
		func->subst_dict = subst_dict;

		GEL_GET_NEW_NODE(n);
		n->type = GEL_FUNCTION_NODE;
		n->func.func = func;
		return n;
	case GEL_COMPARISON_NODE:
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

		GEL_GET_NEW_NODE(n);
		n->type = GEL_COMPARISON_NODE;
		n->comp.args = args;
		n->comp.nargs = nargs;
		n->comp.comp = oli;
		return n;
	case GEL_BOOL_NODE:
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
		g_free (s);
		return NULL;
	}
	
	t = gel_decompile_node (&ptrptr);
	g_free (s);

	if G_UNLIKELY (t == NULL) {
		gel_errorout (_("Bad tree record when decompiling"));
		return NULL;
	}

	return t;
}
