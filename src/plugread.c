/* GENIUS Calculator
 * Copyright (C) 1997-2002,2006 Jiri (George) Lebl
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

#include <glib.h>
#include <string.h>
#include <vicious.h>
#include "calc.h"
#include "plugin.h"
#include "plugread.h"
#include "genius-i18n.h"

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
	VeConfig *cfg;

	p = g_build_filename (ve_sure_string (dir_name),
			      ve_sure_string (file_name), NULL);
	cfg = ve_config_new (p);
	g_free (p);

	name = ve_config_get_translated_string (cfg, "Genius Plugin/Name");
	file = ve_config_get_string (cfg, "Genius Plugin/File");
	copyright = ve_config_get_translated_string (cfg, "Genius Plugin/Copyright");
	author = ve_config_get_string (cfg, "Genius Plugin/Author");
	description = ve_config_get_translated_string (cfg, "Genius Plugin/Description");
	gui = ve_config_get_bool (cfg, "Genius Plugin/GUI=false");
	hide = ve_config_get_bool (cfg, "Genius Plugin/Hide=false");
	ve_config_destroy (cfg);

	if (ve_string_empty (name) ||
	    ve_string_empty (file)) {
		g_free (name);
		g_free (file);
		g_free (copyright);
		g_free (author);
		g_free (description);
		return NULL;
	}
	plg = g_new0 (GelPlugin, 1);
	plg->name = name;
	plg->base = g_strdup (file_name);
	p = strstr (plg->base,".plugin");
	if (p != NULL) *p='\0';
	plg->file = file;
	plg->copyright = copyright;
	plg->author = author;
	plg->description = description;
	plg->gui = gui;
	plg->hide = hide;
	return plg;
}
