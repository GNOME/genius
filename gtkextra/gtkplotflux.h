/* gtkplotflux - 3d scientific plots widget for gtk+
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

#ifndef __GTK_PLOT_FLUX_H__
#define __GTK_PLOT_FLUX_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "gtkplot.h"

#define GTK_PLOT_FLUX(obj)        GTK_CHECK_CAST (obj, gtk_plot_flux_get_type (), GtkPlotFlux)
#define GTK_TYPE_PLOT_FLUX        (gtk_plot_flux_get_type ())
#define GTK_PLOT_FLUX_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_plot_flux_get_type(), GtkPlotFluxClass)
#define GTK_IS_PLOT_FLUX(obj)     GTK_CHECK_TYPE (obj, gtk_plot_flux_get_type ())

typedef struct _GtkPlotFlux             GtkPlotFlux;
typedef struct _GtkPlotFluxClass        GtkPlotFluxClass;


struct _GtkPlotFlux
{
  GtkPlotData data;

  gboolean centered;

  gint arrow_length;
  gint arrow_width;
  GtkPlotSymbolStyle arrow_style;
};

struct _GtkPlotFluxClass
{
  GtkPlotDataClass parent_class;
};


GtkType		gtk_plot_flux_get_type		(void);
GtkWidget*	gtk_plot_flux_new		(void);
void		gtk_plot_flux_set_arrow 	(GtkPlotFlux *flux,
                         			 gint arrow_length,
                         			 gint arrow_width,
                         			 GtkPlotSymbolStyle style);
void		gtk_plot_flux_get_arrow 	(GtkPlotFlux *flux,
                         			 gint *arrow_length,
                         			 gint *arrow_width,
                         			 GtkPlotSymbolStyle *style);
void		gtk_plot_flux_center	 	(GtkPlotFlux *flux,
						 gboolean center);
gboolean	gtk_plot_flux_is_centered 	(GtkPlotFlux *flux);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PLOT_FLUX_H__ */
