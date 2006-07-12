/* GNOME dependant routines
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
#include <stdlib.h>
#include <locale.h>
#include <libgnome/libgnome.h>

#include "ve-misc.h"

char *
ve_find_file (const char *filename, const GList *directories)
{
	char *s, *ss;
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

	s = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_DATADIR,
				       filename, TRUE, NULL);
	if (s != NULL)
		return s;

	ss = g_build_filename (g_get_prgname (), filename, NULL);
	s = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_DATADIR,
				       ss, TRUE, NULL);
	g_free (ss);
	if (s != NULL)
		return s;

	s = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_DATADIR,
				       filename, TRUE, NULL);
	if (s != NULL)
		return s;

	ss = g_build_filename (g_get_prgname (), filename, NULL);
	s = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_DATADIR,
				       ss, TRUE, NULL);
	g_free (ss);
	if (s != NULL)
		return s;

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

/**
 * ve_i18n_get_language_list:
 * @ignored: Not used anymore (Historically: Name of category to look
 *                 up, e.g. %"LC_MESSAGES").
 * 
 * Computes a list of applicable locale names, which can be used to e.g.
 * construct locale-dependent filenames or search paths. The returned list is
 * sorted from most desirable to least desirable and always contains the
 * default locale "C".
 * 
 * Return value: the list of languages, this list should not be freed as it is
 * owned by ve.
 **/
const GList *
ve_i18n_get_language_list (const gchar *category_name)
{
	static GStaticRecMutex lang_list_lock = G_STATIC_REC_MUTEX_INIT;
	static GList *list = NULL;
	const char * const* langs;
	int i;

	g_static_rec_mutex_lock (&lang_list_lock);

	if (list == NULL) {
		langs = g_get_language_names ();
		for (i = 0; langs[i] != NULL; i++) {
			list = g_list_prepend (list, g_strdup(langs[i]));
		}

		list = g_list_reverse (list);
	}

	g_static_rec_mutex_unlock (&lang_list_lock);

	return list;
}
