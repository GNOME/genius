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

#ifndef _GENIUS_UTIL_H_
#define _GENIUS_UTIL_H_

#include <stdlib.h>

/*shift the sring to the right by n*/
void gel_shiftstr(char *s,int n);

/*allocate new space in s for p and append it*/
char * gel_appendstr(char *s,const char *p);

/*allocate new space in s for p and prepend it*/
char * gel_prependstr(char *s,const char *p);

/*stack operations using GSList*/
void gel_stack_push(GSList **stack, gpointer data);
gpointer gel_stack_pop(GSList **stack);
gpointer gel_stack_peek(GSList **stack);

/*escape/unescape \n\t\r\b\a\"\\ ... unescape also \<anything>==<anything>,
  nounescape means characters which to leave escaped*/
char *gel_unescape_string(char *s, char *end, char *nounescape);
/*escape also "'s and \'s*/
char *gel_escape_string(char *s);

#endif
