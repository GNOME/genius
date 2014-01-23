/* gtkplotdt - delaunay triangulization algorithm for gtk+
 * Copyright 2001  Andy Thaller <thaller@ph.tum.de>
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

#ifndef __GTK_PLOT_DELAUNAY_H__
#define __GTK_PLOT_DELAUNAY_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_PLOT_DT(obj)        G_TYPE_CHECK_INSTANCE_CAST (obj, gtk_plot_dt_get_type (), GtkPlotDT)
#define G_TYPE_PLOT_DT   (gtk_plot_dt_get_type ())

#define GTK_PLOT_DT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, gtk_plot_dt_get_type(), GtkPlotDTClass)
#define GTK_IS_PLOT_DT(obj)     G_TYPE_CHECK_INSTANCE_TYPE (obj, gtk_plot_dt_get_type ())


typedef struct _GtkPlotDTnode GtkPlotDTnode;
typedef struct _GtkPlotDTsegment GtkPlotDTsegment;
typedef struct _GtkPlotDTtriangle GtkPlotDTtriangle;
typedef struct _GtkPlotDT GtkPlotDT;
typedef struct _GtkPlotDTClass GtkPlotDTClass;

/* A 2D-node for the delaunay triangulation */
/**
 * GtkPlotDTnode:
 *
 * The GtkPlotDTnode struct contains only private data.
 * It should only be accessed through the functions described below.
 */
struct _GtkPlotDTnode 
{
  /*< private >*/
  gdouble x, y, z;    /* actual coordinates */
  gdouble px, py, pz;	/* pixel coordinates */
  gint id;            /* some kind of 'meta-data' for external use */
  gint a,b,c,d;       /* neighbour node indices in quadrilateral mode */
  gint boundary_marker;
};

/**
 * GtkPlotDTtriangle:
 *
 * The GtkPlotDTtriangle struct contains only private data.
 * It should only be accessed through the functions described below.
 */
struct _GtkPlotDTtriangle 
{
  /*< private >*/
  gint a, b, c;
  GtkPlotDTnode *na, *nb, *nc;
  double radius;         /* radius-square */
  GtkPlotDTnode ccenter; /* center of circle */
  double area;           /* twice the triangle's area */
  GtkPlotDTnode min,max; /* the bounding box */
  GtkPlotDTtriangle *nn[3]; /* neighbours */
  gboolean visited;	 /* auxiliary variable for sweeping though list */
};

/* a progress indicator function with optional 'cancel' functionality
 * 
 * returns '0' for normal operation
 * or any arbitrary code to request immediate abortion
 */
typedef gint (*GtkPlotDTprogressbarFunc) (double progress);


/* data needed for a delaunay triangulation 
 *
 * the nodes are held in an expanding array (use gtk_plot_dt_expand()!)
 *
 */
struct _GtkPlotDT
{
  GtkObject object;

  gboolean quadrilateral;
  gboolean subsampling;

  gint node_0;               /* lowest node-index (may be negative!) */
  gint node_cnt;             /* number of nodes */
  gint node_max;             /* maximum number of nodes */
  GtkPlotDTnode *nodes;     /* the nodes themselves */
  GtkPlotDTnode *tmp_nodes; /* index<0: tmpnodes[-1-index] */

  GList *triangles;
  GCompareFunc compare_func;

  GtkPlotDTprogressbarFunc pbar;
};

struct _GtkPlotDTClass
{
  GtkObjectClass parent_class;

  gboolean	(* add_node)			(GtkPlotDT *data,
						 GtkPlotDTnode node);
  GtkPlotDTnode*(* get_node)			(GtkPlotDT *data,
						 gint idx);
  gboolean	(* triangulate)			(GtkPlotDT *data);
  void		(* clear)			(GtkPlotDT *data);
};

GType   	gtk_plot_dt_get_type               	(void);
GtkObject*	gtk_plot_dt_new 			(gint num);
void		gtk_plot_dt_set_quadrilateral		(GtkPlotDT *dt, 
							 gboolean set);
void		gtk_plot_dt_set_subsampling		(GtkPlotDT *dt, 
							 gboolean set);
gboolean        gtk_plot_dt_add_node			(GtkPlotDT *data, 
							 GtkPlotDTnode node);
GtkPlotDTnode * gtk_plot_dt_get_node			(GtkPlotDT *data, 
							 gint idx);
gboolean        gtk_plot_dt_triangulate			(GtkPlotDT *data);
void            gtk_plot_dt_clear			(GtkPlotDT *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PLOT_DELAUNAY_H__ */
