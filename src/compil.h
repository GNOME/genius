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

#ifndef _COMPIL_H_
#define _COMPIL_H_

#include "mpwrap.h"

/*declarations of structures*/
#include "structs.h"

char *gel_compile_tree (GelETree *t);
GelETree *gel_decompile_tree (char *s);

char * gel_encode_string (const char *s);
char * gel_decode_string (const char *s);

#endif
