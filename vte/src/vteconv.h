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

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_vteconv_h_included
#define vte_vteconv_h_included


#include <glib.h>

G_BEGIN_DECLS

#define VTE_CONV_GUNICHAR_TYPE "X-VTE-GUNICHAR"
#define VTE_INVALID_CONV ((VteConv)-1)

struct _VteConv;
typedef struct _VteConv *VteConv;

VteConv _vte_conv_open(const char *target, const char *source);
size_t _vte_conv(VteConv converter,
		 const guchar **inbuf, gsize *inbytes_left,
		 guchar **outbuf, gsize *outbytes_left);
size_t _vte_conv_uc(VteConv converter,
		    const gunichar **inbuf, gsize *inbytes_left,
		    guchar **outbuf, gsize *outbytes_left);
size_t _vte_conv_uu(VteConv converter,
		    const gunichar **inbuf, gsize *inbytes_left,
		    gunichar **outbuf, gsize *outbytes_left);
size_t _vte_conv_cu(VteConv converter,
		    const guchar **inbuf, gsize *inbytes_left,
		    gunichar **outbuf, gsize *outbytes_left);
gint _vte_conv_close(VteConv converter);

G_END_DECLS

#endif
