/* Glade helper routines
 *
 * Author: George Lebl
 * (c) 2000 Eazel, Inc.
 * (c) 2002 George Lebl
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

#include <config.h>

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "ve-i18n.h"

#include "ve-misc.h"
#include "ve-miscui.h"

#include "glade-helper.h"

static void glade_helper_no_interface (const char *filename);
static void glade_helper_bad_interface (const char *filename,
					const char *widget,
					GType type);
static void glade_helper_bad_columns (const char *filename,
				      const char *widget,
				      GType type,
				      int columns);

static GList *ve_glade_directories = NULL;
static gboolean ve_search_gnome_dirs = TRUE;

GladeXML *
glade_helper_load (const char *file, const char *widget,
		   GType expected_type,
		   gboolean dump_on_destroy)
{
	char *f;
	GladeXML *xml;
	GtkWidget *w;

	g_return_val_if_fail (file != NULL, NULL);

	f = glade_helper_find_glade_file (file);

	if (f == NULL) {
		glade_helper_no_interface (file);
		exit (1);
	}

	/* FIXME: eek use of PACKAGE */
	xml = glade_xml_new (f, widget, GETTEXT_PACKAGE);

	g_free (f);

	/* in case we can't load the interface, bail */
	if (xml == NULL) {
		glade_helper_no_interface (file);
		exit (1);
	}

	w = glade_xml_get_widget (xml, widget);
	if (w == NULL ||
	    ! G_TYPE_CHECK_INSTANCE_TYPE (w, expected_type)) {
		glade_helper_bad_interface (xml->filename != NULL ?
					      xml->filename :
					      _("(memory buffer)"),
					    widget,
					    expected_type);
		exit (1);
	}

	if (dump_on_destroy) {
		g_object_set_data_full (G_OBJECT (w),
					"GladeXML",
					xml,
					(GtkDestroyNotify) g_object_unref);
	}

	return xml;
}

GtkWidget *
glade_helper_load_widget (const char *file, const char *widget,
			  GType expected_type)
{
	GladeXML *xml;
	GtkWidget *w;

	/* this is guaranteed to return non-NULL, otherwise we
	 * would have exited */
	xml = glade_helper_load (file, widget, expected_type, FALSE);

	w = glade_xml_get_widget (xml, widget);
	if (w == NULL ||
	    ! G_TYPE_CHECK_INSTANCE_TYPE (w, expected_type)) {
		glade_helper_bad_interface (xml->filename != NULL ?
					      xml->filename :
					      _("(memory buffer)"),
					    widget,
					    expected_type);
		exit (1);
	}

	g_object_unref (G_OBJECT (xml));
	
	return w;
}

GtkWidget *
glade_helper_get (GladeXML *xml, const char *widget, GType expected_type)
{
	GtkWidget *w;

	w = glade_xml_get_widget (xml, widget);

	if (w == NULL ||
	    ! G_TYPE_CHECK_INSTANCE_TYPE (w, expected_type)) {
		glade_helper_bad_interface (xml->filename != NULL ?
					      xml->filename :
					      _("(memory buffer)"),
					    widget,
					    expected_type);
		exit (1);
	}

	return w;
}

static void
glade_helper_bad_interface (const char *filename, const char *widget,
			    GType type)
{
	GtkWidget *dlg;
	char *typestring;
	const char *typename;
	char *tmp;

	if (type != 0)
		typename = g_type_name (type);
	else
		typename = NULL;

	if (typename == NULL)
		typestring = g_strdup_printf (" (%s)", typename);
	else
		typestring = g_strdup ("");

	tmp = ve_filename_to_utf8 (filename);
	
	dlg = ve_hig_dialog_new (NULL /* parent */,
				 0 /* flags */,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 FALSE /* markup */,
				 _("Cannot load user interface"),
				 _("An error occurred while loading user "
				   "interface element %s%s from file %s.  "
				   "Possibly the glade interface description was "
				   "corrupted.  "
				   "%s cannot continue and will exit now.  "
				   "You should check your "
				   "installation of %s or reinstall %s."),
				 widget,
				 typestring,
				 tmp,
				 PACKAGE,
				 PACKAGE,
				 PACKAGE);

	g_free (typestring);

	g_warning (_("Glade file is on crack! Make sure the correct "
		     "file is installed!\nfile: %s widget: %s"),
		   tmp, widget);

	g_free (tmp);

	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}

static void
glade_helper_bad_columns (const char *filename, const char *widget,
			  GType type, int columns)
{
	GtkWidget *dlg;
	char *typestring;
	const char *typename;
	char *tmp;

	if (type != 0)
		typename = g_type_name (type);
	else
		typename = NULL;

	if (typename == NULL)
		typestring = g_strdup_printf (" (%s)", typename);
	else
		typestring = g_strdup ("");

	tmp = ve_filename_to_utf8 (filename);
	
	dlg = ve_hig_dialog_new (NULL /* parent */,
				 0 /* flags */,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 FALSE /* markup */,
				 _("Cannot load user interface"),
				 ngettext ("An error occurred while loading the "
					   "user interface element %s%s from "
					   "file %s. CList type widget should "
					   "have %d column. Possibly the glade "
					   "interface description was "
					   "corrupted. %s cannot continue and "
					   "will exit now. You should check "
					   "your installation of %s or "
					   "reinstall %s.",
					   "An error occurred while loading the "
					   "user interface element %s%s from "
					   "file %s. CList type widget should "
					   "have %d columns. Possibly the "
					   "glade interface description was "
					   "corrupted. %s cannot continue and "
					   "will exit now. You should check "
					   "your installation of %s or "
					   "reinstall %s.",
					   columns),
				 widget,
				 typestring,
				 tmp,
				 columns,
				 PACKAGE,
				 PACKAGE,
				 PACKAGE);

	g_free (typestring);

	g_warning (_("Glade file is on crack! Make sure the correct "
		     "file is installed!\nfile: %s widget: %s "
		     "expected clist columns: %d"),
		   tmp, widget, columns);

	g_free (tmp);

	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}

static void
glade_helper_no_interface (const char *filename)
{
	GtkWidget *dlg;
	char *tmp;

	tmp = ve_filename_to_utf8 (filename);
	
	dlg = ve_hig_dialog_new (NULL /* parent */,
				 0 /* flags */,
				 GTK_MESSAGE_ERROR,
				 GTK_BUTTONS_OK,
				 FALSE /* markup */,
				 _("Cannot load user interface"),
				 _("An error occurred while loading the user "
				   "interface from file %s.  "
				   "Possibly the glade interface description was "
				   "not found.  "
				   "%s cannot continue and will exit now.  "
				   "You should check your "
				   "installation of %s or reinstall %s."),
				 tmp,
				 PACKAGE,
				 PACKAGE,
				 PACKAGE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);
	g_warning (_("No interface could be loaded. This is BAD! (file: %s)"),
		   tmp);

	g_free (tmp);

	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}

/**
 * glade_helper_find_glade_file:
 * @file:  glade filename
 *
 * Description:  Finds a glade file in the same directories that
 * glade_helper would look in.  A utility if you use glade elsewhere in your
 * application to make using glade simpler.
 *
 * Returns:  Newly allocated string with the full path, or %NULL
 * if not found.
 **/
char *
glade_helper_find_glade_file (const char *file)
{
	g_return_val_if_fail (file != NULL, NULL);

	if (ve_search_gnome_dirs)
		return ve_find_file (file, ve_glade_directories);
	else
		return ve_find_file_simple (file, ve_glade_directories);
}

/**
 * glade_helper_add_glade_directory:
 * @directory:  directory with glade files
 *
 * Description:  Add a standard glade directory to search for
 * glade files.
 **/
void
glade_helper_add_glade_directory (const char *directory)
{
	ve_glade_directories = g_list_append (ve_glade_directories,
					      g_strdup (directory));
}

/**
 * glade_helper_seach_gnome_dirs:
 * @search_gnome_dirs:  boolean
 *
 * Description:  Sets if the gnome directories are searched.  Useful if
 * your program does not use gnome_program_ and you don't want glade_helper
 * to invoke gnome_program initialization by mistake.  This is because
 * it is possible for glade helper to try to use gnome_program_ routines
 * by default.
 **/
void
glade_helper_search_gnome_dirs (gboolean search_gnome_dirs)
{
	ve_search_gnome_dirs = search_gnome_dirs;
}

/* Make label surrounded by tag (if tag = "tag" add <tag>text</tag>) */
void
glade_helper_tagify_label (GladeXML *xml,
			   const char *name,
			   const char *tag)
{
	const char *lbl;
	char *s;
	GtkWidget *label = glade_helper_get (xml, name, GTK_TYPE_WIDGET);
	if (GTK_IS_BUTTON (label)) {
		label = GTK_BIN(label)->child;
	}
	if ( ! GTK_IS_LABEL (label)) {
		glade_helper_bad_interface (xml->filename,
					    name,
					    GTK_TYPE_LABEL);
		exit (1);
	}
	lbl = gtk_label_get_text (GTK_LABEL (label));
	s = g_strdup_printf ("<%s>%s</%s>", tag, lbl, tag);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_label (GTK_LABEL (label), s);
	g_free (s);
}
