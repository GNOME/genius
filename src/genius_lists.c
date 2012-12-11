/* GENIUS Calculator
 * Copyright (C) 1997-2009 Jiri (George) Lebl
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

#define NULL ((void *)0)

const char *genius_toplevels[] = {
	"help",
	"load",
	"cd",
	"pwd",
	"ls",
	"plugin",
	NULL
};
const char *genius_operators[] = {
	"not","and","xor","or","while","until","for","sum","prod","do","to","by","in","if",
	"then","else","function","parameter","call","return","bailout","exception",
	"continue","break","mod","null",
	NULL
};

