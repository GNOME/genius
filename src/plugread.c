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

/*This file requires gnome, use readplugin_nongnome.c for the simpler, dumber,
  non-gnome version*/

#include "config.h"

#include <gnome.h>
#include <glib.h>
#include <string.h>
#include "calc.h"
#include "plugin.h"
#include "plugread.h"

GelPlugin *
gel_readplugin (const char *dir_name, const char *file_name)
{
	char *p;
	char *name;
	char *file;
	char *copyright;
	char *author;
	char *description;
	gboolean gui;
	gboolean hide;
	GelPlugin *plg;

	p = g_strconcat("=",dir_name,"/",file_name,
			"=/Genius Plugin/",NULL);
	gnome_config_push_prefix(p);
	g_free(p);
	name = gnome_config_get_translated_string("Name");
	file = gnome_config_get_string("File");
	copyright = gnome_config_get_translated_string("Copyright");
	author = gnome_config_get_string("Author");
	description = gnome_config_get_translated_string("Description");
	gui = gnome_config_get_bool("GUI=false");
	hide = gnome_config_get_bool("Hide=false");
	gnome_config_pop_prefix();
	p = g_strconcat("=",dir_name,"/",file_name,
			"=",NULL);
	gnome_config_drop_file(p);
	g_free(p);
	if(!name || !*name || !file || !*file) {
		g_free(name);
		g_free(file);
		g_free(copyright);
		g_free(author);
		g_free(description);
		return NULL;
	}
	plg = g_new0 (GelPlugin, 1);
	plg->name = name;
	plg->base = g_strdup(file_name);
	p = strstr(plg->base,".plugin");
	if(p) *p='\0';
	plg->file = file;
	plg->copyright = copyright;
	plg->author = author;
	plg->description = description;
	plg->gui = gui;
	plg->hide = hide;
	return plg;
}
