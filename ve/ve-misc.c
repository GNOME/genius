/* Misc routines
 *
 * (c) 2000 Eazel, Inc.
 * (c) 2001,2002 George Lebl
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

#include "ve-i18n.h"

#include "ve-misc.h"

char **
ve_split (const char *s)
{
	int argc;
	char **argv;

	if (s == NULL)
		return NULL;

	if (g_shell_parse_argv (s, &argc, &argv, NULL)) {
		return argv;
	} else {
		return g_strsplit (s, " ", -1);
	}
}

char *
ve_first_word (const char *s)
{
	int argc;
	char **argv;
	char *ret;

	if (s == NULL)
		return NULL;

	if ( ! g_shell_parse_argv (s, &argc, &argv, NULL)) {
		char *p;
		ret = g_strdup (s);
		p = strchr (ret, ' ');
		if (p != NULL)
			*p = '\0';
		return ret;
	}

	ret = g_strdup (argv[0]);

	g_strfreev (argv);

	return ret;
}

gboolean
ve_first_word_executable (const char *s, gboolean only_existance)
{
	char *bin = ve_first_word (s);
	if (bin == NULL)
		return FALSE;
	if (access (bin, only_existance ? F_OK : X_OK) == 0) {
		g_free (bin);
		return TRUE;
	} else {
		g_free (bin);
		return FALSE;
	}
}

char *
ve_get_first_working_command (const char *list,
			      gboolean only_existance)
{
	int i;
	char **vector;
	char *ret = NULL;

	if (list == NULL)
		return NULL;
       
	vector = g_strsplit (list, ";", -1);
	for (i = 0; vector[i] != NULL; i++) {
		if (ve_first_word_executable (vector[i],
					      only_existance)) {
			ret = g_strdup (vector[i]);
			break;
		}
	}
	g_strfreev (vector);
	return ret;
}

char *
ve_rest (const char *s)
{
	const char *p;
	gboolean single_quot = FALSE;
	gboolean double_quot = FALSE;
	gboolean escape = FALSE;

	if (s == NULL)
		return NULL;

	for (p = s; *p != '\0'; p++) {
		if (single_quot) {
			if (*p == '\'') {
				single_quot = FALSE;
			}
		} else if (escape) {
			escape = FALSE;
		} else if (double_quot) {
			if (*p == '"') {
				double_quot = FALSE;
			} else if (*p == '\\') {
				escape = TRUE;
			}
		} else if (*p == '\'') {
			single_quot = TRUE;
		} else if (*p == '"') {
			double_quot = TRUE;
		} else if (*p == '\\') {
			escape = TRUE;
		} else if (*p == ' ' || *p == '\t') {
			while (*p == ' ' || *p == '\t')
				p++;
			return g_strdup (p);
		}
	}

	return NULL;
}

char **
ve_vector_merge (char * const *v1, char * const *v2)
{
	int len1, argc, i;
	char **argv;

	if (v1 == NULL && v2 == NULL)
		return NULL;

	len1 = ve_vector_len (v1);
	argc = len1 + ve_vector_len (v2);

	argv = g_new (char *, argc+1);
	for (i = 0; i < len1; i++)
		argv[i] = g_strdup (v1[i]);
	for (; i < argc; i++)
		argv[i] = g_strdup (v2[i - len1]);
	argv[i] = NULL;

	return argv;
}

int
ve_vector_len (char * const *v)
{
	int i;
	if (v == NULL)
		return 0;
	for (i = 0; v[i] != NULL; i++)
		;
	return i;
}

gboolean
ve_bool_equal (gboolean a, gboolean b)
{
	if ((a && b) || (!a && !b))
		return TRUE;
	else
		return FALSE;
}

gboolean
ve_is_string_in_list (const GList *list, const char *string)
{
	g_return_val_if_fail (string != NULL, FALSE);

	while (list != NULL) {
		if (list->data != NULL &&
		    strcmp (string, list->data) == 0)
			return TRUE;

		list = list->next;
	}

	return FALSE;
}

gboolean
ve_is_string_in_list_case_no_locale (const GList *list, const char *string)
{
	g_return_val_if_fail (string != NULL, FALSE);

	while (list != NULL) {
		if (list->data != NULL &&
		    g_ascii_strcasecmp (string, list->data) == 0)
			return TRUE;

		list = list->next;
	}

	return FALSE;
}

char *
ve_find_file_simple (const char *filename, const GList *directories)
{
	char *s;
	const GList *li;

	if (filename == NULL)
		return NULL;

	if (access (filename, F_OK) == 0)
		return g_strdup (filename);

	/* an absolute path is just checked */
	if (g_path_is_absolute (filename))
		return NULL;

	for (li = directories; li != NULL; li = li->next) {
		s = g_build_filename (li->data, filename, NULL);
		if (access (s, F_OK) == 0)
			return s;
		g_free (s);
	}

	return NULL;
}

char *
ve_locale_to_utf8 (const char *str)
{
	char *ret = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
	if (ret == NULL) {
		g_warning ("string not in proper locale encoding: \"%s\"", str);
		return g_strdup (str);
	} else {
		return ret;
	}
}

char *
ve_locale_from_utf8 (const char *str)
{
	char *ret = g_locale_from_utf8 (str, -1, NULL, NULL, NULL);
	if (ret == NULL) {
		g_warning ("string not in proper utf8 encoding: \"%s\"", str);
		return g_strdup (str);
	} else {
		return ret;
	}
}

char *
ve_filename_to_utf8 (const char *str)
{
	char *ret = g_filename_to_utf8 (str, -1, NULL, NULL, NULL);
	if (ret == NULL) {
		g_warning ("string not in proper locale encoding: \"%s\"", str);
		return g_strdup (str);
	} else {
		return ret;
	}
}

char *
ve_filename_from_utf8 (const char *str)
{
	char *ret = g_filename_from_utf8 (str, -1, NULL, NULL, NULL);
	if (ret == NULL) {
		g_warning ("string not in proper utf8 encoding: \"%s\"", str);
		return g_strdup (str);
	} else {
		return ret;
	}
}

char *
ve_strftime (struct tm *the_tm, const char *format)
{
	char str[1024];
	char *loc_format = ve_locale_from_utf8 (format);

	/* ignore the format-nonliteral warning here to allow compilation
	 * with warnings as errors */
#ifdef __GNUC__
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#endif
	if (strftime (str, sizeof (str)-1, loc_format, the_tm) == 0) {
		/* according to docs, if the string does not fit, the
		 * contents of str are undefined, thus just use
		 * ??? */
		strcpy (str, "???");
	}
#ifdef __GNUC__
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5)
#pragma GCC diagnostic pop
#endif
#endif

	str [sizeof (str)-1] = '\0'; /* just for sanity */
	g_free (loc_format);

	return ve_locale_to_utf8 (str);
}

pid_t
ve_waitpid_no_signal (pid_t pid, int *status, int options)
{
	pid_t ret;

	for (;;) {
		ret = waitpid (pid, status, options);
		if (ret == 0)
			return 0;
		if (errno != EINTR)
			return ret;
	}
}

gboolean
ve_locale_exists (const char *loc)
{
	gboolean ret;
	char *old = g_strdup (setlocale (LC_MESSAGES, NULL));
	if (setlocale (LC_MESSAGES, loc) != NULL)
		ret = TRUE;
	else
		ret = FALSE;
	setlocale (LC_MESSAGES, old);
	g_free (old);
	return ret;
}

gboolean
ve_is_prog_in_path (const char *prog)
{
	char *full = g_find_program_in_path (prog);
	if (full != NULL) {
		g_free (full);
		return TRUE;
	} else {
		return FALSE;
	}
}

char *
ve_shell_quote_filename (const char *name)
{
	if (name[0] != '-') {
		return g_shell_quote (name);
	} else {
		char *fname = g_strconcat ("./", name, NULL);
		char *quoted = g_shell_quote (fname);
		g_free (fname);
		return quoted;
	}
}
