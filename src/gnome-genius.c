/* GENIUS Calculator
 * Copyright (C) 1997-2002 George Lebl
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

#define GTK_ENABLE_BROKEN

#include <gnome.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "calc.h"
#include "util.h"
#include "dict.h"
#include "geloutput.h"

#include "plugin.h"
#include "inter.h"

#include <readline/readline.h>
#include <readline/history.h>

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
	
extern calc_error_t error_num;
extern int got_eof;
extern int parenth_depth;
extern int interrupted;

static GtkWidget *setupdialog = NULL;
static GtkWidget *window = NULL;
static GtkWidget *term = NULL;
static GString *errors=NULL;
static GString *infos=NULL;

static int errors_printed = 0;

typedef struct {
	int error_box;
	int info_box;
	int scrollback;
	char *font;
} geniussetup_t;

geniussetup_t cursetup = {
	FALSE,
	TRUE,
	1000,
	NULL
};

static FILE *torlfp = NULL;
static int fromrl;

static int forzvt[2];

static char *torlfifo = NULL;
static char *fromrlfifo = NULL;

static char *arg0 = NULL;

static void feed_to_zvt (gpointer data, gint source,
			 GdkInputCondition condition);

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
		mb = gtk_message_dialog_new (GTK_WINDOW (window) /* parent */,
					     0 /* flags */,
					     type,
					     GTK_BUTTONS_OK,
					     "%s",
					     s);
	} else {
		GtkWidget *sw, *t;
		mb = gtk_dialog_new_with_buttons
			(error?_("Error"):_("Information"),
			 GTK_WINDOW (window) /* parent */,
			 0 /* flags */,
			 GTK_STOCK_OK, GTK_RESPONSE_OK,
			 NULL);
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mb)->vbox),
				    sw,
				    TRUE, TRUE, 0);
		t = gtk_text_new(NULL,NULL);
		gtk_text_set_editable(GTK_TEXT(t),FALSE);
		gtk_text_set_line_wrap(GTK_TEXT(t),TRUE);
		gtk_text_set_word_wrap(GTK_TEXT(t),TRUE);
		gtk_text_insert (GTK_TEXT (t),
				 NULL /* FIXME ZVT_TERM(zvt)->font*/,
				 NULL,NULL,s,strlen(s));
		gtk_container_add(GTK_CONTAINER(sw),t);
		gtk_widget_set_usize(sw,500,300);
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
					      GTK_WINDOW (window));

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

/* quit */
static void
quitapp (GtkWidget * widget, gpointer data)
{
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
				     GTK_WINDOW(window));
	
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
	gtk_signal_connect(GTK_OBJECT(adj),"value_changed",
			   GTK_SIGNAL_FUNC(intspincb),&tmpstate.max_digits);


	w=gtk_check_button_new_with_label(_("Results as floats"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpstate.results_as_floats);
	gtk_signal_connect(GTK_OBJECT(w), "toggled",
		   GTK_SIGNAL_FUNC(optioncb),
		   (gpointer)&tmpstate.results_as_floats);
	
	w=gtk_check_button_new_with_label(_("Floats in scientific notation"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpstate.scientific_notation);
	gtk_signal_connect(GTK_OBJECT(w), "toggled",
		   GTK_SIGNAL_FUNC(optioncb),
		   (gpointer)&tmpstate.scientific_notation);

	w=gtk_check_button_new_with_label(_("Always print full expressions"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpstate.full_expressions);
	gtk_signal_connect(GTK_OBJECT(w), "toggled",
		   GTK_SIGNAL_FUNC(optioncb),
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
	gtk_signal_connect(GTK_OBJECT(w), "toggled",
		   GTK_SIGNAL_FUNC(optioncb),
		   (gpointer)&tmpsetup.error_box);

	w=gtk_check_button_new_with_label(_("Display information messages in a dialog"));
	gtk_box_pack_start(GTK_BOX(box),w,FALSE,FALSE,0);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(w), 
				    tmpsetup.info_box);
	gtk_signal_connect(GTK_OBJECT(w), "toggled",
		   GTK_SIGNAL_FUNC(optioncb),
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
	gtk_signal_connect(GTK_OBJECT(adj),"value_changed",
			   GTK_SIGNAL_FUNC(intspincb),&tmpstate.max_errors);


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
	gtk_signal_connect(GTK_OBJECT(adj),"value_changed",
			   GTK_SIGNAL_FUNC(intspincb),&tmpstate.float_prec);


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
	gtk_signal_connect(GTK_OBJECT(adj),"value_changed",
			   GTK_SIGNAL_FUNC(intspincb),&tmpsetup.scrollback);
	
	
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
        gtk_signal_connect(GTK_OBJECT(w),"font_set",
                           GTK_SIGNAL_FUNC(fontsetcb),
			   &tmpsetup.font);


	gtk_signal_connect(GTK_OBJECT(setupdialog), "apply",
			   GTK_SIGNAL_FUNC(do_setup), NULL);	
	gtk_signal_connect(GTK_OBJECT(setupdialog), "destroy",
			   GTK_SIGNAL_FUNC(destroy_setup), NULL);
	gtk_widget_show_all(setupdialog);
}

static void
interrupt_calc(GtkWidget *widget, gpointer data)
{
	interrupted = TRUE;
}

static void
warranty_call (GtkWidget *widget, gpointer data)
{
	/* perhaps a bit ugly */
	gel_evalexp ("warranty", NULL, main_out, NULL, TRUE, NULL);
	gel_printout_infos ();
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
	    ! g_file_exists (s)) {
		gnome_app_error (GNOME_APP (window),
				 _("Can not open file!"));
		return;
	}
	gel_load_guess_file (NULL, s, TRUE);

	gel_printout_infos ();
}

static void
load_cb(GtkWidget *w)
{
	static GtkWidget *fs = NULL;
	
	if(fs) {
		gtk_widget_show_now(fs);
		gdk_window_raise(fs->window);
		return;
	}

	fs = gtk_file_selection_new(_("Load GEL file"));
	
	gtk_window_position (GTK_WINDOW (fs), GTK_WIN_POS_MOUSE);

	gtk_signal_connect (GTK_OBJECT (fs), "destroy",
			    GTK_SIGNAL_FUNC(fs_destroy_cb), &fs);
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC(really_load_cb),
			    fs);

	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				   GTK_OBJECT(fs));
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (fs)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				   GTK_OBJECT(fs));

	gtk_widget_show (fs);
}

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_ITEM_STOCK(N_("_Load"),N_("Load and execute a file in genius"),load_cb, GNOME_STOCK_MENU_OPEN),
	GNOMEUIINFO_MENU_EXIT_ITEM(quitapp,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo settings_menu[] = {  
	GNOMEUIINFO_MENU_PREFERENCES_ITEM(setup_calc,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo calc_menu[] = {  
	GNOMEUIINFO_ITEM_STOCK(N_("_Interrupt"),N_("Interrupt current calculation"),interrupt_calc,GNOME_STOCK_MENU_STOP),
	GNOMEUIINFO_END,
};

static GnomeUIInfo help_menu[] = {  
	/* FIXME: no help 
	 * GNOMEUIINFO_HELP("genius"),*/
	GNOMEUIINFO_ITEM_STOCK (N_("_Warranty"),
				N_("Display warranty information"),
				warranty_call,
				GNOME_STOCK_MENU_ABOUT),
	GNOMEUIINFO_MENU_ABOUT_ITEM(aboutcb,NULL),
	GNOMEUIINFO_END,
};

static GnomeUIInfo plugin_menu[] = {
	GNOMEUIINFO_END,
};
  
static GnomeUIInfo genius_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE(file_menu),
	GNOMEUIINFO_SUBTREE(N_("_Calculator"),calc_menu),
#define PLUGIN_MENU 2
	GNOMEUIINFO_SUBTREE(N_("_Plugins"),plugin_menu),
	GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
	GNOMEUIINFO_MENU_HELP_TREE(help_menu),
	GNOMEUIINFO_END,
};

/* toolbar */
static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK(N_("Interrupt"),N_("Interrupt current calculation"),interrupt_calc,GNOME_STOCK_PIXMAP_STOP),
	GNOMEUIINFO_ITEM_STOCK(N_("Load"),N_("Load and execute a file in genius"),load_cb, GNOME_STOCK_PIXMAP_OPEN),
	GNOMEUIINFO_ITEM_STOCK(N_("Exit"),N_("Exit genius"), quitapp, GNOME_STOCK_PIXMAP_EXIT),
	GNOMEUIINFO_END,
};


#define ELEMENTS(x) (sizeof (x) / sizeof (x [0]))

/*main window creation, slightly copied from same-gnome:)*/
static GtkWidget *
create_main_window(void)
{
	GtkWidget *w;
        w=gnome_app_new("gnome-genius", _("GENIUS Calculator"));
	gtk_window_set_wmclass (GTK_WINDOW (w), "gnome-genius", "gnome-genius");
	gtk_window_set_policy (GTK_WINDOW (w), TRUE, FALSE, TRUE);

        gtk_signal_connect(GTK_OBJECT(w), "delete_event",
		GTK_SIGNAL_FUNC(quitapp), NULL);
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
	    state.output_style == GEL_OUTPUT_TROFF)
		gel_output_set_line_length (main_out, 0, NULL);
	else
		gel_output_set_line_length (main_out, 80, get_term_width);
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
	pid_t pid;
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

	pid = vte_terminal_fork_command (VTE_TERMINAL (term),
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
		gel_evalexp_parsed (e, main_out, "= \e[1;36m", TRUE);
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
	window = create_main_window();

	/*set up the tooltips*/
	tips = gtk_tooltips_new();

	/*the main box to put everything in*/
	hbox = gtk_hbox_new(FALSE,0);

	term = vte_terminal_new ();
	vte_terminal_set_scrollback_lines (VTE_TERMINAL (term),
					   cursetup.scrollback);
	vte_terminal_set_font_from_string (VTE_TERMINAL (term),
					   cursetup.font ?
					   cursetup.font : DEFAULT_FONT);
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
		genius_menu[PLUGIN_MENU].moreinfo = plugins;
		
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
        gnome_app_create_menus(GNOME_APP(window), genius_menu);
	/*set up the toolbar*/
	gnome_app_create_toolbar (GNOME_APP(window), toolbar);

	/* if no plugins, hide the menu */
	if (plugin_count == 0) {
		gtk_widget_hide (genius_menu[PLUGIN_MENU].widget);
	}

	/*setup appbar*/
	w = gnome_appbar_new(FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar(GNOME_APP(window), w);
	gtk_widget_show(w);

	gnome_app_install_menu_hints(GNOME_APP(window),
				     genius_menu);


	/*set up the main window*/
	gnome_app_set_contents (GNOME_APP (window), hbox);
	/* FIXME:
	gtk_widget_queue_resize (zvt);
	*/
	gtk_container_border_width(
		GTK_CONTAINER (GNOME_APP (window)->contents), 5);

	{
		int width = 800;
		int height = 600;

		if (width > gdk_screen_width () * 0.75)
			width = gdk_screen_width () * 0.75;
		if (height > gdk_screen_height () * 0.75)
			height = gdk_screen_height () * 0.75;

		gtk_window_set_default_size (GTK_WINDOW (window), width, height);
	}

	gtk_widget_show_all (window);
	gtk_widget_show_now (window);

	gel_output_printf (main_out,
			   _("Genius %s\n"
			     "%s\n"
			     "This is free software with ABSOLUTELY NO WARRANTY.\n"
			     "For details type `warranty'.\n\n"),
			   VERSION,
			   COPYRIGHT_STRING);
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

	/*
	 * Restore plugins
	 */
	gel_restore_plugins ();

	gel_printout_infos ();

	gtk_widget_grab_focus (term);

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

