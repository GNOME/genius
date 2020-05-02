/* GNOME dependent routines without use of gnome libs
 *
 * (c) 2000 Eazel, Inc.
 * (c) 2001,2002 George Lebl
 * (c) 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "ve-i18n.h"

#include "ve-misc.h"

char *
ve_find_file (const char *filename, const GList *directories)
{
	char *s;
	const char *path;
	char **pathv;
	int i;

	s = ve_find_file_simple (filename, directories);
	if (s != NULL)
		return s;
	/* if the filename was NULL or absolute
	 * no further checks are made */
	if (filename == NULL ||
	    g_path_is_absolute (filename))
		return NULL;

	/* Can't use gnome_program here */

	path = g_getenv ("GNOME_PATH");
	if (path != NULL) {
		pathv = g_strsplit (path, ":", 0);
		for (i = 0; pathv[i] != NULL; i++) {
			s = g_build_filename (pathv[i], "share", filename, NULL);
			if (access (s, F_OK) == 0) {
				g_strfreev (pathv);
				return s;
			}
			g_free (s);
			s = g_build_filename (pathv[i], "share",
					      g_get_prgname (),
					      filename, NULL);
			if (access (s, F_OK) == 0) {
				g_strfreev (pathv);
				return s;
			}
			g_free (s);
		}
		g_strfreev (pathv);
	}

	return NULL;
}
