/* GENIUS Calculator
 * Copyright (C) 1997-2016 Jiri (George) Lebl
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

#ifndef EXAMPLES_H__
#define EXAMPLES_H__

typedef struct _GelExample {
	char *category;
	char *name;
	char *file;
} GelExample;

typedef struct _GelExampleCategory {
	char *name;
	GSList *examples;
} GelExampleCategory;

void gel_read_example_list(void);
extern GSList *gel_example_list;
extern GSList *gel_example_categories_list;

#endif /* EXAMPLES_H__ */
