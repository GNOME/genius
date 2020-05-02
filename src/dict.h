/* GENIUS Calculator
 * Copyright (C) 1997-2009 Jiri (George) Lebl
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

#ifndef _DICT_H_
#define _DICT_H_

/*declarations of structures*/
#include "structs.h"

typedef struct _GelContextFrame  GelContextFrame;
struct _GelContextFrame  {
	GelContextFrame *next;

	GSList *functions;
	GSList *substlist;
	GelToken *name;

	gboolean local_all;
};


/*return current context number (0 is global, -1 is uninitialized)*/
int d_curcontext(void) G_GNUC_PURE;

/*make builtin function and return it*/
GelEFunc * d_makebifunc(GelToken *id, GelBIFunction f, int nargs);

/*make a user function and return it*/
GelEFunc * d_makeufunc (GelToken *id, GelETree *value, GSList *argnames, int nargs,
			const GSList *extra_dict);

/*make a variable function and return in*/
GelEFunc * d_makevfunc(GelToken *id, GelETree *value);

/*make a reference function and return it*/
GelEFunc * d_makereffunc(GelToken *id, GelEFunc *ref);

/*copy a function*/
GelEFunc *d_copyfunc(GelEFunc *o);

/*make a real function from a fake*/
GelEFunc * d_makerealfunc(GelEFunc *o,GelToken *id, gboolean use);

/*make real func and replace o with it, without changing o's context or id*/
/*if use is set, we USE the original function, NULLing appropriately*/
void d_setrealfunc(GelEFunc *n,GelEFunc *fake, gboolean use);

void d_initcontext(void);

/*add a function struct to the dict (in current context)*/
GelEFunc * d_addfunc (GelEFunc *func);
/*add a function struct to the dict (in global context)*/
GelEFunc * d_addfunc_global (GelEFunc *func);

/*set value of an existing function (in local context), used for arguments
  WARNING, does not free the memory allocated by previous value!*/
gboolean d_setvalue (GelToken *id,GelETree *value);

/*this will work right in all situations*/
void d_set_value(GelEFunc *n,GelETree *value);
void d_set_ref(GelEFunc *n,GelEFunc *ref);

/*dictionary functions*/

/*lookup a function in the dictionary, either the whole thing, or just the
  current context otherwise*/
/*lookup a function in the dictionary in the current context*/
GelEFunc * d_lookup_local(GelToken *id) G_GNUC_PURE;
GelEFunc * d_lookup_global_up1(GelToken *id) G_GNUC_PURE;
GelEFunc * d_lookup_only_global (GelToken *id) G_GNUC_PURE;
/*lookup a function in the dictionary, if there are more return the one in the
  highest context*/
GelEFunc * d_lookup_global (GelToken *id) G_GNUC_PURE;

GelToken * d_intern (const char *id);

gboolean d_delete(GelToken *id);
gboolean d_delete_global(GelToken *id);

/*clear all context dictionaries and pop out all the contexts except
  the global one
  also init the context stack if it hasn't been done*/
void d_singlecontext(void);

/*free all memory allocated by a dictionary*/
void d_freedict(GSList *n);

void d_freefunc(GelEFunc *n);

/*replace old with stuff from new and free new*/
void d_replacefunc (GelEFunc *old, GelEFunc *_new);

/*push a new dictionary onto the context stack*/
gboolean d_addcontext (GelEFunc *func);

/*gimme the last dictionary and pop the context stack*/
void d_popcontext(void);

/*gimme the current dictionary*/
GSList * d_getcontext (void) G_GNUC_PURE;

/* this is a list of lists of the context stack,
 * Also it is a pointer to the current context frame */
GelContextFrame * d_get_all_contexts (void) G_GNUC_PURE;

/*gimme the current global dictionary*/
GSList * d_getcontext_global (void) G_GNUC_PURE;
GSList * d_find_similar_globals (const char *id);

/* Put on subst local var list for this current stack */
void d_put_on_subst_list (GelEFunc *func);

/*protect all variables currently in memory, except for "Ans"*/
void d_protect_all(void);

/* add named arguments to a function.  Note that this APPENDS the
 * list and should only be used for built in functions */
void d_add_named_args (GelEFunc *f, const char *args);

#define D_ENSURE_USER_BODY(f) \
	if G_UNLIKELY ((f)->data.user == NULL) {			\
		g_assert ((f)->id->uncompiled != NULL);			\
		(f)->data.user =					\
			gel_decompile_tree ((f)->id->uncompiled);	\
		(f)->id->uncompiled = NULL;				\
		/* On error give null tree */				\
		if ((f)->data.user == NULL)				\
			(f)->data.user = gel_makenum_null ();		\
	}								\

#define D_ENSURE_SUBST_DICT(f) \
	D_ENSURE_USER_BODY (f);							\
	if ( ! (f)->built_subst_dict) {						\
		(f)->subst_dict = gel_get_ids_for_extradict (NULL,		\
						             (f)->named_args,	\
							     (f)->local_idents,	\
							     (f)->data.user);	\
		(f)->built_subst_dict = 1;					\
	}


#endif
