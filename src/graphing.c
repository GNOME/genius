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

static GtkWidget *graph_window = NULL;
static GnomeCanvas *canvas = NULL;
static GnomeCanvasGroup *root = NULL;
static GnomeCanvasGroup *graph = NULL;

#define MAXFUNC 10

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
static GelEFunc *replot_func[MAXFUNC] = { NULL };
static double replotx1 = -M_PI;
static double replotx2 = M_PI;
static double reploty1 = -1.1;
static double reploty2 = 1.1;

static double replot_maxy = - G_MAXDOUBLE/2;
static double replot_miny = G_MAXDOUBLE/2;
static void replot_functions (GelCtx *ctx);

static int replot_in_progress = 0;

#define P2C_X(x) (((x)-x1)*xscale)
#define P2C_Y(y) ((y2-(y))*yscale)

#define WIDTH 640
#define HEIGHT 480

enum {
	RESPONSE_STOP=1,
	RESPONSE_ZOOMOUT,
	RESPONSE_ZOOMFIT,
	RESPONSE_PLOT
};

static void
display_error (const char *err)
{
	static GtkWidget *w = NULL;

	if (w != NULL)
		gtk_widget_destroy (w);

	w = gtk_message_dialog_new (GTK_WINDOW (genius_window) /* parent */,
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
dialog_response (GtkWidget *w, int response, gpointer data)
{
	if (response == GTK_RESPONSE_CLOSE ||
	    response == GTK_RESPONSE_DELETE_EVENT) {
		if (replot_in_progress > 0)
			interrupted = TRUE;
		gtk_widget_destroy (graph_window);
	} else if (response == RESPONSE_STOP && replot_in_progress > 0) {
		interrupted = TRUE;
	} else if (response == RESPONSE_ZOOMOUT && replot_in_progress == 0) {
		GelCtx *ctx;
		double len;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		len = replotx2 - replotx1;
		replotx2 += len/2.0;
		replotx1 -= len/2.0;

		len = reploty2 - reploty1;
		reploty2 += len/2.0;
		reploty1 -= len/2.0;

		ctx = eval_get_context ();
		replot_functions (ctx);
		eval_free_context (ctx);

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	} else if (response == RESPONSE_ZOOMFIT && replot_in_progress == 0) {
		GelCtx *ctx;
		double size;
		gboolean last_info = genius_setup.info_box;
		gboolean last_error = genius_setup.error_box;
		genius_setup.info_box = TRUE;
		genius_setup.error_box = TRUE;

		size = replot_maxy - replot_miny;
		if (size <= 0)
			size = 1.0;

		reploty1 = replot_miny - size * 0.05;
		reploty2 = replot_maxy + size * 0.05;

		/* sanity */
		if (reploty2 < reploty1)
			reploty2 = reploty1 + 0.1;

		ctx = eval_get_context ();
		replot_functions (ctx);
		eval_free_context (ctx);

		gel_printout_infos ();
		genius_setup.info_box = last_info;
		genius_setup.error_box = last_error;
	}
}

static double button_press_x1 = 0.0;
static double button_press_y1 = 0.0;

static GnomeCanvasItem *zoom_rect = NULL;
static gboolean dragging = FALSE;

/* 10 pixels */
#define MINZOOM 10

static gboolean
mouse_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	double xscale, yscale;

	if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
		dragging = TRUE;
		button_press_x1 = event->button.x;
		button_press_y1 = event->button.y;
	} else if (event->type == GDK_MOTION_NOTIFY && dragging) {
		double x1, x2, y1, y2;
		if (zoom_rect != NULL)
			gtk_object_destroy (GTK_OBJECT (zoom_rect));
		if (event->motion.x < button_press_x1) {
			x1 = event->motion.x;
			x2 = button_press_x1;
		} else {
			x1 = button_press_x1;
			x2 = event->motion.x;
		}

		if (event->motion.y < button_press_y1) {
			y1 = event->motion.y;
			y2 = button_press_y1;
		} else {
			y1 = button_press_y1;
			y2 = event->motion.y;
		}

		if (x2 - x1 < MINZOOM &&
		    y2 - y1 < MINZOOM)
			return FALSE;

		if (x2 - x1 < MINZOOM)
			x2 = x1 + MINZOOM;
		if (y2 - y1 < MINZOOM)
			y2 = y1 + MINZOOM;

		zoom_rect =
			gnome_canvas_item_new (root,
					       gnome_canvas_rect_get_type (),
					       "outline_color", "red",
					       "width_units", 1.0,
					       "x1", x1,
					       "x2", x2,
					       "y1", y1,
					       "y2", y2,
					       NULL);
		g_signal_connect (G_OBJECT (zoom_rect), "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &zoom_rect);
	} else if (event->type == GDK_BUTTON_RELEASE && dragging) {
		double x1, x2, y1, y2;
		GelCtx *ctx;

		dragging = FALSE;

		if (zoom_rect != NULL)
			gtk_object_destroy (GTK_OBJECT (zoom_rect));

		if (event->button.x < button_press_x1) {
			x1 = event->button.x;
			x2 = button_press_x1;
		} else {
			x1 = button_press_x1;
			x2 = event->button.x;
		}

		if (event->button.y < button_press_y1) {
			y1 = event->button.y;
			y2 = button_press_y1;
		} else {
			y1 = button_press_y1;
			y2 = event->button.y;
		}

		if (x2 - x1 < MINZOOM &&
		    y2 - y1 < MINZOOM)
			return FALSE;

		if (x2 - x1 < MINZOOM)
			x2 = x1 + MINZOOM;
		if (y2 - y1 < MINZOOM)
			y2 = y1 + MINZOOM;

		/* get current scale */
		xscale = WIDTH/(replotx2-replotx1);
		yscale = HEIGHT/(reploty2-reploty1);

		/* rescale */
		replotx2 = x2 / xscale + replotx1;
		replotx1 = x1 / xscale + replotx1;
		/* note: y is flipped */
		reploty1 = reploty2 - y2 / yscale;
		reploty2 = reploty2 - y1 / yscale;

		ctx = eval_get_context ();
		replot_functions (ctx);
		eval_free_context (ctx);
	}

	return FALSE;
}

static void
canvas_realize (GtkWidget *win)
{
	GdkCursor *cursor = gdk_cursor_new (GDK_CROSSHAIR);
	gdk_window_set_cursor (win->window, cursor);
	gdk_cursor_unref (cursor);
}

static void
ensure_window (void)
{
	if (graph_window != NULL) {
		/* FIXME: present is evil in that it takes focus away */
		gtk_widget_show (graph_window);
		return;
	}

	graph_window = gtk_dialog_new_with_buttons
		(_("Genius Line Plot") /* title */,
		 GTK_WINDOW (genius_window) /* parent */,
		 0 /* flags */,
		 _("_Zoom out"),
		 RESPONSE_ZOOMOUT,
		 _("_Fit Y Axis"),
		 RESPONSE_ZOOMFIT,
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

	canvas = (GnomeCanvas *)gnome_canvas_new_aa ();
	g_signal_connect (G_OBJECT (canvas),
			  "realize",
			  G_CALLBACK (canvas_realize),
			  NULL);
	root = gnome_canvas_root (canvas);
	g_signal_connect (root, "event",
			  G_CALLBACK (mouse_event),
			  NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (graph_window)->vbox),
			    GTK_WIDGET (canvas), TRUE, TRUE, 0);

	gtk_widget_set_size_request (GTK_WIDGET (canvas), WIDTH, HEIGHT);

	gnome_canvas_set_scroll_region (canvas,
					0 /*x1*/,
					0 /*y1*/,
					WIDTH /*x2*/,
					HEIGHT /*y2*/);

	gnome_canvas_item_new (root,
			       gnome_canvas_rect_get_type (),
			       "fill_color", "white",
			       "x1", 0.0,
			       "x2", (double)WIDTH,
			       "y1", 0.0,
			       "y2", (double)HEIGHT,
			       NULL);

	gtk_widget_show_all (graph_window);
}


static void
clear_graph (void)
{
	if (graph != NULL)
		gtk_object_destroy (GTK_OBJECT (graph));
	graph = (GnomeCanvasGroup *)
		gnome_canvas_item_new (root,
				       gnome_canvas_group_get_type (),
				       "x", 0.0,
				       "y", 0.0,
				       NULL);
	g_signal_connect (G_OBJECT (graph),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &graph);
}

static void
get_start_inc (double start, double end, double *first, double *inc, double *tol)
{
	int incs;
	double len = end-start;

	*inc = pow (10, floor (log10 (len)));
	incs = floor (len / *inc);

	while (incs < 3) {
		*inc /= 2.0;
		incs = floor (len / *inc);
	}

	while (incs > 6) {
		*inc *= 2.0;
		incs = floor (len / *inc);
	}

	*first = ceil (start / *inc) * *inc;

	if (start < 0.0 && end > 0.0) {
		*first = -1.0 * incs * *inc;
		while (*first < start)
			*first += *inc;
	}

	*tol = len / 1000.0;
}


static void
plot_axis (double xscale, double yscale, double x1, double x2, double y1, double y2)
{
	double inc, coord, tol, yleg, xleg;
	GnomeCanvasPoints *points;
	GtkAnchorType anchor, fanchor, canchor, lanchor;
	gboolean printed_zero = FALSE;

	if (x1 <= 0 && x2 >= 0) {
		points = gnome_canvas_points_new (2);
		points->coords[0] = P2C_X (0);
		points->coords[1] = P2C_Y (y1);
		points->coords[2] = P2C_X (0);
		points->coords[3] = P2C_Y (y2);

		gnome_canvas_item_new (graph,
				       gnome_canvas_line_get_type (),
				       "fill_color", "black",
				       "points", points,
				       NULL);

		gnome_canvas_points_unref (points);

		xleg = 0.0;
	} else {
		xleg = x1;
	}

	if (y1 <= 0 && y2 >= 0) {
		points = gnome_canvas_points_new (2);
		points->coords[0] = P2C_X (x1);
		points->coords[1] = P2C_Y (0);
		points->coords[2] = P2C_X (x2);
		points->coords[3] = P2C_Y (0);

		gnome_canvas_item_new (graph,
				       gnome_canvas_line_get_type (),
				       "fill_color", "black",
				       "points", points,
				       NULL);

		gnome_canvas_points_unref (points);

		yleg = 0.0;
	} else {
		yleg = y1;
	}

	/*
	 * X axis labels
	 */
	get_start_inc (x1, x2, &coord, &inc, &tol);

	if (yleg < y1 + (y2-y1) / 8.0) {
		fanchor = GTK_ANCHOR_SW;
		lanchor = GTK_ANCHOR_SE;
		canchor = GTK_ANCHOR_S;
	} else {
		fanchor = GTK_ANCHOR_NW;
		lanchor = GTK_ANCHOR_NE;
		canchor = GTK_ANCHOR_N;
	}

	if (coord - (inc/2.0) <= x1)
		anchor = fanchor;
	else
		anchor = canchor;
	do {
		char buf[20];
		if (fabs (coord) < tol) {
			strcpy (buf, "0");
			printed_zero = TRUE;
		} else {
			g_snprintf (buf, sizeof (buf), "%g", coord);
		}

		gnome_canvas_item_new (graph,
				       gnome_canvas_text_get_type (),
				       "text", buf,
				       "fill_color", "black",
				       "anchor", anchor,
				       "x", P2C_X (coord),
				       "y", P2C_Y (yleg),
				       NULL);

		points = gnome_canvas_points_new (2);
		points->coords[0] = P2C_X (coord);
		points->coords[1] = P2C_Y (yleg)-7;
		points->coords[2] = P2C_X (coord);
		points->coords[3] = P2C_Y (yleg)+7;

		gnome_canvas_item_new (graph,
				       gnome_canvas_line_get_type (),
				       "fill_color", "black",
				       "points", points,
				       NULL);

		gnome_canvas_points_unref (points);
		
		coord += inc;
		if (coord + (inc/2.0) >= x2)
			anchor = lanchor;
		else
			anchor = canchor;
	} while (coord <= x2);

	/*
	 * Y axis labels
	 */
	get_start_inc (y1, y2, &coord, &inc, &tol);

	if (xleg < x1 + (x2-x1) / 5.0) {
		fanchor = GTK_ANCHOR_SW;
		lanchor = GTK_ANCHOR_NW;
		canchor = GTK_ANCHOR_W;
	} else {
		fanchor = GTK_ANCHOR_SE;
		lanchor = GTK_ANCHOR_NE;
		canchor = GTK_ANCHOR_E;
	}

	if (coord - (inc/2.0) <= y1)
		anchor = fanchor;
	else
		anchor = canchor;
	do {
		char buf[20];
		if (fabs (coord) < tol) {
			if (printed_zero)
				buf[0] = '\0';
			else
				strcpy (buf, "0");
		} else {
			g_snprintf (buf, sizeof (buf), "%g", coord);
		}

		if (buf[0] != '\0') {
			gnome_canvas_item_new (graph,
					       gnome_canvas_text_get_type (),
					       "text", buf,
					       "fill_color", "black",
					       "anchor", anchor,
					       "x", P2C_X (xleg),
					       "y", P2C_Y (coord),
					       NULL);

			points = gnome_canvas_points_new (2);
			points->coords[0] = P2C_X (xleg)-7;
			points->coords[1] = P2C_Y (coord);
			points->coords[2] = P2C_X (xleg)+7;
			points->coords[3] = P2C_Y (coord);

			gnome_canvas_item_new (graph,
					       gnome_canvas_line_get_type (),
					       "fill_color", "black",
					       "points", points,
					       NULL);

			gnome_canvas_points_unref (points);
		}
		
		coord += inc;
		if (coord + (inc/2.0) >= y2)
			anchor = lanchor;
		else
			anchor = canchor;
	} while (coord <= y2);
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

static void
plot_line (GnomeCanvasItem **line, GnomeCanvasItem **progress_line,
	   const char *color,
	   GnomeCanvasPoints *points,
	   GnomeCanvasPoints *progress_points,
	   int num)
{
	int old_points = points->num_points;
	points->num_points = num;

	if (*line != NULL)
		gtk_object_destroy (GTK_OBJECT (*line));
	if (*progress_line != NULL)
		gtk_object_destroy (GTK_OBJECT (*progress_line));
	*line = gnome_canvas_item_new (graph,
				       gnome_canvas_line_get_type (),
				       "fill_color", color,
				       "width_units", 1.5,
				       "points", points,
				       NULL);
	if (progress_points != NULL) {
		progress_points->coords[2] = points->coords[(num-1)*2];
		*progress_line = gnome_canvas_item_new
			(graph,
			 gnome_canvas_line_get_type (),
			 "fill_color", color,
			 "width_units", 10.0,
			 "points", progress_points,
			 NULL);
	}

	points->num_points = old_points;
}

static void
plot_func (GelCtx *ctx, GelEFunc *func, const char *color, double xscale, double yscale, double x1, double x2, double y1, double y2)
{
#define PERITER 2
	GnomeCanvasItem *line = NULL;
	GnomeCanvasItem *progress_line = NULL;
	GelETree *arg;
	mpw_t x;
	int cur;
	GnomeCanvasPoints *points = gnome_canvas_points_new (WIDTH/PERITER + 1);
	GnomeCanvasPoints *progress_points = gnome_canvas_points_new (2);
	int i;

	progress_points->coords[0] = 0.0;
	progress_points->coords[1] = (double)HEIGHT;
	progress_points->coords[2] = 0.0;
	progress_points->coords[3] = (double)HEIGHT;

	cur = 0;
	mpw_init (x);
	arg = gel_makenum_use (x);
	for (i = 0; i < WIDTH/PERITER + 1; i++) {
		gboolean ex = FALSE;
		double xd = x1+(i*PERITER)/xscale;
		double y;
		mpw_set_d (arg->val.value, xd);
		y = call_func (ctx, func, arg, &ex);

		if G_UNLIKELY (y > replot_maxy)
			replot_maxy = y;
		if G_UNLIKELY (y < replot_miny)
			replot_miny = y;

		xd = P2C_X (xd);
		y = P2C_Y (y);

		/* FIXME: evil hack for infinity */
		if (y >= HEIGHT*2) {
			y = HEIGHT*2;
		} else if (y <= -HEIGHT) {
			y = -HEIGHT;
		}

		if (ex) {
			if (points != NULL) {
				/* FIXME: what about a single point? */
				if (cur > 1)
					plot_line (&line, &progress_line, color,
						   points, progress_points, cur);
				gnome_canvas_points_unref (points);
				points = NULL;
			}
			line = NULL;
			cur = -1;
		} else {
			if G_UNLIKELY (points == NULL) {
				points = gnome_canvas_points_new (WIDTH/PERITER + 1);
			}
			points->coords[cur*2] = xd;
			points->coords[cur*2 + 1] = y;
		}

		if G_UNLIKELY (points != NULL && i % 40 == 0 && cur > 0) {
			plot_line (&line, &progress_line, color,
				   points, progress_points, cur+1);
		}
		if (evalnode_hook != NULL) {
			(*evalnode_hook)();
			if G_UNLIKELY (interrupted) {
				gel_freetree (arg);
				gnome_canvas_points_unref (points);
				return;
			}
		}

		cur++;
	}
	gel_freetree (arg);

	if (points != NULL && cur > 1)
		plot_line (&line, &progress_line, color,
			   points, NULL, cur);

	if (points != NULL)
		gnome_canvas_points_unref (points);
}

static void
label_func (GelCtx *ctx, int i, GelEFunc *func, const char *color)
{
	char *text;
	GnomeCanvasPoints *points = gnome_canvas_points_new (2);
	points->coords[0] = WIDTH-40;
	points->coords[1] = 20+15*i;
	points->coords[2] = WIDTH-20;
	points->coords[3] = 20+15*i;

	gnome_canvas_item_new (graph,
			       gnome_canvas_line_get_type (),
			       "fill_color", color,
			       "width_units", 1.5,
			       "points", points,
			       NULL);

	gnome_canvas_points_unref (points);

	text = NULL;
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

	gnome_canvas_item_new (graph,
			       gnome_canvas_text_get_type (),
			       "x", (double)(WIDTH-45),
			       "y", (double)(20+15*i),
			       "font", "Monospace 10",
			       "fill_color", "black",
			       "anchor", GTK_ANCHOR_EAST,
			       "text", text,
			       NULL);

	g_free (text);
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
replot_functions (GelCtx *ctx)
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
	double xscale, yscale;
	int i;

	ensure_window ();

	gtk_dialog_set_response_sensitive (GTK_DIALOG (graph_window),
					   RESPONSE_STOP, TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (graph_window),
					   RESPONSE_ZOOMOUT, FALSE);

	replot_in_progress ++;

	clear_graph ();

	/* sanity */
	if (replotx2 == replotx1)
		replotx2 = replotx1 + 0.00000001;
	/* sanity */
	if (reploty2 == reploty1)
		reploty2 = reploty1 + 0.00000001;

	replot_maxy = - G_MAXDOUBLE/2;
	replot_miny = G_MAXDOUBLE/2;

	xscale = WIDTH/(replotx2-replotx1);
	yscale = HEIGHT/(reploty2-reploty1);

	plot_axis (xscale, yscale, replotx1, replotx2, reploty1, reploty2);

	for (i = 0; i < MAXFUNC && replot_func[i] != NULL; i++) {
		plot_func (ctx, replot_func[i], colors[i],
			   xscale, yscale, replotx1, replotx2, reploty1, reploty2);

		if (interrupted) break;

		label_func (ctx, i, replot_func[i], colors[i]);

		if (evalnode_hook)
			(*evalnode_hook)();

		if (interrupted) break;
	}

	if (graph_window != NULL) {
		gtk_dialog_set_response_sensitive (GTK_DIALOG (graph_window),
						   RESPONSE_STOP, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (graph_window),
						   RESPONSE_ZOOMOUT, TRUE);
	}

	replot_in_progress --;
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
	GelCtx *ctx;
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

	replotx1 = x1;
	replotx2 = x2;
	reploty1 = y1;
	reploty2 = y2;

	for (i = 0; i < MAXFUNC && replot_func[i] != NULL; i++) {
		d_freefunc (replot_func[i]);
		replot_func[i] = NULL;
	}

	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		replot_func[i] = func[i];
		func[i] = NULL;
	}

	ctx = eval_get_context ();
	replot_functions (ctx);
	eval_free_context (ctx);

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
		display_error (error_to_print);
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
		 _("_Plot"),
		 RESPONSE_PLOT,
		 GTK_STOCK_CLOSE,
		 GTK_RESPONSE_CLOSE,
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

	for (i = 0; i < MAXFUNC && replot_func[i] != NULL; i++) {
		d_freefunc (replot_func[i]);
		replot_func[i] = NULL;
	}

	for (i = 0; i < MAXFUNC && func[i] != NULL; i++) {
		replot_func[i] = func[i];
		func[i] = NULL;
	}

	replotx1 = x1;
	replotx2 = x2;
	reploty1 = y1;
	reploty2 = y2;

	replot_functions (ctx);

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
