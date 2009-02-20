/* Misc UI routines
 *
 * (c) 2000 Eazel, Inc.
 * (c) 2001 George Lebl
 * (c) 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef VE_MISCUI_H
#define VE_MISCUI_H

void		ve_entry_set_red (GtkWidget *w,
				  gboolean state);

GtkWidget *	ve_hig_dialog_new (GtkWindow      *parent,
				   GtkDialogFlags flags,
				   GtkMessageType type,
				   GtkButtonsType buttons,
				   gboolean       markup,
				   const gchar    *header,
				   const gchar    *messagefmt,
				   ...) G_GNUC_PRINTF (7, 8);

int		ve_dialog_run_nonmodal (GtkDialog *dialog);


#endif /* VE_MISCUI_H */
