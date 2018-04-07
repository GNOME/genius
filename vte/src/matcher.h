/*
 * Copyright (C) 2002 Red Hat, Inc.
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

#ifndef vte_matcher_h_included
#define vte_matcher_h_included


#include <glib-object.h>
#include "vtetc.h"

G_BEGIN_DECLS

struct _vte_matcher;

struct _vte_matcher_impl {
	const struct _vte_matcher_class *klass;
	/* private */
};

typedef struct _vte_matcher_impl *(*_vte_matcher_create_func)(void);
typedef const char *(*_vte_matcher_match_func)(struct _vte_matcher_impl *impl,
		const gunichar *pattern, gssize length,
		const char **res, const gunichar **consumed,
		GQuark *quark, GValueArray **array);
typedef void (*_vte_matcher_add_func)(struct _vte_matcher_impl *impl,
		const char *pattern, gssize length,
		const char *result, GQuark quark);
typedef void (*_vte_matcher_print_func)(struct _vte_matcher_impl *impl);
typedef void (*_vte_matcher_destroy_func)(struct _vte_matcher_impl *impl);
struct _vte_matcher_class{
	_vte_matcher_create_func create;
	_vte_matcher_add_func add;
	_vte_matcher_print_func print;
	_vte_matcher_match_func match;
	_vte_matcher_destroy_func destroy;
};

/* Create and init matcher. */
struct _vte_matcher *_vte_matcher_new(const char *emulation,
				      struct _vte_termcap *termcap);

/* Free a matcher. */
void _vte_matcher_free(struct _vte_matcher *matcher);

/* Check if a string matches a sequence the matcher knows about. */
const char *_vte_matcher_match(struct _vte_matcher *matcher,
			       const gunichar *pattern, gssize length,
			       const char **res, const gunichar **consumed,
			       GQuark *quark, GValueArray **array);

/* Dump out the contents of a matcher, mainly for debugging. */
void _vte_matcher_print(struct _vte_matcher *matcher);

/* Free a parameter array. */
void _vte_matcher_free_params_array(struct _vte_matcher *matcher, GValueArray *params);

G_END_DECLS

#endif
