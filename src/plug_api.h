/* GENIUS Calculator
 * Copyright (C) 1997-2002 Jiri (George) Lebl
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

#ifndef _PLUG_API_H_
#define _PLUG_API_H_

typedef struct _GelPluginInfo GelPluginInfo;
struct _GelPluginInfo {
	void (*open)(void); /*open the plugin (this happens every
			      time the user selects the menuitem)*/

	gboolean (*save_state) (const char *unique_id);
			    /* return TRUE if genius should reload this
			       plugin next time.  Note that the unique id
			       can be used if ever multiple sessions are
			       implemented */
	void (*restore_state) (const char *unique_id);
};

/*this is here to avoid "prototype warnings", this is a function which the
  plugins should define*/
GelPluginInfo * init_func(void);

#endif
