/* GENIUS Calculator
 * Copyright (C) 1997-2002 George Lebl
 *
 * Author: George Lebl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the  Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
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

void
gel_read_plugin_list (void)
{
	DIR *dir;
	char *dir_name;
	struct dirent *dent;

	/*free the previous list*/
	g_slist_foreach(gel_plugin_list,(GFunc)free_plugin,NULL);
	g_slist_free(gel_plugin_list);
	gel_plugin_list = NULL;
	
	dir_name = g_strconcat(LIBRARY_DIR,"/plugins",NULL);
	dir = opendir(dir_name);
	if(!dir) {
		g_free(dir_name);
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
	g_free(dir_name);
	gel_plugin_list = g_slist_reverse(gel_plugin_list);
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
		mod = g_module_open(plug->file,G_MODULE_BIND_LAZY);
		if(!mod) {
			(*errorout)(_("Can't open plugin!"));
		 	return NULL;
		}
		g_module_make_resident(mod);
		g_hash_table_insert(opened,g_strdup(plug->file),mod);
	}
	if(!(inf=g_hash_table_lookup(info,mod))) {
		GelPluginInfo *(*init_func)(void);
		
		if(!g_module_symbol(mod,"init_func",(gpointer *)&init_func) ||
		   !init_func || 
		   !(inf=(*init_func)())) {
			(*errorout)(_("Can't initialize plugin!"));
			return NULL;
		}
		g_hash_table_insert(info,mod,inf);
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
	static long int unique_id = 0;
	GSList *li;
	GelPluginInfo *inf;
	VeConfig *cfg;
	char *path;

	path = g_strconcat (ve_sure_string (g_getenv ("HOME")), "/.gnome2/genius", NULL);
	cfg = ve_config_get (path);
	g_free (path);

	if (unique_id == 0)
		unique_id = time (NULL);

	for (li = gel_plugin_list; li != NULL; li = li->next) {
		GelPlugin *plug = li->data;
		gboolean saved = FALSE;
		inf = get_info (plug);
		if (inf != NULL) {
			if (plug->unique_id == NULL) {
				plug->unique_id = g_strdup_printf
					("ID-%ld", unique_id++);
			}
			plug->restore = inf->save_state (plug->unique_id);

			if (plug->restore) {
				char *key = g_strdup_printf
					("/plugins/plugin_restore_%s",
					 plug->base);
				ve_config_set_string (cfg, key, plug->unique_id);
				g_free (key);
				saved = TRUE;
			}
		}
		if ( ! saved) {
			char *key = g_strdup_printf
				("/plugins/plugin_restore_%s",
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

	path = g_strconcat (ve_sure_string (g_getenv ("HOME")), "/.gnome2/genius", NULL);
	cfg = ve_config_get (path);
	g_free (path);

	for (li = gel_plugin_list; li != NULL; li = li->next) {
		GelPlugin *plug = li->data;
		char *key = g_strdup_printf
			("/plugins/plugin_restore_%s",
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
