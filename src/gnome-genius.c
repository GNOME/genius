/* GENIUS Calculator
 * Copyright (C) 1997-2003 George Lebl
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

#include "config.h"

#include <gnome.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
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

#include <readline/readline.h>
#include <readline/history.h>

/* FIXME: need header */
void genius_interrupt_calc (void);

/*Globals:*/

#define DEFAULT_FONT "Monospace 10"

/*calculator state*/
calcstate_t curstate={
	256,
	12,
	FALSE,
	FALSE,
	FALSE,
	5,
	TRUE,
	10
	};
	
extern int got_eof;
extern int parenth_depth;
extern int interrupted;

GtkWidget *genius_window = NULL;

static GtkWidget *setupdialog = NULL;
static GtkWidget *term = NULL;
static GtkWidget *appbar = NULL;
static GtkWidget *notebook = NULL;
static GString *errors=NULL;
static GString *infos=NULL;

static char *clipboard_str = NULL;

static int calc_running = 0;

static int errors_printed = 0;

static char *last_dir = NULL;

typedef struct {
	gboolean error_box;
	gboolean info_box;
	int scrollback;
	char *font;
} geniussetup_t;

geniussetup_t cursetup = {
	FALSE,
	TRUE,
	1000,
	NULL
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

static Program *selected_program = NULL;
static Program *running_program = NULL;

pid_t helper_pid = -1;

static FILE *torlfp = NULL;
static int fromrl;

static int forzvt[2];

static char *torlfifo = NULL;
static char *fromrlfifo = NULL;

static char *arg0 = NULL;

static void feed_to_zvt (gpointer data, gint source,
			 GdkInputCondition condition);
static void new_callback (GtkWidget *menu_item, gpointer data);
static void open_callback (GtkWidget *w);
static void save_callback (GtkWidget *w);
static void save_as_callback (GtkWidget *w);
static void close_callback (GtkWidget *menu_item, gpointer data);
static void load_cb (GtkWidget *w);
static void reload_cb (GtkWidget *w);
static void quitapp (GtkWidget * widget, gpointer data);
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
static void setup_calc (GtkWidget *widget, gpointer data);
static void run_program (GtkWidget *menu_item, gpointer data);
static void manual_call (GtkWidget *widget, gpointer data);
static void warranty_call (GtkWidget *widget, gpointer data);
static void aboutcb (GtkWidget * widget, gpointer data);

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_NEW_ITEM(N_("_New Program"), N_("Create new program tab"), new_callback, NULL),
	GNOMEUIINFO_MENU_OPEN_ITEM (open_callback,NULL),
#define FILE_SAVE_ITEM 2
	GNOMEUIINFO_MENU_SAVE_ITEM (save_callback,NULL),
#define FILE_SAVE_AS_ITEM 3
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (save_as_callback,NULL),
#define FILE_RELOAD_ITEM 4
	GNOMEUIINFO_ITEM_STOCK(N_("_Reload From Disk"),N_("Reload the selected program from disk"), reload_cb, GNOME_STOCK_MENU_REVERT),
#define FILE_CLOSE_ITEM 5
	GNOMEUIINFO_MENU_CLOSE_ITEM (close_callback, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK(N_("_Load and Run"),N_("Load and execute a file in genius"), load_cb, GNOME_STOCK_MENU_OPEN),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (quitapp,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo edit_menu[] = {  
#define EDIT_CUT_ITEM 0
	GNOMEUIINFO_MENU_CUT_ITEM(cut_callback,NULL),
#define EDIT_COPY_ITEM 1
	GNOMEUIINFO_MENU_COPY_ITEM(copy_callback,NULL),
	GNOMEUIINFO_MENU_PASTE_ITEM(paste_callback,NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As Plain _Text"),
			       N_("Copy last answer into the clipboard in plain text"),
			       copy_as_plain,
			       GNOME_STOCK_MENU_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As _LaTeX"),
			       N_("Copy last answer into the clipboard as LaTeX"),
			       copy_as_latex,
			       GNOME_STOCK_MENU_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As _MathML"),
			       N_("Copy last answer into the clipboard as MathML"),
			       copy_as_mathml,
			       GNOME_STOCK_MENU_COPY),
	GNOMEUIINFO_ITEM_STOCK(N_("Copy Answer As T_roff"),
			       N_("Copy last answer into the clipboard as Troff eqn"),
			       copy_as_troff,
			       GNOME_STOCK_MENU_COPY),
	GNOMEUIINFO_END,
};

static GnomeUIInfo settings_menu[] = {  
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(setup_calc,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo calc_menu[] = {  
#define CALC_RUN_ITEM 0
	GNOMEUIINFO_ITEM_STOCK(N_("_Run"),N_("Run current program"),run_program, GTK_STOCK_EXECUTE),
	GNOMEUIINFO_ITEM_STOCK(N_("_Interrupt"),N_("Interrupt current calculation"),genius_interrupt_calc,GNOME_STOCK_MENU_STOP),
	GNOMEUIINFO_END,
};

static GnomeUIInfo help_menu[] = {  
	/* FIXME: no help 
	 * GNOMEUIINFO_HELP("genius"),*/
	GNOMEUIINFO_ITEM_STOCK (N_("_Manual"),
				N_("Display the manual"),
				manual_call,
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
	GNOMEUIINFO_ITEM_STOCK(N_("Interrupt"),N_("Interrupt current calculation"),genius_interrupt_calc,GNOME_STOCK_PIXMAP_STOP),
#define TOOLBAR_RUN_ITEM 1
	GNOMEUIINFO_ITEM_STOCK(N_("Run"),N_("Run current program"),run_program, GTK_STOCK_EXECUTE),
	GNOMEUIINFO_ITEM_STOCK(N_("Open"),N_("Open a GEL file for running"), open_callback, GNOME_STOCK_PIXMAP_OPEN),
	GNOMEUIINFO_ITEM_STOCK(N_("Exit"),N_("Exit genius"), quitapp, GNOME_STOCK_PIXMAP_EXIT),
	GNOMEUIINFO_END,
};


#define ELEMENTS(x) (sizeof (x) / sizeof (x [0]))

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


/*display a message in a messagebox*/
static GtkWidget *
geniusbox (gboolean error,
	   gboolean bind_response,
	   const char *s)
{
	GtkWidget *mb;
	/* if less then 10 lines */
	if (count_char (s, '\n') <= 10) {
		GtkMessageType type = GTK_MESSAGE_INFO;
		if (error)
			type = GTK_MESSAGE_ERROR;
		mb = gtk_message_dialog_new (GTK_WINDOW (genius_window) /* parent */,
					     0 /* flags */,
					     type,
					     GTK_BUTTONS_OK,
					     "%s",
					     s);
	} else {
		GtkWidget *sw;
		GtkWidget *tv;
		GtkTextBuffer *buffer;
		GtkTextIter iter;

		mb = gtk_dialog_new_with_buttons
			(error?_("Error"):_("Information"),
			 GTK_WINDOW (genius_window) /* parent */,
			 0 /* flags */,
			 GTK_STOCK_OK, GTK_RESPONSE_OK,
			 NULL);
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mb)->vbox),
				    sw,
				    TRUE, TRUE, 0);

		tv = gtk_text_view_new ();
		gtk_text_view_set_editable (GTK_TEXT_VIEW (tv), FALSE);
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
		gtk_text_buffer_create_tag (buffer, "foo",
					    "editable", FALSE,
					    "family", "monospace",
					    NULL);

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, s, -1, "foo", NULL);

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
printout_error_num_and_reset(void)
{
	if(cursetup.error_box) {
		if(errors) {
			if(errors_printed-curstate.max_errors > 0) {
				g_string_sprintfa(errors,
						  _("\nToo many errors! (%d followed)"),
						  errors_printed-curstate.max_errors);
			}
			geniusbox (TRUE, TRUE, errors->str);
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
	if(file)
		str = g_strdup_printf("%s:%d: %s",file,line,s);
	else if(line>0)
		str = g_strdup_printf("line %d: %s",line,s);
	else
		str = g_strdup(s);
	
	if(cursetup.error_box) {
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
		geniusbox (FALSE, TRUE, infos->str);
		g_string_free (infos, TRUE);
		infos = NULL;
	}

	printout_error_num_and_reset ();
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
	
	if(cursetup.info_box) {
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
	static GtkWidget *about;
	static const char *authors[] = {
		"George Lebl (jirka@5z.com)",
		NULL
	};
	static const char *documenters[] = {
		"George Lebl (jirka@5z.com)",
		NULL
	};
	const char *translators;

	if (about == NULL) {
		/* Translators should localize the following string
		 * which will give them credit in the About box.
		 * E.g. "Fulano de Tal <fulano@detal.com>"
		 */
		translators = _("translator_credits-PLEASE_ADD_YOURSELF_HERE");

		about = gnome_about_new
			(_("GENIUS Calculator"),
			 VERSION,
			 COPYRIGHT_STRING,
			 _("The Gnome calculator style edition of "
			   "the genius calculator.  For license/warranty "
			   "details, type 'warranty' into the console."),
			 authors,
			 documenters,
			(strcmp (translators, "translator_credits-PLEASE_ADD_YOURSELF_HERE")
			 ? translators : NULL),
			NULL);

		gtk_window_set_transient_for (GTK_WINDOW (about),
					      GTK_WINDOW (genius_window));

		g_signal_connect (about, "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  &about);
	}

	gtk_widget_show_now (about);
	gtk_window_present (GTK_WINDOW (about));
}

static void
set_properties (void)
{
	gnome_config_set_string("/genius/properties/pango_font", cursetup.font?
				cursetup.font:DEFAULT_FONT);
	gnome_config_set_int("/genius/properties/scrollback", cursetup.scrollback);
	gnome_config_set_bool("/genius/properties/error_box", cursetup.error_box);
	gnome_config_set_bool("/genius/properties/info_box", cursetup.info_box);
	gnome_config_set_int("/genius/properties/max_digits", 
			      curstate.max_digits);
	gnome_config_set_bool("/genius/properties/results_as_floats",
			      curstate.results_as_floats);
	gnome_config_set_bool("/genius/properties/scientific_notation",
			      curstate.scientific_notation);
	gnome_config_set_bool("/genius/properties/full_expressions",
			      curstate.full_expressions);
	gnome_config_set_int("/genius/properties/max_errors",
			     curstate.max_errors);
	
	gnome_config_sync();
}

static void
display_error (GtkWidget *parent, const char *err)
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

static gboolean
ask_question (GtkWidget *parent, const char *question)
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
			if ( ! ask_question (NULL,
					     _("Genius is executing something, "
					       "and furthermore there are "
					       "unsaved programs.\nAre "
					       "you sure you wish to quit?")))
				return;
			interrupted = TRUE;
		} else {
			if ( ! ask_question (NULL,
					     _("There are unsaved programs, "
					       "are you sure you wish to quit?")))
				return;
		}
	} else {
		if (calc_running) {
			if ( ! ask_question (NULL,
					     _("Genius is executing something, "
					       "are you sure you wish to "
					       "quit?")))
				return;
			interrupted = TRUE;
		} else {
			if ( ! ask_question (NULL,
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
	if(setupdialog)
		gnome_property_box_changed(GNOME_PROPERTY_BOX(setupdialog));
}

/*option callback*/
static void
optioncb(GtkWidget * widget, int *data)
{
	if(GTK_TOGGLE_BUTTON(widget)->active)
		*data=TRUE;
	else
		*data=FALSE;
	
	if(setupdialog)
		gnome_property_box_changed(GNOME_PROPERTY_BOX(setupdialog));
}

static void
fontsetcb(GnomeFontPicker *gfp, gchar *font_name, char **font)
{
	g_free(*font);
	*font = g_strdup(font_name);
	if(setupdialog)
		gnome_property_box_changed(GNOME_PROPERTY_BOX(setupdialog));
}


static calcstate_t tmpstate={0};
static geniussetup_t tmpsetup={0};

static void
do_setup(GtkWidget *widget, gint page, gpointer data)
{
	if (page == -1) {     /* Just finished global apply */
		g_free(cursetup.font);
		cursetup = tmpsetup;
		if(tmpsetup.font)
			cursetup.font = g_strdup(tmpsetup.font);
		curstate = tmpstate;

		set_new_calcstate(curstate);
		vte_terminal_set_scrollback_lines (VTE_TERMINAL (term),
						   cursetup.scrollback);
		vte_terminal_set_font_from_string (VTE_TERMINAL (term),
						   cursetup.font ?
						   cursetup.font : DEFAULT_FONT);
	}
}

static void
destroy_setup(GtkWidget *widget, gpointer data)
{
	setupdialog = NULL;
}

static void
setup_calc(GtkWidget *widget, gpointer data)
{
	GtkWidget *mainbox,*frame;
	GtkWidget *box;
	GtkWidget *b, *w;
	GtkAdjustment *adj;

	if (setupdialog) {
		gtk_widget_show_now(GTK_WIDGET(setupdialog));
		gdk_window_raise(GTK_WIDGET(setupdialog)->window);
		return;
	}
	
	tmpstate = curstate;
	g_free(tmpsetup.font);
	tmpsetup = cursetup;
	if(cursetup.font)
		tmpsetup.font = g_strdup(cursetup.font);
	
	setupdialog = gnome_property_box_new();
	gtk_window_set_transient_for(GTK_WINDOW(setupdialog),
				     GTK_WINDOW(genius_window));
	
	gtk_window_set_title(GTK_WINDOW(setupdialog),
			     _("GENIUS Calculator setup"));
	
	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gnome_property_box_append_page(GNOME_PROPERTY_BOX(setupdialog),
				       mainbox,
				       gtk_label_new(_("Output")));

	
	frame=gtk_frame_new(_("Number/Expression output options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(box),GNOME_PAD);
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
	gtk_widget_set_usize(w,80,0);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.max_digits);


	w=gtk_check_button_new_with_label(_("Results as floats"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpstate.results_as_floats);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.results_as_floats);
	
	w=gtk_check_button_new_with_label(_("Floats in scientific notation"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpstate.scientific_notation);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.scientific_notation);

	w=gtk_check_button_new_with_label(_("Always print full expressions"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpstate.full_expressions);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpstate.full_expressions);


	frame=gtk_frame_new(_("Error/Info output options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(frame),box);

	gtk_container_border_width(GTK_CONTAINER(box),GNOME_PAD);
	

	w=gtk_check_button_new_with_label(_("Display errors in a dialog"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpsetup.error_box);
	g_signal_connect (G_OBJECT(w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&tmpsetup.error_box);

	w=gtk_check_button_new_with_label(_("Display information messages in a dialog"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
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
	gtk_widget_set_usize(w,80,0);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb),&tmpstate.max_errors);


	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gnome_property_box_append_page(GNOME_PROPERTY_BOX(setupdialog),
				       mainbox,
				       gtk_label_new(_("Precision")));

	
	frame=gtk_frame_new(_("Floating point precision"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(box),GNOME_PAD);
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
	gtk_widget_set_usize(w,80,0);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpstate.float_prec);


	mainbox = gtk_vbox_new(FALSE, GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(mainbox),GNOME_PAD);
	gnome_property_box_append_page(GNOME_PROPERTY_BOX(setupdialog),
				       mainbox,
				       gtk_label_new(_("Terminal")));

	
	frame=gtk_frame_new(_("Terminal options"));
	gtk_box_pack_start(GTK_BOX(mainbox),frame,FALSE,FALSE,0);
	box=gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_border_width(GTK_CONTAINER(box),GNOME_PAD);
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
	gtk_widget_set_usize(w,80,0);
	gtk_box_pack_start(GTK_BOX(b),w,FALSE,FALSE,0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (intspincb), &tmpsetup.scrollback);
	
	
	b=gtk_hbox_new(FALSE,GNOME_PAD);
	gtk_box_pack_start(GTK_BOX(box),b,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(b),
		   gtk_label_new(_("Font:")),
		   FALSE,FALSE,0);
	
        w = gnome_font_picker_new();
	gnome_font_picker_set_font_name (GNOME_FONT_PICKER (w),
					 tmpsetup.font ? tmpsetup.font :
					 DEFAULT_FONT);
        gnome_font_picker_set_mode (GNOME_FONT_PICKER (w),
				    GNOME_FONT_PICKER_MODE_FONT_INFO);
        gtk_box_pack_start(GTK_BOX(b),w,TRUE,TRUE,0);
        g_signal_connect (G_OBJECT (w), "font_set",
			  G_CALLBACK (fontsetcb),
			  &tmpsetup.font);


	g_signal_connect (G_OBJECT (setupdialog), "apply",
			  G_CALLBACK (do_setup), NULL);	
	g_signal_connect (G_OBJECT (setupdialog), "destroy",
			  G_CALLBACK (destroy_setup), NULL);
	gtk_widget_show_all(setupdialog);
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
manual_call (GtkWidget *widget, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* perhaps a bit ugly */
		gboolean last = cursetup.info_box;
		cursetup.info_box = TRUE;
		gel_evalexp ("manual", NULL, main_out, NULL, TRUE, NULL);
		gel_printout_infos ();
		cursetup.info_box = last;
	}
}

static void
warranty_call (GtkWidget *widget, gpointer data)
{
	if (calc_running) {
		executing_warning ();
		return;
	} else {
		/* perhaps a bit ugly */
		gboolean last = cursetup.info_box;
		cursetup.info_box = TRUE;
		gel_evalexp ("warranty", NULL, main_out, NULL, TRUE, NULL);
		gel_printout_infos ();
		cursetup.info_box = last;
	}
}

static void
setup_last_dir (const char *filename)
{
	char *s = g_path_get_dirname (filename);

	g_free (last_dir);
	if (s == NULL) {
		last_dir = NULL;
		return;
	}
	if (strcmp(s, "/") == 0) {
		last_dir = s;
		return;
	}
	last_dir = g_strconcat (s, "/", NULL);
	g_free (s);
}

static void
fs_destroy_cb(GtkWidget *w, GtkWidget **fs)
{
	*fs = NULL;
}

static void
really_load_cb (GtkWidget *w, GtkFileSelection *fs)
{
	const char *s;
	s = gtk_file_selection_get_filename (fs);
	if (s == NULL ||
	    access (s, R_OK) != 0) {
		display_error (GTK_WIDGET (fs),
			       _("Cannot open file!"));
		return;
	}

	setup_last_dir (s);

	gtk_widget_destroy (GTK_WIDGET (fs));

	gel_load_guess_file (NULL, s, TRUE);

	gel_printout_infos ();
}

static void
load_cb (GtkWidget *w)
{
	static GtkWidget *fs = NULL;
	
	if(fs) {
		gtk_widget_show_now(fs);
		gdk_window_raise(fs->window);
		return;
	}

	fs = gtk_file_selection_new(_("Load GEL file"));
	
	gtk_window_position (GTK_WINDOW (fs), GTK_WIN_POS_MOUSE);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (fs_destroy_cb), &fs);
	
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
			  "clicked", G_CALLBACK (really_load_cb),
			  fs);

	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (fs)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				   GTK_OBJECT(fs));
	if (last_dir != NULL)
		gtk_file_selection_set_filename
			(GTK_FILE_SELECTION (fs), last_dir);

	gtk_widget_show (fs);
}

void            gtk_text_buffer_cut_clipboard           (GtkTextBuffer *buffer,
							 GtkClipboard  *clipboard,
                                                         gboolean       default_editable);
void            gtk_text_buffer_copy_clipboard          (GtkTextBuffer *buffer,
							 GtkClipboard  *clipboard);
void            gtk_text_buffer_paste_clipboard         (GtkTextBuffer *buffer,
							 GtkClipboard  *clipboard,
							 GtkTextIter   *override_location,
                                                         gboolean       default_editable);

static void
cut_callback (GtkWidget *menu_item, gpointer data)
{
	int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
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
	gboolean last_info = cursetup.info_box;
	gboolean last_error = cursetup.error_box;
	cursetup.info_box = TRUE;
	cursetup.error_box = TRUE;
	gel_output_setup_string (out, 0, NULL);
	gel_evalexp ("ans", NULL, out, NULL, TRUE, NULL);
	gel_printout_infos ();
	cursetup.info_box = last_info;
	cursetup.error_box = last_error;

	g_free (clipboard_str);
	clipboard_str = gel_output_snarf_string (out);
	gel_output_unref (out);

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
prog_menu_activated (GtkWidget *item, gpointer data)
{
	GtkWidget *w = data;
	int num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), w);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), num);
}

static void
build_program_menu (void)
{
	int n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	int i;
	GtkWidget *menu;

	if (n <= 1) {
		/* No programs in the menu */
		gtk_widget_hide (genius_menu[PROGRAMS_MENU].widget);
		return;
	}

	menu = gtk_menu_new ();
	gtk_widget_show (menu);
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
	}

	gtk_menu_item_set_submenu
		(GTK_MENU_ITEM (genius_menu[PROGRAMS_MENU].widget), menu);
	gtk_widget_show (genius_menu[PROGRAMS_MENU].widget);
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

static void
reload_cb (GtkWidget *menu_item)
{
	GtkTextIter iter, iter_end;
	char *contents;
	int len;

	if (selected_program == NULL ||
	    ! selected_program->real_file)
		return;

	selected_program->ignore_changes++;

	gtk_text_buffer_get_iter_at_offset (selected_program->buffer, &iter, 0);
	gtk_text_buffer_get_iter_at_offset (selected_program->buffer,
					    &iter_end, -1);

	gtk_text_buffer_delete (selected_program->buffer,
				&iter, &iter_end);

	if (g_file_get_contents (selected_program->name,
				 &contents,
				 &len,
				 NULL /* FIXME: error */)) {
		gtk_text_buffer_get_iter_at_offset (selected_program->buffer,
						    &iter, 0);
		gtk_text_buffer_insert_with_tags_by_name
			(selected_program->buffer,
			 &iter, contents, len, "foo", NULL);
		g_free (contents);
		selected_program->changed = FALSE;
	} else {
		display_error (NULL, _("Cannot open file"));
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

static void
new_program (const char *filename)
{
	static int cnt = 1;
	GtkWidget *tv;
	GtkWidget *sw;
	GtkTextBuffer *buffer;
	Program *p;

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	tv = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));

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
	g_object_set_data (G_OBJECT (sw), "program", p);

	g_signal_connect_after (G_OBJECT (p->buffer), "mark_set",
				G_CALLBACK (move_cursor),
				p);

	if (filename == NULL) {
		/* the file name will have an underscore */
		p->name = g_strdup_printf (_("Program_%d.gel"), cnt);
		p->vname = g_strdup_printf (_("Program %d"), cnt);
		cnt++;
	} else {
		char *contents;
		int len;
		p->name = g_strdup (filename);
		if (g_file_get_contents (filename,
					 &contents,
					 &len,
					 NULL /* FIXME: error */)) {
			GtkTextIter iter;
			gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, contents, len, "foo", NULL);
			g_free (contents);
		} else {
			display_error (NULL, _("Cannot open file"));
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
}

static void
new_callback (GtkWidget *menu_item, gpointer data)
{
	new_program (NULL);
}

static void
really_open_cb (GtkWidget *w, GtkFileSelection *fs)
{
	const char *s;
	s = gtk_file_selection_get_filename (fs);
	if (s == NULL ||
	    access (s, R_OK) != 0) {
		display_error (GTK_WIDGET (fs),
			       _("Cannot open file!"));
		return;
	}

	setup_last_dir (s);

	new_program (s);

	gtk_widget_destroy (GTK_WIDGET (fs));
}

static void
open_callback (GtkWidget *w)
{
	static GtkWidget *fs = NULL;
	
	if(fs) {
		gtk_widget_show_now(fs);
		gdk_window_raise(fs->window);
		return;
	}

	fs = gtk_file_selection_new(_("Open GEL file"));
	
	gtk_window_position (GTK_WINDOW (fs), GTK_WIN_POS_MOUSE);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (fs_destroy_cb), &fs);
	
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
			  "clicked", G_CALLBACK (really_open_cb),
			  fs);

	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (fs)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				   GTK_OBJECT(fs));
	if (last_dir != NULL)
		gtk_file_selection_set_filename
			(GTK_FILE_SELECTION (fs), last_dir);

	gtk_widget_show (fs);
}

static gboolean
save_program (Program *p, const char *new_fname)
{
	FILE *fp;
	GtkTextIter iter, iter_end;
	char *prog;
	const char *fname;
	int sz;
	int wrote;

	if (new_fname == NULL)
		fname = p->name;
	else
		fname = new_fname;

	fp = fopen (fname, "w");
	if (fp == NULL)
		return FALSE;

	gtk_text_buffer_get_iter_at_offset (p->buffer, &iter, 0);
	gtk_text_buffer_get_iter_at_offset (p->buffer, &iter_end, -1);
	prog = gtk_text_buffer_get_text (p->buffer, &iter, &iter_end,
					 FALSE /* include_hidden_chars */);
	sz = strlen (prog);

	wrote = fwrite (prog, 1, sz, fp);
	if (sz != wrote) {
		g_free (prog);
		fclose (fp);
		return FALSE;
	}

	/* add trailing \n */
	if (sz > 0 && prog[sz-1] != '\n')
		fwrite ("\n", 1, 1, fp);

	g_free (prog);
	fclose (fp);

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
		display_error (NULL, err);
		g_free (err);
	}
}

static void
really_save_as_cb (GtkWidget *w, GtkFileSelection *fs)
{
	const char *s;

	/* sanity */
	if (selected_program == NULL)
		return;

	s = gtk_file_selection_get_filename (fs);
	if (s == NULL)
		return;
	if (access (s, F_OK) == 0 &&
	    ! ask_question (GTK_WIDGET (fs),
			    _("File already exists.  Overwrite it?")))
		return;

	if ( ! save_program (selected_program, s /* new fname */)) {
		char *err = g_strdup_printf (_("<b>Cannot save file</b>\n"
					       "Details: %s"),
					     g_strerror (errno));
		display_error (GTK_WIDGET (fs), err);
		g_free (err);
		return;
	}

	setup_last_dir (s);

	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, TRUE);
}

static void
really_cancel_save_as_cb (GtkWidget *w, GtkFileSelection *fs)
{
	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, TRUE);
}

static void
save_as_callback (GtkWidget *w)
{
	static GtkWidget *fs = NULL;

	/* sanity */
	if (selected_program == NULL)
		return;
	
	if (fs) {
		gtk_widget_show_now(fs);
		gdk_window_raise(fs->window);
		return;
	}

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (genius_window, FALSE);

	fs = gtk_file_selection_new(_("Open GEL file"));
	
	gtk_window_position (GTK_WINDOW (fs), GTK_WIN_POS_MOUSE);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (fs_destroy_cb), &fs);
	
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
			  "clicked", G_CALLBACK (really_save_as_cb),
			  fs);
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (fs)->cancel_button),
			  "clicked", G_CALLBACK (really_cancel_save_as_cb),
			  fs);

	if (last_dir != NULL)
		gtk_file_selection_set_filename
			(GTK_FILE_SELECTION (fs), last_dir);
	gtk_file_selection_set_filename
		(GTK_FILE_SELECTION (fs), selected_program->name);

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
	if (current == 0) /* if the console */
		return;
	w = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), current);
	p = g_object_get_data (G_OBJECT (w), "program");
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
		display_error (NULL,
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

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
		gtk_text_buffer_get_iter_at_offset (buffer, &iter_end, -1);
		prog = gtk_text_buffer_get_text (buffer, &iter, &iter_end,
						 FALSE /* include_hidden_chars */);

		vte_terminal_feed (VTE_TERMINAL (term),
				   "\r\n\e[0mOutput from \e[0;32m", -1);
		vte_terminal_feed (VTE_TERMINAL (term), vname, -1);
		vte_terminal_feed (VTE_TERMINAL (term),
				   "\e[0m (((\r\n", -1);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);

		pipe (p);

		/* run this in a fork so that we don't block on very
		   long input */
		if (fork () == 0) {
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
			if(got_eof) {
				got_eof = FALSE;
				break;
			}
			if(interrupted)
				break;
		}

		gel_pop_file_info ();

		gel_lexer_close (fp);
		fclose (fp);

		calc_running --;

		gel_printout_infos ();

		running_program = NULL;

		vte_terminal_feed (VTE_TERMINAL (term),
				   "\e[0m)))End", -1);

		/* interrupt the current command line */
		interrupted = TRUE;
		vte_terminal_feed_child (VTE_TERMINAL (term), "\n", 1);

	}

}

/*main window creation, slightly copied from same-gnome:)*/
static GtkWidget *
create_main_window(void)
{
	GtkWidget *w;
        w=gnome_app_new("gnome-genius", _("GENIUS Calculator"));
	gtk_window_set_wmclass (GTK_WINDOW (w), "gnome-genius", "gnome-genius");
	gtk_window_set_policy (GTK_WINDOW (w), TRUE, FALSE, TRUE);

        g_signal_connect (G_OBJECT (w), "delete_event",
			  G_CALLBACK (quitapp), NULL);
        gtk_window_set_policy(GTK_WINDOW(w),1,1,0);
        return w;
}

/* gnome_config employment */

static void
get_properties (void)
{
	gchar buf[256];

	g_snprintf (buf, 256, "/genius/properties/pango_font=%s",
		    cursetup.font ? cursetup.font : DEFAULT_FONT);
	cursetup.font = gnome_config_get_string (buf);
	g_snprintf(buf,256,"/genius/properties/scrollback=%d",
		   cursetup.scrollback);
	cursetup.scrollback = gnome_config_get_int(buf);
	g_snprintf(buf,256,"/genius/properties/error_box=%s",
		   (cursetup.error_box)?"true":"false");
	cursetup.error_box = gnome_config_get_bool(buf);
	g_snprintf(buf,256,"/genius/properties/info_box=%s",
		   (cursetup.info_box)?"true":"false");
	cursetup.info_box = gnome_config_get_bool(buf);
	
	g_snprintf(buf,256,"/genius/properties/max_digits=%d",
		   curstate.max_digits);
	curstate.max_digits = gnome_config_get_int(buf);
	g_snprintf(buf,256,"/genius/properties/results_as_floats=%s",
		   curstate.results_as_floats?"true":"false");
	curstate.results_as_floats = gnome_config_get_bool(buf);
	g_snprintf(buf,256,"/genius/properties/scientific_notation=%s",
		   curstate.scientific_notation?"true":"false");
	curstate.scientific_notation = gnome_config_get_bool(buf);
	g_snprintf(buf,256,"/genius/properties/full_expressions=%s",
		   curstate.full_expressions?"true":"false");
	curstate.full_expressions = gnome_config_get_bool(buf);
	g_snprintf(buf,256,"/genius/properties/max_errors=%d",
		   curstate.max_errors);
	curstate.max_errors = gnome_config_get_int(buf);
}

static GArray *readbuf = NULL;
static int readbufl = 0;

static void
feed_to_zvt_from_string (const char *str, int size)
{
	/*do our own crlf translation*/
	char *s;
	int i,sz;
	for(i=0,sz=0;i<size;i++,sz++)
		if(str[i]=='\n') sz++;
	if (sz == size) {
		vte_terminal_feed (VTE_TERMINAL (term), 
				   str, size);
		return;
	}
	s = g_new(char,sz);
	for(i=0,sz=0;i<size;i++,sz++) {
		if(str[i]=='\n') {
			s[sz++] = str[i];
			s[sz] = '\r';
		} else s[sz] = str[i];
	}
	vte_terminal_feed (VTE_TERMINAL (term), s, sz);
	g_free(s);
}

static void
feed_to_zvt (gpointer data, gint source, GdkInputCondition condition)
{
	int size;
	char buf[256];
	while ((size = read (source, buf, 256)) > 0) {
		feed_to_zvt_from_string (buf, size);
	}
}

static void
output_notify_func (GelOutput *output)
{
	const char *s = gel_output_peek_string (output);
	if (s != NULL) {
		feed_to_zvt (NULL, forzvt[0], 0);
		feed_to_zvt_from_string ((char *)s, strlen (s));
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

static int
catch_interrupts (GtkWidget *w, GdkEvent *e)
{
	if (e->type == GDK_KEY_PRESS &&
	    e->key.keyval == GDK_c &&
	    e->key.state & GDK_CONTROL_MASK) {
		interrupted = TRUE;
		if (readbuf != NULL)
			readbuf = g_array_set_size(readbuf,0);
		readbufl = 0;
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

	foo = NULL;
	if (access ("./genius-readline-helper-fifo", X_OK) == 0)
		foo = g_strdup ("./genius-readline-helper-fifo");
	if (foo == NULL &&
	    access (LIBEXECDIR "/genius-readline-helper-fifo", X_OK) == 0)
		foo = g_strdup (LIBEXECDIR "/genius-readline-helper-fifo");
	if (foo == NULL) {
		dir = g_path_get_dirname (arg0);
		foo = g_strconcat
			(dir, "/../libexec/genius-readline-helper-fifo", NULL);
		if (access (foo, X_OK) != 0) {
			g_free (foo);
			foo = NULL;
		}
		if (foo == NULL) {
			foo = g_strconcat
				(dir, "/genius-readline-helper-fifo", NULL);
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
		GtkWidget *d = geniusbox (TRUE /* error */,
					  FALSE,
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

	g_free (foo);
}

static void
get_new_line(gpointer data, gint source, GdkInputCondition condition)
{
	char buf[5] = "EOF!";

	if (read (source, buf, 4)==4) {
		if (strcmp (buf, "EOF!") == 0) {
			get_cb_p_expression (NULL, torlfp);
		} else if (strcmp(buf,"LINE")==0) {
			int len = 0;
			if(read(source,(gpointer)&len,sizeof(int))!=sizeof(int))
				g_warning("Weird size from helper");
			if(len>0) {
				char *b;
				b = g_new0(char,len+1);
				if(read(source,b,len)!=len)
					g_warning ("Didn't get all the data from helper");
				get_cb_p_expression (b, torlfp);
				g_free(b);
			} else
				get_cb_p_expression ("", torlfp);
		}
	} else {
		g_warning("GOT a strange response from the helper");
	}
}

static void
genius_got_etree (GelETree *e)
{
	if (e != NULL) {
		calc_running ++;
		gel_evalexp_parsed (e, main_out, "= \e[1;36m", TRUE);
		calc_running --;
		gel_output_full_string (main_out, "\e[0m");
		gel_output_flush (main_out);
	}

	gel_printout_infos ();

	if (got_eof) {
		gel_output_full_string (main_out, "\n");
		gel_output_flush (main_out);
		got_eof = FALSE;
		gtk_main_quit();
	}
}

static char *
make_a_fifo (void)
{
	static int cnt = 1;
	for (;;) {
		char *name = g_strdup_printf ("/tmp/genius-fifo-%d-%d",
					      (int)getpid(), cnt++);
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
	torlfifo = make_a_fifo ();
	fromrlfifo = make_a_fifo ();
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
	}
}

static const char *
get_version_details (void)
{
#ifndef HAVE_MPFR
	return _("\nNote: Compiled without MPFR (some operations may be slow) "
		 "see www.mpfr.org");
#else
	return "";
#endif
}

int
main (int argc, char *argv[])
{
	GtkWidget *hbox;
	GtkWidget *w;
	GtkTooltips *tips;
	char *file;
	GnomeUIInfo *plugins;
	int plugin_count = 0;

	genius_is_gui = TRUE;

	arg0 = g_strdup (argv[0]); 
	
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("genius", VERSION, 
			    LIBGNOMEUI_MODULE /* module_info */,
			    argc, argv,
			    /* GNOME_PARAM_POPT_TABLE, options, */
			    NULL);

	if (pipe (forzvt) < 0)
		g_error ("Can't pipe");

	setup_rl_fifos ();

	fcntl (forzvt[0], F_SETFL, O_NONBLOCK);
	gdk_input_add (forzvt[0],
		       GDK_INPUT_READ,
		       feed_to_zvt, NULL);

	main_out = gel_output_new();
	gel_output_setup_string (main_out, 80, get_term_width);
	gel_output_set_notify (main_out, output_notify_func);
	
	evalnode_hook = check_events;
	statechange_hook = set_state;

	gel_read_plugin_list ();

	/*read gnome_config parameters */
	get_properties ();
	
        /*set up the top level window*/
	genius_window = create_main_window();

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
					   cursetup.scrollback);
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

	/* No programs in the menu */
	gtk_widget_hide (genius_menu[PROGRAMS_MENU].widget);

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
	gtk_widget_queue_resize (zvt);
	*/
	gtk_container_border_width(
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

	/* for some reason we must set the font here and not above
	 * or the "monospace 12" (or default terminal font or whatnot)
	 * will get used */
	vte_terminal_set_font_from_string (VTE_TERMINAL (term), 
					   cursetup.font ?
					   cursetup.font : DEFAULT_FONT);


	gtk_widget_show_now (genius_window);

	gel_output_printf (main_out,
			   _("\e[0;32mGenius %s\e[0m\n"
			     "%s\n"
			     "This is free software with ABSOLUTELY NO WARRANTY.\n"
			     "For license details type `\e[01;36mwarranty\e[0m'.\n"
			     "For help type '\e[01;36mmanual\e[0m' or '\e[01;36mhelp\e[0m'.%s\n\n"),
			   VERSION,
			   COPYRIGHT_STRING,
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
	gdk_input_add (fromrl, GDK_INPUT_READ,
		       get_new_line, NULL);

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext ();

	gel_add_graph_functions ();

	/*
	 * Read main library
	 */
	if (access ("../lib/lib.cgel", F_OK) == 0) {
		/*try the library file in the current/../lib directory*/
		gel_load_compiled_file (NULL, "../lib/lib.cgel", FALSE);
	} else {
		gel_load_compiled_file (NULL, LIBRARY_DIR "/gel/lib.cgel",
					FALSE);
	}

	/*
	 * Read init files
	 */
	file = g_strconcat(g_getenv("HOME"),"/.geniusinit",NULL);
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

