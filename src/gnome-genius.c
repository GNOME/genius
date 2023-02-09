/* GENIUS Calculator
 * Copyright (C) 1997-2023 Jiri (George) Lebl
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

#include <vte/vte.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#include "examples.h"
#include "inter.h"

#include "binreloc.h"

#include <vicious.h>
#include <viciousui.h>

#include <readline/readline.h>
#include <readline/history.h>

#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksource.h>
#endif

#include <gio/gio.h>

#include "gnome-genius.h"

/* if we want icons on menus, right now it seems to me it looks better without */
/*#define ICONS_ON_MENUS 1*/

/*Globals:*/

const gboolean genius_is_gui = TRUE;

/*calculator state*/
GelCalcState curstate={
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

long total_errors = 0;

#define MAX_CHOP 1000

static void check_events (void);
const GelHookFunc gel_evalnode_hook = check_events;

static void finished_toplevel_exec (void);
const GelHookFunc _gel_finished_toplevel_exec_hook = finished_toplevel_exec;

static void tree_limit_hit (void);
const GelHookFunc _gel_tree_limit_hook = tree_limit_hit;

extern int parenth_depth;
extern const char *genius_toplevels[];

static GtkApplication *genius_app = NULL;

GtkWidget *genius_window = NULL;
static GtkAccelGroup *genius_accel_group = NULL;
GtkWidget *genius_window_statusbar = NULL;
static GtkWidget *example_menu;
static GtkWidget *plugin_menu;
static GtkWidget *prog_menu;

static GHashTable *genius_menu_items = NULL;

int gel_calc_running = 0;

static GtkWidget *setupdialog = NULL;
static GtkWidget *term = NULL;
static GtkWidget *genius_notebook = NULL;
static GString *errors = NULL;
static GString *infos = NULL;
static GtkRecentManager *recent_manager;

static GtkToolItem *interrupt_tb_button = NULL;
static GtkToolItem *run_tb_button = NULL;
static GtkToolItem *new_tb_button = NULL;
static GtkToolItem *open_tb_button = NULL;
static GtkToolItem *save_tb_button = NULL;
static GtkToolItem *plot_tb_button = NULL;
static GtkToolItem *quit_tb_button = NULL;

static char *clipboard_str = NULL;

static int errors_printed = 0;

static char *last_dir = NULL;

static GList *prog_menu_items = NULL;

static char *genius_datadir = NULL;
static char *genius_datadir_sourceview = NULL;

gboolean genius_in_dev_dir = FALSE;

static gboolean genius_do_not_use_binreloc = FALSE;

GeniusSetup genius_setup = {
	FALSE /* error_box */,
	TRUE /* info_box */,
	TRUE /* blinking_cursor */,
	1000 /* scrollback */,
	NULL /* font */,
	FALSE /* black on white */,
	FALSE /* output_remember */,
	FALSE /* precision_remember */,
	NULL /* editor_color_scheme */
};

typedef struct {
	char *name;
	char *vname; /* visual name */
	int ignore_changes;
	int curline;
	gboolean changed;
	gboolean real_file;
	gboolean selected;
	gboolean readonly;
	GtkWidget *tv;
	GtkTextBuffer *buffer;
	GtkWidget *label;
	GtkWidget *mlabel;
} Program;

enum {
	TARGET_URI_LIST = 100
};

/* kate seems usable on various themes so we default to it */
#define DEFAULT_COLOR_SCHEME "kate"

#define TERMINAL_PALETTE_SIZE 16
const GdkRGBA
terminal_palette_black_on_white[TERMINAL_PALETTE_SIZE] =
{
  { 0, 0, 0, 1 },
  { 0.666, 0, 0, 1 },
  { 0, 0.533, 0, 1 },
  { 0.666, 0.333, 0, 1 },
  { 0, 0, 0.666, 1 },
  { 0.666, 0, 0.666, 1 },
  { 0, 0.666, 0.666, 1 },
  { 0.666, 0.666, 0.666, 1 },

  { 0, 0, 0, 1 },
  { 0.666, 0, 0, 1 },
  { 0, 0.533, 0, 1 },
  { 0.666, 0.333, 0, 1 },
  { 0, 0, 0.666, 1 },
  { 0.666, 0, 0.666, 1 },
  { 0, 0.533, 0.666, 1 },
  { 0.666, 0.666, 0.666, 1 },
};

static GtkTargetEntry drag_types[] = {
	{ (char *)"text/uri-list", 0, TARGET_URI_LIST },
};

static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);

static Program *selected_program = NULL;
static Program *running_program = NULL;

static char *default_console_font = NULL;

pid_t helper_pid = -1;

static FILE *torlfp = NULL;
static int fromrl = -1;

static char *torlfifo = NULL;
static char *fromrlfifo = NULL;

static char *arg0 = NULL;

static void new_callback (GtkWidget *w, gpointer data);
static void open_callback (GtkWidget *w, gpointer data);
static void save_callback (GtkWidget *w, gpointer data);
static void save_all_cb (GtkWidget *w, gpointer data);
static void save_console_cb (GtkWidget *w, gpointer data);
static void save_as_callback (GtkWidget *w, gpointer data);
static void close_callback (GtkWidget *w, gpointer data);
static void load_cb (GtkWidget *w, gpointer data);
static void reload_cb (GtkWidget *w, gpointer data);
static void quitapp (GtkWidget *w, gpointer data);
#ifdef HAVE_GTKSOURCEVIEW
static void setup_undo_redo (void);
static void undo_callback (GtkWidget *w, gpointer data);
static void redo_callback (GtkWidget *w, gpointer data);
#endif
static void cut_callback (GtkWidget *w, gpointer data);
static void copy_callback (GtkWidget *w, gpointer data);
static void paste_callback (GtkWidget *w, gpointer data);
static void clear_cb (GtkClipboard *clipboard, gpointer owner);
static void copy_cb (GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner);
static void copy_answer (void);
static void copy_as_plain (GtkWidget *w, gpointer data);
static void copy_as_latex (GtkWidget *w, gpointer data);
static void copy_as_troff (GtkWidget *w, gpointer data);
static void copy_as_mathml (GtkWidget *w, gpointer data);
static void next_tab (GtkWidget *w, gpointer data);
static void prev_tab (GtkWidget *w, gpointer data);
static void show_console (GtkWidget *w, gpointer data);
static void prog_menu_activated (GtkWidget *item, gpointer data);
static void setup_calc (GtkWidget *w, gpointer data);
static void run_program (GtkWidget *w, gpointer data);
static void genius_interrupt_calc_cb (GtkWidget *w, gpointer data);
static void genius_plot_dialog_cb (GtkWidget *w, gpointer data);
static void show_user_vars (GtkWidget *w, gpointer data);
static void monitor_user_var (GtkWidget *w, gpointer data);
static void full_answer (GtkWidget *w, gpointer data);
static void warranty_call (GtkWidget *w, gpointer data);
static void aboutcb (GtkWidget *w, gpointer data);
static void help_cb (GtkWidget *w, gpointer data);
static void help_on_function (GtkWidget *w, gpointer data);
static void executing_warning (void);
static void display_warning (GtkWidget *parent, const char *warn);

static void actually_open_help (const char *id);

static void fork_helper_setup_comm (void);

static void new_program (const char *filename,
			 gboolean example);

/* right now we're ignoring the icon, see ICONS_ON_MENUS */
typedef struct {
	const char *path;
	const char *icon;
	const char *name;
	const char *acc;
	const char *tooltip;
	void (*callback)(GtkWidget *, gpointer);
} GeniusMenuItem;

static void
simple_menu_item_select_cb (GtkMenuItem *item, gpointer data)
{
	char *message = data;

	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (genius_window_statusbar), 0 /* context */,
				    message);
	}
}

static void
simple_menu_item_deselect_cb (GtkMenuItem *item, gpointer data)
{
	gtk_statusbar_pop (GTK_STATUSBAR (genius_window_statusbar), 0 /* context */);
} 


static GtkWidget *
create_menu (const GeniusMenuItem entries[])
{
	GtkWidget *menu;
	GtkWidget *item;
#ifdef ICONS_ON_MENUS
	GtkWidget *image;
	GtkWidget *box;
#endif
	GtkWidget *label;
	guint acckey;
	GdkModifierType accmods;
	int i;

	menu = gtk_menu_new();
	
	for (i = 0; entries[i].path != NULL; i++) {
		if (strcmp (entries[i].path, "-") == 0) {
			item = gtk_separator_menu_item_new ();
		} else {
			item = gtk_menu_item_new ();
			g_hash_table_insert (genius_menu_items,
					     g_strdup (entries[i].path),
					     item);
#ifdef ICONS_ON_MENUS
			box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
			if (entries[i].icon != NULL) {
				image = gtk_image_new_from_icon_name (entries[i].icon,
								      GTK_ICON_SIZE_MENU);
				gtk_container_add (GTK_CONTAINER (box), image);
			}
#endif

			label = gtk_accel_label_new (_(entries[i].name));

			gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
			gtk_label_set_xalign (GTK_LABEL (label), 0.0);

			if (entries[i].acc != NULL) {
				gtk_accelerator_parse (entries[i].acc,
						       &acckey,
						       &accmods);
				if (acckey != 0) {
					gtk_widget_add_accelerator (item,
								    "activate",
								    genius_accel_group,
								    acckey,
								    accmods,
								    GTK_ACCEL_VISIBLE);
					gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), item);
				}
			}

#ifdef ICONS_ON_MENUS
			gtk_box_pack_end (GTK_BOX (box), label, TRUE, TRUE, 0);

			gtk_container_add (GTK_CONTAINER (item), box);
#else
			gtk_container_add (GTK_CONTAINER (item), label);
#endif

			if (entries[i].callback != NULL) {
				g_signal_connect (G_OBJECT (item), "activate",
						  G_CALLBACK (entries[i].callback),
						  NULL);
			}
			if (entries[i].tooltip) {
				g_signal_connect (item, "select",
						  G_CALLBACK (simple_menu_item_select_cb), 
						  _(entries[i].tooltip));
				g_signal_connect (item, "deselect",
						  G_CALLBACK (simple_menu_item_deselect_cb),
						  NULL);
			}
		}

		gtk_widget_show_all (item);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	gtk_widget_show(menu);

	return menu;
}

/* right now we're ignoring the icon, see ICONS_ON_MENUS */

static const GeniusMenuItem file_entries[] = {
        { "app.new", "document-new", N_("_New Program"),
          "<Control>n", N_("Create new program tab"),
          new_callback },
        { "app.open", "document-open", N_("_Open..."),
          "<Control>o", N_("Open a file"),
          open_callback },
        { "app.open-recent", "document-open", N_("Open R_ecent"),
          NULL, NULL,
          NULL },
        { "app.save", "document-save", N_("_Save"),
          "<Control>s", N_("Save current file"),
          save_callback },
        { "app.save-all", "document-save", N_("Save All _Unsaved"),
          NULL, N_("Save all unsaved programs"),
	  save_all_cb },
        { "app.save-as", "document-save", N_("Save _As..."),
          "<Shift><Control>s", N_("Save to a file"),
	  save_as_callback },
        { "app.reload", "document-revert", N_("_Reload from Disk"),
          NULL, N_("Reload the selected program from disk"),
	  reload_cb },
        { "app.close", "window-close", N_("_Close"),
          "<Control>w", N_("Close the current file"),
	  close_callback },
	{ "-" },
        { "app.load-run", "document-open", N_("_Load and Run..."),
          NULL, N_("Load and execute a file in genius"),
	  load_cb },
	{ "-" },
        { "app.save-console", "document-save", N_("Save Console Ou_tput..."),
          NULL, N_("Save what is visible on the console "
                   "(including scrollback) to a text file"),
	  save_console_cb },
	{ "-" },
        { "app.quit", "application-exit", N_("_Quit"),
          "<Control>q", N_("Quit"),
	  quitapp },
        { NULL }
};


static const GeniusMenuItem edit_entries[] = {
#ifdef HAVE_GTKSOURCEVIEW
        { "app.undo", "edit-undo",  N_("_Undo"),
          "<Control>z", N_("Undo the last action"),
	  undo_callback },
        { "app.redo", "edit-redo", N_("_Redo"),
          "<Shift><Control>z", N_("Redo the undone action"),
	  redo_callback },
	{ "-" },
#endif
        { "app.cut", "edit-cut", N_("Cu_t"),
          "<Control>x", N_("Cut the selection"),
	  cut_callback },
        { "app.copy", "edit-copy", N_("_Copy"),
          "<Control>c", N_("Copy the selection"),
	  copy_callback },
        { "app.paste", "edit-paste", N_("_Paste"),
          "<Control>v", N_("Paste the clipboard"),
	  paste_callback },
	{ "-" },
        { "app.copy-plain", "edit-copy", N_("Copy Answer As Plain Te_xt"),
          NULL, N_("Copy last answer into the clipboard in plain text"),
	  copy_as_plain },
        { "app.copy-latex", "edit-copy", N_("Copy Answer As _LaTeX"),
          NULL, N_("Copy last answer into the clipboard as LaTeX"),
	  copy_as_latex },
        { "app.copy-mathml", "edit-copy", N_("Copy Answer As _MathML"),
          NULL, N_("Copy last answer into the clipboard as MathML"),
	  copy_as_mathml },
        { "app.copy-troff", "edit-copy", N_("Copy Answer As T_roff"),
          NULL, N_("Copy last answer into the clipboard as Troff eqn"),
	  copy_as_troff },
        { NULL }
};

static const GeniusMenuItem calc_entries[] = {
        { "app.run", "system-run", N_("_Run"),
          "<Control>r", N_("Run current program"),
	  run_program },
        { "app.stop", "process-stop", N_("_Interrupt"),
          "<Control>i", N_("Interrupt current calculation"),
	  genius_interrupt_calc_cb },
	{ "-" },
        { "app.answer", "dialog-information", N_("Show _Full Answer"),
          NULL, N_("Show the full text of last answer"),
	  full_answer },
        { "app.vars", "dialog-information", N_("Show User _Variables"),
          NULL, N_("Show the current value of all user variables"),
	  show_user_vars },
        { "app.monitor", "dialog-information", N_("_Monitor a Variable"),
          NULL, N_("Monitor a variable continuously"),
	  monitor_user_var },
	{ "-" },
        { "app.plot", "genius-stock-plot", N_("_Plot..."),
          NULL, N_("Plot functions, vector fields, surfaces, etc..."),
	  genius_plot_dialog_cb },
        { NULL }
};

static const GeniusMenuItem prog_entries[] = {
        { "app.next", "go-next", N_("_Next Tab"),
          "<Control>Page_Down", N_("Go to next tab"),
	  next_tab },
        { "app.previous", "go-previous", N_("_Previous Tab"),
          "<Control>Page_Up", N_("Go to previous tab"),
	  prev_tab },
	{ "-" },
        { "app.console", NULL, N_("_Console"),
          NULL, N_("Go to the console tab"),
	  show_console },
        { NULL }
};

static const GeniusMenuItem pref_entries[] = {
        { "app.prefs", "preferences-system", N_("_Preferences"),
          NULL, N_("Configure Genius"),
	  setup_calc },
        { NULL }
};

static const GeniusMenuItem help_entries[] = {
        { "app.help", "help-browser", N_("_Contents"),
          "F1", N_("View the Genius manual"),
	  help_cb },
        { "app.help-func", "help-browser", N_("_Help on Function"),
          NULL, N_("Help on a function or a command"),
	  help_on_function },
        { "app.warranty", "help-browser", N_("_Warranty"),
          NULL, N_("Display warranty information"),
	  warranty_call },
	{ "-" },
        { "app.about", "help-about", N_("_About"),
          NULL, N_("About Genius"),
	  aboutcb },
        { NULL }
};

static void
enable_menuitem (const gchar *name, gboolean enabled)
{
	GtkWidget *item = g_hash_table_lookup (genius_menu_items, name);
	g_return_if_fail (item != NULL);

	gtk_widget_set_sensitive (item, enabled);
}

static GtkWidget *
recent_create_menu (void)
{
        GtkWidget *recent_menu;
        GtkRecentFilter *recent_filter;

        recent_menu  =
                gtk_recent_chooser_menu_new_for_manager (recent_manager);
        gtk_recent_chooser_set_show_icons
		(GTK_RECENT_CHOOSER (recent_menu), TRUE);
        gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (recent_menu), 10);
        gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (recent_menu),
					  GTK_RECENT_SORT_MRU);

        recent_filter = gtk_recent_filter_new ();
        gtk_recent_filter_add_mime_type (recent_filter,
					 "text/x-genius");
        gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (recent_menu),
				       recent_filter);

        return recent_menu;
}

static void 
file_open_recent (GtkRecentChooser *chooser, gpointer data)
{
        GtkRecentInfo *item;
	const char *uri;

        g_return_if_fail (chooser && GTK_IS_RECENT_CHOOSER(chooser));

        item = gtk_recent_chooser_get_current_item (chooser);
        if (item == NULL)
                return;

        uri = gtk_recent_info_get_uri (item);

	new_program (uri, FALSE);

        gtk_recent_info_unref (item);
}

static void
recent_add (const char *uri)
{
	GtkRecentData *data;

        static char *groups[2] = {
                (char *)"gnome-genius",
                NULL
        };

	data = g_slice_new0 (GtkRecentData);

        data->display_name = NULL;
        data->description = NULL;
        data->mime_type = (char *)"text/x-genius";
        data->app_name = (char *) g_get_application_name ();
        data->app_exec = g_strconcat (g_get_prgname (), " %u", NULL);
        data->groups = groups;
        data->is_private = FALSE;

	gtk_recent_manager_add_full (recent_manager, uri, data);

        g_free (data->app_exec);
	g_slice_free (GtkRecentData, data);
}

static void
add_main_window_contents (GtkWidget *window, GtkWidget *notebook)
{
	GtkWidget *box1, *recent_menu;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *menu;
	GtkWidget *submenu;
	GtkWidget *item;
	GtkToolItem *titem;
	GtkWidget *image;

	menubar = gtk_menu_bar_new ();

	menu = gtk_menu_item_new_with_mnemonic (_("_File"));
	submenu = create_menu (file_entries);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	item = g_hash_table_lookup (genius_menu_items, "app.open-recent");
	g_assert (item != NULL);
	recent_menu  = recent_create_menu ();
	g_signal_connect (G_OBJECT (recent_menu), "item-activated",
			  G_CALLBACK (file_open_recent), NULL);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), recent_menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menu);

	menu = gtk_menu_item_new_with_mnemonic (_("_Edit"));
	submenu = create_menu (edit_entries);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menu);

	menu = gtk_menu_item_new_with_mnemonic (_("_Calculator"));
	submenu = create_menu (calc_entries);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menu);

	example_menu = gtk_menu_item_new_with_mnemonic (_("E_xamples"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), example_menu);

	plugin_menu = gtk_menu_item_new_with_mnemonic (_("P_lugins"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), plugin_menu);

	prog_menu = gtk_menu_item_new_with_mnemonic (_("_Programs"));
	submenu = create_menu (prog_entries);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (prog_menu), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), prog_menu);

	menu = gtk_menu_item_new_with_mnemonic (_("_Settings"));
	submenu = create_menu (pref_entries);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menu);

	menu = gtk_menu_item_new_with_mnemonic (_("_Help"));
	submenu = create_menu (help_entries);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menu);

	toolbar = gtk_toolbar_new ();
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH);

	image = gtk_image_new_from_icon_name ("process-stop",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	interrupt_tb_button = titem =
		gtk_tool_button_new (image, _("Interrupt"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (genius_interrupt_calc_cb), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	image = gtk_image_new_from_icon_name ("system-run",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	run_tb_button = titem =
		gtk_tool_button_new (image, _("Run"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (run_program), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	image = gtk_image_new_from_icon_name ("document-new",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	new_tb_button = titem =
		gtk_tool_button_new (image, _("New"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (new_callback), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	image = gtk_image_new_from_icon_name ("document-open",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	open_tb_button = titem =
		gtk_tool_button_new (image, _("Open"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (open_callback), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	image = gtk_image_new_from_icon_name ("document-save",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	save_tb_button = titem =
		gtk_tool_button_new (image, _("Save"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (save_callback), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	image = gtk_image_new_from_icon_name ("genius-stock-plot",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	plot_tb_button = titem =
		gtk_tool_button_new (image, _("Plot"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (genius_plot_dialog_cb), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	image = gtk_image_new_from_icon_name ("application-exit",
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	quit_tb_button = titem =
		gtk_tool_button_new (image, _("Quit"));
	g_signal_connect (G_OBJECT (titem), "clicked",
			  G_CALLBACK (quitapp), NULL);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	gtk_widget_show_all (toolbar);

	genius_window_statusbar = gtk_statusbar_new ();

	gtk_container_set_border_width (GTK_CONTAINER (window), 0);

	box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (window), box1);

	gtk_box_pack_start (GTK_BOX (box1),
			    menubar,
			    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (box1),
			    toolbar,
			    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (box1), notebook, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (box1), genius_window_statusbar, FALSE, TRUE, 0);
}


void
genius_setup_window_cursor (GtkWidget *win, GdkCursorType type)
{
	GdkCursor *cursor
                = gdk_cursor_new_for_display (gdk_display_get_default (), type);
	if (win != NULL && gtk_widget_get_window (win) != NULL)
		gdk_window_set_cursor (gtk_widget_get_window (win), cursor);
	g_object_unref (cursor);
}

void
genius_unsetup_window_cursor (GtkWidget *win)
{
	if (win != NULL && gtk_widget_get_window (win) != NULL)
		gdk_window_set_cursor (gtk_widget_get_window (win), NULL);
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
	GFile *uri;
	gboolean res;
		
	g_return_val_if_fail (text_uri != NULL, FALSE);
	
	uri = g_file_new_for_uri (text_uri);
	g_return_val_if_fail (uri != NULL, FALSE);

	res = g_file_query_exists (uri, NULL);

	g_object_unref (uri);

	return res;
}

static void
setup_term_color (void)
{
	if (genius_setup.black_on_white) {
		const GdkRGBA black = {0, 0, 0, 1};
		const GdkRGBA white = {1, 1, 1, 1};
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

int
gel_ask_buttons (const char *query, GSList *buttonlist)
{
	GtkWidget *d;
	GtkWidget *box;
	GSList *li;
	int i;
	int ret;

	d = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (d), _("Genius"));

	i = 1;
	for (li = buttonlist; li != NULL; li = li->next) {
		gtk_dialog_add_button (GTK_DIALOG (d),
				       ve_sure_string (li->data),
				       i);
		i++;
	}

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (d))),
			    box,
			    TRUE, TRUE, 0);


	gtk_box_pack_start (GTK_BOX (box),
			    gtk_label_new (ve_sure_string(query)),
			    FALSE, FALSE, 0);

	gtk_widget_show_all (d);
	ret = ve_dialog_run_nonmodal (GTK_DIALOG (d));
	gtk_widget_destroy (d);

	if (ret < 0)
		return -1;
	else
		return ret;
}

char *
gel_ask_string (const char *query, const char *def)
{
	GtkWidget *d;
	GtkWidget *e;
	GtkWidget *box;
	int ret;
	char *txt = NULL;

	d = gtk_dialog_new_with_buttons
		(_("Genius"),
		 NULL /*GTK_WINDOW (genius_window)*/ /* parent */,
		 0 /* flags */,
		 _("_Cancel"), GTK_RESPONSE_CANCEL,
		 _("_OK"), GTK_RESPONSE_OK,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (d))),
			    box,
			    TRUE, TRUE, 0);


	gtk_box_pack_start (GTK_BOX (box),
			    gtk_label_new (ve_sure_string(query)),
			    FALSE, FALSE, 0);

	e = gtk_entry_new ();
	if ( ! ve_string_empty (def)) {
		gtk_entry_set_text (GTK_ENTRY (e), def);
	}
	g_signal_connect (G_OBJECT (e), "activate",
			  G_CALLBACK (dialog_entry_activate), d);
	gtk_box_pack_start (GTK_BOX (box),
			    e,
			    FALSE, FALSE, 0);

	gtk_widget_show_all (d);
	ret = ve_dialog_run_nonmodal (GTK_DIALOG (d));

	if (ret == GTK_RESPONSE_OK) {
		const char *t = gtk_entry_get_text (GTK_ENTRY (e));
		txt = g_strdup (ve_sure_string (t));
	}

	gtk_widget_destroy (d);

	return txt;
}

static void
help_cb (GtkWidget *w, gpointer data)
{
	actually_open_help (NULL /* id */);
}

static void
help_on_function (GtkWidget *w, gpointer data)
{
	GtkWidget *d;
	GtkWidget *e;
	GtkWidget *box;
	GtkEntryCompletion *completion;
	GtkListStore *model;
	int ret;
	int i;
	GSList *funcs;
	GSList *li;
	

	d = gtk_dialog_new_with_buttons
		(_("Help on Function"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Cancel"), GTK_RESPONSE_CANCEL,
		 _("_OK"), GTK_RESPONSE_OK,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (d))),
			    box,
			    TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (box),
			    gtk_label_new (_("Function or command name:")),
			    FALSE, FALSE, 0);

	e = gtk_entry_new ();
	g_signal_connect (G_OBJECT (e), "activate",
			  G_CALLBACK (dialog_entry_activate), d);
	gtk_box_pack_start (GTK_BOX (box),
			    e,
			    FALSE, FALSE, 0);

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_text_column(completion, 0);
	gtk_entry_set_completion (GTK_ENTRY (e), completion);
        model = gtk_list_store_new(1, G_TYPE_STRING);

	for (i = 0; genius_toplevels[i] != NULL; i++) {
		GtkTreeIter iter;
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter, 0, genius_toplevels[i], -1);
	}

	funcs = d_getcontext();
	
	for (li = funcs; li != NULL; li = li->next) {
		GelEFunc *f = li->data;
		GtkTreeIter iter;
		if (f->id == NULL ||
		    f->id->token == NULL)
			continue;
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter, 0, f->id->token, -1);
	}
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (model));
 

run_help_dlg_again:
	gtk_widget_show_all (d);
	ret = gtk_dialog_run (GTK_DIALOG (d));

	if (ret == GTK_RESPONSE_OK) {
		char *txt = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (e))));
		GelHelp *help = gel_get_help (txt, FALSE /* insert */);
		gboolean found = FALSE;

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
	   const char *s,
	   GtkWidget *parent_win)
{
	GtkWidget *mb;
	/* if less than 10 lines */
	if (count_char (ve_sure_string (s), '\n') <= 10 &&
	    ! always_textbox) {
		GtkMessageType type = GTK_MESSAGE_INFO;
		if (error)
			type = GTK_MESSAGE_ERROR;
		mb = gtk_message_dialog_new (parent_win ?
					       GTK_WINDOW (parent_win) :
					       NULL /* parent */,
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
		GdkDisplay *display;
		GdkMonitor *monitor;
		GdkRectangle geom;
		const char *title;

		if (textbox_title != NULL)
			title = textbox_title;
		else if (error)
			title = _("Error");
		else
			title = _("Information");

		mb = gtk_dialog_new_with_buttons
			(title,
			 genius_window ?
			   GTK_WINDOW (genius_window) :
			   NULL /* parent */,
			 0 /* flags */,
			 _("_OK"), GTK_RESPONSE_OK,
			 NULL);
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (mb))),
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
		display = gdk_display_get_default ();
		monitor = gdk_display_get_primary_monitor (display);
		gdk_monitor_get_geometry (monitor, &geom);
		gtk_window_set_default_size
			(GTK_WINDOW (mb),
			 MIN (geom.width - 50, 800),
			 MIN (geom.height - 50, 450));
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
	GelContextFrame *all_contexts, *lic;
	GSList *funcs;
	GSList *li;
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
		for (lic = all_contexts; lic != NULL && lic->next != NULL; lic = lic->next) {
			GelToken *tok = lic->name;
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
		for (li = lic->functions; li != NULL; li = li->next) {
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
show_user_vars (GtkWidget *w, gpointer data)
{
	static GtkWidget *var_box = NULL;
	GtkWidget *sw;
	GtkWidget *tv;
	static GtkTextBuffer *buffer = NULL;
	GdkMonitor *monitor;
	GdkRectangle geom;

	if (var_box != NULL) {
		populate_var_box (buffer);
		return;
	}

	var_box = gtk_dialog_new_with_buttons
		(_("User Variable Listing"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Refresh"), 1,
		 _("_Close"), GTK_RESPONSE_CLOSE,
		 NULL);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (var_box))),
			    sw,
			    TRUE, TRUE, 0);

	tv = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (tv), FALSE);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));

	gtk_container_add (GTK_CONTAINER (sw), tv);

	/* FIXME: 
	 * Perhaps should be smaller with smaller font ...
	 * ... */
	monitor = gdk_display_get_primary_monitor (gdk_display_get_default ());
	gdk_monitor_get_geometry (monitor, &geom);
	gtk_window_set_default_size
		(GTK_WINDOW (var_box),
		 MIN (geom.width - 50, 800),
		 MIN (geom.height - 50, 450));
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

			if (func->data.user->type == GEL_MATRIX_NODE)
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
		 _("_Close"), GTK_RESPONSE_CLOSE,
		 NULL);
	g_free (s);

	g_signal_connect (G_OBJECT (d), "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (d))),
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
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (d))),
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
monitor_user_var (GtkWidget *w, gpointer data)
{
	GtkWidget *d;
	GtkWidget *e;
	GtkWidget *box;
	int ret;

	d = gtk_dialog_new_with_buttons
		(_("Monitor a Variable"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Cancel"), GTK_RESPONSE_CANCEL,
		 _("_OK"), GTK_RESPONSE_OK,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (d))),
			    box,
			    TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (box),
			    gtk_label_new (_("Variable name:")),
			    FALSE, FALSE, 0);

	e = gtk_entry_new ();
	g_signal_connect (G_OBJECT (e), "activate",
			  G_CALLBACK (dialog_entry_activate), d);
	gtk_box_pack_start (GTK_BOX (box),
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
full_answer (GtkWidget *w, gpointer data)
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
			if (ans->data.user->type == GEL_MATRIX_NODE)
				wrap = FALSE;
			gel_pretty_print_etree (out, ans->data.user);
		} else {
			/* ugly? maybe! */
			GelETree n;
			n.type = GEL_FUNCTION_NODE;
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
		   ve_sure_string (s),
		   genius_window /* parent */);

	gel_output_unref (out);
}


static void
printout_error_num_and_reset(GtkWidget *parent)
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
				   errors->str,
				   parent);
			g_string_free(errors,TRUE);
			errors=NULL;
		}
	} else {
		if(errors_printed-curstate.max_errors > 0) {
			gel_output_printf(gel_main_out,
					  _("\e[01;31mToo many errors! (%d followed)\e[0m\n"),
					  errors_printed-curstate.max_errors);
			gel_output_flush (gel_main_out);
		}
	}
	errors_printed = 0;
}

static char *
resolve_file (const char *file)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
	int i;

	for (i = 0; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (genius_notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		if (p == NULL) /* console */
			continue;
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

	total_errors ++;

	if (curstate.max_errors > 0 &&
	    errors_printed++ >= curstate.max_errors)
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
		gel_output_printf_full (gel_main_out, FALSE,
					"\e[01;31m%s\e[0m\r\n", str);
		gel_output_flush (gel_main_out);
	}

	g_free(str);
}

void
gel_printout_infos_parent (GtkWidget *parent)
{
	/* Print out the infos */
	if (infos != NULL) {
		geniusbox (FALSE /*error*/,
			   FALSE /*always textbox*/,
			   NULL /*textbox_title*/,
			   TRUE /*bind_response*/,
			   FALSE /* wrap */,
			   infos->str,
			   parent);
		g_string_free (infos, TRUE);
		infos = NULL;
	}

	printout_error_num_and_reset (parent);
}

void
gel_printout_infos (void)
{
	gel_printout_infos_parent (genius_window);
}

static char *
get_help_lang (void)
{
	const char * const* langs;
	int i;

	langs = g_get_language_names ();
	for (i = 0; langs[i] != NULL; i++) {
		char *file;
		file = g_build_filename (genius_datadir,
					 "genius",
					 "help",
					 langs[i],
					 "html",
					 "index.html",
					 NULL);
		if (access (file, F_OK) == 0) {
			g_free (file);
			return g_strdup (langs[i]);
		}
		g_free (file);
	}
	return g_strdup("C");
}

static void
actually_open_help (const char *id)
{
	GError *error = NULL;
	char *str;
	char *lang;

#if 0
	if (id != NULL) {
		str = g_strdup_printf ("ghelp:genius?%s", id);
	} else {
		str = g_strdup ("ghelp:genius");
	}
#endif

	lang = get_help_lang ();

	str = g_strdup_printf ("file://%s/genius/help/%s/html/index.html",
			       genius_datadir, lang);

	if (id != NULL) {
		char buf[256];
		gboolean found = FALSE;
		char *command = g_strdup_printf ("fgrep -l 'name=\"%s\"' '%s'/genius/help/%s/html/*.html",
						 id,
						 genius_datadir,
						 lang);
		FILE *fp = popen (command, "r");
		if (fp != NULL) {
			if (fgets (buf,(int)sizeof(buf),fp) != NULL) {
				char *p = strchr (buf, '\n');
				if (p != NULL) *p = '\0';
				g_free(str);
				str = g_strdup_printf ("file://%s#%s", buf, id);
				found = TRUE;

			}
			pclose (fp);
		}
		if ( ! found) {
			char *warn = g_strdup_printf (_("<b>Help on %s not found</b>"), id);
			display_warning (NULL /* parent */, warn);
			g_free (warn);
		}
	}

	gtk_show_uri_on_window (NULL, str, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		char *err = g_strdup_printf
			(_("<b>Cannot display help</b>\n\n%s"),
			 error->message);
		genius_display_error (NULL /* parent */, err);
		g_free (err);
		g_error_free (error);
	}

	g_free (str);

#if 0
	if G_UNLIKELY (error != NULL) {
		char *gnomehelp = NULL;
		if (g_error_matches (error, G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED) &&
		    (gnomehelp = g_find_program_in_path("gnome-help")) != NULL) {
			char *argv[3];

			g_error_free (error);
			error = NULL;

			argv[0] = gnomehelp;
			argv[1] = str;
			argv[2] = NULL;
			g_spawn_async (NULL /* wd */,
				       argv,
				       NULL /* envp */,
				       0 /* flags */,
				       NULL /* child_setup */,
				       NULL /* user_data */,
				       NULL /* child_pid */,
				       &error);
			g_free (gnomehelp);
		}
		if (error != NULL) {
			char *err = g_strdup_printf
				(_("<b>Cannot display help</b>\n\n%s"),
				 error->message);
			genius_display_error (NULL /* parent */, err);
			g_free (err);
		}
		g_error_free (error);
	}

#endif
}

void
gel_call_help (const char *function)
{
	if (function == NULL) {
		actually_open_help (NULL);
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

		actually_open_help (id);

		g_free (id);
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
		gel_output_printf_full (gel_main_out, FALSE,
					"\e[32m%s\e[0m\r\n", str);
		gel_output_flush (gel_main_out);
	}

	g_free(str);
}

/*about box*/
static void
aboutcb (GtkWidget *w, gpointer data)
{
	static const char *authors[] = {
		"Jiří (George) Lebl, Ph.D. <jirka@5z.com>",
		N_("Nils Barth (initial implementation of parts of the GEL library)"),
		N_("Adrian E. Feiguin <feiguin@ifir.edu.ar> (GtkExtra - plotting widgetry)"),
		N_("Yavor Doganov <yavor@gnu.org> (Port to GTK3)"),
		NULL
	};
	static const char *documenters[] = {
		"Jiří (George) Lebl, Ph.D. <jirka@5z.com>",
		"Kai Willadsen",
		NULL
	};
	const char *translators;
	char *license;
	/* Translators should localize the following string
	 * which will give them credit in the About box.
	 * E.g. "Fulano de Tal <fulano@detal.com>"
	 */
	const char *new_credits = N_("translator-credits");
	GdkPixbuf *logo;
	char *file;

	/* hack for old translations */
	const char *old_hack = "translator_credits-PLEASE_ADD_YOURSELF_HERE";

	/* Force translation */
	authors[1] = _(authors[1]);
	authors[2] = _(authors[2]);

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
		    _(GENIUS_COPYRIGHT_STRING));
	gtk_show_about_dialog (GTK_WINDOW (genius_window),
			      "program-name", _("Genius Mathematical Tool"), 
			      "version", VERSION,
			      "copyright", _(GENIUS_COPYRIGHT_STRING),
			      "comments",
			      _("The GNOME calculator style edition of "
				"the Genius Mathematical Tool."),
			      "authors", authors,
			      "documenters", documenters,
			      "translator-credits", translators,
			      "logo", logo,
			      "license", license,
			      "website", "http://www.jirka.org/genius.html",
			      NULL);
	g_free (license);

	if (logo != NULL)
		g_object_unref (logo);
}

static void
set_properties (void)
{
	const char *home = g_get_home_dir ();
	char *name;
	VeConfig *cfg;

	if (home == NULL)
		/* FIXME: errors */
		return;

	name = g_build_filename (home, ".genius", "config-gui", NULL);
	cfg = ve_config_new (name);
	g_free (name);

	ve_config_set_bool (cfg, "properties/black_on_white",
			    genius_setup.black_on_white);
	ve_config_set_string (cfg, "properties/pango_font",
			      ve_sure_string (genius_setup.font));
	ve_config_set_int (cfg, "properties/scrollback",
			   genius_setup.scrollback);
	ve_config_set_bool (cfg, "properties/error_box",
			    genius_setup.error_box);
	ve_config_set_bool (cfg, "properties/info_box",
			    genius_setup.info_box);
	ve_config_set_bool (cfg, "properties/blinking_cursor",
			    genius_setup.blinking_cursor);
	ve_config_set_bool (cfg, "properties/output_remember",
			    genius_setup.output_remember);
	if (genius_setup.output_remember) {
		ve_config_set_int (cfg, "properties/max_digits", 
				   curstate.max_digits);
		ve_config_set_bool (cfg, "properties/results_as_floats",
				    curstate.results_as_floats);
		ve_config_set_bool (cfg, "properties/scientific_notation",
				    curstate.scientific_notation);
		ve_config_set_bool (cfg, "properties/full_expressions",
				    curstate.full_expressions);
		ve_config_set_bool (cfg, "properties/mixed_fractions",
				    curstate.mixed_fractions);
		ve_config_set_int (cfg, "properties/chop",
				   curstate.chop);
		ve_config_set_int (cfg, "properties/chop_when",
				   curstate.chop_when);
	}
	ve_config_set_int (cfg, "properties/max_errors",
			   curstate.max_errors);
	ve_config_set_int (cfg, "properties/max_nodes",
			   curstate.max_nodes);
	ve_config_set_bool (cfg, "properties/precision_remember",
			    genius_setup.precision_remember);
	if (genius_setup.precision_remember) {
		ve_config_set_int (cfg, "properties/float_prec",
				   curstate.float_prec);
	}
	ve_config_set_string (cfg, "properties/editor_color_scheme",
			      ve_sure_string (genius_setup.editor_color_scheme));
	
	ve_config_save (cfg, FALSE /* force */);

	ve_config_destroy (cfg);
}

void
genius_display_error (GtkWidget *parent, const char *err)
{
	static GtkWidget *w = NULL;

	if (w != NULL)
		gtk_widget_destroy (w);

	if (parent == NULL)
		parent = genius_window;

	w = gtk_message_dialog_new (parent ?
				    GTK_WINDOW (parent) :
				    NULL /* parent */,
				    GTK_DIALOG_MODAL /* flags */,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    NULL);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (w), err);

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
				    NULL);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (w), warn);

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
				      NULL);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (req), question);

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
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
	int i;

	for (i = 1; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (genius_notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		if (p == NULL) /* console */
			continue;
		g_assert (p != NULL);
		if (p->changed)
			return TRUE;
	}
	return FALSE;
}

/* quit */
static gboolean
quitapp_ask (void)
{
	if (any_changed ()) {
		if (gel_calc_running) {
			if ( ! genius_ask_question (NULL,
						    _("Genius is executing something, "
						      "and furthermore there are "
						      "unsaved programs.\nAre "
						      "you sure you wish to quit?")))
				return FALSE;
			gel_interrupted = TRUE;
		} else {
			if ( ! genius_ask_question (NULL,
						    _("There are unsaved programs, "
						      "are you sure you wish to quit?")))
				return FALSE;
		}
	} else {
		if (gel_calc_running) {
			if ( ! genius_ask_question (NULL,
						    _("Genius is executing something, "
						      "are you sure you wish to "
						      "quit?")))
				return FALSE;
			gel_interrupted = TRUE;
		} else {
			if ( ! genius_ask_question (NULL,
						    _("Are you sure you wish "
						      "to quit?")))
				return FALSE;
		}
	}

	g_application_quit (G_APPLICATION (genius_app));

	return TRUE;
}

/* quit */
static void
quitapp (GtkWidget *w, gpointer data)
{
	quitapp_ask ();
}

/*exact answer callback*/
static void
intspincb (GtkAdjustment *adj, int *data)
{
	*data = gtk_adjustment_get_value (adj);
}

/*option callback*/
static void
optioncb (GtkWidget * widget, int *data)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		*data = TRUE;
	else
		*data = FALSE;
}

static void
fontsetcb (GtkFontButton *fb, char **font)
{
	if (*font) g_free(*font);
	*font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (fb));
}

static void
scheme_changed_cb (GtkComboBoxText *cb, char **scheme)
{
	if (*scheme) g_free(*scheme);
	*scheme = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (cb));
}


static GelCalcState tmpstate={0};
static GeniusSetup tmpsetup={0};

static GelCalcState cancelstate={0};
static GeniusSetup cancelsetup={0};

#define COPY_SETUP(to,from) { \
	g_free (to.font);							\
	g_free (to.editor_color_scheme);					\
	to = from;								\
	if (from.font)								\
		to.font = g_strdup (from.font);					\
	if (from.editor_color_scheme)						\
		to.editor_color_scheme = g_strdup (from.editor_color_scheme);	\
}

static void
setup_response (GtkWidget *widget, gint resp, gpointer data)
{
	PangoFontDescription *desc;

	if (resp == GTK_RESPONSE_HELP) {
		actually_open_help ("genius-prefs");
		return;
	}

	if (resp == GTK_RESPONSE_CANCEL ||
	    resp == GTK_RESPONSE_OK ||
	    resp == GTK_RESPONSE_APPLY) {
		if (resp == GTK_RESPONSE_CANCEL) {
			COPY_SETUP(genius_setup,cancelsetup);
			curstate = cancelstate;
		} else {
			COPY_SETUP(genius_setup,tmpsetup);
			curstate = tmpstate;
		}

		gel_set_new_calcstate (curstate);
		vte_terminal_set_scrollback_lines (VTE_TERMINAL (term),
						   genius_setup.scrollback);
		desc = pango_font_description_from_string
	                (ve_string_empty (genius_setup.font)
	                 ? default_console_font
	                 : genius_setup.font);
		vte_terminal_set_font (VTE_TERMINAL (term), desc);
		pango_font_description_free (desc);
		setup_term_color ();
		vte_terminal_set_cursor_blink_mode
			(VTE_TERMINAL (term),
			 genius_setup.blinking_cursor ?
			 VTE_CURSOR_BLINK_SYSTEM :
			 VTE_CURSOR_BLINK_OFF);

#ifdef HAVE_GTKSOURCEVIEW
		{
			GtkSourceStyleSchemeManager *manager;
			GtkSourceStyleScheme *style;
			int i, n;

			manager = gtk_source_style_scheme_manager_get_default ();
			style = NULL;
			if ( ! ve_string_empty (genius_setup.editor_color_scheme))
				style = gtk_source_style_scheme_manager_get_scheme (manager,
										    genius_setup.editor_color_scheme);
			/* "kate" is the default */
			if (style == NULL)
				style = gtk_source_style_scheme_manager_get_scheme (manager, DEFAULT_COLOR_SCHEME);

			n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
			for (i = 0; i < n; i++) {
				GtkWidget *page = gtk_notebook_get_nth_page
					(GTK_NOTEBOOK (genius_notebook), i);
				Program *p = g_object_get_data (G_OBJECT (page), "program");
				if (p != NULL && p->buffer != NULL && style != NULL) {
					gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (p->buffer),
									    style);
				}
			}
		}
#endif

		if (resp == GTK_RESPONSE_OK ||
		    resp == GTK_RESPONSE_CANCEL)
			gtk_widget_destroy (widget);

		/* save properties to file on OK */
		if (resp == GTK_RESPONSE_OK)
			set_properties ();
	}
}

static void
setup_calc (GtkWidget *ww, gpointer data)
{
	GtkWidget *mainbox,*frame;
	GtkWidget *box;
	GtkWidget *b, *w;
	GtkWidget *notebookw;
	GtkAdjustment *adj;

	if (setupdialog) {
		gtk_window_present (GTK_WINDOW (setupdialog));
		return;
	}

	cancelstate = curstate;
	COPY_SETUP(cancelsetup,genius_setup);
	
	tmpstate = curstate;
	COPY_SETUP(tmpsetup,genius_setup);
	
	setupdialog = gtk_dialog_new_with_buttons
		(_("Genius Setup"),
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Help"), GTK_RESPONSE_HELP,
		 _("_Apply"), GTK_RESPONSE_APPLY,
		 _("_Cancel"), GTK_RESPONSE_CANCEL,
		 _("_OK"), GTK_RESPONSE_OK,
		 NULL);

	notebookw = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (setupdialog))),
			    notebookw, TRUE, TRUE, 0);

	/*
	 * Output tab
	 */
	
	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), GENIUS_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebookw),
				  mainbox,
				  gtk_label_new(_("Output")));

	
	frame=gtk_frame_new(_("Number/Expression output options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box), GENIUS_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);


	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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

	gtk_widget_set_tooltip_text
		(w,
		 _("Should the output settings in the "
		   "\"Number/Expression output options\" frame "
		   "be remembered for next session.  Does not apply "
		   "to the \"Error/Info output options\" frame."));

	frame=gtk_frame_new(_("Error/Info output options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);

	gtk_container_set_border_width(GTK_CONTAINER(box), GENIUS_PAD);
	

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
	
	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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

	/*
	 * Precision tab
	 */

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), GENIUS_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebookw),
				  mainbox,
				  gtk_label_new(_("Precision")));

	
	frame=gtk_frame_new(_("Floating point precision"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box), GENIUS_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	gtk_box_pack_start(GTK_BOX(box), gtk_label_new(
		_("NOTE: The floating point precision might not take effect\n"
		  "for all numbers immediately, only new numbers calculated\n"
		  "and new variables will be affected.")),
			   FALSE,FALSE,0);


	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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

	gtk_widget_set_tooltip_text (w,
				     _("Should the precision setting "
				       "be remembered for next session."));

	/*
	 * Terminal tab
	 */

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), GENIUS_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebookw),
				  mainbox,
				  gtk_label_new(_("Terminal")));
	
	frame=gtk_frame_new(_("Terminal options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box), GENIUS_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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
	
	
	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Font:")),
		   FALSE,FALSE,0);
	
        w = gtk_font_button_new_with_font (ve_string_empty (tmpsetup.font) ?
					   default_console_font :
					   genius_setup.font);
        gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
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

#ifdef HAVE_GTKSOURCEVIEW
	/*
	 * Editor tab
	 */

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), GENIUS_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebookw),
				  mainbox,
				  gtk_label_new(_("Editor")));
	
	frame=gtk_frame_new(_("Editor options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(box), GENIUS_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Color scheme")),
		   FALSE,FALSE,0);

	{
		const char * const * schemes;
		int i;
		int active = -1;
		GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();

		schemes = gtk_source_style_scheme_manager_get_scheme_ids (manager);

		w = gtk_combo_box_text_new ();

		for (i = 0; schemes != NULL && schemes[i] != NULL; i++) {
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(w), NULL, schemes[i]);
			if (strcmp (ve_sure_string (tmpsetup.editor_color_scheme), schemes[i]) == 0 ||
			    (ve_string_empty (tmpsetup.editor_color_scheme) &&
			     strcmp (schemes[i], DEFAULT_COLOR_SCHEME)== 0)) {
				active = i;
			}
		}
		if (active < 0) {
			/* This should not happen */
			gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(w), NULL, ve_sure_string(tmpsetup.editor_color_scheme));
			active = i;
		}

		gtk_combo_box_set_active (GTK_COMBO_BOX (w), active);

		g_signal_connect (G_OBJECT (w), "changed",
				  G_CALLBACK (scheme_changed_cb),
				  &tmpsetup.editor_color_scheme);
        	
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	}

#endif

	/*
	 * Memory tab
	 */

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (mainbox), GENIUS_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebookw),
				  mainbox,
				  gtk_label_new(_("Memory")));

	
	frame=gtk_frame_new(_("Limits"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);
	
	gtk_box_pack_start(GTK_BOX(box), gtk_label_new(
		_("When the limit is reached you will be asked if\n"
		  "you wish to interrupt the calculation or continue.\n"
		  "Setting to 0 disables the limit.")),
			   FALSE,FALSE,0);


	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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
	gel_interrupted = TRUE;
	if ( ! gel_calc_running) {
		vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);
	}
}

static void
genius_interrupt_calc_cb (GtkWidget *w, gpointer data)
{
	genius_interrupt_calc ();
}

static void
genius_plot_dialog_cb (GtkWidget *w, gpointer data)
{
	genius_plot_dialog ();
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
warranty_call (GtkWidget *w, gpointer data)
{
	if (gel_calc_running) {
		executing_warning ();
		return;
	} else {
		/* perhaps a bit ugly */
		gboolean last = genius_setup.info_box;
		genius_setup.info_box = TRUE;
		gel_evalexp ("warranty", NULL, gel_main_out, NULL, TRUE, NULL);
		gel_printout_infos ();
		genius_setup.info_box = last;
	}
}

static int
get_console_pagenum (void)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
	int i;
	for (i = 0; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (genius_notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		if (p == NULL) /* console */
			return i;
	}
	return 0; /* should never happen, but that's safe */
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

static GtkFileChooserNative *load_fs = NULL;

static void
load_response_cb (GtkFileChooser *fs, int response, gpointer data)
{
	const char *s;
	char *str;

	load_fs = NULL;

	if (response != GTK_RESPONSE_ACCEPT) {
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

	str = g_strdup_printf ("\r\n\e[0m%s\e[0;32m", 
			       _("Output from "));
	vte_terminal_feed (VTE_TERMINAL (term), str, -1);
	g_free (str);

	vte_terminal_feed (VTE_TERMINAL (term), s, -1);
	vte_terminal_feed (VTE_TERMINAL (term),
			   "\e[0m (((\r\n", -1);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (genius_notebook),
				       get_console_pagenum ());

	gel_calc_running ++;
	gel_load_guess_file (NULL, s, TRUE);
	gel_test_max_nodes_again ();
	gel_calc_running --;

	gel_printout_infos ();

	str = g_strdup_printf ("\e[0m))) %s", _("End"));
	vte_terminal_feed (VTE_TERMINAL (term), str, -1);
	g_free (str);

	/* interrupt the current command line */
	gel_interrupted = TRUE;
	vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);
}

static void
load_cb (GtkWidget *w, gpointer data)
{
	if (load_fs != NULL) {
		gtk_window_present (GTK_WINDOW (load_fs));
		return;
	}

	load_fs = gtk_file_chooser_native_new (_("Load and Run"),
					       GTK_WINDOW (genius_window),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       _("_Load"),
					       _("_Cancel"));

	add_filters (GTK_FILE_CHOOSER (load_fs));

	g_signal_connect (G_OBJECT (load_fs), "response",
			  G_CALLBACK (load_response_cb), NULL);

	if (last_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (load_fs), last_dir);
	} else {
		char *s = g_get_current_dir ();
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (load_fs), s);
		g_free (s);
	}

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (load_fs));
}

#ifdef HAVE_GTKSOURCEVIEW

static guint ur_idle_id = 0;

static gboolean
setup_undo_redo_idle (gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	GtkWidget *w;
	Program *p;

	ur_idle_id = 0;

	if (page < 0)
		return FALSE;

	w = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), page);
	p = g_object_get_data (G_OBJECT (w), "program");

	if (p == NULL) {
	        enable_menuitem ("app.undo", FALSE);
	        enable_menuitem ("app.redo", FALSE);
	} else {
	        enable_menuitem ("app.undo",
				 gtk_source_buffer_can_undo
				 (GTK_SOURCE_BUFFER (p->buffer)));
	        enable_menuitem ("app.redo",
				 gtk_source_buffer_can_redo
				 (GTK_SOURCE_BUFFER (p->buffer)));
	}

	return FALSE;
}

static void
setup_undo_redo (void)
{
	if (ur_idle_id == 0) {
		ur_idle_id = g_idle_add (setup_undo_redo_idle, NULL);
	}
}


static void
undo_callback (GtkWidget *w, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	GtkWidget *pg;
	Program *p;

	if (page < 0)
		return;

	pg = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), page);
	p = g_object_get_data (G_OBJECT (pg), "program");

	if (p == NULL) {
		/* undo from a terminal? what are you talking about */
		return;
	} else {
		if (gtk_source_buffer_can_undo
			(GTK_SOURCE_BUFFER (p->buffer)))
			gtk_source_buffer_undo
				(GTK_SOURCE_BUFFER (p->buffer));
	}
}

static void
redo_callback (GtkWidget *w, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	GtkWidget *pg;
	Program *p;
	
	if (page < 0)
		return;

	pg = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), page);
	p = g_object_get_data (G_OBJECT (pg), "program");

	if (p == NULL) {
		/* redo from a terminal? what are you talking about */
		return;
	} else {
		if (gtk_source_buffer_can_redo
			(GTK_SOURCE_BUFFER (p->buffer)))
			gtk_source_buffer_redo
				(GTK_SOURCE_BUFFER (p->buffer));
	}
}
#endif

static void
cut_callback (GtkWidget *w, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	GtkWidget *pg;
	Program *p;

	if (page < 0)
		return;

	pg = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), page);
	p = g_object_get_data (G_OBJECT (pg), "program");

	if (p == NULL) {
		/* cut from a terminal? what are you talking about */
		return;
	} else {
		gtk_text_buffer_cut_clipboard
			(p->buffer,
			 gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			 TRUE /* default_editable */);
	}
}


static void
copy_callback (GtkWidget *wparam, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	GtkWidget *pg;
	Program *p;

	if (page < 0)
		return;

	pg = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), page);
	p = g_object_get_data (G_OBJECT (pg), "program");

	if (p == NULL) {
	        vte_terminal_copy_clipboard_format (VTE_TERMINAL (term),
	                                            VTE_FORMAT_TEXT);
	} else {
		gtk_text_buffer_copy_clipboard
			(p->buffer,
			 gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
	}
}

static void
paste_callback (GtkWidget *wparam, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	GtkWidget *pg;
	Program *p;

	if (page < 0)
		return;

	pg = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), page);
	p = g_object_get_data (G_OBJECT (pg), "program");

	if (p == NULL) {
		vte_terminal_paste_clipboard (VTE_TERMINAL (term));
	} else {
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
		{(char *)"UTF8_STRING", 0, 0},
		{(char *)"COMPOUND_TEXT", 0, 0},
		{(char *)"TEXT", 0, 0},
		{(char *)"STRING", 0, 0},
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
copy_as_plain (GtkWidget *w, gpointer data)
{
	if (gel_calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_NORMAL;
		gel_set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		gel_set_new_calcstate (curstate);
	}
}

static void
copy_as_latex (GtkWidget *w, gpointer data)
{
	if (gel_calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_LATEX;
		gel_set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		gel_set_new_calcstate (curstate);
	}
}

static void
copy_as_troff (GtkWidget *w, gpointer data)
{
	if (gel_calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_TROFF;
		gel_set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		gel_set_new_calcstate (curstate);
	}
}

static void
copy_as_mathml (GtkWidget *w, gpointer data)
{
	if (gel_calc_running) {
		executing_warning ();
		return;
	} else {
		/* FIXME: Ugly push/pop of output style */
		GelOutputStyle last_style = curstate.output_style;
		curstate.output_style = GEL_OUTPUT_MATHML;
		gel_set_new_calcstate (curstate);

		copy_answer ();

		curstate.output_style = last_style;
		gel_set_new_calcstate (curstate);
	}
}

static void
setup_label (Program *p)
{
	char *s;
	const char *vname;
	const char *pre = "", *post = "", *mark = "", *mark2 = "";

	g_assert (p != NULL);

	if (p->selected) {
		pre = "<big>";
		post = "</big>";
	}

	if (p->real_file &&
	    p->changed) {
		mark = " [+]";
	}

	if (p->real_file &&
	    p->readonly) {
		mark2 = " (RO)";
	}

	vname = p->vname;
	if (vname == NULL)
		vname = "???";

	s = g_strdup_printf ("%s%s%s%s%s", pre, vname, mark, mark2, post);

	gtk_label_set_markup (GTK_LABEL (p->label), s);
	gtk_label_set_markup (GTK_LABEL (p->mlabel), s);

	g_free (s);
}

static void
next_tab (GtkWidget *w, gpointer data)
{
	gtk_notebook_next_page (GTK_NOTEBOOK (genius_notebook));
}

static void
prev_tab (GtkWidget *w, gpointer data)
{
	gtk_notebook_prev_page (GTK_NOTEBOOK (genius_notebook));
}

static void
show_console (GtkWidget *w, gpointer data)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (genius_notebook),
	                               get_console_pagenum ());
}

static void
prog_menu_activated (GtkWidget *item, gpointer data)
{
	GtkWidget *w = data;
	int num;
       
	if (w == NULL)
		num = get_console_pagenum ();
	else
		num = gtk_notebook_page_num (GTK_NOTEBOOK (genius_notebook), w);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (genius_notebook), num);
}

static void
build_program_menu (void)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
	int i;
	GtkWidget *menu;

	while (prog_menu_items != NULL) {
		gtk_widget_destroy (prog_menu_items->data);
		prog_menu_items = g_list_remove_link (prog_menu_items, prog_menu_items);
	}

	menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (prog_menu));

	for (i = 0; i < n; i++) {
		GtkWidget *item;
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (genius_notebook), i);
		Program *p = g_object_get_data (G_OBJECT (w), "program");
		if (p == NULL) /* console */
			continue;

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
save_contents_vfs (const char *filename, const char *str, int size, GError **error)
{
	GFile* file;
	GFileOutputStream* stream;
	gsize bytes;

	file = g_file_new_for_uri (filename);
	stream = g_file_replace (file, NULL, TRUE, G_FILE_CREATE_NONE, NULL, error);
	
	if G_UNLIKELY (stream == NULL) {
		g_object_unref (file);
		return FALSE;
	}


	if G_UNLIKELY ( ! g_output_stream_write_all (G_OUTPUT_STREAM (stream), str, size, &bytes, NULL, error)) {
		g_object_unref(stream);
		g_object_unref(file);
		return FALSE;
	}

	if (size > 0 && str[size-1] != '\n') {
		if G_UNLIKELY ( ! g_output_stream_write (G_OUTPUT_STREAM (stream), "\n", 1, NULL, error)) {
			g_object_unref(stream);
			g_object_unref(file);
			return FALSE;
		}
	}

	if G_LIKELY (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, error)) {
		g_object_unref (stream);
		g_object_unref (file);
		return TRUE;
	} else {
		g_object_unref (stream);
		g_object_unref (file);
		return FALSE;
	}

}

static char *
get_contents_vfs (const char *filename)
{
	GFile* file;
	GFileInputStream* stream;
	gssize bytes;
	char buffer[4096];
	GString *str;

	file = g_file_new_for_uri (filename);
	stream = g_file_read (file, NULL, NULL);

	if (stream == NULL)
	{
		g_object_unref (file);
		return FALSE;
	}

	str = g_string_new (NULL);

	while ((bytes = g_input_stream_read (G_INPUT_STREAM (stream), buffer, sizeof (buffer) -1, NULL, NULL)) > 0)
	{
		buffer[bytes] = '\0';
		g_string_append (str, buffer);
	}
	
	g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
	g_object_unref (stream);
	g_object_unref (file);
	
	return g_string_free (str, FALSE);
}

static void
reload_cb (GtkWidget *w, gpointer data)
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
	if (contents != NULL &&
	    g_utf8_validate (contents, -1, NULL)) {
		gtk_text_buffer_get_iter_at_offset (selected_program->buffer,
						    &iter, 0);
		gtk_text_buffer_insert_with_tags_by_name
			(selected_program->buffer,
			 &iter, contents, -1, "foo", NULL);
		g_free (contents);
		selected_program->changed = FALSE;
	} else {
		genius_display_error (NULL, _("Cannot open file"));
		if (contents != NULL)
			g_free (contents);
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

	gtk_statusbar_pop (GTK_STATUSBAR (genius_window_statusbar),
			   0 /* context */);
	s = g_strdup_printf (_("Line: %d"), line+1);
	gtk_statusbar_push (GTK_STATUSBAR (genius_window_statusbar),
			    0 /* context */, s);
	g_free (s);
}

#ifdef HAVE_GTKSOURCEVIEW
static GtkSourceLanguageManager*
get_source_language_manager (void)
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

static gboolean
file_exists (const char *fname)
{
	GFile* uri;
	gboolean ret;

	if (ve_string_empty (fname))
		return FALSE; 

	uri = g_file_new_for_uri (fname);
	ret = g_file_query_exists (uri, NULL);
	g_object_unref (uri);

	return ret;
}

static gboolean
file_is_writable (const char *fname)
{
	GFile* file;
	GFileInfo* info;
	gboolean ret;
	
	if (ve_string_empty (fname))
		return FALSE; 

	file = g_file_new_for_uri (fname);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, G_FILE_QUERY_INFO_NONE, NULL, NULL);

	if (info == NULL)
	{
		g_object_unref (file);
		return FALSE;
	}
	ret = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
	g_object_unref (info);
	g_object_unref (file);

	return ret;
}

static int
get_program_pagenum (Program *p)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
	int i;
	for (i = 0; i < n; i++) {
		GtkWidget *w = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (genius_notebook), i);
		Program *pp = g_object_get_data (G_OBJECT (w), "program");
		if (p == pp)
			return i;
	}
	return -1; /* should never happen */
}

static void
whack_program (Program *p)
{
	g_assert (p != NULL);

	if (selected_program == p) {
		p->selected = FALSE;
		selected_program = NULL;
		enable_menuitem ("app.reload", FALSE);
		enable_menuitem ("app.save", FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (save_tb_button), FALSE);
		enable_menuitem ("app.save-as", FALSE);
		enable_menuitem ("app.run", FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (run_tb_button), FALSE);
	}
	g_free (p->name);
	g_free (p->vname);
	g_free (p);
}



static void
close_program (Program *p)
{
	int page;

	if (p == NULL)
		return;

	if (p->changed &&
	    ! genius_ask_question (NULL,
				   _("The program you are closing is unsaved, "
				     "are you sure you wish to close it "
				     "without saving?")))
		return;


	page = get_program_pagenum (p);

	if (page >= 0) /* sanity */
		gtk_notebook_remove_page (GTK_NOTEBOOK (genius_notebook), page);
	whack_program (p);

	build_program_menu ();
}


static void
close_button_clicked (GtkWidget *b, gpointer data)
{
	close_program (data);
}

/* if example, filename is a filename and not a uri */
static void
new_program (const char *filename, gboolean example)
{
	char *contents = NULL;
	static int cnt = 1;
	GtkWidget *tv;
	GtkWidget *sw;
	GtkWidget *b, *cl, *im;
	GtkTextBuffer *buffer;
	Program *p;
#ifdef HAVE_GTKSOURCEVIEW
	GtkSourceLanguage *lang;
	GtkSourceLanguageManager *lm;
	GtkSourceStyleSchemeManager *manager;
	GtkSourceStyleScheme *style;
#endif

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
#ifdef HAVE_GTKSOURCEVIEW
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

	manager = gtk_source_style_scheme_manager_get_default ();
	style = NULL;
	if ( ! ve_string_empty (genius_setup.editor_color_scheme))
		style = gtk_source_style_scheme_manager_get_scheme (manager,
								    genius_setup.editor_color_scheme);
	/* "kate" is the default */
	if (style == NULL)
		style = gtk_source_style_scheme_manager_get_scheme (manager, DEFAULT_COLOR_SCHEME);
	if (style != NULL) {
		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buffer),
						    style);
	}

	g_signal_connect (G_OBJECT (buffer), "notify::can-undo",
			  G_CALLBACK (setup_undo_redo), NULL);
	g_signal_connect (G_OBJECT (buffer), "notify::can-redo",
			  G_CALLBACK (setup_undo_redo), NULL);
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
	p->readonly = FALSE;
	p->buffer = buffer;
	p->tv = tv;
	p->curline = 0;
	p->ignore_changes = 0;
	g_object_set_data (G_OBJECT (sw), "program", p);

	g_signal_connect_after (G_OBJECT (p->buffer), "mark_set",
				G_CALLBACK (move_cursor),
				p);

	if (filename == NULL || example) {
		GFile* file;
		char *d = g_get_current_dir ();
		char *n = g_strdup_printf (_("Program_%d.gel"), cnt);
		/* the file name will have an underscore */
		char *fn = g_build_filename (d, n, NULL);
		g_free (d);
		g_free (n);

		file = g_file_new_for_path (fn);
		p->name = g_file_get_uri (file);

		g_object_unref (file);
		g_free (fn);
		p->vname = g_strdup_printf (_("Program %d"), cnt);
		cnt++;
	} else {
		recent_add (filename);
		p->name = g_strdup (filename);
		if (file_exists (filename)) { 
			p->readonly = ! file_is_writable (filename);
			contents = get_contents_vfs (p->name);
		} else {
			p->readonly = FALSE;
			contents = g_strdup ("");
		}
		p->vname = g_path_get_basename (p->name);
		p->real_file = TRUE;
	}

	if (example && filename != NULL) {
		contents = NULL;
		g_file_get_contents (filename, &contents, NULL, NULL);
	}

	if (contents != NULL &&
	    g_utf8_validate (contents, -1, NULL)) {
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
		if (filename != NULL && ! example) {
			char *s = g_strdup_printf (_("Cannot open %s"), filename);
			genius_display_error (NULL, s);
			g_free (s);
		}
		if (contents != NULL)
			g_free (contents);
	}

	/* the label will change after the set_current_page */
	p->label = gtk_label_new (p->vname);
	p->mlabel = gtk_label_new (p->vname);

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (b), p->label, FALSE, FALSE, 0);
	cl = gtk_button_new ();
	gtk_container_set_border_width (GTK_CONTAINER (cl), 0);
	gtk_button_set_relief (GTK_BUTTON (cl), GTK_RELIEF_NONE);
	gtk_widget_set_focus_on_click (cl, FALSE);
	im = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (cl), im);

	g_signal_connect (G_OBJECT (cl), "clicked",
			  G_CALLBACK (close_button_clicked), p);

	gtk_box_pack_start (GTK_BOX (b), cl, FALSE, FALSE, 3);
	gtk_widget_show_all (b);

	gtk_label_set_xalign (GTK_LABEL (p->mlabel), 0.0);
	gtk_notebook_append_page_menu (GTK_NOTEBOOK (genius_notebook), sw,
				       b, p->mlabel);

	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (genius_notebook),
					  sw,
					  TRUE);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (genius_notebook), -1);

	g_signal_connect (G_OBJECT (buffer), "changed",
			  G_CALLBACK (changed_cb), sw);

	build_program_menu ();

#ifdef HAVE_GTKSOURCEVIEW
	setup_undo_redo ();
#endif
}

static void
new_callback (GtkWidget *w, gpointer data)
{
	new_program (NULL, FALSE);
}

static GtkFileChooserNative *open_fs = NULL;

static void
open_response_cb (GtkFileChooser *fs, int response, gpointer data)
{
	const char *s;

	open_fs = NULL;

	if (response != GTK_RESPONSE_ACCEPT) {
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

	new_program (s, FALSE);
}

static void
open_callback (GtkWidget *w, gpointer data)
{
	if (open_fs != NULL) {
		/* FIXME: fake window present */
		gtk_native_dialog_hide (GTK_NATIVE_DIALOG (open_fs));
		gtk_native_dialog_show (GTK_NATIVE_DIALOG (open_fs));
		return;
	}

	open_fs = gtk_file_chooser_native_new (_("Open..."),
					       GTK_WINDOW (genius_window),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       _("_Open"),
					       _("_Cancel"));
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (open_fs), FALSE);

	add_filters (GTK_FILE_CHOOSER (open_fs));

	g_signal_connect (G_OBJECT (open_fs), "response",
			  G_CALLBACK (open_response_cb), NULL);

	if (last_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (open_fs), last_dir);
	} else {
		char *s = g_get_current_dir ();
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (open_fs), s);
		g_free (s);
	}

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (open_fs));
}

static gboolean
save_program (Program *p, const char *new_fname, GError **error)
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

	if ( ! save_contents_vfs (fname, prog, sz, error)) {
		g_free (prog);
		return FALSE;
	}

	recent_add (fname);

	if (p->name != fname) {
		g_free (p->name);
		p->name = g_strdup (fname);
	}
	g_free (p->vname);
	p->vname = g_path_get_basename (fname);
	p->real_file = TRUE;
	p->changed = FALSE;

	if (selected_program == p) {
	        enable_menuitem ("app.reload", TRUE);
	        enable_menuitem ("app.save", TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (save_tb_button), TRUE);
	}

	setup_label (p);

	return TRUE;
}

static void
save_callback (GtkWidget *w, gpointer data)
{
	GError *error = NULL;

	if (selected_program == NULL ||
	    ! selected_program->real_file)
		return;

	if (selected_program->readonly) {
		genius_display_error (NULL, _("Program is read only"));
	} else if ( ! save_program (selected_program, NULL /* new fname */,
				    &error)) {
		char *err;
		if (error != NULL) {
	       		err = g_strdup_printf (_("<b>Cannot save file %s</b>\n"
					       "Details: %s"),
					       selected_program->vname,
					       error->message);
			g_error_free (error);
		} else {
	       		err = g_strdup_printf (_("<b>Cannot save file %s</b>"),
					       selected_program->vname);
		}
		genius_display_error (NULL, err);
		g_free (err);
	}
}

static void
save_all_cb (GtkWidget *w, gpointer data)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (genius_notebook));
	int i;
	GError *error = NULL;
	gboolean there_are_unsaved = FALSE;
	gboolean there_are_readonly_modified = FALSE;

	for (i = 0; i < n; i++) {
		GtkWidget *page = gtk_notebook_get_nth_page
			(GTK_NOTEBOOK (genius_notebook), i);
		Program *p = g_object_get_data (G_OBJECT (page), "program");
		if (p == NULL) /* console */
			continue;

		if (p->changed && ! p->real_file)
			there_are_unsaved = TRUE;

		if (p->changed && p->real_file) {
			if (p->readonly) {
				there_are_readonly_modified = TRUE;
			} else if ( ! save_program (p, NULL /* new fname */,
						    &error)) {
				char *err;
				if (error != NULL) {
					err = g_strdup_printf (_("<b>Cannot save file %s</b>\n"
								 "Details: %s"),
							       p->vname,
							       error->message);
					g_error_free (error);
					error = NULL;
				} else {
					err = g_strdup_printf (_("<b>Cannot save file %s</b>"),
							       p->vname);
				}
				genius_display_error (NULL, err);
				g_free (err);
			}
		}
	}

	if (there_are_unsaved) {
		genius_display_error (NULL, _("Save new programs by "
					      "\"Save As...\" first!"));
	}

	if (there_are_readonly_modified) {
		genius_display_error (NULL,
				      _("Some read-only programs are "
					"modified.  Use \"Save As...\" "
					"to save them to "
					"a new location."));
	}
}

static GtkFileChooserNative *save_fs = NULL;

static void
save_as_response_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	GError *error = NULL;

	save_fs = NULL;

	if (response != GTK_RESPONSE_ACCEPT) {
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
	
	if ( ! save_program (selected_program, s /* new fname */,
			     &error)) {
		char *err;
		if (error != NULL) {
			err = g_strdup_printf (_("<b>Cannot save file</b>\n"
						 "Details: %s"),
					       error->message);
			g_error_free (error);
		} else {
			err = g_strdup (_("<b>Cannot save file</b>"));
		}
		genius_display_error (GTK_WIDGET (fs), err);
		g_free (err);
		g_free (s);
		return;
	}

	g_free (last_dir);
	last_dir = gtk_file_chooser_get_current_folder (fs);

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, TRUE);

	g_free (s);
}

static void
save_as_callback (GtkWidget *w, gpointer data)
{
	/* sanity */
	if (selected_program == NULL)
		return;
	
	if (save_fs != NULL) {
		/* FIXME: fake window present */
		gtk_native_dialog_hide (GTK_NATIVE_DIALOG (save_fs));
		gtk_native_dialog_show (GTK_NATIVE_DIALOG (save_fs));
		return;
	}

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, FALSE);

	save_fs = gtk_file_chooser_native_new (_("Save As..."),
					       GTK_WINDOW (genius_window),
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       _("_Save"),
					       _("_Cancel"));
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (save_fs), FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (save_fs),
							TRUE);

	add_filters (GTK_FILE_CHOOSER (save_fs));

	g_signal_connect (G_OBJECT (save_fs), "response",
			  G_CALLBACK (save_as_response_cb), NULL);

	if (last_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (save_fs), last_dir);
	} else {
		char *s = g_get_current_dir ();
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (save_fs), s);
		g_free (s);
	}
	if (selected_program->real_file) {
		gtk_file_chooser_set_uri
			(GTK_FILE_CHOOSER (save_fs), selected_program->name);
	} else {
		char *bn = g_path_get_basename (selected_program->name);
		gtk_file_chooser_set_current_name
			(GTK_FILE_CHOOSER (save_fs), bn);
		g_free (bn);
	}

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (save_fs));
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

static GtkFileChooserNative *save_console_fs = NULL;

static void
save_console_response_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	char *output;
	glong row, column;
	int sz;
	GError *error = NULL;

	save_console_fs = NULL;

	if (response != GTK_RESPONSE_ACCEPT) {
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
	
	/* this is moronic! VTE has an idiotic API, there is no
	 * way to find out what the proper row range is! */
	vte_terminal_get_cursor_position (VTE_TERMINAL (term),
					  &column, &row);
	output = vte_terminal_get_text_range (VTE_TERMINAL (term),
					      MAX(row-genius_setup.scrollback+1, 0),
					      0,
					      row,
					      vte_terminal_get_column_count (VTE_TERMINAL (term)) - 1,
					      always_selected,
					      NULL,
					      NULL);
	sz = strlen (output);

	if ( ! save_contents_vfs (s, output, sz, &error)) {
		char *err;
		if (error != NULL) {
	       		err = g_strdup_printf (_("<b>Cannot save file</b>\n"
					       "Details: %s"),
					       error->message);
			g_error_free (error);
		} else {
	       		err = g_strdup (_("<b>Cannot save file</b>"));
		}
		genius_display_error (GTK_WIDGET (fs), err);
		g_free (err);
		g_free (output);
		g_free (s);
		return;
	}
	g_free (output);

	g_free (last_dir);
	last_dir = gtk_file_chooser_get_current_folder (fs);

	g_free (s);
}

static void
save_console_cb (GtkWidget *w, gpointer data)
{
	if (save_console_fs != NULL) {
		/* FIXME: fake window present */
		gtk_native_dialog_hide (GTK_NATIVE_DIALOG (save_console_fs));
		gtk_native_dialog_show (GTK_NATIVE_DIALOG (save_console_fs));
		return;
	}

	save_console_fs = gtk_file_chooser_native_new (_("Save Console Output..."),
						       GTK_WINDOW (genius_window),
						       GTK_FILE_CHOOSER_ACTION_SAVE,
						       _("_Save"),
						       _("_Cancel"));
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (save_console_fs), FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (save_console_fs),
							TRUE);

	g_signal_connect (G_OBJECT (save_console_fs), "response",
			  G_CALLBACK (save_console_response_cb), NULL);

	if (last_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (save_console_fs), last_dir);
	} else {
		char *s = g_get_current_dir ();
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (save_console_fs), s);
		g_free (s);
	}

	gtk_file_chooser_set_current_name
		(GTK_FILE_CHOOSER (save_console_fs), "Genius-Console-Output.txt");

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (save_console_fs));
}


static void
close_callback (GtkWidget *w, gpointer data)
{
	GtkWidget *page;
	Program *p;
	int current = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (genius_notebook), current);
	p = g_object_get_data (G_OBJECT (page), "program");
	if (p == NULL) /* if the console */
		return;

	close_program (p);
}

static gboolean
run_program_idle (gpointer data)
{
	const char *vname;
	const char *name;
	GtkTextBuffer *buffer;
	if (selected_program == NULL) /* if nothing is selected */ {
		genius_display_error (NULL,
				      _("<b>No program selected.</b>\n\n"
					"Create a new program, or select an "
					"existing tab in the notebook."));
		return FALSE;
	}
	buffer = selected_program->buffer;
	/* sanity */
	if (buffer == NULL)
		return FALSE;
	name = selected_program->name;
	vname = selected_program->vname;
	if (vname == NULL)
		vname = "???";

	if (gel_calc_running) {
		executing_warning ();
		return FALSE;
	} else {
		GtkTextIter iter, iter_end;
		char *prog;
		int p[2];
		FILE *fp;
		pid_t pid;
		int status;
		char *str;

		errno = 0;
		if (pipe (p) != 0) {
			char *err = 
				g_strdup_printf
				(_("Cannot open pipe: %s"),
				 g_strerror (errno));
			genius_display_error (NULL, err);
			g_free (err);
			return FALSE;
		}

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
		gtk_notebook_set_current_page (GTK_NOTEBOOK (genius_notebook), get_console_pagenum ());

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
			return FALSE;
		}

		if (pid == 0) {
			int len = strlen (prog);
			status = 0;
			close (p[0]);
			if (write (p[1], prog, len) < len) {
				status = 1;
			} else if (prog[len-1] != '\n') {
				while (write (p[1], "\n", 1) == 0)
					;
			}
			close (p[1]);
			_exit (status);
		}
		close (p[1]);
		fp = fdopen (p[0], "r");
		gel_lexer_open (fp);

		gel_calc_running ++;

		g_free (prog);

		running_program = selected_program;

		gel_push_file_info (name, 1);
		/* FIXME: Should not use gel_main_out, we should have a separate
		   console for output, the switching is annoying */
		while (1) {
			gel_evalexp (NULL, fp, gel_main_out, "= \e[1;36m",
				     TRUE, NULL);
			gel_output_full_string (gel_main_out, "\e[0m");
			if (gel_got_eof) {
				gel_got_eof = FALSE;
				break;
			}
			if (gel_interrupted) {
				break;
			}
		}

		gel_pop_file_info ();

		gel_lexer_close (fp);
		fclose (fp);

		gel_test_max_nodes_again ();
		gel_calc_running --;

		gel_printout_infos ();

		running_program = NULL;

		str = g_strdup_printf ("\e[0m))) %s", _("End"));
		vte_terminal_feed (VTE_TERMINAL (term), str, -1);
		g_free (str);

		/* interrupt the current command line */
		gel_interrupted = TRUE;
		vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);

		/* sanity */
		if (pid > 0) {
			/* It must have finished by now, so reap the child */
			/* must kill it, just in case we were interrupted */
			kill (pid, SIGTERM);
			waitpid (pid, &status, 0);
			if (WIFEXITED (status) &&
			    WEXITSTATUS (status) == 1) {
				genius_display_error (NULL,
						      _("<b>Error executing program</b>\n\n"
							"There was an error while writing the\n"
							"program to the engine."));
			}
		}
	}

	return FALSE;
}

static void
run_program (GtkWidget *w, gpointer data)
{
	g_idle_add (run_program_idle, NULL);
}

static gboolean
delete_event (GtkWidget *w, GdkEventAny *e, gpointer data)
{
	return ! quitapp_ask ();
}

static void
create_main_window (GtkWidget *notebook, GApplication *app)
{
	GdkMonitor *monitor;
	GdkRectangle geom;
	char *s;
	int width;
	int height;

	genius_window = gtk_application_window_new (GTK_APPLICATION (app));

	genius_accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (genius_window), genius_accel_group);

	genius_menu_items = g_hash_table_new (g_str_hash, g_str_equal);

	s = g_strdup_printf (_("Genius %s"), VERSION);
	gtk_window_set_title (GTK_WINDOW (genius_window), s);
	g_free (s);

	add_main_window_contents (genius_window, notebook);

	/* Set default_size */
	width = 800;
	height = 600;

	monitor = gdk_display_get_primary_monitor (gdk_display_get_default ());
	gdk_monitor_get_geometry (monitor, &geom);
	if (width > geom.width * 0.75)
		width = geom.width * 0.75;
	if (height > geom.height * 0.75)
		height = geom.height * 0.75;

	gtk_window_set_default_size (GTK_WINDOW (genius_window), width, height);

        g_signal_connect (G_OBJECT (genius_window), "delete_event",
        		  G_CALLBACK (delete_event), NULL);
}

static void
get_properties (void)
{
	char buf[256];
	const char *home = g_get_home_dir ();
	char *name;
	VeConfig *cfg;
	gboolean old_props = FALSE;

	if (home == NULL)
		/* FIXME: error? */
		return;

	name = g_build_filename (home, ".genius", "config-gui", NULL);
	if (access (name, F_OK) != 0) {
		g_free (name);
		name = g_build_filename (home, ".gnome2", "genius", NULL);
		if (access (name, F_OK) != 0) {
			g_free (name);
			return;
		}
		old_props = TRUE;
	}
	cfg = ve_config_new (name);
	g_free (name);

	g_snprintf(buf,256,"properties/black_on_white=%s",
		   (genius_setup.black_on_white)?"true":"false");
	genius_setup.black_on_white = ve_config_get_bool (cfg, buf);

	g_snprintf (buf, 256, "properties/pango_font=%s",
		    ve_sure_string (genius_setup.font));
	genius_setup.font = ve_config_get_string (cfg, buf);

	g_snprintf(buf,256,"properties/scrollback=%d",
		   genius_setup.scrollback);
	genius_setup.scrollback = ve_config_get_int (cfg, buf);

	g_snprintf(buf,256,"properties/error_box=%s",
		   (genius_setup.error_box)?"true":"false");
	genius_setup.error_box = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/info_box=%s",
		   (genius_setup.info_box)?"true":"false");
	genius_setup.info_box = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/blinking_cursor=%s",
		   (genius_setup.blinking_cursor)?"true":"false");
	genius_setup.blinking_cursor = ve_config_get_bool (cfg, buf);
	
	g_snprintf(buf,256,"properties/max_digits=%d",
		   curstate.max_digits);
	curstate.max_digits = ve_config_get_int (cfg, buf);
	if (curstate.max_digits < 0)
		curstate.max_digits = 0;
	else if (curstate.max_digits > 256)
		curstate.max_digits = 256;

	g_snprintf(buf,256,"properties/results_as_floats=%s",
		   curstate.results_as_floats?"true":"false");
	curstate.results_as_floats = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/scientific_notation=%s",
		   curstate.scientific_notation?"true":"false");
	curstate.scientific_notation = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/full_expressions=%s",
		   curstate.full_expressions?"true":"false");
	curstate.full_expressions = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/mixed_fractions=%s",
		   curstate.mixed_fractions?"true":"false");
	curstate.mixed_fractions = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/output_remember=%s",
		   genius_setup.output_remember?"true":"false");
	genius_setup.output_remember = ve_config_get_bool (cfg, buf);

	g_snprintf(buf,256,"properties/max_errors=%d",
		   curstate.max_errors);
	curstate.max_errors = ve_config_get_int (cfg, buf);
	if (curstate.max_errors < 0)
		curstate.max_errors = 0;

	g_snprintf(buf,256,"properties/max_nodes=%d",
		   curstate.max_nodes);
	curstate.max_nodes = ve_config_get_int (cfg, buf);
	if (curstate.max_nodes < 0)
		curstate.max_nodes = 0;

	g_snprintf(buf,256,"properties/chop=%d",
		   curstate.chop);
	curstate.chop = ve_config_get_int (cfg, buf);
	if (curstate.chop < 0)
		curstate.chop = 0;
	else if (curstate.chop > MAX_CHOP)
		curstate.chop = MAX_CHOP;

	g_snprintf(buf,256,"properties/chop_when=%d",
		   curstate.chop_when);
	curstate.chop_when = ve_config_get_int (cfg, buf);
	if (curstate.chop_when < 0)
		curstate.chop_when = 0;
	else if (curstate.chop_when > MAX_CHOP)
		curstate.chop_when = MAX_CHOP;

	g_snprintf(buf,256,"properties/float_prec=%d",
		   curstate.float_prec);
	curstate.float_prec = ve_config_get_int (cfg, buf);
	if (curstate.float_prec < 60)
		curstate.float_prec = 60;
	else if (curstate.float_prec > 16384)
		curstate.float_prec = 16384;

	g_snprintf(buf,256,"properties/precision_remember=%s",
		   genius_setup.precision_remember?"true":"false");
	genius_setup.precision_remember = ve_config_get_bool (cfg, buf);

	g_snprintf (buf, 256, "properties/editor_color_scheme=%s",
		    ve_sure_string (genius_setup.editor_color_scheme));
	genius_setup.editor_color_scheme = ve_config_get_string (cfg, buf);

	ve_config_destroy (cfg);

	if (old_props) {
		set_properties ();
	}
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

void
gel_set_state (GelCalcState state)
{
	curstate = state;

	if (state.full_expressions ||
	    state.output_style == GEL_OUTPUT_LATEX ||
	    state.output_style == GEL_OUTPUT_MATHML ||
	    state.output_style == GEL_OUTPUT_TROFF)
		gel_output_set_length_limit (gel_main_out, FALSE);
	else
		gel_output_set_length_limit (gel_main_out, TRUE);
}

static void
check_events (void)
{
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static gboolean
finished_exec_idle (gpointer data)
{
	gel_plot_canvas_thaw_completely ();
	return FALSE;
}

static void
finished_toplevel_exec (void)
{
	g_idle_add (finished_exec_idle, NULL);
}

static void
tree_limit_hit (void)
{
	if (genius_ask_question (NULL,
				 _("Memory (node number) limit has been reached, interrupt the computation?"))) {
		gel_interrupted = TRUE;
		/*genius_interrupt_calc ();*/
	}
}

static int
catch_interrupts (GtkWidget *w, GdkEvent *e)
{
	if (e->type == GDK_KEY_PRESS &&
#ifndef GDK_KEY_c
	    e->key.keyval == GDK_c &&
#else
	    e->key.keyval == GDK_KEY_c &&
#endif
	    e->key.state & GDK_CONTROL_MASK) {
		genius_interrupt_calc ();
		return TRUE;
	}
	return FALSE;
}

static void
open_example_cb (GtkWidget *w, GelExample * exam)
{
	new_program (exam->file, TRUE);
}

static void
open_plugin_cb (GtkWidget *w, GelPlugin * plug)
{
	gel_open_plugin (plug);
}

static void
fork_done (VteTerminal *terminal, GPid pid, GError *error, gpointer data)
{
	helper_pid = (pid_t) pid;
}

static void
my_fork_command (VteTerminal *terminal, char **argv)
{
	vte_terminal_spawn_async (terminal,
	                          VTE_PTY_DEFAULT | VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP | VTE_PTY_NO_HELPER,
	                          NULL,
	                          argv,
	                          NULL,
	                          //G_SPAWN_CHILD_INHERITS_STDIN | G_SPAWN_SEARCH_PATH,
	                          G_SPAWN_SEARCH_PATH,
	                          NULL,
	                          NULL,
	                          NULL,
	                          -1,
	                          NULL,
	                          fork_done,
	                          NULL);
}

static void
fork_a_helper (void)
{
	char *argv[6];
	char *foo;
	char *dir;
	char *libexecdir;
	char *file;

	if (genius_do_not_use_binreloc)
		libexecdir = g_strdup (LIBEXECDIR);
	else
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
					  _("Can't execute genius-readline-helper-fifo!\n"),
					  genius_window /* parent */);

		gtk_dialog_run (GTK_DIALOG (d));

		unlink (fromrlfifo);
		unlink (torlfifo);

		exit (1);
	}

	argv[0] = foo;

	argv[1] = torlfifo;
	argv[2] = fromrlfifo;

	argv[3] = NULL;

	my_fork_command (VTE_TERMINAL (term), argv);

	g_free (libexecdir);
	g_free (foo);
}

static void
genius_got_etree (GelETree *e)
{
	if (e != NULL) {
		gel_calc_running ++;
		check_events();
		gel_evalexp_parsed (e, gel_main_out, "= \e[1;36m", TRUE);
		gel_test_max_nodes_again ();
		gel_calc_running --;
		gel_output_full_string (gel_main_out, "\e[0m");
		gel_output_flush (gel_main_out);
	}

	gel_printout_infos ();

	if (gel_got_eof) {
		gel_output_full_string (gel_main_out, "\n");
		gel_output_flush (gel_main_out);
		gel_got_eof = FALSE;
		g_application_quit (G_APPLICATION (genius_app));
	}
}


static gboolean
get_new_line (GIOChannel *source, GIOCondition condition, gpointer data)
{
	int fd = g_io_channel_unix_get_fd (source);
	int r;
	char buf[5] = "EOF!";

	/* make sure the GUI responds */
	check_events ();
	
	if (condition & G_IO_HUP) {
		char *str;
		str = g_strdup_printf ("\r\n\e[01;31m%s\e[0m\r\n", 
				       _("Readline helper died, weird.  Trying to recover, things may be odd."));
		vte_terminal_feed (VTE_TERMINAL (term), str, -1);
		g_free (str);
		close (fromrl);
		fromrl = -1;
		fclose (torlfp);
		torlfp = NULL;

		fork_helper_setup_comm ();
		start_cb_p_expression (genius_got_etree, torlfp);
		return FALSE;
	}

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
fork_helper_setup_comm (void)
{
	GIOChannel *channel;

	fork_a_helper ();

	torlfp = fopen (torlfifo, "w");

	fromrl = open (fromrlfifo, O_RDONLY);
	g_assert (fromrl >= 0);

	channel = g_io_channel_unix_new (fromrl);
	g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, G_IO_IN | G_IO_HUP | G_IO_ERR, 
			     get_new_line, NULL, NULL);
	g_io_channel_unref (channel);
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
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (genius_notebook));
	if (page == 0) {
		gboolean can_copy =
			vte_terminal_get_has_selection (VTE_TERMINAL (term));
		enable_menuitem ("app.copy", can_copy);
	}
}

static void
/* switch_page (GtkNotebook *notebook, GtkNotebookPage *page, guint page_num) */
/* GTK3: switch_page (GtkNotebook *notebook, GtkWidget *page, guint page_num) */
switch_page (GtkNotebook *notebook, gpointer page, guint page_num)
{
	Program *p;

	p = g_object_get_data (G_OBJECT (page), "program");

	if (p == NULL) {
		/* console */
		enable_menuitem ("app.close", FALSE);
		if (selected_program == NULL) {
		        enable_menuitem ("app.run", FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (run_tb_button), FALSE);
		        enable_menuitem ("app.reload", FALSE);
		        enable_menuitem ("app.save", FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (save_tb_button), FALSE);
		        enable_menuitem ("app.save-as", FALSE);
		}
		/* selection changed updates the copy item sensitivity */
		selection_changed ();
		enable_menuitem ("app.cut", FALSE);
#ifdef HAVE_GTKSOURCEVIEW
		setup_undo_redo ();
#endif
		gtk_statusbar_pop (GTK_STATUSBAR (genius_window_statusbar),
				   0 /* context */);
	} else {
		char *s;

		/* something else */
		enable_menuitem ("app.cut", TRUE);
		enable_menuitem ("app.copy", TRUE);
		enable_menuitem ("app.close", TRUE);
		enable_menuitem ("app.run", TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (run_tb_button), TRUE);
		enable_menuitem ("app.save-as", TRUE);

		if (selected_program != NULL) {
			selected_program->selected = FALSE;
			setup_label (selected_program);
		}

		selected_program = p;
		selected_program->selected = TRUE;

		setup_label (selected_program);

		enable_menuitem ("app.reload", selected_program->real_file);
		enable_menuitem ("app.save", selected_program->real_file);
		gtk_widget_set_sensitive (GTK_WIDGET (save_tb_button),
					  selected_program->real_file);

		gtk_statusbar_pop (GTK_STATUSBAR (genius_window_statusbar),
				   0 /* context */);
		s = g_strdup_printf (_("Line: %d"),
				     selected_program->curline + 1);
		gtk_statusbar_push (GTK_STATUSBAR (genius_window_statusbar),
				    0 /* context */, s);
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

static void
loadup_files_from_cmdline (GApplication *app, GFile **files, gint n_files,
                           gchar *hint, gpointer data)
{
	int i;

	g_application_activate (app);

	for (i = 0; i < n_files; i++) {
		char *uri;
		
		uri = g_file_get_uri (files[i]);

		new_program (uri, FALSE);
		
		g_free (uri);
	}
}

static void 
drag_data_received (GtkWidget *widget, GdkDragContext *context, 
		    gint x, gint y, GtkSelectionData *selection_data, 
		    guint info, guint time)
{
	char *uri;
	char **uris;
	int i = 0;

	if (info != TARGET_URI_LIST)
		return;
			
	uris = gtk_selection_data_get_uris (selection_data);

	for (uri = uris[i]; uri != NULL; i++, uri = uris[i]) {
		new_program (uri, FALSE);
	}
	g_strfreev (uris);
}

static void
update_term_geometry (void)
{
	GdkGeometry hints;
	int char_width;
	int char_height;
	GtkBorder border;
	GtkStyleContext *ctxt;

	char_width = vte_terminal_get_char_width (VTE_TERMINAL (term));
	char_height = vte_terminal_get_char_height (VTE_TERMINAL (term));

	ctxt = gtk_widget_get_style_context (term);
	gtk_style_context_get_padding (ctxt,
	                               gtk_style_context_get_state (ctxt),
	                               &border);

	hints.base_width = border.left + border.right;
	hints.base_height = border.top + border.bottom;

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

static void
activate (GApplication *app, gpointer data)
{
	GtkWidget *hbox;
	GtkWidget *w;
	char *file;
	int plugin_count = 0;
	int example_count = 0;
	gboolean give_no_lib_error_after_init = FALSE;

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

	genius_datadir = gbr_find_data_dir (DATADIR);
	/* Test the datadir */
	file = g_build_filename (genius_datadir,
				 "genius", "gel", "lib.cgel", NULL);
	if (access (file, F_OK) != 0) {
		g_free (file);
		g_free (genius_datadir);
		genius_datadir = g_strdup (DATADIR);
		
		/* Do not use binreloc anymore */
		genius_do_not_use_binreloc = TRUE;

		file = g_build_filename (genius_datadir,
					 "genius", "gel", "lib.cgel", NULL);
		if (access (file, F_OK) != 0) {
			give_no_lib_error_after_init = TRUE;
		}
	}
	g_free (file);

	genius_datadir_sourceview = g_build_filename (genius_datadir, "genius",
						      "gtksourceview"
						      G_DIR_SEPARATOR_S,
						      NULL);

	if (give_no_lib_error_after_init) {
		genius_display_error (NULL /* parent */,
				      _("Cannot find the library file, genius installation may be incorrect"));
	}

	/* ensure the directory, if it is a file, no worries not saving the properties is not fatal */
	file = g_build_filename (g_get_home_dir (), ".genius", NULL);
	if (access (file, F_OK) != 0) {
		mkdir (file, 0755);
	} else {
		DIR *dir;

		dir = opendir (file);
		if (dir == NULL) {
			genius_display_error (NULL /* parent */,
					      _("A file .genius in the home directory exists, "
						"but it should be a directory. "
						"Genius will not be able to save preferences."));
		} else {
			closedir (dir);
		}
	}
	g_free (file);


	/*read parameters */
	get_properties ();

	gel_main_out = gel_output_new();
	gel_output_setup_string (gel_main_out, 80, get_term_width);
	gel_output_set_notify (gel_main_out, output_notify_func);
	
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

	recent_manager = gtk_recent_manager_get_default ();
	
	/* create our notebook and setup toplevel window */
	genius_notebook = gtk_notebook_new ();
	/* g_object_set (G_OBJECT (genius_notebook), "tab-vborder", 0, NULL);*/
	gtk_container_set_border_width (GTK_CONTAINER (genius_notebook), 5);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (genius_notebook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (genius_notebook));

        /*set up the top level window*/
	create_main_window (genius_notebook, app);

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

	/* setup the notebook */
	g_signal_connect (G_OBJECT (genius_notebook), "switch_page",
			  G_CALLBACK (switch_page), NULL);

	/*the main box to put everything in*/
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	term = vte_terminal_new ();
	vte_terminal_set_scrollback_lines (VTE_TERMINAL (term),
					   genius_setup.scrollback);
	vte_terminal_set_audible_bell (VTE_TERMINAL (term), TRUE);
	vte_terminal_set_scroll_on_keystroke (VTE_TERMINAL (term), TRUE);
	vte_terminal_set_scroll_on_output (VTE_TERMINAL (term), FALSE);
	vte_terminal_set_word_char_exceptions (VTE_TERMINAL (term),
	                                       "-/_:.,?+%=");
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
	
	w = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL,
	                       gtk_scrollable_get_vadjustment
	                       (GTK_SCROLLABLE (term)));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	
	/*set up the main window*/
	gtk_notebook_append_page (GTK_NOTEBOOK (genius_notebook),
				  hbox,
				  gtk_label_new (_("Console")));
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (genius_notebook),
					  hbox,
					  TRUE);
	/* FIXME:
	gtk_widget_queue_resize (vte);
	*/

	gtk_widget_show_all (genius_window);

	/* Try to deduce the standard font size, kind of evil, but sorta
	 * works.  The user can always set the font themselves. */
	{
		GtkStyleContext *ctxt;
		PangoFontDescription *desc;
		gint sz;

		ctxt = gtk_widget_get_style_context (genius_window);
		gtk_style_context_get (ctxt, GTK_STATE_FLAG_NORMAL,
		                       GTK_STYLE_PROPERTY_FONT, &desc, NULL);
		sz = pango_font_description_get_size (desc) / PANGO_SCALE;
		pango_font_description_free (desc);
		if (sz == 0) sz = 10;
		default_console_font = g_strdup_printf ("Monospace %d", sz);
		desc = pango_font_description_from_string
		        (ve_string_empty (genius_setup.font)
		         ? default_console_font
		         : genius_setup.font);
		vte_terminal_set_font (VTE_TERMINAL (term), desc);
		pango_font_description_free (desc);
	}

	setup_term_color ();
	vte_terminal_set_cursor_blink_mode
		(VTE_TERMINAL (term),
		 genius_setup.blinking_cursor ?
		 VTE_CURSOR_BLINK_SYSTEM :
		 VTE_CURSOR_BLINK_OFF);

	update_term_geometry ();
	g_signal_connect (G_OBJECT (term), "char-size-changed",
			  G_CALLBACK (update_term_geometry), NULL);

	gtk_widget_hide (plugin_menu);
	gtk_widget_hide (example_menu);

	/* Show the window now before going on with the
	 * setup */
	gtk_widget_show_now (genius_window);

	gel_output_printf (gel_main_out,
			   _("%sGenius %s%s\n"
			     "%s\n"
			     "This is free software with ABSOLUTELY NO WARRANTY.\n"
			     "For license details type `%swarranty%s'.\n"
			     "For help type `%smanual%s' or `%shelp%s'.%s\n\n"),
			   "\e[0;32m" /* green */,
			   "\e[0m" /* white on black */,
			   VERSION,
			   _(GENIUS_COPYRIGHT_STRING),
			   "\e[01;36m" /* cyan */,
			   "\e[0m" /* white on black */,
			   "\e[01;36m" /* cyan */,
			   "\e[0m" /* white on black */,
			   "\e[01;36m" /* cyan */,
			   "\e[0m" /* white on black */,
			   get_version_details ());
	gel_output_flush (gel_main_out);
	check_events ();

	gel_set_new_calcstate (curstate);
	gel_set_new_errorout (geniuserror);
	gel_set_new_infoout (geniusinfo);

	/* Read plugins */
	gel_read_plugin_list ();
	if (gel_plugin_list != NULL) {
		GSList *li;
		int i;
		GtkWidget *menu;

		menu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (plugin_menu), menu);

		for (i = 0, li = gel_plugin_list;
		     li != NULL;
		     li = li->next, i++) {
			GtkWidget *item;
			GelPlugin *plug = li->data;
			if (plug->hide)
				continue;

			item = gtk_menu_item_new_with_label (plug->name);
			g_signal_connect (item, "select",
					  G_CALLBACK (simple_menu_item_select_cb), 
					  plug->description);
			g_signal_connect (item, "deselect",
					  G_CALLBACK (simple_menu_item_deselect_cb),
					  NULL);
			gtk_widget_show (item);
			g_signal_connect (G_OBJECT (item), "activate",
					  G_CALLBACK (open_plugin_cb), plug);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			plugin_count ++;
		}
	}
	/* if no plugins, hide the menu */
	if (plugin_count == 0) {
		gtk_widget_hide (plugin_menu);
	} else {
		gtk_widget_show (plugin_menu);
	}


	/* Setup the helper */
	setup_rl_fifos ();
	fork_helper_setup_comm ();

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
	if (file != NULL) {
		gel_load_file (NULL, file, FALSE);
		g_free (file);
	}

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

	/* check events so that we setup the examples menu only
	 * once everything is shown */
	check_events();

	/* Read examples now */
	gel_read_example_list ();
	if (gel_example_list != NULL) {
		GSList *li, *l;
		GtkWidget *menu;

		menu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (example_menu), menu);

		for (li = gel_example_categories_list;
		     li != NULL;
		     li = li->next) {
			GtkWidget *submenu;
			GtkWidget *item;
			GelExampleCategory *cat = li->data;

			item = gtk_menu_item_new_with_label (cat->name);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			submenu = gtk_menu_new ();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
						   submenu);
			gtk_widget_show (GTK_WIDGET (item));
			gtk_widget_show (GTK_WIDGET (submenu));

			for (l = cat->examples; l != NULL; l = l->next) {
				GelExample *exam = l->data;

				item = gtk_menu_item_new_with_label (exam->name);

				g_signal_connect (item, "select",
						  G_CALLBACK (simple_menu_item_select_cb), 
						  exam->name);
				g_signal_connect (item, "deselect",
						  G_CALLBACK (simple_menu_item_deselect_cb), 
						  NULL);
				gtk_widget_show (item);
				g_signal_connect (G_OBJECT (item), "activate",
						  G_CALLBACK (open_example_cb), exam);
				gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
				example_count ++;
			}
		}
	}
	/* if no exampleials, hide the menu */
	if (example_count == 0) {
		gtk_widget_hide (example_menu);
	} else {
		gtk_widget_show (example_menu);
	}
}

static void
startup (GApplication *app, gpointer data)
{
	g_set_application_name (_("GNOME Genius"));
}

int
main (int argc, char *argv[])
{
	int status;
	char *name;

	arg0 = g_strdup (argv[0]);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#ifdef HAVE_GTKSOURCEVIEW
	gtk_source_init ();
#endif

	/* I sort of doubt we can do the uniqueness thing sanely right now,
	 * there is too much global stuff happening to sanely run two
	 * windows from one process */
	genius_app = gtk_application_new ("org.gnome.genius",
					  G_APPLICATION_HANDLES_OPEN |
					  G_APPLICATION_NON_UNIQUE);
	g_signal_connect (genius_app, "startup", G_CALLBACK (startup), NULL);
	g_signal_connect (genius_app, "activate", G_CALLBACK (activate), NULL);
	g_signal_connect (genius_app, "open",
	                  G_CALLBACK (loadup_files_from_cmdline), NULL);
	status = g_application_run (G_APPLICATION (genius_app), argc, argv);

#ifdef HAVE_GTKSOURCEVIEW
	gtk_source_finalize ();
#endif
	g_object_unref (genius_app);
	genius_app = NULL;

	/* if we actually started up */
	if (genius_datadir != NULL) {
		/*
		 * Save properties and plugins
		 */
		set_properties ();
		gel_save_plugins ();

		if (fromrl >= 0)
			close (fromrl);
		if (torlfp != NULL)
			fclose (torlfp);

		if (fromrlfifo != NULL)
			unlink (fromrlfifo);
		if (torlfifo != NULL)
			unlink (torlfifo);

		/* remove old preferences to avoid confusion, by now things should be set in the new file */
		name = g_build_filename (g_get_home_dir (), ".gnome2", "genius", NULL);
		if (access (name, F_OK) == 0)
			unlink (name);
		g_free (name);
	}

	return status;
}
