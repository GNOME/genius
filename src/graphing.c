/* GENIUS Calculator
 * Copyright (C) 2003-2004 Jiri (George) Lebl
 *
 * Author: Jiri (George) Lebl
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
#include <string.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include <math.h>

#include <vicious.h>

#include "gtkextra.h"

#include "calc.h"
#include "eval.h"
#include "util.h"
#include "dict.h"
#include "funclib.h"
#include "matrixw.h"
#include "compil.h"
#include "plugin.h"
#include "geloutput.h"
#include "mpwrap.h"

#include "gnome-genius.h"

#include "graphing.h"

#define MAXFUNC 10

static GtkWidget *graph_window = NULL;
static GtkWidget *plot_canvas = NULL;
static GtkWidget *line_plot = NULL;

static GtkWidget *plot_zoomout_item = NULL;
static GtkWidget *plot_zoomin_item = NULL;
static GtkWidget *plot_zoomfit_item = NULL;
static GtkWidget *plot_print_item = NULL;
static GtkWidget *plot_exportps_item = NULL;
static GtkWidget *plot_exporteps_item = NULL;

static GtkPlotData *line_data[MAXFUNC] = { NULL };

static GtkWidget *plot_dialog = NULL;
static GtkWidget *plot_entries[MAXFUNC] = { NULL };
static GtkWidget *plot_entries_status[MAXFUNC] = { NULL };
static double spinx1 = -M_PI;
static double spinx2 = M_PI;
static double spiny1 = -1.1;
static double spiny2 = 1.1;

static double defx1 = -M_PI;
static double defx2 = M_PI;
static double defy1 = -1.1;
static double defy2 = 1.1;

/* Replotting info */
static GelEFunc *plot_func[MAXFUNC] = { NULL };
static double plotx1 = -M_PI;
static double plotx2 = M_PI;
static double ploty1 = -1.1;
static double ploty2 = 1.1;

static double plot_maxy = - G_MAXDOUBLE/2;
static double plot_miny = G_MAXDOUBLE/2;

static GelCtx *plot_ctx = NULL;
static GelETree *plot_arg = NULL;

static int plot_in_progress = 0;

static void plot_functions (void);
static void plot_axis (void);

#define WIDTH 640
#define HEIGHT 480
#define ASPECT ((double)HEIGHT/(double)WIDTH)

#define PROPORTION 0.85
#define PROPORTION_OFFSET 0.075

enum {
	RESPONSE_STOP = 1,
	RESPONSE_PLOT
};

static void
plot_window_setup (void)
{
	if (graph_window != NULL) {
		if (plot_in_progress)
			genius_setup_window_cursor (plot_canvas, GDK_WATCH);
		else
			genius_unsetup_window_cursor (plot_canvas);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (graph_window),
						   RESPONSE_STOP, plot_in_progress);
		gtk_widget_set_sensitive (plot_zoomout_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_zoomin_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_zoomfit_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_print_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_exportps_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_exporteps_item, ! plot_in_progress);
	}
}

static void
dialog_response (GtkWidget *w, int response, gpointer data)
{
	if (response == GTK_RESPONSE_CLOSE ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		if (plot_in_progress > 0)
			interrupted = TRUE;
		gtk_widget_destroy (graph_window);
	} else if (response == RESPONSE_STOP && plot_in_progress > 0) {
		interrupted = TRUE;
	}
}

static void
print_entry_activate (GtkWidget *entry, gpointer data)
{
	gtk_dialog_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
}


static void
plot_print_cb (void)
{
	gboolean ret;
	GtkWidget *req = NULL;
	GtkWidget *hbox, *w, *cmd;
	int fd;
	char tmpfile[] = "/tmp/genius-ps-XXXXXX";
	static char *last_cmd = NULL;

	if (last_cmd == NULL)
		last_cmd = g_strdup ("lpr");

	req = gtk_dialog_new_with_buttons
		(_("Print") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 GTK_DIALOG_MODAL /* flags */,
		 GTK_STOCK_CANCEL,
		 GTK_RESPONSE_CANCEL,
		 GTK_STOCK_PRINT,
		 GTK_RESPONSE_OK,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (req),
					 GTK_RESPONSE_OK);

	gtk_dialog_set_has_separator (GTK_DIALOG (req), FALSE);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (req)->vbox),
			    hbox, TRUE, TRUE, 0);

	w = gtk_label_new (_("Print command: "));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	cmd = gtk_entry_new ();
	g_signal_connect (G_OBJECT (cmd), "activate",
			  G_CALLBACK (print_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (hbox), cmd, TRUE, TRUE, 0);

	gtk_entry_set_text (GTK_ENTRY (cmd), last_cmd);

	gtk_widget_show_all (hbox);

	g_signal_connect (G_OBJECT (req), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &req);

	if (gtk_dialog_run (GTK_DIALOG (req)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy (req);
		return;
	}

	last_cmd = g_strdup (gtk_entry_get_text (GTK_ENTRY (cmd)));

	gtk_widget_destroy (req);

	fd = g_mkstemp (tmpfile);
	if (fd < 0) {
		genius_display_error (graph_window, _("Cannot open temporary file, cannot print."));
		return;
	}

	plot_in_progress ++;
	plot_window_setup ();

	/* Letter will fit on A4, so just currently do that */
	ret = gtk_plot_export_ps (GTK_PLOT (line_plot),
				  tmpfile,
				  GTK_PLOT_LANDSCAPE,
				  FALSE /* epsflag */,
				  GTK_PLOT_LETTER);

	if ( ! ret || interrupted) {
		plot_in_progress --;
		plot_window_setup ();

		if ( ! interrupted)
			genius_display_error (graph_window, _("Printing failed"));
		interrupted = FALSE;
		close (fd);
		unlink (tmpfile);
		return;
	}

	{
		char *cmdstring = g_strdup_printf ("cat %s | %s", tmpfile, last_cmd);
		system (cmdstring);
		g_free (cmdstring);
	}

	plot_in_progress --;
	plot_window_setup ();

	close (fd);
	unlink (tmpfile);
}

static char *last_export_dir = NULL;

#if ! GTK_CHECK_VERSION(2,3,5)
static void
setup_last_dir (const char *filename)
{
	char *s = g_path_get_dirname (filename);

	g_free (last_export_dir);
	if (s == NULL) {
		last_export_dir = NULL;
		return;
	}
	if (strcmp(s, "/") == 0) {
		last_export_dir = s;
		return;
	}
	last_export_dir = g_strconcat (s, "/", NULL);
	g_free (s);
}
#endif

#if GTK_CHECK_VERSION(2,3,5)
static void
really_export_cb (GtkFileChooser *fs, int response, gpointer data)
#else
static void
really_export_cb (GtkWidget *w, GtkFileSelection *fs, gpointer data)
#endif
{
	char *s;
	char *base;
	gboolean ret;
	gboolean eps = GPOINTER_TO_INT (data);

#if GTK_CHECK_VERSION(2,3,5)
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		/* FIXME: don't want to deal with modality issues right now */
		gtk_widget_set_sensitive (graph_window, TRUE);
		return;
	}
#endif

#if GTK_CHECK_VERSION(2,3,5)
	s = g_strdup (gtk_file_chooser_get_filename (fs));
#else
	s = g_strdup (gtk_file_selection_get_filename (fs));
#endif
	if (s == NULL)
		return;
	base = g_path_get_basename (s);
	if (base != NULL && base[0] != '\0' &&
	    strchr (base, '.') == NULL) {
		char *n;
		if (eps)
			n = g_strconcat (s, ".eps", NULL);
		else
			n = g_strconcat (s, ".ps", NULL);
		g_free (s);
		s = n;
	}
	g_free (base);
	
	if (access (s, F_OK) == 0 &&
	    ! genius_ask_question (GTK_WIDGET (fs),
				   _("File already exists.  Overwrite it?"))) {
		g_free (s);
		return;
	}

#if GTK_CHECK_VERSION(2,3,5)
	g_free (last_export_dir);
	last_export_dir = gtk_file_chooser_get_current_folder (fs);
#else
	setup_last_dir (s);
#endif

	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (graph_window, TRUE);

	plot_in_progress ++;
	plot_window_setup ();

	/* FIXME: There should be some options about size and stuff */
	ret = gtk_plot_export_ps_with_size (GTK_PLOT (line_plot),
					    s,
					    GTK_PLOT_PORTRAIT,
					    eps /* epsflag */,
					    GTK_PLOT_PSPOINTS,
					    400, ASPECT * 400);

	plot_in_progress --;
	plot_window_setup ();

	if ( ! ret || interrupted) {
		if ( ! interrupted)
			genius_display_error (graph_window, _("Export failed"));
		g_free (s);
		interrupted = FALSE;
		return;
	}

	g_free (s);
}

#if ! GTK_CHECK_VERSION(2,3,5)
static void
really_cancel_export_cb (GtkWidget *w, GtkFileSelection *fs)
{
	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (graph_window, TRUE);
}
#endif

static void
do_export_cb (gboolean eps)
{
	static GtkWidget *fs = NULL;
#if GTK_CHECK_VERSION(2,3,5)
	GtkFileFilter *filter_ps;
	GtkFileFilter *filter_all;
#endif
	const char *title;

	if (fs != NULL) {
		gtk_window_present (GTK_WINDOW (fs));
		return;
	}

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (graph_window, FALSE);

	if (eps)
		title = _("Export encapsulated postscript");
	else
		title = _("Export postscript");

#if GTK_CHECK_VERSION(2,3,5)
	fs = gtk_file_chooser_dialog_new (title,
					  GTK_WINDOW (graph_window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					  NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), TRUE);


	filter_ps = gtk_file_filter_new ();
	if (eps) {
		gtk_file_filter_set_name (filter_ps, _("EPS files"));
		gtk_file_filter_add_pattern (filter_ps, "*.eps");
		gtk_file_filter_add_pattern (filter_ps, "*.EPS");
	} else {
		gtk_file_filter_set_name (filter_ps, _("PS files"));
		gtk_file_filter_add_pattern (filter_ps, "*.ps");
		gtk_file_filter_add_pattern (filter_ps, "*.PS");
	}

	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_pattern (filter_all, "*");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_ps);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fs), filter_ps);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	g_signal_connect (G_OBJECT (fs), "response",
			  G_CALLBACK (really_export_cb),
			  GINT_TO_POINTER (eps));

	if (last_export_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_export_dir);
	}
#else
	fs = gtk_file_selection_new (title);
	
	gtk_window_set_position (GTK_WINDOW (fs), GTK_WIN_POS_MOUSE);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (fs)->ok_button),
			  "clicked", G_CALLBACK (really_export_cb),
			  fs);
	g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (fs)->cancel_button),
			  "clicked", G_CALLBACK (really_cancel_export_cb),
			  fs);

	if (last_export_dir != NULL)
		gtk_file_selection_set_filename
			(GTK_FILE_SELECTION (fs), last_export_dir);
#endif

	gtk_widget_show (fs);
}

static void
plot_exportps_cb (void)
{
	do_export_cb (FALSE);
}

static void
plot_exporteps_cb (void)
{
	do_export_cb (TRUE);
}

static void
plot_zoomin_cb (void)
{
	if (plot_in_progress == 0) {
		double len;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		len = plotx2 - plotx1;
		plotx2 -= len/4.0;
		plotx1 += len/4.0;

		len = ploty2 - ploty1;
		ploty2 -= len/4.0;
		ploty1 += len/4.0;

		plot_axis ();

		if (interrupted)
			interrupted = FALSE;

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static void
plot_zoomout_cb (void)
{
	if (plot_in_progress == 0) {
		double len;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		len = plotx2 - plotx1;
		plotx2 += len/2.0;
		plotx1 -= len/2.0;

		len = ploty2 - ploty1;
		ploty2 += len/2.0;
		ploty1 -= len/2.0;

		plot_axis ();

		if (interrupted)
			interrupted = FALSE;

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static void
plot_zoomfit_cb (void)
{
	if (plot_in_progress == 0) {
		double size;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		size = plot_maxy - plot_miny;
		if (size <= 0)
			size = 1.0;

		ploty1 = plot_miny - size * 0.05;
		ploty2 = plot_maxy + size * 0.05;

		/* sanity */
		if (ploty2 < ploty1)
			ploty2 = ploty1 + 0.1;

		plot_axis ();

		if (interrupted)
			interrupted = FALSE;

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static void
plot_select_region (GtkPlotCanvas *canvas,
		    gdouble xmin,
		    gdouble ymin,
		    gdouble xmax,
		    gdouble ymax)
{
	if (plot_in_progress == 0) {
		double len;
		double px, py, pw, ph;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		/* FIXME: evil because this is the selection thingie, hmmm, I dunno another
		   way to do this though */

		gtk_plot_get_position (GTK_PLOT (line_plot), &px, &py);
		gtk_plot_get_size (GTK_PLOT (line_plot), &pw, &ph);

		xmin -= px;
		ymin -= py;
		xmax -= px;
		ymax -= py;

		xmin /= pw;
		ymin /= ph;
		xmax /= pw;
		ymax /= ph;

		{ /* flip the y coordinate */
			double oldymin = ymin;
			ymin = 1.0 - ymax;
			ymax = 1.0 - oldymin;
		}

		len = plotx2 - plotx1;
		plotx1 += len * xmin;
		plotx2 = plotx1 + (len * (xmax-xmin));

		len = ploty2 - ploty1;
		ploty1 += len * ymin;
		ploty2 = ploty1 + (len * (ymax-ymin));

		/* sanity */
		if (plotx2 <= plotx1)
			plotx2 = plotx1 + 0.00000001;
		/* sanity */
		if (ploty2 <= ploty1)
			ploty2 = ploty1 + 0.00000001;

		plot_axis ();

		if (interrupted)
			interrupted = FALSE;

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static void
ensure_window (void)
{
	GtkWidget *menu, *menubar, *item;

	if (graph_window != NULL) {
		/* FIXME: present is evil in that it takes focus away */
		gtk_widget_show (graph_window);
		return;
	}

	graph_window = gtk_dialog_new_with_buttons
		(_("Genius Line Plot") /* title */,
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_STOP,
		 RESPONSE_STOP,
		 GTK_STOCK_CLOSE,
		 GTK_RESPONSE_CLOSE,
		 NULL);
	g_signal_connect (G_OBJECT (graph_window),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &graph_window);
	g_signal_connect (G_OBJECT (graph_window),
			  "response",
			  G_CALLBACK (dialog_response),
			  NULL);

	menubar = gtk_menu_bar_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (graph_window)->vbox),
			    GTK_WIDGET (menubar), FALSE, TRUE, 0);

	menu = gtk_menu_new ();
	item = gtk_menu_item_new_with_mnemonic (_("_Graph"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Print..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_print_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_print_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("_Export postscript..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_exportps_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_exportps_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("E_xport encapsulated postscript..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_exporteps_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_exporteps_item = item;


	menu = gtk_menu_new ();
	item = gtk_menu_item_new_with_mnemonic (_("_Zoom"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

	item = gtk_menu_item_new_with_mnemonic (_("Zoom _out"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_zoomout_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_zoomout_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("Zoom _in"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_zoomin_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_zoomin_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("Fit Y axis"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_zoomfit_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_zoomfit_item = item;


	plot_canvas = gtk_plot_canvas_new (WIDTH, HEIGHT, 1.0);
	GTK_PLOT_CANVAS_UNSET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				     GTK_PLOT_CANVAS_DND_FLAGS);
	GTK_PLOT_CANVAS_SET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				   GTK_PLOT_CANVAS_CAN_SELECT);
	g_signal_connect (G_OBJECT (plot_canvas), "select_region",
			  G_CALLBACK (plot_select_region),
			  NULL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (graph_window)->vbox),
			    GTK_WIDGET (plot_canvas), TRUE, TRUE, 0);

	line_plot = gtk_plot_new_with_size (NULL, PROPORTION, PROPORTION);
	gtk_plot_canvas_add_plot (GTK_PLOT_CANVAS (plot_canvas),
				  GTK_PLOT (line_plot), PROPORTION_OFFSET, PROPORTION_OFFSET);

	gtk_plot_axis_set_visible (GTK_PLOT (line_plot),
				   GTK_PLOT_AXIS_TOP, TRUE);
	gtk_plot_axis_set_visible (GTK_PLOT (line_plot),
				   GTK_PLOT_AXIS_RIGHT, TRUE);
	gtk_plot_grids_set_visible (GTK_PLOT (line_plot),
				    FALSE, FALSE, FALSE, FALSE);
	gtk_plot_axis_hide_title (GTK_PLOT (line_plot),
				  GTK_PLOT_AXIS_TOP);
	gtk_plot_axis_hide_title (GTK_PLOT (line_plot),
				  GTK_PLOT_AXIS_RIGHT);
	gtk_plot_axis_hide_title (GTK_PLOT (line_plot),
				  GTK_PLOT_AXIS_LEFT);
	gtk_plot_axis_hide_title (GTK_PLOT (line_plot),
				  GTK_PLOT_AXIS_BOTTOM);
	/*gtk_plot_axis_set_title (GTK_PLOT (line_plot),
				 GTK_PLOT_AXIS_LEFT, "Y");
	gtk_plot_axis_set_title (GTK_PLOT (line_plot),
				 GTK_PLOT_AXIS_BOTTOM, "X");*/
	gtk_plot_set_legends_border (GTK_PLOT (line_plot),
				     GTK_PLOT_BORDER_LINE, 3);
	gtk_plot_legends_move (GTK_PLOT (line_plot), .80, .05);
	gtk_widget_show (line_plot);

	gtk_widget_show_all (graph_window);
}


static void
clear_graph (void)
{
	int i;
	for (i = 0; i < MAXFUNC; i++) {
		if (line_data[i] != NULL) {
			gtk_plot_remove_data (GTK_PLOT (line_plot),
					      line_data[i]);
			line_data[i] = NULL;
		}
	}
}

static void
get_ticks (double start, double end, double *tick, int *prec)
{
	int incs;
	double len = end-start;

	*tick = pow (10, floor (log10 (len)));
	incs = floor (len / *tick);

	while (incs < 3) {
		*tick /= 2.0;
		incs = floor (len / *tick);
	}

	while (incs > 6) {
		*tick *= 2.0;
		incs = floor (len / *tick);
	}

	if (*tick >= 0.99) {
		*prec = 0;
	} else {
		*prec = - (int) log10 (*tick) + 1;
	}
}

static void
plot_setup_axis (void)
{
	int xprec, yprec;
	double xtick, ytick;

	get_ticks (plotx1, plotx2, &xtick, &xprec);
	get_ticks (ploty1, ploty2, &ytick, &yprec);

	gtk_plot_set_range (GTK_PLOT (line_plot),
			    plotx1, plotx2, ploty1, ploty2);
	gtk_plot_axis_set_ticks (GTK_PLOT (line_plot), GTK_PLOT_AXIS_X, xtick, 10);
	gtk_plot_axis_set_ticks (GTK_PLOT (line_plot), GTK_PLOT_AXIS_Y, ytick, 10);
	gtk_plot_axis_set_labels_style (GTK_PLOT (line_plot),
					GTK_PLOT_AXIS_X,
					GTK_PLOT_LABEL_FLOAT,
					xprec /* precision */);
	gtk_plot_axis_set_labels_style (GTK_PLOT (line_plot),
					GTK_PLOT_AXIS_Y,
					GTK_PLOT_LABEL_FLOAT,
					yprec /* precision */);
}

static void
plot_axis (void)
{
	plot_in_progress ++;

	plot_window_setup ();

	plot_maxy = - G_MAXDOUBLE/2;
	plot_miny = G_MAXDOUBLE/2;

	plot_setup_axis ();

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	if (plot_canvas != NULL)
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));

	plot_in_progress --;
	plot_window_setup ();
}

static double
call_func (GelCtx *ctx, GelEFunc *func, GelETree *arg, gboolean *ex)
{
	GelETree *ret;
	double retd;
	GelETree *args[2];

	args[0] = arg;
	args[1] = NULL;

	ret = funccall (ctx, func, args, 1);

	/* FIXME: handle errors! */
	if (error_num != 0)
		error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == FUNCTION_NODE && ret->func.func->nargs == 1) {
		GelETree *ret2;
		ret2 = funccall (ctx, ret->func.func, args, 1);
		gel_freetree (ret);
		ret = ret2;
		/* FIXME: handle errors! */
		if (error_num != 0)
			error_num = 0;

	}

	if (ret == NULL || ret->type != VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
		return 0;
	}

	retd = mpw_get_double (ret->val.value);
	if (error_num != 0) {
		*ex = TRUE;
		error_num = 0;
	}
	
	gel_freetree (ret);
	return retd;
}

static double
plot_func_data (GtkPlot *plot, GtkPlotData *data, double x, gboolean *error)
{
	static int hookrun = 0;
	gboolean ex = FALSE;
	int i;
	double y;

	if (error != NULL)
		*error = FALSE;

	if G_UNLIKELY (interrupted) {
		if (error != NULL)
			*error = TRUE;
		return 0.0;
	}

	for (i = 0; i < MAXFUNC; i++) {
		if (data == line_data[i])
			break;
	}
	if G_UNLIKELY (i == MAXFUNC) {
		if (error != NULL)
			*error = TRUE;
		return 0.0;
	}

	mpw_set_d (plot_arg->val.value, x);
	y = call_func (plot_ctx, plot_func[i], plot_arg, &ex);

	if G_UNLIKELY (ex) {
		if (error != NULL)
			*error = TRUE;
	} else {
		if G_UNLIKELY (y > plot_maxy)
			plot_maxy = y;
		if G_UNLIKELY (y < plot_miny)
			plot_miny = y;
	}

	if (hookrun++ >= 10) {
		hookrun = 0;
		if (evalnode_hook != NULL) {
			(*evalnode_hook)();
			if G_UNLIKELY (interrupted) {
				if (error != NULL)
					*error = TRUE;
				return y;
			}
		}
	}

	return y;
}

static char *
label_func (int i, GelEFunc *func, const char *color)
{
	char *text = NULL;

	if (func->id != NULL) {
		text = g_strdup_printf ("%s(x)", func->id->token);
	} else if (func->type == GEL_USER_FUNC) {
		int old_style, len;
		GelOutput *out = gel_output_new ();
		D_ENSURE_USER_BODY (func);
		gel_output_setup_string (out, 0, NULL);

		/* FIXME: the push/pop of style is UGLY */
		old_style = calcstate.output_style;
		calcstate.output_style = GEL_OUTPUT_NORMAL;
		gel_print_etree (out, func->data.user, TRUE /* toplevel */);
		calcstate.output_style = old_style;

		text = gel_output_snarf_string (out);
		gel_output_unref (out);

		len = strlen (text);

		if (len > 2 &&
		    text[0] == '(' &&
		    text[len-1] == ')') {
			text[len-1] = '\0';
			strcpy (text, &text[1]);
			len-=2;
		}

		/* only print bodies of short functions */
		if (len > 64) {
			g_free (text);
			text = NULL;
		}
	}

	if (text == NULL) {
		text = g_strdup_printf (_("Function #%d"), i+1);
	}

	return text;
}

#define GET_DOUBLE(var,argnum) \
	{ \
	if (a[argnum]->type != VALUE_NODE) { \
		gel_errorout (_("%s: argument number %d not a number"), "LinePlot", argnum+1); \
		return NULL; \
	} \
	var = mpw_get_double (a[argnum]->val.value); \
	}

static gboolean
get_limits_from_matrix (GelETree *m, double *x1, double *x2, double *y1, double *y2)
{
	GelETree *t;

	if (m->type != MATRIX_NODE ||
	    gel_matrixw_elements (m->mat.matrix) != 4) {
		gel_errorout (_("Graph limits not given as a 4-vector"));
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 0);
	if (t->type != VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x1 = mpw_get_double (t->val.value);
	t = gel_matrixw_vindex (m->mat.matrix, 1);
	if (t->type != VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x2 = mpw_get_double (t->val.value);
	t = gel_matrixw_vindex (m->mat.matrix, 2);
	if (t->type != VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*y1 = mpw_get_double (t->val.value);
	t = gel_matrixw_vindex (m->mat.matrix, 3);
	if (t->type != VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*y2 = mpw_get_double (t->val.value);

	/* FIXME: what about errors */
	if (error_num != 0) {
		error_num = 0;
		return FALSE;
	}

	if (*x1 > *x2) {
		double s = *x1;
		*x1 = *x2;
		*x2 = s;
	}

	if (*y1 > *y2) {
		double s = *y1;
		*y1 = *y2;
		*y2 = s;
	}

	return TRUE;
}

static GelETree *
make_matrix_from_limits (void)
{
	GelETree *n;
	GelMatrixW *m;
	/*make us a new empty node*/
	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 4, 1);

	gel_matrixw_set_index (m, 0, 0) = gel_makenum_d (defx1);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_d (defx2);
	gel_matrixw_set_index (m, 2, 0) = gel_makenum_d (defy1);
	gel_matrixw_set_index (m, 3, 0) = gel_makenum_d (defy2);

	return n;
}

static void
plot_functions (void)
{
	char *colors[] = {
		"darkblue",
		"darkgreen",
		"darkred",
		"magenta",
		"black",
		"darkorange",
		"blue",
		"green",
		"red",
		"brown",
		NULL };
	int i;

	ensure_window ();

	plot_in_progress ++;

	plot_window_setup ();

	if (evalnode_hook != NULL)
		(*evalnode_hook)();

	clear_graph ();

	/* sanity */
	if (plotx2 == plotx1)
		plotx2 = plotx1 + 0.00000001;
	/* sanity */
	if (ploty2 == ploty1)
		ploty2 = ploty1 + 0.00000001;

	plot_maxy = - G_MAXDOUBLE/2;
	plot_miny = G_MAXDOUBLE/2;

	plot_setup_axis ();

	if G_UNLIKELY (plot_arg == NULL) {
		plot_ctx = eval_get_context ();
	}
	if G_UNLIKELY (plot_arg == NULL) {
		mpw_t xx;
		mpw_init (xx);
		plot_arg = gel_makenum_use (xx);
	}

	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		GdkColor color;
		char *label;

		line_data[i] = GTK_PLOT_DATA
			(gtk_plot_data_new_function (plot_func_data));
		gtk_plot_add_data (GTK_PLOT (line_plot),
				   line_data[i]);

		gtk_widget_show (GTK_WIDGET (line_data[i]));

		gdk_color_parse (colors[i], &color);
		gdk_color_alloc (gdk_colormap_get_system (), &color); 
		gtk_plot_data_set_line_attributes (line_data[i],
						   GTK_PLOT_LINE_SOLID,
						   0, 0, 2, &color);

		label = label_func (i, plot_func[i], colors[i]);
		gtk_plot_data_set_legend (line_data[i], label);
		g_free (label);
	}

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	/* could be whacked by closing the window or some such */
	if (plot_canvas != NULL)
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));

	plot_in_progress --;

	plot_window_setup ();
}

/*exact answer callback*/
static void
double_spin_cb(GtkAdjustment *adj, double *data)
{
	*data = adj->value;
}

static void
entry_activate (void)
{
	if (plot_dialog != NULL)
		gtk_dialog_response (GTK_DIALOG (plot_dialog),
				     RESPONSE_PLOT);
}

static GtkWidget *
create_plot_dialog (void)
{
	GtkWidget *mainbox, *frame;
	GtkWidget *box;
	GtkWidget *b, *w;
	GtkWidget *notebook;
	GtkAdjustment *adj;
	int i;

	notebook = gtk_notebook_new ();
	
	mainbox = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (mainbox), GNOME_PAD);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  mainbox,
				  gtk_label_new (_("Function line plot")));
	
	frame = gtk_frame_new (_("Function/Expressions"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);
	w = gtk_label_new (_("Type in function names or expressions involving "
			     "the x variable in the boxes below to graph "
			     "them"));
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	for (i = 0; i < MAXFUNC; i++) {
		b = gtk_hbox_new (FALSE, GNOME_PAD);
		gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

		plot_entries[i] = gtk_entry_new ();
		g_signal_connect (G_OBJECT (plot_entries[i]), "activate",
				  entry_activate, NULL);
		gtk_box_pack_start (GTK_BOX (b), plot_entries[i], TRUE, TRUE, 0);

		plot_entries_status[i] = gtk_image_new ();
		gtk_box_pack_start (GTK_BOX (b), plot_entries_status[i], FALSE, FALSE, 0);
	}

	frame = gtk_frame_new (_("Plot Window"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_vbox_new(FALSE,GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GNOME_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/*
	 * X range
	 */
	b = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);
	w = gtk_label_new(_("X from:"));
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *)gtk_adjustment_new (spinx1,
						   -G_MAXFLOAT,
						   G_MAXFLOAT,
						   1,
						   10,
						   100);
	w = gtk_spin_button_new (adj, 1.0, 5);
	g_signal_connect (G_OBJECT (w), "activate", entry_activate, NULL);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (double_spin_cb), &spinx1);

	w = gtk_label_new(_("to:"));
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *)gtk_adjustment_new (spinx2,
						   -G_MAXFLOAT,
						   G_MAXFLOAT,
						   1,
						   10,
						   100);
	w = gtk_spin_button_new (adj, 1.0, 5);
	g_signal_connect (G_OBJECT (w), "activate", entry_activate, NULL);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (double_spin_cb), &spinx2);

	/*
	 * Y range
	 */
	b = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);
	w = gtk_label_new(_("Y from:"));
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *)gtk_adjustment_new (spiny1,
						   -G_MAXFLOAT,
						   G_MAXFLOAT,
						   1,
						   10,
						   100);
	w = gtk_spin_button_new (adj, 1.0, 5);
	g_signal_connect (G_OBJECT (w), "activate", entry_activate, NULL);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (double_spin_cb), &spiny1);

	w = gtk_label_new(_("to:"));
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *)gtk_adjustment_new (spiny2,
						   -G_MAXFLOAT,
						   G_MAXFLOAT,
						   1,
						   10,
						   100);
	w = gtk_spin_button_new (adj, 1.0, 5);
	g_signal_connect (G_OBJECT (w), "activate", entry_activate, NULL);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (double_spin_cb), &spiny2);

	return notebook;
}

static gboolean
is_letter_or_underscore (char l)
{
	if ((l >= 'a' && l <= 'z') ||
	    (l >= 'A' && l <= 'Z') ||
	    l == '_')
		return TRUE;
	else
		return FALSE;
}

static gboolean
is_letter_underscore_or_number (char l)
{
	if ((l >= 'a' && l <= 'z') ||
	    (l >= 'A' && l <= 'Z') ||
	    (l >= '0' && l <= '9') ||
	    l == '_')
		return TRUE;
	else
		return FALSE;
}

static gboolean
is_identifier (const char *e)
{
	int i;
	if ( ! is_letter_or_underscore (e[0]))
		return FALSE;
	for (i = 1; e[i] != '\0'; i++) {
		if ( ! is_letter_underscore_or_number (e[i]))
			return FALSE;
	}
	return TRUE;
}

static GelEFunc *
function_from_expression (const char *e, gboolean *ex)
{
	GelEFunc *f = NULL;
	GelETree *value;
	char *ce;

	if (ve_string_empty (e))
		return NULL;

	ce = g_strstrip (g_strdup (e));
	if (is_identifier (ce) && strcmp (ce, "x") != 0) {
		f = d_lookup_global (d_intern (ce));
		g_free (ce);
		if (f != NULL) {
			f = d_copyfunc (f);
			f->context = -1;
		} else {
			*ex = TRUE;
		}
		return f;
	}

	value = gel_parseexp (ce,
			      NULL /* infile */,
			      FALSE /* exec_commands */,
			      FALSE /* testparse */,
			      NULL /* finished */,
			      NULL /* dirprefix */);
	g_free (ce);

	/* FIXME: if "x" not used try to evaluate and if it returns a function use that */

	if (value != NULL) {
		f = d_makeufunc (NULL /* id */,
				 value,
				 g_slist_append (NULL, d_intern ("x")),
				 1,
				 NULL /* extra_dict */);
	}

	if (f == NULL)
		*ex = TRUE;

	return f;
}

static void
plot_from_dialog (void)
{
	int funcs = 0;
	GelEFunc *func[MAXFUNC] = { NULL };
	double x1, x2, y1, y2;
	int i;
	gboolean last_info;
	gboolean last_error;
	const char *error_to_print = NULL;

	last_info = genius_setup.info_box;
	last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;

	for (i = 0; i < MAXFUNC; i++) {
		GelEFunc *f;
		gboolean ex = FALSE;
		const char *str = gtk_entry_get_text (GTK_ENTRY (plot_entries[i]));
		f = function_from_expression (str, &ex);
		if (f != NULL) {
			gtk_image_set_from_stock
				(GTK_IMAGE (plot_entries_status[i]),
				 GTK_STOCK_YES,
				 GTK_ICON_SIZE_MENU);
			func[funcs++] = f;
		} else if (ex) {
			gtk_image_set_from_stock
				(GTK_IMAGE (plot_entries_status[i]),
				 GTK_STOCK_DIALOG_WARNING,
				 GTK_ICON_SIZE_MENU);
		} else {
			gtk_image_set_from_pixbuf
				(GTK_IMAGE (plot_entries_status[i]),
				 NULL);
		}
	}

	if (funcs == 0) {
		error_to_print = _("No functions to plot or no functions "
				   "could be parsed");
		goto whack_copied_funcs;
	}

	x1 = spinx1;
	x2 = spinx2;
	y1 = spiny1;
	y2 = spiny2;

	if (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if (x1 == x2) {
		error_to_print = _("Invalid X range");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = _("Invalid Y range");
		goto whack_copied_funcs;
	}

	plotx1 = x1;
	plotx2 = x2;
	ploty1 = y1;
	ploty2 = y2;

	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		d_freefunc (plot_func[i]);
		plot_func[i] = NULL;
	}

	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		plot_func[i] = func[i];
		func[i] = NULL;
	}

	plot_functions ();

	if (interrupted)
		interrupted = FALSE;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	return;

whack_copied_funcs:
	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		d_freefunc (func[i]);
		func[i] = NULL;
	}

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	if (error_to_print != NULL)
		genius_display_error (genius_window, error_to_print);
}

static void
plot_dialog_response (GtkWidget *w, int response, gpointer data)
{
	if (response == GTK_RESPONSE_CLOSE ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		gtk_widget_destroy (plot_dialog);
	} else if (response == RESPONSE_PLOT) {
		plot_from_dialog ();
	}
}

void
genius_lineplot_dialog (void)
{
	GtkWidget *insides;

	if (plot_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (plot_dialog));
		return;
	}

	plot_dialog = gtk_dialog_new_with_buttons
		(_("Create Line Plot") /* title */,
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_CLOSE,
		 GTK_RESPONSE_CLOSE,
		 _("_Plot"),
		 RESPONSE_PLOT,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (plot_dialog),
					 RESPONSE_PLOT);

	gtk_dialog_set_has_separator (GTK_DIALOG (plot_dialog), FALSE);
	g_signal_connect (G_OBJECT (plot_dialog),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &plot_dialog);
	g_signal_connect (G_OBJECT (plot_dialog),
			  "response",
			  G_CALLBACK (plot_dialog_response),
			  NULL);

	insides = create_plot_dialog ();

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (plot_dialog)->vbox),
			    insides, TRUE, TRUE, 0);

	gtk_widget_show_all (plot_dialog);
	gtk_widget_grab_focus (plot_entries[0]);
}

static GelETree *
LinePlot_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2;
	int funcs = 0;
	GelEFunc *func[MAXFUNC] = { NULL };
	int i;

	for (i = 0;
	     i < MAXFUNC && a[i] != NULL && a[i]->type == FUNCTION_NODE;
	     i++) {
		func[funcs] = d_copyfunc (a[i]->func.func);
		func[funcs]->context = -1;
		funcs++;
	}

	if (a[i] != NULL && a[i]->type == FUNCTION_NODE) {
		gel_errorout (_("%s: only up to 10 functions supported"), "LinePlot");
		goto whack_copied_funcs;
	}

	if (funcs == 0) {
		gel_errorout (_("%s: argument not a function"), "LinePlot");
		goto whack_copied_funcs;
	}

	/* Defaults */
	x1 = defx1;
	x2 = defx2;
	y1 = defy1;
	y2 = defy2;

	if (a[i] != NULL) {
		if (a[i]->type == MATRIX_NODE) {
			if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
				goto whack_copied_funcs;
			i++;
		} else {
			GET_DOUBLE(x1,i);
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2,i);
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1,i);
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2,i);
						i++;
					}
				}
			}
			/* FIXME: what about errors */
			if (error_num != 0) {
				error_num = 0;
				goto whack_copied_funcs;
			}
		}
	}

	if (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if (x1 == x2) {
		gel_errorout (_("%s: invalid X range"), "LinePlot");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "LinePlot");
		goto whack_copied_funcs;
	}

	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		d_freefunc (plot_func[i]);
		plot_func[i] = NULL;
	}

	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		plot_func[i] = func[i];
		func[i] = NULL;
	}

	plotx1 = x1;
	plotx2 = x2;
	ploty1 = y1;
	ploty2 = y2;

	plot_functions ();

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_funcs:
	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		d_freefunc (func[i]);
		func[i] = NULL;
	}

	return NULL;
}

static GelETree *
set_LinePlotWindow (GelETree * a)
{
	double x1, x2, y1, y2;
	if ( ! get_limits_from_matrix (a, &x1, &x2, &y1, &y2))
		return NULL;

	defx1 = x1;
	defx2 = x2;
	defy1 = y1;
	defy2 = y2;

	return make_matrix_from_limits ();
}

static GelETree *
get_LinePlotWindow (void)
{
	return make_matrix_from_limits ();
}

void
gel_add_graph_functions (void)
{
	GelEFunc *f;
	GelToken *id;

	new_category ("plotting", _("Plotting"));

	/* FIXME: add more help fields */
#define FUNC(name,args,argn,category,desc) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), name ## _op, args)); \
	d_add_named_args (f, argn); \
	add_category ( #name , category); \
	add_description ( #name , desc);
#define VFUNC(name,args,argn,category,desc) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), name ## _op, args)); \
	d_add_named_args (f, argn); \
	f->vararg = TRUE; \
	add_category ( #name , category); \
	add_description ( #name , desc);
#define ALIAS(name,args,aliasfor) \
	d_addfunc (d_makebifunc (d_intern ( #name ), aliasfor ## _op, args)); \
	add_alias ( #aliasfor , #name );
#define VALIAS(name,args,aliasfor) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), aliasfor ## _op, args)); \
	f->vararg = TRUE; \
	add_alias ( #aliasfor , #name );
#define PARAMETER(name,desc) \
	id = d_intern ( #name ); \
	id->parameter = 1; \
	id->built_in_parameter = 1; \
	id->data1 = set_ ## name; \
	id->data2 = get_ ## name; \
	add_category ( #name , "parameters"); \
	add_description ( #name , desc); \
	/* bogus value */ \
	d_addfunc_global (d_makevfunc (id, gel_makenum_null()));

	VFUNC (LinePlot, 1, "", "plotting", _("Plot a function with a line (very rudimentary).  First come the functions (up to 10) then optionally limits as x1,x2,y1,y2"));

	PARAMETER (LinePlotWindow, _("Plotting window (limits) as a 4-vector of the form [x1,x2,y1,y2]"));
}
