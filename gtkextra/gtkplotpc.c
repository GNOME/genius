/* gtkplotpc - gtkplot print context - a renderer for printing functions
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: gtkplotpc
 * @short_description: Plot Context
 *
 * Base Class for #GtkPlotCairo, #GtkPlotGdk and #GtkPlotPS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <gtk/gtk.h>

#include "gtkplotpc.h"
#include "gtkplotgdk.h"
#include "gtkplot.h"
#include "gtkpsfont.h"
#include "gtkplotcanvas.h"

static void gtk_plot_pc_class_init                 (GtkPlotPCClass *klass);
static void gtk_plot_pc_real_init                  (GtkPlotPC *pc);

static GtkObjectClass *parent_class = NULL;

GType
gtk_plot_pc_get_type (void)
{
  static GType pc_type = 0;

  if (!pc_type)
    {

      pc_type = g_type_register_static_simple (
		gtk_object_get_type(),
		"GtkPlotPC",
		sizeof (GtkPlotPCClass),
		(GClassInitFunc) gtk_plot_pc_class_init,
		sizeof (GtkPlotPC),
		(GInstanceInitFunc) gtk_plot_pc_real_init,
		0);
    }
  return pc_type;
}

static void
gtk_plot_pc_class_init (GtkPlotPCClass *klass)
{
  parent_class = g_type_class_ref (gtk_object_get_type ());
}

static void
gtk_plot_pc_real_init (GtkPlotPC *pc)
{
  gdk_color_black(gdk_colormap_get_system(), &pc->color);

  pc->width = pc->height = 0;

  pc->init_count = 0;
  pc->use_pixmap = TRUE;
}

GtkObject *
gtk_plot_pc_new				(void)
{
  GtkObject *object;

  object = g_object_new (gtk_plot_pc_get_type(), NULL);
        
  return (object);
}

/**
 * gtk_plot_pc_init:
 * @pc:
 *
 *
 *
 * Return value:
 */
gboolean gtk_plot_pc_init                                   (GtkPlotPC *pc)
{
  pc->init_count++;
  if(pc->init_count > 1) return TRUE;

  return(GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->init(pc));
}

/**
 * gtk_plot_pc_leave:
 * @pc:
 *
 *
 */
void gtk_plot_pc_leave                                  (GtkPlotPC *pc)
{
  pc->init_count--;
  if(pc->init_count > 0) return;

  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->leave(pc);
}

/**
 * gtk_plot_pc_set_viewport:
 * @pc:
 * @w:
 * @h:
 *
 *
 */
void gtk_plot_pc_set_viewport (GtkPlotPC *pc, gdouble w, gdouble h)
{
  pc->width = w;
  pc->height = h;
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->set_viewport(pc, w, h);
}

/**
 * gtk_plot_pc_gsave:
 * @pc:
 *
 *
 */
void gtk_plot_pc_gsave                                  (GtkPlotPC *pc)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->gsave(pc);
}

/**
 * gtk_plot_pc_grestore:
 * @pc:
 *
 *
 */
void gtk_plot_pc_grestore                               (GtkPlotPC *pc)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->grestore(pc);
}

/**
 * gtk_plot_pc_clip:
 * @pc:
 * @area:
 *
 *
 */
void gtk_plot_pc_clip                                   (GtkPlotPC *pc,
                                                         GdkRectangle *area)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->clip(pc, area);
}

/**
 * gtk_plot_pc_clip_mask:
 * @pc:
 * @x:
 * @y:
 * @mask:
 *
 *
 */
void gtk_plot_pc_clip_mask                              (GtkPlotPC *pc,
							 gdouble x,
							 gdouble y,
                                                         GdkBitmap *mask)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->clip_mask(pc, x, y, mask);
}

/**
 * gtk_plot_pc_set_color:
 * @pc:
 * @color:
 *
 *
 */
void gtk_plot_pc_set_color                               (GtkPlotPC *pc,
                                                          GdkColor *color)
{
  pc->color = *color;
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->set_color(pc, color);
}

/**
 * gtk_plot_pc_set_lineattr:
 * @pc:
 * @line_width:
 * @line_style:
 * @cap_style:
 * @join_style:
 *
 *
 */
void gtk_plot_pc_set_lineattr                    (GtkPlotPC *pc,
                                                 gfloat line_width,
                                                 GdkLineStyle line_style,
                                                 GdkCapStyle cap_style,
                                                 GdkJoinStyle join_style)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->set_lineattr(pc, line_width, line_style, cap_style, join_style);
}

/**
 * gtk_plot_pc_set_dash:
 * @pc:
 * @offset_:
 * @values:
 * @num_values:
 *
 *
 */
void gtk_plot_pc_set_dash                                (GtkPlotPC *pc,
                                                         gdouble offset_,
                                                         gdouble *values,
                                                         gint num_values)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->set_dash(pc, offset_, values, num_values);
}

/**
 * gtk_plot_pc_draw_line:
 * @pc:
 * @x1:
 * @y1:
 * @x2:
 * @y2:
 *
 *
 */
void gtk_plot_pc_draw_line                               (GtkPlotPC *pc,
                                                         gdouble x1, gdouble y1,
                                                         gdouble x2, gdouble y2)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_line(pc, x1, y1, x2, y2);
}

/**
 * gtk_plot_pc_draw_lines:
 * @points:
 * @numpoints:
 *
 *
 */
void gtk_plot_pc_draw_lines                              (GtkPlotPC *pc,
                                                         GtkPlotPoint *points,
                                                         gint numpoints)
{
  if(!points || numpoints <= 1) return;
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_lines(pc, points, numpoints);
}

/**
 * gtk_plot_pc_draw_point:
 * @pc:
 * @x:
 * @y:
 *
 *
 */
void gtk_plot_pc_draw_point                             (GtkPlotPC *pc,
                                                         gdouble x, gdouble y)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_point(pc, x, y);
}

/**
 * gtk_plot_pc_draw_rectangle:
 * @pc:
 * @filled:
 * @x:
 * @y:
 * @width:
 * @height:
 *
 *
 */
void gtk_plot_pc_draw_rectangle                          (GtkPlotPC *pc,
                                                         gint filled,
                                                         gdouble x, gdouble y,
                                                         gdouble width,
                                                         gdouble height) 
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_rectangle(pc, filled, x, y, width, height);
}

/**
 * gtk_plot_pc_draw_polygon:
 * @pc:
 * @filled:
 * @points:
 *
 *
 */
void gtk_plot_pc_draw_polygon                            (GtkPlotPC *pc,
                                                         gint filled,
                                                         GtkPlotPoint *points,
                                                         gint numpoints)
{
  if(!points || numpoints < 1) return;
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_polygon(pc, filled, points, numpoints);
}

/**
 * gtk_plot_pc_draw_circle:
 * @pc:
 * @filled:
 * @x:
 * @y:
 * @size:
 *
 *
 */
void gtk_plot_pc_draw_circle                             (GtkPlotPC *pc,
                                                         gint filled,
                                                         gdouble x, gdouble y,
                                                         gdouble size)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_circle(pc, filled, x, y, size);
}

/**
 * gtk_plot_pc_draw_ellipse:
 * @pc:
 * @filled:
 * @x:
 * @y:
 * @width:
 * @height:
 *
 *
 */
void gtk_plot_pc_draw_ellipse                            (GtkPlotPC *pc,
                                                         gint filled,
                                                         gdouble x, gdouble y,
                                                         gdouble width,
                                                         gdouble height) 
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_ellipse(pc, filled, x, y, width, height);
}

/**
 * gtk_plot_pc_set_font:
 * @pc:
 * @psfont:
 * @height:
 *
 *
 */
void gtk_plot_pc_set_font                                (GtkPlotPC *pc,
							 GtkPSFont *psfont,
                                                         gint height)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->set_font(pc, psfont, height);
}

/**
 * gtk_plot_pc_draw_string:
 * @pc:
 * @x:
 * @y:
 * @angle:
 * @fg:
 * @bg:
 * @transparent:
 * @border:
 * @border_space:
 * @border_width:
 * @shadow_width:
 * @font:
 * @height:
 * @just:
 * @text:
 *
 *
 */
void gtk_plot_pc_draw_string                             (GtkPlotPC *pc,
                                                         gint x, gint y,
                                                         gint angle,
                                                         const GdkColor *fg,
                                                         const GdkColor *bg,
                                                         gboolean transparent,
                                                         gint border,
                                                         gint border_space,
                                                         gint border_width,
                                                         gint shadow_width,
                                                         const gchar *font,
                                                         gint height,
                                                         GtkJustification just,
                                                         const gchar *text)

{
  if(!text) return;
  if(strlen(text) == 0) return;

  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_string(pc, x, y, 
                                                       angle,
                                                       fg,
                                                       bg,
                                                       transparent,
                                                       border,
                                                       border_space,
                                                       border_width,
                                                       shadow_width,
                                                       font,
                                                       height,
                                                       just,
                                                       text);

}

/**
 * gtk_plot_pc_draw_pixmap:
 * @pc:
 * @pixmap:
 * @mask:
 * @xsrc:
 * @ysrc:
 * @xdest:
 * @ydest:
 * @width:
 * @height:
 * @scale_x:
 * @scale_y:
 *
 *
 */
void  gtk_plot_pc_draw_pixmap                           (GtkPlotPC *pc,
                                                         GdkPixmap *pixmap,
                                                         GdkBitmap *mask,
                                                         gint xsrc, gint ysrc,
                                                         gint xdest, gint ydest,
                                                         gint width, gint height,
							 gdouble scale_x, gdouble scale_y)
{
  GTK_PLOT_PC_CLASS(GTK_OBJECT_GET_CLASS(GTK_OBJECT(pc)))->draw_pixmap(pc,
                                                        pixmap,
                                                        mask,
                                                        xsrc, ysrc,
                                                        xdest, ydest,
                                                        width, height,
                                                        scale_x, scale_y);
}

