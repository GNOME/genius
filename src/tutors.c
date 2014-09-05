/* GENIUS Calculator
 * Copyright (C) 1997-2014 Jiri (George) Lebl
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
#include "compil.h"

#include "mpwrap.h"

#include "parse.h"

#include "tutors.h"

#include "binreloc.h"

GSList *gel_tutor_list = NULL;

static GHashTable *opened = NULL;
static GHashTable *info = NULL;

static GelTutorial *
gel_readtutor (const char *dir_name, const char *file_name)
{
	char *f = g_build_filename (ve_sure_string (dir_name),
				    ve_sure_string (file_name), NULL);
	FILE *fp;
	char buf[512];
	char *name;
	char *cat;
	char *s;
	GelTutorial *tut = NULL;

	fp = fopen (f, "r");

	if (fp == NULL) {
		g_free (f);
		return NULL;
	}

	if (fgets (buf, sizeof (buf), fp) == NULL) {
		g_free (f);
		fclose (fp);
		return NULL;
	}

	s = strchr (buf, ':');
	if (s != NULL) {
		cat = g_strdup (g_strstrip(s+1));
	} else {
		g_free (f);
		fclose (fp);
		return NULL;
	}

	if (fgets (buf, sizeof (buf), fp) == NULL) {
		g_free (f);
		g_free (cat);
		fclose (fp);
		return NULL;
	}

	s = strchr (buf, ':');
	if (s != NULL) {
		name = g_strdup (g_strstrip (s+1));
	} else {
		g_free (f);
		g_free (cat);
		fclose (fp);
		return NULL;
	}

	fclose (fp);

	tut = g_new (GelTutorial, 1);
	tut->category = cat;
	tut->name = name;
	tut->file = f;

	return tut;
}

static void
free_tutor(GelTutorial *tut)
{
	g_free (tut->category);
	g_free (tut->name);
	g_free (tut->file);
	g_free (tut);
}

static void
read_tutors_from_dir (const char *dir_name)
{
	DIR *dir;
	struct dirent *dent;

	dir = opendir (dir_name);
	if (dir == NULL) {
		return;
	}
	while((dent = readdir (dir)) != NULL) {
		char *p;
		GelTutorial *tut;
		if(dent->d_name[0] == '.' &&
		   (dent->d_name[1] == '\0' ||
		    (dent->d_name[1] == '.' &&
		     dent->d_name[2] == '\0')))
			continue;
		p = strrchr(dent->d_name,'.');
		if(!p || strcmp(p,".gel")!=0)
			continue;
		tut = gel_readtutor (dir_name, dent->d_name);
		if (tut != NULL) {
			gel_tutor_list = g_slist_prepend (gel_tutor_list, tut);
		}
	}
	closedir (dir);
}

void
gel_read_tutor_list (void)
{
	char *dir_name;
	char *datadir;

	/*free the previous list*/
	g_slist_foreach (gel_tutor_list, (GFunc)free_tutor, NULL);
	g_slist_free (gel_tutor_list);
	gel_tutor_list = NULL;
	
	datadir = gbr_find_data_dir (DATADIR);
	dir_name = g_build_filename (datadir, "genius", "tutors", NULL);
	g_free (datadir);
	read_tutors_from_dir (dir_name);
	g_free (dir_name);

	dir_name = g_build_filename (g_get_home_dir (),
				     ".genius", "tutors", NULL);
	read_tutors_from_dir (dir_name);
	g_free (dir_name);

	/* FIXME: should do more */
	gel_tutor_list = g_slist_reverse (gel_tutor_list);
}
