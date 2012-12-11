/* GENIUS Calculator
 * Copyright (C) 1997-2002 Jiri (George) Lebl
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
#include "funclib.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "compil.h"

#include "utype.h"

typedef struct _UserType UserType;
struct _UserType {
	int type;
	char *name;
	GelUTDestroyFunc destructor;
	GelUTCopyFunc copier;
};

static GHashTable *types_ht = NULL;
static GArray *types_ar = NULL;
static int type_cookie = 0;

static void
do_init(void)
{
	if(!types_ht)
		types_ht = g_hash_table_new(g_str_hash,g_str_equal);
	else
		types_ar = g_array_new(FALSE,FALSE,sizeof(gpointer));
}

/* make a new unique identifier for a user type and set the destruction
   function which is called when a variable of that type is killed */
int
gel_new_user_type(char *name,
		  GelUTDestroyFunc destructor,
		  GelUTCopyFunc copier)
{
	UserType *ut;

	g_return_val_if_fail(name!=NULL,-1);

	do_init();

	if(g_hash_table_lookup(types_ht,name))
		return -1;

	ut = g_new0(UserType,1);
	ut->type = type_cookie++;
	ut->name = g_strdup(name);
	ut->destructor = destructor;
	ut->copier = copier;

	types_ar = g_array_append_val(types_ar,ut);
	g_hash_table_insert(types_ht,ut->name,ut);

	return ut->type;
}

/* destroy the type itself, not a specific variable */
void
gel_destroy_user_type(int type)
{
	UserType *ut;

	g_return_if_fail(type<type_cookie);
	g_return_if_fail(type>=0);

	ut = g_array_index(types_ar,UserType *,type);
	if(!ut) return;

	g_array_index(types_ar,UserType *,type) = NULL;

	g_hash_table_remove(types_ht,ut->name);

	g_free(ut->name);
	g_free(ut);
}

/* lookup the id of a type */
int
gel_get_user_type(const char *name)
{
	UserType *ut;
	ut = g_hash_table_lookup (types_ht, name);
	if (ut == NULL)
		return -1;
	else
		return ut->type;
}

/* lookup the name of type 'type' */
char *
gel_get_user_type_name(int type)
{
	UserType *ut;

	g_return_val_if_fail(type<type_cookie,NULL);
	g_return_val_if_fail(type>=0,NULL);

	ut = g_array_index(types_ar,UserType *,type);
	if(!ut) return NULL;

	return ut->name;
}

/* make a new variable of type 'type' with data 'data' */
GelETree *
gel_make_new_user_variable(int type, gpointer data)
{
	GelETree *n;
	UserType *ut;

	g_return_val_if_fail(type<type_cookie,NULL);
	g_return_val_if_fail(type>=0,NULL);

	ut = g_array_index(types_ar,UserType *,type);
	if(!ut) return NULL;

	GEL_GET_NEW_NODE(n);

	n->type = GEL_USERTYPE_NODE;
	n->ut.next = NULL;
	n->ut.ttype = ut->type;
	n->ut.data = data;

	return n;
}

/* free the 'data' of type 'type' using the destructor */
void
gel_free_user_variable_data(int type, gpointer data)
{
	UserType *ut;

	g_return_if_fail(type<type_cookie);
	g_return_if_fail(type>=0);

	ut = g_array_index(types_ar,UserType *,type);
	if(!ut) return;

	if(ut->destructor)
		(*ut->destructor)(data);
}

/* copy the 'data' of type 'type' using the copier */
gpointer
gel_copy_user_variable_data(int type, gpointer data)
{
	UserType *ut;

	g_return_val_if_fail(type<type_cookie,NULL);
	g_return_val_if_fail(type>=0,NULL);

	ut = g_array_index(types_ar,UserType *,type);
	if(!ut) return NULL;

	if(ut->copier)
		return (*ut->copier)(data);
	else
		return NULL;
}
