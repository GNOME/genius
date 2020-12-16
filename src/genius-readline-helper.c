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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

static int toplevelokg = TRUE;

static GList *plugins;
static GList *functions;

static int addtoplevels = TRUE;
extern const char *genius_toplevels[];
extern const char *genius_operators[];


static char *
command_generator (const char *text, int state)
{
	static int oi, ti, len;
	static GList *fli = NULL;

	if(!state) {
		oi = 0;
		if(addtoplevels)
			ti = 0;
		else
			ti = -1;
		len = strlen (text);
		fli = functions;
	}
	
	while(ti>=0 && genius_toplevels[ti]) {
		const char *s = genius_toplevels[ti++];
		if(strncmp(s,text,len)==0)
			return strdup(s);
	}

	while(genius_operators[oi]) {
		const char *s = genius_operators[oi++];
		if(strncmp(s,text,len)==0)
			return strdup(s);
	}

	while(fli) {
		char *fid = fli->data;
		fli = g_list_next(fli);
		if(strncmp(fid,text,len)==0)
			return strdup(fid);
	}

	return NULL;
}

static char *
plugin_generator (const char *text, int state)
{
	static int len;
	static GList *li;

	if(!state) {
		len = strlen (text);
		li = plugins;
	}

	while(li) {
		char *pid = li->data;
		li = g_list_next(li);
		if(strncmp(pid,text,len)==0)
			return strdup(pid);
	}

	return NULL;
}

/* Note: keep in sync with inter.c */
/* FIXME: make this common */
static char **
tab_completion (char *text, int start, int end)
{
	char *p;
	for(p=rl_line_buffer;*p==' ' || *p=='\t';p++)
		;
	if(toplevelokg &&
	   (strncmp(p,"load ",5)==0 ||
	    strncmp(p,"load\t",5)==0)) {
		return NULL;
	}
	if(toplevelokg &&
	   (strncmp(p,"ls ",3)==0 ||
	    strncmp(p,"ls\t",3)==0)) {
		return NULL;
	}
	if(toplevelokg &&
	   (strncmp(p,"cd ",3)==0 ||
	    strncmp(p,"cd\t",3)==0)) {
		return NULL;
	}

	if(toplevelokg &&
	   (strncmp(p,"plugin ",7)==0 ||
	    strncmp(p,"plugin\t",7)==0)) {
		return rl_completion_matches (text, plugin_generator);
	}
	
	
	if(toplevelokg &&
	   (!*p ||
	    strncmp(p,"load",strlen(p))==0 ||
	    strncmp(p,"cd",strlen(p))==0 ||
	    strncmp(p,"ls",strlen(p))==0 ||
	    strncmp(p,"pwd",strlen(p))==0 ||
	    strncmp(p,"help",strlen(p))==0 ||
	    strncmp(p,"plugin",strlen(p))==0))
		addtoplevels = TRUE;
	else
		addtoplevels = FALSE;

	return rl_completion_matches (text, command_generator);
}

int
main(int argc, char *argv[])
{
	char buf[4096];
	FILE *infp;
	int outfd;

	if(argc != 3) {
		printf("argc==%d\n",argc);
		puts("SOMETHING IS VERY VERY WRONG WITH GENIUS INSTALLATION!\n"
		     "WRONG NUMBER OF ARGS");
		exit(1);
	}

	infp = fopen (argv[1], "r");
	if (infp == NULL) {
		puts("SOMETHING IS VERY VERY WRONG WITH GENIUS INSTALLATION!\n"
		     "CANNOT OPEN READLINE IN FIFO");
		exit(1);
	}

	outfd = open (argv[2], O_WRONLY);
	if (outfd < 0) {
		puts("SOMETHING IS VERY VERY WRONG WITH GENIUS INSTALLATION!\n"
		     "CANNOT OPEN READLINE OUT FIFO");
		exit(1);
	}

	/* for some reasons we get SIGINT sometimes because of vte? */
	signal (SIGINT, SIG_IGN);

	rl_catch_signals = 1;
	rl_catch_sigwinch = 1;
	rl_terminal_name = "xterm";
	rl_readline_name = "Genius";
	rl_attempted_completion_function =
		(rl_completion_func_t *)tab_completion;

	while(fgets(buf,4096,infp)) {
		int count;
		if(sscanf(buf,"PLUGINS %d\n",&count) == 1) {
			int i;
			GQueue queue = G_QUEUE_INIT;
			if(plugins) {
				g_list_free_full (plugins, g_free);
				plugins = NULL;
			}
			for(i=0;i<count;i++) {
				char *p;
				if(!fgets(buf,4096,infp))
					goto end_with_an_error;
				p = strchr(buf,'\n');
				if(p) *p = '\0';
				g_queue_push_tail (&queue, g_strdup (buf));
			}
			plugins = queue.head;
		} else if(sscanf(buf,"FUNCTIONS %d\n",&count) == 1) {
			int i;
			GQueue queue = G_QUEUE_INIT;
			if(functions) {
				g_list_free_full (functions, g_free);
				functions = NULL;
			}
			for(i=0;i<count;i++) {
				char *p;
				if(!fgets(buf,4096,infp))
					goto end_with_an_error;
				p = strchr(buf,'\n');
				if(p) *p = '\0';
				g_queue_push_tail (&queue, g_strdup (buf));
			}
			functions = queue.head;
		} else if (strncmp (buf, "CWD ", strlen ("CWD "))==0) {
			char *r = strrchr (buf, '\n');
			if (r != NULL)
				*r = '\0';
			if (chdir (&buf[4]) != 0)
				printf ("chdir failed in readline-helper");
		} else if(strcmp(buf,"TOPLEVEL OK\n")==0) {
			toplevelokg = TRUE;
		} else if(strcmp(buf,"TOPLEVEL NOT OK\n")==0) {
			toplevelokg = FALSE;
		} else if(strncmp(buf,"READLINE ",strlen("READLINE "))==0) {
			char *p;
			char *prompt;
			p = strchr(buf,'\n');
			if(p) *p = '\0';
			prompt = &buf[strlen("READLINE ")];
			p = readline(prompt);

			/* Make sure the return gets displayed first, 5ms should not be
			 * noticable, but should be sufficient I hope.  Kind of a hack
			 * here.  With a flush I assume the wait might not be necessary,
			 * but what have we to lose ... */
			fflush(stdout);
			usleep(5000);

			if(p && *p)
				add_history(p);

			if(!p) {
				if (write(outfd,"EOF!",4) < 4)
					printf ("write failed in readline-helper");
			} else {
				int len = strlen(p);
				if (write(outfd,"LINE",4) < 4)
					printf ("write failed in readline-helper");
				if (write(outfd,(gpointer)&len,sizeof(int)) < (int)sizeof (int))
					printf ("write failed in readline-helper");
				if(len>0) {
					if (write(outfd,p,len) < len) {
						printf ("write failed in readline-helper");
					}
				}
				free(p);
			}
		}
	}
end_with_an_error:
	unlink (argv[1]);
	unlink (argv[2]);
	fprintf(stderr,"ERROR! pipe connection broken\n");
	exit(1);
}
