/* GENIUS Calculator
 * Copyright (C) 2004-2021 Jiri (George) Lebl
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
	char *editor_color_scheme;
} GeniusSetup;

extern GeniusSetup genius_setup;
extern GtkWidget *genius_window;
extern int gel_calc_running;
extern long total_errors;

void genius_interrupt_calc (void);

void genius_setup_window_cursor (GtkWidget *win, GdkCursorType type);
void genius_unsetup_window_cursor (GtkWidget *win);

gboolean genius_ask_question (GtkWidget *parent, const char *question);
void genius_display_error (GtkWidget *parent, const char *err);

void gel_printout_infos_parent (GtkWidget *parent);

/* same as GNOME */
#define GENIUS_PAD          8
#define GENIUS_PAD_SMALL    4
#define GENIUS_PAD_BIG      12

#endif /* GNOME_GENIUS_H_ */
