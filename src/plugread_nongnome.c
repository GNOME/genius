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

#include <string.h>
#include <glib.h>
#include "calc.h"
#include "plugin.h"
#include "plugread.h"

plugin_t *
readplugin(char *dir_name,char *file_name)
{
	char buf[1024];
	FILE *fp;
	char *p;
	char *file = NULL;
	char *name = NULL;
	char *copyright = NULL;
	char *author = NULL;
	char *description = NULL;
	int gui = FALSE;
	plugin_t *plg;
	p = g_strconcat(dir_name,"/",file_name,NULL);
	fp = fopen(p,"r");
	g_free(p);
	if(!fp) return NULL;
	while(fgets(buf,1024,fp)) {
		g_strchug(buf);
		p = strchr(buf,'=');
		if(!p) continue;
		*p='\0';
		p++;
		g_strchomp(buf);
		if(strcmp(buf,"Name")==0)
			name = g_strdup(g_strstrip(p));
		else if(strcmp(buf,"File")==0)
			file = g_strdup(g_strstrip(p));
		else if(strcmp(buf,"Copyright")==0)
			copyright = g_strdup(g_strstrip(p));
		else if(strcmp(buf,"Author")==0)
			author = g_strdup(g_strstrip(p));
		else if(strcmp(buf,"Description")==0)
			description = g_strdup(g_strstrip(p));
		else if(strcmp(buf,"GUI")==0)
			gui = strcmp(g_strstrip(p),"true")==0;
	}
	fclose(fp);
	
	if(!name || !*name || !file || !*file) {
		g_free(name);
		g_free(file);
		g_free(copyright);
		g_free(author);
		g_free(description);
		return NULL;
	}
	plg = g_new0(plugin_t,1);
	plg->name = name;
	plg->file = file;
	plg->base = g_strdup(file_name);
	p = strstr(plg->base,".plugin");
	if(p) *p='\0';
	plg->copyright = copyright;
	plg->author = author;
	plg->description = description;
	plg->gui = gui;
	return plg;
}
