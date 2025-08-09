/* GENIUS Calculator
 * Copyright (C) 2003-2023 Jiri (George) Lebl
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
#include <glib.h>
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
static GtkWidget *errors_label_box = NULL;

static GtkWidget *solver_dialog = NULL;
gboolean solver_dialog_slopefield = TRUE;

static GtkWidget *plot_zoomout_item = NULL;
static GtkWidget *plot_zoomin_item = NULL;
static GtkWidget *plot_zoomfit_item = NULL;
static GtkWidget *plot_resetzoom_item = NULL;
static GtkWidget *plot_print_item = NULL;
static GtkWidget *plot_exportps_item = NULL;
static GtkWidget *plot_exporteps_item = NULL;
static GtkWidget *plot_exportpdf_item = NULL;
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
static GtkWidget *plot_y_labels[MAXFUNC] = { NULL };
static GtkWidget *plot_entries_status[MAXFUNC] = { NULL };

static GtkWidget *lineplot_info_label = NULL;

static GtkWidget *parametric_entry_x = NULL;
static GtkWidget *parametric_entry_y = NULL;
static GtkWidget *parametric_entry_z = NULL;
static GtkWidget *parametric_status_x = NULL;
static GtkWidget *parametric_status_y = NULL;
static GtkWidget *parametric_status_z = NULL;

static GtkWidget *parametric_info_label = NULL;
static GtkWidget *parametric_x_label = NULL;
static GtkWidget *parametric_y_label = NULL;
static GtkWidget *parametric_z_label = NULL;
static GtkWidget *parametric_trange_label = NULL;

static GtkWidget *slopefield_entry = NULL;
static GtkWidget *slopefield_status = NULL;
static GtkWidget *slopefield_info_label = NULL;
static GtkWidget *slopefield_der_label = NULL;

static GtkWidget *solver_x_entry = NULL;
static GtkWidget *solver_y_entry = NULL;

static GtkWidget *solver_xinc_entry = NULL;
static GtkWidget *solver_tinc_entry = NULL;
static GtkWidget *solver_tlen_entry = NULL;

static double solver_x = 0.0;
static double solver_y = 0.0;
static double solver_xinc = 0.1;
static double solver_tinc = 0.1;
static double solver_tlen = 5.0;

static GtkWidget *solver_xinc_label = NULL;
static GtkWidget *solver_tinc_label = NULL;
static GtkWidget *solver_tlen_label = NULL;
static GtkWidget *solver_x_pt_label = NULL;
static GtkWidget *solver_y_pt_label = NULL;

static GtkWidget *vectorfield_entry_x = NULL;
static GtkWidget *vectorfield_status_x = NULL;
static GtkWidget *vectorfield_entry_y = NULL;
static GtkWidget *vectorfield_status_y = NULL;

static GtkWidget *vectorfield_info_label = NULL;
static GtkWidget *vectorfield_xder_label = NULL;
static GtkWidget *vectorfield_yder_label = NULL;

static GtkWidget *lineplot_x_range_label = NULL;
static GtkWidget *lineplot_y_range_label = NULL;

static GtkWidget *surface_info_label = NULL;
static GtkWidget *surface_x_range_label = NULL;
static GtkWidget *surface_y_range_label = NULL;

static char *lp_x_name = NULL;
static char *lp_y_name = NULL;
static char *lp_z_name = NULL;
static char *lp_t_name = NULL;

static char *sp_x_name = NULL;
static char *sp_y_name = NULL;
static char *sp_z_name = NULL;

static GSList *solutions_list = NULL;

static GtkWidget *spinx1_entry = NULL;
static const char *spinx1_default = "-10";
static GtkWidget *spinx2_entry = NULL;
static const char *spinx2_default = "10";
static GtkWidget *spiny1_entry = NULL;
static const char *spiny1_default = "-10";
static GtkWidget *spiny2_entry = NULL;
static const char *spiny2_default = "10";

static GtkWidget *spint1_entry = NULL;
static const char *spint1_default = "0.0";
static GtkWidget *spint2_entry = NULL;
static const char *spint2_default = "1.0";
static GtkWidget *spintinc_entry = NULL;
static const char *spintinc_default = "0.01";

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
static gboolean lineplot_draw_labels = TRUE;
static gboolean lineplot_draw_labels_cb = TRUE;
static gboolean lineplot_fit_dependent_axis_cb = TRUE;
static gboolean vectorfield_normalize_arrow_length = FALSE;
static gboolean vectorfield_normalize_arrow_length_cb = FALSE;
static gboolean vectorfield_normalize_arrow_length_parameter = FALSE;
static gboolean surfaceplot_draw_legends = TRUE;
static gboolean surfaceplot_draw_legends_cb = TRUE;
static gboolean surfaceplot_fit_dependent_axis_cb = TRUE;

static GtkWidget* surfaceplot_dep_axis_buttons = NULL;
static GtkWidget* lineplot_dep_axis_buttons = NULL;
static GtkWidget* lineplot_depx_axis_buttons = NULL;
static GtkWidget* lineplot_fit_dep_axis_checkbox = NULL;

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

static int plot_sf_Vtick = 20;
static int plot_sf_Htick = 20;

static int plot_vf_Vtick = 20;
static int plot_vf_Htick = 20;

static int plotVtick = 20;
static int plotHtick = 20;

static double *plot_points_x = NULL;
static double *plot_points_y = NULL;
static double *plot_points_dx = NULL;
static double *plot_points_dy = NULL;

static int plot_points_num = 0;

/*
   Surface
 */
static GtkWidget *surface_plot = NULL;

static GtkPlotData *surface_data = NULL;

static GtkWidget *surface_entry = NULL;
static GtkWidget *surface_entry_status = NULL;
static GtkWidget *surf_spinx1_entry = NULL;
static const char *surf_spinx1_default = "-10";
static GtkWidget *surf_spinx2_entry = NULL;
static const char *surf_spinx2_default = "10";
static GtkWidget *surf_spiny1_entry = NULL;
static const char *surf_spiny1_default = "-10";
static GtkWidget *surf_spiny2_entry = NULL;
static const char *surf_spiny2_default = "10";
static GtkWidget *surf_spinz1_entry = NULL;
static const char *surf_spinz1_default = "-10";
static GtkWidget *surf_spinz2_entry = NULL;
static const char *surf_spinz2_default = "10";

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

/* surface data */
static double *surface_data_x = NULL;
static double *surface_data_y = NULL;
static double *surface_data_z = NULL;
static int surface_data_len = 0;


/* used for both */
static double plot_maxy = - G_MAXDOUBLE/2;
static double plot_miny = G_MAXDOUBLE/2;
static double plot_maxx = - G_MAXDOUBLE/2;
static double plot_minx = G_MAXDOUBLE/2;
static double plot_maxz = - G_MAXDOUBLE/2;
static double plot_minz = G_MAXDOUBLE/2;

static GelCtx *plot_ctx = NULL;
static GelETree *plot_arg = NULL;
static GelETree *plot_arg2 = NULL;
static GelETree *plot_arg3 = NULL;

static int plot_in_progress = 0;
static gboolean whack_window_after_plot = FALSE;

static void plot_axis (void);

/* lineplots */
static void plot_functions (gboolean do_window_present,
			    gboolean from_gui,
			    gboolean fit);
static void recompute_functions (gboolean fitting);

/* surfaces */
static void plot_surface_functions (gboolean do_window_present, gboolean fit_function);
static void recompute_surface_function (gboolean fitting);

static void plot_freeze (void);
static void plot_thaw (void);

/* replot the slope/vector fields after zoom or other axis changing event */
static void replot_fields (void);

static void slopefield_draw_solution (double x, double y, double dx, gboolean is_gui);
static void vectorfield_draw_solution (double x, double y, double dt, double tlen, gboolean is_gui);

static void set_lineplot_labels (void);
static void set_solver_labels (void);
static void set_surface_labels (void);

static gboolean get_number_from_entry (GtkWidget *entry, GtkWidget *win, double *num);
static GtkWidget *
create_range_boxes (const char *title, GtkWidget **titlew,
		    const char *def1, GtkWidget **w1,
		    const char *totitle, GtkWidget **totitlew,
		    const char *def2, GtkWidget **w2,
		    const char *bytitle,
		    const char *defby, GtkWidget **wb,
		    GCallback activate_callback);

#define WIDTH 700
/* FIXME: for long graphs #define WIDTH (2*700) */
#define HEIGHT 500
#define ASPECT ((double)HEIGHT/(double)WIDTH)

#define PROPORTION 0.85
#define PROPORTION3D 0.80
#define PROPORTION_OFFSETX 0.1
/* FIXME: for long graphs #define PROPORTION_OFFSETX 0.05 */
#define PROPORTION_OFFSETY 0.075
#define PROPORTION3D_OFFSET 0.12

#include "funclibhelper.cP"

enum {
	RESPONSE_STOP = 1,
	RESPONSE_PLOT,
	RESPONSE_CLEAR
};

enum {
	EXPORT_PS,
	EXPORT_EPS,
	EXPORT_PDF,
	EXPORT_PNG
};


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

static void
init_var_names (void)
{
	/* this means we have already initialized */
	if (lp_x_name != NULL)
		return;

	lp_x_name = g_strdup ("x");
	lp_y_name = g_strdup ("y");
	lp_z_name = g_strdup ("z");
	lp_t_name = g_strdup ("t");

	sp_x_name = g_strdup ("x");
	sp_y_name = g_strdup ("y");
	sp_z_name = g_strdup ("z");
}

/* FIXME: This seems like a rather ugly hack, am I missing something about
 * spinboxes or are they really this stupid */
static void
update_spinboxes (GtkWidget *w)
{
	if (GTK_IS_SPIN_BUTTON (w))
		gtk_spin_button_update (GTK_SPIN_BUTTON (w));
	else if (GTK_IS_CONTAINER (w)) {
		GList *children, *li;

		children = gtk_container_get_children (GTK_CONTAINER (w));

		for (li = children; li != NULL; li = li->next) {
			update_spinboxes (li->data);
		}

		g_list_free (children);
	}
}


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
						   RESPONSE_STOP, plot_in_progress || gel_calc_running);
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
		if (plot_exportpdf_item != NULL)
			gtk_widget_set_sensitive (plot_exportpdf_item, ! plot_in_progress);
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
	char *tmp;

	if (surface_plot == NULL)
		return;

	req = gtk_dialog_new_with_buttons
		(_("Rotate") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 GTK_DIALOG_MODAL /* flags */,
		 _("_Close"),
		 GTK_RESPONSE_CLOSE,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (req),
					 GTK_RESPONSE_CLOSE);

	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* X dir */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    hbox, TRUE, TRUE, 0);

	tmp = g_strdup_printf (_("Rotate about %s axis: "),
			       sp_x_name);
	w = gtk_label_new (tmp);
	g_free (tmp);

	gtk_size_group_add_widget (sg, w);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
	                   gtk_image_new_from_icon_name ("pan-start-symbolic",
	                                                 GTK_ICON_SIZE_BUTTON));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_x_cb),
			  GINT_TO_POINTER (360-10));
	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
			   gtk_image_new_from_icon_name ("pan-end-symbolic",
	                                                 GTK_ICON_SIZE_BUTTON));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_x_cb),
			  GINT_TO_POINTER (10));

	/* Y dir */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    hbox, TRUE, TRUE, 0);

	tmp = g_strdup_printf (_("Rotate about %s axis: "),
			       sp_y_name);
	w = gtk_label_new (tmp);
	g_free (tmp);

	gtk_size_group_add_widget (sg, w);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
	                   gtk_image_new_from_icon_name ("pan-start-symbolic",
	                                                 GTK_ICON_SIZE_BUTTON));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_y_cb),
			  GINT_TO_POINTER (360-10));
	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
	                   gtk_image_new_from_icon_name ("pan-end-symbolic",
	                                                 GTK_ICON_SIZE_BUTTON));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_y_cb),
			  GINT_TO_POINTER (10));

	/* Z dir */

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    hbox, TRUE, TRUE, 0);

	w = gtk_label_new (_("Rotate about dependent axis: "));
	gtk_size_group_add_widget (sg, w);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
	                   gtk_image_new_from_icon_name ("pan-start-symbolic",
	                                                 GTK_ICON_SIZE_BUTTON));
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (rotate_z_cb),
			  GINT_TO_POINTER (360-10));
	b = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (b),
	                   gtk_image_new_from_icon_name ("pan-end-symbolic",
	                                                 GTK_ICON_SIZE_BUTTON));
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

static int anim_timeout_id = 0;

static gboolean
anim_timeout (gpointer data)
{
	if (surface_plot == NULL || plot_canvas == NULL) {
		anim_timeout_id = 0;
		return FALSE;
	}

	gtk_plot3d_rotate_z (GTK_PLOT3D (surface_plot), 360-2);

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

	/*while (gtk_events_pending ())
		gtk_main_iteration ();*/

	return TRUE;
}

static void
start_rotate_anim_cb (GtkWidget *item, gpointer data)
{
	if (anim_timeout_id == 0) {
		anim_timeout_id = g_timeout_add_full (G_PRIORITY_LOW,
						      100, anim_timeout, NULL, NULL);
	}
}

static void
stop_rotate_anim_cb (GtkWidget *item, gpointer data)
{
	if (anim_timeout_id != 0) {
		g_source_remove (anim_timeout_id);
		anim_timeout_id = 0;
	}
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
		gel_interrupted = TRUE;
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
			gel_interrupted = TRUE;
			whack_window_after_plot = TRUE;
		} else {
			gtk_widget_destroy (graph_window);
		}
	} else if (response == RESPONSE_STOP && (plot_in_progress > 0 || gel_calc_running > 0)) {
		gel_interrupted = TRUE;
	}
}

static void
ok_dialog_entry_activate (GtkWidget *entry, gpointer data)
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
	char *tmpfile;
	static char *last_cmd = NULL;

	if (last_cmd == NULL)
		last_cmd = g_strdup ("lpr");

	req = gtk_dialog_new_with_buttons
		(_("Print") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 GTK_DIALOG_MODAL /* flags */,
		 _("_Cancel"),
		 GTK_RESPONSE_CANCEL,
		 _("_Print"),
		 GTK_RESPONSE_OK,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (req),
					 GTK_RESPONSE_OK);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    hbox, TRUE, TRUE, 0);

	w = gtk_label_new (_("Print command: "));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	cmd = gtk_entry_new ();
	g_signal_connect (G_OBJECT (cmd), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
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

	g_free (last_cmd);
	last_cmd = g_strdup (gtk_entry_get_text (GTK_ENTRY (cmd)));

	gtk_widget_destroy (req);

	tmpfile = g_build_filename (g_get_tmp_dir (), "genius-ps-XXXXXX", NULL);
	fd = g_mkstemp (tmpfile);
	if (fd < 0) {
		g_free (tmpfile);
		genius_display_error (graph_window, _("Cannot open temporary file, cannot print."));
		return;
	}

	plot_in_progress ++;
	gel_calc_running ++;
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

	if ( ! ret || gel_interrupted) {
		plot_in_progress --;
		gel_calc_running --;
		plot_window_setup ();

		if ( ! gel_interrupted)
			genius_display_error (graph_window, _("Printing failed"));
		gel_interrupted = FALSE;
		close (fd);
		unlink (tmpfile);
		g_free (tmpfile);
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
	gel_calc_running --;
	plot_window_setup ();

	close (fd);
	unlink (tmpfile);
	g_free (tmpfile);
}

static char *last_export_dir = NULL;

static void
really_export_cb (GtkFileChooser *fs, int response, gpointer data)
{
	char *s;
	char *base;
	gboolean ret;
	int export_type;
	char *tmpfile = NULL;
	char *file_to_write = NULL;
	int fd = -1;
	gboolean run_epsi = FALSE;
	GtkWidget *w;

	export_type = GPOINTER_TO_INT (data);

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (fs));
		/* FIXME: don't want to deal with modality issues right now */
		gtk_widget_set_sensitive (graph_window, TRUE);
		return;
	}

	/* run epsi checkbox */
	if (export_type == EXPORT_EPS) {
		w = gtk_file_chooser_get_extra_widget (GTK_FILE_CHOOSER (fs));
		if (w != NULL &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) {
			run_epsi = TRUE;
		}
	}

	s = g_strdup (gtk_file_chooser_get_filename (fs));

	if (s == NULL)
		return;
	base = g_path_get_basename (s);
	if (base != NULL && base[0] != '\0' &&
	    strchr (base, '.') == NULL) {
		char *n;
		if (export_type == EXPORT_EPS)
			n = g_strconcat (s, ".eps", NULL);
		else if (export_type == EXPORT_PDF)
			n = g_strconcat (s, ".pdf", NULL);
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
	if (export_type == EXPORT_EPS && run_epsi && ve_is_prog_in_path ("ps2epsi")) {
		tmpfile = g_build_filename (g_get_tmp_dir (), "genius-ps-XXXXXX", NULL);
		fd = g_mkstemp (tmpfile);
		/* FIXME: tell about errors ?*/
		if (fd >= 0) {
			file_to_write = tmpfile;
		}
	} else if (export_type == EXPORT_PDF) {
		tmpfile = g_build_filename (g_get_tmp_dir (), "genius-ps-XXXXXX", NULL);
		fd = g_mkstemp (tmpfile);
		/* FIXME: tell about errors ?*/
		if (fd >= 0) {
			file_to_write = tmpfile;
		}
	}

	plot_in_progress ++;
	gel_calc_running ++;
	plot_window_setup ();

	/* FIXME: There should be some options about size and stuff */
	if (plot_canvas != NULL)
		ret = gtk_plot_canvas_export_ps_with_size
			(GTK_PLOT_CANVAS (plot_canvas),
			 file_to_write,
			 GTK_PLOT_PORTRAIT,
			 (export_type == EXPORT_EPS ||
			  export_type == EXPORT_PDF) /* epsflag */,
			 GTK_PLOT_PSPOINTS,
			 400, ASPECT * 400);
	else
		ret = FALSE;

	/* need this for some reason */
	if (plot_canvas != NULL) {
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

	/* If we used a temporary file, now use ps2epsi or ps2pdf */
	if (fd >= 0 && run_epsi) {
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
	} else if (fd >= 0 && export_type == EXPORT_PDF) {
		int status;
		char *qs = g_shell_quote (s);
		char *cmd = g_strdup_printf ("ps2pdf -dEPSCrop -dPDFSETTINGS=/prepress %s %s", tmpfile, qs);
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
			/* EEK, couldn't run ps2pdf for some reason */
			rename (tmpfile, s);
		}
		g_free (cmd);
		g_free (qs);
	}

	plot_in_progress --;
	gel_calc_running --;
	plot_window_setup ();

	if ( ! ret || gel_interrupted) {
		if ( ! gel_interrupted)
			genius_display_error (graph_window, _("Export failed"));
		g_free (s);
		if (tmpfile != NULL)
			g_free (tmpfile);
		gel_interrupted = FALSE;
		return;
	}

	g_free (s);
	if (tmpfile != NULL)
		g_free (tmpfile);
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

	pix = gdk_pixbuf_get_from_surface
		(GTK_PLOT_CANVAS (plot_canvas)->pixmap,
		 0 /* src x */, 0 /* src y */,
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
	else if (export_type == EXPORT_PDF)
		title = _("Export PDF");
	else if (export_type == EXPORT_PNG)
		title = _("Export PNG");
	else
		/* should never happen */
		title = "Export ???";

	if (export_type == EXPORT_PDF &&
	    ! ve_is_prog_in_path ("ps2pdf")) {
		genius_display_error (graph_window, _("Missing ps2pdf command, perhaps ghostscript is not installed."));
		return;
	}

	fs = gtk_file_chooser_dialog_new (title,
					  GTK_WINDOW (graph_window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  _("_Cancel"), GTK_RESPONSE_CANCEL,
					  _("_Save"), GTK_RESPONSE_OK,
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
	} else if (export_type == EXPORT_PDF) {
		gtk_file_filter_set_name (filter_ps, _("PDF files"));
		gtk_file_filter_add_pattern (filter_ps, "*.pdf");
		gtk_file_filter_add_pattern (filter_ps, "*.PDF");
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

	if (export_type == EXPORT_EPS && ve_is_prog_in_path ("ps2epsi")) {
		GtkWidget *w;
		w = gtk_check_button_new_with_label (_("Generate preview in EPS file (with ps2epsi)"));
		gtk_widget_show (w);
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (fs), w);
	}


	g_signal_connect (G_OBJECT (fs), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &fs);
	if (export_type == EXPORT_EPS ||
	    export_type == EXPORT_PS ||
	    export_type == EXPORT_PDF) {
		g_signal_connect (G_OBJECT (fs), "response",
				  G_CALLBACK (really_export_cb),
				  GINT_TO_POINTER (export_type));
	} else if (export_type == EXPORT_PNG) {
		g_signal_connect (G_OBJECT (fs), "response",
				  G_CALLBACK (really_export_png_cb),
				  NULL);
	}

	if (last_export_dir != NULL) {
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), last_export_dir);
	} else {
		char *s = g_get_current_dir ();
		gtk_file_chooser_set_current_folder
			(GTK_FILE_CHOOSER (fs), s);
		g_free (s);
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
plot_exportpdf_cb (void)
{
	do_export_cb (EXPORT_PDF);
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
		long last_errnum = total_errors;

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

		if (gel_interrupted)
			gel_interrupted = FALSE;

		gel_printout_infos_parent (graph_window);
		if (last_errnum != total_errors &&
		    ! genius_setup.error_box) {
			gtk_widget_show (errors_label_box);
		}
	}
}

static void
plot_zoomout_cb (void)
{
	if (plot_in_progress == 0) {
		double len;
		long last_errnum = total_errors;

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

		if (gel_interrupted)
			gel_interrupted = FALSE;

		gel_printout_infos_parent (graph_window);
		if (last_errnum != total_errors &&
		    ! genius_setup.error_box) {
			gtk_widget_show (errors_label_box);
		}
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
		long last_errnum = total_errors;

		if (plot_mode == MODE_LINEPLOT) {
			size = plot_maxy - plot_miny;
			if (size <= 0.0) {
				/* Just don't do anything */
				return;
			}

			ploty1 = plot_miny - size * 0.05;
			ploty2 = plot_maxy + size * 0.05;

			/* sanity */
			if (ploty2 <= ploty1)
				ploty2 = ploty1 + 0.1;

			/* sanity */
			if (ploty1 < -(G_MAXDOUBLE/2))
				ploty1 = -(G_MAXDOUBLE/2);
			if (ploty2 > (G_MAXDOUBLE/2))
				ploty2 = (G_MAXDOUBLE/2);

		} else if (plot_mode == MODE_LINEPLOT_PARAMETRIC) {
			double sizex;
			size = plot_maxy - plot_miny;
			if (size <= 0.0) {
				/* Just don't do anything */
				return;
			}
			sizex = plot_maxx - plot_minx;
			if (sizex <= 0.0) {
				/* Just don't do anything */
				return;
			}

			plotx1 = plot_minx - sizex * 0.05;
			plotx2 = plot_maxx + sizex * 0.05;

			/* sanity */
			if (plotx2 <= plotx1)
				plotx2 = plotx1 + 0.1;

			/* sanity */
			if (plotx1 < -(G_MAXDOUBLE/2))
				plotx1 = -(G_MAXDOUBLE/2);
			if (plotx2 > (G_MAXDOUBLE/2))
				plotx2 = (G_MAXDOUBLE/2);

			ploty1 = plot_miny - size * 0.05;
			ploty2 = plot_maxy + size * 0.05;

			/* sanity */
			if (ploty2 <= ploty1)
				ploty2 = ploty1 + 0.1;

			/* sanity */
			if (ploty1 < -(G_MAXDOUBLE/2))
				ploty1 = -(G_MAXDOUBLE/2);
			if (ploty2 > (G_MAXDOUBLE/2))
				ploty2 = (G_MAXDOUBLE/2);

		} else if (plot_mode == MODE_SURFACE) {
			size = plot_maxz - plot_minz;
			if (size <= 0.0) {
				/* Just don't do anything */
				return;
			}
			surfacez1 = plot_minz - size * 0.05;
			surfacez2 = plot_maxz + size * 0.05;
			
			/* sanity */
			if (surfacez2 <= surfacez1)
				surfacez2 = surfacez1 + 0.1;

			/* sanity */
			if (surfacez1 < -(G_MAXDOUBLE/2))
				surfacez1 = -(G_MAXDOUBLE/2);
			if (surfacez2 > (G_MAXDOUBLE/2))
				surfacez2 = (G_MAXDOUBLE/2);
		}

		plot_axis ();

		if (gel_interrupted)
			gel_interrupted = FALSE;

		gel_printout_infos_parent (graph_window);
		if (last_errnum != total_errors &&
		    ! genius_setup.error_box) {
			gtk_widget_show (errors_label_box);
		}
	}
}

static void
plot_resetzoom_cb (void)
{
	if (plot_in_progress == 0) {
		long last_errnum = total_errors;

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

		if (gel_interrupted)
			gel_interrupted = FALSE;

		gel_printout_infos_parent (graph_window);
		if (last_errnum != total_errors &&
		    ! genius_setup.error_box) {
			gtk_widget_show (errors_label_box);
		}
	}
}

static void
lineplot_move_graph (double horiz, double vert)
{
	if (plot_in_progress == 0 && 
	    (plot_mode == MODE_LINEPLOT ||
	     plot_mode == MODE_LINEPLOT_PARAMETRIC ||
	     plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
	     plot_mode == MODE_LINEPLOT_VECTORFIELD)) {
		double len;
		long last_errnum = total_errors;

		len = plotx2 - plotx1;
		plotx1 = plotx1 + horiz * len;
		plotx2 = plotx2 + horiz* len;

		len = ploty2 - ploty1;
		ploty1 = ploty1 + vert * len;
		ploty2 = ploty2 + vert* len;

		plot_axis ();

		if (gel_interrupted)
			gel_interrupted = FALSE;

		gel_printout_infos_parent (graph_window);
		if (last_errnum != total_errors &&
		    ! genius_setup.error_box) {
			gtk_widget_show (errors_label_box);
		}
	}
}

static guint dozoom_idle_id = 0;
static gdouble dozoom_xmin;
static gdouble dozoom_ymin;
static gdouble dozoom_xmax;
static gdouble dozoom_ymax;
static gboolean dozoom_just_click;

static gboolean wait_for_click = FALSE;
static gdouble click_x = 0.0;
static gdouble click_y = 0.0;

static gboolean
dozoom_idle (gpointer data)
{
	dozoom_idle_id = 0;

	if (plot_in_progress == 0 && line_plot != NULL) {
		double len;
		long last_errnum = total_errors;

		/* just click, so zoom in */
		if (dozoom_just_click) {
			len = plotx2 - plotx1;
			plotx1 += len * dozoom_xmin - len / 4.0;
			plotx2 = plotx1 + len / 2.0;

			len = ploty2 - ploty1;
			ploty1 += len * dozoom_ymin - len / 4.0;
			ploty2 = ploty1 + len / 2.0;
		} else {
			len = plotx2 - plotx1;
			plotx1 += len * dozoom_xmin;
			plotx2 = plotx1 + (len * (dozoom_xmax-dozoom_xmin));

			len = ploty2 - ploty1;
			ploty1 += len * dozoom_ymin;
			ploty2 = ploty1 + (len * (dozoom_ymax-dozoom_ymin));
		}

		/* sanity */
		if (plotx2 - plotx1 < MINPLOT)
			plotx2 = plotx1 + MINPLOT;
		/* sanity */
		if (ploty2 - ploty1 < MINPLOT)
			ploty2 = ploty1 + MINPLOT;

		plot_axis ();

		if (gel_interrupted)
			gel_interrupted = FALSE;

		gel_printout_infos_parent (graph_window);
		if (last_errnum != total_errors &&
		    ! genius_setup.error_box) {
			gtk_widget_show (errors_label_box);
		}
	}

	return FALSE;
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

	dozoom_just_click = FALSE;
	if (fabs(xmin-xmax) < 0.001 ||
	    fabs(ymin-ymax) < 0.001) {
		dozoom_just_click = TRUE;
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

	if (xmin > xmax) {
		double tmp = xmax;
		xmax = xmin;
		xmin = tmp;
	}

	if (ymin > ymax) {
		double tmp = ymax;
		ymax = ymin;
		ymin = tmp;
	}

	if (wait_for_click) {
		double x, y;
		len = plotx2 - plotx1;
		x = plotx1 + len * xmin;
		len = ploty2 - ploty1;
		y = ploty1 + len * ymin;
		click_x = x;
		click_y = y;
		wait_for_click = FALSE;
		return;
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

		if (solver_x_entry != NULL) {
			char *s = g_strdup_printf ("%g", x);
			gtk_entry_set_text (GTK_ENTRY (solver_x_entry), s);
			g_free (s);
		}
		if (solver_y_entry != NULL) {
			char *s = g_strdup_printf ("%g", y);
			gtk_entry_set_text (GTK_ENTRY (solver_y_entry), s);
			g_free (s);
		}

		if (plot_mode == MODE_LINEPLOT_SLOPEFIELD)
			slopefield_draw_solution (x, y, solver_xinc, TRUE /*is_gui*/);
		else if (plot_mode == MODE_LINEPLOT_VECTORFIELD)
			vectorfield_draw_solution (x, y, solver_tinc,
						   solver_tlen, TRUE /*is_gui*/);

		return;
	}

	/* only for line plots! */
	if (plot_in_progress == 0 && line_plot != NULL) {
		dozoom_xmin = xmin;
		dozoom_xmax = xmax;
		dozoom_ymin = ymin;
		dozoom_ymax = ymax;
		if (dozoom_idle_id == 0) {
			dozoom_idle_id = g_idle_add (dozoom_idle, NULL);
		}
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
			       PROPORTION_OFFSETX,
			       PROPORTION_OFFSETY);
		gtk_plot_resize (GTK_PLOT (line_plot),
				 1.0-2*PROPORTION_OFFSETX,
				 1.0-2*PROPORTION_OFFSETY-0.05);

		gtk_plot_legends_move (GTK_PLOT (line_plot),
				       0.0,
				       1.07);
	} else {
		gtk_plot_move (GTK_PLOT (line_plot),
			       PROPORTION_OFFSETX,
			       PROPORTION_OFFSETY);
		gtk_plot_resize (GTK_PLOT (line_plot),
				 1.0-2*PROPORTION_OFFSETX,
				 1.0-2*PROPORTION_OFFSETY);
		gtk_plot_legends_move (GTK_PLOT (line_plot), 0.80, 0.05);
	}

	if (lineplot_draw_legends)
		gtk_plot_show_legends (GTK_PLOT (line_plot));
	else
		gtk_plot_hide_legends (GTK_PLOT (line_plot));
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
				   PROPORTION_OFFSETX,
				   PROPORTION_OFFSETY,
				   1.0-PROPORTION_OFFSETX,
				   1.0-PROPORTION_OFFSETY);

	GTK_PLOT_CANVAS_SET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				   GTK_PLOT_CANVAS_CAN_SELECT);

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

	gtk_plot_clip_data (GTK_PLOT (line_plot), TRUE);

	line_plot_move_about ();
}

static void
surface_plot_move_about (void)
{
	if (surface_plot == NULL)
		return;

	if (surfaceplot_draw_legends) {
		gtk_plot_move (GTK_PLOT (surface_plot),
			       0.0,
			       PROPORTION3D_OFFSET);
	} else {
		gtk_plot_move (GTK_PLOT (surface_plot),
			       PROPORTION3D_OFFSET,
			       PROPORTION3D_OFFSET);
	}
}


static void
add_surface_plot (void)
{
	GtkPlotAxis *xy, *xz, *yx, *yz, *zx, *zy;
	GtkPlotAxis *top, *left, *bottom;

	surface_plot = gtk_plot3d_new (NULL);
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

	GTK_PLOT_CANVAS_UNSET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				     GTK_PLOT_CANVAS_CAN_SELECT);

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

	gtk_plot_axis_set_title (bottom, sp_x_name);
	gtk_plot_axis_set_title (left, sp_y_name);
	gtk_plot_axis_set_title (top, "");

	gtk_plot_set_legends_border (GTK_PLOT (surface_plot),
				     GTK_PLOT_BORDER_LINE, 3);
	gtk_plot_legends_move (GTK_PLOT (surface_plot), 0.93, 0.05);

	surface_plot_move_about ();
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
		if (plot_mode == MODE_LINEPLOT_SLOPEFIELD) {
			if (get_number_from_entry (solver_x_entry, w, &solver_x) &&
			    get_number_from_entry (solver_y_entry, w, &solver_y) &&
			    get_number_from_entry (solver_xinc_entry, w, &solver_xinc)) {
				slopefield_draw_solution (solver_x, solver_y, solver_xinc, TRUE /*is_gui*/);
			}
		} else {
			if (get_number_from_entry (solver_x_entry, w, &solver_x) &&
			    get_number_from_entry (solver_y_entry, w, &solver_y) &&
			    get_number_from_entry (solver_tinc_entry, w, &solver_tinc) &&
			    get_number_from_entry (solver_tlen_entry, w, &solver_tlen)) {
				vectorfield_draw_solution (solver_x, solver_y, solver_tinc, solver_tlen, TRUE /*is_gui*/);
			}
		}
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
	char *def1, *def2;

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
		 _("_Close"),
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

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (solver_dialog))),
			    box, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);

	w = gtk_label_new (_("Clicking on the graph window now will draw a "
			     "solution according to the parameters set "
			     "below, starting at the point clicked.  "
			     "To be able to zoom by mouse again, close this "
			     "window."));
	gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (w), 30);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	if (plot_mode == MODE_LINEPLOT_SLOPEFIELD) {
		solver_dialog_slopefield = TRUE;

		solver_xinc = (plotx2-plotx1) / 100;

		def1 = g_strdup_printf ("%g", solver_xinc);

		w = create_range_boxes (_("X increment:"), &solver_xinc_label,
					def1, &solver_xinc_entry,
					NULL, NULL,
					NULL, NULL,
					NULL, NULL, NULL,
					solver_entry_activate);
		gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

		g_free (def1);
	} else {
		solver_dialog_slopefield = FALSE;

		solver_tinc = solver_tlen / 100;

		def1 = g_strdup_printf ("%g", solver_tinc);
		def2 = g_strdup_printf ("%g", solver_tlen);

		w = create_range_boxes (_("T increment:"), &solver_tinc_label,
					def1, &solver_tinc_entry,
					_("T interval length:"), &solver_tlen_label,
					def2, &solver_tlen_entry,
					NULL, NULL, NULL,
					solver_entry_activate);
		gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

		g_free (def1);
		g_free (def2);
	}

	if (solver_x < plotx1 || solver_x > plotx2)
		solver_x = plotx1 + (plotx2-plotx1)/2;
	if (solver_y < ploty1 || solver_y > ploty2)
		solver_y = ploty1 + (ploty2-ploty1)/2;

	def1 = g_strdup_printf ("%g", solver_x);
	def2 = g_strdup_printf ("%g", solver_y);

	w = create_range_boxes (_("Point x:"), &solver_x_pt_label,
				def1, &solver_x_entry,
				_("y:"), &solver_y_pt_label,
				def2, &solver_y_entry,
				NULL, NULL, NULL,
				solver_entry_activate);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	g_free (def1);
	g_free (def2);

	set_solver_labels ();

	gtk_widget_show_all (solver_dialog);
}

static void
clear_solutions_cb (GtkWidget *item, gpointer data)
{
	clear_solutions ();
}

static gboolean
dorotate_idle (gpointer data)
{
	int angle = GPOINTER_TO_INT (data);

	if (plot_mode == MODE_SURFACE &&
	    surface_plot != NULL &&
	    plot_in_progress == 0) {
		plot_in_progress++;
		gtk_plot3d_rotate_z (GTK_PLOT3D (surface_plot), angle);

		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

		while (gtk_events_pending ())
			gtk_main_iteration ();
		plot_in_progress--;
	}

	return FALSE;
}

static gboolean
plot_canvas_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	switch (event->keyval) {
	case GDK_KEY_Up:
		lineplot_move_graph (0.0, 0.1);
		break;
	case GDK_KEY_Down:
		lineplot_move_graph (0.0, -0.1);
		break;
	case GDK_KEY_Left:
		lineplot_move_graph (-0.1, 0.0);

		g_idle_add (dorotate_idle, GINT_TO_POINTER(360-10));
		break;
	case GDK_KEY_Right:
		lineplot_move_graph (0.1, 0.0);

		g_idle_add (dorotate_idle, GINT_TO_POINTER(10));
		break;
	default: break;
	}

	return FALSE; 
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
					 GDK_KEY_minus,
					 GDK_CONTROL_MASK);
		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Zoom in",
					 GDK_KEY_plus,
					 GDK_CONTROL_MASK);
		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Fit dependent axis",
					 GDK_KEY_f,
					 GDK_CONTROL_MASK);
		gtk_accel_map_add_entry ("<Genius-Plot>/Zoom/Reset to original zoom",
					 GDK_KEY_r,
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

		gtk_widget_hide (errors_label_box);
		return;
	}

	graph_window = gtk_dialog_new_with_buttons
		(_("Plot") /* title */,
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Stop"),
		 RESPONSE_STOP,
		 _("_Close"),
		 GTK_RESPONSE_CLOSE,
		 NULL);
	gtk_window_set_type_hint (GTK_WINDOW (graph_window),
				  GDK_WINDOW_TYPE_HINT_NORMAL);

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
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (graph_window))),
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

	if (ve_is_prog_in_path ("ps2pdf")) {
		item = gtk_menu_item_new_with_mnemonic (_("Export P_DF..."));
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (plot_exportpdf_cb), NULL);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		plot_exportpdf_item = item;
	} else {
		plot_exportpdf_item = NULL;
	}

	item = gtk_menu_item_new_with_mnemonic (_("Export P_NG..."));
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

	item = gtk_menu_item_new_with_mnemonic (_("Start rotate _animation..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (start_rotate_anim_cb), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Stop rotate a_nimation..."));
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (stop_rotate_anim_cb), NULL);
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

	gtk_widget_show_all (menubar);

	plot_canvas = gtk_plot_canvas_new (WIDTH, HEIGHT, 1.0);
	g_signal_connect (G_OBJECT (plot_canvas),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &plot_canvas);
	GTK_PLOT_CANVAS_UNSET_FLAGS (GTK_PLOT_CANVAS (plot_canvas),
				     GTK_PLOT_CANVAS_DND_FLAGS);
	g_signal_connect (G_OBJECT (plot_canvas), "select_region",
			  G_CALLBACK (plot_select_region),
			  NULL);
	g_signal_connect (G_OBJECT (plot_canvas), "key_press_event",
			  G_CALLBACK (plot_canvas_key_press_event),
			  NULL);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (graph_window))),
			    GTK_WIDGET (plot_canvas), TRUE, TRUE, 0);
	gtk_widget_show (plot_canvas);

	errors_label_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start
		(GTK_BOX (errors_label_box),
		 GTK_WIDGET (gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_SMALL_TOOLBAR)),
		 FALSE, FALSE, 0);
	gtk_box_pack_start
		(GTK_BOX (errors_label_box),
		 GTK_WIDGET (gtk_label_new (_("Errors during plotting (possibly harmless), see the console."))),
		 FALSE, FALSE, 0);
	gtk_widget_show_all (errors_label_box);
	gtk_widget_hide (errors_label_box);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (graph_window))),
			    GTK_WIDGET (errors_label_box), FALSE, FALSE, GENIUS_PAD);



	gtk_widget_show (graph_window);
}


static void
clear_graph (void)
{
	int i;

	stop_rotate_anim_cb (NULL, NULL);

	gtk_widget_hide (errors_label_box);

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
get_ticks (double start, double end, double *tick, int *prec,
	   int *style, int *fontheight)
{
	int incs;
	double len = end-start;
	int tries = 0;
	int tickprec;
	int extra_prec;
	int maxprec;
	int diff_of_prec;

	/* actually maxprec is the minimum since we're taking negatives */
	if (start == 0.0) {
		maxprec = -floor (log10(fabs(end)));
	} else if (end == 0.0) {
		maxprec = -floor (log10(fabs(start)));
	} else {
		maxprec = MIN(-floor (log10(fabs(start))),-floor (log10(fabs(end))));
	}

	tickprec = -floor (log10(len));
	*tick = pow (10, -tickprec);
	incs = floor (len / *tick);

	*fontheight = 12;

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

	diff_of_prec = tickprec + extra_prec - maxprec;

	if (maxprec <= -6) {
		*prec = MAX(diff_of_prec,1);
		*style = GTK_PLOT_LABEL_EXP;
	} else if (maxprec >= 6) {
		*prec = MAX(diff_of_prec,1);
		*style = GTK_PLOT_LABEL_EXP;
	} else if (maxprec <= 0) {
		*style = GTK_PLOT_LABEL_FLOAT;
		*prec = MAX(tickprec + extra_prec,0);
	} else {
		if (diff_of_prec > 2) {
			*prec = MAX(diff_of_prec,1);
			*style = GTK_PLOT_LABEL_EXP;
		} else {
			*style = GTK_PLOT_LABEL_FLOAT;
			*prec = tickprec + extra_prec;
		}
	}

	if (*style == GTK_PLOT_LABEL_FLOAT) {
		if (diff_of_prec > 8) {
			*fontheight = 8;
		} else if (diff_of_prec > 6) {
			*fontheight = 10;
		}
	} else if (*style == GTK_PLOT_LABEL_EXP) {
		if (*prec > 4) {
			*fontheight = 8;
		} else if (*prec > 2) {
			*fontheight = 10;
		}
	}
}

static void
plot_setup_axis (void)
{
	int xprec, yprec, xstyle, ystyle;
	double xtick, ytick;
	GtkPlotAxis *axis;
	GdkRGBA gray;
	int xfontheight, yfontheight;

	get_ticks (plotx1, plotx2, &xtick, &xprec, &xstyle, &xfontheight);
	get_ticks (ploty1, ploty2, &ytick, &yprec, &ystyle, &yfontheight);

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

	gdk_rgba_parse (&gray, "gray75");

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


	axis = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_TOP);
	/* FIXME: this is a hack */
	axis->labels_attr.height = xfontheight;
	gtk_plot_axis_set_labels_style (axis,
					xstyle /* style */,
					xprec /* precision */);
	gtk_plot_axis_show_labels (axis, lineplot_draw_labels ?
				      GTK_PLOT_LABEL_OUT :
				      GTK_PLOT_LABEL_NONE);

	axis = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_BOTTOM);
	/* FIXME: this is a hack */
	axis->labels_attr.height = xfontheight;
	gtk_plot_axis_set_labels_style (axis,
					xstyle /* style */,
					xprec /* precision */);
	gtk_plot_axis_show_labels (axis, lineplot_draw_labels ?
				      GTK_PLOT_LABEL_OUT :
				      GTK_PLOT_LABEL_NONE);

	axis = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_LEFT);
	/* FIXME: this is a hack */
	axis->labels_attr.height = yfontheight;
	gtk_plot_axis_set_labels_style (axis,
					ystyle /* style */,
					yprec /* precision */);
	gtk_plot_axis_show_labels (axis, lineplot_draw_labels ?
				      GTK_PLOT_LABEL_OUT :
				      GTK_PLOT_LABEL_NONE);

	axis = gtk_plot_get_axis (GTK_PLOT (line_plot), GTK_PLOT_AXIS_RIGHT);
	/* FIXME: this is a hack */
	axis->labels_attr.height = yfontheight;
	gtk_plot_axis_set_labels_style (axis,
					ystyle /* style */,
					yprec /* precision */);
	gtk_plot_axis_show_labels (axis, lineplot_draw_labels ?
				      GTK_PLOT_LABEL_OUT :
				      GTK_PLOT_LABEL_NONE);


	/* FIXME: implement logarithmic scale
	gtk_plot_set_xscale (GTK_PLOT (line_plot), GTK_PLOT_SCALE_LOG10);
	gtk_plot_set_yscale (GTK_PLOT (line_plot), GTK_PLOT_SCALE_LOG10);*/

	gtk_plot_thaw (GTK_PLOT (line_plot));
}

static void
surface_setup_axis (void)
{
	int xprec, yprec, zprec;
	int xstyle, ystyle, zstyle;
	double xtick, ytick, ztick;
	GtkPlotAxis *x, *y, *z;
	int xfontheight, yfontheight, zfontheight;

	get_ticks (surfacex1, surfacex2, &xtick, &xprec, &xstyle, &xfontheight);
	get_ticks (surfacey1, surfacey2, &ytick, &yprec, &ystyle, &yfontheight);
	get_ticks (surfacez1, surfacez2, &ztick, &zprec, &zstyle, &zfontheight);

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

	/*FIXME: hack*/
	x->labels_attr.height = xfontheight;
	y->labels_attr.height = yfontheight;
	z->labels_attr.height = zfontheight;

	gtk_plot_axis_set_labels_style (x,
					xstyle,
					xprec /* precision */);
	gtk_plot_axis_set_labels_style (y,
					ystyle,
					yprec /* precision */);
	gtk_plot_axis_set_labels_style (z,
					zstyle,
					zprec /* precision */);

	gtk_plot_axis_thaw (x);
	gtk_plot_axis_thaw (y);
	gtk_plot_axis_thaw (z);
}

static void
surface_setup_gradient (void)
{
	double min, max, absminmax;

	if (surface_data == NULL)
		return;

	min = MAX(surfacez1, plot_minz);
	max = MIN(surfacez2, plot_maxz);

	gtk_plot_data_set_gradient (surface_data,
				    min,
				    max,
				    10 /* nlevels */,
				    0 /* nsublevels */);
	absminmax = MAX(fabs(min),fabs(max));
	if (absminmax < 0.0001 || absminmax > 1000000) {
		gtk_plot_data_gradient_set_style (surface_data,
						  GTK_PLOT_LABEL_EXP,
						  3);
	} else {
		int powten = floor (log10(absminmax));
		int prec = 0;
		if (-powten+2 > 0) {
			prec = -powten+2;
		} else {
			prec = 0;
		}
		gtk_plot_data_gradient_set_style (surface_data,
						  GTK_PLOT_LABEL_FLOAT,
						  prec);
	}

}

static void
plot_axis (void)
{
	plot_in_progress ++;
	gel_calc_running ++;
	plot_window_setup ();

	if (plot_mode == MODE_LINEPLOT) {
		plot_maxy = - G_MAXDOUBLE/2;
		plot_miny = G_MAXDOUBLE/2;
		plot_maxx = - G_MAXDOUBLE/2;
		plot_minx = G_MAXDOUBLE/2;
		recompute_functions (FALSE /*fitting*/);
		plot_setup_axis ();
	} else if (plot_mode == MODE_LINEPLOT_PARAMETRIC ||
		   plot_mode == MODE_LINEPLOT_SLOPEFIELD ||
		   plot_mode == MODE_LINEPLOT_VECTORFIELD) {
		plot_setup_axis ();
	} else if (plot_mode == MODE_SURFACE) {
		plot_maxx = - G_MAXDOUBLE/2;
		plot_minx = G_MAXDOUBLE/2;
		plot_maxy = - G_MAXDOUBLE/2;
		plot_miny = G_MAXDOUBLE/2;
		plot_maxz = - G_MAXDOUBLE/2;
		plot_minz = G_MAXDOUBLE/2;
		recompute_surface_function (FALSE /* fit */);
		surface_setup_axis ();
		if (surface_data != NULL)
			gtk_plot_surface_build_mesh (GTK_PLOT_SURFACE (surface_data));
		surface_setup_gradient ();
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
	gel_calc_running --;
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

	ret = gel_funccall (ctx, func, args, 3);

	/* FIXME: handle errors! */
	if G_UNLIKELY (gel_error_num != 0)
		gel_error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == GEL_FUNCTION_NODE) {
		if (ret->func.func->nargs == 3) {
			GelETree *ret2;
			ret2 = gel_funccall (ctx, ret->func.func, args, 3);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if G_UNLIKELY (gel_error_num != 0)
				gel_error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			return HUGE_VAL;
#else
			return 0;
#endif
		}

	}

	if (ret == NULL || ret->type != GEL_VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		return HUGE_VAL;
#else
		return 0;
#endif
	}

	retd = mpw_get_double (ret->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		*ex = TRUE;
		gel_error_num = 0;
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

	ret = gel_funccall (ctx, func, args, 2);

	/* FIXME: handle errors! */
	if G_UNLIKELY (gel_error_num != 0)
		gel_error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == GEL_FUNCTION_NODE) {
		if (ret->func.func->nargs == 2) {
			GelETree *ret2;
			ret2 = gel_funccall (ctx, ret->func.func, args, 2);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if G_UNLIKELY (gel_error_num != 0)
				gel_error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			return HUGE_VAL;
#else
			return 0;
#endif
		}
	}

	if (ret == NULL || ret->type != GEL_VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		return HUGE_VAL;
#else
		return 0;
#endif
	}

	retd = mpw_get_double (ret->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		*ex = TRUE;
		gel_error_num = 0;
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

	ret = gel_funccall (ctx, func, args, 1);

	/* FIXME: handle errors! */
	if G_UNLIKELY (gel_error_num != 0)
		gel_error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == GEL_FUNCTION_NODE) {
		if (ret->func.func->nargs == 1) {
			GelETree *ret2;
			ret2 = gel_funccall (ctx, ret->func.func, args, 1);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if G_UNLIKELY (gel_error_num != 0)
				gel_error_num = 0;
		} else if (func_ret != NULL) {
			*func_ret = ret;
#ifdef HUGE_VAL
			return HUGE_VAL;
#else
			return 0;
#endif
		}

	}

	if (ret == NULL || ret->type != GEL_VALUE_NODE) {
		*ex = TRUE;
		gel_freetree (ret);
#ifdef HUGE_VAL
		return HUGE_VAL;
#else
		return 0;
#endif
	}

	retd = mpw_get_double (ret->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		*ex = TRUE;
		gel_error_num = 0;
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

	ret = gel_funccall (ctx, func, args, 1);

	/* FIXME: handle errors! */
	if G_UNLIKELY (gel_error_num != 0)
		gel_error_num = 0;

	/* only do one level of indirection to avoid infinite loops */
	if (ret != NULL && ret->type == GEL_FUNCTION_NODE) {
		if (ret->func.func->nargs == 1) {
			GelETree *ret2;
			ret2 = gel_funccall (ctx, ret->func.func, args, 1);
			gel_freetree (ret);
			ret = ret2;
			/* FIXME: handle errors! */
			if G_UNLIKELY (gel_error_num != 0)
				gel_error_num = 0;
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

	if (ret == NULL || ret->type != GEL_VALUE_NODE) {
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
	if G_UNLIKELY (gel_error_num != 0) {
		*ex = TRUE;
		gel_error_num = 0;
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

#if 0
static double
plot_func_data (GtkPlot *plot, GtkPlotData *data, double x, gboolean *error)
{
	static int hookrun = 0;
	gboolean ex = FALSE;
	int i;
	double y;

	if (error != NULL)
		*error = FALSE;

	if G_UNLIKELY (gel_interrupted) {
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

	/* No need of this anymore, my hacked version handles lines going
	 * way off at least in our use case
	if (y > ploty2 || y < ploty1) {
		if (error != NULL)
			*error = TRUE;
	}
	*/


	if G_UNLIKELY (hookrun++ >= 10) {
		if (gel_evalnode_hook != NULL) {
			hookrun = 0;
			(*gel_evalnode_hook)();
			if G_UNLIKELY (gel_interrupted) {
				if (error != NULL)
					*error = TRUE;
				return y;
			}
		}
	}

	return y;
}
#endif

static double
call_xy_or_z_function (GelEFunc *f, double x, double y, gboolean *ex)
{
	GelETree *func_ret = NULL;
	double z;

	if G_UNLIKELY (f == NULL) {
		if (ex != NULL)
			*ex = TRUE;
		return 0.0;
	}

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


/*
static double
surface_func_data (GtkPlot *plot, GtkPlotData *data, double x, double y, gboolean *error)
*/
static double
surface_func_data (double x, double y, gboolean *error)
{
	static int hookrun = 0;
	gboolean ex = FALSE;
	double z;

	if (error != NULL)
		*error = FALSE;

	if G_UNLIKELY (gel_interrupted || surface_func == NULL) {
		if (error != NULL)
			*error = TRUE;
		return 0.0;
	}

	z = call_xy_or_z_function (surface_func, x, y, &ex);

	if G_UNLIKELY (ex) {
		if (error != NULL)
			*error = TRUE;
	} else {
		if G_UNLIKELY (z > plot_maxz)
			plot_maxz = z;
		if G_UNLIKELY (z < plot_minz)
			plot_minz = z;
	}

	/*
	size = surfacez1 - surfacez2;

	if (z > (surfacez2+size*0.2) || z < (surfacez1-size*0.2)) {
		if (error != NULL)
			*error = TRUE;
	}
	*/


	if G_UNLIKELY (hookrun++ >= 10) {
		if (gel_evalnode_hook != NULL) {
			hookrun = 0;
			(*gel_evalnode_hook)();
			if G_UNLIKELY (gel_interrupted) {
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
		old_style = gel_calcstate.output_style;
		gel_calcstate.output_style = GEL_OUTPUT_NORMAL;
		gel_print_etree (out, func->data.user, TRUE /* toplevel */);
		gel_calcstate.output_style = old_style;

		text = gel_output_snarf_string (out);
		gel_output_unref (out);

		len = strlen (text);

		if (len > 2 &&
		    text[0] == '(' &&
		    text[len-1] == ')') {
			char *s;
			text[len-1] = '\0';
			s = g_strdup (&text[1]);
			g_free (text);
			text = s;
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
	if (a[argnum]->type != GEL_VALUE_NODE) { \
		gel_errorout (_("%s: argument number %d not a number"), func, argnum+1); \
		return NULL; \
	} \
	var = mpw_get_double (a[argnum]->val.value); \
	}

static gboolean
get_limits_from_matrix (GelETree *m, double *x1, double *x2, double *y1, double *y2)
{
	GelETree *t;

	if (m->type != GEL_MATRIX_NODE ||
	    gel_matrixw_elements (m->mat.matrix) != 4) {
		gel_errorout (_("Graph limits not given as a 4-vector"));
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 0);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x1 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 1);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x2 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 2);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*y1 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 3);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*y2 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
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

static gboolean
get_limits_from_matrix_xonly (GelETree *m, double *x1, double *x2)
{
	GelETree *t;

	if (m->type != GEL_MATRIX_NODE ||
	    gel_matrixw_elements (m->mat.matrix) != 2) {
		gel_errorout (_("Graph limits not given as a 2-vector"));
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 0);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x1 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 1);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x2 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	if (*x1 > *x2) {
		double s = *x1;
		*x1 = *x2;
		*x2 = s;
	}

	/* sanity */
	if (*x2 - *x1 < MINPLOT)
		*x2 = *x1 + MINPLOT;

	return TRUE;
}

static GelETree *
make_matrix_from_limits (void)
{
	GelETree *n;
	GelMatrixW *m;
	/*make us a new empty node*/
	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 4, 1);

	gel_matrixw_set_indexii (m, 0) = gel_makenum_d (defx1);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_d (defx2);
	gel_matrixw_set_index (m, 2, 0) = gel_makenum_d (defy1);
	gel_matrixw_set_index (m, 3, 0) = gel_makenum_d (defy2);

	return n;
}

static GelETree *
make_matrix_from_lp_varnames (void)
{
	GelETree *n;
	GelMatrixW *m;
	/*make us a new empty node*/
	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 4, 1);

	init_var_names ();

	gel_matrixw_set_indexii (m, 0) = gel_makenum_string (lp_x_name);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_string (lp_y_name);
	gel_matrixw_set_index (m, 2, 0) = gel_makenum_string (lp_z_name);
	gel_matrixw_set_index (m, 3, 0) = gel_makenum_string (lp_t_name);

	return n;
}

static GelETree *
make_matrix_from_sp_varnames (void)
{
	GelETree *n;
	GelMatrixW *m;
	/*make us a new empty node*/
	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 3, 1);

	init_var_names ();

	gel_matrixw_set_indexii (m, 0) = gel_makenum_string (sp_x_name);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_string (sp_y_name);
	gel_matrixw_set_index (m, 2, 0) = gel_makenum_string (sp_z_name);

	return n;
}

static gboolean
get_limits_from_matrix_surf (GelETree *m, double *x1, double *x2, double *y1, double *y2, double *z1, double *z2)
{
	GelETree *t;

	if (m->type != GEL_MATRIX_NODE ||
	    gel_matrixw_elements (m->mat.matrix) != 6) {
		gel_errorout (_("Graph limits not given as a 6-vector"));
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 0);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x1 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 1);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*x2 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 2);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*y1 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 3);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*y2 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 4);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*z1 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
		return FALSE;
	}

	t = gel_matrixw_vindex (m->mat.matrix, 5);
	if (t->type != GEL_VALUE_NODE) {
		gel_errorout (_("Graph limits not given as numbers"));
		return FALSE;
	}
	*z2 = mpw_get_double (t->val.value);
	if G_UNLIKELY (gel_error_num != 0) {
		gel_error_num = 0;
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
	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 6, 1);

	gel_matrixw_set_indexii (m, 0) = gel_makenum_d (surf_defx1);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_d (surf_defx2);
	gel_matrixw_set_index (m, 2, 0) = gel_makenum_d (surf_defy1);
	gel_matrixw_set_index (m, 3, 0) = gel_makenum_d (surf_defy2);
	gel_matrixw_set_index (m, 4, 0) = gel_makenum_d (surf_defz1);
	gel_matrixw_set_index (m, 5, 0) = gel_makenum_d (surf_defz2);

	return n;
}

static gboolean
get_ticks_from_matrix (GelETree *m, int *v, int *h)
{
	GelETree *t;

	if (m->type == GEL_VALUE_NODE) {
		long n = mpw_get_long (m->val.value);
		if G_UNLIKELY (gel_error_num != 0) {
			gel_error_num = 0;
			return FALSE;
		}
		if (n <= 1 || n > 200) {
			gel_errorout (_("Ticks must be between 2 and 200"));
			return FALSE;
		}
		*v = *h = n;
		return TRUE;
	} else if (m->type == GEL_MATRIX_NODE &&
	    gel_matrixw_elements (m->mat.matrix) == 2) {
		t = gel_matrixw_vindex (m->mat.matrix, 0);
		if (t->type != GEL_VALUE_NODE) {
			gel_errorout (_("Ticks not given as numbers"));
			return FALSE;
		}
		*v = mpw_get_long (t->val.value);
		if G_UNLIKELY (gel_error_num != 0) {
			gel_error_num = 0;
			return FALSE;
		}
		if (*v <= 1 || *v > 200) {
			gel_errorout (_("Ticks must be between 2 and 200"));
			return FALSE;
		}
		t = gel_matrixw_vindex (m->mat.matrix, 1);
		if (t->type != GEL_VALUE_NODE) {
			gel_errorout (_("Ticks not given as numbers"));
			return FALSE;
		}
		*h = mpw_get_long (t->val.value);
		if G_UNLIKELY (gel_error_num != 0) {
			gel_error_num = 0;
			return FALSE;
		}
		if (*h <= 1 || *h > 200) {
			gel_errorout (_("Ticks must be between 2 and 200"));
			return FALSE;
		}
		return TRUE;
	} else {
		gel_errorout (_("Ticks not given as a number or a 2-vector"));
		return FALSE;
	}
}

static GelETree *
make_matrix_from_ticks (int v, int h)
{
	GelETree *n;
	GelMatrixW *m;
	/*make us a new empty node*/
	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 2, 1);

	gel_matrixw_set_indexii (m, 0) = gel_makenum_si (v);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_si (h);

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

	if G_UNLIKELY (hookrun++ >= 10) {
		if (gel_evalnode_hook != NULL) {
			hookrun = 0;
			(*gel_evalnode_hook)();
		}
	}

	return TRUE;
}

static GtkPlotData *
draw_line (double *x, double *y, int len, int thickness, GdkRGBA *color,
	   char *legend, gboolean filled)
{
	GtkPlotData *data;

	data = GTK_PLOT_DATA (gtk_plot_data_new ());
	gtk_plot_data_set_points (data, x, y, NULL, NULL, len);
	g_object_set_data_full (G_OBJECT (data),
				"x", x, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"y", y, (GDestroyNotify)g_free);
	gtk_plot_add_data (GTK_PLOT (line_plot), data);
	if (legend == NULL)
		gtk_plot_data_hide_legend (data);
	else
		gtk_plot_data_set_legend (data,
					  legend);

	gtk_plot_data_set_line_attributes (data,
					   GTK_PLOT_LINE_SOLID,
					   CAIRO_LINE_CAP_ROUND,
					   CAIRO_LINE_JOIN_ROUND,
					   thickness, color);

	gtk_plot_data_fill_area (data, filled);

	gtk_widget_show (GTK_WIDGET (data));


	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

	return data;
}

static GtkPlotData *
draw_points (double *x, double *y, int len, int thickness, GdkRGBA *color,
	     char *legend)
{
	GtkPlotData *data;

	data = GTK_PLOT_DATA (gtk_plot_data_new ());
	gtk_plot_data_set_points (data, x, y, NULL, NULL, len);
	g_object_set_data_full (G_OBJECT (data),
				"x", x, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"y", y, (GDestroyNotify)g_free);
	gtk_plot_add_data (GTK_PLOT (line_plot), data);
	if (legend == NULL)
		gtk_plot_data_hide_legend (data);
	else
		gtk_plot_data_set_legend (data,
					  legend);

	gtk_plot_data_set_line_attributes (data,
					   GTK_PLOT_LINE_SOLID,
					   CAIRO_LINE_CAP_ROUND,
					   CAIRO_LINE_JOIN_ROUND,
					   thickness, color);

	gtk_plot_data_set_connector (data, GTK_PLOT_CONNECT_NONE);

	gtk_widget_show (GTK_WIDGET (data));

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

	return data;
}

static GtkPlotData *
draw_surface_line (double *x, double *y, double *z,
		   int len, int thickness, GdkRGBA *color, char *legend)
{
	GtkPlotData *data;

	data = GTK_PLOT_DATA (gtk_plot_data_new ());
	gtk_plot_data_set_x (data, x);
	gtk_plot_data_set_y (data, y);
	gtk_plot_data_set_z (data, z);
	gtk_plot_data_set_numpoints (data, len);
	g_object_set_data_full (G_OBJECT (data),
				"x", x, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"y", y, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"z", z, (GDestroyNotify)g_free);
	gtk_plot_add_data (GTK_PLOT (surface_plot), data);
	if (legend == NULL)
		gtk_plot_data_hide_legend (data);
	else
		gtk_plot_data_set_legend (data,
					  legend);

	gtk_plot_data_set_line_attributes (data,
					   GTK_PLOT_LINE_SOLID,
					   CAIRO_LINE_CAP_ROUND,
					   CAIRO_LINE_JOIN_ROUND,
					   thickness, color);

	gtk_widget_show (GTK_WIDGET (data));

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

	return data;
}

static GtkPlotData *
draw_surface_points (double *x, double *y, double *z,
		     int len, int thickness, GdkRGBA *color, char *legend)
{
	GtkPlotData *data;

	data = GTK_PLOT_DATA (gtk_plot_data_new ());
	gtk_plot_data_set_x (data, x);
	gtk_plot_data_set_y (data, y);
	gtk_plot_data_set_z (data, z);
	gtk_plot_data_set_numpoints (data, len);
	g_object_set_data_full (G_OBJECT (data),
				"x", x, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"y", y, (GDestroyNotify)g_free);
	g_object_set_data_full (G_OBJECT (data),
				"z", z, (GDestroyNotify)g_free);
	gtk_plot_add_data (GTK_PLOT (surface_plot), data);
	if (legend == NULL)
		gtk_plot_data_hide_legend (data);
	else
		gtk_plot_data_set_legend (data,
					  legend);

	gtk_plot_data_set_line_attributes (data,
					   GTK_PLOT_LINE_SOLID,
					   CAIRO_LINE_CAP_ROUND,
					   CAIRO_LINE_JOIN_ROUND,
					   thickness, color);

	gtk_plot_data_set_connector (data, GTK_PLOT_CONNECT_NONE);

	gtk_widget_show (GTK_WIDGET (data));

	gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
	gtk_plot_canvas_refresh (GTK_PLOT_CANVAS (plot_canvas));

	return data;
}

#if 0
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
#endif

static void
solution_destroyed (GtkWidget *plotdata, gpointer data)
{
	solutions_list = g_slist_remove (solutions_list, plotdata);
}

static void
slopefield_draw_solution (double x, double y, double dx, gboolean is_gui)
{
	double *xx, *yy;
	double cx, cy;
	int len1, len2, len;
	int i;
	GdkRGBA color;
	GQueue points1 = G_QUEUE_INIT;
	GSList *points2 = NULL;
	GList *li;
	GSList *sli;
	GtkPlotData *data;
	double fudgey;

	if (slopefield_func == NULL)
		return;

	plot_in_progress ++;
	gel_calc_running ++;
	plot_window_setup ();

	gdk_rgba_parse (&color, "red");

	fudgey = (ploty2-ploty1)/100;

	len1 = 0;
	cx = x;
	cy = y;
	while (cx < plotx2 && cy > ploty1-fudgey && cy < ploty2+fudgey) {
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

		g_queue_push_tail (&points1, pt);
	}

	len2 = 0;
	cx = x;
	cy = y;
	while (cx > plotx1 && cy > ploty1-fudgey && cy < ploty2+fudgey) {
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
	for (sli = points2; sli != NULL; sli = sli->next) {
		double *pt = sli->data;
		sli->data = NULL;

		xx[i] = pt[0];
		yy[i] = pt[1];

		g_free (pt);

		i++;
	}

	xx[i] = x;
	yy[i] = y;

	i++;

	for (li = points1.head; li != NULL; li = li->next) {
		double *pt = li->data;
		li->data = NULL;

		xx[i] = pt[0];
		yy[i] = pt[1];

		g_free (pt);

		i++;
	}

	g_queue_clear (&points1);
	g_slist_free (points2);

	/* Adjust ends */
	/*clip_line_ends (xx, yy, len);*/

	data = draw_line (xx, yy, len,
			  2 /* thickness */,
			  &color,
			  NULL /* legend */,
			  FALSE /* filled */);
	solutions_list = g_slist_prepend (solutions_list,
					  data);
	g_signal_connect (G_OBJECT (data), "destroy",
			  G_CALLBACK (solution_destroyed), NULL);

	if (is_gui && gel_interrupted)
		gel_interrupted = FALSE;

	plot_in_progress --;
	gel_calc_running --;
	plot_window_setup ();
}

static void
vectorfield_draw_solution (double x, double y, double dt, double tlen, gboolean is_gui)
{
	double *xx, *yy;
	double cx, cy, t;
	int len;
	int i;
	GdkRGBA color;
	GtkPlotData *data;
	gboolean ex;

	if (vectorfield_func_x == NULL ||
	    vectorfield_func_y == NULL ||
	    dt <= 0.0 ||
	    tlen <= 0.0)
		return;

	plot_in_progress ++;
	gel_calc_running ++;
	plot_window_setup ();

	gdk_rgba_parse (&color, "red");

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
		double xk1, xk2, xk3, xk4, xsl;
		double yk1, yk2, yk3, yk4, ysl;

		ex = FALSE;
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

	data = draw_line (xx, yy, len,
			  2 /* thickness */,
			  &color,
			  NULL /* legend */,
			  FALSE /* filled */);
	solutions_list = g_slist_prepend (solutions_list,
					  data);
	g_signal_connect (G_OBJECT (data), "destroy",
			  G_CALLBACK (solution_destroyed), NULL);

	if (is_gui && gel_interrupted)
		gel_interrupted = FALSE;

	plot_in_progress --;
	gel_calc_running--;
	plot_window_setup ();
}


static void
replot_fields (void)
{
	init_var_names ();

	if (slopefield_func != NULL) {
		get_slopefield_points ();
		if (plot_points_num > 0) {
			GdkRGBA color;

			if (slopefield_data == NULL) {
				char *label, *tmp;

				slopefield_data = GTK_PLOT_DATA(gtk_plot_flux_new());
				gtk_plot_add_data (GTK_PLOT (line_plot),
						   slopefield_data);
				gdk_rgba_parse (&color, "blue");
				gtk_plot_data_set_line_attributes
					(slopefield_data,
					 GTK_PLOT_LINE_NONE,
					 CAIRO_LINE_CAP_ROUND,
					 CAIRO_LINE_JOIN_ROUND,
					 1 /* thickness */,
					 &color);
				gtk_plot_data_set_symbol (slopefield_data,
							  GTK_PLOT_SYMBOL_NONE /* symbol type? */,
							  GTK_PLOT_SYMBOL_EMPTY /* symbol style */,
							  1 /* size? */,
							  (plotHtick > 15 || plotVtick > 15) ? 1 : 2
							    /* line_width */,
							  &color /* color */,
							  &color /* border_color? */);


				gtk_plot_flux_set_arrow (GTK_PLOT_FLUX (slopefield_data),
							 0, 0, GTK_PLOT_SYMBOL_EMPTY);

				gtk_plot_flux_show_scale (GTK_PLOT_FLUX (slopefield_data), FALSE);

				tmp = g_strdup_printf ("%s,%s",
						       lp_x_name,
						       lp_y_name);
				label = label_func (-1, slopefield_func, tmp, slopefield_name);
				g_free (tmp);
				/* FIXME: gtkextra is broken (adding the "  ")
				 * and I don't feel like fixing it */
				/* add dy/dx = */
				tmp = g_strconcat ("d",
						   lp_y_name,
						   "/d",
						   lp_x_name,
						   " = ", label, "  ", NULL);
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
			GdkRGBA color;

			if (vectorfield_data == NULL) {
				char *l1, *l2, *tmp;

				vectorfield_data = GTK_PLOT_DATA(gtk_plot_flux_new());
				gtk_plot_add_data (GTK_PLOT (line_plot),
						   vectorfield_data);
				gdk_rgba_parse (&color, "blue");
				gtk_plot_data_set_line_attributes
					(vectorfield_data,
					 GTK_PLOT_LINE_NONE,
					 CAIRO_LINE_CAP_ROUND,
					 CAIRO_LINE_JOIN_ROUND,
					 1 /* thickess */,
					 &color);
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

				tmp = g_strdup_printf ("%s,%s",
						       lp_x_name,
						       lp_y_name);
				l1 = label_func (-1, vectorfield_func_x, tmp, vectorfield_name_x);
				l2 = label_func (-1, vectorfield_func_y, tmp, vectorfield_name_y);
				g_free (tmp);
				/* FIXME: gtkextra is broken (adding the "  ")
				 * and I don't feel like fixing it */
				/*tmp = g_strconcat ("dx/dt = ", l1, ",  dy/dt = ", l2, "  ", NULL);*/
				tmp = g_strconcat ("d",
						   lp_x_name,
						   "/d",
						   lp_t_name,
						   " = ", l1, ",  d",
						   lp_y_name,
						   "/d",
						   lp_t_name,
						   " = ", l2,
						   "  ", NULL);
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

		plot_ctx = gel_eval_get_context ();

		mpw_init (xx);
		plot_arg = gel_makenum_use (xx);

		mpw_init (xx);
		plot_arg2 = gel_makenum_use (xx);

		mpw_init (xx);
		plot_arg3 = gel_makenum_use (xx);
	}
}

typedef struct {
	double x;
	double y;
} Point;

#ifdef NAN
# define BADPTVAL NAN
#else
#ifdef INFINITY
# define BADPTVAL INFINITY
#else
# define BADPTVAL (1.0/0.0)
#endif
#endif

static Point *
function_get_us_a_point (int funci, double x)
{
	gboolean ex = FALSE;
	double rety;
	Point *pt = g_new0 (Point, 1);
	mpw_set_d (plot_arg->val.value, x);
	rety = call_func (plot_ctx, plot_func[funci], plot_arg, &ex, NULL);

	if G_UNLIKELY (ex) {
		pt->x = x;
		pt->y = BADPTVAL;
	} else {
		pt->x = x;
		pt->y = rety;

		if G_UNLIKELY (rety > plot_maxy)
			plot_maxy = rety;
		if G_UNLIKELY (rety < plot_miny)
			plot_miny = rety;
	}

	return pt;
}


static double
approx_arcsin(double x)
{
	/* Pade quotient approximation, see
	 * http://www.ecse.rpi.edu/~wrf/Research/Short_Notes/arcsin/onlyelem.html*/
	return ((-17.0/60.0)*x*x*x+x)/(1.0-(9.0/20.0)*x*x);
}


static void
bisect_points (int funci, GQueue *points, GList *li, double sizex, double sizey, int level, int *count)
{
	Point *pt;
	Point *nextpt;
	Point *prevpt;
	Point *newpt;
	double xdiffscaled;
	double ydiffscaled;
	double xprevdiffscaled;
	double yprevdiffscaled;
	double seglensq;
	gboolean do_bisect = FALSE;
	gboolean bisect_prev = FALSE;
	gboolean bisect_next = FALSE;

	pt = li->data;
	nextpt = li->next->data;

	if ( ! isfinite (nextpt->y) || ! isfinite(pt->y))
		return;

	if (li->prev) {
		prevpt = li->prev->data;
		if ( ! isfinite (prevpt->y))
			prevpt = NULL;
	} else {
		prevpt = NULL;
	}

	xdiffscaled = (nextpt->x-pt->x)/sizex;
	ydiffscaled = (nextpt->y-pt->y)/sizey;

	seglensq = xdiffscaled*xdiffscaled+ydiffscaled*ydiffscaled;

	/* lines of size 1% are fine */
	if (seglensq >= 0.01*0.01) {
		do_bisect = TRUE;
		bisect_next = TRUE;
	} else if (prevpt != NULL) {
		double seglen1sq;
		xprevdiffscaled = (pt->x-prevpt->x)/sizex;
		yprevdiffscaled = (pt->y-prevpt->y)/sizey;

		seglen1sq = xprevdiffscaled*xprevdiffscaled+yprevdiffscaled*yprevdiffscaled;

		/* difference of angles is bigger than approx 0.1 radians */
		if (fabs (approx_arcsin (yprevdiffscaled/sqrt(seglen1sq)) - approx_arcsin (ydiffscaled/sqrt(seglensq))) > 0.1) {
			do_bisect = TRUE;
			bisect_prev = TRUE;
		}
	}


	if (do_bisect) {
		(*count)++;
		newpt = function_get_us_a_point (funci, pt->x + (nextpt->x-pt->x)/2.0);
		g_queue_insert_after (points, li, newpt);

		if (level < 3) {
			GList *linext = li->next;
			if (bisect_prev)
				bisect_points (funci, points, li->prev, sizex, sizey, level+1, count);
			bisect_points (funci, points, li, sizex, sizey, level+1, count);
			if (bisect_next)
				bisect_points (funci, points, linext, sizex, sizey, level+1, count);
		}
	}


}

static void
recompute_function (int funci, double **x, double **y, int *len, gboolean fitting)
{
	int i, count, lentried;
	double maxfuzz;
	double fuzz[16];
	GQueue *points = g_queue_new ();
	GList *li;
	double sizex, sizey;
	double tmpploty1, tmpploty2;

	lentried = WIDTH/2;/* FIXME: perhaps settable */

	/* up to 1% of the interval is fuzzed */
	maxfuzz = 0.01*(plotx2-plotx1)/(lentried-1);
	for (i = 0; i < 16; i++) {
		fuzz[i] = g_random_double_range (-maxfuzz, maxfuzz);
	}

	count = 0;
	for (i = 0; i < lentried; i++) {
		static int hookrun = 0;
		double thex;
		Point *pt;

		if G_UNLIKELY (gel_interrupted) {
			break;
		}

		thex = plotx1 + ((plotx2-plotx1)*(double)i)/(lentried-1) +
			fuzz[i&0xf];
		/* don't fuzz beyond the domain */
		if (thex < plotx1)
			thex = plotx1;
		else if (thex > plotx2)
			thex = plotx2;

		count++;
		pt = function_get_us_a_point (funci, thex);
		g_queue_push_tail (points, pt);

		if G_UNLIKELY (hookrun++ >= 10) {
			if (gel_evalnode_hook != NULL) {
				hookrun = 0;
				(*gel_evalnode_hook)();
				if G_UNLIKELY (gel_interrupted) {
					break;
				}
			}
		}
	}

	if (fitting) {
		sizey = plot_maxy - plot_miny;
		if (sizey <= 0.0)
			sizey = 0.01;
		tmpploty1 = plot_miny - 0.05*sizey;
		tmpploty2 = plot_maxy + 0.05*sizey;

		sizey *= 1.05 * sizey;
	} else {
		sizey = ploty2 - ploty1;
		tmpploty1 = ploty1;
		tmpploty2 = ploty2;
	}
	sizex = plotx2 - plotx1;

	/* sanity */
	if (sizey <= 0.0)
		sizey = 0.01;
	if (sizex <= 0.0)
		sizex = 0.01;


	/* adaptively bisect intervals */
	li = g_queue_peek_head_link (points);
	while (li != NULL && li->next != NULL) {
		Point *pt = li->data;
		GList *orignext = li->next;
		Point *nextpt = orignext->data;

		if ((pt->y < tmpploty1-0.5*sizey && 
		     nextpt->y < tmpploty1-0.5*sizey) ||
		    (pt->y > tmpploty2+0.5*sizey && 
		     nextpt->y > tmpploty2+0.5*sizey)) {
			li = orignext;
			continue;
		}

		bisect_points (funci, points, li, sizex, sizey, 1, &count);
		li = orignext;
	}

	/* find "steep jumps" and insert invalid points */
	li = g_queue_peek_head_link (points);
	while (li != NULL && li->next != NULL) {
		Point *pt = li->data;
		Point *nextpt = li->next->data;
		GList *orignext = li->next;
		double xdiffscaled;
		double ydiffscaled;

		if ( ! isfinite (nextpt->y) || ! isfinite(pt->y)) {
			li = orignext;
			continue;
		}

		xdiffscaled = (nextpt->x-pt->x)/sizex;
		ydiffscaled = (nextpt->y-pt->y)/sizey;

		/* derivative at least 100 after scaling, length bigger than 1% */
		if (100.0*fabs(xdiffscaled) < fabs(ydiffscaled) &&
		    xdiffscaled*xdiffscaled+ydiffscaled*ydiffscaled > 0.01*0.01 &&
		    li->next->next != NULL && li->prev != NULL) {
			Point *prevpt = li->prev->data;
			Point *nextnextpt = li->next->next->data;
			double xnextdiffscaled;
			double ynextdiffscaled;
			double xprevdiffscaled;
			double yprevdiffscaled;

			xnextdiffscaled = (nextnextpt->x-nextpt->x)/sizex;
			ynextdiffscaled = (nextnextpt->y-nextpt->y)/sizey;

			xprevdiffscaled = (pt->x-prevpt->x)/sizex;
			yprevdiffscaled = (pt->y-prevpt->y)/sizey;


			/* too steep! and steeper than surrounding which is derivative at most 10,
			 * or if the prev and next derivatives are of different sign */
			if ( (10.0*fabs(xprevdiffscaled) >= fabs(yprevdiffscaled) ||
			      (ydiffscaled > 0.0 && yprevdiffscaled < 0.0) ||
			      (ydiffscaled < 0.0 && yprevdiffscaled > 0.0) )
			     &&
			     (10.0*fabs(xnextdiffscaled) >= fabs(ynextdiffscaled) ||
			      (ydiffscaled > 0.0 && ynextdiffscaled < 0.0) ||
			      (ydiffscaled < 0.0 && ynextdiffscaled > 0.0) )
			     ) {
				Point *newpt;
				newpt = g_new0 (Point, 1);
				newpt->x = BADPTVAL;
				newpt->y = BADPTVAL;

				g_queue_insert_after (points, li, newpt);
				count++;
			}
		};
		li = orignext;
	}

	*len = count;
	*x = g_new0 (double, count);
	*y = g_new0 (double, count);
	i = 0;
	for (li = g_queue_peek_head_link (points); li != NULL; li = li->next) {
		Point *pt = li->data;
		li->data = NULL;

		(*x)[i] = pt->x;
		(*y)[i] = pt->y;
		i++;

		g_free (pt);
	}

	g_queue_free (points);
}

#if 0
static double
get_maxes(double **y, int len, int place)
{
	double max = 0.0;
	int i;

	for (i = MAX(place-5,1); i < place; i++) {
		double diff = fabs(y[i]-y[i-1]);
		if (diff > max)
			max = diff;
	}

	for (i = place+1; i <= MIN(place+5,len-2); i++) {
		double diff = fabs(y[i]-y[i+1]);
		if (diff > max)
			max = diff;
	}
	return max;
}

/* insert invalid points and places where things jump way too much,
 * FIXME: we should make the step smaller adaptively I think */
static void
cutup_function (double **x, double **y, int *len)
{
	double *oldx = *x;
	double *oldy = *y;
	int oldlen = *len;

	*x = g_new0 (double, *len);
	*y = g_new0 (double, *len);

	for (i
}
#endif


static void
recompute_functions (gboolean fitting)
{
	int i;
	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		double *x, *y;
		int len;
		recompute_function (i, &x, &y, &len, fitting);

		gtk_plot_data_set_points (line_data[i], x, y, NULL, NULL, len);
		g_object_set_data_full (G_OBJECT (line_data[i]),
					"x", x, (GDestroyNotify)g_free);
		g_object_set_data_full (G_OBJECT (line_data[i]),
					"y", y, (GDestroyNotify)g_free);
	}
}

static void
recompute_surface_function (gboolean fitting)
{
	gboolean error = FALSE;
	double x, y;
	int i, j, n;

	/* only if plotting a function do we need to reset the min/max */
	plot_maxz = - G_MAXDOUBLE/2;
	plot_minz = G_MAXDOUBLE/2;

	if (surface_data_x != NULL) g_free (surface_data_x);
	if (surface_data_y != NULL) g_free (surface_data_y);
	if (surface_data_z != NULL) g_free (surface_data_z);
	surface_data_x = (double *)g_malloc((31*31 + 1) * sizeof(double));
	surface_data_y = (double *)g_malloc((31*31 + 1) * sizeof(double));
	surface_data_z = (double *)g_malloc((31*31 + 1) * sizeof(double));

	/* FIXME: 30 should be configurable! */
	n = 0;
	for (j = 0; j <= 30; j++) {
		if (j < 30)
			y = surfacey1 + (j*(surfacey2-surfacey1))/30.0;
		else
			y = surfacey2;
		for (i = 0; i <= 30; i++) {
			if (i < 30)
				x = surfacex1 + (i*(surfacex2-surfacex1))/30.0;
			else
				x = surfacex2;

			surface_data_x[n] = x;
			surface_data_y[n] = y;
			surface_data_z[n] = surface_func_data (x, y, &error);
			n++;
		}
	}

	surface_data_len = n;

	if (fitting) {
		double size = plot_maxz - plot_minz;
		if (size <= 0)
			size = 1.0;
		surfacez1 = plot_minz - size * 0.05;
		surfacez2 = plot_maxz + size * 0.05;
		
		/* sanity */
		if (surfacez2 <= surfacez1)
			surfacez2 = surfacez1 + 0.1;

		/* sanity */
		if (surfacez1 < -(G_MAXDOUBLE/2))
			surfacez1 = -(G_MAXDOUBLE/2);
		if (surfacez2 > (G_MAXDOUBLE/2))
			surfacez2 = (G_MAXDOUBLE/2);
	}

	if (surface_data != NULL) {
		gtk_plot_data_set_x (GTK_PLOT_DATA (surface_data), surface_data_x);
		gtk_plot_data_set_y (GTK_PLOT_DATA (surface_data), surface_data_y);
		gtk_plot_data_set_z (GTK_PLOT_DATA (surface_data), surface_data_z);
		gtk_plot_data_set_numpoints (GTK_PLOT_DATA (surface_data), surface_data_len);
	}
}

static void
plot_functions (gboolean do_window_present,
		gboolean from_gui,
		gboolean fit)
{
	const char *colors[] = {
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

	init_var_names ();

	ensure_window (do_window_present);

	if (plot_canvas != NULL /* sanity */)
		gtk_plot_canvas_freeze (GTK_PLOT_CANVAS (plot_canvas));

	clear_graph ();

	add_line_plot ();

	plot_in_progress ++;
	gel_calc_running ++;
	plot_window_setup ();
	gtk_plot_freeze (GTK_PLOT (line_plot));

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
	plot_maxx = - G_MAXDOUBLE/2;
	plot_minx = G_MAXDOUBLE/2;

	init_plot_ctx ();

	if (gel_evalnode_hook != NULL)
		(*gel_evalnode_hook)();

	color_i = 0;

	for (i = 0; i < MAXFUNC && plot_func[i] != NULL; i++) {
		GdkRGBA color;
		char *label;

		line_data[i] = GTK_PLOT_DATA (gtk_plot_data_new ());
		gtk_plot_add_data (GTK_PLOT (line_plot),
				   line_data[i]);

		gtk_widget_show (GTK_WIDGET (line_data[i]));

		gdk_rgba_parse (&color, colors[color_i++]);
		gtk_plot_data_set_line_attributes (line_data[i],
						   GTK_PLOT_LINE_SOLID,
						   CAIRO_LINE_CAP_ROUND,
						   CAIRO_LINE_JOIN_ROUND,
						   2, &color);

		label = label_func (i, plot_func[i],
				    lp_x_name,
				    plot_func_name[i]);
		gtk_plot_data_set_legend (line_data[i], label);
		g_free (label);
	}

	recompute_functions (fit);

	if (plot_func[0] != NULL && fit) {
		double size = plot_maxy - plot_miny;
		if (size <= 0)
			size = 1.0;
		ploty1 = plot_miny - size * 0.05;
		ploty2 = plot_maxy + size * 0.05;

		/* sanity */
		if (ploty2 <= ploty1)
			ploty2 = ploty1 + 0.1;

		/* sanity */
		if (ploty1 < -(G_MAXDOUBLE/2))
			ploty1 = -(G_MAXDOUBLE/2);
		if (ploty2 > (G_MAXDOUBLE/2))
			ploty2 = (G_MAXDOUBLE/2);
	}


	if ((parametric_func_x != NULL && parametric_func_y != NULL) ||
	    (parametric_func_z != NULL)) {
		GdkRGBA color;
		char *label;
		int len;
		double *x, *y;
		double t;

		parametric_data = GTK_PLOT_DATA (gtk_plot_data_new ());

		/* could be one off, will adjust later */
		len = MAX(ceil (((plott2 - plott1) / plottinc)) + 2,1);
		x = g_new0 (double, len);
		y = g_new0 (double, len);

		t = plott1;
		for (i = 0; i < len; i++) {
			parametric_get_value (&(x[i]), &(y[i]), t);

			if G_UNLIKELY (gel_interrupted) {
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

		gtk_plot_data_set_points (parametric_data, x, y, NULL, NULL, len);
		g_object_set_data_full (G_OBJECT (parametric_data),
					"x", x, (GDestroyNotify)g_free);
		g_object_set_data_full (G_OBJECT (parametric_data),
					"y", y, (GDestroyNotify)g_free);
		gtk_plot_add_data (GTK_PLOT (line_plot), parametric_data);

		gtk_widget_show (GTK_WIDGET (parametric_data));

		gdk_rgba_parse (&color, colors[color_i++]);
		gtk_plot_data_set_line_attributes (parametric_data,
						   GTK_PLOT_LINE_SOLID,
						   CAIRO_LINE_CAP_ROUND,
						   CAIRO_LINE_JOIN_ROUND,
						   2, &color);

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

		if (fit) {
			double sizex = plot_maxx - plot_minx;
			double sizey = plot_maxy - plot_miny;
			if (sizex <= 0)
				sizex = 1.0;
			if (sizey <= 0)
				sizey = 1.0;
			plotx1 = plot_minx - sizex * 0.05;
			plotx2 = plot_maxx + sizex * 0.05;
			ploty1 = plot_miny - sizey * 0.05;
			ploty2 = plot_maxy + sizey * 0.05;

			/* sanity */
			if (plotx2 <= plotx1)
				plotx2 = plotx1 + 0.1;
			if (ploty2 <= ploty1)
				ploty2 = ploty1 + 0.1;

			/* sanity */
			if (plotx1 < -(G_MAXDOUBLE/2))
				plotx1 = -(G_MAXDOUBLE/2);
			if (plotx2 > (G_MAXDOUBLE/2))
				plotx2 = (G_MAXDOUBLE/2);
			if (ploty1 < -(G_MAXDOUBLE/2))
				ploty1 = -(G_MAXDOUBLE/2);
			if (ploty2 > (G_MAXDOUBLE/2))
				ploty2 = (G_MAXDOUBLE/2);
		}
	} 

	plot_setup_axis ();

	replot_fields ();

	line_plot_move_about ();

	/* could be whacked by closing the window or some such */
	if (plot_canvas != NULL) {
		gtk_plot_canvas_thaw (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
	}

	gtk_plot_thaw (GTK_PLOT (line_plot));
	plot_in_progress --;
	gel_calc_running --;
	plot_window_setup ();

	if (gel_evalnode_hook != NULL)
		(*gel_evalnode_hook)();
}

static void
plot_surface_functions (gboolean do_window_present, gboolean fit_function)
{
	init_var_names ();

	ensure_window (do_window_present);

	if (plot_canvas != NULL /* sanity */)
		gtk_plot_canvas_freeze (GTK_PLOT_CANVAS (plot_canvas));

	clear_graph ();

	add_surface_plot ();

	plot_in_progress ++;
	gel_calc_running ++;
	plot_window_setup ();


	/* sanity */
	if (surfacex2 == surfacex1)
		surfacex2 = surfacex1 + MINPLOT;
	if (surfacey2 == surfacey1)
		surfacey2 = surfacey1 + MINPLOT;
	if (surfacez2 == surfacez1)
		surfacez2 = surfacez1 + MINPLOT;


	gtk_plot3d_reset_angles (GTK_PLOT3D (surface_plot));
	gtk_plot3d_rotate_x (GTK_PLOT3D (surface_plot), 60.0);
	gtk_plot3d_rotate_z (GTK_PLOT3D (surface_plot), 30.0);

	init_plot_ctx ();

	if (gel_evalnode_hook != NULL)
		(*gel_evalnode_hook)();

	if (surface_func != NULL) {
		recompute_surface_function (fit_function);
	}

	if (surface_data_x != NULL &&
	    surface_data_y != NULL &&
	    surface_data_z != NULL) {
		surface_data = GTK_PLOT_DATA (gtk_plot_surface_new ());
		gtk_plot_surface_use_amplitud (GTK_PLOT_SURFACE (surface_data), FALSE);
		gtk_plot_surface_use_height_gradient (GTK_PLOT_SURFACE (surface_data), TRUE);
		gtk_plot_surface_set_mesh_visible (GTK_PLOT_SURFACE (surface_data), TRUE);
		if (surfaceplot_draw_legends) {
			gtk_plot_data_gradient_set_visible (GTK_PLOT_DATA (surface_data), TRUE);
			gtk_plot_data_show_legend (GTK_PLOT_DATA (surface_data));
		} else {
			gtk_plot_data_gradient_set_visible (GTK_PLOT_DATA (surface_data), FALSE);
			gtk_plot_data_hide_legend (GTK_PLOT_DATA (surface_data));
		}
		gtk_plot_data_move_gradient (GTK_PLOT_DATA (surface_data),
					     0.93, 0.15);
		gtk_plot_axis_hide_title (GTK_PLOT_DATA (surface_data)->gradient);

		gtk_plot_data_set_x (GTK_PLOT_DATA (surface_data), surface_data_x);
		gtk_plot_data_set_y (GTK_PLOT_DATA (surface_data), surface_data_y);
		gtk_plot_data_set_z (GTK_PLOT_DATA (surface_data), surface_data_z);
		gtk_plot_data_set_numpoints (GTK_PLOT_DATA (surface_data), surface_data_len);

		gtk_plot_add_data (GTK_PLOT (surface_plot),
				   surface_data);

		surface_setup_gradient ();

		gtk_widget_show (GTK_WIDGET (surface_data));
	}

	if (surface_data != NULL) {
		if (surface_func != NULL) {
			char *label = label_func (-1, surface_func, /* FIXME: correct variable */ "...", surface_func_name);
			gtk_plot_data_set_legend (surface_data, label);
			g_free (label);
		} else if (surface_func_name) {
			gtk_plot_data_set_legend (surface_data, surface_func_name);
		} else
			gtk_plot_data_set_legend (surface_data, "");
	}

	surface_setup_axis ();

	/* FIXME: this doesn't work (crashes) must fix in GtkExtra
	gtk_plot3d_autoscale (GTK_PLOT3D (surface_plot));
	*/

	if (surface_data != NULL) {
		gtk_plot_surface_build_mesh (GTK_PLOT_SURFACE (surface_data));
	}


	/* could be whacked by closing the window or some such */
	if (surface_plot != NULL) {
		if (surfaceplot_draw_legends)
			gtk_plot_show_legends (GTK_PLOT (surface_plot));
		else
			gtk_plot_hide_legends (GTK_PLOT (surface_plot));
	}

	/* could be whacked by closing the window or some such */
	if (plot_canvas != NULL) {
		gtk_plot_canvas_thaw (GTK_PLOT_CANVAS (plot_canvas));
		gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
		gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));

		if (gel_evalnode_hook != NULL)
			(*gel_evalnode_hook)();
	}

	plot_in_progress --;
	gel_calc_running --;
	plot_window_setup ();
}

/*exact answer callback*/
static void
int_spin_cb (GtkAdjustment *adj, int *data)
{
	*data = (int)(gtk_adjustment_get_value (adj));
}

static void
entry_activate (void)
{
	if (plot_dialog != NULL)
		gtk_dialog_response (GTK_DIALOG (plot_dialog),
				     RESPONSE_PLOT);
}

static GtkWidget *
create_range_boxes (const char *title, GtkWidget **titlew,
		    const char *def1, GtkWidget **w1,
		    const char *totitle, GtkWidget **totitlew,
		    const char *def2, GtkWidget **w2,
		    const char *bytitle,
		    const char *defby, GtkWidget **wb,
		    GCallback activate_callback)
{
	GtkWidget *b, *w;

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	w = gtk_label_new(title);
	if (titlew != NULL) {
		*titlew = w;
		g_signal_connect (G_OBJECT (w),
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  titlew);
	}
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	w = gtk_entry_new ();
	if (w1 != NULL) {
		*w1 = w;
		g_signal_connect (G_OBJECT (w),
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  w1);
	}
	gtk_entry_set_text (GTK_ENTRY (w), def1);
	g_signal_connect (G_OBJECT (w), "activate",
			  G_CALLBACK (activate_callback), NULL);
	gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);

	if (totitle != NULL) {
		w = gtk_label_new (totitle);
		if (totitlew != NULL) {
			*totitlew = w;
			g_signal_connect (G_OBJECT (w),
					  "destroy",
					  G_CALLBACK (gtk_widget_destroyed),
					  totitlew);
		}
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
		w = gtk_entry_new ();
		if (w2 != NULL) {
			*w2 = w;
			g_signal_connect (G_OBJECT (w),
					  "destroy",
					  G_CALLBACK (gtk_widget_destroyed),
					  w2);
		}
		gtk_entry_set_text (GTK_ENTRY (w), def2);
		g_signal_connect (G_OBJECT (w), "activate",
				  G_CALLBACK (activate_callback), NULL);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	}

	if (bytitle != NULL) {
		w = gtk_label_new (bytitle);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
		w = gtk_entry_new ();
		if (wb != NULL) {
			*wb = w;
			g_signal_connect (G_OBJECT (w),
					  "destroy",
					  G_CALLBACK (gtk_widget_destroyed),
					  wb);
		}
		gtk_entry_set_text (GTK_ENTRY (w), defby);
		g_signal_connect (G_OBJECT (w), "activate",
				  G_CALLBACK (activate_callback), NULL);
		gtk_box_pack_start (GTK_BOX (b), w, FALSE, FALSE, 0);
	}

	return b;
}

static GtkWidget *
create_int_spinbox (const char *title, int *val, int min, int max)
{
	GtkWidget *b, *w;
	GtkAdjustment *adj;

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
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
		       GtkWidget **labelw,
		       GtkWidget **entry,
		       GtkWidget **status)
{
	GtkWidget *b;
	GtkWidget *l;

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);

	l = gtk_label_new (label);
	if (labelw != NULL) {
		*labelw = l;
		g_signal_connect (G_OBJECT (l),
				  "destroy",
				  G_CALLBACK (gtk_widget_destroyed),
				  labelw);
	}

	gtk_box_pack_start (GTK_BOX (b), l, FALSE, FALSE, 0);

	*entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (*entry), "activate",
			  G_CALLBACK (entry_activate), NULL);
	gtk_box_pack_start (GTK_BOX (b), *entry, TRUE, TRUE, 0);

	*status = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (b), *status, FALSE, FALSE, 0);
	return b;
}

static GtkWidget *
create_simple_expression_box (const char *label,
			      GtkWidget **labelw,
			      GtkWidget **entry)
{
	GtkWidget *b;
	GtkWidget *l;

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);

	l = gtk_label_new (label);
	if (labelw != NULL)
		*labelw = l;

	gtk_box_pack_start (GTK_BOX (b), l, FALSE, FALSE, 0);

	*entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (b), *entry, TRUE, TRUE, 0);

	return b;
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
set_lineplot_labels (void)
{
	char *s;
	int i;

	if (plot_dialog == NULL)
		return;

	s = g_strdup_printf (_("Type in function name or expression involving "
			       "the %s and %s variables (or the %s variable which will be %s=%s+i%s) "
			       "that gives the slope "
			       "at the point (%s,%s)."),
			     lp_x_name,
			     lp_y_name,
			     lp_z_name,
			     lp_z_name,
			     lp_x_name,
			     lp_y_name,
			     lp_x_name,
			     lp_y_name);
	gtk_label_set_text (GTK_LABEL (slopefield_info_label), s);
	g_free (s);

	s = g_strdup_printf ("d%s/d%s=",
			     lp_y_name,
			     lp_x_name);
	gtk_label_set_text (GTK_LABEL (slopefield_der_label), s);
	g_free (s);

	s = g_strdup_printf (_("%s from:"),
			     lp_x_name);
	gtk_label_set_text (GTK_LABEL (lineplot_x_range_label), s);
	g_free (s);

	s = g_strdup_printf (_("%s from:"),
			     lp_y_name);
	gtk_label_set_text (GTK_LABEL (lineplot_y_range_label), s);
	g_free (s);

	s = g_strdup_printf (_("Type in function names or expressions involving "
			       "the %s and %s variables (or the %s variable which will be %s=%s+i%s) "
			       "that give the d%s/d%s and d%s/d%s of the autonomous system to be plotted "
			       "at the point (%s,%s)."),
			     lp_x_name,
			     lp_y_name,
			     lp_z_name,
			     lp_z_name,
			     lp_x_name,
			     lp_y_name,
			     lp_x_name,
			     lp_t_name,
			     lp_y_name,
			     lp_t_name,
			     lp_x_name,
			     lp_y_name);
	gtk_label_set_text (GTK_LABEL (vectorfield_info_label), s);
	g_free (s);

	s = g_strdup_printf ("d%s/d%s=",
			     lp_x_name,
			     lp_t_name);
	gtk_label_set_text (GTK_LABEL (vectorfield_xder_label), s);
	g_free (s);

	s = g_strdup_printf ("d%s/d%s=",
			     lp_y_name,
			     lp_t_name);
	gtk_label_set_text (GTK_LABEL (vectorfield_yder_label), s);
	g_free (s);

	s = g_strdup_printf (_("Type in function names or expressions involving "
			       "the %s variable in the boxes below to graph "
			       "them"), lp_x_name);
	gtk_label_set_text (GTK_LABEL (lineplot_info_label), s);
	g_free (s);

	s = g_strdup_printf ("%s=", lp_y_name);
	for (i = 0; i < MAXFUNC; i++) {
		gtk_label_set_text (GTK_LABEL (plot_y_labels[i]), s);
	}
	g_free (s);

	s = g_strdup_printf (_("Type in function names or expressions involving "
			       "the %s variable in the boxes below to graph "
			       "them.  Either fill in both boxes with %s= and %s= "
			       "in front of them giving the %s and %s coordinates "
			       "separately, or alternatively fill in the %s= box "
			       "giving %s and %s as the real and imaginary part of "
			       "a complex number."),
			     lp_t_name,
			     lp_x_name,
			     lp_y_name,
			     lp_x_name,
			     lp_y_name,
			     lp_z_name,
			     lp_x_name,
			     lp_y_name);
	gtk_label_set_text (GTK_LABEL (parametric_info_label), s);
	g_free (s);

	s = g_strdup_printf ("%s=",
			     lp_x_name);
	gtk_label_set_text (GTK_LABEL (parametric_x_label), s);
	g_free (s);

	s = g_strdup_printf ("%s=",
			     lp_y_name);
	gtk_label_set_text (GTK_LABEL (parametric_y_label), s);
	g_free (s);

	s = g_strdup_printf ("%s=",
			     lp_z_name);
	gtk_label_set_text (GTK_LABEL (parametric_z_label), s);
	g_free (s);

	s = g_strdup_printf (_("Parameter %s from:"),
			     lp_t_name);
	gtk_label_set_text (GTK_LABEL (parametric_trange_label), s);
	g_free (s);
}

static void
set_solver_labels (void)
{
	char *s;

	if (solver_dialog == NULL)
		return;

	if (solver_xinc_label != NULL) {
		s = g_strdup_printf (_("%s increment:"),
				     lp_x_name);
		gtk_label_set_text (GTK_LABEL (solver_xinc_label), s);
		g_free (s);
	}

	if (solver_tinc_label != NULL) {
		s = g_strdup_printf (_("%s increment:"),
				     lp_t_name);
		gtk_label_set_text (GTK_LABEL (solver_tinc_label), s);
		g_free (s);
	}

	if (solver_tlen_label != NULL) {
		s = g_strdup_printf (_("%s interval length:"),
				     lp_t_name);
		gtk_label_set_text (GTK_LABEL (solver_tlen_label), s);
		g_free (s);
	}

	if (solver_x_pt_label != NULL) {
		s = g_strdup_printf (_("Point %s:"),
				     lp_x_name);
		gtk_label_set_text (GTK_LABEL (solver_x_pt_label), s);
		g_free (s);
	}

	if (solver_y_pt_label != NULL) {
		s = g_strdup_printf ("%s:",
				     lp_y_name);
		gtk_label_set_text (GTK_LABEL (solver_y_pt_label), s);
		g_free (s);
	}
}

static void
set_surface_labels (void)
{
	char *s;

	if (plot_dialog == NULL)
		return;

	s = g_strdup_printf
		(_("Type a function name or an expression involving "
		   "the %s and %s variables (or the %s variable which will be %s=%s+i%s) "
		   "in the boxes below to graph them.  Functions with one argument only "
		   "will be passed a complex number."),
		 sp_x_name,
		 sp_y_name,
		 sp_z_name,
		 sp_z_name,
		 sp_x_name,
		 sp_y_name);
	gtk_label_set_text (GTK_LABEL (surface_info_label), s);
	g_free (s);

	s = g_strdup_printf (_("%s from:"),
			     sp_x_name);
	gtk_label_set_text (GTK_LABEL (surface_x_range_label), s);
	g_free (s);

	s = g_strdup_printf (_("%s from:"),
			     sp_y_name);
	gtk_label_set_text (GTK_LABEL (surface_y_range_label), s);
	g_free (s);
}

static char *
get_varname_from_entry (GtkEntry *e, const char *def, gboolean *ex)
{
	char *str = g_strdup (gtk_entry_get_text (GTK_ENTRY (e)));
	if (ve_string_empty (str)) {
		g_free (str); 
		*ex = TRUE;
		return g_strdup (def);
	}

	if ( ! is_identifier (str))
		*ex = TRUE;

	str = g_strcanon (str,
			  G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "_",
			  '_');
	if (str[0] >= '0' && str[0] <= '9') {
		*ex = TRUE;
		str[0] = '_';
	}

	return str;
}

static void
change_lineplot_varnames (GtkWidget *button, gpointer data)
{
	GtkWidget *req = NULL;
	GtkWidget *b, *l;
	GtkWidget *xe;
	GtkWidget *ye;
	GtkWidget *ze;
	GtkWidget *te;
	GtkWidget *errlabel;
        GtkSizeGroup *sg;

	req = gtk_dialog_new_with_buttons
		(_("Change variable names") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 GTK_DIALOG_MODAL /* flags */,
		 _("_OK"),
		 GTK_RESPONSE_OK,
		 _("_Cancel"),
		 GTK_RESPONSE_CANCEL,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (req),
					 GTK_RESPONSE_OK);

	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	errlabel = gtk_label_new (_("Some values were illegal"));
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    errlabel, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("independent variable (x):"),
					  &l, &xe);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (xe), lp_x_name);
	g_signal_connect (G_OBJECT (xe), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("dependent variable (y):"),
					  &l, &ye);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (ye), lp_y_name);
	g_signal_connect (G_OBJECT (ye), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("complex variable (z = x+iy):"),
					  &l, &ze);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (ze), lp_z_name);
	g_signal_connect (G_OBJECT (ze), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("parameter variable (t):"),
					  &l, &te);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (te), lp_t_name);
	g_signal_connect (G_OBJECT (te), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	gtk_widget_show_all (req);
	gtk_widget_hide (errlabel);

run_dialog_again:

	if (gtk_dialog_run (GTK_DIALOG (req)) == GTK_RESPONSE_OK) {
		gboolean ex = FALSE;
		char *xn, *yn, *zn, *tn;

		xn = get_varname_from_entry (GTK_ENTRY (xe), "x", &ex);
		yn = get_varname_from_entry (GTK_ENTRY (ye), "y", &ex);
		zn = get_varname_from_entry (GTK_ENTRY (ze), "z", &ex);
		tn = get_varname_from_entry (GTK_ENTRY (te), "t", &ex);
		if (strcmp (xn, yn) == 0 ||
		    strcmp (xn, zn) == 0 ||
		    strcmp (xn, tn) == 0 ||
		    strcmp (yn, zn) == 0 ||
		    strcmp (yn, tn) == 0 ||
		    strcmp (zn, tn) == 0)
			ex = TRUE;
		if (ex) {
			gtk_entry_set_text (GTK_ENTRY (xe), xn);
			gtk_entry_set_text (GTK_ENTRY (ye), yn);
			gtk_entry_set_text (GTK_ENTRY (ze), zn);
			gtk_entry_set_text (GTK_ENTRY (te), tn);
			g_free (xn);
			g_free (yn);
			g_free (zn);
			g_free (tn);
			gtk_widget_show (errlabel);
			goto run_dialog_again;
		}

		g_free (lp_x_name);
		g_free (lp_y_name);
		g_free (lp_z_name);
		g_free (lp_t_name);
		lp_x_name = xn;
		lp_y_name = yn;
		lp_z_name = zn;
		lp_t_name = tn;
	}
	gtk_widget_destroy (req);

	set_lineplot_labels ();
	set_solver_labels ();
}

static void
change_surface_varnames (GtkWidget *button, gpointer data)
{
	GtkWidget *req = NULL;
	GtkWidget *b, *l;
	GtkWidget *xe;
	GtkWidget *ye;
	GtkWidget *ze;
	GtkWidget *errlabel;
        GtkSizeGroup *sg;

	req = gtk_dialog_new_with_buttons
		(_("Change variable names") /* title */,
		 GTK_WINDOW (graph_window) /* parent */,
		 GTK_DIALOG_MODAL /* flags */,
		 _("_OK"),
		 GTK_RESPONSE_OK,
		 _("_Cancel"),
		 GTK_RESPONSE_CANCEL,
		 NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (req),
					 GTK_RESPONSE_OK);

	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	errlabel = gtk_label_new (_("Some values were illegal"));
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    errlabel, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("independent variable (x):"),
					  &l, &xe);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (xe), sp_x_name);
	g_signal_connect (G_OBJECT (xe), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("independent variable (y):"),
					  &l, &ye);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (ye), sp_y_name);
	g_signal_connect (G_OBJECT (ye), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	b = create_simple_expression_box (_("independent complex variable (z = x+iy):"),
					  &l, &ze);
	gtk_label_set_xalign (GTK_LABEL (l), 0.0);
	gtk_size_group_add_widget (sg, l);
	gtk_entry_set_text (GTK_ENTRY (ze), sp_z_name);
	g_signal_connect (G_OBJECT (ze), "activate",
			  G_CALLBACK (ok_dialog_entry_activate), req);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (req))),
			    b, FALSE, FALSE, 0);

	gtk_widget_show_all (req);
	gtk_widget_hide (errlabel);

run_dialog_again:

	if (gtk_dialog_run (GTK_DIALOG (req)) == GTK_RESPONSE_OK) {
		gboolean ex = FALSE;
		char *xn, *yn, *zn;

		xn = get_varname_from_entry (GTK_ENTRY (xe), "x", &ex);
		yn = get_varname_from_entry (GTK_ENTRY (ye), "y", &ex);
		zn = get_varname_from_entry (GTK_ENTRY (ze), "z", &ex);
		if (strcmp (xn, yn) == 0 ||
		    strcmp (xn, zn) == 0 ||
		    strcmp (yn, zn) == 0)
			ex = TRUE;
		if (ex) {
			gtk_entry_set_text (GTK_ENTRY (xe), xn);
			gtk_entry_set_text (GTK_ENTRY (ye), yn);
			gtk_entry_set_text (GTK_ENTRY (ze), zn);
			g_free (xn);
			g_free (yn);
			g_free (zn);
			gtk_widget_show (errlabel);
			goto run_dialog_again;
		}

		g_free (sp_x_name);
		g_free (sp_y_name);
		g_free (sp_z_name);
		sp_x_name = xn;
		sp_y_name = yn;
		sp_z_name = zn;
	}
	gtk_widget_destroy (req);

	set_surface_labels ();
}

static void
setup_page (int page)
{
	if (page == 0 /* functions */) {
		gtk_widget_set_sensitive (lineplot_fit_dep_axis_checkbox, TRUE);

		if (lineplot_fit_dependent_axis_cb) {
			gtk_widget_set_sensitive (lineplot_dep_axis_buttons, FALSE);
		} else {
			gtk_widget_set_sensitive (lineplot_dep_axis_buttons, TRUE);
		}
		gtk_widget_set_sensitive (lineplot_depx_axis_buttons, TRUE);
	} else if (page == 1 /* parametric */) {
		gtk_widget_set_sensitive (lineplot_fit_dep_axis_checkbox, TRUE);

		if (lineplot_fit_dependent_axis_cb) {
			gtk_widget_set_sensitive (lineplot_dep_axis_buttons, FALSE);
			gtk_widget_set_sensitive (lineplot_depx_axis_buttons, FALSE);
		} else {
			gtk_widget_set_sensitive (lineplot_dep_axis_buttons, TRUE);
			gtk_widget_set_sensitive (lineplot_depx_axis_buttons, TRUE);
		}
	} else {
		gtk_widget_set_sensitive (lineplot_fit_dep_axis_checkbox, FALSE);

		gtk_widget_set_sensitive (lineplot_dep_axis_buttons, TRUE);
		gtk_widget_set_sensitive (lineplot_depx_axis_buttons, TRUE);
	}
}

static void
lineplot_switch_page_cb (GtkNotebook *notebook, GtkWidget *page, guint page_num,
			 gpointer data)
{
	setup_page (page_num);
}


/*option callback*/
static void
lineplot_fit_cb_cb (GtkWidget * widget)
{
	int function_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (function_notebook));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		lineplot_fit_dependent_axis_cb = TRUE;
	} else {
		lineplot_fit_dependent_axis_cb = FALSE;
	}

	setup_page (function_page);

}


static GtkWidget *
create_lineplot_box (void)
{
	GtkWidget *mainbox, *frame;
	GtkWidget *box, *hbox, *b, *fb, *w;
	GdkMonitor *monitor;
	GdkRectangle geom;
	int i;

	init_var_names ();

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (mainbox), GENIUS_PAD);

	function_notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (mainbox), function_notebook, FALSE, FALSE, 0);

	/*
	 * Line plot entries
	 */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	lineplot_info_label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (lineplot_info_label), 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (lineplot_info_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (lineplot_info_label), 30);
	gtk_widget_set_size_request (lineplot_info_label, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), lineplot_info_label, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (lineplot_info_label),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &lineplot_info_label);

	fb = box;

	monitor = gdk_display_get_primary_monitor (gdk_display_get_default ());
	gdk_monitor_get_geometry (monitor, &geom);
	if (geom.height < 800) {
		w = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
						GTK_POLICY_NEVER,
						GTK_POLICY_ALWAYS);
		gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

		b = gtk_viewport_new (NULL, NULL);
		gtk_container_add (GTK_CONTAINER (w), b);

		fb = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
		gtk_container_set_border_width (GTK_CONTAINER (fb), GENIUS_PAD);

		gtk_container_add (GTK_CONTAINER (b), fb);
	}



	for (i = 0; i < MAXFUNC; i++) {
		b = create_expression_box ("y=",
					   &(plot_y_labels[i]),
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

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	parametric_info_label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (parametric_info_label), 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (parametric_info_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (parametric_info_label), 30);
	gtk_widget_set_size_request (parametric_info_label, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), parametric_info_label, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (parametric_info_label),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &parametric_info_label);

	/* x */
	b = create_expression_box ("x=",
				   &parametric_x_label,
				   &parametric_entry_x,
				   &parametric_status_x);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* y */
	b = create_expression_box ("y=",
				   &parametric_y_label,
				   &parametric_entry_y,
				   &parametric_status_y);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	w = gtk_label_new (_("or"));
	gtk_label_set_xalign (GTK_LABEL (w), 0.0);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

	/* z */
	b = create_expression_box ("z=",
				   &parametric_z_label,
				   &parametric_entry_z,
				   &parametric_status_z);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* just spacing */
	gtk_box_pack_start (GTK_BOX (box), gtk_label_new (""), FALSE, FALSE, 0);

	/* t range */
	b = create_range_boxes (_("Parameter t from:"), &parametric_trange_label,
				spint1_default, &spint1_entry,
				_("to:"), NULL,
				spint2_default, &spint2_entry,
				_("by:"),
				spintinc_default, &spintinc_entry,
				entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (function_notebook),
				  box,
				  gtk_label_new_with_mnemonic (_("Pa_rametric")));

	/*
	 * Slopefield
	 */

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);

	slopefield_info_label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (slopefield_info_label), 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (slopefield_info_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (slopefield_info_label), 30);
	gtk_widget_set_size_request (slopefield_info_label, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), slopefield_info_label, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (slopefield_info_label),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &slopefield_info_label);

	/* dy/dx */
	b = create_expression_box ("dy/dx=",
				   &slopefield_der_label,
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

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	vectorfield_info_label = gtk_label_new ("");

	gtk_label_set_xalign (GTK_LABEL (vectorfield_info_label), 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (vectorfield_info_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (vectorfield_info_label), 30);
	gtk_widget_set_size_request (vectorfield_info_label, 610, -1);
	gtk_box_pack_start (GTK_BOX (box), vectorfield_info_label, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (vectorfield_info_label),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &vectorfield_info_label);

	/* dx/dt */
	b = create_expression_box ("dx/dt=",
				   &vectorfield_xder_label,
				   &vectorfield_entry_x,
				   &vectorfield_status_x);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	/* dy/dt */
	b = create_expression_box ("dy/dt=",
				   &vectorfield_yder_label,
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

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (mainbox), hbox, FALSE, FALSE, 0);

	/* draw legend? */
	w = gtk_check_button_new_with_mnemonic (_("_Draw legend"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      lineplot_draw_legends_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&lineplot_draw_legends_cb);

	/* draw axis labels? */
	w = gtk_check_button_new_with_mnemonic (_("Draw axis labels"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      lineplot_draw_labels_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&lineplot_draw_labels_cb);

	/* change varnames */
	b = gtk_button_new_with_label (_("Change variable names..."));
	gtk_box_pack_end (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (change_lineplot_varnames), NULL);



	/* plot window */
	frame = gtk_frame_new (_("Plot Window"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/*
	 * X range
	 */
	b = create_range_boxes (_("X from:"), &lineplot_x_range_label,
				spinx1_default, &spinx1_entry,
				_("to:"), NULL,
				spinx2_default, &spinx2_entry,
				NULL, NULL, NULL,
				entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);
	lineplot_depx_axis_buttons = b;

	/*
	 * Y range
	 */
	b = create_range_boxes (_("Y from:"), &lineplot_y_range_label,
				spiny1_default, &spiny1_entry,
				_("to:"), NULL,
				spiny2_default, &spiny2_entry,
				NULL, NULL, NULL,
				entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);
	lineplot_dep_axis_buttons = b;

	/* fit dependent axis? */
	w = gtk_check_button_new_with_label (_("Fit dependent axis"));
	lineplot_fit_dep_axis_checkbox = w;
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      lineplot_fit_dependent_axis_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (lineplot_fit_cb_cb), NULL);
	lineplot_fit_cb_cb (w);



	/* set labels correctly */
	set_lineplot_labels ();

	g_signal_connect (G_OBJECT (function_notebook), "switch_page",
			  G_CALLBACK (lineplot_switch_page_cb), NULL);

	return mainbox;
}

/*option callback*/
static void
surface_fit_cb_cb (GtkWidget * widget)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		surfaceplot_fit_dependent_axis_cb = TRUE;
		gtk_widget_set_sensitive (surfaceplot_dep_axis_buttons, FALSE);
	} else {
		surfaceplot_fit_dependent_axis_cb = FALSE;
		gtk_widget_set_sensitive (surfaceplot_dep_axis_buttons, TRUE);
	}
}

static GtkWidget *
create_surface_box (void)
{
	GtkWidget *mainbox, *frame;
	GtkWidget *hbox, *box, *b, *w;

	init_var_names ();

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (mainbox), GENIUS_PAD);
	
	frame = gtk_frame_new (_("Function / Expression"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);
	surface_info_label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (surface_info_label), 0.0);
	gtk_label_set_line_wrap (GTK_LABEL (surface_info_label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (surface_info_label), 30);
	gtk_widget_set_size_request (surface_info_label, 610, -1);
	gtk_label_set_line_wrap (GTK_LABEL (surface_info_label), TRUE);

	gtk_box_pack_start (GTK_BOX (box), surface_info_label, FALSE, FALSE, 0);

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);

	surface_entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (surface_entry), "activate",
			  G_CALLBACK (entry_activate), NULL);
	gtk_box_pack_start (GTK_BOX (b), surface_entry, TRUE, TRUE, 0);

	surface_entry_status = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (b), surface_entry_status, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GENIUS_PAD);
	gtk_box_pack_start (GTK_BOX (mainbox), hbox, FALSE, FALSE, 0);

	/* draw legend? */
	w = gtk_check_button_new_with_mnemonic (_("_Draw legend"));
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      surfaceplot_draw_legends_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (optioncb),
			  (gpointer)&surfaceplot_draw_legends_cb);

	/* change varnames */

	b = gtk_button_new_with_label (_("Change variable names..."));
	gtk_box_pack_end (GTK_BOX (hbox), b, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (b), "clicked",
			  G_CALLBACK (change_surface_varnames), NULL);

	/*
	 * Plot window frame
	 */

	frame = gtk_frame_new (_("Plot Window"));
	gtk_box_pack_start (GTK_BOX (mainbox), frame, FALSE, FALSE, 0);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, GENIUS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (box), GENIUS_PAD);
	gtk_container_add (GTK_CONTAINER (frame), box);

	/*
	 * X range
	 */
	b = create_range_boxes (_("X from:"), &surface_x_range_label,
				surf_spinx1_default, &surf_spinx1_entry,
				_("to:"), NULL,
				surf_spinx2_default, &surf_spinx2_entry,
				NULL, NULL, NULL,
				entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/*
	 * Y range
	 */
	b = create_range_boxes (_("Y from:"), &surface_y_range_label,
				surf_spiny1_default, &surf_spiny1_entry,
				_("to:"), NULL,
				surf_spiny2_default, &surf_spiny2_entry,
				NULL, NULL, NULL,
				entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);

	/*
	 * Z range
	 */
	b = create_range_boxes (_("Dependent axis from:"), NULL,
				surf_spinz1_default, &surf_spinz1_entry,
				_("to:"), NULL,
				surf_spinz2_default, &surf_spinz2_entry,
				NULL, NULL, NULL,
				entry_activate);
	gtk_box_pack_start (GTK_BOX(box), b, FALSE, FALSE, 0);
	surfaceplot_dep_axis_buttons = b;

	/* fit dependent axis? */
	w = gtk_check_button_new_with_label (_("Fit dependent axis"));
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
				      surfaceplot_fit_dependent_axis_cb);
	g_signal_connect (G_OBJECT (w), "toggled",
			  G_CALLBACK (surface_fit_cb_cb), NULL);
	surface_fit_cb_cb (w);

	/* set labels correctly */
	set_surface_labels ();

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

static GelEFunc *
function_from_expression (const char *e, const char *var, gboolean *ex)
{
	GelEFunc *f = NULL;
	GelETree *value;
	char *ce;

	if (ve_string_empty (e))
		return NULL;

	ce = g_strstrip (g_strdup (e));
	if (is_identifier (ce) &&
	    strcmp (ce, var) != 0) {
		f = d_lookup_global (d_intern (ce));
		if (f != NULL && f->nargs > 0) {
			/* Is a function, so just pass that along */
			g_free (ce);
			f = d_copyfunc (f);
			f->context = -1;
			return f;
		} else if (f != NULL) {
			/* this is a constant, just evaluate it
			 * as any other expression */
			f = NULL;
		} else {
			/* can't find it, throw exception */
			g_free (ce);
			*ex = TRUE;
			return NULL;
		}
	}

	value = gel_parseexp (ce,
			      NULL /* infile */,
			      FALSE /* exec_commands */,
			      FALSE /* testparse */,
			      NULL /* finished */,
			      NULL /* dirprefix */);
	g_free (ce);

	/* Have to reset the error here, else we may die */
	gel_error_num = GEL_NO_ERROR;
	gel_got_eof = FALSE;

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
function_from_expression2 (const char *e,
			   const char *xname,
			   const char *yname,
			   const char *zname,
			   gboolean *ex)
{
	GelEFunc *f = NULL;
	GelETree *value;
	char *ce;
	gboolean got_x, got_y, got_z;

	if (ve_string_empty (e))
		return NULL;

	ce = g_strstrip (g_strdup (e));
	if (is_identifier (ce) &&
	    strcmp (ce, xname) != 0 &&
	    strcmp (ce, yname) != 0 &&
	    strcmp (ce, zname) != 0) {
		f = d_lookup_global (d_intern (ce));
		if (f != NULL && f->nargs > 0) {
			/* Is a function, so just pass that along */
			g_free (ce);
			f = d_copyfunc (f);
			f->context = -1;
			return f;
		} else if (f != NULL) {
			/* this is a constant, just evaluate it
			 * as any other expression */
			f = NULL;
		} else {
			/* can't find it, throw exception */
			g_free (ce);
			*ex = TRUE;
			return NULL;
		}
	}

	value = gel_parseexp (ce,
			      NULL /* infile */,
			      FALSE /* exec_commands */,
			      FALSE /* testparse */,
			      NULL /* finished */,
			      NULL /* dirprefix */);
	g_free (ce);

	/* Have to reset the error here, else we may die */
	gel_error_num = GEL_NO_ERROR;
	gel_got_eof = FALSE;

	/* FIXME: funcbody?  I think it must be done. */
	got_x = gel_eval_find_identifier (value, d_intern (xname), TRUE /*funcbody*/);
	got_y = gel_eval_find_identifier (value, d_intern (yname), TRUE /*funcbody*/);
	got_z = gel_eval_find_identifier (value, d_intern (zname), TRUE /*funcbody*/);

	/* FIXME: if "x" or "y" or "z" not used try to evaluate and if it returns a function use that */
	if (value != NULL) {
		if ( ! got_x && ! got_y && got_z) {
			f = d_makeufunc (NULL /* id */,
					 value,
					 g_slist_append (NULL, d_intern (zname)),
					 1,
					 NULL /* extra_dict */);
		} else if ( ! got_z) {
			GSList *l = g_slist_append (NULL, d_intern (xname));
			l = g_slist_append (l, d_intern (yname));
			f = d_makeufunc (NULL /* id */,
					 value,
					 l,
					 2,
					 NULL /* extra_dict */);
		} else {
			GSList *l = g_slist_append (NULL, d_intern (xname));
			l = g_slist_append (l, d_intern (yname));
			l = g_slist_append (l, d_intern (zname));
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

static gboolean
get_number_from_entry (GtkWidget *entry, GtkWidget *win, double *num)
{
	GelETree *t;
	double d;
	char *str = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	if (ve_string_empty (str)) {
		genius_display_error (win, _("Empty value as range value"));
		if (str != NULL) g_free (str); 
		return FALSE;
	}

	if (gel_calc_running) {
		genius_display_error (win, _("Genius is executing something already"));
		g_free (str);
		return FALSE;
	}

	gel_calc_running++;
	gel_execinit ();
	t = gel_parseexp (str, NULL, FALSE, FALSE, NULL, NULL);
	g_free (str);
	if (t == NULL) {
		gel_calc_running--;
		genius_display_error (win, _("Cannot parse range value"));
		return FALSE;
	}

	t = gel_runexp (t);
	gel_calc_running--;

	/* FIXME: handle errors? ! */
	if G_UNLIKELY (gel_error_num != 0)
		gel_error_num = 0;

	if (t == NULL) {
		genius_display_error (win, _("Cannot execute range value"));
		return FALSE;
	}

	if (t->type != GEL_VALUE_NODE) {
		genius_display_error (win, _("Range value not a value"));
		gel_freetree(t);
		return FALSE;
	}

	if (t->type != GEL_VALUE_NODE) {
		genius_display_error (win, _("Range value not a value"));
		gel_freetree(t);
		return FALSE;
	}

	d = mpw_get_double (t->val.value);
	gel_freetree (t);
	if G_UNLIKELY (gel_error_num != 0) {
		genius_display_error (win, _("Cannot convert range value to a reasonable real number"));
		gel_error_num = 0;
		return FALSE;
	}

	*num = d;
	
	return TRUE;
}


static GelEFunc *
get_func_from_entry (GtkWidget *entry, GtkWidget *status,
		     const char *var, gboolean *ex)
{
	GelEFunc *f;
	const char *str = gtk_entry_get_text (GTK_ENTRY (entry));
	f = function_from_expression (str, var, ex);
	if (f != NULL) {
		gtk_image_set_from_icon_name
			(GTK_IMAGE (status),
			 "gtk-yes",
			 GTK_ICON_SIZE_MENU);
	} else if (*ex) {
		gtk_image_set_from_icon_name
			(GTK_IMAGE (status),
			 "dialog-warning",
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
		      const char *xname,
		      const char *yname,
		      const char *zname,
		      gboolean *ex)
{
	GelEFunc *f;
	const char *str = gtk_entry_get_text (GTK_ENTRY (entry));
	f = function_from_expression2 (str, xname, yname, zname, ex);
	if (f != NULL) {
		gtk_image_set_from_icon_name
			(GTK_IMAGE (status),
			 "gtk-yes",
			 GTK_ICON_SIZE_MENU);
	} else if (*ex) {
		gtk_image_set_from_icon_name
			(GTK_IMAGE (status),
			 "dialog-warning",
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
	long last_errnum;
	char *error_to_print = NULL;
	gboolean ex;

	plot_mode = MODE_SURFACE;

	last_errnum = total_errors;

	ex = FALSE;
	func = get_func_from_entry2 (surface_entry, surface_entry_status, 
				     sp_x_name, sp_y_name, sp_z_name, &ex);

	if (func == NULL) {
		error_to_print = g_strdup (_("No functions to plot or no functions "
					     "could be parsed"));
		goto whack_copied_funcs;
	}

	surfaceplot_draw_legends = surfaceplot_draw_legends_cb;

	if ( ! get_number_from_entry (surf_spinx1_entry, plot_dialog, &x1) ||
	     ! get_number_from_entry (surf_spinx2_entry, plot_dialog, &x2) ||
	     ! get_number_from_entry (surf_spiny1_entry, plot_dialog, &y1) ||
	     ! get_number_from_entry (surf_spiny2_entry, plot_dialog, &y2) ||
	     ! get_number_from_entry (surf_spinz1_entry, plot_dialog, &z1) ||
	     ! get_number_from_entry (surf_spinz2_entry, plot_dialog, &z2) ) {
		goto whack_copied_funcs;
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
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  sp_x_name);
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  sp_y_name);
		goto whack_copied_funcs;
	}

	if (z1 == z2) {
		error_to_print = g_strdup (_("Invalid dependent range"));
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
	
	/* don't plot from data */
	if (surface_data_x != NULL) {
		g_free (surface_data_x);
		surface_data_x = NULL;
	}
	if (surface_data_y != NULL) {
		g_free (surface_data_y);
		surface_data_y = NULL;
	}
	if (surface_data_z != NULL) {
		g_free (surface_data_z);
		surface_data_z = NULL;
	}
	/* just in case */
	if (surface_data != NULL) {
		gtk_plot_data_set_x (GTK_PLOT_DATA (surface_data), NULL);
		gtk_plot_data_set_y (GTK_PLOT_DATA (surface_data), NULL);
		gtk_plot_data_set_z (GTK_PLOT_DATA (surface_data), NULL);
	}

	/* setup name when the functions don't have their own name */
	if (surface_func->id == NULL)
		surface_func_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (surface_entry)));

	plot_mode = MODE_SURFACE;
	plot_surface_functions (TRUE /* do_window_present */,
				surfaceplot_fit_dependent_axis_cb /*fit*/);

	if (surfaceplot_fit_dependent_axis_cb) {
		reset_surfacez1 = surfacez1;
		reset_surfacez2 = surfacez2;
	}

	if (gel_interrupted)
		gel_interrupted = FALSE;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	return;

whack_copied_funcs:
	if (func != NULL) {
		d_freefunc (func);
		func = NULL;
	}

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	if (error_to_print != NULL) {
		genius_display_error (plot_dialog, error_to_print);
		g_free (error_to_print);
	}
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
	char *error_to_print = NULL;
	long last_errnum;

	plot_mode = MODE_LINEPLOT;

	last_errnum = total_errors;

	for (i = 0; i < MAXFUNC; i++) {
		GelEFunc *f;
		gboolean ex = FALSE;
		f = get_func_from_entry (plot_entries[i],
					 plot_entries_status[i],
					 lp_x_name,
					 &ex);
		if (f != NULL) {
			func[i] = f;
			funcs++;
		}
	}

	if (funcs == 0) {
		error_to_print = g_strdup (_("No functions to plot or no functions "
					     "could be parsed"));
		goto whack_copied_funcs;
	}

	if ( ! get_number_from_entry (spinx1_entry, plot_dialog, &x1) ||
	     ! get_number_from_entry (spinx2_entry, plot_dialog, &x2) ||
	     ! get_number_from_entry (spiny1_entry, plot_dialog, &y1) ||
	     ! get_number_from_entry (spiny2_entry, plot_dialog, &y2) ) {
		goto whack_copied_funcs;
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
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_x_name);
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_y_name);
		goto whack_copied_funcs;
	}

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;

	line_plot_clear_funcs ();

	j = 0;
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

	plot_functions (TRUE /* do_window_present */,
			TRUE /* from_gui */,
			lineplot_fit_dependent_axis_cb /*fit*/);

	if (lineplot_fit_dependent_axis_cb) {
		reset_ploty1 = ploty1;
		reset_ploty2 = ploty2;
	}

	if (gel_interrupted)
		gel_interrupted = FALSE;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	return;

whack_copied_funcs:
	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		d_freefunc (func[i]);
		func[i] = NULL;
	}

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	if (error_to_print != NULL) {
		genius_display_error (plot_dialog, error_to_print);
		g_free (error_to_print);
	}
}

static void
plot_from_dialog_parametric (void)
{
	GelEFunc *funcpx = NULL;
	GelEFunc *funcpy = NULL;
	GelEFunc *funcpz = NULL;
	double x1, x2, y1, y2;
	char *error_to_print = NULL;
	gboolean exx = FALSE;
	gboolean exy = FALSE;
	gboolean exz = FALSE;
	long last_errnum;

	plot_mode = MODE_LINEPLOT_PARAMETRIC;

	last_errnum = total_errors;

	funcpx = get_func_from_entry (parametric_entry_x,
				      parametric_status_x,
				      lp_t_name,
				      &exx);
	funcpy = get_func_from_entry (parametric_entry_y,
				      parametric_status_y,
				      lp_t_name,
				      &exy);
	funcpz = get_func_from_entry (parametric_entry_z,
				      parametric_status_z,
				      lp_t_name,
				      &exz);
	if (((funcpx || exx) || (funcpy || exy)) && (funcpz || exz)) {
		error_to_print = g_strdup_printf (_("Only specify %s and %s, or %s, not all at once."), lp_x_name, lp_y_name, lp_z_name);
		goto whack_copied_funcs;
	}

	if ( ! ( (funcpz == NULL && funcpx != NULL && funcpy != NULL) ||
		 (funcpz != NULL && funcpx == NULL && funcpy == NULL))) {
		error_to_print = g_strdup (_("No functions to plot or no functions "
					     "could be parsed"));
		goto whack_copied_funcs;
	}

	if ( ! get_number_from_entry (spinx1_entry, plot_dialog, &x1) ||
	     ! get_number_from_entry (spinx2_entry, plot_dialog, &x2) ||
	     ! get_number_from_entry (spiny1_entry, plot_dialog, &y1) ||
	     ! get_number_from_entry (spiny2_entry, plot_dialog, &y2) ) {
		goto whack_copied_funcs;
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
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_x_name);
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_y_name);
		goto whack_copied_funcs;
	}

	if ( ! get_number_from_entry (spint1_entry, plot_dialog, &plott1) ||
	     ! get_number_from_entry (spint2_entry, plot_dialog, &plott2) ||
	     ! get_number_from_entry (spintinc_entry, plot_dialog, &plottinc) ) {
		goto whack_copied_funcs;
	}

	reset_plotx1 = plotx1 = x1;
	reset_plotx2 = plotx2 = x2;
	reset_ploty1 = ploty1 = y1;
	reset_ploty2 = ploty2 = y2;


	if (plott1 >= plott2 ||
	    plottinc <= 0.0) {
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_t_name);
		goto whack_copied_funcs;
	}


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

	plot_functions (TRUE /* do_window_present */,
			TRUE /* from_gui */,
			lineplot_fit_dependent_axis_cb /*fit*/);

	if (lineplot_fit_dependent_axis_cb) {
		reset_plotx1 = plotx1;
		reset_plotx2 = plotx2;
		reset_ploty1 = ploty1;
		reset_ploty2 = ploty2;
	}

	if (gel_interrupted)
		gel_interrupted = FALSE;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	return;

whack_copied_funcs:
	d_freefunc (funcpx);
	funcpx = NULL;
	d_freefunc (funcpy);
	funcpy = NULL;
	d_freefunc (funcpz);
	funcpz = NULL;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	if (error_to_print != NULL) {
		genius_display_error (plot_dialog, error_to_print);
		g_free (error_to_print);
	}
}

static void
plot_from_dialog_slopefield (void)
{
	GelEFunc *funcp = NULL;
	double x1, x2, y1, y2;
	char *error_to_print = NULL;
	gboolean ex = FALSE;
	long last_errnum;

	plot_mode = MODE_LINEPLOT_SLOPEFIELD;

	last_errnum = total_errors;

	init_var_names ();

	ex = FALSE;
	funcp = get_func_from_entry2 (slopefield_entry, slopefield_status, 
				      lp_x_name,
				      lp_y_name,
				      lp_z_name,
				      &ex);

	if (funcp == NULL) {
		error_to_print = g_strdup(_("No functions to plot or no functions "
					    "could be parsed"));
		goto whack_copied_funcs;
	}

	if ( ! get_number_from_entry (spinx1_entry, plot_dialog, &x1) ||
	     ! get_number_from_entry (spinx2_entry, plot_dialog, &x2) ||
	     ! get_number_from_entry (spiny1_entry, plot_dialog, &y1) ||
	     ! get_number_from_entry (spiny2_entry, plot_dialog, &y2) ) {
		goto whack_copied_funcs;
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
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_x_name);
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_y_name);
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

	plot_functions (TRUE /* do_window_present */,
			TRUE /* from_gui */,
			FALSE /*fit*/);

	if (gel_interrupted)
		gel_interrupted = FALSE;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	return;

whack_copied_funcs:
	d_freefunc (funcp);
	funcp = NULL;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	if (error_to_print != NULL) {
		genius_display_error (plot_dialog, error_to_print);
		g_free (error_to_print);
	}
}

static void
plot_from_dialog_vectorfield (void)
{
	GelEFunc *funcpx = NULL;
	GelEFunc *funcpy = NULL;
	double x1, x2, y1, y2;
	char *error_to_print = NULL;
	gboolean ex = FALSE;
	long last_errnum;

	plot_mode = MODE_LINEPLOT_VECTORFIELD;

	last_errnum = total_errors;

	vectorfield_normalize_arrow_length = 
		vectorfield_normalize_arrow_length_cb;

	ex = FALSE;
	funcpx = get_func_from_entry2 (vectorfield_entry_x,
				       vectorfield_status_x, 
				       lp_x_name, lp_y_name, lp_z_name, &ex);
	ex = FALSE;
	funcpy = get_func_from_entry2 (vectorfield_entry_y,
				       vectorfield_status_y,
				       lp_x_name, lp_y_name, lp_z_name, &ex);

	if (funcpx == NULL || funcpy == NULL) {
		error_to_print = g_strdup (_("No functions to plot or no functions "
					     "could be parsed"));
		goto whack_copied_funcs;
	}

	if ( ! get_number_from_entry (spinx1_entry, plot_dialog, &x1) ||
	     ! get_number_from_entry (spinx2_entry, plot_dialog, &x2) ||
	     ! get_number_from_entry (spiny1_entry, plot_dialog, &y1) ||
	     ! get_number_from_entry (spiny2_entry, plot_dialog, &y2) ) {
		goto whack_copied_funcs;
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
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_x_name);
		goto whack_copied_funcs;
	}

	if (y1 == y2) {
		error_to_print = g_strdup_printf (_("Invalid %s range"),
						  lp_y_name);
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

	plot_functions (TRUE /* do_window_present */,
			TRUE /* from_gui */,
			FALSE /*fit*/);

	if (gel_interrupted)
		gel_interrupted = FALSE;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	return;

whack_copied_funcs:
	d_freefunc (funcpx);
	funcpx = NULL;
	d_freefunc (funcpy);
	funcpy = NULL;

	gel_printout_infos_parent (graph_window);
	if (last_errnum != total_errors &&
	    ! genius_setup.error_box) {
		gtk_widget_show (errors_label_box);
	}

	if (error_to_print != NULL) {
		genius_display_error (plot_dialog, error_to_print);
		g_free (error_to_print);
	}
}

static void
plot_from_dialog (void)
{
	int function_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (function_notebook));

	lineplot_draw_legends = lineplot_draw_legends_cb;
	lineplot_draw_labels = lineplot_draw_labels_cb;

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
		update_spinboxes (w);
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
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Close"),
		 GTK_RESPONSE_CLOSE,
		 _("_Plot"),
		 RESPONSE_PLOT,
		 NULL);
	gtk_window_set_type_hint (GTK_WINDOW (plot_dialog),
				  GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_dialog_set_default_response (GTK_DIALOG (plot_dialog),
					 RESPONSE_PLOT);

	g_signal_connect (G_OBJECT (plot_dialog),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &plot_dialog);
	g_signal_connect (G_OBJECT (plot_dialog),
			  "response",
			  G_CALLBACK (plot_dialog_response),
			  NULL);

	insides = create_plot_dialog ();

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (plot_dialog))),
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
	gboolean fitz = FALSE;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SurfacePlot", "SurfacePlot");
		return NULL;
	}

	i = 0;

	if (a[i] != NULL && a[i]->type != GEL_FUNCTION_NODE) {
		gel_errorout (_("%s: argument not a function"), "SurfacePlot");
		goto whack_copied_funcs;
	}

	func = d_copyfunc (a[i]->func.func);
	func->context = -1;

	i++;

	if (a[i] != NULL && a[i]->type == GEL_FUNCTION_NODE) {
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
		if (a[i]->type == GEL_MATRIX_NODE) {
			if (gel_matrixw_elements (a[i]->mat.matrix) == 6) {
				if ( ! get_limits_from_matrix_surf (a[i], &x1, &x2, &y1, &y2, &z1, &z2))
					goto whack_copied_funcs;
				fitz = FALSE;
			} else if (gel_matrixw_elements (a[i]->mat.matrix) == 4) {
				if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
					goto whack_copied_funcs;
				fitz = TRUE;
			} else {
				gel_errorout (_("Graph limits not given as a 4-vector or a 6-vector"));
				goto whack_copied_funcs;
			}
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
						fitz = TRUE;
						if (a[i] != NULL) {
							GET_DOUBLE(z1, i, "SurfacePlot");
							i++;
							fitz = FALSE;
							if (a[i] != NULL) {
								GET_DOUBLE(z2, i, "SurfacePlot");
								i++;
							}
						}
					}
				}
			}
			/* FIXME: what about errors */
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
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

	/* don't plot from data */
	if (surface_data_x != NULL) {
		g_free (surface_data_x);
		surface_data_x = NULL;
	}
	if (surface_data_y != NULL) {
		g_free (surface_data_y);
		surface_data_y = NULL;
	}
	if (surface_data_z != NULL) {
		g_free (surface_data_z);
		surface_data_z = NULL;
	}
	/* just in case */
	if (surface_data != NULL) {
		gtk_plot_data_set_x (GTK_PLOT_DATA (surface_data), NULL);
		gtk_plot_data_set_y (GTK_PLOT_DATA (surface_data), NULL);
		gtk_plot_data_set_z (GTK_PLOT_DATA (surface_data), NULL);
	}

	reset_surfacex1 = surfacex1 = x1;
	reset_surfacex2 = surfacex2 = x2;
	reset_surfacey1 = surfacey1 = y1;
	reset_surfacey2 = surfacey2 = y2;
	reset_surfacez1 = surfacez1 = z1;
	reset_surfacez2 = surfacez2 = z2;

	plot_mode = MODE_SURFACE;
	plot_surface_functions (FALSE /* do_window_present */,
				fitz /* fit */);

	if (fitz) {
		reset_surfacez1 = surfacez1;
		reset_surfacez2 = surfacez2;
	}

	if (gel_interrupted)
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

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SlopefieldDrawSolution", "SlopefieldDrawSolution");
		return NULL;
	}

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

	slopefield_draw_solution (x, y, dx, FALSE /*is_gui*/);

	return gel_makenum_null ();
}

static GelETree *
SlopefieldClearSolutions_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SlopefieldClearSolutions", "SlopefieldClearSolutions");
		return NULL;
	}
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

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "VectorfieldDrawSolution", "VectorfieldDrawSolution");
		return NULL;
	}

	GET_DOUBLE (x, 0, "VectorfieldDrawSolution");
	GET_DOUBLE (y, 1, "VectorfieldDrawSolution");
	GET_DOUBLE (dt, 2, "VectorfieldDrawSolution");
	GET_DOUBLE (tlen, 3, "VectorfieldDrawSolution");

	if G_UNLIKELY (dt <= 0.0) {
		gel_errorout (_("%s: dt must be positive"),
			      "VectorfieldDrawSolution");
		return NULL;
	}

	if G_UNLIKELY (tlen <= 0.0) {
		gel_errorout (_("%s: tlen must be positive"),
			      "VectorfieldDrawSolution");
		return NULL;
	}

	if G_UNLIKELY (plot_mode != MODE_LINEPLOT_VECTORFIELD ||
		       vectorfield_func_x == NULL ||
		       vectorfield_func_y == NULL) {
		gel_errorout (_("%s: Vector field not active"),
			      "VectorfieldDrawSolution");
		return NULL;
	}

	vectorfield_draw_solution (x, y, dt, tlen, FALSE /*is_gui*/);

	return gel_makenum_null ();
}


static GelETree *
VectorfieldClearSolutions_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "VectorfieldClearSolutions", "VectorfieldClearSolutions");
		return NULL;
	}

	if G_UNLIKELY (plot_mode != MODE_LINEPLOT_VECTORFIELD) {
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

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SlopefieldPlot", "SlopefieldPlot");
		return NULL;
	}

	if G_UNLIKELY (a[0] == NULL ||
		       a[0]->type != GEL_FUNCTION_NODE) {
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
		if (a[i]->type == GEL_MATRIX_NODE) {
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
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
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

	plotVtick = plot_sf_Vtick;
	plotHtick = plot_sf_Htick;

	plot_mode = MODE_LINEPLOT_SLOPEFIELD;
	plot_functions (FALSE /* do_window_present */,
			FALSE /* from gui */,
			FALSE /*fit*/);

	if (gel_interrupted)
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

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "VectorfieldPlot", "VectorfieldPlot");
		return NULL;
	}

	/* FIXME: also accept just one function and then treat it as complex
	 * valued */

	if G_UNLIKELY (a[0] == NULL || a[1] == NULL ||
		       a[0]->type != GEL_FUNCTION_NODE ||
		       a[1]->type != GEL_FUNCTION_NODE) {
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
		if (a[i]->type == GEL_MATRIX_NODE) {
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
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
				goto whack_copied_funcs;
			}
		}
	}

	if G_UNLIKELY (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if G_UNLIKELY (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if G_UNLIKELY (x1 == x2) {
		gel_errorout (_("%s: invalid X range"), "VectorfieldPlot");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (y1 == y2) {
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

	vectorfield_normalize_arrow_length =
		vectorfield_normalize_arrow_length_parameter;

	plotVtick = plot_vf_Vtick;
	plotHtick = plot_vf_Htick;

	plot_mode = MODE_LINEPLOT_VECTORFIELD;
	plot_functions (FALSE /* do_window_present */,
			FALSE /* from_gui */,
			FALSE /* fit */);

	if (gel_interrupted)
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
	gboolean fity = FALSE;
	GelEFunc *func[MAXFUNC] = { NULL };
	int i;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "LinePlot", "LinePlot");
		return NULL;
	}

	for (i = 0;
	     i < MAXFUNC && a[i] != NULL && a[i]->type == GEL_FUNCTION_NODE;
	     i++) {
		func[funcs] = d_copyfunc (a[i]->func.func);
		func[funcs]->context = -1;
		funcs++;
	}

	if G_UNLIKELY (a[i] != NULL && a[i]->type == GEL_FUNCTION_NODE) {
		gel_errorout (_("%s: only up to 10 functions supported"), "LinePlot");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (funcs == 0) {
		gel_errorout (_("%s: argument not a function"), "LinePlot");
		goto whack_copied_funcs;
	}

	/* Defaults */
	x1 = defx1;
	x2 = defx2;
	y1 = defy1;
	y2 = defy2;

	if (a[i] != NULL) {
		if (a[i]->type == GEL_MATRIX_NODE) {
			if (gel_matrixw_elements (a[i]->mat.matrix) == 4) {
				if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
					goto whack_copied_funcs;
				fity = FALSE;
			} else if (gel_matrixw_elements (a[i]->mat.matrix) == 2) {
				if ( ! get_limits_from_matrix_xonly (a[i], &x1, &x2))
					goto whack_copied_funcs;
				fity = TRUE;
			} else {
				gel_errorout (_("Graph limits not given as a 2-vector or a 4-vector"));
				goto whack_copied_funcs;
			}
			i++;
		} else {
			GET_DOUBLE(x1, i, "LinePlot");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "LinePlot");
				i++;
				fity = TRUE;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "LinePlot");
					i++;
					fity = FALSE;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "LinePlot");
						i++;
					}
				}
			}
			/* FIXME: what about errors */
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
				goto whack_copied_funcs;
			}
		}
	}

	if G_UNLIKELY (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if G_UNLIKELY (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if G_UNLIKELY (x1 == x2) {
		gel_errorout (_("%s: invalid X range"), "LinePlot");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (y1 == y2) {
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

	plot_mode = MODE_LINEPLOT;
	plot_functions (FALSE /* do_window_present */,
			FALSE /* from_gui */,
			fity /* fit */);

	if (fity) {
		reset_ploty1 = ploty1;
		reset_ploty2 = ploty2;
	}

	if G_UNLIKELY (gel_interrupted)
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
	gboolean fit = FALSE;
	GelEFunc *funcx = NULL;
	GelEFunc *funcy = NULL;
	int i;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "LinePlotParametric", "LinePlotParametric");
		return NULL;
	}

	if G_UNLIKELY (a[0] == NULL || a[1] == NULL ||
		       a[0]->type != GEL_FUNCTION_NODE ||
		       a[1]->type != GEL_FUNCTION_NODE) {
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
		if G_UNLIKELY (gel_error_num != 0) {
			gel_error_num = 0;
			goto whack_copied_funcs;
		}
	}

	/* Get window limits */
	if (a[i] != NULL) {
		if ( (a[i]->type == GEL_STRING_NODE &&
		      strcasecmp (a[i]->str.str, "fit") == 0) ||
		     (a[i]->type == GEL_IDENTIFIER_NODE &&
		      strcasecmp (a[i]->id.id->token, "fit") == 0)) {
			fit = TRUE;
		} else if (a[i]->type == GEL_MATRIX_NODE) {
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
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
				goto whack_copied_funcs;
			}
		}
	}

	if G_UNLIKELY (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if G_UNLIKELY (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if G_UNLIKELY (x1 == x2) {
		gel_errorout (_("%s: invalid X range"), "LinePlotParametric");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "LinePlotParametric");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (t1 >= t2 || tinc <= 0) {
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

	plot_mode = MODE_LINEPLOT_PARAMETRIC;
	plot_functions (FALSE /* do_window_present */,
			FALSE /* from_gui */,
			fit /* fit */);

	if (fit) {
		reset_plotx1 = plotx1;
		reset_plotx2 = plotx2;
		reset_ploty1 = ploty1;
		reset_ploty2 = ploty2;
	}

	if G_UNLIKELY (gel_interrupted)
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
	gboolean fit = FALSE;
	GelEFunc *func = NULL;
	int i;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "LinePlotCParametric", "LinePlotCParametric");
		return NULL;
	}

	if G_UNLIKELY (a[0] == NULL ||
		       a[0]->type != GEL_FUNCTION_NODE) {
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
		if G_UNLIKELY (gel_error_num != 0) {
			gel_error_num = 0;
			goto whack_copied_funcs;
		}
	}

	/* Get window limits */
	if (a[i] != NULL) {
		if ( (a[i]->type == GEL_STRING_NODE &&
		      strcasecmp (a[i]->str.str, "fit") == 0) ||
		     (a[i]->type == GEL_IDENTIFIER_NODE &&
		      strcasecmp (a[i]->id.id->token, "fit") == 0)) {
			fit = TRUE;
		} else if (a[i]->type == GEL_MATRIX_NODE) {
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
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
				goto whack_copied_funcs;
			}
		}
	}

	if G_UNLIKELY (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if G_UNLIKELY (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if G_UNLIKELY (x1 == x2) {
		gel_errorout (_("%s: invalid X range"), "LinePlotCParametric");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "LinePlotCParametric");
		goto whack_copied_funcs;
	}

	if G_UNLIKELY (t1 >= t2 || tinc <= 0) {
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

	plot_mode = MODE_LINEPLOT_PARAMETRIC;
	plot_functions (FALSE /* do_window_present */,
			FALSE /* from_gui */,
			fit /* fit */);

	if (fit) {
		reset_plotx1 = plotx1;
		reset_plotx2 = plotx2;
		reset_ploty1 = ploty1;
		reset_ploty2 = ploty2;
	}

	if G_UNLIKELY (gel_interrupted)
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
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "LinePlotClear", "LinePlotClear");
		return NULL;
	}

	line_plot_clear_funcs ();

	/* This will just clear the window */
	plot_mode = MODE_LINEPLOT;
	plot_functions (FALSE /* do_window_present */,
			FALSE /* from_gui */,
			FALSE /* fit */);

	if G_UNLIKELY (gel_interrupted)
		return NULL;
	else
		return gel_makenum_null ();
}

static GelETree *
SurfacePlotClear_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SurfacePlotClear", "SurfacePlotClear");
		return NULL;
	}
  
	  
	if (surface_func != NULL) {
		d_freefunc (surface_func);
		surface_func = NULL;
	}
	g_free (surface_func_name);
	surface_func_name = NULL;

	/* don't plot from data */
	if (surface_data_x != NULL) {
		g_free (surface_data_x);
		surface_data_x = NULL;
	}
	if (surface_data_y != NULL) {
		g_free (surface_data_y);
		surface_data_y = NULL;
	}
	if (surface_data_z != NULL) {
		g_free (surface_data_z);
		surface_data_z = NULL;
	}
	if (surface_data != NULL) {
		gtk_plot_data_set_x (GTK_PLOT_DATA (surface_data), NULL);
		gtk_plot_data_set_y (GTK_PLOT_DATA (surface_data), NULL);
		gtk_plot_data_set_z (GTK_PLOT_DATA (surface_data), NULL);
	}

	plot_mode = MODE_SURFACE;
	plot_surface_functions (TRUE /* do_window_present */,
				FALSE /*fit*/);

	if G_UNLIKELY (gel_interrupted)
		return NULL;
	else
		return gel_makenum_null ();
}

static int plot_canvas_freeze_count = 0;

static void
plot_freeze (void) {
	if (plot_canvas_freeze_count == 0) {
		if (plot_canvas != NULL /* sanity */)
			gtk_plot_canvas_freeze (GTK_PLOT_CANVAS (plot_canvas));
	}

	plot_canvas_freeze_count ++;

}

static GelETree *
PlotCanvasFreeze_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	plot_freeze ();

	return gel_makenum_null ();
}

static void
plot_thaw (void)
{
	if (plot_canvas_freeze_count > 0) {
		plot_canvas_freeze_count --;
		if (plot_canvas_freeze_count == 0) {
			if (plot_canvas != NULL /* sanity */) {
				plot_in_progress ++;
				gel_calc_running ++;
				gtk_plot_canvas_thaw (GTK_PLOT_CANVAS (plot_canvas));
				gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
				gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
				plot_in_progress --;
				gel_calc_running --;
				plot_window_setup ();

				if (gel_evalnode_hook != NULL)
					(*gel_evalnode_hook)();
			}
		}
	}
}

static GelETree *
PlotCanvasThaw_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	plot_thaw ();

	return gel_makenum_null ();
}

static GelETree *
PlotWindowPresent_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	ensure_window (TRUE /* present */);

	return gel_makenum_null ();
}

static GelETree *
LinePlotWaitForClick_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	GelMatrixW *m;

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

	wait_for_click = TRUE;

	while (wait_for_click) {
		while (gtk_events_pending ())
			gtk_main_iteration ();
		if (gel_interrupted) {
			wait_for_click = FALSE;
			return NULL;
		}
		if (line_plot == NULL) {
			wait_for_click = FALSE;
			return gel_makenum_null ();
		}
	}

	/*make us a new empty node*/
	GEL_GET_NEW_NODE (n);
	n->type = GEL_MATRIX_NODE;
	m = n->mat.matrix = gel_matrixw_new ();
	n->mat.quoted = FALSE;
	gel_matrixw_set_size (m, 2, 1);

	gel_matrixw_set_indexii (m, 0) = gel_makenum_d (click_x);
	gel_matrixw_set_index (m, 1, 0) = gel_makenum_d (click_y);

	return n;
}

static GelETree *
LinePlotMouseLocation_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (line_plot != NULL) {
		GdkSeat *s;
		GelETree *n;
		GelMatrixW *m;
		int xx, yy;
		double x, y;

	        s = gdk_display_get_default_seat (gdk_display_get_default ());
	        gdk_window_get_device_position (gtk_widget_get_window
	                                        (GTK_WIDGET (line_plot)),
	                                        gdk_seat_get_pointer (s),
	                                        &xx, &yy, NULL);
		gtk_plot_get_point (GTK_PLOT (line_plot), xx, yy, &x, &y);

		/*make us a new empty node*/
		GEL_GET_NEW_NODE (n);
		n->type = GEL_MATRIX_NODE;
		m = n->mat.matrix = gel_matrixw_new ();
		n->mat.quoted = FALSE;
		gel_matrixw_set_size (m, 2, 1);

		gel_matrixw_set_indexii (m, 0) = gel_makenum_d (x);
		gel_matrixw_set_index (m, 1, 0) = gel_makenum_d (y);

		return n;
	} else {
		gel_errorout (_("%s: Not in line plot mode.  Perhaps run LinePlot or LinePlotClear first."),
			      "LinePlotMouseLocation");
		return gel_makenum_null ();
	}
}

void
gel_plot_canvas_thaw_completely (void)
{
	if (plot_canvas_freeze_count > 0) {
		plot_canvas_freeze_count = 0;
		if (plot_canvas != NULL /* sanity */) {
			plot_in_progress ++;
			gel_calc_running ++;
			gtk_plot_canvas_thaw (GTK_PLOT_CANVAS (plot_canvas));
			gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
			gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
			plot_in_progress --;
			gel_calc_running --;

			/* a hook will get called eventually, no need to call it now,
			 * this only gets called once we're about to be idle and get out of
			 * computation */
		}
	}
	
	/*always do setup */
	plot_window_setup ();
}

static gboolean
update_lineplot_window (double x1, double x2, double y1, double y2)
{
	if G_UNLIKELY (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if G_UNLIKELY (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	reset_plotx1 = defx1 = x1;
	reset_plotx2 = defx2 = x2;
	reset_ploty1 = defy1 = y1;
	reset_ploty2 = defy2 = y2;
	
	if (plotx1 == x1 &&
	    plotx2 == x2 &&
	    ploty1 == y1 &&
	    ploty2 == y2)
		return FALSE;

	plotx1 = x1;
	plotx2 = x2;
	ploty1 = y1;
	ploty2 = y2;

	return TRUE;
}

static gboolean
update_surfaceplot_window (double x1, double x2, double y1, double y2, double z1, double z2)
{
	gboolean update = FALSE;

	if G_UNLIKELY (x1 > x2) {
		double s = x1;
		x1 = x2;
		x2 = s;
	}

	if G_UNLIKELY (y1 > y2) {
		double s = y1;
		y1 = y2;
		y2 = s;
	}

	if G_UNLIKELY (z1 > z2) {
		double s = z1;
		z1 = z2;
		z2 = s;
	}


	if (surfacex1 != x1 ||
	    surfacex2 != x2 ||
	    surfacey1 != y1 ||
	    surfacey2 != y2 ||
	    surfacez1 != z1 ||
	    surfacez2 != z2)
		update = TRUE;

	reset_surfacex1 = surfacex1 = surf_defx1 = x1;
	reset_surfacex2 = surfacex2 = surf_defx2 = x2;
	reset_surfacey1 = surfacey1 = surf_defy1 = y1;
	reset_surfacey2 = surfacey2 = surf_defy2 = y2;
	reset_surfacez1 = surfacez1 = surf_defz1 = z1;
	reset_surfacez2 = surfacez2 = surf_defz2 = z2;

	return update;
}


static gboolean
get_line_numbers (GelETree *a, double **x, double **y, int *len,
		  double *minx, double *maxx, double *miny, double *maxy,
		  const char *funcname, int minn)
{
	int i;
	GelMatrixW *m;
	gboolean nominmax = TRUE;
#define UPDATE_MINMAX \
	if (minx != NULL) { \
		if (xx > *maxx || nominmax) *maxx = xx; \
		if (xx < *minx || nominmax) *minx = xx; \
		if (yy > *maxy || nominmax) *maxy = yy; \
		if (yy < *miny || nominmax) *miny = yy; \
		nominmax = FALSE; \
	}

	g_return_val_if_fail (a->type == GEL_MATRIX_NODE, FALSE);

	m = a->mat.matrix;

	if G_UNLIKELY ( ! gel_is_matrix_value_only (m)) {
		gel_errorout (_("%s: Points should be given as a real, n by 2 matrix "
				"with columns for x and y, n>=%d, or as a complex "
				"valued n by 1 matrix"),
			      funcname, minn);
		return FALSE;
	}

	if (gel_matrixw_width (m) == 2 &&
	    gel_matrixw_height (m) >= minn) {
		if G_UNLIKELY ( ! gel_is_matrix_value_only_real (m)) {
			gel_errorout (_("%s: If points are given as an n by 2 matrix, then this matrix must be real"),
				      funcname);
			return FALSE;
		}

		*len = gel_matrixw_height (m);


		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy;
			GelETree *t = gel_matrixw_index (m, 0, i);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 1, i);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else if (gel_matrixw_width (m) == 1 &&
		   gel_matrixw_height (m) >= minn) {
		*len = gel_matrixw_height (m);


		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy;
			GelETree *t = gel_matrixw_index (m, 0, i);
			mpw_get_complex_double (t->val.value, &xx, &yy);
			(*x)[i] = xx;
			(*y)[i] = yy;
			UPDATE_MINMAX
		}
		/*
		 * This was undocumented and conflicts with using complex numbers
	} else if (gel_matrixw_width (m) == 1 &&
		   gel_matrixw_height (m) % 2 == 0 &&
		   gel_matrixw_height (m) >= 2*minn) {
		*len = gel_matrixw_height (m) / 2;

		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy;
			GelETree *t = gel_matrixw_index (m, 0, 2*i);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 0, (2*i) + 1);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else if (gel_matrixw_height (m) == 1 &&
		   gel_matrixw_width (m) % 2 == 0 &&
		   gel_matrixw_width (m) >= 2*minn) {
		*len = gel_matrixw_width (m) / 2;

		*x = g_new (double, *len);
		*y = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy;
			GelETree *t = gel_matrixw_index (m, 2*i, 0);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, (2*i) + 1, 0);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
		*/
	} else {
		gel_errorout (_("%s: Points should be given as a real, n by 2 matrix "
				"with columns for x and y, n>=%d, or as a complex "
				"valued n by 1 matrix"),
			      funcname, minn);
		return FALSE;
	}

	return TRUE;
#undef UPDATE_MINMAX
}


static gboolean
get_surface_line_numbers (GelETree *a,
			  double **x, double **y, double **z, int *len,
			  double *minx, double *maxx,
			  double *miny, double *maxy,
			  double *minz, double *maxz,
			  const char *funcname, int minn)
{
	int i;
	GelMatrixW *m;
	gboolean nominmax = TRUE;
#define UPDATE_MINMAX \
	if (minx != NULL) { \
		if (xx > *maxx || nominmax) *maxx = xx; \
		if (xx < *minx || nominmax) *minx = xx; \
		if (yy > *maxy || nominmax) *maxy = yy; \
		if (yy < *miny || nominmax) *miny = yy; \
		if (zz > *maxz || nominmax) *maxz = zz; \
		if (zz < *minz || nominmax) *minz = zz; \
		nominmax = FALSE; \
	}

	g_return_val_if_fail (a->type == GEL_MATRIX_NODE, FALSE);

	m = a->mat.matrix;

	if G_UNLIKELY ( ! gel_is_matrix_value_only_real (m)) {
		gel_errorout (_("%s: Points should be given as a real, n by 3 matrix "
				"with columns for x, y and z, n>=%d"),
			      funcname, minn);
		return FALSE;
	}

	if (gel_matrixw_width (m) == 3 &&
	    gel_matrixw_height (m) >= minn) {
		*len = gel_matrixw_height (m);

		*x = g_new (double, *len);
		*y = g_new (double, *len);
		*z = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy, zz;
			GelETree *t = gel_matrixw_index (m, 0, i);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 1, i);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 2, i);
			(*z)[i] = zz = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else if (gel_matrixw_width (m) == 1 &&
		   gel_matrixw_height (m) % 3 == 0 &&
		   gel_matrixw_height (m) >= 3*minn) {
		*len = gel_matrixw_height (m) / 3;

		*x = g_new (double, *len);
		*y = g_new (double, *len);
		*z = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy, zz;
			GelETree *t = gel_matrixw_index (m, 0, 3*i);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 0, (3*i) + 1);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 0, (3*i) + 2);
			(*z)[i] = zz = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else if (gel_matrixw_height (m) == 1 &&
		   gel_matrixw_width (m) % 3 == 0 &&
		   gel_matrixw_width (m) >= 3*minn) {
		*len = gel_matrixw_width (m) / 3;

		*x = g_new (double, *len);
		*y = g_new (double, *len);
		*z = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy, zz;
			GelETree *t = gel_matrixw_index (m, 3*i, 0);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, (3*i) + 1, 0);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, (3*i) + 2, 0);
			(*z)[i] = zz = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else {
		gel_errorout (_("%s: Points should be given as a real, n by 3 matrix "
				"with columns for x, y and z, n>=%d"),
			      funcname, minn);
		return FALSE;
	}

	return TRUE;
#undef UPDATE_MINMAX
}

static void
draw_arrowhead (double xx1, double yy1, double xx2, double yy2,
		int thickness, GdkRGBA *color)
{
	double x1, x2, y1, y2, xm, ym;
	double *ax, *ay;
	double angle;

	/* nowhere to go */
	if (xx1 == xx2 && yy1 == yy2)
		return;


	gtk_plot_get_pixel (GTK_PLOT (line_plot), xx1, yy1, &x1, &y1);
	gtk_plot_get_pixel (GTK_PLOT (line_plot), xx2, yy2, &x2, &y2);

	angle = atan2 ( (y2-y1) , (x2-x1) );

	ax = g_new (double, 3);
	ay = g_new (double, 3);
	ax[1] = xx2;
	ay[1] = yy2;

	xm = x2 - cos(angle) * /*al*/ 5* thickness;
	ym = y2 - sin(angle) * /*al*/ 5* thickness;
	gtk_plot_get_point (GTK_PLOT (line_plot),
			    xm - sin(angle)* /*aw*/5* thickness / 2.0,
			    ym + cos(angle)* /*aw*/5* thickness / 2.0,
			    & (ax[0]), & (ay[0]));
	gtk_plot_get_point (GTK_PLOT (line_plot),
			    xm + sin(angle)* /*aw*/5* thickness / 2.0,
			    ym - cos(angle)* /*aw*/5* thickness / 2.0,
			    & (ax[2]), & (ay[2]));

	draw_line (ax, ay, 3, thickness, color, NULL /* legend */,
		   FALSE /* filled */);
} 

static gboolean
get_color (GelETree *a, GdkRGBA *c, const char *funcname)
{
	if (a == NULL) {
		gel_errorout (_("%s: No color specified"),
			      funcname);
		return FALSE;
	} else if (a->type == GEL_STRING_NODE) {
		if ( ! gdk_rgba_parse (c, a->str.str)) {
			gel_errorout (_("%s: Cannot parse color '%s'"),
				      funcname, a->str.str);
			return FALSE;
		}
		return TRUE;
	} else if (a->type == GEL_IDENTIFIER_NODE) {
		if ( ! gdk_rgba_parse (c, a->id.id->token)) {
			gel_errorout (_("%s: Cannot parse color '%s'"),
				      funcname, a->id.id->token);
			return FALSE;
		}
		return TRUE;
	} else if (a->type == GEL_MATRIX_NODE) {
		GelMatrixW *m = a->mat.matrix;
		GelETree *t;
		double r;
		double g;
		double b;

		if G_UNLIKELY ( ! gel_is_matrix_value_only_real (m) ||
			       gel_matrixw_elements(m) != 3) {
			gel_errorout (_("%s: A vector giving color should be a 3-vector of real numbers between 0 and 1"),
				      funcname);
			return FALSE;
		}
		/* we know we have 3 values, so we always get non-null t here that's a value node */
		t = gel_matrixw_vindex (m, 0);
		r = mpw_get_double (t->val.value);
		t = gel_matrixw_vindex (m, 1);
		g = mpw_get_double (t->val.value);
		t = gel_matrixw_vindex (m, 2);
		b = mpw_get_double (t->val.value);

#define FUDGE 0.000001
		if G_UNLIKELY ( r < -FUDGE || r > (1+FUDGE) ||
				g < -FUDGE || g > (1+FUDGE) ||
				b < -FUDGE || b > (1+FUDGE) ) {
			gel_errorout (_("%s: Warning: Values for red, green, or blue out of range (0 to 1), I will clip them to this interval"),
				      funcname);
		}
#undef FUDGE
		r = MAX(MIN(r,1.0),0.0);
		g = MAX(MIN(g,1.0),0.0);
		b = MAX(MIN(b,1.0),0.0);

		c->red = r;
		c->green = g;
		c->blue = b;
		c->alpha = 1.0;

		return TRUE;
	}


	gel_errorout (_("%s: Color must be a string or a three-vector of rgb values (between 0 and 1)"),
		      funcname);

	return FALSE;
}


static GelETree *
LinePlotDrawLine_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int len;
	int nextarg;
	double *x, *y;
	double minx = 0, miny = 0, maxx = 0, maxy = 0;
	GdkRGBA color;
	int thickness;
	gboolean arrow_origin = FALSE;
	gboolean arrow_end = FALSE;
	gboolean filled = FALSE;
	int i;
	gboolean update = FALSE;
	char *legend = NULL;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "LinePlotDrawLine", "LinePlotDrawLine");
		return NULL;
	}

	ensure_window (FALSE /* do_window_present */);

	if (a[0]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	} else if (a[0]->type == GEL_MATRIX_NODE) {
		if G_UNLIKELY ( ! get_line_numbers (a[0], &x, &y, &len,
						    &minx, &maxx, &miny, &maxy,
						    "LinePlotDrawLine",
						    2))
			return NULL;
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

		minx = MIN(x1,x2);
		maxx = MAX(x1,x2);
		miny = MIN(y1,y2);
		maxy = MAX(y1,y2);
	}

	gdk_rgba_parse (&color, "black");
	thickness = 2;

	for (i = nextarg; a[i] != NULL; i++) {
		if G_LIKELY (a[i]->type == GEL_STRING_NODE ||
			     a[i]->type == GEL_IDENTIFIER_NODE) {
			GelToken *id;
			static GelToken *colorid = NULL;
			static GelToken *thicknessid = NULL;
			static GelToken *windowid = NULL;
			static GelToken *fitid = NULL;
			static GelToken *arrowid = NULL;
			static GelToken *originid = NULL;
			static GelToken *endid = NULL;
			static GelToken *bothid = NULL;
			static GelToken *noneid = NULL;
			static GelToken *legendid = NULL;
			static GelToken *filledid = NULL;

			if (colorid == NULL) {
				colorid = d_intern ("color");
				thicknessid = d_intern ("thickness");
				windowid = d_intern ("window");
				fitid = d_intern ("fit");
				arrowid = d_intern ("arrow");
				originid = d_intern ("origin");
				endid = d_intern ("end");
				bothid = d_intern ("both");
				noneid = d_intern ("none");
				legendid = d_intern ("legend");
				filledid = d_intern ("filled");
			}

			if (a[i]->type == GEL_STRING_NODE)
				id = d_intern (a[i]->str.str);
			else
				id = a[i]->id.id;
			if (id == colorid) {
			        if G_UNLIKELY ( ! get_color (a[i+1], &color, "LinePlotDrawLine")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == thicknessid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No thickness specified"),
						      "LinePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if G_UNLIKELY ( ! check_argument_positive_integer (a, i+1,
										   "LinePlotDrawLine")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				thickness = gel_get_nonnegative_integer (a[i+1]->val.value,
									 "LinePlotDrawLine");
				i++;
			} else if (id == windowid) {
				double x1, x2, y1, y2;
				if G_UNLIKELY (a[i+1] == NULL ||
					       (a[i+1]->type != GEL_STRING_NODE &&
						a[i+1]->type != GEL_IDENTIFIER_NODE &&
						a[i+1]->type != GEL_MATRIX_NODE)) {
					gel_errorout (_("%s: No window specified"),
						      "LinePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if ((a[i+1]->type == GEL_STRING_NODE &&
				     fitid == d_intern (a[i+1]->str.str)) ||
				    (a[i+1]->type == GEL_IDENTIFIER_NODE &&
				     fitid == a[i+1]->id.id)) {
					x1 = minx;
					x2 = maxx;
					y1 = miny;
					y2 = maxy;
					if G_UNLIKELY (x1 == x2) {
						x1 -= 0.1;
						x2 += 0.1;
					}

					/* assume line is a graph so x fits tightly */

					if G_UNLIKELY (y1 == y2) {
						y1 -= 0.1;
						y2 += 0.1;
					} else {
						/* Make window 5% larger on each vertical side */
						double height = (y2-y1);
						y1 -= height * 0.05;
						y2 += height * 0.05;
					}

					update = update_lineplot_window (x1, x2, y1, y2);
				} else if (get_limits_from_matrix (a[i+1], &x1, &x2, &y1, &y2)) {
					update = update_lineplot_window (x1, x2, y1, y2);
				} else {
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == arrowid) {
				GelToken *astyleid;

				if G_UNLIKELY (a[i+1] == NULL ||
					       (a[i+1]->type != GEL_STRING_NODE &&
						a[i+1]->type != GEL_IDENTIFIER_NODE)) {
					gel_errorout (_("%s: arrow style should be \"origin\", \"end\", \"both\", or \"none\""),
						      "LinePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if (a[i+1]->type == GEL_STRING_NODE)
					astyleid = d_intern (a[i+1]->str.str);
				else
					astyleid = a[i+1]->id.id;

				if (astyleid == originid) {
					arrow_origin = TRUE;
					arrow_end = FALSE;
				} else if (astyleid == endid) {
					arrow_origin = FALSE;
					arrow_end = TRUE;
				} else if (astyleid == bothid) {
					arrow_origin = TRUE;
					arrow_end = TRUE;
				} else if (astyleid == noneid) { 
					arrow_origin = FALSE;
					arrow_end = FALSE;
				} else {
					gel_errorout (_("%s: arrow style should be \"origin\", \"end\", \"both\", or \"none\""),
						      "LinePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == legendid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No legend specified"),
						      "LinePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if (a[i+1]->type == GEL_STRING_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->str.str);
				} else if (a[i+1]->type == GEL_IDENTIFIER_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->id.id->token);
				} else {
					gel_errorout (_("%s: Legend must be a string"),
						      "LinePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == filledid) {
				filled = TRUE;
			} else {
				gel_errorout (_("%s: Unknown style: %s"),
					      "LinePlotDrawLine",
					      id->token);
				g_free (legend);
				g_free (x);
				g_free (y);
				return NULL;
			}
		} else {
			gel_errorout (_("%s: Bad parameter"), "LinePlotDrawLine");
			g_free (legend);
			g_free (x);
			g_free (y);
			return NULL;
		}
	}

	if (plot_mode != MODE_LINEPLOT &&
	    plot_mode != MODE_LINEPLOT_PARAMETRIC &&
	    plot_mode != MODE_LINEPLOT_SLOPEFIELD &&
	    plot_mode != MODE_LINEPLOT_VECTORFIELD) {
		plot_mode = MODE_LINEPLOT;
		clear_graph ();
		update = FALSE;
	}


	if (line_plot == NULL) {
		add_line_plot ();
		plot_setup_axis ();
		update = FALSE;
	}

	if (update) {
		plot_axis ();
	}

	draw_line (x, y, len, thickness, &color, legend, filled);

	if (arrow_end && len > 1)
		draw_arrowhead (x[len-2], y[len-2],
				x[len-1], y[len-1],
				thickness, &color);
	if (arrow_origin && len > 1)
		draw_arrowhead (x[1], y[1],
				x[0], y[0],
				thickness, &color);

	g_free (legend);

	return gel_makenum_null ();
}

static GelETree *
LinePlotDrawPoints_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int len;
	int nextarg;
	double *x, *y;
	double minx = 0, miny = 0, maxx = 0, maxy = 0;
	GdkRGBA color;
	int thickness;
	int i;
	gboolean update = FALSE;
	char *legend = NULL;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "LinePlotDrawPoints", "LinePlotDrawPoints");
		return NULL;
	}

	ensure_window (FALSE /* do_window_present */);

	if (a[0]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	} else if (a[0]->type == GEL_MATRIX_NODE) {
		if G_UNLIKELY ( ! get_line_numbers (a[0], &x, &y, &len,
						    &minx, &maxx, &miny, &maxy,
						    "LinePlotDrawPoints",
						    1))
			return NULL;
		nextarg = 1;
	} else {
		double x1, y1;
		if G_UNLIKELY (gel_count_arguments (a) < 2) {
			gel_errorout (_("%s: Wrong number of arguments"),
				      "LinePlotDrawPoints");
			return NULL;
		}
		GET_DOUBLE(x1, 0, "LinePlotDrawPoints");
		GET_DOUBLE(y1, 1, "LinePlotDrawPoints");
		len = 1;
		x = g_new (double, 1);
		x[0] = x1;
		y = g_new (double, 1);
		y[0] = y1;
		nextarg = 2;

		minx = x1;
		maxx = x1;
		miny = y1;
		maxy = y1;
	}

	gdk_rgba_parse (&color, "black");
	thickness = 2;

	for (i = nextarg; a[i] != NULL; i++) {
		if G_LIKELY (a[i]->type == GEL_STRING_NODE ||
			     a[i]->type == GEL_IDENTIFIER_NODE) {
			GelToken *id;
			static GelToken *colorid = NULL;
			static GelToken *thicknessid = NULL;
			static GelToken *windowid = NULL;
			static GelToken *fitid = NULL;
			static GelToken *legendid = NULL;

			if (colorid == NULL) {
				colorid = d_intern ("color");
				thicknessid = d_intern ("thickness");
				windowid = d_intern ("window");
				fitid = d_intern ("fit");
				legendid = d_intern ("legend");
			}

			if (a[i]->type == GEL_STRING_NODE)
				id = d_intern (a[i]->str.str);
			else
				id = a[i]->id.id;
			if (id == colorid) {
			        if G_UNLIKELY ( ! get_color (a[i+1], &color, "LinePlotDrawPoints")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == thicknessid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No thickness specified"),
						      "LinePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if G_UNLIKELY ( ! check_argument_positive_integer (a, i+1,
										   "LinePlotDrawPoints")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				thickness = gel_get_nonnegative_integer (a[i+1]->val.value,
									 "LinePlotDrawPoints");
				i++;
			} else if (id == windowid) {
				double x1, x2, y1, y2;
				if G_UNLIKELY (a[i+1] == NULL ||
					       (a[i+1]->type != GEL_STRING_NODE &&
						a[i+1]->type != GEL_IDENTIFIER_NODE &&
						a[i+1]->type != GEL_MATRIX_NODE)) {
					gel_errorout (_("%s: No window specified"),
						      "LinePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if ((a[i+1]->type == GEL_STRING_NODE &&
				     fitid == d_intern (a[i+1]->str.str)) ||
				    (a[i+1]->type == GEL_IDENTIFIER_NODE &&
				     fitid == a[i+1]->id.id)) {
					x1 = minx;
					x2 = maxx;
					y1 = miny;
					y2 = maxy;
					if G_UNLIKELY (x1 == x2) {
						x1 -= 0.1;
						x2 += 0.1;
					}

					/* assume line is a graph so x fits tightly */

					if G_UNLIKELY (y1 == y2) {
						y1 -= 0.1;
						y2 += 0.1;
					} else {
						/* Make window 5% larger on each vertical side */
						double height = (y2-y1);
						y1 -= height * 0.05;
						y2 += height * 0.05;
					}

					update = update_lineplot_window (x1, x2, y1, y2);
				} else if (get_limits_from_matrix (a[i+1], &x1, &x2, &y1, &y2)) {
					update = update_lineplot_window (x1, x2, y1, y2);
				} else {
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else if (id == legendid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No legend specified"),
						      "LinePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				if (a[i+1]->type == GEL_STRING_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->str.str);
				} else if (a[i+1]->type == GEL_IDENTIFIER_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->id.id->token);
				} else {
					gel_errorout (_("%s: Legend must be a string"),
						      "LinePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					return NULL;
				}
				i++;
			} else {
				gel_errorout (_("%s: Unknown style"), "LinePlotDrawPoints");
				g_free (legend);
				g_free (x);
				g_free (y);
				return NULL;
			}
		} else {
			gel_errorout (_("%s: Bad parameter"), "LinePlotDrawPoints");
			g_free (legend);
			g_free (x);
			g_free (y);
			return NULL;
		}
	}

	if (plot_mode != MODE_LINEPLOT &&
	    plot_mode != MODE_LINEPLOT_PARAMETRIC &&
	    plot_mode != MODE_LINEPLOT_SLOPEFIELD &&
	    plot_mode != MODE_LINEPLOT_VECTORFIELD) {
		plot_mode = MODE_LINEPLOT;
		clear_graph ();
		update = FALSE;
	}


	if (line_plot == NULL) {
		add_line_plot ();
		plot_setup_axis ();
		update = FALSE;
	}

	if (update) {
		plot_axis ();
	}

	draw_points (x, y, len, thickness, &color, legend);

	g_free(legend);

	return gel_makenum_null ();
}

static GelETree *
SurfacePlotDrawLine_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int len;
	int nextarg;
	double *x, *y, *z;
	double minx = 0, miny = 0, maxx = 0, maxy = 0, minz = 0, maxz = 0;
	GdkRGBA color;
	int thickness;
	int i;
	gboolean update = FALSE;
	char *legend = NULL;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SurfacePlotDrawLine", "SurfacePlotDrawLine");
		return NULL;
	}

	ensure_window (FALSE /* do_window_present */);

	if (a[0]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	} else if (a[0]->type == GEL_MATRIX_NODE) {
		if G_UNLIKELY ( ! get_surface_line_numbers (a[0], &x, &y, &z, &len,
							    &minx, &maxx,
							    &miny, &maxy,
							    &minz, &maxz,
							    "SurfacePlotDrawLine",
							    2))
			return NULL;
		nextarg = 1;
	} else {
		double x1, y1, x2, y2, z1, z2;
		if G_UNLIKELY (gel_count_arguments (a) < 6) {
			gel_errorout (_("%s: Wrong number of arguments"),
				      "SurfacePlotDrawLine");
			return NULL;
		}
		GET_DOUBLE(x1, 0, "SurfacePlotDrawLine");
		GET_DOUBLE(y1, 1, "SurfacePlotDrawLine");
		GET_DOUBLE(z1, 2, "SurfacePlotDrawLine");
		GET_DOUBLE(x2, 3, "SurfacePlotDrawLine");
		GET_DOUBLE(y2, 4, "SurfacePlotDrawLine");
		GET_DOUBLE(z2, 5, "SurfacePlotDrawLine");
		len = 2;
		x = g_new (double, 2);
		x[0] = x1;
		x[1] = x2;
		y = g_new (double, 2);
		y[0] = y1;
		y[1] = y2;
		z = g_new (double, 2);
		z[0] = z1;
		z[1] = z2;
		nextarg = 6;

		minx = MIN(x1,x2);
		maxx = MAX(x1,x2);
		miny = MIN(y1,y2);
		maxy = MAX(y1,y2);
		minz = MIN(z1,z2);
		maxz = MAX(z1,z2);
	}

	gdk_rgba_parse (&color, "black");
	thickness = 2;

	for (i = nextarg; a[i] != NULL; i++) {
		if G_LIKELY (a[i]->type == GEL_STRING_NODE ||
			     a[i]->type == GEL_IDENTIFIER_NODE) {
			GelToken *id;
			static GelToken *colorid = NULL;
			static GelToken *thicknessid = NULL;
			static GelToken *windowid = NULL;
			static GelToken *fitid = NULL;
			static GelToken *legendid = NULL;

			if (colorid == NULL) {
				colorid = d_intern ("color");
				thicknessid = d_intern ("thickness");
				windowid = d_intern ("window");
				fitid = d_intern ("fit");
				legendid = d_intern ("legend");
			}

			if (a[i]->type == GEL_STRING_NODE)
				id = d_intern (a[i]->str.str);
			else
				id = a[i]->id.id;
			if (id == colorid) {
			        if G_UNLIKELY ( ! get_color (a[i+1], &color, "SurfacePlotDrawLine")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				i++;
			} else if (id == thicknessid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No thickness specified"),
						      "SurfacePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				if G_UNLIKELY ( ! check_argument_positive_integer (a, i+1,
										   "SurfacePlotDrawLine")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				thickness = gel_get_nonnegative_integer (a[i+1]->val.value,
									 "SurfacePlotDrawLine");
				i++;
			} else if (id == windowid) {
				double x1, x2, y1, y2, z1, z2;
				if G_UNLIKELY (a[i+1] == NULL ||
					       (a[i+1]->type != GEL_STRING_NODE &&
						a[i+1]->type != GEL_IDENTIFIER_NODE &&
						a[i+1]->type != GEL_MATRIX_NODE)) {
					gel_errorout (_("%s: No window specified"),
						      "SurfacePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				if ((a[i+1]->type == GEL_STRING_NODE &&
				     fitid == d_intern (a[i+1]->str.str)) ||
				    (a[i+1]->type == GEL_IDENTIFIER_NODE &&
				     fitid == a[i+1]->id.id)) {
					x1 = minx;
					x2 = maxx;
					y1 = miny;
					y2 = maxy;
					z1 = minz;
					z2 = maxz;
					if G_UNLIKELY (x1 == x2) {
						x1 -= 0.1;
						x2 += 0.1;
					}
					if G_UNLIKELY (y1 == y2) {
						y1 -= 0.1;
						y2 += 0.1;
					}

					/* assume line is a graph so x fits tightly */

					if G_UNLIKELY (z1 == z2) {
						z1 -= 0.1;
						z2 += 0.1;
					} else {
						/* Make window 5% larger on each vertical side */
						double height = (z2-z1);
						z1 -= height * 0.05;
						z2 += height * 0.05;
					}

					update = update_surfaceplot_window (x1, x2, y1, y2, z1, z2);
				} else if (get_limits_from_matrix_surf (a[i+1], &x1, &x2, &y1, &y2, &z1, &z2)) {
					update = update_surfaceplot_window (x1, x2, y1, y2, z1, z2);
				} else {
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				i++;
			} else if (id == legendid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No legend specified"),
						      "SurfacePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				if (a[i+1]->type == GEL_STRING_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->str.str);
				} else if (a[i+1]->type == GEL_IDENTIFIER_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->id.id->token);
				} else {
					gel_errorout (_("%s: Legend must be a string"),
						      "SurfacePlotDrawLine");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				i++;
			} else {
				gel_errorout (_("%s: Unknown style"), "SurfacePlotDrawLine");
				g_free (legend);
				g_free (x);
				g_free (y);
				g_free (z);
				return NULL;
			}
		} else {
			gel_errorout (_("%s: Bad parameter"), "SurfacePlotDrawLine");
			g_free (legend);
			g_free (x);
			g_free (y);
			g_free (z);
			return NULL;
		}
	}

	if (plot_mode != MODE_SURFACE ||
	    surface_plot == NULL) {
		plot_mode = MODE_SURFACE;
		plot_surface_functions (TRUE /* do_window_present */,
					surfaceplot_fit_dependent_axis_cb /*fit*/);
		update = FALSE;

	}

	draw_surface_line (x, y, z, len, thickness, &color, legend);

	if (update && surface_plot != NULL) {
		plot_axis ();
	}

	g_free (legend);

	return gel_makenum_null ();
}

static GelETree *
SurfacePlotDrawPoints_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int len;
	int nextarg;
	double *x, *y, *z;
	double minx = 0, miny = 0, maxx = 0, maxy = 0, minz = 0, maxz = 0;
	GdkRGBA color;
	int thickness;
	int i;
	gboolean update = FALSE;
	char *legend = NULL;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SurfacePlotDrawPoints", "SurfacePlotDrawPoints");
		return NULL;
	}

	ensure_window (FALSE /* do_window_present */);

	if (a[0]->type == GEL_NULL_NODE) {
		return gel_makenum_null ();
	} else if (a[0]->type == GEL_MATRIX_NODE) {
		if G_UNLIKELY ( ! get_surface_line_numbers (a[0], &x, &y, &z, &len,
							    &minx, &maxx,
							    &miny, &maxy,
							    &minz, &maxz,
							    "SurfacePlotDrawPoints",
							    1))
			return NULL;
		nextarg = 1;
	} else {
		double x1, y1, z1;
		if G_UNLIKELY (gel_count_arguments (a) < 3) {
			gel_errorout (_("%s: Wrong number of arguments"),
				      "SurfacePlotDrawSurface");
			return NULL;
		}
		GET_DOUBLE(x1, 0, "SurfacePlotDrawPoints");
		GET_DOUBLE(y1, 1, "SurfacePlotDrawPoints");
		GET_DOUBLE(z1, 2, "SurfacePlotDrawPoints");
		len = 1;
		x = g_new (double, 1);
		x[0] = x1;
		y = g_new (double, 1);
		y[0] = y1;
		z = g_new (double, 1);
		z[0] = z1;
		nextarg = 3;

		minx = x1;
		maxx = x1;
		miny = y1;
		maxy = y1;
		minz = z1;
		maxz = z1;
	}

	gdk_rgba_parse (&color, "black");
	thickness = 2;

	for (i = nextarg; a[i] != NULL; i++) {
		if G_LIKELY (a[i]->type == GEL_STRING_NODE ||
			     a[i]->type == GEL_IDENTIFIER_NODE) {
			GelToken *id;
			static GelToken *colorid = NULL;
			static GelToken *thicknessid = NULL;
			static GelToken *windowid = NULL;
			static GelToken *fitid = NULL;
			static GelToken *legendid = NULL;

			if (colorid == NULL) {
				colorid = d_intern ("color");
				thicknessid = d_intern ("thickness");
				windowid = d_intern ("window");
				fitid = d_intern ("fit");
				legendid = d_intern ("legend");
			}

			if (a[i]->type == GEL_STRING_NODE)
				id = d_intern (a[i]->str.str);
			else
				id = a[i]->id.id;
			if (id == colorid) {
			        if G_UNLIKELY ( ! get_color (a[i+1], &color, "SurfacePlotDrawPoints")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				i++;
			} else if (id == thicknessid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No thickness specified"),
						      "SurfacePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				if G_UNLIKELY ( ! check_argument_positive_integer (a, i+1,
										   "SurfacePlotDrawPoints")) {
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				thickness = gel_get_nonnegative_integer (a[i+1]->val.value,
									 "SurfacePlotDrawPoints");
				i++;
			} else if (id == windowid) {
				double x1, x2, y1, y2, z1, z2;
				if G_UNLIKELY (a[i+1] == NULL ||
					       (a[i+1]->type != GEL_STRING_NODE &&
						a[i+1]->type != GEL_IDENTIFIER_NODE &&
						a[i+1]->type != GEL_MATRIX_NODE)) {
					gel_errorout (_("%s: No window specified"),
						      "SurfacePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				if ((a[i+1]->type == GEL_STRING_NODE &&
				     fitid == d_intern (a[i+1]->str.str)) ||
				    (a[i+1]->type == GEL_IDENTIFIER_NODE &&
				     fitid == a[i+1]->id.id)) {
					x1 = minx;
					x2 = maxx;
					y1 = miny;
					y2 = maxy;
					z1 = minz;
					z2 = maxz;
					if G_UNLIKELY (x1 == x2) {
						x1 -= 0.1;
						x2 += 0.1;
					}
					if G_UNLIKELY (y1 == y2) {
						y1 -= 0.1;
						y2 += 0.1;
					}

					/* assume line is a graph so x fits tightly */

					if G_UNLIKELY (z1 == z2) {
						z1 -= 0.1;
						z2 += 0.1;
					} else {
						/* Make window 5% larger on each vertical side */
						double height = (z2-z1);
						z1 -= height * 0.05;
						z2 += height * 0.05;
					}

					update = update_surfaceplot_window (x1, x2, y1, y2, z1, z2);
				} else if (get_limits_from_matrix_surf (a[i+1], &x1, &x2, &y1, &y2, &z1, &z2)) {
					update = update_surfaceplot_window (x1, x2, y1, y2, z1, z2);
				} else {
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				i++;
			} else if (id == legendid) {
				if G_UNLIKELY (a[i+1] == NULL)  {
					gel_errorout (_("%s: No legend specified"),
						      "SurfacePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				if (a[i+1]->type == GEL_STRING_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->str.str);
				} else if (a[i+1]->type == GEL_IDENTIFIER_NODE) {
					g_free (legend);
					legend = g_strdup (a[i+1]->id.id->token);
				} else {
					gel_errorout (_("%s: Legend must be a string"),
						      "SurfacePlotDrawPoints");
					g_free (legend);
					g_free (x);
					g_free (y);
					g_free (z);
					return NULL;
				}
				i++;
			} else {
				gel_errorout (_("%s: Unknown style"), "SurfacePlotDrawPoints");
				g_free (legend);
				g_free (x);
				g_free (y);
				g_free (z);
				return NULL;
			}
		} else {
			gel_errorout (_("%s: Bad parameter"), "SurfacePlotDrawPoints");
			g_free (legend);
			g_free (x);
			g_free (y);
			g_free (z);
			return NULL;
		}
	}

	if (plot_mode != MODE_SURFACE ||
	    surface_plot == NULL) {
		plot_mode = MODE_SURFACE;
		plot_surface_functions (TRUE /* do_window_present */,
					surfaceplot_fit_dependent_axis_cb /*fit*/);
		update = FALSE;

	}

	draw_surface_points (x, y, z, len, thickness, &color, legend);

	if (update && surface_plot != NULL) {
		plot_axis ();
	}

	g_free (legend);

	return gel_makenum_null ();
}

static gboolean
get_surface_data (GelETree *a, double **x, double **y, double **z, int *len,
		  double *minx, double *maxx,
		  double *miny, double *maxy,
		  double *minz, double *maxz)
{
	int i;
	GelMatrixW *m;
	gboolean nominmax = TRUE;
#define UPDATE_MINMAX \
	if (minx != NULL) { \
		if (xx > *maxx || nominmax) *maxx = xx; \
		if (xx < *minx || nominmax) *minx = xx; \
		if (yy > *maxy || nominmax) *maxy = yy; \
		if (yy < *miny || nominmax) *miny = yy; \
		if (zz > *maxz || nominmax) *maxz = zz; \
		if (zz < *minz || nominmax) *minz = zz; \
		nominmax = FALSE; \
	}

	g_return_val_if_fail (a->type == GEL_MATRIX_NODE, FALSE);

	m = a->mat.matrix;

	if G_UNLIKELY ( ! gel_is_matrix_value_only_real (m)) {
		gel_errorout (_("%s: Surface should be given as a real, n by 3 matrix "
				"with columns for x, y, z, where n>=3"),
			      "SurfacePlotData");
		return FALSE;
	}

	if (gel_matrixw_width (m) == 3 &&
	    gel_matrixw_height (m) >= 3) {
		*len = gel_matrixw_height (m);

		*x = g_new (double, *len);
		*y = g_new (double, *len);
		*z = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy, zz;
			GelETree *t = gel_matrixw_index (m, 0, i);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 1, i);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 2, i);
			(*z)[i] = zz = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else if (gel_matrixw_width (m) == 1 &&
		   gel_matrixw_height (m) % 3 == 0 &&
		   gel_matrixw_height (m) >= 9) {
		*len = gel_matrixw_height (m) / 3;

		*x = g_new (double, *len);
		*y = g_new (double, *len);
		*z = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy, zz;
			GelETree *t = gel_matrixw_index (m, 0, 3*i);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 0, (3*i) + 1);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, 0, (3*i) + 2);
			(*z)[i] = zz = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else if (gel_matrixw_height (m) == 1 &&
		   gel_matrixw_width (m) % 3 == 0 &&
		   gel_matrixw_width (m) >= 9) {
		*len = gel_matrixw_width (m) / 3;

		*x = g_new (double, *len);
		*y = g_new (double, *len);
		*z = g_new (double, *len);

		for (i = 0; i < *len; i++) {
			double xx, yy, zz;
			GelETree *t = gel_matrixw_index (m, 3*i, 0);
			(*x)[i] = xx = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, (3*i) + 1, 0);
			(*y)[i] = yy = mpw_get_double (t->val.value);
			t = gel_matrixw_index (m, (3*i) + 2, 0);
			(*z)[i] = zz = mpw_get_double (t->val.value);
			UPDATE_MINMAX
		}
	} else {
		gel_errorout (_("%s: Surface should be given as a real, n by 3 matrix "
				"with columns for x, y, z, where n>=3"),
			      "SurfacePlotData");
		return FALSE;
	}

	return TRUE;
#undef UPDATE_MINMAX
}

static GelETree *
SurfacePlotData_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1 = 0, x2 = 0, y1 = 0, y2 = 0, z1 = 0, z2 = 0;
	double *x,*y,*z;
	char *name = NULL;
	int len;
	int i;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SurfacePlotData", "SurfacePlotData");
		return NULL;
	}

	i = 0;

	if (a[i] != NULL && a[i]->type != GEL_MATRIX_NODE) {
		gel_errorout (_("%s: argument not a matrix of data"), "SurfacePlotData");
		return NULL;
	}

	if ( ! get_surface_data (a[i], &x, &y, &z, &len,
				 &x1, &x2, &y1, &y2, &z1, &z2)) {
		return NULL;
	}

	/* sanity checks */
	if (x1 == x2) {
		x1=x1-1;
		x2=x2+1;
	}
	if (y1 == y2) {
		y1=y1-1;
		y2=y2+1;
	}
	if (z1 == z2) {
		z1=z1-1;
		z2=z2+1;
	}

	i++;

	if (a[i] != NULL && a[i]->type == GEL_STRING_NODE) {
		name = a[i]->str.str;
		i++;
	}

	/* Defaults to min/max of the data */

	if (a[i] != NULL) {
		if (a[i]->type == GEL_MATRIX_NODE) {
			if (gel_matrixw_elements (a[i]->mat.matrix) == 6) {
				if ( ! get_limits_from_matrix_surf (a[i], &x1, &x2, &y1, &y2, &z1, &z2))
					goto whack_copied_data;
			} else {
				if ( ! get_limits_from_matrix (a[i], &x1, &x2, &y1, &y2))
					goto whack_copied_data;
			}

			i++;
		} else {
			GET_DOUBLE(x1, i, "SurfacePlotData");
			i++;
			if (a[i] != NULL) {
				GET_DOUBLE(x2, i, "SurfacePlotData");
				i++;
				if (a[i] != NULL) {
					GET_DOUBLE(y1, i, "SurfacePlotData");
					i++;
					if (a[i] != NULL) {
						GET_DOUBLE(y2, i, "SurfacePlotData");
						i++;
						if (a[i] != NULL) {
							GET_DOUBLE(z1, i, "SurfacePlotData");
							i++;
							if (a[i] != NULL) {
								GET_DOUBLE(z2, i, "SurfacePlotData");
								i++;
							}
						}
					}
				}
			}
			/* FIXME: what about errors */
			if G_UNLIKELY (gel_error_num != 0) {
				gel_error_num = 0;
				goto whack_copied_data;
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
		gel_errorout (_("%s: invalid X range"), "SurfacePlotData");
		goto whack_copied_data;
	}

	if (y1 == y2) {
		gel_errorout (_("%s: invalid Y range"), "SurfacePlotData");
		goto whack_copied_data;
	}

	if (z1 == z2) {
		gel_errorout (_("%s: invalid Z range"), "SurfacePlotData");
		goto whack_copied_data;
	}

	/* name could also come after */
	if (a[i] != NULL && a[i]->type == GEL_STRING_NODE) {
		name = a[i]->str.str;
		i++;
	}

	if (surface_func_name != NULL)
		g_free (surface_func_name);
	if (name != NULL)
		surface_func_name = g_strdup (name);
	else
		surface_func_name = NULL;

	surface_func = NULL;
	surface_data_x = x;
	x = NULL;
	surface_data_y = y;
	y = NULL;
	surface_data_z = z;
	z = NULL;
	surface_data_len = len;

	reset_surfacex1 = surfacex1 = x1;
	reset_surfacex2 = surfacex2 = x2;
	reset_surfacey1 = surfacey1 = y1;
	reset_surfacey2 = surfacey2 = y2;
	reset_surfacez1 = surfacez1 = z1;
	reset_surfacez2 = surfacez2 = z2;

	plot_minz = z1;
	plot_maxz = z2;

	plot_mode = MODE_SURFACE;
	plot_surface_functions (FALSE /* do_window_present */,
				FALSE /* fit */);

	if (gel_interrupted)
		return NULL;
	else
		return gel_makenum_null ();

whack_copied_data:
	if (x != NULL)
		g_free (x);
	if (y != NULL)
		g_free (y);
	if (z != NULL)
		g_free (z);

	return NULL;
}

static gboolean
get_surface_data_grid (GelETree *a,
		       double **x, double **y, double **z, int *len,
		       double minx, double maxx,
		       double miny, double maxy,
		       gboolean setz,
		       double *minz, double *maxz)
{
	int i, j, k;
	GelMatrixW *m;
	gboolean nominmax = TRUE;
	int w, h;

#define UPDATE_MINMAX \
	if (setz) { \
		if (zz > *maxz || nominmax) *maxz = zz; \
		if (zz < *minz || nominmax) *minz = zz; \
		nominmax = FALSE; \
	}

	g_return_val_if_fail (a->type == GEL_MATRIX_NODE, FALSE);

	m = a->mat.matrix;

	if G_UNLIKELY ( ! gel_is_matrix_value_only_real (m)) {
		gel_errorout (_("%s: Surface grid data should be given as a real matrix "),
			      "SurfacePlotDataGrid");
		return FALSE;
	}

	w = gel_matrixw_width (m);
	h = gel_matrixw_height (m);
	*len = w * h;

	*x = g_new (double, *len);
	*y = g_new (double, *len);
	*z = g_new (double, *len);

	k = 0;
	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			double zz;
			GelETree *t = gel_matrixw_index (m, i, j);
			(*z)[k] = zz = mpw_get_double (t->val.value);
			(*x)[k] = minx+((double)j)*(maxx-minx)/((double)(h-1));
			(*y)[k] = miny+((double)i)*(maxy-miny)/((double)(w-1));
			k++;
			UPDATE_MINMAX
		}
	}

	return TRUE;
#undef UPDATE_MINMAX
}

static GelETree *
SurfacePlotDataGrid_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2, z1, z2;
	double *x,*y,*z;
	char *name = NULL;
	int len;
	gboolean setz = FALSE;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "SurfacePlotDataGrid", "SurfacePlotDataGrid");
		return NULL;
	}

	if (a[1]->type != GEL_MATRIX_NODE) {
		gel_errorout (_("%s: first argument not a matrix of data"), "SurfacePlotDataGrid");
		return NULL;
	}

	if (a[1]->type != GEL_MATRIX_NODE &&
	    (gel_matrixw_elements (a[1]->mat.matrix) != 6 &&
	     gel_matrixw_elements (a[1]->mat.matrix) != 4)) {
		gel_errorout (_("%s: second argument not a 4 or 6 element vector of limits"), "SurfacePlotDataGrid");
		return NULL;
	}

	if (gel_matrixw_elements (a[1]->mat.matrix) == 6) {
		if ( ! get_limits_from_matrix_surf (a[1], &x1, &x2, &y1, &y2, &z1, &z2))
			return NULL;
		setz = FALSE;
	} else {
		if ( ! get_limits_from_matrix (a[1], &x1, &x2, &y1, &y2))
			return NULL;
		setz = TRUE;
	}

	if (a[2] != NULL && a[2]->type == GEL_STRING_NODE) {
		name = a[2]->str.str;
	} else if (a[2] != NULL && a[3] != NULL) {
		gel_errorout (_("%s: too many arguments or last argument not a string label"), "SurfacePlotDataGrid");
		return NULL;
	}

	if ( ! get_surface_data_grid (a[0], &x, &y, &z, &len, x1, x2, y1, y2, setz, &z1, &z2)) {
		return NULL;
	}

	/* sanity */
	if (z1 == z2) {
		z1=z1-1;
		z2=z2+1;
	}

	if (surface_func_name != NULL)
		g_free (surface_func_name);
	if (name != NULL)
		surface_func_name = g_strdup (name);
	else
		surface_func_name = NULL;

	surface_func = NULL;
	surface_data_x = x;
	x = NULL;
	surface_data_y = y;
	y = NULL;
	surface_data_z = z;
	z = NULL;
	surface_data_len = len;

	reset_surfacex1 = surfacex1 = x1;
	reset_surfacex2 = surfacex2 = x2;
	reset_surfacey1 = surfacey1 = y1;
	reset_surfacey2 = surfacey2 = y2;
	reset_surfacez1 = surfacez1 = z1;
	reset_surfacez2 = surfacez2 = z2;

	plot_minz = z1;
	plot_maxz = z2;

	plot_mode = MODE_SURFACE;
	plot_surface_functions (FALSE /* do_window_present */,
				FALSE /* fit */);

	if (gel_interrupted)
		return NULL;
	else
		return gel_makenum_null ();

	return NULL;
}

static GelETree *
ExportPlot_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	char *file;
	char *type;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "ExportPlot", "ExportPlot");
		return NULL;
	}

	if (a[0]->type != GEL_STRING_NODE ||
	    ve_string_empty(a[0]->str.str)) {
		gel_errorout (_("%s: first argument not a nonempty string"), "ExportPlot");
		return NULL;
	}
	file = a[0]->str.str;

	if (a[1] == NULL) {
		char *dot = strrchr (file, '.');
		if (dot == NULL) {
			gel_errorout (_("%s: type not specified and filename has no extension"), "ExportPlot");
			return NULL;
		}
		type = dot+1;
	}

	if (a[1] != NULL) {
		if (a[1]->type != GEL_STRING_NODE ||
		    ve_string_empty(a[1]->str.str)) {
			gel_errorout (_("%s: second argument not a nonempty string"), "ExportPlot");
			return NULL;
		}
		type = a[1]->str.str;
	}

	if (a[1] != NULL && a[2] != NULL) {
		gel_errorout (_("%s: too many arguments"), "ExportPlot");
		return NULL;
	}

	if (plot_canvas == NULL) {
		gel_errorout (_("%s: plot canvas not active, cannot export"), "ExportPlot");
		return NULL;
	}

	if (strcasecmp (type, "png") == 0) {
		GdkPixbuf *pix;

		/* sanity */
		if (GTK_PLOT_CANVAS (plot_canvas)->pixmap == NULL) {
			gel_errorout (_("%s: export failed"), "ExportPlot");
			return NULL;
		}

		pix = gdk_pixbuf_get_from_surface
			(GTK_PLOT_CANVAS (plot_canvas)->pixmap,
			 0 /* src x */, 0 /* src y */,
			 GTK_PLOT_CANVAS (plot_canvas)->pixmap_width,
			 GTK_PLOT_CANVAS (plot_canvas)->pixmap_height);

		if (pix == NULL ||
		    ! gdk_pixbuf_save (pix, file, "png", NULL /* error */, NULL)) {
			if (pix != NULL)
				g_object_unref (G_OBJECT (pix));
			gel_errorout (_("%s: export failed"), "ExportPlot");
			return NULL;
		}

		g_object_unref (G_OBJECT (pix));
	} else if (strcasecmp (type, "eps") == 0 ||
		   strcasecmp (type, "ps") == 0) {
		gboolean eps = (strcasecmp (type, "eps") == 0);

		plot_in_progress ++;
		gel_calc_running ++;
		plot_window_setup ();

		if ( ! gtk_plot_canvas_export_ps_with_size
			(GTK_PLOT_CANVAS (plot_canvas),
			 file,
			 GTK_PLOT_PORTRAIT,
			 eps /* epsflag */,
			 GTK_PLOT_PSPOINTS,
			 400, ASPECT * 400)) {
			plot_in_progress --;
			gel_calc_running --;
			plot_window_setup ();
			gel_errorout (_("%s: export failed"), "ExportPlot");
			return NULL;
		}

		/* need this for some reason */
		if (plot_canvas != NULL) {
			gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
		}

		plot_in_progress --;
		gel_calc_running --;
		plot_window_setup ();
	} else {
		gel_errorout (_("%s: unknown file type, can be \"png\", \"eps\", or \"ps\"."), "ExportPlot");
		return NULL;
	}

	return gel_makenum_bool (TRUE);
}



static GelETree *
set_LinePlotWindow (GelETree * a)
{
	double x1, x2, y1, y2;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_LinePlotWindow", "set_LinePlotWindow");
		return NULL;
	}


	if G_UNLIKELY ( ! get_limits_from_matrix (a, &x1, &x2, &y1, &y2))
		return NULL;

	if (update_lineplot_window (x1, x2, y1, y2)) {
		if (line_plot != NULL)
			plot_axis ();
	}

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

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_SurfacePlotWindow", "set_SurfacePlotWindow");
		return NULL;
	}

	if G_UNLIKELY ( ! get_limits_from_matrix_surf (a, &x1, &x2, &y1, &y2, &z1, &z2))
		return NULL;

	if (update_surfaceplot_window (x1, x2, y1, y2, z1, z2)) {
		if (surface_plot != NULL) {
			plot_axis ();
		}
	}

	return make_matrix_from_limits_surf ();
}

static GelETree *
get_SurfacePlotWindow (void)
{
	return make_matrix_from_limits_surf ();
}

static GelETree *
set_SlopefieldTicks (GelETree * a)
{
	int v, h;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_SlopefieldTicks", "set_SlopefieldTicks");
		return NULL;
	}

	if G_UNLIKELY ( ! get_ticks_from_matrix (a, &v, &h))
		return NULL;

	plot_sf_Vtick = v;
	plot_sf_Htick = h;

	return make_matrix_from_ticks (plot_sf_Vtick, plot_sf_Htick);
}

static GelETree *
get_SlopefieldTicks (void)
{
	return make_matrix_from_ticks (plot_sf_Vtick, plot_sf_Htick);
}

static GelETree *
set_VectorfieldTicks (GelETree * a)
{
	int v, h;

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_VectorfieldTicks", "set_VectorfieldTicks");
		return NULL;
	}

	if G_UNLIKELY ( ! get_ticks_from_matrix (a, &v, &h))
		return NULL;

	plot_vf_Vtick = v;
	plot_vf_Htick = h;

	return make_matrix_from_ticks (plot_vf_Vtick, plot_vf_Htick);
}

static GelETree *
get_VectorfieldTicks (void)
{
	return make_matrix_from_ticks (plot_vf_Vtick, plot_vf_Htick);
}

static GelETree *
set_LinePlotVariableNames (GelETree * a)
{
	GelETree *t;
	char *sx, *sy, *sz, *st;

	init_var_names ();

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_LinePlotVariableNames", "set_LinePlotVariableNames");
		return NULL;
	}

	if (a->type != GEL_MATRIX_NODE ||
	    gel_matrixw_elements (a->mat.matrix) != 4) {
		gel_errorout (_("Variable names not given in a 4-vector"));
		return NULL;
	}

	t = gel_matrixw_vindex (a->mat.matrix, 0);
	if (t->type == GEL_IDENTIFIER_NODE) {
		sx = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		sx = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	t = gel_matrixw_vindex (a->mat.matrix, 1);
	if (t->type == GEL_IDENTIFIER_NODE) {
		sy = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		sy = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	t = gel_matrixw_vindex (a->mat.matrix, 2);
	if (t->type == GEL_IDENTIFIER_NODE) {
		sz = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		sz = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	t = gel_matrixw_vindex (a->mat.matrix, 3);
	if (t->type == GEL_IDENTIFIER_NODE) {
		st = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		st = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	if ( ! is_identifier (sx) ||
	     ! is_identifier (sy) ||
	     ! is_identifier (sz) ||
	     ! is_identifier (st)) {
		gel_errorout (_("Variable names must be valid identifiers"));
		return NULL;
	}
	if (strcmp (sx, sy) == 0 ||
	    strcmp (sx, sz) == 0 ||
	    strcmp (sx, st) == 0 ||
	    strcmp (sy, sz) == 0 ||
	    strcmp (sy, st) == 0 ||
	    strcmp (sz, st) == 0) {
		gel_errorout (_("Variable names must be mutually distinct"));
		return NULL;
	}

	g_free (lp_x_name);
	g_free (lp_y_name);
	g_free (lp_z_name);
	g_free (lp_t_name);
	lp_x_name = g_strdup (sx);
	lp_y_name = g_strdup (sy);
	lp_z_name = g_strdup (sz);
	lp_t_name = g_strdup (st);

	set_lineplot_labels ();
	set_solver_labels ();

	return make_matrix_from_lp_varnames ();
}

static GelETree *
get_LinePlotVariableNames (void)
{
	return make_matrix_from_lp_varnames ();
}

static GelETree *
set_SurfacePlotVariableNames (GelETree * a)
{
	GelETree *t;
	char *sx, *sy, *sz;

	init_var_names ();

	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_SurfacePlotVariableNames", "set_SurfacePlotVariableNames");
		return NULL;
	}

	if (a->type != GEL_MATRIX_NODE ||
	    gel_matrixw_elements (a->mat.matrix) != 3) {
		gel_errorout (_("Variable names not given in a 3-vector"));
		return NULL;
	}

	t = gel_matrixw_vindex (a->mat.matrix, 0);
	if (t->type == GEL_IDENTIFIER_NODE) {
		sx = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		sx = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	t = gel_matrixw_vindex (a->mat.matrix, 1);
	if (t->type == GEL_IDENTIFIER_NODE) {
		sy = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		sy = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	t = gel_matrixw_vindex (a->mat.matrix, 2);
	if (t->type == GEL_IDENTIFIER_NODE) {
		sz = t->id.id->token;
	} else if (t->type == GEL_STRING_NODE) {
		sz = t->str.str;
	} else {
		gel_errorout (_("Variable names should be strings"));
		return NULL;
	}
	if ( ! is_identifier (sx) ||
	     ! is_identifier (sy) ||
	     ! is_identifier (sz)) {
		gel_errorout (_("Variable names must be valid identifiers"));
		return NULL;
	}
	if (strcmp (sx, sy) == 0 ||
	    strcmp (sx, sz) == 0 ||
	    strcmp (sy, sz) == 0) {
		gel_errorout (_("Variable names must be mutually distinct"));
		return NULL;
	}

	g_free (sp_x_name);
	g_free (sp_y_name);
	g_free (sp_z_name);
	sp_x_name = g_strdup (sx);
	sp_y_name = g_strdup (sy);
	sp_z_name = g_strdup (sz);

	set_surface_labels ();

	if (surface_plot != NULL) {
		GtkPlotAxis *axis;

		axis = gtk_plot_get_axis (GTK_PLOT (surface_plot), GTK_PLOT_AXIS_BOTTOM);
		gtk_plot_axis_set_title (axis, sp_x_name);
		axis = gtk_plot_get_axis (GTK_PLOT (surface_plot), GTK_PLOT_AXIS_LEFT);
		gtk_plot_axis_set_title (axis, sp_y_name);
		axis = gtk_plot_get_axis (GTK_PLOT (surface_plot), GTK_PLOT_AXIS_TOP);
		gtk_plot_axis_set_title (axis, "");

		if (plot_canvas != NULL) {
			gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
			gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
		}
	}

	return make_matrix_from_sp_varnames ();
}

static GelETree *
get_SurfacePlotVariableNames (void)
{
	return make_matrix_from_sp_varnames ();
}

static GelETree *
set_VectorfieldNormalized (GelETree * a)
{
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_VectorfieldNormalized", "set_VectorfieldNormalized");
		return NULL;
	}

	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_VectorfieldNormalized"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		vectorfield_normalize_arrow_length_parameter
			= ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
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
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_LinePlotDrawLegends", "set_LinePlotDrawLegends");
		return NULL;
	}
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_LinePlotDrawLegends"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		lineplot_draw_legends
			= ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		lineplot_draw_legends = a->bool_.bool_;

	if (line_plot != NULL) {
		line_plot_move_about ();

		if (plot_canvas != NULL) {
			gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
			gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
		}
	}

	return gel_makenum_bool (lineplot_draw_legends);
}
static GelETree *
get_LinePlotDrawLegends (void)
{
	return gel_makenum_bool (lineplot_draw_legends);
}

static GelETree *
set_SurfacePlotDrawLegends (GelETree * a)
{
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_SurfacePlotDrawLegends", "set_SurfacePlotDrawLegends");
		return NULL;
	}
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_LinePlotDrawLegends"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		surfaceplot_draw_legends
			= ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		surfaceplot_draw_legends = a->bool_.bool_;

	if (surface_plot != NULL) {
		if (surface_data != NULL) {
			if (surfaceplot_draw_legends) {
				gtk_plot_data_gradient_set_visible (GTK_PLOT_DATA (surface_data), TRUE);
				gtk_plot_data_show_legend (GTK_PLOT_DATA (surface_data));
			} else {
				gtk_plot_data_gradient_set_visible (GTK_PLOT_DATA (surface_data), FALSE);
				gtk_plot_data_hide_legend (GTK_PLOT_DATA (surface_data));
			}
		}

		if (surfaceplot_draw_legends)
			gtk_plot_show_legends (GTK_PLOT (surface_plot));
		else
			gtk_plot_hide_legends (GTK_PLOT (surface_plot));

		surface_plot_move_about ();

		if (plot_canvas != NULL) {
			gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
			gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
		}
	}

	return gel_makenum_bool (surfaceplot_draw_legends);
}
static GelETree *
get_SurfacePlotDrawLegends (void)
{
	return gel_makenum_bool (surfaceplot_draw_legends);
}

static GelETree *
set_LinePlotDrawAxisLabels (GelETree * a)
{
	if G_UNLIKELY (plot_in_progress != 0) {
		gel_errorout (_("%s: Plotting in progress, cannot call %s"),
			      "set_LinePlotDrawAxisLabels", "set_LinePlotDrawAxisLabels");
		return NULL;
	}
	if G_UNLIKELY ( ! check_argument_bool (&a, 0, "set_LinePlotDrawAxisLabels"))
		return NULL;
	if (a->type == GEL_VALUE_NODE)
		lineplot_draw_labels
			= ! mpw_zero_p (a->val.value);
	else /* a->type == GEL_BOOL_NODE */
		lineplot_draw_labels = a->bool_.bool_;

	if (line_plot != NULL) {
		plot_setup_axis ();

		if (plot_canvas != NULL) {
			gtk_plot_canvas_paint (GTK_PLOT_CANVAS (plot_canvas));
			gtk_widget_queue_draw (GTK_WIDGET (plot_canvas));
		}
	}

	return gel_makenum_bool (lineplot_draw_labels);
}
static GelETree *
get_LinePlotDrawAxisLabels (void)
{
	return gel_makenum_bool (lineplot_draw_labels);
}

void
gel_add_graph_functions (void)
{
	GelEFunc *f;
	GelToken *id;

	gel_new_category ("plotting", N_("Plotting"), TRUE /* internal */);

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

	FUNC (SurfacePlotClear, 0, "", "plotting", N_("Show the surface (3d) plot window and clear out functions"));

	VFUNC (SurfacePlotData, 2, "data,args", "plotting", N_("Plot surface data given as n by 3 matrix (n>=3) of data with each row being x,y,z.  Optionally can pass a label string and limits.  If no limits passed, limits computed from data."));
	VFUNC (SurfacePlotDataGrid, 3, "data,limits,label", "plotting", N_("Plot surface data given as a matrix (where rows are the x coordinate and columns are the y coordinate), the limits are given as [x1,x2,y1,y2] or optionally [x1,x2,y1,y2,z1,z2], and optionally a string for the label."));
	VFUNC (SurfacePlotDrawLine, 2, "x1,y1,z1,x2,y2,z2,args", "plotting", N_("Draw a line from x1,y1,z1 to x2,y2,z2 on the surface (3d) plot.  x1,y1,z1,x2,y2,z2 can be replaced by a n by 3 matrix for a longer line"));
	VFUNC (SurfacePlotDrawPoints, 2, "x,y,z,args", "plotting", N_("Draw a point at x,y,z on the surface (3d) plot.  x,y,z can be replaced by a n by 3 matrix for more points."));

	FUNC (LinePlotClear, 0, "", "plotting", N_("Show the line plot window and clear out functions"));
	VFUNC (LinePlotDrawLine, 2, "x1,y1,x2,y2,args", "plotting", N_("Draw a line from x1,y1 to x2,y2.  x1,y1,x2,y2 can be replaced by a n by 2 matrix for a longer line."));
	VFUNC (LinePlotDrawPoints, 2, "x,y,args", "plotting", N_("Draw a point at x,y.  x,y can be replaced by a n by 2 matrix for more points."));

	FUNC (PlotCanvasFreeze, 0, "", "plotting", N_("Freeze the plot canvas, that is, inhibit drawing"));
	FUNC (PlotCanvasThaw, 0, "", "plotting", N_("Thaw the plot canvas and redraw the plot immediately"));
	FUNC (PlotWindowPresent, 0, "", "plotting", N_("Raise the plot window, and create the window if necessary"));

	FUNC (LinePlotWaitForClick, 0, "", "plotting", N_("Wait for a click on the line plot window, return the location."));
	FUNC (LinePlotMouseLocation, 0, "", "plotting", N_("Return current mouse location on the line plot window."));

	VFUNC (ExportPlot, 2, "filename,type", "plotting", N_("Export the current contents of the plot canvas to a file.  The file type is given by the string type, which can be \"png\", \"eps\", or \"ps\"."));

	PARAMETER (SlopefieldTicks, N_("Number of slopefield ticks as a vector [vertical,horizontal]."));
	PARAMETER (VectorfieldTicks, N_("Number of vectorfield ticks as a vector [vertical,horizontal]."));
	PARAMETER (LinePlotVariableNames, N_("Default names used by all 2D plot functions.  Should be a 4 vector of strings or identifiers [x,y,z,t]."));
	PARAMETER (SurfacePlotVariableNames, N_("Default names used by surface plot functions.  Should be a 3 vector of strings or identifiers [x,y,z] (where z=x+iy and not the dependent axis)."));

	PARAMETER (VectorfieldNormalized, N_("Normalize vectorfields if true.  That is, only show direction and not magnitude."));
	PARAMETER (LinePlotDrawLegends, N_("If to draw legends or not on line plots."));
	PARAMETER (LinePlotDrawAxisLabels, N_("If to draw axis labels on line plots."));

	PARAMETER (SurfacePlotDrawLegends, N_("If to draw legends or not on surface plots."));

	PARAMETER (LinePlotWindow, N_("Line plotting window (limits) as a 4-vector of the form [x1,x2,y1,y2]"));
	PARAMETER (SurfacePlotWindow, N_("Surface plotting window (limits) as a 6-vector of the form [x1,x2,y1,y2,z1,z2]"));
}
