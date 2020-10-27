/* Config reader routines
 *
 * (c) 2002,2011 George Lebl
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
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "ve-i18n.h"
#include "ve-misc.h"

#include "ve-config.h"

typedef struct _VeSection VeSection;
typedef struct _VeLine VeLine;

enum {
	VE_LINE_TEXT /* comment or some other line */,
	VE_LINE_KEY
};

struct _VeLine {
	int type;
	VeSection *parent;
	char *fullkey; /* key with section name */
	const char *key; /* key without section name, pointer
			    into fullkey, or NULL if of type TEXT */
	char *string;
};

struct _VeSection {
	char *name;
	VeConfig *config;
	GList *lines;
};

struct _VeConfig {
	char *file;
	time_t mtime;
	time_t last_recheck;
	gboolean dirty;

	gboolean hashed;

	VeSection *root;

	GList *sections;

	GHashTable *line_ht;
};

static GHashTable *config_hash = NULL;

static VeSection *
find_section (VeConfig *config, const char *name)
{
	GList *li;

	if (name == NULL) {
		return config->root;
	}

	for (li = config->sections; li != NULL; li = li->next) {
		VeSection *section = li->data;
		if (strcmp (section->name, name) == 0)
			return section;
	}
	return NULL;
}

static void
free_line (VeLine *line)
{
	g_free (line->fullkey);
	line->fullkey = NULL;
	g_free (line->string);
	line->string = NULL;
	g_free (line);
}

/* does not remove from parent->lines */
static void
destroy_line (VeLine *line)
{
	if (line->fullkey != NULL)
		g_hash_table_remove (line->parent->config->line_ht,
				     line->fullkey);

	free_line (line);
}

static void
destroy_section (VeSection *section)
{
	g_free (section->name);
	section->name = NULL;

	g_list_free_full (section->lines, (GDestroyNotify) destroy_line);
	section->lines = NULL;

	g_free (section);
}

static VeSection *
new_section (VeConfig *config, const char *name)
{
	VeSection *section = g_new0 (VeSection, 1);
	section->config = config;
	section->name = g_strdup (name);
	return section;
}

static void
add_text_line (VeConfig *config, VeSection *section, const char *text)
{
	VeLine *line = g_new0 (VeLine, 1);
	line->type = VE_LINE_TEXT;
	line->parent = section;
	line->string = g_strdup (text);

	section->lines = g_list_append (section->lines, line);
}

/* Find first instance of this key where it is commented out, this
   is likely where we want to insert this key */
static GList *
find_commented_out (VeSection *section, VeLine *line)
{
	GList *li;
	int keylen = strlen (line->key);
       
	for (li = section->lines; li != NULL; li = li->next) {
		char *p;
		VeLine *ll = li->data;
		if (ll->type != VE_LINE_TEXT)
			continue;
		p = ll->string;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p != '#')
			continue;
		p++;
		while (*p == ' ' || *p == '\t')
			p++;
		if (strncmp (p, line->key, keylen) == 0 &&
		    (p[keylen] == '[' ||
		     p[keylen] == '=' ||
		     p[keylen] == ' ' ||
		     p[keylen] == '\t'))
			return li;
	}
	return NULL;
}

/* append, but be nice to whitespace and comments */
static void
careful_append (VeSection *section, VeLine *line)
{
	GList *li;
	VeLine *ll;
	gboolean seen_white = FALSE;

	g_assert (line->type == VE_LINE_KEY);

	li = find_commented_out (section, line);
	if (li != NULL) {
		if (li->next == NULL)
			section->lines = g_list_append
				(section->lines, line);
		else
			section->lines = g_list_insert_before
				(section->lines,
				 li->next, line);

		return;
	}

	li = g_list_last (section->lines);
	while (li != NULL) {
		ll = li->data;

		if (ll->type == VE_LINE_TEXT) {
			if (ll->string[0] == '\0') {
				seen_white = TRUE;
			} else if (seen_white) {
				section->lines = g_list_insert_before
					(section->lines,
					 li->next, line);
				return;
			}
		} else {
			if (li->next == NULL)
				section->lines = g_list_append
					(section->lines, line);
			else
				section->lines = g_list_insert_before
					(section->lines,
					 li->next, line);
			return;
		}
		li = li->prev;
	}

	section->lines = g_list_prepend (section->lines, line);
}

static void
add_key_line (VeConfig *config, VeSection *section,
	      const char *key, const char *val,
	      gboolean careful_add)
{
	VeLine *line = g_new0 (VeLine, 1);
	line->type = VE_LINE_KEY;
	line->parent = section;
	if (section->name == NULL) {
		line->fullkey = g_strdup (key);
		line->key = line->fullkey;
	} else {
		line->fullkey = g_strdup_printf ("%s/%s",
						 section->name,
						 key);
		line->key = strchr (line->fullkey, '/');
		line->key ++;
	}
	line->string = g_strdup (val);

	/* rare */
	if (g_hash_table_lookup (config->line_ht, line->fullkey) != NULL) {
		free_line (line);
		return;
	}

	if ( ! careful_add ||
	    section->lines == NULL) {
		section->lines = g_list_append (section->lines, line);
	} else {
		careful_append (section, line);
	}

	g_hash_table_insert (config->line_ht,
			     line->fullkey,
			     line);
}

static time_t
get_mtime (const char *file)
{
	struct stat s;
	if (stat (file, &s) == 0)
		return s.st_mtime;
	else
		return 0;
}

static void
read_config (VeConfig *config)
{
	FILE *fp;
	VeSection *section = config->root;
	char buf[2048];
	int cnt;
	char *getsret;

	config->mtime = get_mtime (config->file);

	VE_IGNORE_EINTR (fp = fopen (config->file, "r"));
	if (fp == NULL)
		return;

	cnt = 0;

	for (;;) {
		char *nows = buf;
		char *eq;
		char *p;

		VE_IGNORE_EINTR (getsret = fgets (buf, sizeof (buf), fp));
		if (getsret == NULL)
			break;

		p = strchr (buf, '\n');
		if (p != NULL)
			*p = '\0';

		while (*nows == ' ' || *nows == '\t')
			nows++;
		if (*nows == '[') {
			p = strchr (nows, ']');

			if (p != NULL)
				*p = '\0';

			nows++;

			section = new_section (config, nows);
			config->sections = g_list_append (config->sections,
							  section);
		} else if (*nows == '\0') {
			add_text_line (config, section, "");
		} else if (*nows == '#') {
			add_text_line (config, section, buf);
		} else if ((eq = strchr (nows, '=')) != NULL) {
			char *val;
			*eq = '\0';
			eq ++;
			val = g_strcompress (eq);
			add_key_line (config, section,
				      nows /* key */,
				      val /* value */,
				      FALSE /* careful_add */);
			g_free (val);
		}
		/* Note: all invalid lines are whacked from the file */

		/* Note this is a hard limit to avoid run away files.
		   Do we really expect files longer then 5000 lines?
		   This is to be ultra anal for security's sake. */
		cnt ++;
		if (cnt > 5000)
			break;
	}

	VE_IGNORE_EINTR (fclose (fp));
}

static VeLine *
find_line (VeConfig *config,
	   const char *key)
{
	char *dkey;
	char *p;
	VeLine *line;

	dkey = g_strdup (key);
	p = strchr (dkey, '=');
	if (p != NULL)
		*p = '\0';

	line = g_hash_table_lookup (config->line_ht, dkey);
	g_free (dkey);
	return line;
}

static VeSection *
find_last_section (VeConfig *config)
{
	GList *li;

	if (config->sections == NULL)
		return config->root;

	li = g_list_last (config->sections);
	return li->data;
}

static VeSection *
find_or_make_section (VeConfig *config,
		      const char *key,
		      char **key_name,
		      gboolean make_new)
{
	char *dkey;
	char *p;
	VeSection *section;
	VeSection *last_sect;

	dkey = g_strdup (key);
	p = strchr (dkey, '=');
	if (p != NULL)
		*p = '\0';
	p = strchr (dkey, '/');
	if (p == NULL) {
		*key_name = dkey;
		/* we're adding to the root which is completely
		   empty so add a blank line to separate it */
		if (make_new && config->root->lines == NULL)
			add_text_line (config, config->root, "");
		return config->root;
	}
	*p = '\0';

	*key_name = g_strdup (p+1);;

	section = find_section (config, dkey);
	if (section != NULL) {
		g_free (dkey);
		return section;
	}

	if ( ! make_new) {
		g_free (dkey);
		return NULL;
	}

	/* Add an empty line on end of last section */
	last_sect = find_last_section (config);
	if (last_sect != config->root ||
	    config->root->lines != NULL)
		add_text_line (config, last_sect, "");

	section = new_section (config, dkey);
	config->sections = g_list_append (config->sections, section);

	g_free (dkey);

	return section;
}

VeConfig *
ve_config_new (const char *file)
{
	VeConfig *config;

	g_assert (file != NULL);

	config = g_new0 (VeConfig, 1);

	config->file = g_strdup (file);
	config->root = new_section (config, NULL);
	config->line_ht = g_hash_table_new (g_str_hash, g_str_equal);

	read_config (config);

	return config;
}

VeConfig *
ve_config_get (const char *file)
{
	VeConfig *config = NULL;

	if (config_hash == NULL)
		config_hash = g_hash_table_new (g_str_hash, g_str_equal);
	else
		config = g_hash_table_lookup (config_hash, file);

	if (config == NULL) {
		config = ve_config_new (file);
		config->hashed = TRUE;
		g_hash_table_insert (config_hash, g_strdup (file), config);
	} else {
		ve_config_recheck (config);
	}

	return config;
}

static void
config_clear (VeConfig *config)
{
	g_free (config->file);
	config->file = NULL;

	destroy_section (config->root);

	g_list_free_full (config->sections, (GDestroyNotify) destroy_section);
	config->sections = NULL;

	g_hash_table_destroy (config->line_ht);
	config->line_ht = NULL;

	memset (config, 0, sizeof (VeConfig));
}

void
ve_config_recheck (VeConfig *config)
{
	time_t curtime = time (NULL);

	g_return_if_fail (config != NULL);

	/* don't check more then once a second,
	 * it would be pointless anyway */
	if (config->last_recheck == curtime)
		return;

	if (get_mtime (config->file) != config->mtime) {
		char *file = g_strdup (config->file);

		/* clear all data and null the structure */
		config_clear (config);

		/* reset the config file */
		config->file = file;
		config->root = new_section (config, NULL);
		config->line_ht = g_hash_table_new (g_str_hash, g_str_equal);

		/* read it again */
		read_config (config);
	}

	config->last_recheck = curtime;
}

void
ve_config_destroy (VeConfig *config)
{
	g_return_if_fail (config != NULL);
	g_return_if_fail ( ! config->hashed);

	config_clear (config);

	g_free (config);
}

static void
save_section (VeSection *section, FILE *fp)
{
	GList *li;
	for (li = section->lines; li != NULL; li = li->next) {
		VeLine *line = li->data;
		if (line->type == VE_LINE_TEXT) {
			VE_IGNORE_EINTR (fprintf (fp, "%s\n", line->string));
		} else if (line->type == VE_LINE_KEY) {
			char *out = g_strescape (ve_sure_string (line->string),
						 G_CSET_LATINC G_CSET_LATINS);
			VE_IGNORE_EINTR (fprintf (fp, "%s=%s\n",
						  line->key,
						  out));
			g_free (out);
		}
	}
}

gboolean
ve_config_save (VeConfig *config, gboolean force)
{
	FILE *fp;
	GList *li;

	g_return_val_if_fail (config != NULL, FALSE);

	if ( ! force && ! config->dirty)
		return TRUE;

	VE_IGNORE_EINTR (fp = fopen (config->file, "w"));
	if (fp == NULL)
		return FALSE;

	save_section (config->root, fp);

	for (li = config->sections; li != NULL; li = li->next) {
		VeSection *section = li->data;
		VE_IGNORE_EINTR (fprintf (fp, "[%s]\n", section->name));
		save_section (section, fp);
	}

	VE_IGNORE_EINTR (fclose (fp));

	/* update the mtime */
	config->mtime = get_mtime (config->file);
	config->dirty = FALSE;

	return TRUE;
}

char *
ve_config_get_translated_string (VeConfig *config,
				 const char *key)
{
	char *dkey;
	char *def;
	VeLine *line = NULL;
	char *ret;
	const char * const* langs;
	int i;

	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	dkey = g_strdup (key);
	def = strchr (dkey, '=');
	if (def != NULL) {
		*def = '\0';
		def++;
	}

	langs = g_get_language_names ();
	for (i = 0; langs[i] != NULL; i++) {
		char *full = g_strdup_printf ("%s[%s]", dkey, langs[i]);
		line = g_hash_table_lookup (config->line_ht, full);
		g_free (full);
		if (line != NULL)
			break;
	}
	if (line == NULL)
		line = g_hash_table_lookup (config->line_ht, dkey);

	if (line != NULL)
		ret = g_strdup (line->string);
	else
		ret = g_strdup (def);
	g_free (dkey);
	return ret;
}

char *
ve_config_get_string (VeConfig *config,
		      const char *key)
{
	char *dkey;
	char *def;
	VeLine *line = NULL;
	char *ret;

	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	dkey = g_strdup (key);
	def = strchr (dkey, '=');
	if (def != NULL) {
		*def = '\0';
		def++;
	}

	line = g_hash_table_lookup (config->line_ht, dkey);

	if (line != NULL)
		ret = g_strdup (line->string);
	else
		ret = g_strdup (def);

	g_free (dkey);
	return ret;
}

gboolean
ve_config_get_bool (VeConfig *config,
		    const char *key)
{
	char *val;

	g_return_val_if_fail (config != NULL, FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	val = ve_config_get_string (config, key);
	if (val != NULL &&
	    (val[0] == 'T' ||
	     val[0] == 't' ||
	     val[0] == 'Y' ||
	     val[0] == 'y' ||
	     atoi (val) != 0)) {
		g_free (val);
		return TRUE;
	} else {
		g_free (val);
		return FALSE;
	}
}

int
ve_config_get_int (VeConfig *config,
		   const char *key)
{
	char *val;
	int ret;

	g_return_val_if_fail (config != NULL, 0);
	g_return_val_if_fail (key != NULL, 0);

	val = ve_config_get_string (config, key);
	if (val == NULL)
		return 0;
	ret = atoi (val);
	g_free (val);
	return ret;
}

void
ve_config_set_string (VeConfig *config,
		      const char *key,
		      const char *string)
{
	VeLine *line;
	VeSection *section;
	char *key_name = NULL;

	g_return_if_fail (config != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (string != NULL);

	config->dirty = TRUE;

	line = find_line (config, key);
	if (line != NULL) {
		g_free (line->string);
		line->string = g_strdup (string);
		return;
	}

	section = find_or_make_section (config, key, &key_name,
					TRUE /* make_new */);
	add_key_line (config, section, key_name, string,
		      TRUE /* careful_add */);
	g_free (key_name);
}

void
ve_config_set_bool (VeConfig *config,
		    const char *key,
		    gboolean boolean)
{
	VeLine *line;
	VeSection *section;
	char *key_name = NULL;

	g_return_if_fail (config != NULL);
	g_return_if_fail (key != NULL);

	config->dirty = TRUE;

	line = find_line (config, key);
	if (line != NULL) {
		g_free (line->string);
		line->string = g_strdup (boolean ? "true" : "false");
		return;
	}

	section = find_or_make_section (config, key, &key_name,
					TRUE /* make_new */);
	add_key_line (config, section, key_name, boolean ? "true" : "false",
		      TRUE /* careful_add */);
	g_free (key_name);
}

void
ve_config_set_int (VeConfig *config,
		   const char *key,
		   int integer)
{
	VeLine *line;
	VeSection *section;
	char *key_name = NULL;
	char *num;

	g_return_if_fail (config != NULL);
	g_return_if_fail (key != NULL);

	config->dirty = TRUE;

	num = g_strdup_printf ("%d", integer);

	line = find_line (config, key);
	if (line != NULL) {
		g_free (line->string);
		line->string = num;
		return;
	}

	section = find_or_make_section (config, key, &key_name,
					TRUE /* make_new */);
	add_key_line (config, section, key_name, num,
		      TRUE /* careful_add */);
	g_free (key_name);
	g_free (num);
}

void
ve_config_delete_section (VeConfig *config,
			  const char *section)
{
	VeSection *sec;

	g_return_if_fail (config != NULL);
	g_return_if_fail (section != NULL);

	if (section == NULL) {
		config->dirty = TRUE;

		destroy_section (config->root);
		config->root = new_section (config, NULL);
		return;
	}

	sec = find_section (config, section);
	if (sec != NULL) {
		config->dirty = TRUE;

		config->sections = g_list_remove (config->sections, sec);
		destroy_section (sec);
	}
}

void
ve_config_delete_key (VeConfig *config,
		      const char *key)
{
	VeLine *line = NULL;

	g_return_if_fail (config != NULL);
	g_return_if_fail (key != NULL);

	line = find_line (config, key);
	if (line != NULL) {
		config->dirty = TRUE;

		line->parent->lines = g_list_remove (line->parent->lines,
						     line);
		destroy_line (line);
	}
}

void
ve_config_delete_translations (VeConfig *config, const char *key)
{
	VeSection *section;
	char *key_name;
	int len;
	GList *lines, *li;

	g_return_if_fail (config != NULL);
	g_return_if_fail (key != NULL);

	section = find_or_make_section (config, key, &key_name,
					FALSE /* make_new */);
	if (section == NULL) {
		g_free (key_name);
		return;
	}

	len = strlen (key_name);
	lines = g_list_copy (section->lines);
	for (li = lines; li != NULL; li = li->next) {
		VeLine *line = li->data;
		if (line->type == VE_LINE_KEY &&
		    strncmp (line->key, key_name, len) == 0 &&
		    line->key[len] == '[') {
			config->dirty = TRUE;

			section->lines = g_list_remove (section->lines, line);
			destroy_line (line);
		}
	}
	g_list_free (lines);
}

GList *
ve_config_get_sections (VeConfig *config)
{
	GList *li;
	GQueue queue = G_QUEUE_INIT;

	g_return_val_if_fail (config != NULL, NULL);

	for (li = config->sections; li != NULL; li = li->next) {
		VeSection *section = li->data;
		g_queue_push_tail (&queue, g_strdup (section->name));
	}
	return queue.head;
}

GList *
ve_config_get_keys (VeConfig *config, const char *section)
{
	VeSection *sec;
	GList *li;
	GQueue queue = G_QUEUE_INIT;

	g_return_val_if_fail (config != NULL, NULL);
	g_return_val_if_fail (section != NULL, NULL);

	sec = find_section (config, section);
	if (sec == NULL)
		return NULL;

	for (li = sec->lines; li != NULL; li = li->next) {
		VeLine *ll = li->data;
		if (ll->type == VE_LINE_KEY &&
		    ! ve_string_empty (ll->key))
			g_queue_push_tail (&queue, g_strdup (ll->key));
	}
	return queue.head;
}

void
ve_config_free_list_of_strings (GList *list)
{
	GList *li;

	if (list == NULL)
		return;

	for (li = list; li != NULL; li = li->next) {
		g_free (li->data);
		li->data = NULL;
	}

	g_list_free (list);
}

