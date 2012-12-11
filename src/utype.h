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

#ifndef _UTYPE_H_
#define _UTYPE_H_

/*declarations of structures*/
#include "structs.h"

/* User type stuff */
typedef void (*GelUTDestroyFunc)(gpointer data);
typedef gpointer (*GelUTCopyFunc)(gpointer data);
/* make a new unique identifier for a user type and set the destruction
   function which is called when a variable of that type is killed,
   and a function to copy the data,
   returns -1 on error */
int gel_new_user_type(char *name,
		      GelUTDestroyFunc destructor,
		      GelUTCopyFunc copier);
/* destroy the type itself, not a specific variable */
void gel_destroy_user_type(int type);
/* lookup the id of a type */
int gel_get_user_type(const char *name);
/* lookup the name of type 'type' (don't free!)*/
char * gel_get_user_type_name(int type);
/* make a new variable of type 'type' with data 'data' */
GelETree *gel_make_new_user_variable(int type, gpointer data);
/* free the 'data' of type 'type' using the destructor */
void gel_free_user_variable_data(int type, gpointer data);
/* copy the 'data' of type 'type' using the copier */
gpointer gel_copy_user_variable_data(int type, gpointer data);

#endif
