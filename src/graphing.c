/* GENIUS Calculator
 * Copyright (C) 2003 George Lebl
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
#include <libgnomecanvas/libgnomecanvas.h>
#include <math.h>

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

#include "graphing.h"

static GtkWidget *graph_window = NULL;
static GnomeCanvas *canvas = NULL;
static GnomeCanvasGroup *root = NULL;
static GnomeCanvasGroup *graph = NULL;

#define WIDTH 640
#define HEIGHT 480

static void
ensure_window (void)
{
	if (graph_window != NULL) {
		/* FIXME: present is evil in that it takes focus away */
		gtk_widget_show (graph_window);
		return;
	}

	graph_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (graph_window),
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &graph_window);

	canvas = (GnomeCanvas *)gnome_canvas_new_aa ();
	root = gnome_canvas_root (canvas);
	gtk_container_add (GTK_CONTAINER (graph_window), GTK_WIDGET (canvas));
	gtk_container_set_border_width (GTK_CONTAINER (graph_window), 10);

	gtk_widget_set_usize (GTK_WIDGET (canvas), WIDTH, HEIGHT);

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

#define P2C_X(x) (((x)-x1)*xscale)
#define P2C_Y(y) ((y2-(y))*yscale)

static void
plot_axis (double xscale, double yscale, double x1, double x2, double y1, double y2)
{
	GnomeCanvasPoints *points = gnome_canvas_points_new (2);
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
}

static double
call_func (GelCtx *ctx, GelEFunc *func, GelETree *arg)
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
	/* FIXME: handle errors! */
	if (ret == NULL || ret->type != VALUE_NODE) {
		gel_freetree (ret);
		return 0;
	}

	retd = mpw_get_double (ret->val.value);
	/* FIXME: handle errors! */
	if (error_num != 0)
		error_num = 0;
	
	gel_freetree (ret);
	return retd;
}

static void
plot_func (GelCtx *ctx, GelEFunc *func, const char *color, double xscale, double yscale, double x1, double x2, double y1, double y2)
{
#define PERITER 2
	GelETree *arg;
	mpw_t x;
	GnomeCanvasPoints *points = gnome_canvas_points_new (WIDTH/PERITER + 1);
	int i;
	mpw_init (x);
	arg = gel_makenum_use (x);
	for (i = 0; i < WIDTH/PERITER + 1; i++) {
		double xd = x1+(i*PERITER)/xscale;
		double y;
		mpw_set_d (arg->val.value, xd);
		y = call_func (ctx, func, arg);
		points->coords[i*2] = P2C_X (xd);
		points->coords[i*2 + 1] = P2C_Y (y);
		/* hack for "infinity" */
		if (points->coords[i*2 + 1] >= HEIGHT*2)
			points->coords[i*2 + 1] = HEIGHT*2;
		/* hack for "infinity" */
		else if (points->coords[i*2 + 1] <= -HEIGHT)
			points->coords[i*2 + 1] = -HEIGHT;
	}
	gel_freetree (arg);

	gnome_canvas_item_new (graph,
			       gnome_canvas_line_get_type (),
			       "fill_color", color,
			       "points", points,
			       NULL);

	gnome_canvas_points_unref (points);
}

#define GET_DOUBLE(var,argnum) \
	{ \
	if (a[argnum]->type != VALUE_NODE) { \
		(*errorout)(_("LinePlot: argument not a number")); \
		return NULL; \
	} \
	var = mpw_get_double (a[argnum]->val.value); \
	}

static GelETree *
LinePlot_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	double x1, x2, y1, y2;
	double xscale, yscale;
	GelEFunc *func[10];
	int funcs = 0;
	int i;
	char *colors[] = {
		"darkblue",
		"darkgreen",
		"red",
		"brown",
		"magenta",
		"black",
		"orange",
		"lightblue",
		"lightgreen",
		"yellow",
		NULL };

	for (i = 0;
	     i < 10 && a[i] != NULL && a[i]->type == FUNCTION_NODE;
	     i++) {
		func[funcs++] = a[i]->func.func;
	}

	if (a[i] != NULL && a[i]->type == FUNCTION_NODE) {
		(*errorout)(_("LinePlot: only up to 10 functions supported"));
		return NULL;
	}

	if (funcs == 0) {
		(*errorout)(_("LinePlot: argument not a function"));
		return NULL;
	}

	/* Defaults */
	x1 = -M_PI;
	x2 = M_PI;
	y1 = -1.1;
	y2 = 1.1;

	if (a[i] != NULL) {
		GET_DOUBLE(x1,i++);
		if (a[i] != NULL) {
			GET_DOUBLE(x2,i++);
			if (a[i] != NULL) {
				GET_DOUBLE(y1,i++);
				if (a[i] != NULL) {
					GET_DOUBLE(y2,i++);
				}
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

	ensure_window ();
	clear_graph ();

	xscale = WIDTH/(x2-x1);
	yscale = HEIGHT/(y2-y1);

	plot_axis (xscale, yscale, x1, x2, y1, y2);

	for (i = 0; i < funcs; i++) {
		plot_func (ctx, func[i], colors[i],
			   xscale, yscale, x1, x2, y1, y2);
	}

	return gel_makenum_null ();
}

void
gel_add_graph_functions (void)
{
	GelEFunc *f;
	/* GelToken *id;*/

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

	VFUNC (LinePlot, 1, "", "plotting", _("Plot a function with a line (very rudimentary).  First argument is the function then optionally limits as x1,x2,y1,y2"));
}
