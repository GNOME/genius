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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "genius-i18n.h"
#include "util.h"

/*shift the sring to the right by n*/
void
gel_shiftstr(char *s,int n)
{
	char *p;
	if(!s || n<=0)
		return;
	/*copies \0 as well*/
	for(p=s+strlen(s);p>=s;p--)
		*(p+n)=*p;
}

/*allocate new space in s for p and append it*/
char *
gel_appendstr (char *s,const char *p)
{
	if (p == NULL || *p == '\0')
		return s;
	if (s != NULL) {
		s = g_realloc (s, strlen(s)+strlen(p)+1);
		strcat(s,p);
	} else {
		s = g_strdup (p);
	}
	return s;
}

/*allocate new space in s for p and prepend it*/
char *
gel_prependstr(char *s,const char *p)
{
	char *p2;
	if (p == NULL || *p == '\0')
		return s;
	if (s != NULL) {
		p2=(char *)g_malloc(strlen(s)+strlen(p)+1);
		strcpy(p2,p);
		strcat(p2,s);
		g_free(s);
		return p2;
	}
	return g_strdup (p);
}



void
gel_stack_push(GSList **stack, gpointer data)
{
	g_return_if_fail (stack != NULL);
	*stack = g_slist_prepend(*stack,data);
}

gpointer
gel_stack_pop(GSList **stack)
{
	gpointer data;
	GSList *p;

	g_return_val_if_fail (stack != NULL, NULL);

	if(*stack == NULL) {
		g_warning(_("Stack underflow!"));
		return NULL;
	}

	data = (*stack)->data;
	p = (*stack)->next;
	g_slist_free_1(*stack);
	*stack = p;
	return data;
}

gpointer
gel_stack_peek (GSList **stack)
{
	gpointer data;

	g_return_val_if_fail (stack != NULL, NULL);

	if(*stack == NULL) {
		g_warning(_("Stack underflow!"));
		return NULL;
	}

	data = (*stack)->data;
	return data;
}

char *
gel_unescape_string(char *s,char *end,char *nounescape)
{
	char *n;
	char *p;
	
	g_return_val_if_fail(s!=NULL,NULL);
	
	n = p = g_new(char,strlen(s)+1);

	while(*s && (!end || s<end)) {
		if(*s == '\\' && *(s+1) && (!end || (s+1)<end)) {
			switch(*(s+1)) {
			case 'n': *(p++) = '\n'; break;
			case 't': *(p++) = '\t'; break;
			case 'b': *(p++) = '\b'; break;
			case 'r': *(p++) = '\r'; break;
			case 'a': *(p++) = '\a'; break;
			default:
				if(nounescape) {
					if(strchr(nounescape,*(s+1)))
						*(p++) = '\\';
					*(p++) = *(s+1);
				} else 
					*(p++) = *(s+1);
				break;
			}
			s+=2;
		} else
			*(p++)=*(s++);
	}
	*p = '\0';
	return n;
}

/*escape also "'s and \'s*/
char *
gel_escape_string(char *s)
{
	char *n;
	char *p;
	int i;
	
	g_return_val_if_fail(s!=NULL,NULL);
	
	for(i=0,p=s;*p;p++) {
		switch(*p) {
		case '\n':
		case '\t':
		case '\b':
		case '\r':
		case '\a':
		case '"':
		case '\\': i++; break;
		default: ;
		}
	}
	
	n = p = g_new(char,strlen(s)+i+1);

	while(*s) {
		switch(*s) {
		case '\n':
			*(p++) = '\\'; *(p++) = 'n'; s++; break;
		case '\t':
			*(p++) = '\\'; *(p++) = 't'; s++; break;
		case '\b':
			*(p++) = '\\'; *(p++) = 'b'; s++; break;
		case '\r':
			*(p++) = '\\'; *(p++) = 'r'; s++; break;
		case '\a':
			*(p++) = '\\'; *(p++) = 'a'; s++; break;

		case '"':
		case '\\': 
			*(p++) = '\\';
			*(p++)=*(s++);
			break;
		default:
			*(p++)=*(s++);
			 break;
		}
	}
	*p = '\0';
	return n;
}
