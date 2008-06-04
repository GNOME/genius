/* GENIUS Calculator
 * Copyright (C) 1997-2008 Jiri (George) Lebl
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

#include <gnome.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "calc.h"
#include "util.h"
#include "dict.h"
#include "eval.h"
#include "lexer.h"
#include "geloutput.h"
#include "graphing.h"

#include "plugin.h"
#include "inter.h"

#include "binreloc.h"

#include <vicious.h>

#include <readline/readline.h>
#include <readline/history.h>

#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcelanguage.h>
#ifdef HAVE_GTKSOURCEVIEW2
#include <gtksourceview/gtksourcelanguagemanager.h>
#else
#include <gtksourceview/gtksourcelanguagesmanager.h>
#include <gtksourceview/gtksourceprintjob.h>
#endif
#endif

#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gnome-genius.h"

/*Globals:*/

/*calculator state*/
calcstate_t curstate={
	128,
	12,
	FALSE,
	FALSE,
	FALSE,
	5,
	TRUE,
	10,
	0, /* output_style */
	2000000, /* max_nodes */
	20, /* chop */
	5 /* chop_when */
	};

#define MAX_CHOP 1000
	
extern int parenth_depth;
extern gboolean interrupted;
extern const char *genius_toplevels[];

GtkWidget *genius_window = NULL;

static GtkWidget *setupdialog = NULL;
static GtkWidget *term = NULL;
static GtkWidget *appbar = NULL;
static GtkWidget *notebook = NULL;
static GtkTooltips *tips;
static GString *errors=NULL;
static GString *infos=NULL;

static char *clipboard_str = NULL;

static int calc_running = 0;

static int errors_printed = 0;

static char *last_dir = NULL;

static GList *prog_menu_items = NULL;

static char *genius_datadir = NULL;
static char *genius_datadir_sourceview = NULL;

static gboolean genius_in_dev_dir = FALSE;

GeniusSetup genius_setup = {
	FALSE /* error_box */,
	TRUE /* info_box */,
	TRUE /* blinking_cursor */,
	1000 /* scrollback */,
	NULL /* font */,
	FALSE /* black on white */,
	FALSE /* output_remember */,
	FALSE /* precision_remember */
};

typedef struct {
	char *name;
	char *vname; /* visual name */
	int ignore_changes;
	int curline;
	gboolean changed;
	gboolean real_file;
	gboolean selected;
	GtkWidget *tv;
	GtkTextBuffer *buffer;
	GtkWidget *label;
} Program;

enum {
	TARGET_URI_LIST = 100
};

#define TERMINAL_PALETTE_SIZE 16
const GdkColor
terminal_palette_black_on_white[TERMINAL_PALETTE_SIZE] =
{
  { 0, 0x0000, 0x0000, 0x0000 },
  { 0, 0xaaaa, 0x0000, 0x0000 },
  { 0, 0x0000, 0x8888, 0x0000 },
  { 0, 0xaaaa, 0x5555, 0x0000 },
  { 0, 0x0000, 0x0000, 0xaaaa },
  { 0, 0xaaaa, 0x0000, 0xaaaa },
  { 0, 0x0000, 0xaaaa, 0xaaaa },
  { 0, 0xaaaa, 0xaaaa, 0xaaaa },

  { 0, 0x0000, 0x0000, 0x0000 },
  { 0, 0xaaaa, 0x0000, 0x0000 },
  { 0, 0x0000, 0x8888, 0x0000 },
  { 0, 0xaaaa, 0x5555, 0x0000 },
  { 0, 0x0000, 0x0000, 0xaaaa },
  { 0, 0xaaaa, 0x0000, 0xaaaa },
  { 0, 0x0000, 0x8888, 0xaaaa },
  { 0, 0xaaaa, 0xaaaa, 0xaaaa },
};

static GtkTargetEntry drag_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST },
};

static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);

static Program *selected_program = NULL;
static Program *running_program = NULL;

static char *default_console_font = NULL;

pid_t helper_pid = -1;

static FILE *torlfp = NULL;
static int fromrl;

static char *torlfifo = NULL;
static char *fromrlfifo = NULL;

static char *arg0 = NULL;

static void new_callback (GtkWidget *menu_item, gpointer data);
static void open_callback (GtkWidget *w);
static void save_callback (GtkWidget *w);
static void save_all_cb (GtkWidget *w);
static void save_console_cb (GtkWidget *w);
static void save_as_callback (GtkWidget *w);
static void close_callback (GtkWidget *menu_item, gpointer data);
static void load_cb (GtkWidget *w);
static void reload_cb (GtkWidget *w);
static void quitapp (GtkWidget * widget, gpointer data);
#ifdef HAVE_GTKSOURCEVIEW
static void setup_undo_redo (void);
static void undo_callback (GtkWidget *menu_item, gpointer data);
static void redo_callback (GtkWidget *menu_item, gpointer data);
#endif
static void cut_callback (GtkWidget *menu_item, gpointer data);
static void copy_callback (GtkWidget *menu_item, gpointer data);
static void paste_callback (GtkWidget *menu_item, gpointer data);
static void clear_cb (GtkClipboard *clipboard, gpointer owner);
static void copy_cb (GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner);
static void copy_answer (void);
static void copy_as_plain (GtkWidget *menu_item, gpointer data);
static void copy_as_latex (GtkWidget *menu_item, gpointer data);
static void copy_as_troff (GtkWidget *menu_item, gpointer data);
static void copy_as_mathml (GtkWidget *menu_item, gpointer data);
static void next_tab (GtkWidget *menu_item, gpointer data);
static void prev_tab (GtkWidget *menu_item, gpointer data);
static void prog_menu_activated (GtkWidget *item, gpointer data);
static void setup_calc (GtkWidget *widget, gpointer data);
static void run_program (GtkWidget *menu_item, gpointer data);
static void show_user_vars (GtkWidget *menu_item, gpointer data);
static void monitor_user_var (GtkWidget *menu_item, gpointer data);
static void full_answer (GtkWidget *menu_item, gpointer data);
static void warranty_call (GtkWidget *widget, gpointer data);
static void aboutcb (GtkWidget * widget, gpointer data);
static void help_on_function (GtkWidget *menuitem, gpointer data);
static void executing_warning (void);
static void display_warning (GtkWidget *parent, const char *warn);

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_NEW_ITEM(N_("_New Program"), N_("Create new program tab"), new_callback, NULL),
	GNOMEUIINFO_MENU_OPEN_ITEM (open_callback,NULL),
#define FILE_SAVE_ITEM 2
	GNOMEUIINFO_MENU_SAVE_ITEM (save_callback,NULL),
#define FILE_SAVE_ALL_ITEM 3
	GNOMEUIINFO_ITEM_STOCK(N_("Save All _Unsaved"),N_("Save all unsaved programs"), save_all_cb, GTK_STOCK_SAVE),
#define FILE_SAVE_AS_ITEM 4
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (save_as_callback,NULL),
#define FILE_RELOAD_ITEM 5
	GNOMEUIINFO_ITEM_STOCK(N_("_Reload from Disk"),N_("Reload the selected program from disk"), reload_cb, GTK_STOCK_REVERT_TO_SAVED),
#define FILE_CLOSE_ITEM 6
	GNOMEUIINFO_MENU_CLOSE_ITEM (close_callback, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK(N_("_Load and Run..."),N_("Load and execute a file in genius"), load_cb, GTK_STOCK_OPEN),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK(N_("Save Console Ou_tput..."),N_("Save what is visible on the console (including scrollback) to a text file"), save_console_cb, GTK_STOCK_SAVE),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (quitapp,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo edit_menu[] = {  
#ifdef HAVE_GTKSOURCEVIEW
#define EDIT_UNDO_ITEM 0
	GNOMEUIINFO_MENU_UNDO_ITEM(undo_callback,NULL),
#define EDIT_REDO_ITEM 1
	GNOMEUIINFO_MENU_REDO_ITEM(redo_callback,NULL),
	GNOMEUIINFO_SEPARATOR,
#define EDIT_CUT_ITEM 3
	GNOMEUIINFO_MENU_CUT_ITEM(cut_callback,NULL),
#define EDIT_COPY_ITEM 4
	GNOMEUIINFO_MENU_COPY_ITEM(copy_callback,NULL),
#else
#define EDIT_CUT_ITEM 0
	GNOMEUIINFO_MENU_CUT_ITEM(cut_callback,NULL),
#define EDIT_COPY_ITEM 1
	GNOMEUIINFO_MENU_COPY_ITEM(copy_callback,NULL),
#endif
	GNOMEUIINFO_MENU_PASTE_ITEM(paste_callback,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As Plain _Text"),
			       N_("Copy last answer into the clipboard in plain text"),
			       copy_as_plain,
			       GTK_STOCK_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As _LaTeX"),
			       N_("Copy last answer into the clipboard as LaTeX"),
			       copy_as_latex,
			       GTK_STOCK_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As _MathML"),
			       N_("Copy last answer into the clipboard as MathML"),
			       copy_as_mathml,
			       GTK_STOCK_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As T_roff"),
			       N_("Copy last answer into the clipboard as Troff eqn"),
			       copy_as_troff,
			       GTK_STOCK_COPY),
	GNOMEUIINFO_END,
};

static GnomeUIInfo settings_menu[] = {  
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(setup_calc,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo calc_menu[] = {  
#define CALC_RUN_ITEM 0
	{ GNOME_APP_UI_ITEM, N_("_Run"), N_("Run current program"),
		(gpointer)run_program, NULL, NULL, \
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE, 
		GDK_r, (GdkModifierType) GDK_CONTROL_MASK, NULL },

	GNOMEUIINFO_ITEM_STOCK(N_("_Interrupt"),N_("Interrupt current calculation"),genius_interrupt_calc,GTK_STOCK_STOP),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Show _Full Answer"), N_("Show the full text of last answer"), full_answer, GTK_STOCK_DIALOG_INFO),
	GNOMEUIINFO_ITEM_STOCK (N_("Show User _Variables"), N_("Show the current value of all user variables"), show_user_vars, GTK_STOCK_DIALOG_INFO),
	GNOMEUIINFO_ITEM_STOCK (N_("_Monitor a Variable"), N_("Monitor a variable continuously"), monitor_user_var, GTK_STOCK_DIALOG_INFO),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("_Plot"), N_("Plot a function"), genius_plot_dialog, GNOME_STOCK_BOOK_OPEN),
	GNOMEUIINFO_END,
};

static GnomeUIInfo help_menu[] = {  
	GNOMEUIINFO_HELP("genius"),
	GNOMEUIINFO_ITEM_STOCK (N_("_Help on Function"),
				N_("Help on a function or a command"),
				help_on_function,
				GTK_STOCK_HELP),
	GNOMEUIINFO_ITEM_STOCK (N_("_Warranty"),
				N_("Display warranty information"),
				warranty_call,
				GTK_STOCK_HELP),
	GNOMEUIINFO_MENU_ABOUT_ITEM(aboutcb,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo plugin_menu[] = {
	GNOMEUIINFO_END,
};

static GnomeUIInfo programs_menu[] = {
	{ GNOME_APP_UI_ITEM, N_("_Next Tab"), N_("Go to next tab"),
		(gpointer)next_tab, NULL, NULL, \
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_GO_FORWARD, 
		GDK_Page_Down, (GdkModifierType) GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, N_("_Previous Tab"), N_("Go to previous tab"),
		(gpointer)prev_tab, NULL, NULL, \
		GNOME_APP_PIXMAP_STOCK, GTK_STOCK_GO_BACK, 
		GDK_Page_Up, (GdkModifierType) GDK_CONTROL_MASK, NULL },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM (N_("_Console"),
			  N_("Go to the console tab"),
			  prog_menu_activated,
			  NULL),

	GNOMEUIINFO_END,
};
  
static GnomeUIInfo genius_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE(file_menu),
	GNOMEUIINFO_MENU_EDIT_TREE(edit_menu),
	GNOMEUIINFO_SUBTREE(N_("_Calculator"),calc_menu),
#define PLUGINS_MENU 3
	GNOMEUIINFO_SUBTREE(N_("P_lugins"),plugin_menu),
#define PROGRAMS_MENU 4
	GNOMEUIINFO_SUBTREE(N_("_Programs"),programs_menu),
	GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
	GNOMEUIINFO_MENU_HELP_TREE(help_menu),
	GNOMEUIINFO_END,
};

/* toolbar */
static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK(N_("Interrupt"),N_("Interrupt current calculation"),genius_interrupt_calc,GTK_STOCK_STOP),
#define TOOLBAR_RUN_ITEM 1
	GNOMEUIINFO_ITEM_STOCK(N_("Run"),N_("Run current program"),run_program, GTK_STOCK_EXECUTE),
	GNOMEUIINFO_ITEM_STOCK(N_("New"),N_("Create new program tab"), new_callback, GTK_STOCK_NEW),
	GNOMEUIINFO_ITEM_STOCK(N_("Open"),N_("Open a GEL file for running"), open_callback, GTK_STOCK_OPEN),
	GNOMEUIINFO_ITEM_STOCK(N_("Plot"), N_("Plot a function"), genius_plot_dialog, GNOME_STOCK_BOOK_OPEN),
	GNOMEUIINFO_ITEM_STOCK(N_("Exit"),N_("Exit genius"), quitapp, GTK_STOCK_QUIT),
	GNOMEUIINFO_END,
};


#define ELEMENTS(x) (sizeof (x) / sizeof (x [0]))

void
genius_setup_window_cursor (GtkWidget *win, GdkCursorType type)
{
	GdkCursor *cursor = gdk_cursor_new (type);
	if (win != NULL && win->window != NULL)
		gdk_window_set_cursor (win->window, cursor);
	gdk_cursor_unref (cursor);
}

void
genius_unsetup_window_cursor (GtkWidget *win)
{
	if (win != NULL && win->window != NULL)
		gdk_window_set_cursor (win->window, NULL);
}


static int
count_char (const char *s, char c)
{
	int i = 0;
	while(*s) {
		if(*(s++) == c)
			i++;
	}
	return i;
}

static gboolean
uri_exists (const gchar* text_uri)
{
	GnomeVFSURI *uri;
	gboolean res;
		
	g_return_val_if_fail (text_uri != NULL, FALSE);
	
	uri = gnome_vfs_uri_new (text_uri);
	g_return_val_if_fail (uri != NULL, FALSE);

	res = gnome_vfs_uri_exists (uri);

	gnome_vfs_uri_unref (uri);

	return res;
}

static void
setup_term_color (void)
{
	if (genius_setup.black_on_white) {
		GdkColor black = {0, 0, 0, 0};
		GdkColor white = {0, 65535, 65535, 65535};
		vte_terminal_set_colors (VTE_TERMINAL (term),
					 &black,
					 &white,
					 terminal_palette_black_on_white,
					 TERMINAL_PALETTE_SIZE);
	} else {
		vte_terminal_set_default_colors (VTE_TERMINAL (term));
	}
}

static void
dialog_entry_activate (GtkWidget *e, gpointer data)
{
	GtkWidget *d = data;
	gtk_dialog_response (GTK_DIALOG (d), GTK_RESPONSE_OK);
}

static void
help_on_function (GtkWidget *menuitem, gpointer data)
{
	GtkWidget *d;
	GtkWidget *e;
	int ret;

	d = gtk_dialog_new_with_buttons
		(_("Help on Function"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OK, GTK_RESPONSE_OK,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);

	gtk_dialog_set_has_separator (GTK_DIALOG (d), FALSE);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox),
			    gtk_label_new (_("Function or command name:")),
			    FALSE, FALSE, 0);

	e = gtk_entry_new ();
	g_signal_connect (G_OBJECT (e), "activate",
			  G_CALLBACK (dialog_entry_activate), d);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox),
			    e,
			    FALSE, FALSE, 0);

run_help_dlg_again:
	gtk_widget_show_all (d);
	ret = gtk_dialog_run (GTK_DIALOG (d));

	if (ret == GTK_RESPONSE_OK) {
		char *txt = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (e))));
		GelHelp *help = get_help (txt, FALSE /* insert */);
		gboolean found = FALSE;
		int i;

		for (i = 0; genius_toplevels[i] != NULL; i++) {
			if (strcmp (txt, genius_toplevels[i]) == 0) {
				gel_call_help (txt);
				found = TRUE;
				break;
			}
		}

		if ( ! found && help == NULL) {
			char *similar_ids = gel_similar_possible_ids (txt);
			char *warn;
			if (similar_ids == NULL) {
				warn = g_strdup_printf
					(_("<b>Help on %s not found</b>"),
					 txt);
			} else {
				warn = g_strdup_printf
					(_("<b>Help on %s not found</b>\n\n"
					   "Perhaps you meant %s."),
					 txt, similar_ids);
				g_free (similar_ids);
			}
			display_warning (genius_window, warn);
			g_free (warn);
			g_free (txt);
			goto run_help_dlg_again;
		} else if ( ! found && help != NULL) {
			if (help->aliasfor != NULL)
				gel_call_help (help->aliasfor);
			else
				gel_call_help (txt);
		}
		g_free (txt);
	}

	gtk_widget_destroy (d);
}

/*display a message in a messagebox*/
static GtkWidget *
geniusbox (gboolean error,
	   gboolean always_textbox,
	   const char *textbox_title,
	   gboolean bind_response,
	   gboolean wrap,
	   const char *s)
{
	GtkWidget *mb;
	/* if less than 10 lines */
	if (count_char (ve_sure_string (s), '\n') <= 10 &&
	    ! always_textbox) {
		GtkMessageType type = GTK_MESSAGE_INFO;
		if (error)
			type = GTK_MESSAGE_ERROR;
		mb = gtk_message_dialog_new (GTK_WINDOW (genius_window) /* parent */,
					     0 /* flags */,
					     type,
					     GTK_BUTTONS_OK,
					     "%s",
					     ve_sure_string (s));
	} else {
		GtkWidget *sw;
		GtkWidget *tv;
		GtkTextBuffer *buffer;
		GtkTextIter iter;
		const char *title;

		if (textbox_title != NULL)
			title = textbox_title;
		else if (error)
			title = _("Error");
		else
			title = _("Information");

		mb = gtk_dialog_new_with_buttons
			(title,
			 GTK_WINDOW (genius_window) /* parent */,
			 0 /* flags */,
			 GTK_STOCK_OK, GTK_RESPONSE_OK,
			 NULL);
		gtk_dialog_set_has_separator (GTK_DIALOG (mb), FALSE);
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mb)->vbox),
				    sw,
				    TRUE, TRUE, 0);

		tv = gtk_text_view_new ();
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
		gtk_text_view_set_editable (GTK_TEXT_VIEW (tv), FALSE);
 
		if (wrap)
			gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (tv),
						     GTK_WRAP_CHAR);
		else
			gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (tv),
						     GTK_WRAP_NONE);

		gtk_text_buffer_create_tag (buffer, "foo",
					    "editable", FALSE,
					    "family", "monospace",
					    NULL);

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, ve_sure_string (s), -1, "foo", NULL);

		gtk_container_add (GTK_CONTAINER (sw), tv);

		/* FIXME: 
		 * Perhaps should be smaller with smaller font ...
		 * ... */
		gtk_window_set_default_size
			(GTK_WINDOW (mb),
			 MIN (gdk_screen_width ()-50, 800), 
			 MIN (gdk_screen_height ()-50, 450));
	}
	if (bind_response) {
		g_signal_connect (G_OBJECT (mb), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
	}
	gtk_widget_show_all (mb);

	return mb;
}

static void
populate_var_box (GtkTextBuffer *buffer)
{
	GSList *all_contexts;
	GSList *funcs;
	GSList *context_names;
	GSList *li, *lic;
	GelOutput *out;
	GtkTextIter iter;
	GtkTextIter iter_end;
	gboolean printed_local_title = FALSE;

	/* delete everything */
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &iter_end, -1);
	gtk_text_buffer_delete (buffer, &iter, &iter_end);

	/* get iterator */
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

	all_contexts = d_get_all_contexts ();
	context_names = d_get_context_names ();
	funcs = d_getcontext_global ();

	out = gel_output_new ();
	gel_output_setup_string (out, 0, NULL);

	gtk_text_buffer_insert_with_tags_by_name
		(buffer, &iter, _("Global variables:\n\n"), -1, "title", NULL);

	for (li = funcs; li != NULL; li = li->next) {
		GelEFunc *f = li->data;
		if (f->type != GEL_VARIABLE_FUNC ||
		    f->id == NULL ||
		    /* only for toplevel */ f->id->parameter ||
		    /* only for toplevel */ f->id->protected_ ||
		    f->id->token == NULL ||
		    f->data.user == NULL ||
		    f->context > 0)
			continue;

		gel_print_etree (out, f->data.user, FALSE /*no toplevel, keep this short*/);

		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, f->id->token, -1, "variable", NULL);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, ": ", -1, "value", NULL);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, gel_output_peek_string (out), -1, "value", NULL);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, "\n", -1, "value", NULL);

		gel_output_clear_string (out);
	}


	if (d_curcontext () > 0) {
		int i = d_curcontext ();
		int cols = 0;

		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, _("\nFunction call stack:\n"), -1, "title", NULL);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, _("(depth of context in parentheses)\n\n"), -1, "note", NULL);

		/* go over all local contexts (not the last one, that is global) */
		for (lic = context_names; lic != NULL && lic->next != NULL; lic = lic->next) {
			GelToken *tok = lic->data;
			char *s;

			if (tok == NULL) {
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, "??", -1, "variable", NULL);
				cols += 2;
			} else {
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, tok->token, -1, "variable", NULL);
				cols += strlen (tok->token);
			}

			s = g_strdup_printf (" (%d)", i);
			cols += strlen (s);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, s, -1, "context", NULL);
			g_free (s);

			if (i <= 1) {
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, "\n", -1, "context", NULL);
				if (i < 1)
					printf ("EKI!!");
			} else if (cols >= 58) {
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, ",\n", -1, "context", NULL);
				cols = 0;
			} else {
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, ", ", -1, "context", NULL);
				cols += 2;
			}

			i--;
		}
	}


	/* go over all local contexts (not the last one, that is global) */
	for (lic = all_contexts; lic != NULL && lic->next != NULL; lic = lic->next) {
		for (li = lic->data; li != NULL; li = li->next) {
			GelEFunc *f = li->data;
			char *s;
			if (f->type != GEL_VARIABLE_FUNC ||
			    f->id == NULL ||
			    f->id->token == NULL ||
			    f->data.user == NULL ||
			    f->context <= 0)
				continue;

			if ( ! printed_local_title) {
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, _("\nLocal variables:\n"), -1, "title", NULL);
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, _("(depth of context in parentheses)\n\n"), -1, "note", NULL);
				printed_local_title = TRUE;
			}

			gel_print_etree (out, f->data.user, FALSE /*no toplevel, keep this short*/);

			s = g_strdup_printf (" (%d)", f->context);

			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, f->id->token, -1, "variable", NULL);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, s, -1, "context", NULL);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, ": ", -1, "value", NULL);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, gel_output_peek_string (out), -1, "value", NULL);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, "\n", -1, "value", NULL);

			g_free (s);
			gel_output_clear_string (out);
		}
	}

	gel_output_unref (out);
}

static void
var_box_response (GtkWidget *widget, gint resp, gpointer data)
{
	if (resp == 1) {
		populate_var_box (/* buffer */data);
	} else {
		gtk_widget_destroy (widget);
	}
}


static void
show_user_vars (GtkWidget *menu_item, gpointer data)
{
	static GtkWidget *var_box = NULL;
	GtkWidget *sw;
	GtkWidget *tv;
	static GtkTextBuffer *buffer = NULL;

	if (var_box != NULL) {
		populate_var_box (buffer);
		return;
	}

	var_box = gtk_dialog_new_with_buttons
		(_("User Variable Listing"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_REFRESH, 1,
		 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		 NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (var_box), FALSE);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (var_box)->vbox),
			    sw,
			    TRUE, TRUE, 0);

	tv = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (tv), FALSE);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));

	gtk_container_add (GTK_CONTAINER (sw), tv);

	/* FIXME: 
	 * Perhaps should be smaller with smaller font ...
	 * ... */
	gtk_window_set_default_size
		(GTK_WINDOW (var_box),
		 MIN (gdk_screen_width ()-50, 800), 
		 MIN (gdk_screen_height ()-50, 450));
	g_signal_connect (G_OBJECT (var_box), "response",
			  G_CALLBACK (var_box_response),
			  buffer);
	g_signal_connect (var_box, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &var_box);
	g_signal_connect (var_box, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &buffer);
	gtk_widget_show_all (var_box);

	gtk_text_buffer_create_tag (buffer, "variable",
				    "editable", FALSE,
				    "family", "monospace",
				    "weight", PANGO_WEIGHT_BOLD,
				    NULL);

	gtk_text_buffer_create_tag (buffer, "context",
				    "editable", FALSE,
				    "family", "monospace",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);

	gtk_text_buffer_create_tag (buffer, "value",
				    "editable", FALSE,
				    "family", "monospace",
				    NULL);

	gtk_text_buffer_create_tag (buffer, "title",
				    "editable", FALSE,
				    "scale", PANGO_SCALE_LARGE,
				    "weight", PANGO_WEIGHT_BOLD,
				    NULL);

	gtk_text_buffer_create_tag (buffer, "note",
				    "editable", FALSE,
				    "scale", PANGO_SCALE_SMALL,
				    "style", PANGO_STYLE_ITALIC,
				    NULL);

	populate_var_box (buffer);
}

typedef struct {
	guint tm;
	GelToken *var;
	GtkWidget *tv;
	GtkTextBuffer *buf;
	GtkWidget *updatecb;
	GelOutput *out;
} MonitorData;

static void
monitor_destroyed (GtkWidget *d, gpointer data)
{
	MonitorData *md = data;
	g_source_remove (md->tm);
	gel_output_unref (md->out);
	g_free (md);
}

static gboolean
monitor_timeout (gpointer data)
{
	MonitorData *md = data;
	char *s;
	GSList *li;
	GelEFunc *func;
	GtkTextIter iter;
	GtkTextIter iter_end;
	gboolean any_matrix = FALSE;

	if ( ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (md->updatecb)))
		return TRUE;

	/* get iterator */
	gtk_text_buffer_get_iter_at_offset (md->buf, &iter, 0);

	if (md->var == NULL ||
	    md->var->refs == NULL) {
		s = g_strdup_printf (_("%s undefined"), md->var->token);
		gtk_text_buffer_insert_with_tags_by_name
			(md->buf, &iter, s, -1, "value", NULL);
		g_free (s);
	} else {
		int vars = 0;

		for (li = md->var->refs; li != NULL; li = li->next) {
			func = li->data;
			if ((func->type != GEL_VARIABLE_FUNC &&
			     func->type != GEL_USER_FUNC) ||
			    func->id == NULL ||
			    func->id->parameter ||
			    func->id->protected_ ||
			    func->id->token == NULL ||
			    func->data.user == NULL)
				continue;

			/* This is a hack: it fakes the pretty
			 * output routine to add a newline when printing
			 * a matrix */
			gel_output_string (md->out, "\n ");
			gel_output_clear_string (md->out);

			if (func->data.user->type == MATRIX_NODE)
				any_matrix = TRUE;
			gel_pretty_print_etree (md->out, func->data.user);

			if (vars > 0)
				gtk_text_buffer_insert_with_tags_by_name
					(md->buf, &iter, "\n", -1,
					 "value", NULL);

			if (func->context == 0)
				/* printed before a global variable */
				s = g_strdup (_("(global) "));
			else
				/* printed before local variable in certain
				 * context */
				s = g_strdup_printf (_("(context %d) "),
						     func->context);
			gtk_text_buffer_insert_with_tags_by_name
				(md->buf, &iter, s, -1, "context", NULL);
			g_free (s);

			gtk_text_buffer_insert_with_tags_by_name
				(md->buf, &iter, md->var->token, -1,
				 "variable", NULL);
			gtk_text_buffer_insert_with_tags_by_name
				(md->buf, &iter, " = ", -1,
				 "value", NULL);

			gtk_text_buffer_insert_with_tags_by_name
				(md->buf, &iter,
				 gel_output_peek_string (md->out), -1,
				 "value", NULL);

			gel_output_clear_string (md->out);
			vars ++;
		}
		if (vars == 0) {
			s = g_strdup_printf (_("%s not a user variable"),
					     md->var->token);
			gtk_text_buffer_insert_with_tags_by_name
				(md->buf, &iter, s, -1, "value", NULL);
			g_free (s);
		}
	}

	/* matrices look better unwrapped */
	if (any_matrix)
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (md->tv), GTK_WRAP_NONE);
	else
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (md->tv), GTK_WRAP_CHAR);

	/* delete everything left over */
	gtk_text_buffer_get_iter_at_offset (md->buf, &iter_end, -1);
	gtk_text_buffer_delete (md->buf, &iter, &iter_end);

	return TRUE;
}

static void
run_monitor (const char *var)
{
	GtkWidget *d;
	MonitorData *md;
	GtkWidget *sw;
	char *s;

	md = g_new0 (MonitorData, 1);
	md->var = d_intern (var);
	md->out = gel_output_new ();
	gel_output_setup_string (md->out, 0, NULL);

	s = g_strdup_printf (_("Monitoring: %s"), var);
	d = gtk_dialog_new_with_buttons
		(s,
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		 NULL);
	g_free (s);

	gtk_dialog_set_has_separator (GTK_DIALOG (d), FALSE);

	g_signal_connect (G_OBJECT (d), "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox),
			    sw,
			    TRUE, TRUE, 0);


	md->tv = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (md->tv), FALSE);
	md->buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (md->tv));

	gtk_text_buffer_create_tag (md->buf, "variable",
				    "editable", FALSE,
				    "family", "monospace",
				    "weight", PANGO_WEIGHT_BOLD,
				    NULL);

	gtk_text_buffer_create_tag (md->buf, "context",
				    "editable", FALSE,
				    "family", "monospace",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);

	gtk_text_buffer_create_tag (md->buf, "value",
				    "editable", FALSE,
				    "family", "monospace",
				    NULL);

	gtk_container_add (GTK_CONTAINER (sw), md->tv);

	md->updatecb = gtk_check_button_new_with_label
		(_("Update continuously"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox),
			    md->updatecb,
			    FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (md->updatecb), TRUE);

	gtk_window_set_default_size
		(GTK_WINDOW (d), 270, 150);

	md->tm = g_timeout_add (1500,
				monitor_timeout,
				md);

	g_signal_connect (G_OBJECT (d), "destroy",
			  G_CALLBACK (monitor_destroyed),
			  md);

	gtk_widget_show_all (d);

	monitor_timeout (md);
}

static void
monitor_user_var (GtkWidget *menu_item, gpointer data)
{
	GtkWidget *d;
	GtkWidget *e;
	int ret;

	d = gtk_dialog_new_with_buttons
		(_("Monitor a Variable"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OK, GTK_RESPONSE_OK,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);

	gtk_dialog_set_has_separator (GTK_DIALOG (d), FALSE);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox),
			    gtk_label_new (_("Variable name:")),
			    FALSE, FALSE, 0);

	e = gtk_entry_new ();
	g_signal_connect (G_OBJECT (e), "activate",
			  G_CALLBACK (dialog_entry_activate), d);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox),
			    e,
			    FALSE, FALSE, 0);

	gtk_widget_show_all (d);
	ret = gtk_dialog_run (GTK_DIALOG (d));

	if (ret == GTK_RESPONSE_OK) {
		char *txt = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (e))));
		run_monitor (txt);
		g_free (txt);
	}

	gtk_widget_destroy (d);
}

static void
full_answer (GtkWidget *menu_item, gpointer data)
{
	GelOutput *out;
	const char *s;
	GelEFunc *ans;
	gboolean wrap;

	out = gel_output_new ();
	gel_output_setup_string (out, 0, NULL);

	ans = d_lookup_only_global (d_intern ("Ans"));

	wrap = TRUE;

	if (ans != NULL) {
		if (ans->type == GEL_VARIABLE_FUNC) {
			if (ans->data.user->type == MATRIX_NODE)
				wrap = FALSE;
			gel_pretty_print_etree (out, ans->data.user);
		} else {
			/* ugly? maybe! */
			GelETree n;
			n.type = FUNCTION_NODE;
			n.any.next = NULL;
			n.func.func = ans;
			gel_pretty_print_etree (out, &n);
		}
	}

	s = gel_output_peek_string (out);

	geniusbox (FALSE /*error*/,
		   TRUE /*always textbox*/,
		   _("Full Answer") /*textbox_title*/,
		   TRUE /*bind_response*/,
		   wrap /* wrap */,
		   ve_sure_string (s));

	gel_output_unref (out);
}


static void
printout_error_num_and_reset(void)
{
	if(genius_setup.error_box) {
		if(errors) {
			if(errors_printed-curstate.max_errors > 0) {
				g_string_append_printf (errors,
							_("\nToo many errors! (%d followed)"),
							errors_printed - curstate.max_errors);
			}
			geniusbox (TRUE /*error*/,
				   FALSE /*always textbox*/,
				   NULL /*textbox_title*/,
				   TRUE /*bind_response*/,
				   FALSE /* wrap */,
				   errors->str);
			g_string_free(errors,TRUE);
			errors=NULL;
		}
	} else {
		if(errors_printed-curstate.max_errors > 0) {
			gel_output_printf(main_out,
					  _("\e[01;31mToo many errors! (%d followed)\e[0m\n"),
					  errors_printed-curstate.max_errors);
			gel_output_flush (main_out);
		}
	}
	errors_printed = 0;
}

static char *
resolve_file (const char *file)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	int i;

	if (n <= 1)
		return g_strdup (file);

	for (i = 1; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		g_assert (p != NULL);
		if (p->name != NULL &&
		    p->vname != NULL &&
		    strcmp (p->name, file) == 0)
			return g_strdup (p->vname);
	}
	return g_strdup (file);
}

/*get error message*/
static void
geniuserror(const char *s)
{
	char *file;
	int line;
	char *str;
	if(curstate.max_errors > 0 &&
	   errors_printed++>=curstate.max_errors)
		return;

	gel_get_file_info(&file,&line);
	/* put insertion point at the line of the error */
	if (line > 0 && running_program != NULL) {
		GtkTextIter iter;
		gtk_text_buffer_get_iter_at_line
			(GTK_TEXT_BUFFER (running_program->buffer),
			 &iter,
			 line-1);
		gtk_text_buffer_place_cursor
			(GTK_TEXT_BUFFER (running_program->buffer),
			 &iter);
		gtk_text_view_scroll_mark_onscreen
			(GTK_TEXT_VIEW (running_program->tv),
			 gtk_text_buffer_get_mark (running_program->buffer,
						   "insert"));
	}
	if (file != NULL) {
		char *fn = resolve_file (file);
		str = g_strdup_printf("%s:%d: %s",fn,line,s);
		g_free (fn);
	} else if (line > 0)
		str = g_strdup_printf("line %d: %s",line,s);
	else
		str = g_strdup(s);
	
	if(genius_setup.error_box) {
		if(errors) {
			g_string_append_c(errors,'\n');
			g_string_append(errors,str);
		} else {
			errors = g_string_new(str);
		}
	} else {
		gel_output_printf_full (main_out, FALSE,
					"\e[01;31m%s\e[0m\r\n", str);
		gel_output_flush (main_out);
	}

	g_free(str);
}

void
gel_printout_infos (void)
{
	/* Print out the infos */
	if (infos != NULL) {
		geniusbox (FALSE /*error*/,
			   FALSE /*always textbox*/,
			   NULL /*textbox_title*/,
			   TRUE /*bind_response*/,
			   FALSE /* wrap */,
			   infos->str);
		g_string_free (infos, TRUE);
		infos = NULL;
	}

	printout_error_num_and_reset ();
}

void
gel_call_help (const char *function)
{
	if (function == NULL) {
		/* FIXME: errors */

		gnome_help_display ("genius", NULL, NULL /* error */);
	} else {
		char *id = NULL;
		int i;
		for (i = 0; genius_toplevels[i] != NULL && id == NULL; i++) {
			if (strcmp (function, genius_toplevels[i]) == 0) {
				id = g_strdup_printf ("gel-command-%s",
						      function);
				break;
			}
		}
		if (id == NULL) {
			id = g_strdup_printf ("gel-function-%s",
					      function);
		}

		/* FIXME: errors */

		gnome_help_display ("genius", id, NULL /* error */);
	}
}


/*get info message*/
static void
geniusinfo(const char *s)
{
	char *file;
	int line;
	char *str;
	gel_get_file_info(&file,&line);
	if(file)
		str = g_strdup_printf("%s:%d: %s",file,line,s);
	else if(line>0)
		str = g_strdup_printf("line %d: %s",line,s);
	else
		str = g_strdup(s);
	
	if(genius_setup.info_box) {
		if(infos) {
			g_string_append_c(infos,'\n');
			g_string_append(infos,str);
		} else {
			infos = g_string_new(str);
		}
	} else {
		gel_output_printf_full (main_out, FALSE,
					"\e[32m%s\e[0m\r\n", str);
		gel_output_flush (main_out);
	}

	g_free(str);
}

/*about box*/
static void
aboutcb(GtkWidget * widget, gpointer data)
{
#if ! GTK_CHECK_VERSION(2,6,0)
	static GtkWidget *about;
#endif
	static char *authors[] = {
		"Jiří (George) Lebl, Ph.D. <jirka@5z.com>",
		N_("Nils Barth (initial implementation of parts of the GEL library)"),
		N_("Adrian E. Feiguin <feiguin@ifir.edu.ar> (GtkExtra - plotting widgetry)"),
		NULL
	};
	static const char *documenters[] = {
		"Jiří (George) Lebl, Ph.D. <jirka@5z.com>",
		"Kai Willadsen",
		NULL
	};
	const char *translators;
#if GTK_CHECK_VERSION(2,6,0)
	char *license;

	/* Force translation */
	authors[1] = _(authors[1]);
	authors[2] = _(authors[2]);

	{
#else
	if (about == NULL) {
#endif
		/* Translators should localize the following string
		 * which will give them credit in the About box.
		 * E.g. "Fulano de Tal <fulano@detal.com>"
		 */
		char *new_credits = N_("translator-credits");
		GdkPixbuf *logo;
		char *file;

		/* hack for old translations */
		char *old_hack = "translator_credits-PLEASE_ADD_YOURSELF_HERE";

		translators = _(new_credits);
		if (strcmp (translators, new_credits) == 0) {
			translators = NULL;
		}

		/* hack for old translations */
		if (translators == NULL) {
			translators = _(old_hack);
			if (strcmp (translators, old_hack) == 0) {
				translators = NULL;
			}
		}

		file = g_build_filename (genius_datadir,
					 "genius",
					 "genius-graph.png",
					 NULL);
		logo = gdk_pixbuf_new_from_file (file, NULL);
		g_free (file);

#if GTK_CHECK_VERSION(2,6,0)
		license = g_strdup_printf (_("Genius %s\n"
		       "%s\n\n"
		       "    This program is free software: you can redistribute it and/or modify\n"
		       "    it under the terms of the GNU General Public License as published by\n"
		       "    the Free Software Foundation, either version 3 of the License, or\n"
		       "    (at your option) any later version.\n"
		       "\n"
		       "    This program is distributed in the hope that it will be useful,\n"
		       "    but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		       "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		       "    GNU General Public License for more details.\n"
		       "\n"
		       "    You should have received a copy of the GNU General Public License\n"
		       "    along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"),
			    VERSION,
			    COPYRIGHT_STRING);
		gtk_show_about_dialog (GTK_WINDOW (genius_window),
				      "program-name", _("Genius Mathematical Tool"), 
				      "version", VERSION,
				      "copyright", COPYRIGHT_STRING,
				      "comments",
				      _("The Gnome calculator style edition of "
					"the Genius Mathematical Tool."),
				      "authors", authors,
				      "documenters", documenters,
				      "translator-credits", translators,
				      "logo", logo,
				      "license", license,
				      "website", "http://www.jirka.org/genius.html",
				      NULL);
		g_free (license);
#else
		about = gnome_about_new
			(_("About Genius"),
			 VERSION,
			 COPYRIGHT_STRING,
			 _("The Gnome calculator style edition of "
			   "the genius calculator.  For license/warranty "
			   "details, type 'warranty' into the console."),
			 authors,
			 documenters,
			 translators,
			 logo);
#endif

		if (logo != NULL)
			g_object_unref (logo);

#if ! GTK_CHECK_VERSION(2,6,0)
		gtk_window_set_transient_for (GTK_WINDOW (about),
					      GTK_WINDOW (genius_window));

		g_signal_connect (about, "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &about);
#endif
	}

#if ! GTK_CHECK_VERSION(2,6,0)
	gtk_widget_show_now (about);
	gtk_window_present (GTK_WINDOW (about));
#endif
}

static void
set_properties (void)
{
	gnome_config_set_bool("/genius/properties/black_on_white",
			      genius_setup.black_on_white);
	gnome_config_set_string ("/genius/properties/pango_font",
				 ve_sure_string (genius_setup.font));
	gnome_config_set_int("/genius/properties/scrollback",
			     genius_setup.scrollback);
	gnome_config_set_bool("/genius/properties/error_box",
			      genius_setup.error_box);
	gnome_config_set_bool("/genius/properties/info_box",
			      genius_setup.info_box);
	gnome_config_set_bool("/genius/properties/blinking_cursor",
			      genius_setup.blinking_cursor);
	gnome_config_set_bool("/genius/properties/output_remember",
			      genius_setup.output_remember);
	if (genius_setup.output_remember) {
		gnome_config_set_int("/genius/properties/max_digits", 
				      curstate.max_digits);
		gnome_config_set_bool("/genius/properties/results_as_floats",
				      curstate.results_as_floats);
		gnome_config_set_bool("/genius/properties/scientific_notation",
				      curstate.scientific_notation);
		gnome_config_set_bool("/genius/properties/full_expressions",
				      curstate.full_expressions);
		gnome_config_set_bool("/genius/properties/mixed_fractions",
				      curstate.mixed_fractions);
		gnome_config_set_int("/genius/properties/chop",
				     curstate.chop);
		gnome_config_set_int("/genius/properties/chop_when",
				     curstate.chop_when);
	}
	gnome_config_set_int("/genius/properties/max_errors",
			     curstate.max_errors);
	gnome_config_set_int("/genius/properties/max_nodes",
			     curstate.max_nodes);
	gnome_config_set_bool("/genius/properties/precision_remember",
			      genius_setup.precision_remember);
	if (genius_setup.precision_remember) {
		gnome_config_set_int("/genius/properties/float_prec",
				     curstate.float_prec);
	}
	
	gnome_config_sync();
}

void
genius_display_error (GtkWidget *parent, const char *err)
{
	static GtkWidget *w = NULL;

	if (w != NULL)
		gtk_widget_destroy (w);

	if (parent == NULL)
		parent = genius_window;

	w = gtk_message_dialog_new (GTK_WINDOW (parent) /* parent */,
				    GTK_DIALOG_MODAL /* flags */,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    "%s",
				    err);
	gtk_label_set_use_markup
		(GTK_LABEL (GTK_MESSAGE_DIALOG (w)->label), TRUE);

	g_signal_connect (G_OBJECT (w), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &w);

	gtk_dialog_run (GTK_DIALOG (w));
	gtk_widget_destroy (w);
}

static void
display_warning (GtkWidget *parent, const char *warn)
{
	static GtkWidget *w = NULL;

	if (w != NULL)
		gtk_widget_destroy (w);

	if (parent == NULL)
		parent = genius_window;

	w = gtk_message_dialog_new (GTK_WINDOW (parent) /* parent */,
				    GTK_DIALOG_MODAL /* flags */,
				    GTK_MESSAGE_WARNING,
				    GTK_BUTTONS_CLOSE,
				    "%s",
				    warn);
	gtk_label_set_use_markup
		(GTK_LABEL (GTK_MESSAGE_DIALOG (w)->label), TRUE);

	g_signal_connect (G_OBJECT (w), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &w);

	gtk_dialog_run (GTK_DIALOG (w));
	gtk_widget_destroy (w);
}

gboolean
genius_ask_question (GtkWidget *parent, const char *question)
{
	int ret;
	static GtkWidget *req = NULL;

	if (req != NULL)
		gtk_widget_destroy (req);

	if (parent == NULL)
		parent = genius_window;

	req = gtk_message_dialog_new (GTK_WINDOW (parent) /* parent */,
				      GTK_DIALOG_MODAL /* flags */,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_YES_NO,
				      "%s",
				      question);
	gtk_label_set_use_markup
		(GTK_LABEL (GTK_MESSAGE_DIALOG (req)->label), TRUE);

	g_signal_connect (G_OBJECT (req), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &req);


	ret = gtk_dialog_run (GTK_DIALOG (req));
	gtk_widget_destroy (req);

	if (ret == GTK_RESPONSE_YES)
		return TRUE;
	else /* this includes window close */
		return FALSE;
}

static gboolean
any_changed (void)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	int i;

	if (n <= 1)
		return FALSE;

	for (i = 1; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		g_assert (p != NULL);
		if (p->changed)
			return TRUE;
	}
	return FALSE;
}

/* quit */
static void
quitapp (GtkWidget * widget, gpointer data)
{
	if (any_changed ()) {
		if (calc_running) {
			if ( ! genius_ask_question (NULL,
						    _("Genius is executing something, "
						      "and furthermore there are "
						      "unsaved programs.\nAre "
						      "you sure you wish to quit?")))
				return;
			interrupted = TRUE;
		} else {
			if ( ! genius_ask_question (NULL,
						    _("There are unsaved programs, "
						      "are you sure you wish to quit?")))
				return;
		}
	} else {
		if (calc_running) {
			if ( ! genius_ask_question (NULL,
						    _("Genius is executing something, "
						      "are you sure you wish to "
						      "quit?")))
				return;
			interrupted = TRUE;
		} else {
			if ( ! genius_ask_question (NULL,
						    _("Are you sure you wish "
						      "to quit?")))
				return;
		}
	}

	gtk_main_quit ();
}

/*exact answer callback*/
static void
intspincb(GtkAdjustment *adj, int *data)
{
	*data=adj->value;
}

/*option callback*/
static void
optioncb(GtkWidget * widget, int *data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*data=TRUE;
	else
		*data=FALSE;
}

static void
fontsetcb (GtkWidget *fb, char **font)
{
	g_free(*font);
	*font = g_strdup (gtk_font_button_get_font_name (GTK_FONT_BUTTON (fb)));
}


static calcstate_t tmpstate={0};
static GeniusSetup tmpsetup={0};

static calcstate_t cancelstate={0};
static GeniusSetup cancelsetup={0};

static void
setup_response (GtkWidget *widget, gint resp, gpointer data)
{
	if (resp == GTK_RESPONSE_HELP) {
		gnome_help_display ("genius", "genius-prefs", NULL /* error */);
		return;
	}

	if (resp == GTK_RESPONSE_CANCEL ||
	    resp == GTK_RESPONSE_OK ||
	    resp == GTK_RESPONSE_APPLY) {
		if (resp == GTK_RESPONSE_CANCEL) {
			g_free (genius_setup.font);
			genius_setup = cancelsetup;
			if (cancelsetup.font)
				genius_setup.font = g_strdup (cancelsetup.font);
			curstate = cancelstate;
		} else {
			g_free (genius_setup.font);
			genius_setup = tmpsetup;
			if (tmpsetup.font)
				genius_setup.font = g_strdup (tmpsetup.font);
			curstate = tmpstate;
		}

		set_new_calcstate (curstate);
		vte_terminal_set_scrollback_lines (VTE_TERMINAL (term),
						   genius_setup.scrollback);
		vte_terminal_set_font_from_string
			(VTE_TERMINAL (term),
			 ve_string_empty (genius_setup.font) ?
			   default_console_font :
			   genius_setup.font);
		setup_term_color ();
		vte_terminal_set_cursor_blinks
			(VTE_TERMINAL (term),
			genius_setup.blinking_cursor);


		if (resp == GTK_RESPONSE_OK ||
		    resp == GTK_RESPONSE_CANCEL)
			gtk_widget_destroy (widget);
	}
}

static void
setup_calc(GtkWidget *widget, gpointer data)
{
	GtkWidget *mainbox,*frame;
	GtkWidget *box;
	GtkWidget *b, *w;
	GtkWidget *notebook;
	GtkAdjustment *adj;

	if (setupdialog) {
		gtk_window_present (GTK_WINDOW (setupdialog));
		return;
	}

	cancelstate = curstate;
	g_free (tmpsetup.font);
	cancelsetup = genius_setup;
	if (genius_setup.font)
		cancelsetup.font = g_strdup (genius_setup.font);
	
	tmpstate = curstate;
	g_free (tmpsetup.font);
	tmpsetup = genius_setup;
	if (genius_setup.font)
		tmpsetup.font = g_strdup (genius_setup.font);
	
	setupdialog = gtk_dialog_new_with_buttons
		(_("Genius Setup"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_HELP, GTK_RESPONSE_HELP,
		 GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OK, GTK_RESPONSE_OK,
		 NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (setupdialog), FALSE);

	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (setupdialog)->vbox),
			    notebook, TRUE, TRUE, 0);
	
	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  mainbox,
				  gtk_label_new(_("Output")));

	
	frame=gtk_frame_new(_("Number/Expression output options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);


	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Maximum digits to output (0=unlimited)")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpstate.max_digits,
						  0,
						  256,
						  1,
						  5,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 80, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.max_digits);


	w=gtk_check_button_new_with_label(_("Results as floats"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpstate.results_as_floats);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.results_as_floats);
	
	w=gtk_check_button_new_with_label(_("Floats in scientific notation"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpstate.scientific_notation);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.scientific_notation);

	w=gtk_check_button_new_with_label(_("Always print full expressions"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpstate.full_expressions);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.full_expressions);

	w=gtk_check_button_new_with_label(_("Use mixed fractions"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpstate.mixed_fractions);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.mixed_fractions);

	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Display 0.0 when floating point number is less than 10^-x "
				   "(0=never chop)")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpstate.chop,
						  0,
						  MAX_CHOP,
						  1,
						  5,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 80, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.chop);

	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Only chop numbers when another number is greater than 10^-x")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpstate.chop_when,
						  0,
						  MAX_CHOP,
						  1,
						  5,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 80, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.chop_when);

	w=gtk_check_button_new_with_label(_("Remember output settings across sessions"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpsetup.output_remember);
	g_signal_connect (G_OBJECT(w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.output_remember);

	gtk_tooltips_set_tip (tips, w,
			      _("Should the output settings in the "
			       "\"Number/Expression output options\" frame "
			       "be remembered for next session.  Does not apply "
			       "to the \"Error/Info output options\" frame."),
			      NULL);

	frame=gtk_frame_new(_("Error/Info output options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);

	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD);
	

	w=gtk_check_button_new_with_label(_("Display errors in a dialog"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpsetup.error_box);
	g_signal_connect (G_OBJECT(w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.error_box);

	w=gtk_check_button_new_with_label(_("Display information messages in a dialog"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpsetup.info_box);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.info_box);
	
	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Maximum errors to display (0=unlimited)")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpstate.max_errors,
						  0,
						  256,
						  1,
						  5,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 80, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb),&tmpstate.max_errors);


	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  mainbox,
				  gtk_label_new(_("Precision")));

	
	frame=gtk_frame_new(_("Floating point precision"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	gtk_box_pack_start(GTK_BOX(box), gtk_label_new(
		_("NOTE: The floating point precision might not take effect\n"
		  "for all numbers immediately, only new numbers calculated\n"
		  "and new variables will be affected.")),
			   FALSE,FALSE,0);


	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Floating point precision (bits)")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpstate.float_prec,
						  60,
						  16384,
						  1,
						  10,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 80, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.float_prec);

	w=gtk_check_button_new_with_label(_("Remember precision setting across sessions"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpsetup.precision_remember);
	g_signal_connect (G_OBJECT(w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.precision_remember);

	gtk_tooltips_set_tip (tips, w,
			      _("Should the precision setting "
			       "be remembered for next session."),
			      NULL);


	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  mainbox,
				  gtk_label_new(_("Terminal")));
	
	frame=gtk_frame_new(_("Terminal options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Scrollback lines")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpsetup.scrollback,
						  50,
						  10000,
						  1,
						  10,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 80, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpsetup.scrollback);
	
	
	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Font:")),
		   FALSE,FALSE,0);
	
        w = gtk_font_button_new_with_font (ve_string_empty (tmpsetup.font) ?
					   default_console_font :
					   genius_setup.font);
        gtk_box_pack_start (GTK_BOX (b), w, TRUE, TRUE, 0);
        g_signal_connect (G_OBJECT (w), "font_set",
			  G_CALLBACK (fontsetcb),
			  &tmpsetup.font);

	w=gtk_check_button_new_with_label(_("Black on white"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpsetup.black_on_white);
	g_signal_connect (G_OBJECT(w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.black_on_white);

	w=gtk_check_button_new_with_label(_("Blinking cursor"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      tmpsetup.blinking_cursor);
	g_signal_connect (G_OBJECT(w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.blinking_cursor);


	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  mainbox,
				  gtk_label_new(_("Memory")));

	
	frame=gtk_frame_new(_("Limits"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box),GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	gtk_box_pack_start(GTK_BOX(box), gtk_label_new(
		_("When the limit is reached you will be asked if\n"
		  "you wish to interrupt the calculation or continue.\n"
		  "Setting to 0 disables the limit.")),
			   FALSE,FALSE,0);


	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Maximum number of nodes to allocate")),
		   FALSE,FALSE,0);
	adj = (GtkAdjustment *)gtk_adjustment_new(tmpstate.max_nodes,
						  0,
						  1000000000,
						  1000,
						  1000000,
						  0);
	w = gtk_spin_button_new(adj,1.0,0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w),TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON(w),
					   GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w),
					  TRUE);
	gtk_widget_set_size_request (w, 150, -1);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.max_nodes);

	g_signal_connect (G_OBJECT (setupdialog), "response",
			  G_CALLBACK (setup_response), NULL);	
	g_signal_connect (G_OBJECT (setupdialog), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), 
			  &setupdialog);
	gtk_widget_show_all (setupdialog);
}

void
genius_interrupt_calc (void)
{
	interrupted = TRUE;
	if (!calc_running) {
		vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);
	}
}

static void
executing_warning (void)
{
	display_warning (NULL,
			 _("<b>Genius is currently executing something.</b>\n\n"
			   "Please try again later or interrupt the current "
			   "operation."));
}

static void
warranty_call (GtkWidget *widget, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* perhaps a bit ugly */
		gboolean last = genius_setup.info_box;
		genius_setup.info_box = TRUE;
		gel_evalexp ("warranty", NULL, main_out, NULL, TRUE, NULL);
		gel_printout_infos ();
		genius_setup.info_box = last;
	}
}

static void
add_filters (GtkFileChooser *fs)
{
	GtkFileFilter *filter_gel;
	GtkFileFilter *filter_all;

	filter_gel = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_gel, _("GEL files"));
	gtk_file_filter_add_pattern (filter_gel, "*.gel");
	gtk_file_filter_add_pattern (filter_gel, "*.GEL");

	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_pattern (filter_all, "*");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_gel);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fs), filter_gel);

}

static void
really_load_cb (GtkFileChooser *fs, int response, gpointer data)
{
	const char *s;
	char *str;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		return;
	}

	s = gtk_file_chooser_get_filename (fs);
	if (s == NULL ||
	    access (s, F_OK) != 0) {
		genius_display_error (GTK_WIDGET (fs),
				      _("Cannot open file!"));
		return;
	}

	g_free (last_dir);
	last_dir = gtk_file_chooser_get_current_folder (fs);

	gtk_widget_destroy (GTK_WIDGET (fs));

	str = g_strdup_printf ("\r\n\e[0m%s\e[0;32m", 
			       _("Output from "));
	vte_terminal_feed (VTE_TERMINAL (term), str, -1);
	g_free (str);

	vte_terminal_feed (VTE_TERMINAL (term), s, -1);
	vte_terminal_feed (VTE_TERMINAL (term),
			   "\e[0m (((\r\n", -1);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);

	calc_running ++;
	gel_load_guess_file (NULL, s, TRUE);
	gel_test_max_nodes_again ();
	calc_running --;

	gel_printout_infos ();

	str = g_strdup_printf ("\e[0m))) %s", _("End"));
	vte_terminal_feed (VTE_TERMINAL (term), str, -1);
	g_free (str);

	/* interrupt the current command line */
	interrupted = TRUE;
	vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);
}

static void
load_cb (GtkWidget *w)
{
	static GtkWidget *fs = NULL;
	
	if (fs != NULL) {
		gtk_window_present (GTK_WINDOW (fs));
		return;
	}

	fs = gtk_file_chooser_dialog_new (_("Load and Run"),
					  GTK_WINDOW (genius_window),
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  _("_Load"), GTK_RESPONSE_OK,
					  NULL);

	add_filters (GTK_FILE_CHOOSER (fs));

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	g_signal_connect (G_OBJECT (fs), "response",
			  G_CALLBACK (really_load_cb), NULL);

	if (last_dir != NULL)
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_dir);

	gtk_widget_show (fs);
}

#ifdef HAVE_GTKSOURCEVIEW

static guint ur_idle_id = 0;

static gboolean
setup_undo_redo_idle (gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

	ur_idle_id = 0;

	if (page < 0)
		return FALSE;
	if (page == 0) {
		gtk_widget_set_sensitive (edit_menu[EDIT_UNDO_ITEM].widget,
					  FALSE);
		gtk_widget_set_sensitive (edit_menu[EDIT_REDO_ITEM].widget,
					  FALSE);
	} else {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), page);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		gtk_widget_set_sensitive
			(edit_menu[EDIT_UNDO_ITEM].widget,
			 gtk_source_buffer_can_undo
			   (GTK_SOURCE_BUFFER (p->buffer)));
		gtk_widget_set_sensitive
			(edit_menu[EDIT_REDO_ITEM].widget,
			 gtk_source_buffer_can_redo
			   (GTK_SOURCE_BUFFER (p->buffer)));
	}

	return FALSE;
}

static void
setup_undo_redo (void)
{
	if (ur_idle_id == 0) {
		ur_idle_id = gtk_idle_add (setup_undo_redo_idle, NULL);
	}
}


static void
undo_callback (GtkWidget *menu_item, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (page < 0)
		return;
	if (page == 0) {
		/* undo from a terminal? what are you talking about */
		return;
	} else {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), page);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		if (gtk_source_buffer_can_undo
			(GTK_SOURCE_BUFFER (p->buffer)))
			gtk_source_buffer_undo
				(GTK_SOURCE_BUFFER (p->buffer));
	}
}

static void
redo_callback (GtkWidget *menu_item, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (page < 0)
		return;
	if (page == 0) {
		/* redo from a terminal? what are you talking about */
		return;
	} else {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), page);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		if (gtk_source_buffer_can_redo
			(GTK_SOURCE_BUFFER (p->buffer)))
			gtk_source_buffer_redo
				(GTK_SOURCE_BUFFER (p->buffer));
	}
}
#endif

static void
cut_callback (GtkWidget *menu_item, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (page < 0)
		return;
	if (page == 0) {
		/* cut from a terminal? what are you talking about */
		return;
	} else {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), page);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		gtk_text_buffer_cut_clipboard
			(p->buffer,
			 gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			 TRUE /* default_editable */);
	}
}


static void
copy_callback (GtkWidget *menu_item, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (page < 0)
		return;
	if (page == 0) {
		vte_terminal_copy_clipboard (VTE_TERMINAL (term));
	} else {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), page);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		gtk_text_buffer_copy_clipboard
			(p->buffer,
			 gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
	}
}

static void
paste_callback (GtkWidget *menu_item, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (page < 0)
		return;
	if (page == 0) {
		vte_terminal_paste_clipboard (VTE_TERMINAL (term));
	} else {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), page);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		gtk_text_buffer_paste_clipboard
			(p->buffer,
			 gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			 NULL /* override location */,
			 TRUE /* default editable */);
		gtk_text_view_scroll_mark_onscreen
			(GTK_TEXT_VIEW (p->tv),
			 gtk_text_buffer_get_mark (p->buffer, "insert"));
	}
}

static void
clear_cb (GtkClipboard *clipboard, gpointer owner)
{
	/* do nothing on losing the clipboard */
}

/* text was actually requested */
static void
copy_cb(GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner)
{
	gtk_selection_data_set_text (data, clipboard_str, -1);
}

static void
copy_answer (void)
{
	GtkClipboard *cb;
	GtkTargetEntry targets[] = {
		{"UTF8_STRING", 0, 0},
		{"COMPOUND_TEXT", 0, 0},
		{"TEXT", 0, 0},
		{"STRING", 0, 0},
	};
	/* perhaps a bit ugly */
	GelOutput *out = gel_output_new ();
	gboolean last_info = genius_setup.info_box;
	gboolean last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;
	gel_output_setup_string (out, 0, NULL);
	gel_evalexp ("ans", NULL, out, NULL, TRUE, NULL);
	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	g_free (clipboard_str);
	clipboard_str = gel_output_snarf_string (out);
	gel_output_unref (out);

	cb = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

	gtk_clipboard_set_with_owner (cb,
				      targets,
				      G_N_ELEMENTS(targets),
				      copy_cb,
				      clear_cb,
				      G_OBJECT (genius_window));

	cb = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_set_with_owner (cb,
				      targets,
				      G_N_ELEMENTS(targets),
				      copy_cb,
				      clear_cb,
				      G_OBJECT (genius_window));
}


static void
copy_as_plain (GtkWidget *menu_item, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_NORMAL;
		set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		set_new_calcstate (curstate);
	}
}

static void
copy_as_latex (GtkWidget *menu_item, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_LATEX;
		set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		set_new_calcstate (curstate);
	}
}

static void
copy_as_troff (GtkWidget *menu_item, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_TROFF;
		set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		set_new_calcstate (curstate);
	}
}

static void
copy_as_mathml (GtkWidget *menu_item, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_MATHML;
		set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		set_new_calcstate (curstate);
	}
}

static void
setup_label (Program *p)
{
	char *s;
	const char *vname;
	const char *pre = "", *post = "", *mark = "";

	g_assert (p != NULL);

	if (p->selected) {
		pre = "<b>";
		post = "</b>";
	}

	if (p->real_file &&
	    p->changed) {
		mark = " [+]";
	}

	vname = p->vname;
	if (vname == NULL)
		vname = "???";

	s = g_strdup_printf ("%s%s%s%s", pre, vname, mark, post);

	gtk_label_set_markup (GTK_LABEL (p->label), s);
}

static void
next_tab (GtkWidget *menu_item, gpointer data)
{
	gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
}

static void
prev_tab (GtkWidget *menu_item, gpointer data)
{
	gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
}

static void
prog_menu_activated (GtkWidget *item, gpointer data)
{
	GtkWidget *w = data;
	int num;
       
	if (w == NULL)
		num = 0;
	else
		num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), w);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), num);
}

static void
build_program_menu (void)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	int i;
	GtkWidget *menu;

	while (prog_menu_items != NULL) {
		gtk_widget_destroy (prog_menu_items->data);
		prog_menu_items = g_list_remove_link (prog_menu_items, prog_menu_items);
	}

	menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (genius_menu[PROGRAMS_MENU].widget));

	for (i = 1; i < n; i++) {
		GtkWidget *item;
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		g_assert (p != NULL);

		item = gtk_menu_item_new_with_label (p->vname);
		gtk_widget_show (item);
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (prog_menu_activated), w);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		prog_menu_items = g_list_prepend (prog_menu_items, item);
	}
}

static void
changed_cb (GtkTextBuffer *buffer, GtkWidget *tab_widget)
{
	Program *p;
	GtkTextIter iter, iter_end;

	p = g_object_get_data (G_OBJECT (tab_widget), "program");
	g_assert (p != NULL);

	if (p->ignore_changes)
		return;

	/* apply the foo tag to entered text */
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &iter_end, -1);
	gtk_text_buffer_apply_tag_by_name (buffer, "foo",
					   &iter, &iter_end);

	if ( ! p->changed) {
		p->changed = TRUE;
		setup_label (p);
	}
}

static gboolean
save_contents_vfs (const char *file, const char *str, int size)
{
	GnomeVFSHandle *handle;
	GnomeVFSFileSize bytes;
	GnomeVFSResult result;

	/* FIXME: we should handle errors better by perhaps moving
	   to a different name first and erasing only when saving
	   was all fine */

	/* Be safe about saving files, unlink and create in
	 * exclusive mode */
	result = gnome_vfs_unlink (file);
	/* FIXME: error handling, but not if it's
	 * the file-doesn't-exist kind of error which is fine */
	result = gnome_vfs_create (&handle, file,
				   GNOME_VFS_OPEN_WRITE,
				   TRUE /* exclusive */,
				   0644);
	if (result != GNOME_VFS_OK) {
		/* FIXME: error handling */
		return FALSE;
	}

	result = gnome_vfs_write (handle, str, size, &bytes);
	if (result != GNOME_VFS_OK || bytes != size) {
		gnome_vfs_close (handle);
		/* FIXME: error handling */
		return FALSE;
	}

	/* add traling \n if needed */
	if (size > 0 && str[size-1] != '\n')
		gnome_vfs_write (handle, "\n", 1, &bytes);
	/* FIXME: error handling? */

	gnome_vfs_close (handle);

	return TRUE;
}

static char *
get_contents_vfs (const char *file)
{
	GnomeVFSHandle *handle;
	GnomeVFSFileSize bytes;
	char buffer[4096];
	GnomeVFSResult result;
	GString *str;

	/* FIXME: add limit to avoid reading until never */

	result = gnome_vfs_open (&handle, file,
				 GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		/* FIXME: error handling */
		return NULL;
	}

	str = g_string_new (NULL);

	while (gnome_vfs_read (handle,
			       buffer,
			       sizeof (buffer)-1,
			       &bytes) == GNOME_VFS_OK) {
		buffer[bytes] = '\0';
		g_string_append (str, buffer);
	}

	gnome_vfs_close (handle);

	return g_string_free (str, FALSE);
}

static void
reload_cb (GtkWidget *menu_item)
{
	GtkTextIter iter, iter_end;
	char *contents;

	if (selected_program == NULL ||
	    ! selected_program->real_file)
		return;

	selected_program->ignore_changes++;

	gtk_text_buffer_get_iter_at_offset (selected_program->buffer, &iter, 0);
	gtk_text_buffer_get_iter_at_offset (selected_program->buffer,
					    &iter_end, -1);

	gtk_text_buffer_delete (selected_program->buffer,
				&iter, &iter_end);

	contents = get_contents_vfs (selected_program->name);
	if (contents != NULL) {
		gtk_text_buffer_get_iter_at_offset (selected_program->buffer,
						    &iter, 0);
		gtk_text_buffer_insert_with_tags_by_name
			(selected_program->buffer,
			 &iter, contents, -1, "foo", NULL);
		g_free (contents);
		selected_program->changed = FALSE;
	} else {
		genius_display_error (NULL, _("Cannot open file"));
	}

	selected_program->ignore_changes--;

	setup_label (selected_program);
}

static void
move_cursor (GtkTextBuffer *buffer,
	     const GtkTextIter *new_location,
	     GtkTextMark *mark,
	     gpointer data)
{
	Program *p = data;
	GtkTextIter iter;
	int line;
	char *s;

	gtk_text_buffer_get_iter_at_mark
		(p->buffer,
		 &iter,
		 gtk_text_buffer_get_insert (p->buffer));
	
	line = gtk_text_iter_get_line (&iter);

	if (line == p->curline)
		return;

	p->curline = line;

	gnome_appbar_pop (GNOME_APPBAR (appbar));
	s = g_strdup_printf (_("Line: %d"), line+1);
	gnome_appbar_push (GNOME_APPBAR (appbar), s);
	g_free (s);
}

#ifdef HAVE_GTKSOURCEVIEW
#ifdef HAVE_GTKSOURCEVIEW2
static GtkSourceLanguageManager*
get_source_language_manager ()
{
	static GtkSourceLanguageManager *lm = NULL;
	gchar **lang_dirs;
	const gchar * const *default_lang_dirs;
	gint nlang_dirs, i;
	
	if (lm == NULL) {
		lm = gtk_source_language_manager_new ();

		nlang_dirs = 0;
		default_lang_dirs = gtk_source_language_manager_get_search_path (gtk_source_language_manager_get_default ());
		for (nlang_dirs = 0; default_lang_dirs[nlang_dirs] != NULL; nlang_dirs++);

		lang_dirs = g_new0 (gchar *, nlang_dirs + 2);
		for (i = 0; i < nlang_dirs; i++) {
		  lang_dirs[i] = (gchar *) default_lang_dirs[i];
		}
		lang_dirs[nlang_dirs] =
			genius_datadir_sourceview;

		gtk_source_language_manager_set_search_path (lm, lang_dirs);

		/* FIXME: is lang_dirs leaked? */
	}

	return lm;
}
#endif
#endif


static void
new_program (const char *filename)
{
	static int cnt = 1;
	GtkWidget *tv;
	GtkWidget *sw;
	GtkTextBuffer *buffer;
	Program *p;
#ifdef HAVE_GTKSOURCEVIEW
#ifdef HAVE_GTKSOURCEVIEW2
	GtkSourceLanguage *lang;
	GtkSourceLanguageManager *lm;
#else
	GtkSourceLanguage *lang;
	GtkSourceLanguagesManager *lm;
	GList lang_dirs;
#endif
#endif

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
#ifdef HAVE_GTKSOURCEVIEW
#ifdef HAVE_GTKSOURCEVIEW2
	tv = gtk_source_view_new ();
	gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (tv), TRUE);
	gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (tv), TRUE);
	/*gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (tv), 8);
	gtk_source_view_set_insert_spaces_instead_of_tabs
		(GTK_SOURCE_VIEW (tv), TRUE);*/
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
	lm = get_source_language_manager ();

	lang = gtk_source_language_manager_get_language (lm, "genius");
	g_assert (lang != NULL);
	if (lang != NULL) {
		gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (buffer), TRUE);
		gtk_source_buffer_set_language
			(GTK_SOURCE_BUFFER (buffer), lang);
	}

	g_signal_connect (G_OBJECT (buffer), "notify::can-undo",
			  G_CALLBACK (setup_undo_redo), NULL);
	g_signal_connect (G_OBJECT (buffer), "notify::can-redo",
			  G_CALLBACK (setup_undo_redo), NULL);
#else
	tv = gtk_source_view_new ();
	gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (tv), TRUE);
	gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (tv), TRUE);
	/*gtk_source_view_set_tabs_width (GTK_SOURCE_VIEW (tv), 8);
	gtk_source_view_set_insert_spaces_instead_of_tabs
		(GTK_SOURCE_VIEW (tv), TRUE);*/
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
	lang_dirs.data = genius_datadir_sourceview;
	lang_dirs.prev = NULL;
	lang_dirs.next = NULL;
	lm = GTK_SOURCE_LANGUAGES_MANAGER
		(g_object_new (GTK_TYPE_SOURCE_LANGUAGES_MANAGER,
			       "lang_files_dirs", &lang_dirs,
			       NULL));

	lang = gtk_source_languages_manager_get_language_from_mime_type
		(lm, "text/x-genius");
	if (lang != NULL) {
		g_object_set (G_OBJECT (buffer), "highlight", TRUE, NULL);
		gtk_source_buffer_set_language
			(GTK_SOURCE_BUFFER (buffer), lang);
	}

	g_signal_connect (G_OBJECT (buffer), "can-undo",
			  G_CALLBACK (setup_undo_redo), NULL);
	g_signal_connect (G_OBJECT (buffer), "can-redo",
			  G_CALLBACK (setup_undo_redo), NULL);
#endif
#else
	tv = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
#endif

	gtk_text_buffer_create_tag (buffer, "foo",
				    "family", "monospace",
				    NULL);

	gtk_container_add (GTK_CONTAINER (sw), tv);

	gtk_widget_show_all (sw);

	p = g_new0 (Program, 1);
	p->real_file = FALSE;
	p->changed = FALSE;
	p->selected = FALSE;
	p->buffer = buffer;
	p->tv = tv;
	p->curline = 0;
	p->ignore_changes = 0;
	g_object_set_data (G_OBJECT (sw), "program", p);

	g_signal_connect_after (G_OBJECT (p->buffer), "mark_set",
				G_CALLBACK (move_cursor),
				p);

	if (filename == NULL) {
		char *d = g_get_current_dir ();
		char *n = g_strdup_printf (_("Program_%d.gel"), cnt);
		/* the file name will have an underscore */
		char *fn = g_build_filename (d, n, NULL);
		g_free (d);
		g_free (n);
		p->name = gnome_vfs_get_uri_from_local_path (fn);
		g_free (fn);
		p->vname = g_strdup_printf (_("Program %d"), cnt);
		cnt++;
	} else {
		char *contents;
		p->name = g_strdup (filename);
		contents = get_contents_vfs (p->name);
		if (contents != NULL) {
			GtkTextIter iter;
#ifdef HAVE_GTKSOURCEVIEW
			gtk_source_buffer_begin_not_undoable_action
				(GTK_SOURCE_BUFFER (buffer));
#endif
			gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, contents, -1, "foo", NULL);
#ifdef HAVE_GTKSOURCEVIEW
			gtk_source_buffer_end_not_undoable_action
				(GTK_SOURCE_BUFFER (buffer));
#endif
			g_free (contents);
		} else {
			char *s = g_strdup_printf (_("Cannot open %s"), filename);
			genius_display_error (NULL, s);
			g_free (s);
		}
		p->vname = g_path_get_basename (p->name);
		p->real_file = TRUE;
	}
	/* the label will change after the set_current_page */
	p->label = gtk_label_new (p->vname);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sw, p->label);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), -1);

	g_signal_connect (G_OBJECT (buffer), "changed",
			  G_CALLBACK (changed_cb), sw);

	build_program_menu ();

#ifdef HAVE_GTKSOURCEVIEW
	setup_undo_redo ();
#endif
}

static void
new_callback (GtkWidget *menu_item, gpointer data)
{
	new_program (NULL);
}

static void
really_open_cb (GtkFileChooser *fs, int response, gpointer data)
{
	const char *s;
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		return;
	}

	s = gtk_file_chooser_get_uri (fs);
	if (s == NULL ||
	    ! uri_exists (s)) {
		genius_display_error (GTK_WIDGET (fs),
				      _("Cannot open file!"));
		return;
	}

	g_free (last_dir);
	last_dir = gtk_file_chooser_get_current_folder (fs);

	new_program (s);

	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
open_callback (GtkWidget *w)
{
	static GtkWidget *fs = NULL;
	
	if (fs != NULL) {
		gtk_window_present (GTK_WINDOW (fs));
		return;
	}

	fs = gtk_file_chooser_dialog_new (_("Open..."),
					  GTK_WINDOW (genius_window),
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					  NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	add_filters (GTK_FILE_CHOOSER (fs));

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	g_signal_connect (G_OBJECT (fs), "response",
			  G_CALLBACK (really_open_cb), NULL);

	if (last_dir != NULL)
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_dir);

	gtk_widget_show (fs);
}

static gboolean
save_program (Program *p, const char *new_fname)
{
	GtkTextIter iter, iter_end;
	char *prog;
	const char *fname;
	int sz;

	if (new_fname == NULL)
		fname = p->name;
	else
		fname = new_fname;

	gtk_text_buffer_get_iter_at_offset (p->buffer, &iter, 0);
	gtk_text_buffer_get_iter_at_offset (p->buffer, &iter_end, -1);
	prog = gtk_text_buffer_get_text (p->buffer, &iter, &iter_end,
					 FALSE /* include_hidden_chars */);
	sz = strlen (prog);

	if ( ! save_contents_vfs (fname, prog, sz)) {
		g_free (prog);
		return FALSE;
	}

	if (p->name != fname) {
		g_free (p->name);
		p->name = g_strdup (fname);
	}
	g_free (p->vname);
	p->vname = g_path_get_basename (fname);
	p->real_file = TRUE;
	p->changed = FALSE;

	if (selected_program == p) {
		gtk_widget_set_sensitive (file_menu[FILE_RELOAD_ITEM].widget,
					  TRUE);
		gtk_widget_set_sensitive (file_menu[FILE_SAVE_ITEM].widget,
					  TRUE);
	}

	setup_label (p);

	return TRUE;
}

static void
save_callback (GtkWidget *w)
{
	if (selected_program == NULL ||
	    ! selected_program->real_file)
		return;

	if ( ! save_program (selected_program, NULL /* new fname */)) {
		char *err = g_strdup_printf (_("<b>Cannot save file</b>\n"
					       "Details: %s"),
					     g_strerror (errno));
		genius_display_error (NULL, err);
		g_free (err);
	}
}

static void
save_all_cb (GtkWidget *w)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	int i;
	gboolean there_are_unsaved = FALSE;

	if (n <= 1)
		return;

	for (i = 1; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		g_assert (p != NULL);

		if (p->changed && ! p->real_file)
			there_are_unsaved = TRUE;

		if (p->changed && p->real_file) {
			if ( ! save_program (p, NULL /* new fname */)) {
				char *err = g_strdup_printf (_("<b>Cannot save file</b>\n"
							       "Details: %s"),
							     g_strerror (errno));
				genius_display_error (NULL, err);
				g_free (err);
			}
		}
	}

	if (there_are_unsaved) {
		genius_display_error (NULL, _("Save new programs by "
					      "\"Save As..\" first!"));
	}
}

static void
really_save_as_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		/* FIXME: don't want to deal with modality issues right now */
		gtk_widget_set_sensitive (genius_window, TRUE);
		return;
	}

	/* sanity */
	if (selected_program == NULL)
		return;

	s = g_strdup (gtk_file_chooser_get_uri (fs));
	if (s == NULL)
		return;
	base = g_path_get_basename (s);
	if (base != NULL && base[0] != '\0' &&
	    strchr (base, '.') == NULL) {
		char *n = g_strconcat (s, ".gel", NULL);
		g_free (s);
		s = n;
	}
	g_free (base);
	
	if (uri_exists (s) &&
	    ! genius_ask_question (GTK_WIDGET (fs),
			    _("File already exists.  Overwrite it?"))) {
		g_free (s);
		return;
	}

	if ( ! save_program (selected_program, s /* new fname */)) {
		char *err = g_strdup_printf (_("<b>Cannot save file</b>\n"
					       "Details: %s"),
					     g_strerror (errno));
		genius_display_error (GTK_WIDGET (fs), err);
		g_free (err);
		g_free (s);
		return;
	}

	g_free (last_dir);
	last_dir = gtk_file_chooser_get_current_folder (fs);

	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, TRUE);

	g_free (s);
}

static void
save_as_callback (GtkWidget *w)
{
	static GtkWidget *fs = NULL;

	/* sanity */
	if (selected_program == NULL)
		return;
	
	if (fs != NULL) {
		gtk_window_present (GTK_WINDOW (fs));
		return;
	}

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, FALSE);

	fs = gtk_file_chooser_dialog_new (_("Save As..."),
					  GTK_WINDOW (genius_window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					  NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	add_filters (GTK_FILE_CHOOSER (fs));

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	g_signal_connect (G_OBJECT (fs), "response",
			  G_CALLBACK (really_save_as_cb), NULL);

	if (last_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_dir);
	}
	if (selected_program->real_file) {
		gtk_file_chooser_set_uri
			(GTK_FILE_CHOOSER (fs), selected_program->name);
	} else {
		char *bn = g_path_get_basename (selected_program->name);
		gtk_file_chooser_set_current_name
			(GTK_FILE_CHOOSER (fs), bn);
		g_free (bn);
	}

	gtk_widget_show (fs);
}

/* vte_terminal_get_text_range is totally and utterly a broken API.  There is
 * no good way to get at the buffer currently.  We'd have to just keep the
 * buffer ourselves over again.  That is stupid.  At some point we must get rid of
 * VTE, then we can implement this sucker. */

static gboolean
always_selected (VteTerminal *terminal, glong column, glong row, gpointer data)
{
	return TRUE;
}

static void
really_save_console_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	char *output;
	glong row, column;
	int sz;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		/* FIXME: don't want to deal with modality issues right now */
		gtk_widget_set_sensitive (genius_window, TRUE);
		return;
	}

	s = g_strdup (gtk_file_chooser_get_uri (fs));
	if (s == NULL)
		return;
	base = g_path_get_basename (s);
	if (base != NULL && base[0] != '\0' &&
	    strchr (base, '.') == NULL) {
		char *n = g_strconcat (s, ".gel", NULL);
		g_free (s);
		s = n;
	}
	g_free (base);
	
	if (uri_exists (s) &&
	    ! genius_ask_question (GTK_WIDGET (fs),
			    _("File already exists.  Overwrite it?"))) {
		g_free (s);
		return;
	}

	/* this is moronic! VTE has an idiotic API, there is no
	 * way to find out what the proper row range is! */
	vte_terminal_get_cursor_position (VTE_TERMINAL (term),
					  &column, &row);
	output = vte_terminal_get_text_range (VTE_TERMINAL (term),
					      MAX(row-genius_setup.scrollback+1, 0),
					      0,
					      row,
					      VTE_TERMINAL (term)->column_count - 1,
					      always_selected,
					      NULL,
					      NULL);
	sz = strlen (output);

	if ( ! save_contents_vfs (s, output, sz)) {
		char *err = g_strdup_printf (_("<b>Cannot save file</b>\n"
					       "Details: %s"),
					     g_strerror (errno));
		genius_display_error (GTK_WIDGET (fs), err);
		g_free (err);
		g_free (output);
		g_free (s);
		return;
	}
	g_free (output);

	g_free (last_dir);
	last_dir = gtk_file_chooser_get_current_folder (fs);

	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, TRUE);

	g_free (s);
}

static void
save_console_cb (GtkWidget *w)
{
	static GtkWidget *fs = NULL;

	if (fs != NULL) {
		gtk_window_present (GTK_WINDOW (fs));
		return;
	}

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, FALSE);

	fs = gtk_file_chooser_dialog_new (_("Save Console Output..."),
					  GTK_WINDOW (genius_window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					  NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), FALSE);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	g_signal_connect (G_OBJECT (fs), "response",
			  G_CALLBACK (really_save_console_cb), NULL);

	if (last_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_dir);
	}

	gtk_file_chooser_set_current_name
		(GTK_FILE_CHOOSER (fs), "Genius-Console-Output.txt");

	gtk_widget_show (fs);
}


static void
whack_program (Program *p)
{
	g_assert (p != NULL);

	if (selected_program == p) {
		p->selected = FALSE;
		selected_program = NULL;
		gtk_widget_set_sensitive (file_menu[FILE_RELOAD_ITEM].widget,
					  FALSE);
		gtk_widget_set_sensitive (file_menu[FILE_SAVE_ITEM].widget,
					  FALSE);
		gtk_widget_set_sensitive (file_menu[FILE_SAVE_AS_ITEM].widget,
					  FALSE);
		gtk_widget_set_sensitive (toolbar[TOOLBAR_RUN_ITEM].widget,
					  FALSE);
		gtk_widget_set_sensitive (calc_menu[CALC_RUN_ITEM].widget,
					  FALSE);
	}
	g_free (p->name);
	g_free (p->vname);
	g_free (p);
}


static void
close_callback (GtkWidget *menu_item, gpointer data)
{
	GtkWidget *w;
	Program *p;
	int current = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (current <= 0) /* if the console */
		return;
	w = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), current);
	p = g_object_get_data (G_OBJECT (w), "program");

	if (p->changed &&
	    ! genius_ask_question (NULL,
				   _("The program you are closing is unsaved, "
				     "are you sure you wish to close it "
				     "without saving?")))
		return;

	gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), current);
	whack_program (p);

	build_program_menu ();
}

static void
run_program (GtkWidget *menu_item, gpointer data)
{
	const char *vname;
	const char *name;
	GtkTextBuffer *buffer;
	if (selected_program == NULL) /* if nothing is selected */ {
		genius_display_error (NULL,
				      _("<b>No program selected.</b>\n\n"
					"Create a new program, or select an "
					"existing tab in the notebook."));
		return;
	}
	buffer = selected_program->buffer;
	/* sanity */
	if (buffer == NULL)
		return;
	name = selected_program->name;
	vname = selected_program->vname;
	if (vname == NULL)
		vname = "???";

	if (calc_running) {
		executing_warning ();
		return;
	} else {
		GtkTextIter iter, iter_end;
		char *prog;
		int p[2];
		FILE *fp;
		int pid;
		int status;
		char *str;

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
		gtk_text_buffer_get_iter_at_offset (buffer, &iter_end, -1);
		prog = gtk_text_buffer_get_text (buffer, &iter, &iter_end,
						 FALSE /* include_hidden_chars */);

		str = g_strdup_printf ("\r\n\e[0m%s\e[0;32m", 
				       _("Output from "));
		vte_terminal_feed (VTE_TERMINAL (term), str, -1);
		g_free (str);
		vte_terminal_feed (VTE_TERMINAL (term), vname, -1);
		vte_terminal_feed (VTE_TERMINAL (term),
				   "\e[0m (((\r\n", -1);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);

		pipe (p);

		/* run this in a fork so that we don't block on very
		   long input */
		pid = fork ();
		if (pid < 0) {
			close (p[1]);
			close (p[0]);
			g_free (prog);
			genius_display_error (NULL,
					      _("<b>Cannot execute program</b>\n\n"
						"Cannot fork."));
			return;
		}

		if (pid == 0) {
			close (p[0]);
			write (p[1], prog, strlen (prog));
			close (p[1]);
			_exit (0);
		}
		close (p[1]);
		fp = fdopen (p[0], "r");
		gel_lexer_open (fp);

		calc_running ++;

		g_free (prog);

		running_program = selected_program;

		gel_push_file_info (name, 1);
		/* FIXME: Should not use main_out, we should have a separate
		   console for output, the switching is annoying */
		while (1) {
			gel_evalexp (NULL, fp, main_out, "= \e[1;36m",
				     TRUE, NULL);
			gel_output_full_string (main_out, "\e[0m");
			if (gel_got_eof) {
				gel_got_eof = FALSE;
				break;
			}
			if (interrupted) {
				break;
			}
		}

		gel_pop_file_info ();

		gel_lexer_close (fp);
		fclose (fp);

		gel_test_max_nodes_again ();
		calc_running --;

		gel_printout_infos ();

		running_program = NULL;

		str = g_strdup_printf ("\e[0m))) %s", _("End"));
		vte_terminal_feed (VTE_TERMINAL (term), str, -1);
		g_free (str);

		/* interrupt the current command line */
		interrupted = TRUE;
		vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);

		/* sanity */
		if (pid > 0) {
			/* It must have finished by now, so reap the child */
			/* must kill it, just in case we were interrupted */
			kill (pid, SIGTERM);
			waitpid (pid, &status, 0);
		}
	}

}

static gboolean
delete_event (GtkWidget *w, GdkEventAny *e, gpointer data)
{
	quitapp (w, data);
	return TRUE;
}


/*main window creation, slightly copied from same-gnome:)*/
static GtkWidget *
create_main_window (void)
{
	GtkWidget *w;
	char *s = g_strdup_printf (_("Genius %s"), VERSION);
        w = gnome_app_new("gnome-genius", s);
	g_free (s);
	gtk_window_set_wmclass (GTK_WINDOW (w), "gnome-genius", "gnome-genius");

        g_signal_connect (G_OBJECT (w), "delete_event",
			  G_CALLBACK (delete_event), NULL);
        return w;
}

/* gnome_config employment */

static void
get_properties (void)
{
	gchar buf[256];

	g_snprintf(buf,256,"/genius/properties/black_on_white=%s",
		   (genius_setup.black_on_white)?"true":"false");
	genius_setup.black_on_white = gnome_config_get_bool(buf);

	g_snprintf (buf, 256, "/genius/properties/pango_font=%s",
		    ve_sure_string (genius_setup.font));
	genius_setup.font = gnome_config_get_string (buf);

	g_snprintf(buf,256,"/genius/properties/scrollback=%d",
		   genius_setup.scrollback);
	genius_setup.scrollback = gnome_config_get_int(buf);

	g_snprintf(buf,256,"/genius/properties/error_box=%s",
		   (genius_setup.error_box)?"true":"false");
	genius_setup.error_box = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/info_box=%s",
		   (genius_setup.info_box)?"true":"false");
	genius_setup.info_box = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/blinking_cursor=%s",
		   (genius_setup.blinking_cursor)?"true":"false");
	genius_setup.blinking_cursor = gnome_config_get_bool(buf);
	
	g_snprintf(buf,256,"/genius/properties/max_digits=%d",
		   curstate.max_digits);
	curstate.max_digits = gnome_config_get_int(buf);
	if (curstate.max_digits < 0)
		curstate.max_digits = 0;
	else if (curstate.max_digits > 256)
		curstate.max_digits = 256;

	g_snprintf(buf,256,"/genius/properties/results_as_floats=%s",
		   curstate.results_as_floats?"true":"false");
	curstate.results_as_floats = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/scientific_notation=%s",
		   curstate.scientific_notation?"true":"false");
	curstate.scientific_notation = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/full_expressions=%s",
		   curstate.full_expressions?"true":"false");
	curstate.full_expressions = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/mixed_fractions=%s",
		   curstate.mixed_fractions?"true":"false");
	curstate.mixed_fractions = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/output_remember=%s",
		   genius_setup.output_remember?"true":"false");
	genius_setup.output_remember = gnome_config_get_bool(buf);

	g_snprintf(buf,256,"/genius/properties/max_errors=%d",
		   curstate.max_errors);
	curstate.max_errors = gnome_config_get_int(buf);
	if (curstate.max_errors < 0)
		curstate.max_errors = 0;

	g_snprintf(buf,256,"/genius/properties/max_nodes=%d",
		   curstate.max_nodes);
	curstate.max_nodes = gnome_config_get_int(buf);
	if (curstate.max_nodes < 0)
		curstate.max_nodes = 0;

	g_snprintf(buf,256,"/genius/properties/chop=%d",
		   curstate.chop);
	curstate.chop = gnome_config_get_int(buf);
	if (curstate.chop < 0)
		curstate.chop = 0;
	else if (curstate.chop > MAX_CHOP)
		curstate.chop = MAX_CHOP;

	g_snprintf(buf,256,"/genius/properties/chop_when=%d",
		   curstate.chop_when);
	curstate.chop_when = gnome_config_get_int(buf);
	if (curstate.chop_when < 0)
		curstate.chop_when = 0;
	else if (curstate.chop_when > MAX_CHOP)
		curstate.chop_when = MAX_CHOP;

	g_snprintf(buf,256,"/genius/properties/float_prec=%d",
		   curstate.float_prec);
	curstate.float_prec = gnome_config_get_int(buf);
	if (curstate.float_prec < 60)
		curstate.float_prec = 60;
	else if (curstate.float_prec > 16384)
		curstate.float_prec = 16384;

	g_snprintf(buf,256,"/genius/properties/precision_remember=%s",
		   genius_setup.precision_remember?"true":"false");
	genius_setup.precision_remember = gnome_config_get_bool(buf);
}

static void
feed_by_chunks (const char *s, int size)
{
	int i = 0;

	while (i < size) {
		if (size - i > 1024) {
			vte_terminal_feed (VTE_TERMINAL (term), 
					   &(s[i]), 1024);
			i+= 1024;
		} else {
			vte_terminal_feed (VTE_TERMINAL (term), 
					   &(s[i]), size-i);
			break;
		}
	}
}

static void
feed_to_vte_from_string (const char *str, int size)
{
	/*do our own crlf translation*/
	char *s;
	int i,sz;
	for(i=0,sz=0;i<size;i++,sz++) {
		if (str[i] == '\n') {
			sz++;
		}
	}
	if (sz == size) {
		feed_by_chunks (str, size);
		/* FIXME: bugs in vte cause segfaults
		vte_terminal_feed (VTE_TERMINAL (term), 
				   str, size);
				   */
		return;
	}
	s = g_new(char,sz);
	for(i=0,sz=0;i<size;i++,sz++) {
		if(str[i]=='\n') {
			s[sz++] = str[i];
			s[sz] = '\r';
		} else s[sz] = str[i];
	}
	feed_by_chunks (s, sz);
	/* FIXME: bugs in vte cause segfaults
	vte_terminal_feed (VTE_TERMINAL (term), 
			   s, sz);
			   */
	g_free(s);
}

static void
output_notify_func (GelOutput *output)
{
	const char *s = gel_output_peek_string (output);
	if (s != NULL) {
		feed_to_vte_from_string ((char *)s, strlen (s));
		gel_output_clear_string (output);
	}
}

static int
get_term_width(GelOutput *gelo)
{
	return vte_terminal_get_column_count (VTE_TERMINAL (term));
}

static void
set_state (calcstate_t state)
{
	curstate = state;

	if (state.full_expressions ||
	    state.output_style == GEL_OUTPUT_LATEX ||
	    state.output_style == GEL_OUTPUT_MATHML ||
	    state.output_style == GEL_OUTPUT_TROFF)
		gel_output_set_length_limit (main_out, FALSE);
	else
		gel_output_set_length_limit (main_out, TRUE);
}

static void
check_events (void)
{
	if (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
tree_limit_hit (void)
{
	if (genius_ask_question (NULL,
				 _("Memory (node number) limit has been reached, interrupt the computation?"))) {
		interrupted = TRUE;
		/*genius_interrupt_calc ();*/
	}
}

static int
catch_interrupts (GtkWidget *w, GdkEvent *e)
{
	if (e->type == GDK_KEY_PRESS &&
	    e->key.keyval == GDK_c &&
	    e->key.state & GDK_CONTROL_MASK) {
		genius_interrupt_calc ();
		return TRUE;
	}
	return FALSE;
}

static void
open_plugin_cb (GtkWidget *w, GelPlugin * plug)
{
	gel_open_plugin (plug);
}

static void
fork_a_helper (void)
{
	char *argv[6];
	char *foo;
	char *dir;
	char *libexecdir;
	char *file;

	libexecdir = gbr_find_libexec_dir (LIBEXECDIR);

	foo = NULL;

	if (genius_in_dev_dir &&
	    access ("./genius-readline-helper-fifo", X_OK) == 0)
		foo = g_strdup ("./genius-readline-helper-fifo");

	file = g_build_filename (libexecdir, "genius-readline-helper-fifo",
				 NULL);
	if (foo == NULL && access (file, X_OK) == 0)
		foo = file;
	else
		g_free (file);

	if (foo == NULL) {
		dir = g_path_get_dirname (arg0);
		foo = g_build_filename
			(dir, ".." , "libexec",
			 "genius-readline-helper-fifo", NULL);
		if (access (foo, X_OK) != 0) {
			g_free (foo);
			foo = NULL;
		}
		if (foo == NULL) {
			foo = g_build_filename
				(dir, "genius-readline-helper-fifo", NULL);
			if (access (foo, X_OK) != 0) {
				g_free (foo);
				foo = NULL;
			}
		}

		g_free (dir);
	}
	if (foo == NULL)
		foo = g_find_program_in_path ("genius-readline-helper-fifo");

	if (foo == NULL) {
		GtkWidget *d = geniusbox (TRUE /*error*/,
					  FALSE /*always textbox*/,
					  NULL /*textbox_title*/,
					  FALSE /*bind_response*/,
					  FALSE /*wrap*/,
					  _("Can't execute genius-readline-helper-fifo!\n"));

		gtk_dialog_run (GTK_DIALOG (d));

		unlink (fromrlfifo);
		unlink (torlfifo);

		exit (1);
	}

	argv[0] = foo;

	argv[1] = torlfifo;
	argv[2] = fromrlfifo;

	argv[3] = NULL;

	helper_pid = vte_terminal_fork_command (VTE_TERMINAL (term),
						foo,
						argv,
						NULL /* envv */,
						NULL /* directory */,
						FALSE /* lastlog */,
						FALSE /* utmp */,
						FALSE /* wtmp */);

	g_free (libexecdir);
	g_free (foo);
}

static gboolean
get_new_line (GIOChannel *source, GIOCondition condition, gpointer data)
{
	int fd = g_io_channel_unix_get_fd (source);
	int r;
	char buf[5] = "EOF!";

	if ( ! (condition & G_IO_IN))
		return TRUE;

	do {
		r = read (fd, buf, 4);
	} while (errno == EINTR);
	if (r == 4) {
		if (strcmp (buf, "EOF!") == 0) {
			get_cb_p_expression (NULL, torlfp);
		} else if (strcmp (buf, "LINE") == 0) {
			int len = 0;
			do {
				r = read (fd, (gpointer) &len, sizeof (int));
			} while (errno == EINTR);
			if (r != sizeof(int))
				g_warning("Weird size from helper");
			if (len > 0) {
				char *b;
				b = g_new0(char,len+1);
				do {
					r = read (fd, b, len);
				} while (errno == EINTR);
				if (r != len)
					g_warning ("Didn't get all the data from helper");
				get_cb_p_expression (b, torlfp);
				g_free(b);
			} else
				get_cb_p_expression ("", torlfp);
		}
	} else {
		g_warning("GOT a strange response from the helper");
	}

	return TRUE;
}

static void
genius_got_etree (GelETree *e)
{
	if (e != NULL) {
		calc_running ++;
		gel_evalexp_parsed (e, main_out, "= \e[1;36m", TRUE);
		gel_test_max_nodes_again ();
		calc_running --;
		gel_output_full_string (main_out, "\e[0m");
		gel_output_flush (main_out);
	}

	gel_printout_infos ();

	if (gel_got_eof) {
		gel_output_full_string (main_out, "\n");
		gel_output_flush (main_out);
		gel_got_eof = FALSE;
		gtk_main_quit();
	}
}

static char *
make_a_fifo (const char *postfix)
{
	if (g_get_home_dir () != NULL) {
		char *name = g_strdup_printf ("%s/.genius-fifo-%s",
					      g_get_home_dir (),
					      postfix);
		/* this will not work if we don't own this, but this will
		 * make sure we clean up old links */
		unlink (name);
		if (mkfifo (name, 0600) == 0) {
			return name;
		}
		g_free (name);
	}
       
	for (;;) {
		char *name = g_strdup_printf ("/tmp/genius-fifo-%x-%s",
					      (guint)g_random_int (),
					      postfix);
		/* this will not work if we don't own this, but this will
		 * make sure we clean up old links */
		unlink (name);
		if (mkfifo (name, 0600) == 0) {
			return name;
		}
		g_free (name);
	}
}

static void
setup_rl_fifos (void)
{
	torlfifo = make_a_fifo ("torl");
	fromrlfifo = make_a_fifo ("fromrl");
}

static void
selection_changed (void)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	if (page == 0) {
		gboolean can_copy =
			vte_terminal_get_has_selection (VTE_TERMINAL (term));
		gtk_widget_set_sensitive (edit_menu[EDIT_COPY_ITEM].widget,
					  can_copy);
	}
}

static void
switch_page (GtkNotebook *notebook, GtkNotebookPage *page, guint page_num)
{
	if (page_num == 0) {
		/* console */
		gtk_widget_set_sensitive (file_menu[FILE_CLOSE_ITEM].widget,
					  FALSE);
		if (selected_program == NULL) {
			gtk_widget_set_sensitive
				(calc_menu[CALC_RUN_ITEM].widget,
				 FALSE);
			gtk_widget_set_sensitive
				(toolbar[TOOLBAR_RUN_ITEM].widget,
				 FALSE);
			gtk_widget_set_sensitive
				(file_menu[FILE_RELOAD_ITEM].widget,
				 FALSE);
			gtk_widget_set_sensitive
				(file_menu[FILE_SAVE_ITEM].widget,
				 FALSE);
			gtk_widget_set_sensitive
				(file_menu[FILE_SAVE_AS_ITEM].widget,
				 FALSE);
		}
		/* selection changed updates the copy item sensitivity */
		selection_changed ();
		gtk_widget_set_sensitive (edit_menu[EDIT_CUT_ITEM].widget,
					  FALSE);
#ifdef HAVE_GTKSOURCEVIEW
		setup_undo_redo ();
#endif
		gnome_appbar_pop (GNOME_APPBAR (appbar));
	} else {
		char *s;
		GtkWidget *w;
		/* something else */
		gtk_widget_set_sensitive (edit_menu[EDIT_CUT_ITEM].widget,
					  TRUE);
		gtk_widget_set_sensitive (edit_menu[EDIT_COPY_ITEM].widget,
					  TRUE);
		gtk_widget_set_sensitive (file_menu[FILE_CLOSE_ITEM].widget,
					  TRUE);
		gtk_widget_set_sensitive (calc_menu[CALC_RUN_ITEM].widget,
					  TRUE);
		gtk_widget_set_sensitive (toolbar[TOOLBAR_RUN_ITEM].widget,
					  TRUE);
		gtk_widget_set_sensitive (file_menu[FILE_SAVE_AS_ITEM].widget,
					  TRUE);

		if (selected_program != NULL) {
			selected_program->selected = FALSE;
			setup_label (selected_program);
		}

		w = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
					       page_num);
		selected_program = g_object_get_data (G_OBJECT (w), "program");
		selected_program->selected = TRUE;

		setup_label (selected_program);

		gtk_widget_set_sensitive (file_menu[FILE_RELOAD_ITEM].widget,
					  selected_program->real_file);
		gtk_widget_set_sensitive (file_menu[FILE_SAVE_ITEM].widget,
					  selected_program->real_file);

		gnome_appbar_pop (GNOME_APPBAR (appbar));
		s = g_strdup_printf (_("Line: %d"),
				     selected_program->curline + 1);
		gnome_appbar_push (GNOME_APPBAR (appbar), s);
		g_free (s);

#ifdef HAVE_GTKSOURCEVIEW
		setup_undo_redo ();
#endif
	}
}

static const char *
get_version_details (void)
{
	static GString *str = NULL;
	if (str != NULL)
		return str->str;
	str = g_string_new (NULL);

#ifndef HAVE_GTKSOURCEVIEW
	g_string_append (str, _("\nNote: Compiled without GtkSourceView (better source editor)"));
#endif
	return str->str;
}

static gboolean
is_uri (const char *s)
{
	const char *p;
	if ( ! s)
		return FALSE;

	for (p = s; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'); p++)
		;
	if (p == s)
		return FALSE;
	if (*p == ':') {
		GnomeVFSURI *uri =
			gnome_vfs_uri_new (s);

		if (uri != NULL) {
			gnome_vfs_uri_unref (uri);
			return TRUE;
		} else {
			return FALSE;
		}
	}
	return FALSE;
}

static void
loadup_files_from_cmdline (GnomeProgram *program)
{
	GValue value = { 0, };
	poptContext ctx;
	char **args;
	int i;

	g_value_init (&value, G_TYPE_POINTER);
	g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
	ctx = g_value_get_pointer (&value);
	g_value_unset (&value);

	args = (char**) poptGetArgs(ctx);
	for (i = 0; args != NULL && args[i] != NULL; i++) {
		char *fn;
		if (is_uri (args[i])) {
			fn = g_strdup (args[i]);
		} else if (g_path_is_absolute (args[i])) {
			fn = gnome_vfs_get_uri_from_local_path (args[i]);
		} else {
			char *d = g_get_current_dir ();
			char *n = g_build_filename (d, args[i], NULL);
			fn = gnome_vfs_get_uri_from_local_path (n);
			g_free (d);
			g_free (n);
		}
		new_program (fn);
		g_free (fn);
	}
}

static void 
drag_data_received (GtkWidget *widget, GdkDragContext *context, 
		    gint x, gint y, GtkSelectionData *selection_data, 
		    guint info, guint time)
{
	GList *list;
	GList *li;
	
	if (info != TARGET_URI_LIST)
		return;
			
	list = gnome_vfs_uri_list_parse ((gpointer)selection_data->data);

	for (li = list; li != NULL; li = li->next) {
		const GnomeVFSURI *uri = li->data;
		char *s = gnome_vfs_uri_to_string (uri,
						   GNOME_VFS_URI_HIDE_NONE);
		new_program (s);
	}
	
	gnome_vfs_uri_list_free (list);
}

static void
update_term_geometry (void)
{
	GdkGeometry hints;
	int char_width;
	int char_height;
	int xpad, ypad;

	char_width = VTE_TERMINAL (term)->char_width;
	char_height = VTE_TERMINAL (term)->char_height;
  
	vte_terminal_get_padding (VTE_TERMINAL (term), &xpad, &ypad);

	hints.base_width = xpad;
	hints.base_height = ypad;

#define MIN_WIDTH_CHARS 10
#define MIN_HEIGHT_CHARS 4

	hints.width_inc = char_width;
	hints.height_inc = char_height;

	/* min size is min size of just the geometry widget, remember. */
	hints.min_width = hints.base_width + hints.width_inc * MIN_WIDTH_CHARS;
	hints.min_height = hints.base_height + hints.height_inc * MIN_HEIGHT_CHARS;

	gtk_window_set_geometry_hints (GTK_WINDOW (genius_window),
				       term,
				       &hints,
				       GDK_HINT_RESIZE_INC |
				       GDK_HINT_MIN_SIZE |
				       GDK_HINT_BASE_SIZE);
}


int
main (int argc, char *argv[])
{
	GtkWidget *hbox;
	GtkWidget *w;
	char *file;
	GnomeUIInfo *plugins;
	int plugin_count = 0;
	GIOChannel *channel;
	GnomeProgram *program;

	genius_is_gui = TRUE;

	arg0 = g_strdup (argv[0]); 

	/* kind of a hack to find out if we are being run from the
	 * directory we were built in */
	file = g_get_current_dir ();
	if (file != NULL &&
	    strcmp (file, BUILDDIR "/src") == 0 &&
	    access ("genius.c", F_OK) == 0 &&
	    access ("../lib/lib.cgel", F_OK) == 0) {
		genius_in_dev_dir = TRUE;
	} else {
		genius_in_dev_dir = FALSE;
		gbr_init (NULL);
	}
	g_free (file);
	
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	genius_datadir = gbr_find_data_dir (DATADIR);
	genius_datadir_sourceview = g_build_filename (genius_datadir, "genius",
						      "gtksourceview"
						      G_DIR_SEPARATOR_S,
						      NULL);

	program = gnome_program_init ("genius", VERSION, 
				      LIBGNOMEUI_MODULE /* module_info */,
				      argc, argv,
				      GNOME_PARAM_APP_DATADIR, genius_datadir,
				      /* GNOME_PARAM_POPT_TABLE, options, */
				      NULL);

	setup_rl_fifos ();

	main_out = gel_output_new();
	gel_output_setup_string (main_out, 80, get_term_width);
	gel_output_set_notify (main_out, output_notify_func);
	
	evalnode_hook = check_events;
	statechange_hook = set_state;
	_gel_tree_limit_hook = tree_limit_hit;

	gel_read_plugin_list ();

	/*read gnome_config parameters */
	get_properties ();

	file = g_build_filename (genius_datadir,
				 "icons",
				 "hicolor",
				 "48x48",
				 "apps",
				 "gnome-genius.png",
				 NULL);
	if (access (file, F_OK) == 0)
		gtk_window_set_default_icon_from_file (file, NULL);
	g_free (file);

	
        /*set up the top level window*/
	genius_window = create_main_window();


	/* Drag and drop support */
	gtk_drag_dest_set (GTK_WIDGET (genius_window),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drag_types, n_drag_types,
			   GDK_ACTION_COPY);
		
	g_signal_connect (G_OBJECT (genius_window), "drag_data_received",
			  G_CALLBACK (drag_data_received), 
			  NULL);

	/*set up the tooltips*/
	tips = gtk_tooltips_new();

	/* setup the notebook */
	notebook = gtk_notebook_new ();
	g_signal_connect (G_OBJECT (notebook), "switch_page",
			  G_CALLBACK (switch_page), NULL);
	gtk_widget_show (notebook);
	gnome_app_set_contents (GNOME_APP (genius_window), notebook);

	/*the main box to put everything in*/
	hbox = gtk_hbox_new(FALSE,0);

	term = vte_terminal_new ();
	vte_terminal_set_scrollback_lines (VTE_TERMINAL (term),
					   genius_setup.scrollback);
	vte_terminal_set_cursor_blinks (VTE_TERMINAL (term), TRUE);
	vte_terminal_set_audible_bell (VTE_TERMINAL (term), TRUE);
	vte_terminal_set_scroll_on_keystroke (VTE_TERMINAL (term), TRUE);
	vte_terminal_set_scroll_on_output (VTE_TERMINAL (term), FALSE);
	vte_terminal_set_word_chars (VTE_TERMINAL (term),
				     "-A-Za-z0-9/_:.,?+%=");
	vte_terminal_set_backspace_binding (VTE_TERMINAL (term),
					    VTE_ERASE_ASCII_BACKSPACE);
	/* FIXME: how come does backspace and not delete */
	vte_terminal_set_delete_binding (VTE_TERMINAL (term),
					 VTE_ERASE_ASCII_DELETE);
	g_signal_connect (G_OBJECT (term), "selection_changed",
			  G_CALLBACK (selection_changed),
			  NULL);
	g_signal_connect (G_OBJECT (term), "event",
			  G_CALLBACK (catch_interrupts),
			  NULL);

	gtk_box_pack_start (GTK_BOX (hbox), term, TRUE, TRUE, 0);
	
	w = gtk_vscrollbar_new
		(vte_terminal_get_adjustment (VTE_TERMINAL (term)));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	
	if (gel_plugin_list != NULL) {
		GSList *li;
		int i;
		plugins = g_new0(GnomeUIInfo,g_slist_length(gel_plugin_list)+1);
		genius_menu[PLUGINS_MENU].moreinfo = plugins;
		
		for (i = 0, li = gel_plugin_list;
		     li != NULL;
		     li = li->next, i++) {
			GelPlugin *plug = li->data;
			if (plug->hide)
				continue;
			plugins[i].type = GNOME_APP_UI_ITEM;
			plugins[i].label = g_strdup(plug->name);
			plugins[i].hint = g_strdup(plug->description);
			plugins[i].moreinfo = GTK_SIGNAL_FUNC(open_plugin_cb);
			plugins[i].user_data = plug;
			plugins[i].pixmap_type = GNOME_APP_PIXMAP_NONE;
			plugin_count ++;
		}
		plugins[i].type = GNOME_APP_UI_ENDOFINFO;
	}

	/*set up the menu*/
        gnome_app_create_menus(GNOME_APP(genius_window), genius_menu);
	/*set up the toolbar*/
	gnome_app_create_toolbar (GNOME_APP(genius_window), toolbar);

	/* if no plugins, hide the menu */
	if (plugin_count == 0) {
		gtk_widget_hide (genius_menu[PLUGINS_MENU].widget);
	}

	/*setup appbar*/
	appbar = gnome_appbar_new(FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar(GNOME_APP(genius_window), appbar);
	gtk_widget_show(appbar);

	gnome_app_install_menu_hints(GNOME_APP(genius_window),
				     genius_menu);

	/*set up the main window*/
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  hbox,
				  gtk_label_new (_("Console")));
	/* FIXME:
	gtk_widget_queue_resize (vte);
	*/
	gtk_container_set_border_width(
		GTK_CONTAINER (GNOME_APP (genius_window)->contents), 5);

	{
		int width = 800;
		int height = 600;

		if (width > gdk_screen_width () * 0.75)
			width = gdk_screen_width () * 0.75;
		if (height > gdk_screen_height () * 0.75)
			height = gdk_screen_height () * 0.75;

		gtk_window_set_default_size (GTK_WINDOW (genius_window), width, height);
	}

	gtk_widget_show_all (genius_window);

	/* Try to deduce the standard font size, kind of evil, but sorta
	 * works.  The user can always set the font themselves. */
	{
		GtkStyle *style = gtk_widget_get_style (genius_window);
		int sz = (style == NULL ||
			  style->font_desc == NULL) ? 10 :
			pango_font_description_get_size (style->font_desc) / PANGO_SCALE;
		if (sz == 0) sz = 10;
		default_console_font = g_strdup_printf ("Monospace %d", sz);
	}

	/* for some reason we must set the font here and not above
	 * or the "monospace 12" (or default terminal font or whatnot)
	 * will get used */
	vte_terminal_set_font_from_string (VTE_TERMINAL (term),
					   ve_string_empty (genius_setup.font) ?
					     default_console_font :
					     genius_setup.font);
	setup_term_color ();
	vte_terminal_set_cursor_blinks
		(VTE_TERMINAL (term),
		 genius_setup.blinking_cursor);
	vte_terminal_set_encoding (VTE_TERMINAL (term), "UTF-8");

	update_term_geometry ();
	g_signal_connect (G_OBJECT (term), "char-size-changed",
			  G_CALLBACK (update_term_geometry), NULL);

	gtk_widget_show_now (genius_window);

	gel_output_printf (main_out,
			   _("%sGenius %s%s\n"
			     "%s\n"
			     "This is free software with ABSOLUTELY NO WARRANTY.\n"
			     "For license details type `%swarranty%s'.\n"
			     "For help type '%smanual%s' or '%shelp%s'.%s\n\n"),
			   "\e[0;32m" /* green */,
			   "\e[0m" /* white on black */,
			   VERSION,
			   COPYRIGHT_STRING,
			   "\e[01;36m" /* cyan */,
			   "\e[0m" /* white on black */,
			   "\e[01;36m" /* cyan */,
			   "\e[0m" /* white on black */,
			   "\e[01;36m" /* cyan */,
			   "\e[0m" /* white on black */,
			   get_version_details ());
	gel_output_flush (main_out);
	check_events ();

	set_new_calcstate (curstate);
	set_new_errorout (geniuserror);
	set_new_infoout (geniusinfo);

	fork_a_helper ();

	torlfp = fopen (torlfifo, "w");

	fromrl = open (fromrlfifo, O_RDONLY);
	g_assert (fromrl >= 0);

	channel = g_io_channel_unix_new (fromrl);
	g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, G_IO_IN | G_IO_HUP | G_IO_ERR, 
			     get_new_line, NULL, NULL);
	g_io_channel_unref (channel);

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext ();

	gel_init ();

	gel_add_graph_functions ();

	/*
	 * Read main library
	 */
	if (genius_in_dev_dir) {
		/*try the library file in the current/../lib directory*/
		gel_load_compiled_file (NULL, "../lib/lib.cgel", FALSE);
	} else {
		file = g_build_filename (genius_datadir,
					       "genius",
					       "gel",
					       "lib.cgel",
					       NULL);
		gel_load_compiled_file (NULL, file, FALSE);
		g_free (file);
	}

	/*
	 * Read init files
	 */
	file = g_build_filename (g_get_home_dir (), ".geniusinit",NULL);
	if(file)
		gel_load_file(NULL, file, FALSE);
	g_free(file);

	gel_load_file (NULL, "geniusinit.gel", FALSE);

	/* Add a default last answer */
	d_addfunc (d_makevfunc (d_intern ("Ans"),
				gel_makenum_string
				(_("The only thing that "
				   "interferes with my "
				   "learning is my education.  "
				   "-- Albert Einstein"))));

	/*
	 * Restore plugins
	 */
	gel_restore_plugins ();

	gel_printout_infos ();

	gtk_widget_grab_focus (term);

	/* act like the selection changed to disable the copy item */
	selection_changed ();

	start_cb_p_expression (genius_got_etree, torlfp);

	/* Load all given files */
	loadup_files_from_cmdline (program);

	gtk_main ();

	/*
	 * Save properties and plugins
	 */
	set_properties ();
	gel_save_plugins ();

	close (fromrl);
	fclose (torlfp);

	unlink (fromrlfifo);
	unlink (torlfifo);

	return 0;
}
