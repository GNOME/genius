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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "eval.h"
#include "dict.h"
#include "util.h"
#include "funclib.h"
#include "compil.h"

/*the context stack structure*/
typedef struct _context_t {
	GSList *stack;
	int top;
} context_t;

static context_t context={NULL,-1};

static GHashTable *dictionary;

extern GHashTable *uncompiled;

GelEFunc *free_funcs = NULL;

/*return current context number (0 is global, -1 is uninitialized)*/
int
d_curcontext(void)
{
	return context.top;
}

/*make builtin function and return it*/
GelEFunc *
d_makebifunc(GelToken *id, dictfunc f, int nargs)
{
	GelEFunc *n;

	if(!free_funcs)
		n = g_new(GelEFunc,1);
	else {
		n = free_funcs;
		free_funcs = free_funcs->data.next;
	}
	n->id = id;
	n->data.func = f;
	n->nargs = nargs;
	n->vararg = FALSE;
	n->named_args = NULL;
	n->context = context.top;
	n->type = GEL_BUILTIN_FUNC;

	return n;
}

/*make a user function and return it*/
GelEFunc *
d_makeufunc(GelToken *id, GelETree *value, GSList *argnames, int nargs)
{
	GelEFunc *n;

	if(!free_funcs)
		n = g_new(GelEFunc,1);
	else {
		n = free_funcs;
		free_funcs = free_funcs->data.next;
	}
	n->id=id;
	n->data.user=value;
	n->nargs=nargs;
	n->vararg = FALSE;
	n->named_args=argnames;
	n->context=context.top;
	n->type=GEL_USER_FUNC;

	return n;
}

/*make a variable function and return it*/
GelEFunc *
d_makevfunc(GelToken *id, GelETree *value)
{
	GelEFunc *n;

	if(!free_funcs)
		n = g_new(GelEFunc,1);
	else {
		n = free_funcs;
		free_funcs = free_funcs->data.next;
	}
	n->id=id;
	n->data.user=value;
	n->nargs=0;
	n->vararg = FALSE;
	n->named_args=NULL;
	n->context=context.top;
	n->type=GEL_VARIABLE_FUNC;

	return n;
}

/*make a user function and return it*/
GelEFunc *
d_makereffunc(GelToken *id, GelEFunc *ref)
{
	GelEFunc *n;

	if(!free_funcs)
		n = g_new(GelEFunc,1);
	else {
		n = free_funcs;
		free_funcs = free_funcs->data.next;
	}
	n->id=id;
	n->data.ref=ref;
	n->nargs=0;
	n->vararg = FALSE;
	n->named_args=NULL;
	n->context=context.top;
	n->type=GEL_REFERENCE_FUNC;

	return n;
}

/*copy a function*/
GelEFunc *
d_copyfunc(GelEFunc *o)
{
	GelEFunc *n;

	if(!free_funcs)
		n = g_new(GelEFunc,1);
	else {
		n = free_funcs;
		free_funcs = free_funcs->data.next;
	}
	memcpy(n,o,sizeof(GelEFunc));
	if(n->type == GEL_USER_FUNC ||
	   n->type == GEL_VARIABLE_FUNC) {
		D_ENSURE_USER_BODY (o);
		n->data.user=copynode(o->data.user);
	}
	n->named_args = g_slist_copy(o->named_args);
	
	return n;
}

/*make a real function from a fake*/
GelEFunc *
d_makerealfunc(GelEFunc *o,GelToken *id, int use)
{
	GelEFunc *n;

	if(!free_funcs)
		n = g_new(GelEFunc,1);
	else {
		n = free_funcs;
		free_funcs = free_funcs->data.next;
	}
	memcpy(n,o,sizeof(GelEFunc));
	n->id = id;
	n->context = context.top;

	if(n->type == GEL_USER_FUNC ||
	   n->type == GEL_VARIABLE_FUNC) {
		D_ENSURE_USER_BODY (o);
		if(use) {
			n->data.user = o->data.user;
			o->data.user = NULL;
		} else
			n->data.user = copynode(o->data.user);
	}
	if(use) {
		o->named_args = NULL;
		o->named_args = 0;
	} else
		n->named_args = g_slist_copy(o->named_args);
	
	return n;
}

/*make real func and replace n with it, without changing n's context or id*/
/*if use is set, we USE the original function, NULLing approriately*/
void
d_setrealfunc(GelEFunc *n,GelEFunc *fake, int use)
{
	if(n->type == GEL_USER_FUNC ||
	   n->type == GEL_VARIABLE_FUNC)
		gel_freetree(n->data.user);
	
	n->type = fake->type;
	n->data = fake->data;
	if(n->type == GEL_USER_FUNC ||
	   n->type == GEL_VARIABLE_FUNC) {
		D_ENSURE_USER_BODY (fake);
		if(use) {
			n->data.user = fake->data.user;
			fake->data.user = NULL;
		} else
			n->data.user = copynode(fake->data.user);
	}

	if(use) {
		n->named_args = fake->named_args;
		n->nargs = fake->nargs;
		fake->named_args = NULL;
		fake->nargs = 0;
	} else {
		n->named_args = g_slist_copy(fake->named_args);
		n->nargs = fake->nargs;
	}
}


void
d_initcontext(void)
{
	GelToken *tok;
	context.top=0; /*0 means that element 0 exists!*/
	/*add an empty dictionary*/
	context.stack=g_slist_prepend(NULL,NULL);

	dictionary = g_hash_table_new(g_str_hash,g_str_equal);

	/*add Ans and ans as the same token*/

	tok = g_new0(GelToken,1);
	tok->token = g_strdup("Ans");
	g_hash_table_insert(dictionary,tok->token,tok);
	g_hash_table_insert(dictionary,g_strdup("ans"),tok);

	/*this is where the built in functions register into the global
	  dictionary*/
	gel_funclib_addall();
}

/*compare two GelEFunc's by their context numbers*/
/*static int
compare_func_bycontext(gconstpointer p1, gconstpointer p2)
{
	GelEFunc *func1 = (GelEFunc *)p1;
	GelEFunc *func2 = (GelEFunc *)p2;
	
	return func1->context < func2->context;
}*/

/*add a function struct to the dict (in current context)*/
GelEFunc *
d_addfunc (GelEFunc *func)
{
	GelEFunc *n;
	
	g_return_val_if_fail(func->context == context.top,func);
	
	/*we already found it (in current context)*/
	n = d_lookup_local(func->id);
	if(n) {
		d_replacefunc(n,func);
		return n;
	}

	context.stack->data = g_slist_prepend(context.stack->data,func);
	
	func->id->refs = g_slist_prepend(func->id->refs,func);
	func->id->curref = func;

	return func;
}

/*add a function struct to the dict (in global context)*/
GelEFunc *
d_addfunc_global (GelEFunc *func)
{
	GelEFunc *n;
	GSList *last;
	
	g_return_val_if_fail (func->context == 0, func);

	/* get the function in the lowest context */
	last = g_slist_last (func->id->refs);
	n = last != NULL ? last->data : NULL;
	/* if this function is global */
	if (n != NULL && n->context == 0) {
		d_replacefunc (n, func);
		return n;
	}

	last = g_slist_last (context.stack);
	g_assert (last != NULL);

	last->data = g_slist_prepend (last->data, func);
	
	func->id->refs = g_slist_append (func->id->refs, func);
	if (func->id->curref == NULL)
		func->id->curref = func;

	return func;
}

/*set value of an existing function (in local context), used for arguments
  WARNING, does not free the memory allocated by previous value!*/
int
d_setvalue(GelToken *id,GelETree *value)
{
	GelEFunc *f;
	f=d_lookup_local(id);
	if(!f || (f->type!=GEL_USER_FUNC &&
		  f->type!=GEL_VARIABLE_FUNC))
		return FALSE;
	f->data.user=value;
	return TRUE;
}

/*dictionary functions*/

/*lookup a function in the dictionary in the current context*/
GelEFunc *
d_lookup_local(GelToken *id)
{
	GelEFunc *func;
	
	if(!id ||
	   !id->refs)
		return NULL;
	
	/*the first one must be the lowest context*/
	func = id->refs->data;
	
	/*not in currect context and we only want the currect context ones*/
	if(func->context<context.top)
		return NULL;

	return func;
}

/*lookup a function in the dictionary NOT in the current context*/
GelEFunc *
d_lookup_global_up1(GelToken *id)
{
	GelEFunc *func;
	
	if(!id ||
	   !id->refs)
		return NULL;
	
	/*the first one must be the lowest context*/
	func = id->refs->data;
	
	if(func->context<context.top)
		return func;
	if(!id->refs->next)
		return NULL;
	
	return id->refs->next->data;
}

GelToken *
d_intern (const char *id)
{
	GelToken * tok = g_hash_table_lookup(dictionary,id);
	if(!tok) {
		tok = g_new0(GelToken,1);
		tok->token = g_strdup(id);
		g_hash_table_insert(dictionary,tok->token,tok);
	}
	return tok;
}

int
d_delete(GelToken *id)
{
	/*FIXME: Delete function!*/
	return FALSE;
}

/*clear all context dictionaries and pop out all the contexts except
  the global one
  also init the context stack if it hasn't been done*/
void
d_singlecontext(void)
{
	if(context.stack==NULL)
		d_initcontext();
	else
		while(context.top>0)
			d_freedict(d_popcontext());
}

/*free all memory allocated by a dictionary*/
void
d_freedict(GSList *n)
{
	GSList *li;
	for(li=n;li!=NULL;li=g_slist_next(li))
		d_freefunc(li->data);
	g_slist_free(n);
}

void
d_freefunc(GelEFunc *n)
{
	if(!n)
		return;
	g_assert(!n->id || g_slist_find(n->id->refs,n)==NULL);
	/*if(n->id) {
		n->id->refs = g_slist_remove(n->id->refs,n);
		n->id->curref = n->id->refs?n->id->refs->data:NULL;
	} */
	if((n->type == GEL_USER_FUNC ||
	    n->type == GEL_VARIABLE_FUNC) &&
	   n->data.user)
		gel_freetree(n->data.user);
	g_slist_free(n->named_args);
	/*prepend to free list*/
	n->data.next = free_funcs;
	free_funcs = n;
}

/*replace old with stuff from new and free new,
  new has to have the same id, also new->id should
  not hold a reference to new*/
void
d_replacefunc(GelEFunc *old,GelEFunc *new)
{
	g_return_if_fail(old && new);
	g_return_if_fail(old->id == new->id);

	if(old->type == GEL_USER_FUNC ||
	   old->type == GEL_VARIABLE_FUNC)
		gel_freetree(old->data.user);
	g_slist_free(old->named_args);
	memcpy(old,new,sizeof(GelEFunc));
	/*prepend to free list*/
	new->data.next = free_funcs;
	free_funcs = new;
}

/*set_ref*/
void
d_set_ref(GelEFunc *n,GelEFunc *ref)
{
	if(!n || !ref)
		return;
	if(n->type == GEL_USER_FUNC ||
	   n->type == GEL_VARIABLE_FUNC)
		gel_freetree(n->data.user);
	n->type = GEL_REFERENCE_FUNC;
	g_slist_free(n->named_args);
	n->nargs = 0;
	n->named_args = NULL;
	
	n->data.ref = ref;
}

/*set_value*/
void
d_set_value(GelEFunc *n,GelETree *value)
{
	if(!n || !value)
		return;
	if(n->type == GEL_USER_FUNC ||
	   n->type == GEL_VARIABLE_FUNC)
		gel_freetree(n->data.user);
	n->type = GEL_VARIABLE_FUNC;
	g_slist_free(n->named_args);
	n->nargs = 0;
	n->named_args = NULL;
	
	n->data.user = value;
}

/*push a new dictionary onto the context stack*/
int
d_addcontext(void)
{
	context.stack = g_slist_prepend(context.stack,NULL);
	context.top++;
	return TRUE;
}

/*gimme the last dictionary and pop the context stack*/
GSList *
d_popcontext(void)
{
	if(context.top==-1)
		return NULL;
	else {
		GSList *li;
		GSList *dict = context.stack->data;

		for(li=dict;li!=NULL;li=g_slist_next(li)) {
			GelEFunc *func = li->data;
			func->id->refs = g_slist_remove(func->id->refs,func);
			func->id->curref =
				func->id->refs?func->id->refs->data:NULL;
		}
		context.top--;
		li = context.stack;
		context.stack = g_slist_remove_link(context.stack,li);
		g_slist_free_1(li);
		return dict;
	}
}

/*gimme the current dictinary*/
GSList *
d_getcontext(void)
{
	if(context.top==-1)
		return NULL;
	else
		return context.stack->data;
}

void
d_protect_all(void)
{
	GSList *funcs;
	GSList *li;
	funcs = d_getcontext();
	if(!funcs) return;
	for(li=funcs;li;li=g_slist_next(li)) {
		GelEFunc *func = li->data;
		if(!func->id || strcmp(func->id->token,"Ans")==0)
			continue;
		if ( ! func->id->parameter)
			func->id->protected = 1;
	}
}
