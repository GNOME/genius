/*
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef vte_vtebg_included
#define vte_vtebg_included

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VTE_TYPE_BG            (vte_bg_get_type())
#define VTE_BG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VTE_TYPE_BG, VteBg))
#define VTE_BG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VTE_TYPE_BG, VteBgClass))
#define VTE_IS_BG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VTE_TYPE_BG))
#define VTE_IS_BG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VTE_TYPE_BG))
#define VTE_BG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VTE_TYPE_BG, VteBgClass))

typedef struct _VteBg         VteBg;
typedef struct _VteBgPrivate  VteBgPrivate;
typedef struct _VteBgClass    VteBgClass;

struct _VteBg {
	GObject parent;

        /*< private >*/
	VteBgPrivate *pvt;
};

struct _VteBgClass {
	GObjectClass parent_class;
};

GType vte_bg_get_type(void);

VteBg *vte_bg_get_for_screen(GdkScreen *screen);

typedef enum {
	VTE_BG_SOURCE_NONE,
	VTE_BG_SOURCE_ROOT,
	VTE_BG_SOURCE_PIXBUF,
	VTE_BG_SOURCE_FILE
} VteBgSourceType;

cairo_surface_t *
vte_bg_get_surface(VteBg *bg,
		   VteBgSourceType source_type,
		   GdkPixbuf *source_pixbuf,
		   const char *source_file,
		   const PangoColor *tint,
		   double saturation,
		   cairo_surface_t *other);

G_END_DECLS

#endif
