/* gtkplotflux - flux plots widget for gtk+
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
#include "gtkplot3d.h"
#include "gtkplotdata.h"
#include "gtkplotflux.h"
#include "gtkpsfont.h"

static void gtk_plot_flux_class_init 	(GtkPlotFluxClass *klass);
static void gtk_plot_flux_init 		(GtkPlotFlux *data);
static void gtk_plot_flux_set_arg       (GtkObject *object,
                                         GtkArg    *arg,
                                         guint      arg_id);
static void gtk_plot_flux_get_arg       (GtkObject *object,
                                         GtkArg    *arg,
                                         guint      arg_id);
static void gtk_plot_flux_draw_legend	(GtkPlotData *data, 
					 gint x, gint y);
static void gtk_plot_flux_draw_symbol	(GtkPlotData *data,
                                         gdouble x, 
					 gdouble y, 
					 gdouble z,
					 gdouble a,
                                         gdouble dx, 
					 gdouble dy, 
					 gdouble dz, 
					 gdouble da);
static void gtk_plot_flux_draw_arrow	(GtkPlotFlux *flux, 
                                         gdouble x1, gdouble y1, 
                                         gdouble x2, gdouble y2);

static gint roundint (gdouble x);

enum { 
  ARG_0,
  ARG_CENTERED,
  ARG_STYLE,
  ARG_WIDTH,
  ARG_LENGTH,
};

static GtkPlotDataClass *parent_class = NULL;

GtkType
gtk_plot_flux_get_type (void)
{
  static GtkType data_type = 0;

  if (!data_type)
    {
      GtkTypeInfo data_info =
      {
	"GtkPlotFlux",
	sizeof (GtkPlotFlux),
	sizeof (GtkPlotFluxClass),
	(GtkClassInitFunc) gtk_plot_flux_class_init,
	(GtkObjectInitFunc) gtk_plot_flux_init,
	/* reserved 1*/ NULL,
        /* reserved 2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      data_type = gtk_type_unique (gtk_plot_data_get_type(), &data_info);
    }
  return data_type;
}

static void
gtk_plot_flux_class_init (GtkPlotFluxClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkPlotDataClass *data_class;

  parent_class = gtk_type_class (gtk_plot_data_get_type ());

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  data_class = (GtkPlotDataClass *) klass;

  object_class->set_arg = gtk_plot_flux_set_arg;
  object_class->get_arg = gtk_plot_flux_get_arg;

  gtk_object_add_arg_type ("GtkPlotFlux::centered",
                           GTK_TYPE_UINT,
                           GTK_ARG_READWRITE,
                           ARG_CENTERED);
  gtk_object_add_arg_type ("GtkPlotFlux::style",
                           GTK_TYPE_UINT,
                           GTK_ARG_READWRITE,
                           ARG_STYLE);
  gtk_object_add_arg_type ("GtkPlotFlux::width",
                           GTK_TYPE_UINT,
                           GTK_ARG_READWRITE,
                           ARG_WIDTH);
  gtk_object_add_arg_type ("GtkPlotFlux::length",
                           GTK_TYPE_UINT,
                           GTK_ARG_READWRITE,
                           ARG_LENGTH);

  data_class->draw_legend = gtk_plot_flux_draw_legend;
  data_class->draw_symbol = gtk_plot_flux_draw_symbol;
}

static void
gtk_plot_flux_set_arg (GtkObject      *object,
                      GtkArg         *arg,
                      guint           arg_id)
{
  GtkPlotFlux *data;

  data = GTK_PLOT_FLUX (object);

  switch (arg_id)
    {
      case ARG_CENTERED:
        data->centered  = GTK_VALUE_UINT(*arg);
        break;
      case ARG_WIDTH:
        data->arrow_width  = GTK_VALUE_UINT(*arg);
        break;
      case ARG_LENGTH:
        data->arrow_length  = GTK_VALUE_UINT(*arg);
        break;
      case ARG_STYLE:
        data->arrow_style  = GTK_VALUE_UINT(*arg);
        break;
    }
}

static void
gtk_plot_flux_get_arg (GtkObject      *object,
                      GtkArg         *arg,
                      guint           arg_id)
{
  GtkPlotFlux *data;

  data = GTK_PLOT_FLUX (object);

  switch (arg_id)
    {
      case ARG_CENTERED:
        GTK_VALUE_UINT(*arg) = data->centered;
        break;
      case ARG_WIDTH:
        GTK_VALUE_UINT(*arg) = data->arrow_width;
        break;
      case ARG_LENGTH:
        GTK_VALUE_UINT(*arg) = data->arrow_length;
        break;
      case ARG_STYLE:
        GTK_VALUE_UINT(*arg) = data->arrow_style;
        break;
      default:
        arg->type = GTK_TYPE_INVALID;
        break;
    }
}

static void
gtk_plot_flux_init (GtkPlotFlux *dataset)
{
  GtkWidget *widget;
  GdkColor black, white;
  GdkColormap *colormap;

  widget = GTK_WIDGET(dataset);

  colormap = gdk_colormap_get_system();

  gdk_color_black(colormap, &black);
  gdk_color_white(colormap, &white);

  GTK_PLOT_DATA(dataset)->symbol.symbol_style = GTK_PLOT_SYMBOL_EMPTY;
  GTK_PLOT_DATA(dataset)->symbol.color = black;
  GTK_PLOT_DATA(dataset)->line.line_style = GTK_PLOT_LINE_SOLID;
  GTK_PLOT_DATA(dataset)->line.line_width = 1;
  GTK_PLOT_DATA(dataset)->line.color = black;

  dataset->centered = TRUE;
  dataset->arrow_length = 8;
  dataset->arrow_width = 8;
  dataset->arrow_style = GTK_PLOT_SYMBOL_FILLED;
}

GtkWidget*
gtk_plot_flux_new ()
{
  GtkWidget *widget;

  widget = gtk_type_new (gtk_plot_flux_get_type ());

  return (widget);
}


static void
gtk_plot_flux_draw_symbol(GtkPlotData *dataset,
                          gdouble x, gdouble y, gdouble z, gdouble a,
                          gdouble dx, gdouble dy, gdouble dz, gdouble da)
{
  GtkPlot *plot;
  GtkPlotFlux *flux = NULL;
  GdkRectangle area, clip_area;
  gdouble pz;
  gdouble m;
  gdouble x1 = 0.0, y1 = 0.0, x2 = 0.0, y2=0.0;

  g_return_if_fail(GTK_IS_PLOT_FLUX(dataset));

  flux = GTK_PLOT_FLUX(dataset);

  g_return_if_fail(dataset->plot != NULL);
  g_return_if_fail(GTK_WIDGET_REALIZED(dataset->plot));

  plot = dataset->plot;

  m = plot->magnification;
  area.x = GTK_WIDGET(plot)->allocation.x;
  area.y = GTK_WIDGET(plot)->allocation.y;
  area.width = GTK_WIDGET(plot)->allocation.width;
  area.height = GTK_WIDGET(plot)->allocation.height;

  clip_area.x = area.x + roundint(plot->x * area.width);
  clip_area.y = area.y + roundint(plot->y * area.height);
  clip_area.width = roundint(plot->width * area.width);
  clip_area.height = roundint(plot->height * area.height);

/*
  gtk_plot_pc_clip(plot->pc, &clip_area);
*/

  if(GTK_IS_PLOT3D(plot)){
       gtk_plot3d_get_pixel(GTK_PLOT3D(plot), x, y, z,
                            &x1, &y1, &pz);
       gtk_plot3d_get_pixel(GTK_PLOT3D(plot), 
                            x + dx * dataset->a_scale, 
			    y + dy * dataset->a_scale, 
			    z + dz * dataset->a_scale, 
                            &x2, &y2, &pz);
  }else{
       if(plot->clip_data && 
          (x < plot->xmin || x > plot->xmax || y <plot->ymin || y > plot->ymax))
            return;
       gtk_plot_get_pixel(plot, x, y, &x1, &y1);
       gtk_plot_get_pixel(plot, 
			  x + dx * dataset->a_scale, y + dy * dataset->a_scale,
			  &x2, &y2);

       gtk_plot_flux_draw_arrow (flux, x1, y1, x2, y2);
       gtk_plot_data_draw_symbol(dataset, x1, y1);
  }

/*
  gtk_plot_pc_clip(plot->pc, NULL);
*/
}

static void
gtk_plot_flux_draw_legend(GtkPlotData *data, gint x, gint y)
{
  GtkPlotFlux *flux;
  GtkPlot *plot = NULL;
  GtkPlotText legend;
  GdkRectangle area;
  gint lascent, ldescent, lheight, lwidth;
  gdouble m;
  gint line_width;

  flux = GTK_PLOT_FLUX(data);

  g_return_if_fail(data->plot != NULL);
  g_return_if_fail(GTK_IS_PLOT(data->plot));
  g_return_if_fail(GTK_WIDGET_REALIZED(data->plot));

  plot = data->plot;
  area.x = GTK_WIDGET(plot)->allocation.x;
  area.y = GTK_WIDGET(plot)->allocation.y;
  area.width = GTK_WIDGET(plot)->allocation.width;
  area.height = GTK_WIDGET(plot)->allocation.height;

  m = plot->magnification;
  legend = plot->legends_attr;

  if(data->legend)
    legend.text = data->legend;
  else
    legend.text = "";

  gtk_plot_text_get_size(legend.text, legend.angle, legend.font,
                         roundint(legend.height * m), 
                         &lwidth, &lheight,
                         &lascent, &ldescent);

  if(data->show_legend){
    gdouble vx;
    line_width = plot->legends_line_width;
  
    legend.x = (gdouble)(area.x + x + roundint(4 + line_width * m))
               / (gdouble)area.width;
    legend.y = (gdouble)(area.y + y + lascent) / (gdouble)area.height;
  
    gtk_plot_draw_text(plot, legend);
  
  
    if(flux->centered)
      vx = x + roundint(line_width / 2.0 * m);
    else{
      vx = x + roundint(m * (data->symbol.size + data->symbol.border.line_width) / 2.0);
      line_width -= roundint(m * (data->symbol.size + data->symbol.border.line_width) / 2.0);
    }
  
    gtk_plot_flux_draw_arrow(flux, 
                             area.x + vx, 
                             area.y + y + (lascent + ldescent) / 2,
                             area.x + vx + roundint(line_width * m),
                             area.y + y + (lascent + ldescent) / 2);
                             
    gtk_plot_data_draw_symbol(data, 
                              area.x + vx, 
                              area.y + y + (lascent + ldescent) / 2);
    y += 2 * lheight;
  } else 
    y += lheight;

  if(data->show_gradient)
    gtk_plot_data_draw_gradient(data, x, y);
}

static void
gtk_plot_flux_draw_arrow(GtkPlotFlux *flux, gdouble x1, gdouble y1, gdouble x2, gdouble y2)
{
  GtkPlot *plot;
  GtkPlotData *data;
  GtkPlotPoint arrow[3];
  gdouble xm, ym;
  gdouble width, height;
  gdouble arrow_width;
  gdouble line_width;
  gdouble angle;
  gdouble length;
  gdouble m;

  data = GTK_PLOT_DATA(flux);
  plot = data->plot;

  m = plot->magnification;

  width = fabs(x2 - x1);
  height = fabs(y2 - y1);

  if(width == 0 && height == 0) return;
  if(width != 0)
      angle = atan2((y2 - y1), (x2 - x1));
  else
      angle = asin((y2 - y1)/height);

  length = (y2 - y1)*(y2 - y1) + (x2 - x1)*(x2 - x1);
  if(length > 0.0) length = sqrt(length);

  arrow_width = flux->arrow_width;
  line_width = data->symbol.border.line_width;
  gtk_plot_pc_set_color(plot->pc, &data->symbol.color);
  gtk_plot_pc_set_lineattr (plot->pc, line_width, 0, 0, 0);
  gtk_plot_pc_set_dash (plot->pc, 0, 0, 0);

  if(flux->centered && width != 0){
    x1 -= cos(angle) * length / 2.0;
    x2 -= cos(angle) * length / 2.0;
  }
  if(flux->centered && height != 0){
    y1 -= sin(angle) * length / 2.0;
    y2 -= sin(angle) * length / 2.0;
  }


  if(flux->arrow_style == GTK_PLOT_SYMBOL_EMPTY)
    gtk_plot_pc_draw_line(plot->pc, x1, y1, x2, y2); 
  else
    gtk_plot_pc_draw_line(plot->pc, x1, y1, 
                          x2 - flux->arrow_length * m * cos(angle) / 2., 
                          y2 - flux->arrow_length * m * sin(angle) / 2.);

  arrow[1].x = x2;
  arrow[1].y = y2;
  xm = x2 - cos(angle) * flux->arrow_length * m;
  ym = y2 - sin(angle) * flux->arrow_length * m;
  arrow[0].x = xm - sin(angle)* arrow_width * m / 2.0;
  arrow[0].y = ym + cos(angle)* arrow_width * m / 2.0;
  arrow[2].x = xm + sin(angle)* arrow_width * m / 2.0;
  arrow[2].y = ym - cos(angle)* arrow_width * m / 2.0;

  switch(flux->arrow_style){
    case GTK_PLOT_SYMBOL_EMPTY:
      gtk_plot_pc_draw_lines (plot->pc, arrow, 3);
      break;
    case GTK_PLOT_SYMBOL_OPAQUE:
      gtk_plot_pc_set_color(plot->pc, &plot->background);
      gtk_plot_pc_draw_polygon (plot->pc, TRUE, arrow, 3);
      gtk_plot_pc_set_color(plot->pc, &data->symbol.color);
      gtk_plot_pc_draw_polygon (plot->pc, FALSE, arrow, 3);
      break;
    case GTK_PLOT_SYMBOL_FILLED:
      gtk_plot_pc_draw_polygon (plot->pc, TRUE, arrow, 3);
  }
}

void
gtk_plot_flux_set_arrow (GtkPlotFlux *flux, 
                         gint arrow_length, 
                         gint arrow_width, 
                         GtkPlotSymbolStyle arrow_style)
{
  flux->arrow_length = arrow_length;
  flux->arrow_width = arrow_width;
  flux->arrow_style = arrow_style;
}

void
gtk_plot_flux_get_arrow (GtkPlotFlux *flux, 
                         gint *arrow_length, 
                         gint *arrow_width, 
                         GtkPlotSymbolStyle *arrow_style)
{
  *arrow_length = flux->arrow_length;
  *arrow_width = flux->arrow_width;
  *arrow_style = flux->arrow_style;
}

void
gtk_plot_flux_center (GtkPlotFlux *flux, gboolean center)
{
  flux->centered = center;
} 

gboolean
gtk_plot_flux_is_centered (GtkPlotFlux *flux)
{
  return(flux->centered);
} 

static gint
roundint (gdouble x)
{
 gint sign = 1;

/* if(x <= 0.) sign = -1; 
*/
 return (x+sign*.50999999471);
}
