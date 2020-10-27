/* GENIUS Calculator
 * Copyright (C) 1997-2017 Jiri (George) Lebl
 *
 * Author: Jiri (George) Lebl
 *
 * This file is part of Genius.
 *
 * Genius is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <glib.h>
#include <gmodule.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <readline/tilde.h>

#include <vicious.h>

#include "calc.h"
#include "eval.h"
#include "util.h"
#include "dict.h"
#include "funclib.h"
#include "matrixw.h"
#include "compil.h"

#include "mpwrap.h"

#include "parse.h"

#include "plug_api.h"
#include "plugin.h"
#include "plugread.h"

#include "binreloc.h"

GSList *gel_plugin_list = NULL;

static GHashTable *opened = NULL;
static GHashTable *info = NULL;

static void
free_plugin(GelPlugin *plg)
{
	g_free(plg->base);
	g_free(plg->file);
	g_free(plg->name);
	g_free(plg->author);
	g_free(plg->copyright);
	g_free(plg->description);
	g_free(plg);
}

static void
read_plugins_from_dir (const char *dir_name)
{
	DIR *dir;
	struct dirent *dent;

	dir = opendir (dir_name);
	if (dir == NULL) {
		return;
	}
	while((dent = readdir (dir)) != NULL) {
		char *p;
		GelPlugin *plg;
		if(dent->d_name[0] == '.' &&
		   (dent->d_name[1] == '\0' ||
		    (dent->d_name[1] == '.' &&
		     dent->d_name[2] == '\0')))
			continue;
		p = strrchr(dent->d_name,'.');
		if(!p || strcmp(p,".plugin")!=0)
			continue;
		plg = gel_readplugin(dir_name,dent->d_name);
		if(plg) {
			if(plg->gui && !genius_is_gui)
				free_plugin(plg);
			else
				gel_plugin_list = g_slist_prepend(gel_plugin_list,plg);
		}
	}
	closedir (dir);
}

void
gel_read_plugin_list (void)
{
	char *dir_name;
	char *datadir;

	/*free the previous list*/
	g_slist_free_full (gel_plugin_list, (GDestroyNotify)free_plugin);
	gel_plugin_list = NULL;
	
	datadir = gbr_find_data_dir (DATADIR);
	dir_name = g_build_filename (datadir, "genius", "plugins", NULL);
	g_free (datadir);
	read_plugins_from_dir (dir_name);
	g_free (dir_name);

	dir_name = g_build_filename (g_get_home_dir (),
				     ".genius", "plugins", NULL);
	read_plugins_from_dir (dir_name);
	g_free (dir_name);

	gel_plugin_list = g_slist_reverse (gel_plugin_list);
}

static GelPluginInfo *
open_get_info (GelPlugin *plug)
{
	GModule *mod;
	GelPluginInfo *inf;
	if(!opened)
		opened = g_hash_table_new(g_str_hash,g_str_equal);
	if(!info)
		info = g_hash_table_new(NULL,NULL);
	
	if(!(mod=g_hash_table_lookup(opened,plug->file))) {
		char *fname = g_strconcat (plug->file,
					   ".",
					   G_MODULE_SUFFIX,
					   NULL);
		if (access (fname, R_OK) == 0)
			mod = g_module_open (fname,
					     G_MODULE_BIND_LOCAL);
		else if (access (plug->file, R_OK) == 0)
			mod = g_module_open (plug->file,
					     G_MODULE_BIND_LOCAL);
		g_free (fname);
		if (mod == NULL) {
			gel_errorout (_("Can't open plugin!"));
			gel_errorout ("%s", g_module_error ());
		 	return NULL;
		}
		g_module_make_resident(mod);
		g_hash_table_insert(opened,g_strdup(plug->file),mod);
	}
	if(!(inf=g_hash_table_lookup(info,mod))) {
		gpointer f;
		gboolean ret;
		GelPluginInfo *(*the_init_func)(void);
		
		ret = g_module_symbol (mod, "init_func", &f);
		
		if ( ! ret ||
		    f == NULL) {
			gel_errorout (_("Can't initialize plugin!"));
			return NULL;
		}

		the_init_func = f;
		inf = (*the_init_func)();

		if (inf == NULL) {
			gel_errorout (_("Can't initialize plugin!"));
			return NULL;
		}

		g_hash_table_insert (info, mod, inf);
	}

	plug->running = TRUE;

	return inf;
}

void
gel_open_plugin (GelPlugin *plug)
{
	GelPluginInfo *inf;

	inf = open_get_info (plug);

	if (inf != NULL)
		(*inf->open)();
}

static void
restore_plugin (GelPlugin *plug, const char *unique_id)
{
	GelPluginInfo *inf;

	inf = open_get_info (plug);

	if (inf != NULL)
		(*inf->restore_state) (unique_id);
}

static GelPluginInfo *
get_info (GelPlugin *plug)
{
	GModule *mod;
	GelPluginInfo *inf;

	if ( ! plug->running ||
	    opened == NULL ||
	    info == NULL)
		return NULL;

	mod = g_hash_table_lookup (opened, plug->file);
	if (mod == NULL)
		return NULL;

	inf = g_hash_table_lookup (info, mod);

	return inf;
}

void
gel_save_plugins (void)
{
	GSList *li;
	GelPluginInfo *inf;
	VeConfig *cfg;
	char *path;

	/* no plugins */
	if (gel_plugin_list == NULL)
		return;

	if (genius_is_gui)
		path = g_build_filename (g_get_home_dir (),
					 ".genius", "config-gui", NULL);
	else
		path = g_build_filename (g_get_home_dir (),
					 ".genius", "config-cmdline", NULL);
	cfg = ve_config_get (path);
	g_free (path);

	for (li = gel_plugin_list; li != NULL; li = li->next) {
		GelPlugin *plug = li->data;
		gboolean saved = FALSE;
		inf = get_info (plug);
		if (inf != NULL) {
			if (plug->unique_id == NULL) {
				plug->unique_id = g_strdup_printf
					("ID-%08lX%08lX",
					 (unsigned long)g_random_int (),
					 (unsigned long)g_random_int ());
			}
			plug->restore = inf->save_state (plug->unique_id);

			if (plug->restore) {
				char *key = g_strdup_printf
					("plugins/plugin_restore_%s",
					 plug->base);
				ve_config_set_string (cfg, key, plug->unique_id);
				g_free (key);
				saved = TRUE;
			}
		}
		if ( ! saved) {
			char *key = g_strdup_printf
				("plugins/plugin_restore_%s",
				 plug->base);
			ve_config_set_string (cfg, key, "NO");
			g_free (key);
		}
	}
	ve_config_save (cfg, FALSE /* force */);
}

void
gel_restore_plugins (void)
{
	GSList *li;
	VeConfig *cfg;
	char *path;

	/* no plugins */
	if (gel_plugin_list == NULL)
		return;

	if (genius_is_gui) {
		path = g_build_filename (g_get_home_dir (),
					 ".genius", "config-gui", NULL);
		if (access (path, F_OK) != 0) {
			g_free (path);
			path = g_build_filename (g_get_home_dir (),
						 ".gnome2", "genius", NULL);
		}
	} else
		path = g_build_filename (g_get_home_dir (),
					 ".genius", "config-cmdline", NULL);
	cfg = ve_config_get (path);
	g_free (path);

	for (li = gel_plugin_list; li != NULL; li = li->next) {
		GelPlugin *plug = li->data;
		char *key = g_strdup_printf
			("plugins/plugin_restore_%s",
			 plug->base);
		char *unique_id = ve_config_get_string (cfg, key);
		g_free (key);

		if (ve_string_empty (unique_id) ||
		    strcmp (unique_id, "NO") == 0) {
			g_free (unique_id);
			continue;
		}

		restore_plugin (plug, unique_id);

		g_free (unique_id);
	}
}
