/* GENIUS Calculator
 * Copyright (C) 2004-2007 George Lebl
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

#ifndef GNOME_GENIUS_H_
#define GNOME_GENIUS_H_

typedef struct {
	gboolean error_box;
	gboolean info_box;
	gboolean blinking_cursor;
	int scrollback;
	char *font;
	gboolean black_on_white;
	gboolean output_remember; /* remember output settings */
	gboolean precision_remember; /* remember precision setting */
} GeniusSetup;

extern GeniusSetup genius_setup;
extern GtkWidget *genius_window;
extern GHashTable *uncompiled;
extern calcstate_t calcstate;

void genius_interrupt_calc (void);

void genius_setup_window_cursor (GtkWidget *win, GdkCursorType type);
void genius_unsetup_window_cursor (GtkWidget *win);

gboolean genius_ask_question (GtkWidget *parent, const char *question);
void genius_display_error (GtkWidget *parent, const char *err);

/* same as GNOME */
#define GENIUS_PAD          8
#define GENIUS_PAD_SMALL    4
#define GENIUS_PAD_BIG      12

#endif /* GNOME_GENIUS_H_ */
