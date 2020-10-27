/* gtkplotcanvas - gtkplot canvas widget for gtk+
 * Copyright 1999-2001  Adrian E. Feiguin <feiguin@ifir.edu.ar>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include "gtkplot.h"
#include "gtkplotcanvas.h"
#include "gtkplotcanvasellipse.h"
#include "gtkplotps.h"

/**
 * SECTION: gtkplotcanvasellipse
 * @short_description: 
 *
 * FIXME:: need long description
 */


#define DEFAULT_MARKER_SIZE 6
#define P_(string) string

enum {
  ARG_0,
  ARG_LINE,
  ARG_FILLED,
  ARG_BG
};

static void gtk_plot_canvas_ellipse_init	(GtkPlotCanvasEllipse *ellipse);
static void gtk_plot_canvas_ellipse_class_init  (GtkPlotCanvasChildClass *klass);
static void gtk_plot_canvas_ellipse_draw 	(GtkPlotCanvas *canvas,
						 GtkPlotCanvasChild *child);
static void gtk_plot_canvas_ellipse_select	(cairo_t *cr,
						 GtkPlotCanvas *canvas,
						 GtkPlotCanvasChild *child, 
						 GtkAllocation area);
static void gtk_plot_canvas_ellipse_move	(GtkPlotCanvas *canvas,
						 GtkPlotCanvasChild *child,
						 gdouble x, gdouble y);
static void gtk_plot_canvas_ellipse_resize	(GtkPlotCanvas *canvas,
						 GtkPlotCanvasChild *child,
						 gdouble x1, gdouble y1,
						 gdouble x2, gdouble y2);
static void gtk_plot_canvas_ellipse_get_property(GObject      *object,
                                                 guint            prop_id,
                                                 GValue          *value,
                                                 GParamSpec      *pspec);
static void gtk_plot_canvas_ellipse_set_property(GObject      *object,
                                                 guint            prop_id,
                                                 const GValue          *value,
                                                 GParamSpec      *pspec);


gint roundint                     (gdouble x);
static GtkPlotCanvasChildClass *parent_class = NULL;

GType
gtk_plot_canvas_ellipse_get_type (void)
{
  static GType plot_canvas_ellipse_type = 0;

  if (!plot_canvas_ellipse_type)
    {
      plot_canvas_ellipse_type = g_type_register_static_simple (
		gtk_plot_canvas_child_get_type(), 
		"GtkPlotCanvasEllipse",
		sizeof (GtkPlotCanvasEllipseClass),
		(GClassInitFunc) gtk_plot_canvas_ellipse_class_init,
		sizeof (GtkPlotCanvasEllipse),
		(GInstanceInitFunc) gtk_plot_canvas_ellipse_init,
		0);
    }
  return plot_canvas_ellipse_type;
}

GtkPlotCanvasChild*
gtk_plot_canvas_ellipse_new (GtkPlotLineStyle style,
                          gfloat width,
                          const GdkRGBA *fg,
                          const GdkRGBA *bg,
                          gboolean fill)
{
  GtkPlotCanvasEllipse *ellipse;
                                                                                
  ellipse = g_object_new (gtk_plot_canvas_ellipse_get_type (), NULL);
                                   
  ellipse->line.line_width = width;                                             
  if(fg) ellipse->line.color = *fg;
  if(bg) ellipse->bg = *bg;
  ellipse->filled = fill;
                                                                                
  return GTK_PLOT_CANVAS_CHILD (ellipse);
}

static void
gtk_plot_canvas_ellipse_init (GtkPlotCanvasEllipse *ellipse)
{
  gdk_rgba_parse(&ellipse->line.color, "black");
  gdk_rgba_parse(&ellipse->bg, "white");

  ellipse->line.line_style = GTK_PLOT_LINE_SOLID;
  ellipse->line.line_width = 0;
  ellipse->filled = TRUE;
}

static void
gtk_plot_canvas_ellipse_class_init (GtkPlotCanvasChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  parent_class = g_type_class_ref (gtk_plot_canvas_child_get_type ());

  klass->draw = gtk_plot_canvas_ellipse_draw; 
  klass->move = gtk_plot_canvas_ellipse_move; 
  klass->move_resize = gtk_plot_canvas_ellipse_resize; 
  klass->draw_selection = gtk_plot_canvas_ellipse_select; 

  gobject_class->get_property = gtk_plot_canvas_ellipse_get_property;
  gobject_class->set_property = gtk_plot_canvas_ellipse_set_property;
  

  /**
   * GtkPlotCanvasEllipse:line:
   *
   *
   **/                       
  g_object_class_install_property (gobject_class,
                           ARG_LINE,
  g_param_spec_pointer ("line",
                           P_("Line"),
                           P_("Line Attributes"),
                           G_PARAM_READABLE|G_PARAM_WRITABLE));

  /**
   * GtkPlotCanvasEllipse:filled:
   *
   *
   **/    
  g_object_class_install_property (gobject_class,
                           ARG_FILLED,
  g_param_spec_boolean ("filled",
                           P_("Filled"),
                           P_("Fill Figure"),
                           FALSE,
                           G_PARAM_READABLE|G_PARAM_WRITABLE));

  /**
   * GtkPlotCanvasEllipse:color_bg:
   *
   *
   **/    
  g_object_class_install_property (gobject_class,
                           ARG_BG,
  g_param_spec_pointer ("color_bg",
                           P_("Filling Color"),
                           P_("Filling Color"),
                           G_PARAM_READABLE|G_PARAM_WRITABLE));

}

static void
gtk_plot_canvas_ellipse_get_property (GObject      *object,
                                    guint            prop_id,
                                    GValue          *value,
                                    GParamSpec      *pspec)
{
  GtkPlotCanvasEllipse *ellipse = GTK_PLOT_CANVAS_ELLIPSE (object);
                                                                                
  switch(prop_id){
    case ARG_LINE:
      g_value_set_pointer(value, &ellipse->line);
      break;
    case ARG_FILLED:
      g_value_set_boolean(value, ellipse->filled);
      break;
    case ARG_BG:
      g_value_set_pointer(value, &ellipse->bg);
      break;
    default:
      break;
  }
}
                                                                                
static void
gtk_plot_canvas_ellipse_set_property (GObject      *object,
                                    guint            prop_id,
                                    const GValue          *value,
                                    GParamSpec      *pspec)
{
  GtkPlotCanvasEllipse *ellipse = GTK_PLOT_CANVAS_ELLIPSE (object);
                                                                                
  switch(prop_id){
    case ARG_LINE:
      ellipse->line = *((GtkPlotLine *)g_value_get_pointer(value));
      break;
    case ARG_FILLED:
      ellipse->filled = g_value_get_boolean(value);
      break;
    case ARG_BG:
      ellipse->bg = *((GdkRGBA *)g_value_get_pointer(value));
      break;
    default:
      break;
  }
}

static void 
gtk_plot_canvas_ellipse_draw 		(GtkPlotCanvas *canvas,
					 GtkPlotCanvasChild *child)
{
  GtkPlotCanvasEllipse *ellipse = GTK_PLOT_CANVAS_ELLIPSE(child);
  gint width = child->allocation.width;
  gint height = child->allocation.height;

  if(width == 0 && height == 0) return;

  if(ellipse->filled){
     gtk_plot_pc_set_color(canvas->pc, &ellipse->bg);
     gtk_plot_pc_draw_ellipse(canvas->pc, TRUE,
                              child->allocation.x, child->allocation.y, 
                              width, height);
  }
  gtk_plot_canvas_set_line_attributes(canvas, ellipse->line);
  if(ellipse->line.line_style != GTK_PLOT_LINE_NONE)
     gtk_plot_pc_draw_ellipse(canvas->pc, FALSE,
                              child->allocation.x, child->allocation.y, 
                              width, height);

}

static void
draw_marker(GtkPlotCanvas *canvas, cairo_t *cr, gint x, gint y)
{
  cairo_rectangle(cr,
                  x - DEFAULT_MARKER_SIZE / 2, y - DEFAULT_MARKER_SIZE / 2,
                  DEFAULT_MARKER_SIZE + 1, DEFAULT_MARKER_SIZE + 1);
  cairo_fill(cr);
}

static void
gtk_plot_canvas_ellipse_select(cairo_t *cr, GtkPlotCanvas *canvas, GtkPlotCanvasChild *child, GtkAllocation area)
{
  const double dashes[] = { 5., 5. };

  cairo_set_source_rgb(cr, 0, 0, 0);

  cairo_rectangle(cr, area.x, area.y, area.width, area.height);
  cairo_stroke(cr);
  draw_marker(canvas, cr, area.x, area.y);
  draw_marker(canvas, cr, area.x, area.y + area.height);
  draw_marker(canvas, cr, area.x + area.width, area.y);
  draw_marker(canvas, cr, area.x + area.width, area.y + area.height);
  if(area.height > DEFAULT_MARKER_SIZE * 2){
    draw_marker(canvas, cr, area.x, area.y + area.height / 2);
    draw_marker(canvas, cr, area.x + area.width,
                                area.y + area.height / 2);
  }
  if(area.width > DEFAULT_MARKER_SIZE * 2){
    draw_marker(canvas, cr, area.x + area.width / 2, area.y);
    draw_marker(canvas, cr, area.x + area.width / 2,
                                area.y + area.height);
  }

  cairo_set_line_width(cr, 1);
  cairo_set_dash(cr, dashes, 2, 0);
  cairo_save(cr);
  cairo_translate(cr, roundint (area.x) + roundint (area.width) / 2,
                  roundint (area.y) + roundint (area.height) / 2);
  cairo_scale(cr, roundint (area.width) / 2, roundint (area.height) / 2);
  cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
  cairo_restore(cr);
  cairo_stroke(cr);
}


static void 
gtk_plot_canvas_ellipse_move		(GtkPlotCanvas *canvas,
					 GtkPlotCanvasChild *child,
					 gdouble x, gdouble y)
{
  return;
}

static void 
gtk_plot_canvas_ellipse_resize		(GtkPlotCanvas *canvas,
					 GtkPlotCanvasChild *child,
					 gdouble x1, gdouble y1, 
					 gdouble x2, gdouble y2)
{
  return;
}

/**
 * gtk_plot_canvas_ellipse_set_attributes:
 * @ellipse: a #GtkPlotCanvasEllipse widget.
 * @style:
 * @width:
 * @fg:
 * @bg:
 * @fill:
 *
 *
 */
void
gtk_plot_canvas_ellipse_set_attributes	(GtkPlotCanvasEllipse *ellipse,
                                    	 GtkPlotLineStyle style,
					 gdouble width,
                                         const GdkRGBA *fg,
                                         const GdkRGBA *bg,
                                         gboolean fill)
{
  if(fg) ellipse->line.color = *fg;
  if(bg) ellipse->bg = *bg;
  ellipse->line.line_width = width;
  ellipse->line.line_style = style;
  ellipse->filled = fill;
}

