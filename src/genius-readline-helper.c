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
extern const char *genius_params[];
extern const char *genius_toplevels[];
extern const char *genius_operators[];


static char *
command_generator (const char *text, int state)
{
	static int oi,ti,pi,len;
	static GList *fli = NULL;

	if(!state) {
		oi = 0;
		pi = 0;
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

	while(genius_params[pi]) {
		const char *s = genius_params[pi++];
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

	rl_catch_signals = 1;
	rl_catch_sigwinch = 1;
	rl_terminal_name = "xterm";
	rl_readline_name = "Genius";
	rl_attempted_completion_function =
		(CPPFunction *)tab_completion;

	while(fgets(buf,4096,infp)) {
		int count;
		if(sscanf(buf,"PLUGINS %d\n",&count) == 1) {
			int i;
			if(plugins) {
				g_list_foreach(plugins,(GFunc)g_free,NULL);
				g_list_free(plugins);
				plugins = NULL;
			}
			for(i=0;i<count;i++) {
				char *p;
				if(!fgets(buf,4096,infp))
					goto end_with_an_error;
				p = strchr(buf,'\n');
				if(p) *p = '\0';
				plugins = g_list_prepend(plugins,g_strdup(buf));
			}
			plugins = g_list_reverse(plugins);
		} else if(sscanf(buf,"FUNCTIONS %d\n",&count) == 1) {
			int i;
			if(functions) {
				g_list_foreach(functions,(GFunc)g_free,NULL);
				g_list_free(functions);
				functions = NULL;
			}
			for(i=0;i<count;i++) {
				char *p;
				if(!fgets(buf,4096,infp))
					goto end_with_an_error;
				p = strchr(buf,'\n');
				if(p) *p = '\0';
				functions = g_list_prepend(functions,g_strdup(buf));
			}
			functions = g_list_reverse(functions);
		} else if (strncmp (buf, "CWD ", strlen ("CWD "))==0) {
			char *r = strrchr (buf, '\n');
			if (r != NULL)
				*r = '\0';
			chdir (&buf[4]);
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

			if(p && *p)
				add_history(p);

			if(!p) {
				write(outfd,"EOF!",4);
			} else {
				int len = strlen(p);
				write(outfd,"LINE",4);
				write(outfd,(gpointer)&len,sizeof(int));
				if(len>0)
					write(outfd,p,len);
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
