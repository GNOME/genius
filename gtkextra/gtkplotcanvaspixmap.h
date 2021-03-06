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

#ifndef __GTK_PLOT_CANVAS_PIXMAP_H__
#define __GTK_PLOT_CANVAS_PIXMAP_H__

#define GTK_PLOT_CANVAS_PIXMAP(obj)        G_TYPE_CHECK_INSTANCE_CAST (obj, gtk_plot_canvas_pixmap_get_type (), GtkPlotCanvasPixmap)
#define GTK_PLOT_CANVAS_PIXMAP_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, gtk_plot_canvas_pixmap_get_type(), GtkPlotCanvasPixmapClass)
#define GTK_IS_PLOT_CANVAS_PIXMAP(obj)     G_TYPE_CHECK_INSTANCE_TYPE (obj, gtk_plot_canvas_pixmap_get_type ())
#define G_TYPE_PLOT_CANVAS_PIXMAP (gtk_plot_canvas_pixmap_get_type ())


#include <gdk/gdk.h>
#include "gtkplotpc.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkPlotCanvasPixmap	GtkPlotCanvasPixmap;
typedef struct _GtkPlotCanvasPixmapClass	GtkPlotCanvasPixmapClass;

/**
 * GtkPlotCanvasPixmap:
 *
 * The GtkPlotCanvasPixmap struct contains only private data.
 * It should only be accessed through the functions described below.
 */
struct _GtkPlotCanvasPixmap
{
  GtkPlotCanvasChild parent;

  cairo_surface_t *pixmap;
  cairo_pattern_t *mask;
};

struct _GtkPlotCanvasPixmapClass
{
  GtkPlotCanvasChildClass parent_class;
};

GType 		gtk_plot_canvas_pixmap_get_type	(void);
GtkPlotCanvasChild * 
		gtk_plot_canvas_pixmap_new(cairo_surface_t *_pixmap, cairo_pattern_t *mask);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_PLOT_CANVAS_PIXMAP_H__ */
