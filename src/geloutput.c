/* GENIUS Calculator
 * Copyright (C) 1997-2025 Jiri (George) Lebl
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
#include <stdio.h>

#include "structs.h"

#include "geloutput.h"


int
gel_output_get_columns (GelOutput *gelo)
{
	if (gelo->line_length_get != NULL)
		return gelo->line_length_get(gelo);
	else
		return gelo->line_length;
}

void
gel_output_set_length_limit (GelOutput *gelo, gboolean length_limit)
{
	gelo->length_limit = length_limit;
}

static void
gel_output_putchar(GelOutput *gelo, char ch, gboolean limit, int ll)
{
	if (gelo->outs == NULL) {
		gelo->outs = g_string_new (NULL);
	}

	if (ch == '\n') {
		gelo->inside_escape = FALSE;
		gelo->cur_line_pos = 0;
	} else if(ch=='\e') {
		gelo->inside_escape = TRUE;
	} else if(gelo->inside_escape) {
		if(ch == 'm')
			gelo->inside_escape = FALSE;
	} else {
		gelo->cur_line_pos++;

		if(limit) {
			if(gelo->cur_line_pos > ll-3)
				return;
			if(gelo->cur_line_pos == ll-3) {
				g_string_append (gelo->outs, "...");
				return;
			}
		}
	}

	g_string_append_c(gelo->outs, ch);
}

static void
gel_output_print_string (GelOutput *gelo, const char *string, gboolean limit)
{
	int ll;
	const char *p;

	if (gelo->output_type == GEL_OUTPUT_BLACK_HOLE) {
		if (gelo->notify != NULL &&
		    ! gelo->no_notify)
			gelo->notify (gelo);
		return;
	}

	if (limit && gelo->length_limit) {
		ll = gel_output_get_columns (gelo);	
		if (ll <= 0)
			limit = FALSE;
	} else {
		ll = 0;
		limit = FALSE;
	}

	for (p = string; *p != '\0'; p++) {
		if (*p=='\n') {
			gel_output_putchar (gelo, '\n', limit, ll);
		} else if (limit &&
			   gelo->cur_line_pos > ll &&
			   ! gelo->inside_escape) {
			continue;
		} else if(*p=='\t') {
			int n;
			int left = (8-(gelo->cur_line_pos%8));
			for (n = 0; n < left; n++)
				gel_output_putchar (gelo, ' ', limit, ll);
		} else {
			gel_output_putchar (gelo, *p, limit, ll);
		}
	}

	if (gelo->output_type == GEL_OUTPUT_FILE &&
	    gelo->outs != NULL) {
		g_assert (gelo->outfp != NULL);
		fputs (gelo->outs->str, gelo->outfp);
		g_string_truncate (gelo->outs, 0);
	}

	if (gelo->notify != NULL &&
	    ! gelo->no_notify)
		gelo->notify (gelo);
}

void
gel_output_printf_full(GelOutput *gelo, gboolean limit, const char *format, ...)
{
	gchar *buffer;
	va_list args;

	va_start (args, format);
	buffer = g_strdup_vprintf (format, args);
	va_end (args);

	gel_output_print_string(gelo, buffer, limit);

	g_free(buffer);
}

void
gel_output_printf(GelOutput *gelo, const char *format, ...)
{
	gchar *buffer;
	va_list args;

	va_start (args, format);
	buffer = g_strdup_vprintf (format, args);
	va_end (args);

	gel_output_print_string(gelo, buffer, FALSE);

	g_free(buffer);
}

void
gel_output_string (GelOutput *gelo, const char *string)
{
	g_return_if_fail (gelo != NULL);
	g_return_if_fail (string != NULL);
	
	gel_output_print_string (gelo, string, TRUE);
}

void
gel_output_full_string (GelOutput *gelo, const char *string)
{
	g_return_if_fail (gelo != NULL);
	g_return_if_fail (string != NULL);

	gel_output_print_string (gelo, string, FALSE);
}


GelOutput *
gel_output_new (void)
{
	GelOutput *gelo;

	gelo = g_new0 (GelOutput, 1);
	gelo->ref_count = 1;
	gelo->no_notify = 0;
	gelo->output_type = GEL_OUTPUT_FILE;
	gelo->outfp = stdout;
	gelo->length_limit = TRUE;
	gelo->force_chop = 0;

	return gelo;
}

static void
gel_output_destroy (GelOutput *gelo)
{
	g_return_if_fail (gelo!=NULL);

	if (gelo->outs != NULL)
		g_string_free (gelo->outs, TRUE);

	g_free(gelo);
}

void
gel_output_ref (GelOutput *gelo)
{
	g_return_if_fail (gelo != NULL);

	gelo->ref_count++;
}

void
gel_output_unref (GelOutput *gelo)
{
	g_return_if_fail (gelo != NULL);

	gelo->ref_count--;

	if (gelo->ref_count <= 0)
		gel_output_destroy (gelo);
}

void
gel_output_setup_string (GelOutput *gelo, int line_length,
			 GelOutputLineFunc line_length_get)
{
	g_return_if_fail (gelo!=NULL);

	gelo->output_type = GEL_OUTPUT_STRING;
	gelo->line_length = line_length;
	gelo->line_length_get = line_length_get;
	gelo->outfp = NULL;
	gelo->cur_line_pos = 0;
	gelo->inside_escape = FALSE;

	if (gelo->outs != NULL) {
		g_string_free (gelo->outs, TRUE);
		gelo->outs = NULL;
	}
}

char *
gel_output_snarf_string (GelOutput *gelo)
{
	char *r;

	g_return_val_if_fail (gelo != NULL, NULL);
	g_return_val_if_fail (gelo->output_type == GEL_OUTPUT_STRING, NULL);

	if (gelo->outs == NULL ||
	    gelo->outs->str == NULL ||
	    *(gelo->outs->str) == '\0')
		return NULL;

	r = g_string_free (gelo->outs, FALSE);
	gelo->outs = NULL;

	return r;
}

const char *
gel_output_peek_string (GelOutput *gelo)
{
	g_return_val_if_fail (gelo != NULL, NULL);
	g_return_val_if_fail (gelo->output_type == GEL_OUTPUT_STRING, NULL);

	if (gelo->outs == NULL ||
	    gelo->outs->str == NULL ||
	    *(gelo->outs->str) == '\0')
		return NULL;
	
	return gelo->outs->str;
}

void
gel_output_clear_string (GelOutput *gelo)
{
	g_return_if_fail (gelo != NULL);
	g_return_if_fail (gelo->output_type == GEL_OUTPUT_STRING);

	if (gelo->outs == NULL ||
	    gelo->outs->str == NULL ||
	    *(gelo->outs->str) == '\0')
		return;

	g_string_truncate (gelo->outs, 0);
}

void
gel_output_set_gstring (GelOutput *gelo, GString *gs)
{
	if (gelo->outs != NULL)
		g_string_free (gelo->outs, TRUE);
	gelo->outs = gs;
}

/* file output stuff */
void
gel_output_setup_file (GelOutput *gelo, FILE *outfp, int line_length,
		       GelOutputLineFunc line_length_get)
{
	g_return_if_fail (gelo!=NULL);
	g_return_if_fail (outfp!=NULL);

	gelo->output_type = GEL_OUTPUT_FILE;
	gelo->outfp = outfp;
	gelo->line_length = line_length;
	gelo->line_length_get = line_length_get;
	gelo->cur_line_pos = 0;
	gelo->inside_escape = FALSE;

	if(gelo->outs) {
		g_string_free(gelo->outs, TRUE);
		gelo->outs = NULL;
	}
}

void
gel_output_setup_black_hole (GelOutput *gelo)
{
	g_return_if_fail(gelo!=NULL);

	gelo->output_type = GEL_OUTPUT_BLACK_HOLE;
	gelo->outfp = NULL;
	gelo->line_length = 0;
	gelo->line_length_get = NULL;
	gelo->cur_line_pos = 0;
	gelo->inside_escape = FALSE;

	if(gelo->outs) {
		g_string_free(gelo->outs, TRUE);
		gelo->outs = NULL;
	}
}

/* some accessors */

void
gel_output_get_line_length (GelOutput *gelo,
			    int *line_length,
			    GelOutputLineFunc *line_length_get)
{
	g_return_if_fail(gelo!=NULL);
	
	if(line_length) *line_length = gelo->line_length;
	if(line_length_get) *line_length_get = gelo->line_length_get;
}

void
gel_output_set_line_length (GelOutput *gelo, 
			    int line_length,
			    GelOutputLineFunc line_length_get)
{
	g_return_if_fail(gelo!=NULL);
	g_return_if_fail(line_length>=0);

	gelo->line_length = line_length;
	gelo->line_length_get = line_length_get;
}

void
gel_output_set_notify(GelOutput *gelo,
		      GelOutputNotifyFunc notify)
{
	g_return_if_fail(gelo!=NULL);

	gelo->notify = notify;
}

void
gel_output_set_data(GelOutput *gelo, gpointer data)
{
	g_return_if_fail(gelo!=NULL);

	gelo->data = data;
}

gpointer
gel_output_get_data(GelOutput *gelo)
{
	g_return_val_if_fail(gelo!=NULL, NULL);

	return gelo->data;
}

void
gel_output_flush(GelOutput *gelo)
{
	g_return_if_fail (gelo != NULL);

	if(gelo->output_type == GEL_OUTPUT_FILE) {
		g_assert (gelo->outfp != NULL);
		if (gelo->outs != NULL) {
			fputs (gelo->outs->str, gelo->outfp);
			g_string_truncate (gelo->outs, 0);
		}
		fflush(gelo->outfp);
	}
}

void
gel_output_push_nonotify (GelOutput *gelo)
{
	g_return_if_fail (gelo != NULL);
	gelo->no_notify++;
}

void
gel_output_pop_nonotify (GelOutput *gelo)
{
	g_return_if_fail (gelo != NULL);
	gelo->no_notify--;
	if (gelo->no_notify <= 0 &&
	    gelo->notify != NULL)
		gelo->notify (gelo);
}
