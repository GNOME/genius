/* GENIUS Calculator
 * Copyright (C) 1997-2007 Jiri (George) Lebl
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

#ifndef _GELOUTPUT_H_
#define _GELOUTPUT_H_

#include <stdio.h>
#include "structs.h"

void		gel_output_printf_full		(GelOutput *gelo,
						 gboolean limit,
						 const char *format,
						 ...) G_GNUC_PRINTF (3, 4);
void		gel_output_printf		(GelOutput *gelo,
						 const char *format,
						 ...) G_GNUC_PRINTF (2, 3);
void		gel_output_string		(GelOutput *gelo,
						 const char *string);
void		gel_output_full_string		(GelOutput *gelo,
						 const char *string);

GelOutput *	gel_output_new			(void);
void		gel_output_ref			(GelOutput *gelo);
void		gel_output_unref		(GelOutput *gelo);

/* set input notify handler */
void		gel_output_set_notify		(GelOutput *gelo,
						 GelOutputNotifyFunc notify);
void		gel_output_push_nonotify	(GelOutput *gelo);
void		gel_output_pop_nonotify		(GelOutput *gelo);

/* user data */
/* usable outside of genius */
void		gel_output_set_data		(GelOutput *gelo,
						 gpointer data);
gpointer	gel_output_get_data		(GelOutput *gelo);

/* string output stuff */
void		gel_output_setup_string		(GelOutput *gelo, 
						 int line_length,
						 GelOutputLineFunc line_length_get);
char *		gel_output_snarf_string		(GelOutput *gelo);
const char *	gel_output_peek_string		(GelOutput *gelo);
void		gel_output_clear_string		(GelOutput *gelo);

/* set the internal gstring to the given one, useful if you want
   to print into your own gstring.  Just make sure to set it to
   null before the object is destroyed */
void		gel_output_set_gstring		(GelOutput *gelo,
						 GString *gs);

/* file output stuff */
void		gel_output_setup_file		(GelOutput *gelo, 
						 FILE *outfp,
						 int line_length,
						 GelOutputLineFunc line_length_get);

/* Black hole setup */
void		gel_output_setup_black_hole	(GelOutput *gelo);

/* some accessors, this line length is not the current one,
 * just the default one */
void		gel_output_get_line_length	(GelOutput *gelo,
						 int *line_length,
						 GelOutputLineFunc *line_length_get);
void		gel_output_set_line_length	(GelOutput *gelo, 
						 int line_length,
						 GelOutputLineFunc line_length_get);
void		gel_output_set_length_limit	(GelOutput *gelo,
						 gboolean length_limit);

/* This gets the current line length (columns) */
int		gel_output_get_columns		(GelOutput *gelo);

/* quivalent to fflush on the FILE pointer */
void		gel_output_flush		(GelOutput *gelo);

#endif
