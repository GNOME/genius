/* GENIUS Calculator
 * Copyright (C) 2003-2009 Jiri (George) Lebl
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

#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

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
#include "matop.h"

#include "gnome-genius.h"

#include "graphing.h"

#define MAXFUNC 10

static GtkWidget *graph_window = NULL;
static GtkWidget *plot_canvas = NULL;
static GtkWidget *plot_dialog = NULL;
static GtkWidget *plot_notebook = NULL;
static GtkWidget *function_notebook = NULL;

static GtkWidget *solver_dialog = NULL;
gboolean solver_dialog_slopefield = TRUE;

static GtkWidget *plot_zoomout_item = NULL;
static GtkWidget *plot_zoomin_item = NULL;
static GtkWidget *plot_zoomfit_item = NULL;
static GtkWidget *plot_resetzoom_item = NULL;
static GtkWidget *plot_print_item = NULL;
static GtkWidget *plot_exportps_item = NULL;
static GtkWidget *plot_exporteps_item = NULL;
static GtkWidget *plot_exportpng_item = NULL;

static GtkWidget *view_menu_item = NULL;
static GtkWidget *solver_menu_item = NULL;

enum {
	MODE_LINEPLOT,
	MODE_LINEPLOT_PARAMETRIC,
	MODE_LINEPLOT_SLOPEFIELD,
	MODE_LINEPLOT_VECTORFIELD,
	MODE_SURFACE
} plot_mode = MODE_LINEPLOT;

static GtkPlotCanvasChild *plot_child = NULL;

/* Smallest plot window */
#define MINPLOT (1e-10)

/*
   plot (lineplot)
 */
static GtkWidget *line_plot = NULL;

static GtkPlotData *line_data[MAXFUNC] = { NULL };
static GtkPlotData *parametric_data = NULL;
static GtkPlotData *slopefield_data = NULL;
static GtkPlotData *vectorfield_data = NULL;

static GtkWidget *plot_entries[MAXFUNC] = { NULL };
static GtkWidget *plot_entries_status[MAXFUNC] = { NULL };

static GtkWidget *parametric_entry_x = NULL;
static GtkWidget *parametric_entry_y = NULL;
static GtkWidget *parametric_entry_z = NULL;
static GtkWidget *parametric_status_x = NULL;
static GtkWidget *parametric_status_y = NULL;
static GtkWidget *parametric_status_z = NULL;

static GtkWidget *slopefield_entry = NULL;
static GtkWidget *slopefield_status = NULL;

static GtkWidget *solver_x_sb = NULL;
static GtkWidget *solver_y_sb = NULL;

static GtkWidget *vectorfield_entry_x = NULL;
static GtkWidget *vectorfield_status_x = NULL;
static GtkWidget *vectorfield_entry_y = NULL;
static GtkWidget *vectorfield_status_y = NULL;

static GSList *solutions_list = NULL;

static double spinx1 = -10;
static double spinx2 = 10;
static double spiny1 = -10;
static double spiny2 = 10;
static double spint1 = 0.0;
static double spint2 = 1.0;
static double spintinc = 0.01;

static int spinSVtick = 20;
static int spinSHtick = 20;

static int spinVVtick = 20;
static int spinVHtick = 20;

static double defx1 = -10;
static double defx2 = 10;
static double defy1 = -10;
static double defy2 = 10;
static double deft1 = 0.0;
static double deft2 = 1.0;
static double deftinc = 0.01;

static gboolean lineplot_draw_legends = TRUE;
static gboolean lineplot_draw_legends_cb = TRUE;
static gboolean lineplot_draw_legends_parameter = TRUE;
static gboolean vectorfield_normalize_arrow_length = FALSE;
static gboolean vectorfield_normalize_arrow_length_cb = FALSE;
static gboolean vectorfield_normalize_arrow_length_parameter = FALSE;

/* Replotting info */
static GelEFunc *plot_func[MAXFUNC] = { NULL };
static char *plot_func_name[MAXFUNC] = { NULL };
static GelEFunc *parametric_func_x = NULL;
static GelEFunc *parametric_func_y = NULL;
static GelEFunc *parametric_func_z = NULL;
static char *parametric_name = NULL;
static GelEFunc *slopefield_func = NULL;
static char *slopefield_name = NULL;
static GelEFunc *vectorfield_func_x = NULL;
static GelEFunc *vectorfield_func_y = NULL;
static char *vectorfield_name_x = NULL;
static char *vectorfield_name_y = NULL;
static double plotx1 = -10;
static double plotx2 = 10;
static double ploty1 = -10;
static double ploty2 = 10;
static double plott1 = 0.0;
static double plott2 = 1.0;
static double plottinc = 0.01;

/* for the zoom reset */
static double reset_plotx1 = -10;
static double reset_plotx2 = 10;
static double reset_ploty1 = -10;
static double reset_ploty2 = 10;

static int plotVtick = 20;
static int plotHtick = 20;

static double *plot_points_x = NULL;
static double *plot_points_y = NULL;
static double *plot_points_dx = NULL;
static double *plot_points_dy = NULL;

static int plot_points_num = 0;

static double solver_xinc = 0.1;
static double solver_tinc = 0.1;
static double solver_tlen = 5;
static double solver_x = 0.0;
static double solver_y = 0.0;

/*
   Surface
 */
static GtkWidget *surface_plot = NULL;

static GtkPlotData *surface_data = NULL;

static GtkWidget *surface_entry = NULL;
static GtkWidget *surface_entry_status = NULL;
static double surf_spinx1 = -10;
static double surf_spinx2 = 10;
static double surf_spiny1 = -10;
static double surf_spiny2 = 10;
static double surf_spinz1 = -10;
static double surf_spinz2 = 10;

static double surf_defx1 = -10;
static double surf_defx2 = 10;
static double surf_defy1 = -10;
static double surf_defy2 = 10;
static double surf_defz1 = -10;
static double surf_defz2 = 10;

/* Replotting info */
static GelEFunc *surface_func = NULL;
static char *surface_func_name = NULL;
static double surfacex1 = -10;
static double surfacex2 = 10;
static double surfacey1 = -10;
static double surfacey2 = 10;
static double surfacez1 = -10;
static double surfacez2 = 10;

/* for the zoom reset */
static double reset_surfacex1 = -10;
static double reset_surfacex2 = 10;
static double reset_surfacey1 = -10;
static double reset_surfacey2 = 10;
static double reset_surfacez1 = -10;
static double reset_surfacez2 = 10;


/* used for both */
static double plot_maxy = - G_MAXDOUBLE/2;
static double plot_miny = G_MAXDOUBLE/2;
static double plot_maxx = - G_MAXDOUBLE/2;
static double plot_minx = G_MAXDOUBLE/2;

static GelCtx *plot_ctx = NULL;
static GelETree *plot_arg = NULL;
static GelETree *plot_arg2 = NULL;
static GelETree *plot_arg3 = NULL;

static int plot_in_progress = 0;
static gboolean whack_window_after_plot = FALSE;

static void plot_axis (void);

/* lineplots */
static void plot_functions (gboolean do_window_present);

/* surfaces */
static void plot_surface_functions (gboolean do_window_present);

/* replot the slope/vector fields after zoom or other axis changing event */
static void replot_fields (void);

static void slopefield_draw_solution (double x, double y, double dx);
static void vectorfield_draw_solution (double x, double y, double dt, double tlen);

static GtkWidget *
create_range_spinboxes (const char *title, double *val1, GtkWidget **w1,
			double min1, double max1, double step1,
			const char *totitle, double *val2, GtkWidget **w2,
			double min2, double max2, double step2,
			const char *bytitle, double *by, GtkWidget **wb,
			double minby, double maxby, double stepby,
			GCallback activate_callback);

#define WIDTH 640
#define HEIGHT 480
#define ASPECT ((double)HEIGHT/(double)WIDTH)

#define PROPORTION 0.85
#define PROPORTION3D 0.80
#define PROPORTION_OFFSET 0.075
#define PROPORTION3D_OFFSET 0.1

#include "funclibhelper.cP"

enum {
	RESPONSE_STOP = 1,
	RESPONSE_PLOT,
	RESPONSE_CLEAR
};

static void
plot_window_setup (void)
{
	if (graph_window != NULL) {
		if (plot_in_progress == 0 &&
		    whack_window_after_plot) {
			gtk_widget_destroy (graph_window);
			whack_window_after_plot = FALSE;
			return;
		}

		if (plot_in_progress)
			genius_setup_window_cursor (plot_canvas, GDK_WATCH);
		else
			genius_unsetup_window_cursor (plot_canvas);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (graph_window),
						   RESPONSE_STOP, plot_in_progress);
		gtk_widget_set_sensitive (plot_zoomout_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_zoomin_item, ! plot_in_progress);

		if (plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
		    plot_mode == MODE_LINEPLOT_VECTORFIELD) {
			gtk_widget_set_sensitive (plot_zoomfit_item, FALSE);
		} else {
			gtk_widget_set_sensitive (plot_zoomfit_item, ! plot_in_progress);
		}

		gtk_widget_set_sensitive (plot_resetzoom_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_print_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_exportps_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_exporteps_item, ! plot_in_progress);
		gtk_widget_set_sensitive (plot_exportpng_item, ! plot_in_progress);
		gtk_widget_set_sensitive (view_menu_item, ! plot_in_progress);
		gtk_widget_set_sensitive (solver_menu_item, ! plot_in_progress);

		if (plot_mode == MODE_SURFACE) {
			gtk_widget_show (view_menu_item);
		} else {
			gtk_widget_hide (view_menu_item);
		}

		if (plot_mode == MODE_LINEPLOT_SLOPEFIELD) {
			if ( ! solver_dialog_slopefield &&
			    solver_dialog != NULL) {
				gtk_widget_destroy (solver_dialog);
			}
			gtk_widget_show (solver_menu_item);
		} else if (plot_mode == MODE_LINEPLOT_VECTORFIELD) {
			if (solver_dialog_slopefield &&
			    solver_dialog != NULL) {
				gtk_widget_destroy (solver_dialog);
			}
			gtk_widget_show (solver_menu_item);
		} else {
			if (solver_dialog != NULL) {
				gtk_widget_destroy (solver_dialog);
			}
			gtk_widget_hide (solver_menu_item);
		}
	}
}

static void
show_z_axis (gboolean do_show)
{
	GtkPlotAxis *zx, *zy;

	zx = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot),
				  GTK_PLOT_SIDE_ZX);

	zy = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot),
				  GTK_PLOT_SIDE_ZY);

	if (do_show) {
		gtk_plot_axis_show_labels (zy,
					   GTK_PLOT_LABEL_OUT);
		gtk_plot_axis_show_labels (zx,
					   GTK_PLOT_LABEL_OUT);
		gtk_plot_axis_show_ticks (zy,
					  GTK_PLOT_TICKS_OUT,
					  GTK_PLOT_TICKS_OUT);
		gtk_plot_axis_show_ticks (zx,
					  GTK_PLOT_TICKS_OUT,
					  GTK_PLOT_TICKS_OUT);
		gtk_plot_axis_show_title (zy);
		gtk_plot_axis_show_title (zx);
	} else {
		gtk_plot_axis_show_labels (zy,
					   GTK_PLOT_LABEL_NONE);
		gtk_plot_axis_show_labels (zx,
					   GTK_PLOT_LABEL_NONE);
		gtk_plot_axis_show_ticks (zy,
					  GTK_PLOT_TICKS_NONE,
					  GTK_PLOT_TICKS_NONE);
		gtk_plot_axis_show_ticks (zx,
					  GTK_PLOT_TICKS_NONE,
					  GTK_PLOT_TICKS_NONE);
		gtk_plot_axis_hide_title (zx);
		gtk_plot_axis_hide_title (zy);
	}
}

static void
rotate_x_cb (GtkWidget *button, gpointer data)
{
	int rot = GPOINTER_TO_INT (data);

	gtk_plot3d_rotate_x (GTK_PLOT3D (surface_plot), rot);

	show_z_axis (TRUE);

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));
}

static void
rotate_y_cb (GtkWidget *button, gpointer data)
{
	int rot = GPOINTER_TO_INT (data);

	gtk_plot3d_rotate_y (GTK_PLOT3D (surface_plot), rot);

	show_z_axis (TRUE);

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));
}

static void
rotate_z_cb (GtkWidget *button, gpointer data)
{
	int rot = GPOINTER_TO_INT (data);

	gtk_plot3d_rotate_z (GTK_PLOT3D (surface_plot), rot);

	/* don't neccessarily show the z axis here, if we're in top
	   view this could be legitimate */

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));
}

static void
rotate_cb (GtkWidget *item, gpointer data)
{
	GtkWidget *req = NULL;
	GtkWidget *hbox, *w, *b;
        GtkSizeGroup *sg;

	if (surface_plot == NULL)
		return;

	req = gtk_dialog_new_with_buttons
		(_("Rotate") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 GTK_DIALOG_MODAL /* flags */,
		 GTK_STOCK_CLOSE,
		 GTK_RESPONSE_CLOSE,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (req),
					 GTK_RESPONSE_CLOSE);

	gtk_dialog_set_has_separator (GTK_DIALOG (req), FALSE);

	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* X dir */

	hbox = gtk_hbox_new (FALSE, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (req)->vbox),
			    hbox, TRUE, TRUE, 0);

	w = gtk_label_new (_("Rotate X: "));
	gtk_size_group_add_widget (sg, w);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_x_cb),
			  GINT_TO_POINTER (360-10));
	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_x_cb),
			  GINT_TO_POINTER (10));

	/* Y dir */

	hbox = gtk_hbox_new (FALSE, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (req)->vbox),
			    hbox, TRUE, TRUE, 0);

	w = gtk_label_new (_("Rotate Y: "));
	gtk_size_group_add_widget (sg, w);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_y_cb),
			  GINT_TO_POINTER (360-10));
	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_y_cb),
			  GINT_TO_POINTER (10));

	/* Z dir */

	hbox = gtk_hbox_new (FALSE, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (req)->vbox),
			    hbox, TRUE, TRUE, 0);

	w = gtk_label_new (_("Rotate Z: "));
	gtk_size_group_add_widget (sg, w);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_z_cb),
			  GINT_TO_POINTER (360-10));
	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_z_cb),
			  GINT_TO_POINTER (10));

	g_signal_connect (G_OBJECT (req), "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &req);

	gtk_widget_show_all (req);

	gtk_dialog_run (GTK_DIALOG (req));
	gtk_widget_destroy (req);
}

static void
reset_angles_cb (GtkWidget *button, gpointer data)
{
	if (surface_plot != NULL) {
		gtk_plot3d_reset_angles (GTK_PLOT3D (surface_plot));
		gtk_plot3d_rotate_x (GTK_PLOT3D (surface_plot), 60.0);
		gtk_plot3d_rotate_z (GTK_PLOT3D (surface_plot), 30.0);

		show_z_axis (TRUE);

		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));
	}
}

static void
top_view_cb (GtkWidget *button, gpointer data)
{
	if (surface_plot != NULL) {
		gtk_plot3d_reset_angles (GTK_PLOT3D (surface_plot));
		/*gtk_plot3d_rotate_y (GTK_PLOT3D (surface_plot), 90.0);*/

		show_z_axis (FALSE);

		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));
	}
}

static gboolean
graph_window_delete_event (GtkWidget *w, gpointer data)
{
	if (plot_in_progress > 0) {
		interrupted = TRUE;
		whack_window_after_plot = TRUE;
		return TRUE;
	} else {
		return FALSE;
	}
}


static void
graph_window_response (GtkWidget *w, int response, gpointer data)
{
	if (response == GTK_RESPONSE_CLOSE ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		if (plot_in_progress > 0) {
			interrupted = TRUE;
			whack_window_after_plot = TRUE;
		} else {
			gtk_widget_destroy (graph_window);
		}
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

	hbox = gtk_hbox_new (FALSE, GENIUS_PAD);
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
	if (plot_canvas != NULL)
		ret = gtk_plot_canvas_export_ps (GTK_PLOT_CANVAS (plot_canvas),
						 tmpfile,
						 GTK_PLOT_LANDSCAPE,
						 FALSE /* epsflag */,
						 GTK_PLOT_LETTER);
	else
		ret = FALSE;

	/* need this for some reason */
	if (plot_canvas != NULL) {
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

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
		errno = 0;
		if (system (cmdstring) < 0) {
			char *err = g_strdup_printf (
			_("Printing failed: %s"), 
				      g_strerror (errno));
			genius_display_error (graph_window, err);
			g_free (err);
		}


		g_free (cmdstring);

	}

	plot_in_progress --;
	plot_window_setup ();

	close (fd);
	unlink (tmpfile);
}

static char *last_export_dir = NULL;

static void
really_export_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	gboolean ret;
	gboolean eps;
	char tmpfile[] = "/tmp/genius-ps-XXXXXX";
	char *file_to_write = NULL;
	int fd = -1;

	eps = GPOINTER_TO_INT (data);

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		/* FIXME: don't want to deal with modality issues right now */
		gtk_widget_set_sensitive (graph_window, TRUE);
		return;
	}

	s = g_strdup (gtk_file_chooser_get_filename (fs));

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

	g_free (last_export_dir);
	last_export_dir = gtk_file_chooser_get_current_folder (fs);

	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (graph_window, TRUE);

	file_to_write = s;
	if (eps && ve_is_prog_in_path ("ps2epsi")) {
		fd = g_mkstemp (tmpfile);
		/* FIXME: tell about errors ?*/
		if (fd >= 0) {
			file_to_write = tmpfile;
		}
	}

	plot_in_progress ++;
	plot_window_setup ();

	/* FIXME: There should be some options about size and stuff */
	if (plot_canvas != NULL)
		ret = gtk_plot_canvas_export_ps_with_size
			(GTK_PLOT_CANVAS (plot_canvas),
			 file_to_write,
			 GTK_PLOT_PORTRAIT,
			 eps /* epsflag */,
			 GTK_PLOT_PSPOINTS,
			 400, ASPECT * 400);
	else
		ret = FALSE;

	/* need this for some reason */
	if (plot_canvas != NULL) {
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

	/* If we used a temporary file, now use ps2epsi */
	if (fd >= 0) {
		int status;
		char *qs = g_shell_quote (s);
		char *cmd = g_strdup_printf ("ps2epsi %s %s", tmpfile, qs);
		if ( ! g_spawn_command_line_sync  (cmd,
						   NULL /*stdout*/,
						   NULL /*stderr*/,
						   &status,
						   NULL /* error */)) {
			status = -1;
		}
		close (fd);
		if (status == 0) {
			unlink (tmpfile);
		} else {
			/* EEK, couldn't run ps2epsi for some reason */
			rename (tmpfile, s);
		}
		g_free (cmd);
		g_free (qs);
	}

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

static void
really_export_png_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	GdkPixbuf *pix;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		/* FIXME: don't want to deal with modality issues right now */
		gtk_widget_set_sensitive (graph_window, TRUE);
		return;
	}

	s = g_strdup (gtk_file_chooser_get_filename (fs));
	if (s == NULL)
		return;
	base = g_path_get_basename (s);
	if (base != NULL && base[0] != '\0' &&
	    strchr (base, '.') == NULL) {
		char *n = g_strconcat (s, ".png", NULL);
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

	g_free (last_export_dir);
	last_export_dir = gtk_file_chooser_get_current_folder (fs);

	gtk_widget_destroy (GTK_WIDGET (fs));
	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (graph_window, TRUE);


	/* sanity */
	if (GTK_PLOT_CANVAS (plot_canvas)->pixmap == NULL) {
		genius_display_error (graph_window, _("Export failed"));
		return;
	}
	pix = gdk_pixbuf_get_from_drawable
		(NULL /* dest */,
		 GTK_PLOT_CANVAS (plot_canvas)->pixmap,
		 NULL /* cmap */,
		 0 /* src x */, 0 /* src y */,
		 0 /* dest x */, 0 /* dest y */,
		 GTK_PLOT_CANVAS (plot_canvas)->pixmap_width,
		 GTK_PLOT_CANVAS (plot_canvas)->pixmap_height);

	if (pix == NULL ||
	    ! gdk_pixbuf_save (pix, s, "png", NULL /* error */, NULL)) {
		if (pix != NULL)
			g_object_unref (G_OBJECT (pix));
		g_free (s);
		genius_display_error (graph_window, _("Export failed"));
		return;
	}

	g_object_unref (G_OBJECT (pix));
	g_free (s);
}

enum {
	EXPORT_PS,
	EXPORT_EPS,
	EXPORT_PNG
};

static void
do_export_cb (int export_type)
{
	static GtkWidget *fs = NULL;
	GtkFileFilter *filter_ps;
	GtkFileFilter *filter_all;
	const char *title;

	if (fs != NULL) {
		gtk_window_present (GTK_WINDOW (fs));
		return;
	}

	/* FIXME: don't want to deal with modality issues right now */
	gtk_widget_set_sensitive (graph_window, FALSE);

	if (export_type == EXPORT_EPS)
		title = _("Export encapsulated postscript");
	else if (export_type == EXPORT_PS)
		title = _("Export postscript");
	else if (export_type == EXPORT_PNG)
		title = _("Export PNG");
	else
		/* should never happen */
		title = "Export ???";

	fs = gtk_file_chooser_dialog_new (title,
					  GTK_WINDOW (graph_window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					  NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fs), TRUE);


	filter_ps = gtk_file_filter_new ();
	if (export_type == EXPORT_EPS) {
		gtk_file_filter_set_name (filter_ps, _("EPS files"));
		gtk_file_filter_add_pattern (filter_ps, "*.eps");
		gtk_file_filter_add_pattern (filter_ps, "*.EPS");
	} else if (export_type == EXPORT_PS) {
		gtk_file_filter_set_name (filter_ps, _("PS files"));
		gtk_file_filter_add_pattern (filter_ps, "*.ps");
		gtk_file_filter_add_pattern (filter_ps, "*.PS");
	} else if (export_type == EXPORT_PNG) {
		gtk_file_filter_set_name (filter_ps, _("PNG files"));
		gtk_file_filter_add_pattern (filter_ps, "*.png");
		gtk_file_filter_add_pattern (filter_ps, "*.PNG");
	}

	filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter_all, _("All files"));
	gtk_file_filter_add_pattern (filter_all, "*");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_ps);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), filter_all);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fs), filter_ps);

	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	if (export_type == EXPORT_EPS) {
		g_signal_connect (G_OBJECT (fs), "response",
				  G_CALLBACK (really_export_cb),
				  GINT_TO_POINTER (TRUE /*eps*/));
	} else if (export_type == EXPORT_PS) {
		g_signal_connect (G_OBJECT (fs), "response",
				  G_CALLBACK (really_export_cb),
				  GINT_TO_POINTER (FALSE /*eps*/));
	} else if (export_type == EXPORT_PNG) {
		g_signal_connect (G_OBJECT (fs), "response",
				  G_CALLBACK (really_export_png_cb),
				  NULL);
	}

	if (last_export_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_export_dir);
	}

	gtk_widget_show (fs);
}

static void
plot_exportps_cb (void)
{
	do_export_cb (EXPORT_PS);
}

static void
plot_exporteps_cb (void)
{
	do_export_cb (EXPORT_EPS);
}

static void
plot_exportpng_cb (void)
{
	do_export_cb (EXPORT_PNG);
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

		if (plot_mode == MODE_LINEPLOT ||
		    plot_mode == MODE_LINEPLOT_PARAMETRIC ||
		    plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
		    plot_mode == MODE_LINEPLOT_VECTORFIELD) {
			len = plotx2 - plotx1;
			plotx2 -= len/4.0;
			plotx1 += len/4.0;

			len = ploty2 - ploty1;
			ploty2 -= len/4.0;
			ploty1 += len/4.0;
		} else if (plot_mode == MODE_SURFACE) {
			len = surfacex2 - surfacex1;
			surfacex2 -= len/4.0;
			surfacex1 += len/4.0;

			len = surfacey2 - surfacey1;
			surfacey2 -= len/4.0;
			surfacey1 += len/4.0;

			len = surfacez2 - surfacez1;
			surfacez2 -= len/4.0;
			surfacez1 += len/4.0;
		}

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

		if (plot_mode == MODE_LINEPLOT ||
		    plot_mode == MODE_LINEPLOT_PARAMETRIC ||
		    plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
		    plot_mode == MODE_LINEPLOT_VECTORFIELD) {
			len = plotx2 - plotx1;
			plotx2 += len/2.0;
			plotx1 -= len/2.0;

			len = ploty2 - ploty1;
			ploty2 += len/2.0;
			ploty1 -= len/2.0;
		} else if (plot_mode == MODE_SURFACE) {
			len = surfacex2 - surfacex1;
			surfacex2 += len/2.0;
			surfacex1 -= len/2.0;

			len = surfacey2 - surfacey1;
			surfacey2 += len/2.0;
			surfacey1 -= len/2.0;

			len = surfacez2 - surfacez1;
			surfacez2 += len/2.0;
			surfacez1 -= len/2.0;
		}

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
	if (plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
	    plot_mode == MODE_LINEPLOT_VECTORFIELD) {
		/* No zoom to fit during slopefield/vectorfield plots */
		return;
	}

	if (plot_in_progress == 0) {
		double size;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		size = plot_maxy - plot_miny;
		if (size <= 0)
			size = 1.0;

		if (plot_mode == MODE_LINEPLOT) {
			ploty1 = plot_miny - size * 0.05;
			ploty2 = plot_maxy + size * 0.05;

			/* sanity */
			if (ploty2 < ploty1)
				ploty2 = ploty1 + 0.1;

			/* sanity */
			if (ploty1 < -(G_MAXDOUBLE/2))
				ploty1 = -(G_MAXDOUBLE/2);
			if (ploty2 > (G_MAXDOUBLE/2))
				ploty2 = (G_MAXDOUBLE/2);

		} else if (plot_mode == MODE_LINEPLOT_PARAMETRIC) {
			double sizex;
			sizex = plot_maxx - plot_minx;
			if (sizex <= 0)
				sizex = 1.0;

			plotx1 = plot_minx - sizex * 0.05;
			plotx2 = plot_maxx + sizex * 0.05;

			/* sanity */
			if (plotx2 < plotx1)
				plotx2 = plotx1 + 0.1;

			/* sanity */
			if (plotx1 < -(G_MAXDOUBLE/2))
				plotx1 = -(G_MAXDOUBLE/2);
			if (plotx2 > (G_MAXDOUBLE/2))
				plotx2 = (G_MAXDOUBLE/2);

			ploty1 = plot_miny - size * 0.05;
			ploty2 = plot_maxy + size * 0.05;

			/* sanity */
			if (ploty2 < ploty1)
				ploty2 = ploty1 + 0.1;

			/* sanity */
			if (ploty1 < -(G_MAXDOUBLE/2))
				ploty1 = -(G_MAXDOUBLE/2);
			if (ploty2 > (G_MAXDOUBLE/2))
				ploty2 = (G_MAXDOUBLE/2);

		} else if (plot_mode == MODE_SURFACE) {
			surfacez1 = plot_miny - size * 0.05;
			surfacez2 = plot_maxy + size * 0.05;

			/* sanity */
			if (surfacez2 < surfacez1)
				surfacez2 = surfacez1 + 0.1;

			/* sanity */
			if (surfacez1 < -(G_MAXDOUBLE/2))
				surfacez1 = -(G_MAXDOUBLE/2);
			if (surfacez2 > (G_MAXDOUBLE/2))
				surfacez2 = (G_MAXDOUBLE/2);
		}

		plot_axis ();

		if (interrupted)
			interrupted = FALSE;

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static void
plot_resetzoom_cb (void)
{
	if (plot_in_progress == 0) {
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		if (plot_mode == MODE_LINEPLOT ||
		    plot_mode == MODE_LINEPLOT_PARAMETRIC ||
		    plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
		    plot_mode == MODE_LINEPLOT_VECTORFIELD) {
			plotx1 = reset_plotx1;
			plotx2 = reset_plotx2;
			ploty1 = reset_ploty1;
			ploty2 = reset_ploty2;
		} else if (plot_mode == MODE_SURFACE) {
			surfacex1 = reset_surfacex1;
			surfacex2 = reset_surfacex2;
			surfacey1 = reset_surfacey1;
			surfacey2 = reset_surfacey2;
			surfacez1 = reset_surfacez1;
			surfacez2 = reset_surfacez2;
		}

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
	double len;
	double px, py, pw, ph;
	gboolean just_click = FALSE;

	if (fabs(xmin-xmax) < 0.001 ||
	    fabs(ymin-ymax) < 0.001) {
		just_click = TRUE;
	}

	/* FIXME: evil because this is the selection thingie,
	   hmmm, I dunno another way to do this though */

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

	if (plot_in_progress == 0 &&
	    line_plot != NULL &&
	    (plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
	     plot_mode == MODE_LINEPLOT_VECTORFIELD) &&
	    solver_dialog != NULL) {
		double x, y;
		len = plotx2 - plotx1;
		x = plotx1 + len * xmin;
		len = ploty2 - ploty1;
		y = ploty1 + len * ymin;

		if (solver_x_sb != NULL)
			gtk_spin_button_set_value
				(GTK_SPIN_BUTTON (solver_x_sb), x);
		if (solver_y_sb != NULL)
			gtk_spin_button_set_value
				(GTK_SPIN_BUTTON (solver_y_sb), y);

		if (plot_mode == MODE_LINEPLOT_SLOPEFIELD)
			slopefield_draw_solution (x, y, solver_xinc);
		else if (plot_mode == MODE_LINEPLOT_VECTORFIELD)
			vectorfield_draw_solution (x, y, solver_tinc,
						   solver_tlen);

		return;
	}


	/* only for line plots! */
	if (plot_in_progress == 0 && line_plot != NULL) {
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		/* just click, so zoom in */
		if (just_click) {
			len = plotx2 - plotx1;
			plotx1 += len * xmin - len / 4.0;
			plotx2 = plotx1 + len / 2.0;

			len = ploty2 - ploty1;
			ploty1 += len * ymin - len / 4.0;
			ploty2 = ploty1 + len / 2.0;
		} else {
			len = plotx2 - plotx1;
			plotx1 += len * xmin;
			plotx2 = plotx1 + (len * (xmax-xmin));

			len = ploty2 - ploty1;
			ploty1 += len * ymin;
			ploty2 = ploty1 + (len * (ymax-ymin));
		}

		/* sanity */
		if (plotx2 - plotx1 < MINPLOT)
			plotx2 = plotx1 + MINPLOT;
		/* sanity */
		if (ploty2 - ploty1 < MINPLOT)
			ploty2 = ploty1 + MINPLOT;

		plot_axis ();

		if (interrupted)
			interrupted = FALSE;

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static void
line_plot_move_about (void)
{
	if (line_plot == NULL)
		return;

	if ((plot_mode == MODE_LINEPLOT_PARAMETRIC || 
	     plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
	     plot_mode == MODE_LINEPLOT_VECTORFIELD) &&
	    lineplot_draw_legends) {
		/* move plot out of the way if we are in parametric mode and
		 * there is a legend */
		gtk_plot_move (GTK_PLOT (line_plot),
			       PROPORTION_OFFSET,
			       PROPORTION_OFFSET);
		gtk_plot_resize (GTK_PLOT (line_plot),
				 1.0-2*PROPORTION_OFFSET,
				 1.0-2*PROPORTION_OFFSET-0.05);

		gtk_plot_legends_move (GTK_PLOT (line_plot),
				       0.0,
				       1.07);
	} else {
		gtk_plot_move (GTK_PLOT (line_plot),
			       PROPORTION_OFFSET,
			       PROPORTION_OFFSET);
		gtk_plot_resize (GTK_PLOT (line_plot),
				 1.0-2*PROPORTION_OFFSET,
				 1.0-2*PROPORTION_OFFSET);
		gtk_plot_legends_move (GTK_PLOT (line_plot), 0.80, 0.05);
	}
}

static void
add_line_plot (void)
{
	GtkPlotAxis *top, *right, *bottom, *left;

	line_plot = gtk_plot_new_with_size (NULL, PROPORTION, PROPORTION);
	gtk_widget_show (line_plot);
	g_signal_connect (G_OBJECT (line_plot),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &line_plot);

	plot_child = gtk_plot_canvas_plot_new (GTK_PLOT (line_plot));
	gtk_plot_canvas_put_child (GTK_PLOT_CANVAS (plot_canvas),
				   plot_child,
				   PROPORTION_OFFSET,
				   PROPORTION_OFFSET,
				   1.0-PROPORTION_OFFSET,
				   1.0-PROPORTION_OFFSET);

	top = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_TOP);
	right = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_RIGHT);
	bottom = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_BOTTOM);
	left = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_LEFT);

	gtk_plot_axis_set_visible (top, TRUE);
	gtk_plot_axis_set_visible (right, TRUE);
	gtk_plot_grids_set_visible (GTK_PLOT (line_plot),
				    FALSE, FALSE, FALSE, FALSE);
	gtk_plot_axis_hide_title (top);
	gtk_plot_axis_hide_title (right);
	gtk_plot_axis_hide_title (left);
	gtk_plot_axis_hide_title (bottom);
	/*gtk_plot_axis_set_title (left, "Y");
	gtk_plot_axis_set_title (bottom, "X");*/
	gtk_plot_set_legends_border (GTK_PLOT (line_plot),
				     GTK_PLOT_BORDER_LINE, 3);

	line_plot_move_about ();
}

static void
add_surface_plot (void)
{
	GtkPlotAxis *xy, *xz, *yx, *yz, *zx, *zy;
	GtkPlotAxis *top, *left, *bottom;

	surface_plot = gtk_plot3d_new_with_size (NULL, PROPORTION3D, PROPORTION3D);
	gtk_widget_show (surface_plot);
	g_signal_connect (G_OBJECT (surface_plot),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &surface_plot);

	plot_child = gtk_plot_canvas_plot_new (GTK_PLOT (surface_plot));
	gtk_plot_canvas_put_child (GTK_PLOT_CANVAS (plot_canvas),
				   plot_child,
				   0.0,
				   PROPORTION3D_OFFSET,
				   0.8,
				   1.0-PROPORTION3D_OFFSET);

	xy = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot), GTK_PLOT_SIDE_XY);
	xz = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot), GTK_PLOT_SIDE_XZ);
	yx = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot), GTK_PLOT_SIDE_YX);
	yz = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot), GTK_PLOT_SIDE_YZ);
	zx = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot), GTK_PLOT_SIDE_ZX);
	zy = gtk_plot3d_get_side (GTK_PLOT3D (surface_plot), GTK_PLOT_SIDE_ZY);

	gtk_plot_axis_show_title (xy);
	gtk_plot_axis_show_title (xz);
	gtk_plot_axis_show_title (yx);
	gtk_plot_axis_show_title (yz);
	gtk_plot_axis_show_title (zx);
	gtk_plot_axis_show_title (zy);

	top = gtk_plot_get_axis (GTK_PLOT (surface_plot), GTK_PLOT_AXIS_TOP);
	bottom = gtk_plot_get_axis (GTK_PLOT (surface_plot), GTK_PLOT_AXIS_BOTTOM);
	left = gtk_plot_get_axis (GTK_PLOT (surface_plot), GTK_PLOT_AXIS_LEFT);

	gtk_plot_axis_set_title (bottom, "X");
	gtk_plot_axis_set_title (left, "Y");
	gtk_plot_axis_set_title (top, "Z");

	gtk_plot_set_legends_border (GTK_PLOT (surface_plot),
				     GTK_PLOT_BORDER_LINE, 3);
	gtk_plot_legends_move (GTK_PLOT (surface_plot), 0.93, 0.05);
}

static void
clear_solutions (void)
{
	GSList *sl, *li;
	sl = solutions_list;
	solutions_list = NULL;

	for (li = sl; li != NULL; li = li->next) {
		GtkWidget *d = li->data;
		li->data = NULL;
		if (d != NULL)
			gtk_widget_destroy (d);
	}

	g_slist_free (sl);

	if (plot_canvas != NULL) {
		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}
}

static void
solver_dialog_response (GtkWidget *w, int response, gpointer data)
{
	if (response == RESPONSE_PLOT) {
		if (plot_mode == MODE_LINEPLOT_SLOPEFIELD)
			slopefield_draw_solution (solver_x, solver_y, solver_xinc);
		else
			vectorfield_draw_solution (solver_x, solver_y, solver_tinc, solver_tlen);
	} else if (response == RESPONSE_CLEAR) {
		clear_solutions ();
	} else  {
		gtk_widget_destroy (solver_dialog);
	}
}

static void
solver_entry_activate (void)
{
	if (solver_dialog != NULL)
		gtk_dialog_response (GTK_DIALOG (solver_dialog),
				     RESPONSE_PLOT);
}

static void
solver_cb (GtkWidget *item, gpointer data)
{
	GtkWidget *box, *w;
	double inc;

	if (line_plot == NULL ||
	    (plot_mode != MODE_LINEPLOT_SLOPEFIELD &&
	     plot_mode != MODE_LINEPLOT_VECTORFIELD))
		return;

	if (solver_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (solver_dialog));
		return;
	}

	solver_dialog = gtk_dialog_new_with_buttons
		(_("Solver") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 0 /* flags */,
		 GTK_STOCK_CLOSE,
		 GTK_RESPONSE_CLOSE,
		 _("Clea_r solutions"),
		 RESPONSE_CLEAR,
		 _("_Plot solution"),
		 RESPONSE_PLOT,
		 NULL);
	g_signal_connect (G_OBJECT (solver_dialog),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &solver_dialog);
	g_signal_connect (G_OBJECT (solver_dialog),
			  "response",
			  G_CALLBACK (solver_dialog_response),
			  NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (solver_dialog),
					 RESPONSE_PLOT);

	gtk_dialog_set_has_separator (GTK_DIALOG (solver_dialog), FALSE);

	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (solver_dialog)->vbox),
			    box, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);

	w = gtk_label_new (_("Clicking on the graph window now will draw a "
			     "solution according to the parameters set "
			     "below, starting at the point clicked.  "
			     "To be able to zoom by mouse again, close this "
			     "window."));
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	if (plot_mode == MODE_LINEPLOT_SLOPEFIELD) {
		solver_dialog_slopefield = TRUE;

		solver_xinc = (plotx2-plotx1) / 100;

		if (solver_xinc <= 0.005)
			inc = 0.001;
		else if (solver_xinc <= 0.05)
			inc = 0.01;
		else if (solver_xinc <= 0.5)
			inc = 0.1;
		else
			inc = 1;

		w = create_range_spinboxes (_("X increment:"), &solver_xinc, NULL,
					    0, G_MAXDOUBLE, inc,
					    NULL, NULL, NULL, 0, 0, 0,
					    NULL, NULL, NULL, 0, 0, 0,
					    solver_entry_activate);
		gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	} else {
		solver_dialog_slopefield = FALSE;

		solver_tinc = solver_tlen / 100;

		if (solver_tinc <= 0.005)
			inc = 0.001;
		else if (solver_tinc <= 0.05)
			inc = 0.01;
		else if (solver_tinc <= 0.5)
			inc = 0.1;
		else
			inc = 1;

		w = create_range_spinboxes (_("T increment:"), &solver_tinc, NULL,
					    0, G_MAXDOUBLE, inc,
					    _("T interval length:"), &solver_tlen, NULL,
					    0, G_MAXDOUBLE, 1,
					    NULL, NULL, NULL, 0, 0, 0,
					    solver_entry_activate);
		gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	}

	if (solver_x < plotx1 || solver_x > plotx2)
		solver_x = plotx1 + (plotx2-plotx1)/2;
	if (solver_y < ploty1 || solver_y > ploty2)
		solver_y = ploty1 + (ploty2-ploty1)/2;

	w = create_range_spinboxes (_("Point x:"), &solver_x,
				    &solver_x_sb,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("y:"), &solver_y,
				    &solver_y_sb,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    NULL, NULL, NULL, 0, 0, 0,
				    solver_entry_activate);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	gtk_widget_show_all (solver_dialog);
}

static void
clear_solutions_cb (GtkWidget *item, gpointer data)
{
	clear_solutions ();
}

static void
graph_window_destroyed (GtkWidget *w, gpointer data)
{
	graph_window = NULL;
	if (solver_dialog != NULL)
		gtk_widget_destroy (solver_dialog);
}

static void
ensure_window (gboolean do_window_present)
{
	GtkWidget *menu, *menubar, *item;
	static gboolean first_time = TRUE;
	static GtkAccelGroup *accel_group = NULL;

	if (first_time) {
		accel_group = gtk_accel_group_new ();

		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Zoom out",
					 GDK_minus,
					 GDK_CONTROL_MASK);
		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Zoom in",
					 GDK_plus,
					 GDK_CONTROL_MASK);
		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Fit dependent axis",
					 GDK_f,
					 GDK_CONTROL_MASK);
		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Reset to original zoom",
					 GDK_r,
					 GDK_CONTROL_MASK);
		first_time = FALSE;
	}

	/* ensure we don't whack things, just paranoia */
	whack_window_after_plot = FALSE;

	if (graph_window != NULL) {
		/* present is evil in that it takes focus away,
		 * only want to do it on the GUI triggered actions. */
		if (do_window_present)
			gtk_window_present (GTK_WINDOW (graph_window));
		else
			gtk_widget_show (graph_window);
		return;
	}

	graph_window = gtk_dialog_new_with_buttons
		(_("Plot") /* title */,
		 NULL /*GTK_WINDOW (genius_window)*/ /* parent */,
		 0 /* flags */,
		 GTK_STOCK_STOP,
		 RESPONSE_STOP,
		 GTK_STOCK_CLOSE,
		 GTK_RESPONSE_CLOSE,
		 NULL);
	gtk_window_add_accel_group (GTK_WINDOW (graph_window),
				    accel_group);

	g_signal_connect (G_OBJECT (graph_window),
			  "destroy",
			  G_CALLBACK (graph_window_destroyed), NULL);
	g_signal_connect (G_OBJECT (graph_window),
			  "delete_event",
			  G_CALLBACK (graph_window_delete_event),
			  NULL);
	g_signal_connect (G_OBJECT (graph_window),
			  "response",
			  G_CALLBACK (graph_window_response),
			  NULL);

	menubar = gtk_menu_bar_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (graph_window)->vbox),
			    GTK_WIDGET (menubar), FALSE, TRUE, 0);

	/*
	 * Graph menu
	 */
	menu = gtk_menu_new ();
	gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
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

	item = gtk_menu_item_new_with_mnemonic (_("_Export PNG..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_exportpng_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	plot_exportpng_item = item;


	/*
	 * Zoom menu
	 */
	menu = gtk_menu_new ();
	gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
	item = gtk_menu_item_new_with_mnemonic (_("_Zoom"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

	item = gtk_menu_item_new_with_mnemonic (_("Zoom _out"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_zoomout_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), "<Genius-Plot>/Zoom/Zoom out");
	plot_zoomout_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("Zoom _in"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_zoomin_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), "<Genius-Plot>/Zoom/Zoom in");
	plot_zoomin_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("_Fit dependent axis"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_zoomfit_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), "<Genius-Plot>/Zoom/Fit dependent axis");
	plot_zoomfit_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("_Reset to original zoom"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (plot_resetzoom_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), "<Genius-Plot>/Zoom/Reset to original zoom");
	plot_resetzoom_item = item;


	/*
	 * View menu
	 */
	menu = gtk_menu_new ();
	gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
	item = gtk_menu_item_new_with_mnemonic (_("_View"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
	view_menu_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("_Reset angles"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (reset_angles_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Top view"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (top_view_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("R_otate axis..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (rotate_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	/*
	 * Solver menu
	 */
	menu = gtk_menu_new ();
	gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
	item = gtk_menu_item_new_with_mnemonic (_("_Solver"));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
	solver_menu_item = item;

	item = gtk_menu_item_new_with_mnemonic (_("_Solver..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (solver_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Clear solutions"));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (clear_solutions_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);


	plot_canvas = gtk_plot_canvas_new (WIDTH, HEIGHT, 1.0);
	GTK_PLOT_CANVAS_UNSET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				     GTK_PLOT_CANVAS_DND_FLAGS);
	g_signal_connect (G_OBJECT (plot_canvas), "select_region",
			  G_CALLBACK (plot_select_region),
			  NULL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (graph_window)->vbox),
			    GTK_WIDGET (plot_canvas), TRUE, TRUE, 0);



	gtk_widget_show_all (graph_window);
}


static void
clear_graph (void)
{
	int i;

	/* to avoid the costly removes */
	g_slist_free (solutions_list);
	solutions_list = NULL;

	if (plot_child != NULL) {
		if (plot_canvas != NULL)
			gtk_plot_canvas_remove_child (GTK_PLOT_CANVAS (plot_canvas),
						      plot_child);
		surface_plot = NULL;
		line_plot = NULL;
		plot_child = NULL;
	}

	for (i = 0; i < MAXFUNC; i++) {
		line_data[i] = NULL;
	}

	parametric_data =  NULL;

	slopefield_data =  NULL;

	vectorfield_data =  NULL;

	surface_data = NULL;
}

static void
get_ticks (double start, double end, double *tick, int *prec)
{
	int incs;
	double len = end-start;
	int tries = 0;
	int tickprec;
	int extra_prec;

	tickprec = -floor (log10(len));
	*tick = pow (10, -tickprec);
	incs = floor (len / *tick);

	extra_prec = 0;

	while (incs < 4) {
		*tick /= 2.0;

		extra_prec ++;

		incs = floor (len / *tick);
		/* sanity */
		if (tries ++ > 100) {
			break;
		}
	}

	while (incs > 6) {
		*tick *= 2.0;
		incs = floor (len / *tick);

		if (extra_prec > 0)
			extra_prec --;

		/* sanity */
		if (tries ++ > 100) {
			break;
		}
	}

	if (tickprec + extra_prec <= 0) {
		*prec = 0;
	} else {
		*prec = tickprec + extra_prec;
	}
}

static void
plot_setup_axis (void)
{
	int xprec, yprec;
	double xtick, ytick;
	GtkPlotAxis *x, *y;
	GdkColor gray;

	get_ticks (plotx1, plotx2, &xtick, &xprec);
	get_ticks (ploty1, ploty2, &ytick, &yprec);

	gtk_plot_freeze (GTK_PLOT (line_plot));

	gtk_plot_set_range (GTK_PLOT (line_plot),
			    plotx1, plotx2, ploty1, ploty2);
	gtk_plot_set_ticks (GTK_PLOT (line_plot), GTK_PLOT_AXIS_X, xtick, 9);
	gtk_plot_set_ticks (GTK_PLOT (line_plot), GTK_PLOT_AXIS_Y, ytick, 9);

	/* this should all be configurable */
	gtk_plot_x0_set_visible (GTK_PLOT (line_plot), TRUE);
	gtk_plot_y0_set_visible (GTK_PLOT (line_plot), TRUE);

	gtk_plot_grids_set_visible (GTK_PLOT (line_plot),
				    TRUE /* vmajor */,
				    FALSE /* vminor */,
				    TRUE /* vmajor */,
				    FALSE /* vminor */);

	gdk_color_parse ("gray75", &gray);

	gtk_plot_x0line_set_attributes (GTK_PLOT (line_plot),
					GTK_PLOT_LINE_SOLID,
					1 /* width */,
					&gray);
	gtk_plot_y0line_set_attributes (GTK_PLOT (line_plot),
					GTK_PLOT_LINE_SOLID,
					1 /* width */,
					&gray);

	gtk_plot_major_vgrid_set_attributes (GTK_PLOT (line_plot),
					     GTK_PLOT_LINE_DOTTED,
					     1 /* width */,
					     &gray);
	gtk_plot_major_hgrid_set_attributes (GTK_PLOT (line_plot),
					     GTK_PLOT_LINE_DOTTED,
					     1 /* width */,
					     &gray);


	x = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_TOP);
	gtk_plot_axis_set_labels_style (x,
					GTK_PLOT_LABEL_FLOAT,
					xprec /* precision */);

	x = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_BOTTOM);
	gtk_plot_axis_set_labels_style (x,
					GTK_PLOT_LABEL_FLOAT,
					xprec /* precision */);

	y = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_LEFT);
	gtk_plot_axis_set_labels_style (y,
					GTK_PLOT_LABEL_FLOAT,
					yprec /* precision */);

	y = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_RIGHT);
	gtk_plot_axis_set_labels_style (y,
					GTK_PLOT_LABEL_FLOAT,
					yprec /* precision */);


	/* FIXME: implement logarithmic scale
	gtk_plot_set_xscale (GTK_PLOT (line_plot), GTK_PLOT_SCALE_LOG10);
	gtk_plot_set_yscale (GTK_PLOT (line_plot), GTK_PLOT_SCALE_LOG10);
	*/

	gtk_plot_thaw (GTK_PLOT (line_plot));
}

static void
surface_setup_axis (void)
{
	int xprec, yprec, zprec;
	double xtick, ytick, ztick;
	GtkPlotAxis *x, *y, *z;

	get_ticks (surfacex1, surfacex2, &xtick, &xprec);
	get_ticks (surfacey1, surfacey2, &ytick, &yprec);
	get_ticks (surfacez1, surfacez2, &ztick, &zprec);

	x = gtk_plot3d_get_axis (GTK_PLOT3D (surface_plot), GTK_PLOT_AXIS_X);
	y = gtk_plot3d_get_axis (GTK_PLOT3D (surface_plot), GTK_PLOT_AXIS_Y);
	z = gtk_plot3d_get_axis (GTK_PLOT3D (surface_plot), GTK_PLOT_AXIS_Z);

	gtk_plot_axis_freeze (x);
	gtk_plot_axis_freeze (y);
	gtk_plot_axis_freeze (z);

	gtk_plot3d_set_xrange (GTK_PLOT3D (surface_plot), surfacex1, surfacex2);
	gtk_plot_axis_set_ticks (x, xtick, 1);
	gtk_plot3d_set_yrange (GTK_PLOT3D (surface_plot), surfacey1, surfacey2);
	gtk_plot_axis_set_ticks (y, ytick, 1);
	gtk_plot3d_set_zrange (GTK_PLOT3D (surface_plot), surfacez1, surfacez2);
	gtk_plot_axis_set_ticks (z, ztick, 1);

	gtk_plot_axis_set_labels_style (x,
					GTK_PLOT_LABEL_FLOAT,
					xprec /* precision */);
	gtk_plot_axis_set_labels_style (y,
					GTK_PLOT_LABEL_FLOAT,
					yprec /* precision */);
	gtk_plot_axis_set_labels_style (z,
					GTK_PLOT_LABEL_FLOAT,
					zprec /* precision */);

	gtk_plot_axis_thaw (x);
	gtk_plot_axis_thaw (y);
	gtk_plot_axis_thaw (z);
}

/* FIXME: perhaps should be smarter ? */
static void
surface_setup_steps (void)
{
	gtk_plot_surface_set_xstep (GTK_PLOT_SURFACE (surface_data), (surfacex2-surfacex1)/30);
	gtk_plot_surface_set_ystep (GTK_PLOT_SURFACE (surface_data), (surfacey2-surfacey1)/30);

	gtk_plot_data_set_gradient (surface_data,
				    surfacez1,
				    surfacez2,
				    10 /* nlevels */,
				    0 /* nsublevels */);
}

static void
plot_axis (void)
{
	plot_in_progress ++;
	plot_window_setup ();

	plot_maxy = - G_MAXDOUBLE/2;
	plot_miny = G_MAXDOUBLE/2;
	plot_maxx = - G_MAXDOUBLE/2;
	plot_minx = G_MAXDOUBLE/2;

	if (plot_mode == MODE_LINEPLOT ||
	    plot_mode == MODE_LINEPLOT_PARAMETRIC ||
	    plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
	    plot_mode == MODE_LINEPLOT_VECTORFIELD) {
		plot_setup_axis ();
	} else if (plot_mode == MODE_SURFACE) {
		surface_setup_axis ();
		surface_setup_steps ();
		/* FIXME: this doesn't work (crashes) must fix in GtkExtra, then
		   we can always just autoscale stuff
		   gtk_plot3d_autoscale (GTK_PLOT3D (surface_plot));
		 */
	}

	replot_fields ();

	if (plot_canvas != NULL) {
		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

	plot_in_progress --;
	plot_window_setup ();
}

static double
call_func3 (GelCtx *ctx,
	    GelEFunc *func,
	    GelETree *arg,
	    GelETree *arg2,
	    GelETree *arg3,
	    gboolean *ex,
	    GelETree **func_ret)
{
	GelETree *ret;
	double retd;
	GelETree *args[4];

	args[0] = arg;
	args[1] = arg2;
	args[2] = arg3;
	args[3] = NULL;

	ret = funccall (ctx, func, args, 3);

	/* FIXME: handle errors! */
	if (error_num != 0)
		error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == FUNCTION_NODE) {
		if (ret->func.func->nargs == 3) {
			GelETree *ret2;
			ret2 = funccall (ctx, ret->func.func, args, 3);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if (error_num != 0)
				error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			return HUGE_VAL;
#else
			return 0;
#endif
		}

	}

	if (ret == NULL || ret->type != VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		return HUGE_VAL;
#else
		return 0;
#endif
	}

	retd = mpw_get_double (ret->val.value);
	if (error_num != 0) {
		*ex = TRUE;
		error_num = 0;
#ifdef HUGE_VAL
		retd = HUGE_VAL;
#endif
	}
	
	gel_freetree (ret);
	return retd;
}

static double
call_func2 (GelCtx *ctx,
	    GelEFunc *func,
	    GelETree *arg,
	    GelETree *arg2,
	    gboolean *ex,
	    GelETree **func_ret)
{
	GelETree *ret;
	double retd;
	GelETree *args[3];

	args[0] = arg;
	args[1] = arg2;
	args[2] = NULL;

	ret = funccall (ctx, func, args, 2);

	/* FIXME: handle errors! */
	if (error_num != 0)
		error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == FUNCTION_NODE) {
		if (ret->func.func->nargs == 2) {
			GelETree *ret2;
			ret2 = funccall (ctx, ret->func.func, args, 2);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if (error_num != 0)
				error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			return HUGE_VAL;
#else
			return 0;
#endif
		}
	}

	if (ret == NULL || ret->type != VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		return HUGE_VAL;
#else
		return 0;
#endif
	}

	retd = mpw_get_double (ret->val.value);
	if (error_num != 0) {
		*ex = TRUE;
		error_num = 0;
#ifdef HUGE_VAL
		retd = HUGE_VAL;
#endif
	}
	
	gel_freetree (ret);
	return retd;
}

static double
call_func (GelCtx *ctx,
	   GelEFunc *func,
	   GelETree *arg,
	   gboolean *ex,
	   GelETree **func_ret)
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
	if (ret != NULL && ret->type == FUNCTION_NODE) {
		if (ret->func.func->nargs == 1) {
			GelETree *ret2;
			ret2 = funccall (ctx, ret->func.func, args, 1);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if (error_num != 0)
				error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			return HUGE_VAL;
#else
			return 0;
#endif
		}

	}

	if (ret == NULL || ret->type != VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		return HUGE_VAL;
#else
		return 0;
#endif
	}

	retd = mpw_get_double (ret->val.value);
	if (error_num != 0) {
		*ex = TRUE;
		error_num = 0;
#ifdef HUGE_VAL
		retd = HUGE_VAL;
#endif
	}
	
	gel_freetree (ret);
	return retd;
}

static void
call_func_z (GelCtx *ctx,
	     GelEFunc *func,
	     GelETree *arg,
	     double *retx,
	     double *rety,
	     gboolean *ex,
	     GelETree **func_ret)
{
	GelETree *ret;
	GelETree *args[2];

	args[0] = arg;
	args[1] = NULL;

	ret = funccall (ctx, func, args, 1);

	/* FIXME: handle errors! */
	if (error_num != 0)
		error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == FUNCTION_NODE) {
		if (ret->func.func->nargs == 1) {
			GelETree *ret2;
			ret2 = funccall (ctx, ret->func.func, args, 1);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if (error_num != 0)
				error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			*retx = HUGE_VAL;
			*rety = HUGE_VAL;
#else
			*retx = 0.0;
			*rety = 0.0;
#endif
			return;
		}

	}

	if (ret == NULL || ret->type != VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		*retx = HUGE_VAL;
		*rety = HUGE_VAL;
#else
		*retx = 0.0;
		*rety = 0.0;
#endif
		return;
	}

	mpw_get_complex_double (ret->val.value, retx, rety);
	if (error_num != 0) {
		*ex = TRUE;
		error_num = 0;
#ifdef HUGE_VAL
		*retx = HUGE_VAL;
		*rety = HUGE_VAL;
#else
		*retx = 0.0;
		*rety = 0.0;
#endif
	}
	
	gel_freetree (ret);
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
	y = call_func (plot_ctx, plot_func[i], plot_arg, &ex, NULL);

	if G_UNLIKELY (ex) {
		if (error != NULL)
			*error = TRUE;
	} else {
		if G_UNLIKELY (y > plot_maxy)
			plot_maxy = y;
		if G_UNLIKELY (y < plot_miny)
			plot_miny = y;
	}

	if (y > ploty2 || y < ploty1) {
		if (error != NULL)
			*error = TRUE;
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

static double
call_xy_or_z_function (GelEFunc *f, double x, double y, gboolean *ex)
{
	GelETree *func_ret = NULL;
	double z;

	/* complex function */
	if (f->nargs == 1) {
		mpw_set_d_complex (plot_arg->val.value, x, y);
		z = call_func (plot_ctx, f, plot_arg, ex,
			       &func_ret);
	} else if (f->nargs == 2) {
		mpw_set_d (plot_arg->val.value, x);
		mpw_set_d (plot_arg2->val.value, y);
		z = call_func2 (plot_ctx, f, plot_arg, plot_arg2,
				ex, &func_ret);
	} else {
		mpw_set_d (plot_arg->val.value, x);
		mpw_set_d (plot_arg2->val.value, y);
		mpw_set_d_complex (plot_arg3->val.value, x, y);
		z = call_func3 (plot_ctx, f, plot_arg, plot_arg2,
				plot_arg3, ex, &func_ret);
	}
	if (func_ret != NULL) {
		/* complex function */
		if (func_ret->func.func->nargs == 1) {
			mpw_set_d_complex (plot_arg->val.value, x, y);
			z = call_func (plot_ctx, func_ret->func.func, plot_arg, ex,
				       NULL);
		} else if (func_ret->func.func->nargs == 2) {
			mpw_set_d (plot_arg->val.value, x);
			mpw_set_d (plot_arg2->val.value, y);
			z = call_func2 (plot_ctx, func_ret->func.func, plot_arg, plot_arg2,
					ex, NULL);
		} else {
			mpw_set_d (plot_arg->val.value, x);
			mpw_set_d (plot_arg2->val.value, y);
			mpw_set_d_complex (plot_arg3->val.value, x, y);
			z = call_func3 (plot_ctx, func_ret->func.func, plot_arg, plot_arg2,
					plot_arg3, ex, NULL);
		}
	}

	return z;
}


static double
surface_func_data (GtkPlot *plot, GtkPlotData *data, double x, double y, gboolean *error)
{
	static int hookrun = 0;
	gboolean ex = FALSE;
	double z, size;

	if (error != NULL)
		*error = FALSE;

	if G_UNLIKELY (interrupted) {
		if (error != NULL)
			*error = TRUE;
		return 0.0;
	}

	z = call_xy_or_z_function (surface_func, x, y, &ex);

	if G_UNLIKELY (ex) {
		if (error != NULL)
			*error = TRUE;
	} else {
		if G_UNLIKELY (z > plot_maxy)
			plot_maxy = z;
		if G_UNLIKELY (z < plot_miny)
			plot_miny = z;
	}

	size = surfacez1 - surfacez2;

	if (z > (surfacez2+size*0.2) || z < (surfacez1-size*0.2)) {
		if (error != NULL)
			*error = TRUE;
	}


	if (hookrun++ >= 10) {
		hookrun = 0;
		if (evalnode_hook != NULL) {
			(*evalnode_hook)();
			if G_UNLIKELY (interrupted) {
				if (error != NULL)
					*error = TRUE;
				return z;
			}
		}
	}

	return z;
}

static void
get_slopefield_points (void)
{
	double x, y, vt, ht, z, dx, dy;
	int i, j, k;
	gboolean ex;
	double pw, ph;
	double xmul, ymul;
	double sz;
	double mt;

	/* FIXME: evil, see the AAAARGH below! */

	gtk_plot_get_size (GTK_PLOT (line_plot), &pw, &ph);
	xmul = (pw * WIDTH) / (plotx2 - plotx1);
	ymul = (ph * HEIGHT) / (ploty2 - ploty1);

	ht = (plotx2 - plotx1) / (plotHtick);
	vt = (ploty2 - ploty1) / (plotVtick);

	mt = MIN (((pw * WIDTH) / (plotHtick)),
		  ((ph * HEIGHT) / (plotVtick))) * 0.8;

	g_free (plot_points_x);
	g_free (plot_points_y);
	g_free (plot_points_dx);
	g_free (plot_points_dy);

	plot_points_x = g_new (double, (plotHtick)*(plotVtick));
	plot_points_y = g_new (double, (plotHtick)*(plotVtick));
	plot_points_dx = g_new (double, (plotHtick)*(plotVtick));
	plot_points_dy = g_new (double, (plotHtick)*(plotVtick));

	k = 0;
	for (i = 0; i < plotHtick; i++) {
		for (j = 0; j < plotVtick; j++) {
			x = plotx1 + ht*(i+0.5);
			y = ploty1 + vt*(j+0.5);
			ex = FALSE;
			z = call_xy_or_z_function (slopefield_func,
						   x, y, &ex);

			if G_LIKELY ( ! ex) {
				/* gtkextra fluxplot is nuts, it does the
				 * dx and dy are in pixel coordinates, 
				 * AAAAAARGH! */

				z = z*ymul;
				sz = sqrt(z*z + xmul*xmul);
				dx = xmul / sz;
				dy = z / sz;

				dx *= mt;
				dy *= mt;

				plot_points_x[k] = x;
				plot_points_dx[k] = dx;

				plot_points_y[k] = y;
				plot_points_dy[k] = dy;

				k++;
			}
		}
	}

	plot_points_num = k;
}

static void
get_vectorfield_points (void)
{
	double x, y, vt, ht, dx, dy;
	int i, j, k;
	gboolean ex;
	double maxsz = 0.0;
	double pw, ph;
	double xmul, ymul;
	double mt, sz;

	/* FIXME: evil, see the AAAARGH below! */

	gtk_plot_get_size (GTK_PLOT (line_plot), &pw, &ph);
	xmul = (pw * WIDTH) / (plotx2 - plotx1);
	ymul = (ph * HEIGHT) / (ploty2 - ploty1);

	ht = (plotx2 - plotx1) / (plotHtick);
	vt = (ploty2 - ploty1) / (plotVtick);

	mt = MIN (((pw * WIDTH) / (plotHtick)),
		  ((ph * HEIGHT) / (plotVtick))) * 0.95;

	g_free (plot_points_x);
	g_free (plot_points_y);
	g_free (plot_points_dx);
	g_free (plot_points_dy);

	plot_points_x = g_new (double, (plotHtick)*(plotVtick));
	plot_points_y = g_new (double, (plotHtick)*(plotVtick));
	plot_points_dx = g_new (double, (plotHtick)*(plotVtick));
	plot_points_dy = g_new (double, (plotHtick)*(plotVtick));

	k = 0;
	for (i = 0; i < plotHtick; i++) {
		for (j = 0; j < plotVtick; j++) {
			x = plotx1 + ht*(i+0.5);
			y = ploty1 + vt*(j+0.5);
			ex = FALSE;
			dx = call_xy_or_z_function (vectorfield_func_x,
						    x, y, &ex);
			dy = call_xy_or_z_function (vectorfield_func_y,
						    x, y, &ex);

			if G_LIKELY ( ! ex) {
				/* gtkextra fluxplot is nuts, it does the
				 * dx and dy are in pixel coordinates, 
				 * AAAAAARGH! */

				dx = dx*xmul;
				dy = dy*ymul;

				sz = sqrt(dx*dx + dy*dy);

				if (vectorfield_normalize_arrow_length) {
					if (sz > 0) {
						dx /= sz;
						dy /= sz;
					}
				} else {
					if (sz > maxsz)
						maxsz = sz;
				}

				plot_points_x[k] = x;
				plot_points_dx[k] = dx * mt;

				plot_points_y[k] = y;
				plot_points_dy[k] = dy * mt;


				k++;
			}
		}
	}

	plot_points_num = k;

	if ( ! vectorfield_normalize_arrow_length) {
		if (maxsz > 0) {
			for (k = 0; k < plot_points_num; k++) {
				plot_points_dx[k] /= maxsz;
				plot_points_dy[k] /= maxsz;
			}
		}
	}
}

static char *
label_func (int i, GelEFunc *func, const char *var, const char *name)
{
	char *text = NULL;

	if (name != NULL) {
		return g_strdup (name);
	} else if (func->id != NULL) {
		text = g_strdup_printf ("%s(%s)", func->id->token, var);
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
		if (i < 0)
			text = g_strdup_printf (_("Function"));
		else
			text = g_strdup_printf (_("Function #%d"), i+1);
	}

	return text;
}

#define GET_DOUBLE(var,argnum,func) \
	{ \
	if (a[argnum]->type != VALUE_NODE) { \
		gel_errorout (_("%s: argument number %d not a number"), func, argnum+1); \
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

	/* sanity */
	if (*x2 - *x1 < MINPLOT)
		*x2 = *x1 + MINPLOT;
	if (*y2 - *y1 < MINPLOT)
		*y2 = *y1 + MINPLOT;

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

static gboolean
get_limits_from_matrix_surf (GelETree *m, double *x1, double *x2, double *y1, double *y2, double *z1, double *z2)
{
	GelETree *t;

	if (m->type != MATRIX_NODE ||
	    gel_matrixw_elements (m->mat.matrix) != 6) {
		gel_errorout (_("Graph limits not given as a 6-vector"));
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
	t = gel_matrixw_vindex (m->mat.matrix, 4);
	if (t->type != VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*z1 = mpw_get_double (t->val.value);
	t = gel_matrixw_vindex (m->mat.matrix, 5);
	if (t->type != VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*z2 = mpw_get_double (t->val.value);

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

	if (*z1 > *z2) {
		double s = *z1;
		*z1 = *z2;
		*z2 = s;
	}


	/* sanity */
	if (*x2 - *x1 < MINPLOT)
		*x2 = *x1 + MINPLOT;
	if (*y2 - *y1 < MINPLOT)
		*y2 = *y1 + MINPLOT;
	if (*z2 - *z1 < MINPLOT)
		*z2 = *z1 + MINPLOT;

	return TRUE;
}

static GelETree *
make_matrix_from_limits_surf (void)
{
	GelETree *n;
	GelMatrixW *m;
	/*make us a new empty node*/
	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 6, 1);

	gel_matrixw_set_index (m, 0, 0) = gel_makenum_d (surf_defx1);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_d (surf_defx2);
	gel_matrixw_set_index (m, 2, 0) = gel_makenum_d (surf_defy1);
	gel_matrixw_set_index (m, 3, 0) = gel_makenum_d (surf_defy2);
	gel_matrixw_set_index (m, 4, 0) = gel_makenum_d (surf_defz1);
	gel_matrixw_set_index (m, 5, 0) = gel_makenum_d (surf_defz2);

	return n;
}

static gboolean
parametric_get_value (double *x, double *y, double t)
{
	static int hookrun = 0;
	gboolean ex = FALSE;

	mpw_set_d (plot_arg->val.value, t);
	if (parametric_func_z != NULL) {
		call_func_z (plot_ctx, parametric_func_z, plot_arg, x, y, &ex, NULL);
	} else {
		*x = call_func (plot_ctx, parametric_func_x, plot_arg, &ex, NULL);
		if G_LIKELY ( ! ex)
			*y = call_func (plot_ctx, parametric_func_y, plot_arg, &ex, NULL);
	}

	if G_UNLIKELY (ex) {
		*x = 0.0;
		*y = 0.0;
		return FALSE;
	} else {
		if G_UNLIKELY (*y > plot_maxy)
			plot_maxy = *y;
		if G_UNLIKELY (*y < plot_miny)
			plot_miny = *y;
		if G_UNLIKELY (*x > plot_maxx)
			plot_maxx = *x;
		if G_UNLIKELY (*x < plot_minx)
			plot_minx = *x;
	}
	/* FIXME: sanity on x/y ??? */

	if (hookrun++ >= 10) {
		hookrun = 0;
		if (evalnode_hook != NULL) {
			(*evalnode_hook)();
		}
	}

	return TRUE;
}

static GtkPlotData *
draw_line (double *x, double *y, int len, int thickness, GdkColor *color)
{
	double *dx, *dy;
	GtkPlotData *data;

	data = GTK_PLOT_DATA (gtk_plot_data_new ());
	dx = g_new0 (double, len);
	dy = g_new0 (double, len);
	gtk_plot_data_set_points (data, x, y, dx, dy, len);
	g_object_set_data_full (G_OBJECT (data),
				"x", x, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"y", y, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"dx", dx, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"dy", dy, (GDestroyNotify)g_free);
	gtk_plot_add_data (GTK_PLOT (line_plot), data);
	gtk_plot_data_hide_legend (data);

	gdk_color_alloc (gdk_colormap_get_system (), color); 

	gtk_plot_data_set_line_attributes (data,
					   GTK_PLOT_LINE_SOLID,
					   0, 0, thickness, color);

	gtk_widget_show (GTK_WIDGET (data));

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

	return data;
}

static void
clip_line_ends (double xx[], double yy[], int len)
{
	if G_UNLIKELY (len < 2)
		return;

	if (xx[0] < plotx1) {
		double slope = (yy[1]-yy[0])/(xx[1]-xx[0]);
		xx[0] = plotx1;
		yy[0] = yy[1] - (xx[1]-plotx1) * slope;
	}

	if (yy[0] < ploty1) {
		if G_LIKELY (/* sanity */(xx[1]-xx[0]) > 0) {
			double slope = (yy[1]-yy[0])/(xx[1]-xx[0]);
			xx[0] = (ploty1 -yy[1] + slope*xx[1]) / slope;
			yy[0] = ploty1;
		}
	} else if (yy[0] > ploty2) {
		if G_LIKELY (/* sanity */(xx[1]-xx[0]) > 0) {
			double slope = (yy[1]-yy[0])/(xx[1]-xx[0]);
			xx[0] = (ploty2 -yy[1] + slope*xx[1]) / slope;
			yy[0] = ploty2;
		}
	}

	if (xx[len-1] > plotx2) {
		double slope = (yy[len-1]-yy[len-2])
			/ (xx[len-1]-xx[len-2]);
		xx[len-1] = plotx2;
		yy[len-1] = yy[len-2] + (plotx2-xx[len-2]) * slope;
	}

	if (yy[len-1] < ploty1) {
		if G_LIKELY (/* sanity */(xx[len-1]-xx[len-2]) > 0) {
			double slope = (yy[len-1]-yy[len-2])/(xx[len-1]-xx[len-2]);
			xx[len-1] = (ploty1 - yy[len-2] + slope*xx[len-2]) / slope;
			yy[len-1] = ploty1;
		}
	} else if (yy[len-1] > ploty2) {
		if G_LIKELY (/* sanity */(xx[len-1]-xx[len-2]) > 0) {
			double slope = (yy[len-1]-yy[len-2])/(xx[len-1]-xx[len-2]);
			xx[len-1] = (ploty2 - yy[len-2] + slope*xx[len-2]) / slope;
			yy[len-1] = ploty2;
		}
	}
}

static void
solution_destroyed (GtkWidget *plotdata, gpointer data)
{
	solutions_list = g_slist_remove (solutions_list, plotdata);
}

static void
slopefield_draw_solution (double x, double y, double dx)
{
	double *xx, *yy;
	double cx, cy;
	int len1, len2, len;
	int i;
	GdkColor color;
	GSList *points1 = NULL;
	GSList *points2 = NULL;
	GSList *li;
	GtkPlotData *data;

	if (slopefield_func == NULL)
		return;

	gdk_color_parse ("red", &color);

	len1 = 0;
	cx = x;
	cy = y;
	while (cx < plotx2 && cy > ploty1 && cy < ploty2) {
		double *pt;
		gboolean ex = FALSE;
		double k1, k2, k3, k4, sl;

		/* standard Runge-Kutta */
		k1 = call_xy_or_z_function (slopefield_func,
					    cx, cy, &ex);
		if G_UNLIKELY (ex) break;
		k2 = call_xy_or_z_function (slopefield_func,
					    cx+(dx/2), cy+(dx/2)*k1, &ex);
		if G_UNLIKELY (ex) break;
		k3 = call_xy_or_z_function (slopefield_func,
					    cx+(dx/2), cy+(dx/2)*k2, &ex);
		if G_UNLIKELY (ex) break;
		k4 = call_xy_or_z_function (slopefield_func,
					    cx+dx, cy+dx*k3, &ex);
		if G_UNLIKELY (ex) break;

		sl = (k1+2*k2+2*k3+k4)/6.0;

		cy += sl * dx;
		cx += dx;
		
		len1 ++;

		pt = g_new (double, 2);
		pt[0] = cx;
		pt[1] = cy;

		points1 = g_slist_prepend (points1, pt);
	}

	points1 = g_slist_reverse (points1);

	len2 = 0;
	cx = x;
	cy = y;
	while (cx > plotx1 && cy > ploty1 && cy < ploty2) {
		double *pt;
		gboolean ex = FALSE;
		double k1, k2, k3, k4, sl;

		/* standard Runge-Kutta */
		k1 = call_xy_or_z_function (slopefield_func,
					    cx, cy, &ex);
		if G_UNLIKELY (ex) break;
		k2 = call_xy_or_z_function (slopefield_func,
					    cx-(dx/2), cy-(dx/2)*k1, &ex);
		if G_UNLIKELY (ex) break;
		k3 = call_xy_or_z_function (slopefield_func,
					    cx-(dx/2), cy-(dx/2)*k2, &ex);
		if G_UNLIKELY (ex) break;
		k4 = call_xy_or_z_function (slopefield_func,
					    cx-dx, cy-dx*k3, &ex);
		if G_UNLIKELY (ex) break;

		sl = (k1+2*k2+2*k3+k4)/6.0;

		cy -= sl* dx;
		cx -= dx;
		
		len2 ++;

		pt = g_new (double, 2);
		pt[0] = cx;
		pt[1] = cy;

		points2 = g_slist_prepend (points2, pt);
	}

	len = len1 + 1 + len2;
	xx = g_new0 (double, len);
	yy = g_new0 (double, len);

	i = 0;
	for (li = points2; li != NULL; li = li->next) {
		double *pt = li->data;
		li->data = NULL;

		xx[i] = pt[0];
		yy[i] = pt[1];

		g_free (pt);

		i++;
	}

	xx[i] = x;
	yy[i] = y;

	i++;

	for (li = points1; li != NULL; li = li->next) {
		double *pt = li->data;
		li->data = NULL;

		xx[i] = pt[0];
		yy[i] = pt[1];

		g_free (pt);

		i++;
	}

	g_slist_free (points1);
	g_slist_free (points2);

	/* Adjust ends */
	clip_line_ends (xx, yy, len);

	data = draw_line (xx, yy, len, 2 /* thickness */, &color);
	solutions_list = g_slist_prepend (solutions_list,
					  data);
	g_signal_connect (G_OBJECT (data), "destroy",
			  G_CALLBACK (solution_destroyed), NULL);
}

static void
vectorfield_draw_solution (double x, double y, double dt, double tlen)
{
	double *xx, *yy;
	double cx, cy, t;
	int len;
	int i;
	GdkColor color;
	GtkPlotData *data;

	if (vectorfield_func_x == NULL ||
	    vectorfield_func_y == NULL ||
	    dt <= 0.0 ||
	    tlen <= 0.0)
		return;

	gdk_color_parse ("red", &color);

	len = (int)(tlen / dt) + 2;
	xx = g_new0 (double, len);
	yy = g_new0 (double, len);

	i = 1;
	xx[0] = x;
	yy[0] = y;
	cx = x;
	cy = y;
	t = 0.0;
	while (t < tlen && i < len) {
		gboolean ex = FALSE;
		double xk1, xk2, xk3, xk4, xsl;
		double yk1, yk2, yk3, yk4, ysl;

		/* standard Runge-Kutta */
		xk1 = call_xy_or_z_function (vectorfield_func_x,
					     cx, cy, &ex);
		if G_UNLIKELY (ex) break;
		yk1 = call_xy_or_z_function (vectorfield_func_y,
					     cx, cy, &ex);
		if G_UNLIKELY (ex) break;

		xk2 = call_xy_or_z_function (vectorfield_func_x,
					     cx+(dt/2)*xk1, cy+(dt/2)*yk1, &ex);
		if G_UNLIKELY (ex) break;
		yk2 = call_xy_or_z_function (vectorfield_func_y,
					     cx+(dt/2)*xk1, cy+(dt/2)*yk1, &ex);
		if G_UNLIKELY (ex) break;

		xk3 = call_xy_or_z_function (vectorfield_func_x,
					     cx+(dt/2)*xk2, cy+(dt/2)*yk2, &ex);
		if G_UNLIKELY (ex) break;
		yk3 = call_xy_or_z_function (vectorfield_func_y,
					     cx+(dt/2)*xk2, cy+(dt/2)*yk2, &ex);
		if G_UNLIKELY (ex) break;

		xk4 = call_xy_or_z_function (vectorfield_func_x,
					     cx+dt*xk3, cy+dt*yk3, &ex);
		if G_UNLIKELY (ex) break;
		yk4 = call_xy_or_z_function (vectorfield_func_y,
					     cx+dt*xk3, cy+dt*yk3, &ex);
		if G_UNLIKELY (ex) break;

		xsl = (xk1+2*xk2+2*xk3+xk4)/6.0;
		ysl = (yk1+2*yk2+2*yk3+yk4)/6.0;

		cx += xsl * dt;
		cy += ysl * dt;

		xx[i] = cx;
		yy[i] = cy;

		i ++;
		t += dt;
	}

	len = i;

	data = draw_line (xx, yy, len, 2 /* thickness */, &color);
	solutions_list = g_slist_prepend (solutions_list,
					  data);
	g_signal_connect (G_OBJECT (data), "destroy",
			  G_CALLBACK (solution_destroyed), NULL);
}


static void
replot_fields (void)
{
	if (slopefield_func != NULL) {
		get_slopefield_points ();
		if (plot_points_num > 0) {
			GdkColor color;

			if (slopefield_data == NULL) {
				char *label, *tmp;

				slopefield_data = GTK_PLOT_DATA(gtk_plot_flux_new());
				gtk_plot_add_data (GTK_PLOT (line_plot),
						   slopefield_data);
				gdk_color_parse ("blue", &color);
				gtk_plot_data_set_line_attributes (slopefield_data,
								   GTK_PLOT_LINE_NONE,
								   0, 0, 1, &color);
				gtk_plot_data_set_symbol (slopefield_data,
							  GTK_PLOT_SYMBOL_NONE /* symbol type? */,
							  GTK_PLOT_SYMBOL_EMPTY /* symbol style */,
							  1 /* size? */,
							  (plotHtick > 15 || plotVtick > 15) ? 1 : 2
							    /* line_width */,
							  &color /* color */,
							  &color /* border_color? */);


				gtk_plot_flux_set_arrow (GTK_PLOT_FLUX (slopefield_data),
							 0, 0, GTK_PLOT_SYMBOL_NONE);

				gtk_plot_flux_show_scale (GTK_PLOT_FLUX (slopefield_data), FALSE);

				label = label_func (-1, slopefield_func, "x,y", slopefield_name);
				/* FIXME: gtkextra is broken (adding the "  ")
				 * and I don't feel like fixing it */
				tmp = g_strconcat ("dy/dx = ", label, "  ", NULL);
				g_free (label);
				gtk_plot_data_set_legend (slopefield_data, tmp);
				g_free (tmp);

				gtk_widget_show (GTK_WIDGET (slopefield_data));
			}
			gtk_plot_data_set_points (slopefield_data,
						  plot_points_x,
						  plot_points_y,
						  plot_points_dx,
						  plot_points_dy,
						  plot_points_num);
		}

		/* sanity */
		g_return_if_fail (vectorfield_func_x == NULL && vectorfield_func_x == NULL);
	}

	if (vectorfield_func_x != NULL && vectorfield_func_y != NULL) {
		get_vectorfield_points ();
		if (plot_points_num > 0) {
			GdkColor color;

			if (vectorfield_data == NULL) {
				char *l1, *l2, *tmp;

				vectorfield_data = GTK_PLOT_DATA(gtk_plot_flux_new());
				gtk_plot_add_data (GTK_PLOT (line_plot),
						   vectorfield_data);
				gdk_color_parse ("blue", &color);
				gtk_plot_data_set_line_attributes (vectorfield_data,
								   GTK_PLOT_LINE_NONE,
								   0, 0, 1, &color);
				gtk_plot_data_set_symbol (vectorfield_data,
							  GTK_PLOT_SYMBOL_NONE /* symbol type? */,
							  GTK_PLOT_SYMBOL_EMPTY /* symbol style */,
							  1 /* size? */,
							  (plotHtick > 15 || plotVtick > 15) ? 1 : 2
							    /* line_width */,
							  &color /* color */,
							  &color /* border_color? */);


				gtk_plot_flux_set_arrow (GTK_PLOT_FLUX (vectorfield_data),
							 6, 6, GTK_PLOT_SYMBOL_EMPTY);

				gtk_plot_flux_show_scale (GTK_PLOT_FLUX (vectorfield_data), FALSE);

				l1 = label_func (-1, vectorfield_func_x, "x,y", vectorfield_name_x);
				l2 = label_func (-1, vectorfield_func_y, "x,y", vectorfield_name_y);
				/* FIXME: gtkextra is broken (adding the "  ")
				 * and I don't feel like fixing it */
				tmp = g_strconcat ("dx/dt = ", l1, ",  dy/dt = ", l2, "  ", NULL);
				g_free (l1);
				g_free (l2);
				gtk_plot_data_set_legend (vectorfield_data, tmp);
				g_free (tmp);

				gtk_widget_show (GTK_WIDGET (vectorfield_data));
			}
			gtk_plot_data_set_points (vectorfield_data,
						  plot_points_x,
						  plot_points_y,
						  plot_points_dx,
						  plot_points_dy,
						  plot_points_num);
		}
	}

}

static void
init_plot_ctx (void)
{
	if G_UNLIKELY (plot_ctx == NULL) {
		mpw_t xx;

		plot_ctx = eval_get_context ();

		mpw_init (xx);
		plot_arg = gel_makenum_use (xx);

		mpw_init (xx);
		plot_arg2 = gel_makenum_use (xx);

		mpw_init (xx);
		plot_arg3 = gel_makenum_use (xx);
	}
}

static void
plot_functions (gboolean do_window_present)
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
		"yellow", /* should never get here, but just for sanity */
		"orange",
		NULL };
	int i;
	int color_i;

	ensure_window (do_window_present);

	if (plot_canvas != NULL /* sanity */)
		gtk_plot_canvas_freeze (GTK_PLOT_CANVAS (plot_canvas));

	clear_graph ();

	add_line_plot ();

	GTK_PLOT_CANVAS_SET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				   GTK_PLOT_CANVAS_CAN_SELECT);

	plot_in_progress ++;
	plot_window_setup ();

	/* sanity */
	if (plotx2 < plotx1) {
		double t = plotx2;
		plotx2 = plotx1;
		plotx1 = t;
	}
	if (ploty2 < ploty1) {
		double t = ploty2;
		ploty2 = ploty1;
		ploty1 = t;
	}

	/* sanity */
	if (plotx2 - plotx1 < MINPLOT)
		plotx2 = plotx1 + MINPLOT;
	/* sanity */
	if (ploty2 - ploty1  < MINPLOT)
		ploty2 = ploty1 + MINPLOT;

	plot_maxy = - G_MAXDOUBLE/2;
	plot_miny = G_MAXDOUBLE/2;

	plot_setup_axis ();

	init_plot_ctx ();

	if (evalnode_hook != NULL)
		(*evalnode_hook)();

	color_i = 0;

	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		GdkColor color;
		char *label;

		line_data[i] = GTK_PLOT_DATA
			(gtk_plot_data_new_function (plot_func_data));
		gtk_plot_add_data (GTK_PLOT (line_plot),
				   line_data[i]);

		gtk_widget_show (GTK_WIDGET (line_data[i]));

		gdk_color_parse (colors[color_i++], &color);
		gdk_color_alloc (gdk_colormap_get_system (), &color); 
		gtk_plot_data_set_line_attributes (line_data[i],
						   GTK_PLOT_LINE_SOLID,
						   0, 0, 2, &color);

		label = label_func (i, plot_func[i], "x", plot_func_name[i]);
		gtk_plot_data_set_legend (line_data[i], label);
		g_free (label);
	}

	if ((parametric_func_x != NULL && parametric_func_y != NULL) ||
	    (parametric_func_z != NULL)) {
		GdkColor color;
		char *label;
		int len;
		double *x, *y, *dx, *dy;
		double t;

		parametric_data = GTK_PLOT_DATA (gtk_plot_data_new ());

		/* could be one off, will adjust later */
		len = MAX(ceil (((plott2 - plott1) / plottinc)) + 2,1);
		x = g_new0 (double, len);
		y = g_new0 (double, len);
		dx = g_new0 (double, len);
		dy = g_new0 (double, len);

		t = plott1;
		for (i = 0; i < len; i++) {
			parametric_get_value (&(x[i]), &(y[i]), t);

			if G_UNLIKELY (interrupted) {
				break;
			}

			t = t + plottinc;
			if (t >= plott2) {
				i++;
				parametric_get_value (&(x[i]), &(y[i]), plott2);
				i++;
				break;
			}
		}
		/* how many actually went */
		len = MAX(1,i);

		gtk_plot_data_set_points (parametric_data, x, y, dx, dy, len);
		g_object_set_data_full (G_OBJECT (parametric_data),
					"x", x, (GDestroyNotify)g_free);
		g_object_set_data_full (G_OBJECT (parametric_data),
					"y", y, (GDestroyNotify)g_free);
		g_object_set_data_full (G_OBJECT (parametric_data),
					"dx", dx, (GDestroyNotify)g_free);
		g_object_set_data_full (G_OBJECT (parametric_data),
					"dy", dy, (GDestroyNotify)g_free);
		gtk_plot_add_data (GTK_PLOT (line_plot), parametric_data);

		gtk_widget_show (GTK_WIDGET (parametric_data));

		gdk_color_parse (colors[color_i++], &color);
		gdk_color_alloc (gdk_colormap_get_system (), &color); 
		gtk_plot_data_set_line_attributes (parametric_data,
						   GTK_PLOT_LINE_SOLID,
						   0, 0, 2, &color);

		if (parametric_name != NULL) {
			label = g_strdup (parametric_name);
		} else if (parametric_func_z) {
			label = label_func (-1, parametric_func_z, "t", NULL);
		} else {
			char *l1, *l2;
			l1 = label_func (-1, parametric_func_x, "t", NULL);
			l2 = label_func (-1, parametric_func_y, "t", NULL);
			label = g_strconcat (l1, ", ", l2, NULL);
			g_free (l1);
			g_free (l2);
		}
		gtk_plot_data_set_legend (parametric_data, label);
		g_free (label);
	} 

	replot_fields ();

	if (lineplot_draw_legends)
		gtk_plot_show_legends (GTK_PLOT (line_plot));
	else
		gtk_plot_hide_legends (GTK_PLOT (line_plot));

	line_plot_move_about ();

	/* could be whacked by closing the window or some such */
	if (plot_canvas != NULL) {
		gtk_plot_canvas_thaw (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

	plot_in_progress --;
	plot_window_setup ();
}

static void
plot_surface_functions (gboolean do_window_present)
{
	ensure_window (do_window_present);

	clear_graph ();

	add_surface_plot ();

	if (plot_canvas != NULL /* sanity */)
		gtk_plot_canvas_freeze (GTK_PLOT_CANVAS (plot_canvas));

	GTK_PLOT_CANVAS_UNSET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				     GTK_PLOT_CANVAS_CAN_SELECT);

	plot_in_progress ++;
	plot_window_setup ();

	/* sanity */
	if (surfacex2 == surfacex1)
		surfacex2 = surfacex1 + MINPLOT;
	if (surfacey2 == surfacey1)
		surfacey2 = surfacey1 + MINPLOT;
	if (surfacez2 == surfacez1)
		surfacez2 = surfacez1 + MINPLOT;

	plot_maxy = - G_MAXDOUBLE/2;
	plot_miny = G_MAXDOUBLE/2;

	surface_setup_axis ();

	gtk_plot3d_reset_angles (GTK_PLOT3D (surface_plot));
	gtk_plot3d_rotate_x (GTK_PLOT3D (surface_plot), 60.0);
	gtk_plot3d_rotate_z (GTK_PLOT3D (surface_plot), 30.0);

	init_plot_ctx ();

	if (evalnode_hook != NULL)
		(*evalnode_hook)();


	if (surface_func != NULL) {
		char *label;

		surface_data = GTK_PLOT_DATA
			(gtk_plot_surface_new_function (surface_func_data));
		gtk_plot_surface_use_amplitud (GTK_PLOT_SURFACE (surface_data), FALSE);
		gtk_plot_surface_use_height_gradient (GTK_PLOT_SURFACE (surface_data), TRUE);
		gtk_plot_surface_set_mesh_visible (GTK_PLOT_SURFACE (surface_data), TRUE);
		gtk_plot_data_gradient_set_visible (GTK_PLOT_DATA (surface_data), TRUE);
		gtk_plot_data_move_gradient (GTK_PLOT_DATA (surface_data),
					     0.93, 0.15);
		gtk_plot_axis_hide_title (GTK_PLOT_DATA (surface_data)->gradient);

		gtk_plot_add_data (GTK_PLOT (surface_plot),
				   surface_data);

		surface_setup_steps ();

		gtk_widget_show (GTK_WIDGET (surface_data));

		label = label_func (-1, surface_func, /* FIXME: correct variable */ "...", surface_func_name);
		gtk_plot_data_set_legend (surface_data, label);
		g_free (label);
	}

	/* FIXME: this doesn't work (crashes) must fix in GtkExtra
	gtk_plot3d_autoscale (GTK_PLOT3D (surface_plot));
	*/

	/* could be whacked by closing the window or some such */
	if (plot_canvas != NULL) {
		gtk_plot_canvas_thaw (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

	plot_in_progress --;
	plot_window_setup ();
}

/*exact answer callback*/
static void
double_spin_cb(GtkAdjustment *adj, double *data)
{
	*data = adj->value;
}

/*exact answer callback*/
static void
int_spin_cb(GtkAdjustment *adj, int *data)
{
	*data = (int)(adj->value);
}

static void
entry_activate (void)
{
	if (plot_dialog != NULL)
		gtk_dialog_response (GTK_DIALOG (plot_dialog),
				     RESPONSE_PLOT);
}

static GtkWidget *
create_range_spinboxes (const char *title, double *val1, GtkWidget **w1,
			double min1, double max1, double step1,
			const char *totitle, double *val2, GtkWidget **w2,
			double min2, double max2, double step2,
			const char *bytitle, double *by, GtkWidget **wb,
			double minby, double maxby, double stepby,
			GCallback activate_callback)
{
	GtkWidget *b, *w;
	GtkAdjustment *adj;

	b = gtk_hbox_new (FALSE, GENIUS_PAD);
	w = gtk_label_new(title);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *)gtk_adjustment_new (*val1,
						   min1,
						   max1,
						   step1,
						   step1*10,
						   0);
	w = gtk_spin_button_new (adj, step1, 5);
	if (w1 != NULL) {
		*w1 = w;
		g_signal_connect (G_OBJECT (w),
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  w1);
	}
	g_signal_connect (G_OBJECT (w), "activate",
			  G_CALLBACK (gtk_spin_button_update), NULL);
	g_signal_connect (G_OBJECT (w), "activate",
			  G_CALLBACK (activate_callback), NULL);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (double_spin_cb), val1);

	if (val2 != NULL) {
		w = gtk_label_new (totitle);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
		adj = (GtkAdjustment *)gtk_adjustment_new (*val2,
							   min2,
							   max2,
							   step2,
							   step2*10,
							   0);
		w = gtk_spin_button_new (adj, step2, 5);
		if (w2 != NULL) {
			*w2 = w;
			g_signal_connect (G_OBJECT (w),
					  "destroy",
					  G_CALLBACK (gtk_widget_destroyed),
					  w2);
		}
		g_signal_connect (G_OBJECT (w), "activate",
				  G_CALLBACK (gtk_spin_button_update), NULL);
		g_signal_connect (G_OBJECT (w), "activate",
				  G_CALLBACK (activate_callback), NULL);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
		gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
		gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (adj), "value_changed",
				  G_CALLBACK (double_spin_cb), val2);
	}

	if (by != NULL) {
		w = gtk_label_new (bytitle);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
		adj = (GtkAdjustment *)gtk_adjustment_new (*by,
							   minby,
							   maxby,
							   stepby,
							   stepby*10,
							   0);
		w = gtk_spin_button_new (adj, stepby, 5);
		if (wb != NULL) {
			*wb = w;
			g_signal_connect (G_OBJECT (w),
					  "destroy",
					  G_CALLBACK (gtk_widget_destroyed),
					  wb);
		}
		g_signal_connect (G_OBJECT (w), "activate",
				  G_CALLBACK (gtk_spin_button_update), NULL);
		g_signal_connect (G_OBJECT (w), "activate",
				  G_CALLBACK (activate_callback), NULL);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
		gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
		gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), FALSE);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (adj), "value_changed",
				  G_CALLBACK (double_spin_cb), by);
	}

	return b;
}

static GtkWidget *
create_int_spinbox (const char *title, int *val, int min, int max)
{
	GtkWidget *b, *w;
	GtkAdjustment *adj;

	b = gtk_hbox_new (FALSE, GENIUS_PAD);
	w = gtk_label_new(title);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	adj = (GtkAdjustment *)gtk_adjustment_new (*val,
						   min,
						   max,
						   1,
						   10,
						   0);
	w = gtk_spin_button_new (adj, 1.0, 0);
	g_signal_connect (G_OBJECT (w), "activate",
			  G_CALLBACK (gtk_spin_button_update), NULL);
	g_signal_connect (G_OBJECT (w), "activate",
			  G_CALLBACK (entry_activate), NULL);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (w), TRUE);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (w), GTK_UPDATE_ALWAYS);
	gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (w), TRUE);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (adj), "value_changed",
			  G_CALLBACK (int_spin_cb), val);

	return b;
}

static GtkWidget *
create_expression_box (const char *label,
		       GtkWidget **entry,
		       GtkWidget **status)
{
	GtkWidget *b;

	b = gtk_hbox_new (FALSE, GENIUS_PAD);

	gtk_box_pack_start (GTK_BOX (b),
			    gtk_label_new (label), FALSE, FALSE, 0);

	*entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (*entry), "activate",
			  G_CALLBACK (entry_activate), NULL);
	gtk_box_pack_start (GTK_BOX (b), *entry, TRUE, TRUE, 0);

	*status = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (b), *status, FALSE, FALSE, 0);
	return b;
}

/*option callback*/
static void
optioncb (GtkWidget * widget, int *data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active)
		*data = TRUE;
	else
		*data = FALSE;
}

static GtkWidget *
create_lineplot_box (void)
{
	GtkWidget *mainbox, *frame;
	GtkWidget *box, *b, *fb, *w;
	int i;


	mainbox = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (mainbox), GENIUS_PAD);

	function_notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (mainbox), function_notebook, FALSE, FALSE, 0);

	/*
	 * Line plot entries
	 */
	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	w = gtk_label_new (_("Type in function names or expressions involving "
			     "the x variable in the boxes below to graph "
			     "them"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_widget_set_size_request (w, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	fb = box;

	if (gdk_screen_height () < 800) {
		w = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
						GTK_POLICY_NEVER,
						GTK_POLICY_ALWAYS);
		gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

		b = gtk_viewport_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (w), b);

		fb = gtk_vbox_new (FALSE, GENIUS_PAD);
		gtk_container_set_border_width (GTK_CONTAINER (fb), GENIUS_PAD);

		gtk_container_add (GTK_CONTAINER (b), fb);
	}



	for (i = 0; i < MAXFUNC; i++) {
		b = create_expression_box ("y=",
					   &(plot_entries[i]),
					   &(plot_entries_status[i]));
		gtk_box_pack_start (GTK_BOX (fb), b, FALSE, FALSE, 0);
	}

	gtk_notebook_append_page (GTK_NOTEBOOK (function_notebook),
				  box,
				  gtk_label_new_with_mnemonic (_("_Functions / Expressions")));

	/*
	 * Parametric plot entries
	 */

	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	w = gtk_label_new (_("Type in function names or expressions involving "
			     "the t variable in the boxes below to graph "
			     "them.  Either fill in both boxes with x= and y= "
			     "in front of them giving the x and y coordinates "
			     "separately, or alternatively fill in the z= box "
			     "giving x and y as the real and imaginary part of "
			     "a complex number."));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_widget_set_size_request (w, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	/* x */
	b = create_expression_box ("x=",
				   &parametric_entry_x,
				   &parametric_status_x);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* y */
	b = create_expression_box ("y=",
				   &parametric_entry_y,
				   &parametric_status_y);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	w = gtk_label_new (_("or"));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	/* z */
	b = create_expression_box ("z=",
				   &parametric_entry_z,
				   &parametric_status_z);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* just spacing */
	gtk_box_pack_start (GTK_BOX (box), gtk_label_new (""), FALSE, FALSE, 0);

	/* t range */
	b = create_range_spinboxes (_("Parameter t from:"), &spint1, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("to:"), &spint2, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("by:"), &spintinc, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (function_notebook),
				  box,
				  gtk_label_new_with_mnemonic (_("Pa_rametric")));

	/*
	 * Slopefield
	 */

	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	w = gtk_label_new (_("Type in function name or expression involving "
			     "the x and y variables (or the z variable which will be z=x+iy) "
			     "that gives the slope "
			     "at the point (x,y)."));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_widget_set_size_request (w, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	/* dy/dx */
	b = create_expression_box ("dy/dx=",
				   &slopefield_entry,
				   &slopefield_status);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* # of ticks */
	b = create_int_spinbox (_("Vertical ticks:"), &spinSVtick, 2, 50);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/* # of ticks */
	b = create_int_spinbox (_("Horizontal ticks:"), &spinSHtick, 2, 50);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (function_notebook),
				  box,
				  gtk_label_new_with_mnemonic (_("Sl_ope field")));

	/*
	 * Vectorfield
	 */

	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	w = gtk_label_new (_("Type in function names or expressions involving "
			     "the x and y variables (or the z variable which will be z=x+iy) "
			     "that give the dx/dt and dy/dt of the autonomous system to be plotted "
			     "at the point (x,y)."));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_widget_set_size_request (w, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	/* dx/dt */
	b = create_expression_box ("dx/dt=",
				   &vectorfield_entry_x,
				   &vectorfield_status_x);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* dy/dt */
	b = create_expression_box ("dy/dt=",
				   &vectorfield_entry_y,
				   &vectorfield_status_y);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* Normalize the arrow length? */
	w = gtk_check_button_new_with_mnemonic (_("_Normalize arrow length (do not show size)"));
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      vectorfield_normalize_arrow_length_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&vectorfield_normalize_arrow_length_cb);

	/* # of ticks */
	b = create_int_spinbox (_("Vertical ticks:"), &spinVVtick, 2, 50);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/* # of ticks */
	b = create_int_spinbox (_("Horizontal ticks:"), &spinVHtick, 2, 50);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (function_notebook),
				  box,
				  gtk_label_new_with_mnemonic (_("_Vector field")));

	/*
	 * Below notebook
	 */

	w = gtk_check_button_new_with_mnemonic (_("_Draw legend"));
	gtk_box_pack_start (GTK_BOX (mainbox), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      lineplot_draw_legends_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&lineplot_draw_legends_cb);


	frame = gtk_frame_new (_("Plot Window"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/*
	 * X range
	 */
	b = create_range_spinboxes (_("X from:"), &spinx1, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("to:"), &spinx2, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    NULL, NULL, NULL, 0, 0, 0,
				    entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/*
	 * Y range
	 */
	b = create_range_spinboxes (_("Y from:"), &spiny1, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("to:"), &spiny2, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    NULL, NULL, NULL, 0, 0, 0,
				    entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	return mainbox;
}

static GtkWidget *
create_surface_box (void)
{
	GtkWidget *mainbox, *frame;
	GtkWidget *box, *b, *w;

	mainbox = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (mainbox), GENIUS_PAD);
	
	frame = gtk_frame_new (_("Function / Expression"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);
	w = gtk_label_new (_("Type a function name or an expression involving "
			     "the x and y variables (or the z variable which will be z=x+iy) "
			     "in the boxes below to graph them.  Functions with one argument only "
			     "will be passed a complex number."));
	gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
	gtk_widget_set_size_request (w, 610, -1);
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);

	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	b = gtk_hbox_new (FALSE, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	surface_entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (surface_entry), "activate",
			  G_CALLBACK (entry_activate), NULL);
	gtk_box_pack_start (GTK_BOX (b), surface_entry, TRUE, TRUE, 0);

	surface_entry_status = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (b), surface_entry_status, FALSE, FALSE, 0);

	frame = gtk_frame_new (_("Plot Window"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_vbox_new (FALSE, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/*
	 * X range
	 */
	b = create_range_spinboxes (_("X from:"), &surf_spinx1, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("to:"), &surf_spinx2, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    NULL, NULL, NULL, 0, 0, 0,
				    entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/*
	 * Y range
	 */
	b = create_range_spinboxes (_("Y from:"), &surf_spiny1, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("to:"), &surf_spiny2, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    NULL, NULL, NULL, 0, 0, 0,
				    entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/*
	 * Z range
	 */
	b = create_range_spinboxes (_("Z from:"), &surf_spinz1, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    _("to:"), &surf_spinz2, NULL,
				    -G_MAXDOUBLE, G_MAXDOUBLE, 1,
				    NULL, NULL, NULL, 0, 0, 0,
				    entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	return mainbox;
}

static GtkWidget *
create_plot_dialog (void)
{
	plot_notebook = gtk_notebook_new ();
	
	gtk_notebook_append_page (GTK_NOTEBOOK (plot_notebook),
				  create_lineplot_box (),
				  gtk_label_new_with_mnemonic (_("Function _line plot")));

	gtk_notebook_append_page (GTK_NOTEBOOK (plot_notebook),
				  create_surface_box (),
				  gtk_label_new_with_mnemonic (_("_Surface plot")));
	
	return plot_notebook;
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
function_from_expression (const char *e, const char *var, gboolean *ex)
{
	GelEFunc *f = NULL;
	GelETree *value;
	char *ce;

	if (ve_string_empty (e))
		return NULL;

	ce = g_strstrip (g_strdup (e));
	if (is_identifier (ce) && strcmp (ce, var) != 0) {
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

	/* FIXME: if "x" (var) not used try to evaluate and if it returns a function use that */

	if (value != NULL) {
		f = d_makeufunc (NULL /* id */,
				 value,
				 g_slist_append (NULL, d_intern (var)),
				 1,
				 NULL /* extra_dict */);
	}

	if (f == NULL)
		*ex = TRUE;

	return f;
}

static GelEFunc *
function_from_expression2 (const char *e, gboolean *ex)
{
	GelEFunc *f = NULL;
	GelETree *value;
	char *ce;
	gboolean got_x, got_y, got_z;

	if (ve_string_empty (e))
		return NULL;

	ce = g_strstrip (g_strdup (e));
	if (is_identifier (ce) &&
	    strcmp (ce, "x") != 0 &&
	    strcmp (ce, "y") != 0 &&
	    strcmp (ce, "z") != 0) {
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

	/* FIXME: funcbody?  I think it must be done. */
	got_x = eval_find_identifier (value, d_intern ("x"), TRUE /*funcbody*/);
	got_y = eval_find_identifier (value, d_intern ("y"), TRUE /*funcbody*/);
	got_z = eval_find_identifier (value, d_intern ("z"), TRUE /*funcbody*/);

	/* FIXME: if "x" or "y" or "z" not used try to evaluate and if it returns a function use that */
	if (value != NULL) {
		if ( ! got_x && ! got_y && got_z) {
			f = d_makeufunc (NULL /* id */,
					 value,
					 g_slist_append (NULL, d_intern ("z")),
					 1,
					 NULL /* extra_dict */);
		} else if ( ! got_z) {
			GSList *l = g_slist_append (NULL, d_intern ("x"));
			l = g_slist_append (l, d_intern ("y"));
			f = d_makeufunc (NULL /* id */,
					 value,
					 l,
					 2,
					 NULL /* extra_dict */);
		} else {
			GSList *l = g_slist_append (NULL, d_intern ("x"));
			l = g_slist_append (l, d_intern ("y"));
			l = g_slist_append (l, d_intern ("z"));
			f = d_makeufunc (NULL /* id */,
					 value,
					 l,
					 3,
					 NULL /* extra_dict */);
		}
	}

	if (f == NULL)
		*ex = TRUE;

	return f;
}


static GelEFunc *
get_func_from_entry (GtkWidget *entry, GtkWidget *status,
		     const char *var, gboolean *ex)
{
	GelEFunc *f;
	const char *str = gtk_entry_get_text (GTK_ENTRY (entry));
	f = function_from_expression (str, var, ex);
	if (f != NULL) {
		gtk_image_set_from_stock
			(GTK_IMAGE (status),
			 GTK_STOCK_YES,
			 GTK_ICON_SIZE_MENU);
	} else if (*ex) {
		gtk_image_set_from_stock
			(GTK_IMAGE (status),
			 GTK_STOCK_DIALOG_WARNING,
			 GTK_ICON_SIZE_MENU);
		f = NULL;
	} else {
		gtk_image_set_from_pixbuf
			(GTK_IMAGE (status),
			 NULL);
		f = NULL;
	}
	return f;
}

static GelEFunc *
get_func_from_entry2 (GtkWidget *entry, GtkWidget *status,
		      gboolean *ex)
{
	GelEFunc *f;
	const char *str = gtk_entry_get_text (GTK_ENTRY (entry));
	f = function_from_expression2 (str, ex);
	if (f != NULL) {
		gtk_image_set_from_stock
			(GTK_IMAGE (status),
			 GTK_STOCK_YES,
			 GTK_ICON_SIZE_MENU);
	} else if (*ex) {
		gtk_image_set_from_stock
			(GTK_IMAGE (status),
			 GTK_STOCK_DIALOG_WARNING,
			 GTK_ICON_SIZE_MENU);
		f = NULL;
	} else {
		gtk_image_set_from_pixbuf
			(GTK_IMAGE (status),
			 NULL);
		f = NULL;
	}
	return f;
}

static void
surface_from_dialog (void)
{
	GelEFunc *func = { NULL };
	double x1, x2, y1, y2, z1, z2;
	gboolean last_info;
	gboolean last_error;
	const char *error_to_print = NULL;
	gboolean ex;

	plot_mode = MODE_SURFACE;

	last_info = genius_setup.info_box;
	last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;

	ex = FALSE;
	func = get_func_from_entry2 (surface_entry, surface_entry_status, &ex);

	if (func == NULL) {
		error_to_print = _("No functions to plot or no functions "
				   "could be parsed");
		goto whack_copied_funcs;
	}

	x1 = surf_spinx1;
	x2 = surf_spinx2;
	y1 = surf_spiny1;
	y2 = surf_spiny2;
	z1 = surf_spinz1;
	z2 = surf_spinz2;

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

	if (z1 > z2) {
		double s = z1;
		z1 = z2;
		z2 = s;
	}

	if (x1 == x2) {
		error_to_print = _("Invalid X range");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = _("Invalid Y range");
		goto whack_copied_funcs;
	}

	if (z1 == z2) {
		error_to_print = _("Invalid Z range");
		goto whack_copied_funcs;
	}

	reset_surfacex1 = surfacex1 = x1;
	reset_surfacex2 = surfacex2 = x2;
	reset_surfacey1 = surfacey1 = y1;
	reset_surfacey2 = surfacey2 = y2;
	reset_surfacez1 = surfacez1 = z1;
	reset_surfacez2 = surfacez2 = z2;

	if (surface_func != NULL) {
		d_freefunc (surface_func);
		surface_func = NULL;
	}
	g_free (surface_func_name);
	surface_func_name = NULL;

	surface_func = func;
	func = NULL;

	/* setup name when the functions don't have their own name */
	if (surface_func->id == NULL)
		surface_func_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (surface_entry)));

	plot_mode = MODE_SURFACE;
	plot_surface_functions (TRUE /* do_window_present */);

	if (interrupted)
		interrupted = FALSE;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	return;

whack_copied_funcs:
	if (func != NULL) {
		d_freefunc (func);
		func = NULL;
	}

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	if (error_to_print != NULL)
		genius_display_error (plot_dialog, error_to_print);
}

static void
line_plot_clear_funcs (void)
{
	int i;

	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		d_freefunc (plot_func[i]);
		plot_func[i] = NULL;
		g_free (plot_func_name[i]);
		plot_func_name[i] = NULL;
	}

	d_freefunc (parametric_func_x);
	parametric_func_x = NULL;
	d_freefunc (parametric_func_y);
	parametric_func_y = NULL;
	d_freefunc (parametric_func_z);
	parametric_func_z = NULL;
	g_free (parametric_name);
	parametric_name = NULL;

	d_freefunc (vectorfield_func_x);
	vectorfield_func_x = NULL;
	d_freefunc (vectorfield_func_y);
	vectorfield_func_y = NULL;
	g_free (vectorfield_name_x);
	vectorfield_name_x = NULL;
	g_free (vectorfield_name_y);
	vectorfield_name_y = NULL;

	d_freefunc (slopefield_func);
	slopefield_func = NULL;
	g_free (slopefield_name);
	slopefield_name = NULL;
}

static void
plot_from_dialog_lineplot (void)
{
	int funcs = 0;
	GelEFunc *func[MAXFUNC] = { NULL };
	double x1, x2, y1, y2;
	int i, j;
	gboolean last_info;
	gboolean last_error;
	const char *error_to_print = NULL;

	plot_mode = MODE_LINEPLOT;

	last_info = genius_setup.info_box;
	last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;

	for (i = 0; i < MAXFUNC; i++) {
		GelEFunc *f;
		gboolean ex = FALSE;
		f = get_func_from_entry (plot_entries[i],
					 plot_entries_status[i],
					 "x",
					 &ex);
		if (f != NULL) {
			func[i] = f;
			funcs++;
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

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	line_plot_clear_funcs ();

	for (i = 0; i < MAXFUNC; i++) {
		if (func[i] != NULL) {
			plot_func[j] = func[i];
			func[i] = NULL;
			/* setup name when the functions don't have their own name */
			if (plot_func[j]->id == NULL)
				plot_func_name[j] = g_strdup (gtk_entry_get_text (GTK_ENTRY (plot_entries[i])));
			j++;
		}
	}

	plot_functions (TRUE /* do_window_present */);

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
		genius_display_error (plot_dialog, error_to_print);
}

static void
plot_from_dialog_parametric (void)
{
	GelEFunc *funcpx = NULL;
	GelEFunc *funcpy = NULL;
	GelEFunc *funcpz = NULL;
	double x1, x2, y1, y2;
	gboolean last_info;
	gboolean last_error;
	const char *error_to_print = NULL;
	gboolean exx = FALSE;
	gboolean exy = FALSE;
	gboolean exz = FALSE;

	plot_mode = MODE_LINEPLOT_PARAMETRIC;

	last_info = genius_setup.info_box;
	last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;

	funcpx = get_func_from_entry (parametric_entry_x,
				      parametric_status_x,
				      "t",
				      &exx);
	funcpy = get_func_from_entry (parametric_entry_y,
				      parametric_status_y,
				      "t",
				      &exy);
	funcpz = get_func_from_entry (parametric_entry_z,
				      parametric_status_z,
				      "t",
				      &exz);
	if (((funcpx || exx) || (funcpy || exy)) && (funcpz || exz)) {
		error_to_print = _("Only specify x and y, or z, not all at once.");
		goto whack_copied_funcs;
	}

	if ( ! ( (funcpz == NULL && funcpx != NULL && funcpy != NULL) ||
		 (funcpz != NULL && funcpx == NULL && funcpy == NULL))) {
		error_to_print = _("No functions to plot or no functions "
				   "could be parsed");
		goto whack_copied_funcs;
	}

	if (spint1 >= spint2 ||
	    spintinc <= 0.0) {
		error_to_print = _("Invalid t range");
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

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	plott1 = spint1;
	plott2 = spint2;
	plottinc = spintinc;

	line_plot_clear_funcs ();

	parametric_func_x = funcpx;
	parametric_func_y = funcpy;
	parametric_func_z = funcpz;
	if (funcpz != NULL) {
		parametric_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (parametric_entry_z)));
	} else {
		parametric_name = g_strconcat (gtk_entry_get_text (GTK_ENTRY (parametric_entry_x)),
					       ",",
					       gtk_entry_get_text (GTK_ENTRY (parametric_entry_y)),
					       NULL);
	}

	plot_functions (TRUE /* do_window_present */);

	if (interrupted)
		interrupted = FALSE;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	return;

whack_copied_funcs:
	d_freefunc (funcpx);
	funcpx = NULL;
	d_freefunc (funcpy);
	funcpy = NULL;
	d_freefunc (funcpz);
	funcpz = NULL;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	if (error_to_print != NULL)
		genius_display_error (plot_dialog, error_to_print);
}

static void
plot_from_dialog_slopefield (void)
{
	GelEFunc *funcp = NULL;
	double x1, x2, y1, y2;
	gboolean last_info;
	gboolean last_error;
	const char *error_to_print = NULL;
	gboolean ex = FALSE;

	plot_mode = MODE_LINEPLOT_SLOPEFIELD;

	last_info = genius_setup.info_box;
	last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;

	ex = FALSE;
	funcp = get_func_from_entry2 (slopefield_entry, slopefield_status, &ex);

	if (funcp == NULL) {
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

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	plotVtick = spinSVtick;
	plotHtick = spinSHtick;

	line_plot_clear_funcs ();

	slopefield_func = funcp;
	slopefield_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (slopefield_entry)));

	plot_functions (TRUE /* do_window_present */);

	if (interrupted)
		interrupted = FALSE;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	return;

whack_copied_funcs:
	d_freefunc (funcp);
	funcp = NULL;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	if (error_to_print != NULL)
		genius_display_error (plot_dialog, error_to_print);
}

static void
plot_from_dialog_vectorfield (void)
{
	GelEFunc *funcpx = NULL;
	GelEFunc *funcpy = NULL;
	double x1, x2, y1, y2;
	gboolean last_info;
	gboolean last_error;
	const char *error_to_print = NULL;
	gboolean ex = FALSE;

	plot_mode = MODE_LINEPLOT_VECTORFIELD;

	last_info = genius_setup.info_box;
	last_error = genius_setup.error_box;
	genius_setup.info_box = TRUE;
	genius_setup.error_box = TRUE;

	vectorfield_normalize_arrow_length = 
		vectorfield_normalize_arrow_length_cb;

	ex = FALSE;
	funcpx = get_func_from_entry2 (vectorfield_entry_x, vectorfield_status_x, &ex);
	ex = FALSE;
	funcpy = get_func_from_entry2 (vectorfield_entry_y, vectorfield_status_y, &ex);

	if (funcpx == NULL || funcpy == NULL) {
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

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	plotVtick = spinVVtick;
	plotHtick = spinVHtick;

	line_plot_clear_funcs ();

	vectorfield_func_x = funcpx;
	vectorfield_func_y = funcpy;
	vectorfield_name_x = g_strdup (gtk_entry_get_text (GTK_ENTRY (vectorfield_entry_x)));
	vectorfield_name_y = g_strdup (gtk_entry_get_text (GTK_ENTRY (vectorfield_entry_y)));

	plot_functions (TRUE /* do_window_present */);

	if (interrupted)
		interrupted = FALSE;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	return;

whack_copied_funcs:
	d_freefunc (funcpx);
	funcpx = NULL;
	d_freefunc (funcpy);
	funcpy = NULL;

	gel_printout_infos ();
	genius_setup.info_box = last_info;
	genius_setup.error_box = last_error;

	if (error_to_print != NULL)
		genius_display_error (plot_dialog, error_to_print);
}

static void
plot_from_dialog (void)
{
	int function_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (function_notebook));

	lineplot_draw_legends = lineplot_draw_legends_cb;

	if (function_page == 0)
		plot_from_dialog_lineplot ();
	else if (function_page == 1)
		plot_from_dialog_parametric ();
	else if (function_page == 2)
		plot_from_dialog_slopefield ();
	else if (function_page == 3)
		plot_from_dialog_vectorfield ();
}

static void
plot_dialog_response (GtkWidget *w, int response, gpointer data)
{
	if (response == GTK_RESPONSE_CLOSE ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		gtk_widget_destroy (plot_dialog);
	} else if (response == RESPONSE_PLOT) {
		int pg = gtk_notebook_get_current_page (GTK_NOTEBOOK (plot_notebook));
		if (pg == 0 /* line plot */)
			plot_from_dialog ();
		else if (pg == 1 /* surface plot */)
			surface_from_dialog ();
	}
}

void
genius_plot_dialog (void)
{
	GtkWidget *insides;

	if (plot_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (plot_dialog));
		return;
	}

	plot_dialog = gtk_dialog_new_with_buttons
		(_("Create Plot") /* title */,
		 NULL /*GTK_WINDOW (genius_window)*/ /* parent */,
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
SurfacePlot_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2, z1, z2;
	int i;
	GelEFunc *func = NULL;

	i = 0;

	if (a[i] != NULL && a[i]->type != FUNCTION_NODE) {
		gel_errorout (_("%s: argument not a function"), "SurfacePlot");
		goto whack_copied_funcs;
	}

	func = d_copyfunc (a[i]->func.func);
	func->context = -1;

	i++;

	if (a[i] != NULL && a[i]->type == FUNCTION_NODE) {
		gel_errorout (_("%s: only one function supported"), "SurfacePlot");
		goto whack_copied_funcs;
	}

	/* Defaults */
	x1 = surf_defx1;
	x2 = surf_defx2;
	y1 = surf_defy1;
	y2 = surf_defy2;
	z1 = surf_defz1;
	z2 = surf_defz2;

	if (a[i] != NULL) {
		if (a[i]->type == MATRIX_NODE) {
			if ( ! get_limits_from_matrix_surf (a[i], &x1, &x2, &y1, &y2, &z1, &z2))
				goto whack_copied_funcs;
			i++;
		} else {
			GET_DOUBLE(x1, i, "SurfacePlot");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "SurfacePlot");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "SurfacePlot");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "SurfacePlot");
						i++;
						if (a[i] != NULL) {
							GET_DOUBLE(z1, i, "SurfacePlot");
							i++;
							if (a[i] != NULL) {
								GET_DOUBLE(z2, i, "SurfacePlot");
								i++;
							}
						}
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

	if (z1 > z2) {
		double s = z1;
		z1 = z2;
		z2 = s;
	}

	if (x1 == x2) {
		gel_errorout (_("%s: invalid X range"), "SurfacePlot");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "SurfacePlot");
		goto whack_copied_funcs;
	}

	if (z1 == z2) {
		gel_errorout (_("%s: invalid Z range"), "SurfacePlot");
		goto whack_copied_funcs;
	}

	if (surface_func != NULL) {
		d_freefunc (surface_func);
	}
	g_free (surface_func_name);
	surface_func_name = NULL;

	surface_func = func;
	func = NULL;

	reset_surfacex1 = surfacex1 = x1;
	reset_surfacex2 = surfacex2 = x2;
	reset_surfacey1 = surfacey1 = y1;
	reset_surfacey2 = surfacey2 = y2;
	reset_surfacez1 = surfacez1 = z1;
	reset_surfacez2 = surfacez2 = z2;

	plot_mode = MODE_SURFACE;
	plot_surface_functions (FALSE /* do_window_present */);

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_funcs:
	if (func != NULL) {
		d_freefunc (func);
		func = NULL;
	}

	return NULL;
}

static GelETree *
SlopefieldDrawSolution_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x, y, dx;

	GET_DOUBLE (x, 0, "SlopefieldDrawSolution");
	GET_DOUBLE (y, 1, "SlopefieldDrawSolution");
	GET_DOUBLE (dx, 2, "SlopefieldDrawSolution");

	if (dx <= 0.0) {
		gel_errorout (_("%s: dx must be positive"),
			      "SlopefieldDrawSolution");
		return NULL;
	}

	if (plot_mode != MODE_LINEPLOT_SLOPEFIELD ||
	    slopefield_func == NULL) {
		gel_errorout (_("%s: Slope field not active"),
			      "SlopefieldDrawSolution");
		return NULL;
	}

	slopefield_draw_solution (x, y, dx);

	return gel_makenum_null ();
}

static GelETree *
SlopefieldClearSolutions_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (plot_mode != MODE_LINEPLOT_SLOPEFIELD) {
		gel_errorout (_("%s: Slope field not active"),
			      "SlopefieldClearSolutions");
		return NULL;
	}

	clear_solutions ();

	return gel_makenum_null ();
}

static GelETree *
VectorfieldDrawSolution_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x, y, dt, tlen;

	GET_DOUBLE (x, 0, "VectorfieldDrawSolution");
	GET_DOUBLE (y, 1, "VectorfieldDrawSolution");
	GET_DOUBLE (dt, 2, "VectorfieldDrawSolution");
	GET_DOUBLE (tlen, 3, "VectorfieldDrawSolution");

	if (dt <= 0.0) {
		gel_errorout (_("%s: dt must be positive"),
			      "VectorfieldDrawSolution");
		return NULL;
	}

	if (tlen <= 0.0) {
		gel_errorout (_("%s: tlen must be positive"),
			      "VectorfieldDrawSolution");
		return NULL;
	}

	if (plot_mode != MODE_LINEPLOT_VECTORFIELD ||
	    vectorfield_func_x == NULL ||
	    vectorfield_func_y == NULL) {
		gel_errorout (_("%s: Vector field not active"),
			      "VectorfieldDrawSolution");
		return NULL;
	}

	vectorfield_draw_solution (x, y, dt, tlen);

	return gel_makenum_null ();
}


static GelETree *
VectorfieldClearSolutions_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (plot_mode != MODE_LINEPLOT_VECTORFIELD) {
		gel_errorout (_("%s: Vector field not active"),
			      "VectorfieldClearSolutions");
		return NULL;
	}

	clear_solutions ();

	return gel_makenum_null ();
}

static GelETree *
SlopefieldPlot_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2;
	GelEFunc *func = NULL;
	int i;

	if (a[0] == NULL ||
	    a[0]->type != FUNCTION_NODE) {
		gel_errorout (_("%s: First argument must be a function"),
			      "SlopefieldPlot");
		return NULL;
	}

	func = d_copyfunc (a[0]->func.func);
	func->context = -1;

	/* Defaults */
	x1 = defx1;
	x2 = defx2;
	y1 = defy1;
	y2 = defy2;

	i = 1;

	/* Get window limits */
	if (a[i] != NULL) {
		if (a[i]->type == MATRIX_NODE) {
			if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
				goto whack_copied_funcs;
			i++;
		} else {
			GET_DOUBLE(x1, i, "SlopefieldPlot");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "SlopefieldPlot");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "SlopefieldPlot");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "SlopefieldPlot");
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
		gel_errorout (_("%s: invalid X range"), "SlopefieldPlot");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "SlopefieldPlot");
		goto whack_copied_funcs;
	}

	line_plot_clear_funcs ();

	slopefield_func = func;

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	lineplot_draw_legends = lineplot_draw_legends_parameter;

	plot_mode = MODE_LINEPLOT_SLOPEFIELD;
	plot_functions (FALSE /* do_window_present */);

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_funcs:
	d_freefunc (func);
	func = NULL;

	return NULL;
}

static GelETree *
VectorfieldPlot_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2;
	GelEFunc *funcx = NULL;
	GelEFunc *funcy = NULL;
	int i;

	/* FIXME: also accept just one function and then treat it as complex
	 * valued */

	if (a[0] == NULL || a[1] == NULL ||
	    a[0]->type != FUNCTION_NODE ||
	    a[1]->type != FUNCTION_NODE) {
		gel_errorout (_("%s: First two arguments must be functions"), "VectorfieldPlot");
		return NULL;
	}

	funcx = d_copyfunc (a[0]->func.func);
	funcx->context = -1;
	funcy = d_copyfunc (a[1]->func.func);
	funcy->context = -1;

	/* Defaults */
	x1 = defx1;
	x2 = defx2;
	y1 = defy1;
	y2 = defy2;

	i = 2;

	/* Get window limits */
	if (a[i] != NULL) {
		if (a[i]->type == MATRIX_NODE) {
			if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
				goto whack_copied_funcs;
			i++;
		} else {
			GET_DOUBLE(x1, i, "VectorfieldPlot");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "VectorfieldPlot");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "VectorfieldPlot");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "VectorfieldPlot");
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
		gel_errorout (_("%s: invalid X range"), "VectorfieldPlot");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "VectorfieldPlot");
		goto whack_copied_funcs;
	}

	line_plot_clear_funcs ();

	vectorfield_func_x = funcx;
	vectorfield_func_y = funcy;

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	lineplot_draw_legends = lineplot_draw_legends_parameter;
	vectorfield_normalize_arrow_length =
		vectorfield_normalize_arrow_length_parameter;

	plot_mode = MODE_LINEPLOT_VECTORFIELD;
	plot_functions (FALSE /* do_window_present */);

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_funcs:
	d_freefunc (funcx);
	funcx = NULL;
	d_freefunc (funcy);
	funcy = NULL;

	return NULL;
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
			GET_DOUBLE(x1, i, "LinePlot");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "LinePlot");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "LinePlot");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "LinePlot");
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

	line_plot_clear_funcs ();

	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		plot_func[i] = func[i];
		func[i] = NULL;
	}

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	lineplot_draw_legends = lineplot_draw_legends_parameter;

	plot_mode = MODE_LINEPLOT;
	plot_functions (FALSE /* do_window_present */);

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
LinePlotParametric_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2, t1, t2, tinc;
	GelEFunc *funcx = NULL;
	GelEFunc *funcy = NULL;
	int i;

	if (a[0] == NULL || a[1] == NULL ||
	    a[0]->type != FUNCTION_NODE ||
	    a[1]->type != FUNCTION_NODE) {
		gel_errorout (_("%s: First two arguments must be functions"), "LinePlotParametric");
		return NULL;
	}

	funcx = d_copyfunc (a[0]->func.func);
	funcx->context = -1;
	funcy = d_copyfunc (a[1]->func.func);
	funcy->context = -1;

	/* Defaults */
	x1 = defx1;
	x2 = defx2;
	y1 = defy1;
	y2 = defy2;
	t1 = deft1;
	t2 = deft2;
	tinc = deftinc;

	i = 2;

	/* Get t limits */
	if (a[i] != NULL) {
		GET_DOUBLE(t1, i, "LinePlotParametric");
		i++;
		if (a[i] != NULL) {
			GET_DOUBLE(t2, i, "LinePlotParametric");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(tinc, i, "LinePlotParametric");
				i++;
			}
		}
		/* FIXME: what about errors */
		if (error_num != 0) {
			error_num = 0;
			goto whack_copied_funcs;
		}
	}

	/* Get window limits */
	if (a[i] != NULL) {
		if (a[i]->type == MATRIX_NODE) {
			if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
				goto whack_copied_funcs;
			i++;
		} else {
			GET_DOUBLE(x1, i, "LinePlotParametric");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "LinePlotParametric");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "LinePlotParametric");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "LinePlotParametric");
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
		gel_errorout (_("%s: invalid X range"), "LinePlotParametric");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "LinePlotParametric");
		goto whack_copied_funcs;
	}

	if (t1 >= t2 || tinc <= 0) {
		gel_errorout (_("%s: invalid T range"), "LinePlotParametric");
		goto whack_copied_funcs;
	}

	line_plot_clear_funcs ();

	parametric_func_x = funcx;
	parametric_func_y = funcy;

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	plott1 = t1;
	plott2 = t2;
	plottinc = tinc;

	lineplot_draw_legends = lineplot_draw_legends_parameter;

	plot_mode = MODE_LINEPLOT_PARAMETRIC;
	plot_functions (FALSE /* do_window_present */);

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_funcs:
	d_freefunc (funcx);
	funcx = NULL;
	d_freefunc (funcy);
	funcy = NULL;

	return NULL;
}

static GelETree *
LinePlotCParametric_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2, t1, t2, tinc;
	GelEFunc *func = NULL;
	int i;

	if (a[0] == NULL ||
	    a[0]->type != FUNCTION_NODE) {
		gel_errorout (_("%s: First argument must be a function"),
			      "LinePlotCParametric");
		return NULL;
	}

	func = d_copyfunc (a[0]->func.func);
	func->context = -1;

	/* Defaults */
	x1 = defx1;
	x2 = defx2;
	y1 = defy1;
	y2 = defy2;
	t1 = deft1;
	t2 = deft2;
	tinc = deftinc;

	i = 1;

	/* Get t limits */
	if (a[i] != NULL) {
		GET_DOUBLE(t1, i, "LinePlotCParametric");
		i++;
		if (a[i] != NULL) {
			GET_DOUBLE(t2, i, "LinePlotCParametric");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(tinc, i, "LinePlotCParametric");
				i++;
			}
		}
		/* FIXME: what about errors */
		if (error_num != 0) {
			error_num = 0;
			goto whack_copied_funcs;
		}
	}

	/* Get window limits */
	if (a[i] != NULL) {
		if (a[i]->type == MATRIX_NODE) {
			if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
				goto whack_copied_funcs;
			i++;
		} else {
			GET_DOUBLE(x1, i, "LinePlotCParametric");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "LinePlotCParametric");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "LinePlotCParametric");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "LinePlotCParametric");
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
		gel_errorout (_("%s: invalid X range"), "LinePlotCParametric");
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "LinePlotCParametric");
		goto whack_copied_funcs;
	}

	if (t1 >= t2 || tinc <= 0) {
		gel_errorout (_("%s: invalid T range"), "LinePlotCParametric");
		goto whack_copied_funcs;
	}

	line_plot_clear_funcs ();

	parametric_func_z = func;

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	plott1 = t1;
	plott2 = t2;
	plottinc = tinc;

	lineplot_draw_legends = lineplot_draw_legends_parameter;

	plot_mode = MODE_LINEPLOT_PARAMETRIC;
	plot_functions (FALSE /* do_window_present */);

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_funcs:
	d_freefunc (func);
	func = NULL;

	return NULL;
}

static GelETree *
LinePlotClear_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	line_plot_clear_funcs ();

	/* This will just clear the window */
	plot_mode = MODE_LINEPLOT;
	plot_functions (FALSE /* do_window_present */);

	if (interrupted)
		return NULL;
	else
		return gel_makenum_null ();
}

static gboolean
get_line_numbers (GelETree *a, double **x, double **y, int *len)
{
	int i;
	GelMatrixW *m;

	g_return_val_if_fail (a->type == MATRIX_NODE, FALSE);

	m = a->mat.matrix;

	if ( ! gel_is_matrix_value_only_real (m)) {
		gel_errorout (_("%s: Line should be given as a real, n by 2 matrix "
				"with columns for x and y, n>=2"),
			      "LinePlotDrawLine");
		return FALSE;
	}

	if (gel_matrixw_width (m) == 2 &&
	    gel_matrixw_height (m) >= 2) {
		*len = gel_matrixw_height (m);

		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			GelETree *t = gel_matrixw_index (m, 0, i);
			(*x)[i] = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 1, i);
			(*y)[i] = mpw_get_double (t->val.value);
		}
	} else if (gel_matrixw_width (m) == 1 &&
		   gel_matrixw_height (m) % 2 == 0 &&
		   gel_matrixw_height (m) >= 4) {
		*len = gel_matrixw_height (m) / 2;

		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			GelETree *t = gel_matrixw_index (m, 0, 2*i);
			(*x)[i] = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 0, (2*i) + 1);
			(*y)[i] = mpw_get_double (t->val.value);
		}
	} else if (gel_matrixw_height (m) == 1 &&
		   gel_matrixw_width (m) % 2 == 0 &&
		   gel_matrixw_width (m) >= 4) {
		*len = gel_matrixw_width (m) / 2;

		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			GelETree *t = gel_matrixw_index (m, 2*i, 0);
			(*x)[i] = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, (2*i) + 1, 0);
			(*y)[i] = mpw_get_double (t->val.value);
		}
	} else {
		gel_errorout (_("%s: Line should be given as a real, n by 2 matrix "
				"with columns for x and y, n>=2"),
			      "LinePlotDrawLine");
		return FALSE;
	}

	return TRUE;
}


static GelETree *
LinePlotDrawLine_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int len;
	int nextarg;
	double *x, *y;
	GdkColor color;
	int thickness;
	int i;

	ensure_window (FALSE /* do_window_present */);

	if (plot_mode != MODE_LINEPLOT &&
	    plot_mode != MODE_LINEPLOT_PARAMETRIC &&
	    plot_mode != MODE_LINEPLOT_SLOPEFIELD &&
	    plot_mode != MODE_LINEPLOT_VECTORFIELD) {
		plot_mode = MODE_LINEPLOT;
		clear_graph ();
	}
	if (line_plot == NULL) {
		add_line_plot ();
		plot_setup_axis ();
	}

	if (a[0]->type == MATRIX_NODE) {
		if ( ! get_line_numbers (a[0], &x, &y, &len))
			return FALSE;
		nextarg = 1;
	} else {
		double x1, y1, x2, y2;
		if G_UNLIKELY (gel_count_arguments (a) < 4) {
			gel_errorout (_("%s: Wrong number of arguments"),
				      "LinePlotDrawLine");
			return NULL;
		}
		GET_DOUBLE(x1, 0, "LinePlotDrawLine");
		GET_DOUBLE(y1, 1, "LinePlotDrawLine");
		GET_DOUBLE(x2, 2, "LinePlotDrawLine");
		GET_DOUBLE(y2, 3, "LinePlotDrawLine");
		len = 2;
		x = g_new (double, 2);
		x[0] = x1;
		x[1] = x2;
		y = g_new (double, 2);
		y[0] = y1;
		y[1] = y2;
		nextarg = 4;
	}

	gdk_color_parse ("black", &color);
	thickness = 2;

	for (i = nextarg; a[i] != NULL; i++) {
		if G_LIKELY (a[i]->type == STRING_NODE ||
			     a[i]->type == IDENTIFIER_NODE) {
			GelToken *id;
			static GelToken *colorid = NULL;
			static GelToken *thicknessid = NULL;

			if (colorid == NULL) {
				colorid = d_intern ("color");
				thicknessid = d_intern ("thickness");
			}

			if (a[i]->type == STRING_NODE)
				id = d_intern (a[i]->str.str);
			else
				id = a[i]->id.id;
			if (id == colorid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No color specified"),
						      "LinePlotDrawLine");
					g_free (x);
					g_free (y);
					return NULL;
				}
				/* FIXME: helper routine for getting color */
				if (a[i+1]->type == STRING_NODE) {
					gdk_color_parse (a[i+1]->str.str, &color);
				} else if (a[i+1]->type == IDENTIFIER_NODE) {
					gdk_color_parse (a[i+1]->id.id->token, &color);
				} else {
					gel_errorout (_("%s: Color must be a string"),
						      "LinePlotDrawLine");
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == thicknessid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No thickness specified"),
						      "LinePlotDrawLine");
					g_free (x);
					g_free (y);
					return NULL;
				}
				if G_UNLIKELY ( ! check_argument_positive_integer (a, i+1,
										   "LinePlotDrawLine")) {
					g_free (x);
					g_free (y);
					return NULL;
				}
				thickness = gel_get_nonnegative_integer (a[i+1]->val.value,
									 "LinePlotDrawLine");
				i++;
			} else {
				gel_errorout (_("%s: Unknown style"), "LinePlotDrawLine");
				g_free (x);
				g_free (y);
				return NULL;
			}
			
		} else {
			gel_errorout (_("%s: Bad parameter"), "LinePlotDrawLine");
			g_free (x);
			g_free (y);
			return NULL;
		}
	}

	draw_line (x, y, len, thickness, &color);

	return gel_makenum_null ();
}

static GelETree *
set_LinePlotWindow (GelETree * a)
{
	double x1, x2, y1, y2;
	if ( ! get_limits_from_matrix (a, &x1, &x2, &y1, &y2))
		return NULL;

	reset_plotx1 = plotx1 = defx1 = x1;
	reset_plotx2 = plotx2 = defx2 = x2;
	reset_ploty1 = ploty1 = defy1 = y1;
	reset_ploty2 = ploty2 = defy2 = y2;

	return make_matrix_from_limits ();
}

static GelETree *
get_LinePlotWindow (void)
{
	return make_matrix_from_limits ();
}

static GelETree *
set_SurfacePlotWindow (GelETree * a)
{
	double x1, x2, y1, y2, z1, z2;
	if ( ! get_limits_from_matrix_surf (a, &x1, &x2, &y1, &y2, &z1, &z2))
		return NULL;

	surf_defx1 = x1;
	surf_defx2 = x2;
	surf_defy1 = y1;
	surf_defy2 = y2;
	surf_defz1 = z1;
	surf_defz2 = z2;

	return make_matrix_from_limits_surf ();
}

static GelETree *
get_SurfacePlotWindow (void)
{
	return make_matrix_from_limits_surf ();
}

static GelETree *
set_VectorfieldNormalized (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_VectorfieldNormalized"))
		return NULL;
	if (a->type == VALUE_NODE)
		vectorfield_normalize_arrow_length_parameter
			= ! mpw_zero_p (a->val.value);
	else /* a->type == BOOL_NODE */
		vectorfield_normalize_arrow_length_parameter = a->bool_.bool_;

	return gel_makenum_bool (vectorfield_normalize_arrow_length_parameter);
}
static GelETree *
get_VectorfieldNormalized (void)
{
	return gel_makenum_bool (vectorfield_normalize_arrow_length_parameter);
}

static GelETree *
set_LinePlotDrawLegends (GelETree * a)
{
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_LinePlotDrawLegend"))
		return NULL;
	if (a->type == VALUE_NODE)
		lineplot_draw_legends_parameter
			= ! mpw_zero_p (a->val.value);
	else /* a->type == BOOL_NODE */
		lineplot_draw_legends_parameter = a->bool_.bool_;

	return gel_makenum_bool (lineplot_draw_legends_parameter);
}
static GelETree *
get_LinePlotDrawLegends (void)
{
	return gel_makenum_bool (lineplot_draw_legends_parameter);
}

void
gel_add_graph_functions (void)
{
	GelEFunc *f;
	GelToken *id;

	new_category ("plotting", N_("Plotting"), TRUE /* internal */);

	VFUNC (LinePlot, 2, "func,args", "plotting", N_("Plot a function with a line.  First come the functions (up to 10) then optionally limits as x1,x2,y1,y2"));
	VFUNC (LinePlotParametric, 3, "xfunc,yfunc,args", "plotting", N_("Plot a parametric function with a line.  First come the functions for x and y then optionally the t limits as t1,t2,tinc, then optionally the limits as x1,x2,y1,y2"));
	VFUNC (LinePlotCParametric, 2, "zfunc,args", "plotting", N_("Plot a parametric complex valued function with a line.  First comes the function that returns x+iy then optionally the t limits as t1,t2,tinc, then optionally the limits as x1,x2,y1,y2"));

	VFUNC (SlopefieldPlot, 2, "func,args", "plotting", N_("Draw a slope field.  First comes the function dy/dx in terms of x and y (or a complex z) then optionally the limits as x1,x2,y1,y2"));
	VFUNC (VectorfieldPlot, 3, "xfunc,yfunc,args", "plotting", N_("Draw a vector field.  First come the functions dx/dt and dy/dt in terms of x and y then optionally the limits as x1,x2,y1,y2"));

	FUNC (SlopefieldDrawSolution, 3, "x,y,dx", "plotting", N_("Draw a solution for a slope field starting at x,y and using dx as increment"));
	FUNC (SlopefieldClearSolutions, 0, "", "plotting", N_("Clear all the slopefield solutions"));

	FUNC (VectorfieldDrawSolution, 4, "x,y,dt,tlen", "plotting", N_("Draw a solution for a vector field starting at x,y, using dt as increment for tlen units"));
	FUNC (VectorfieldClearSolutions, 0, "", "plotting", N_("Clear all the vectorfield solutions"));


	VFUNC (SurfacePlot, 2, "func,args", "plotting", N_("Plot a surface function which takes either two arguments or a complex number.  First comes the function then optionally limits as x1,x2,y1,y2,z1,z2"));

	FUNC (LinePlotClear, 0, "", "plotting", N_("Show the line plot window and clear out functions"));
	VFUNC (LinePlotDrawLine, 2, "x1,y1,x2,y2,args", "plotting", N_("Draw a line from x1,y1 to x2,y2.  x1,y1,x2,y2 can be replaced by a n by 2 matrix for a longer line"));

	PARAMETER (VectorfieldNormalized, N_("Normalize vectorfields if true.  That is, only show direction and not magnitude."));
	PARAMETER (LinePlotDrawLegends, N_("If to draw legends or not on line plots."));

	PARAMETER (LinePlotWindow, N_("Line plotting window (limits) as a 4-vector of the form [x1,x2,y1,y2]"));
	PARAMETER (SurfacePlotWindow, N_("Surface plotting window (limits) as a 6-vector of the form [x1,x2,y1,y2,z1,z2]"));
}
