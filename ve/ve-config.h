/* Config reader routines
 *
 * (c) 2002 George Lebl
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
#ifndef VE_CONFIG_H
#define VE_CONFIG_H

typedef struct _VeConfig VeConfig;

/* read new config or get from disk, this will give you a new
 * private copy that you can do with as you like */
VeConfig *	ve_config_new		(const char *file);
/* get perhaps an already existing object kept in a hash,
 * you should never call destroy on an object like this,
 * note that this automatically does a recheck on an existing
 * file.  If you ever change things, make sure to save them
 * immediately (or use your own private copy) */
VeConfig *	ve_config_get		(const char *file);
/* check for an updated version on disk and if so reread,
 * note that this will forget existing changes unless they
 * were saved */
void		ve_config_recheck	(VeConfig *config);
/* destroy the config object and all data it points to */
void		ve_config_destroy	(VeConfig *config);
/* save the data to disk */
gboolean	ve_config_save		(VeConfig *config,
					 gboolean force);

char *		ve_config_get_translated_string (VeConfig *config,
						 const char *key);
char *		ve_config_get_string	(VeConfig *config,
					 const char *key);
gboolean	ve_config_get_bool	(VeConfig *config,
					 const char *key);
int		ve_config_get_int	(VeConfig *config,
					 const char *key);
void		ve_config_set_string	(VeConfig *config,
					 const char *key,
					 const char *string);
void		ve_config_set_bool	(VeConfig *config,
					 const char *key,
					 gboolean boolean);
void		ve_config_set_int	(VeConfig *config,
					 const char *key,
					 int integer);

/* section == NULL means the 'root' section, that is
   keys with no section */

/* A newly allocated list of section names */
GList *         ve_config_get_sections  (VeConfig *config);
/* A newly allocated list of key names from section */
GList *         ve_config_get_keys  (VeConfig *config,
				     const char *section);
void            ve_config_free_list_of_strings (GList *list);

void		ve_config_delete_section(VeConfig *config,
					 const char *section);
void		ve_config_delete_key	(VeConfig *config,
					 const char *key);
void		ve_config_delete_translations (VeConfig *config,
					       const char *key);

#endif /* VE_CONFIG_H */
