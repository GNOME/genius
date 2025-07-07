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

/*
 * this is a command line genius interface program!
 */

#include "config.h"

#include <glib.h>

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "calc.h"
#include "eval.h"
#include "util.h"
#include "dict.h"
#include "inter.h"
#include "geloutput.h"
#include "lexer.h"

#include "plugin.h"

#include "genius-i18n.h"

#include "binreloc.h"

#include <readline/readline.h>
#include <readline/history.h>

#ifdef USE_NCURSES
#ifdef INC_NCURSES
#include <ncurses/curses.h>
#include <ncurses/term.h>
#else
#include <curses.h>
#include <term.h>
#endif
#else
#include <termcap.h>
#endif

#include <vicious.h>

/*Globals:*/

const gboolean genius_is_gui = FALSE;

/*calculator state*/
GelCalcState curstate={
	128,
	12,
	FALSE,
	FALSE,
	FALSE,
	5,
	TRUE,
	10,
	0, /* output_style */
	0, /* max_nodes */ /* FIXME: implement here just like in gnome-genius */
	20, /* chop */
	5 /* chop_when */
	};

const GelHookFunc gel_evalnode_hook = NULL;
const GelHookFunc _gel_tree_limit_hook = NULL;
const GelHookFunc _gel_finished_toplevel_exec_hook = NULL;
	
extern int parenth_depth;

static int use_readline = TRUE;

gboolean genius_in_dev_dir = FALSE;

static int errors_printed = 0;
static long total_errors_printed = 0;

static void
puterror(const char *s)
{
	char *file;
	int line;
	gel_get_file_info(&file,&line);
	if(file)
		g_printerr ("%s:%d: %s\n",file,line,s);
	else if(line>0)
		g_printerr (_("line %d: %s\n"),line,s);
	else
		g_printerr ("%s\n",s);
}

static void
calc_puterror(const char *s)
{
	if(curstate.max_errors == 0 ||
	   errors_printed++<curstate.max_errors)
		puterror(s);
	total_errors_printed++;
}

void
gel_printout_infos (void)
{
	if(errors_printed-curstate.max_errors > 0)
		g_printerr (_("Too many errors! (%d followed)\n"),
			errors_printed-curstate.max_errors);
	errors_printed = 0;
}

static char *
get_help_index (const char *datadir)
{
	const char * const* langs;
	int i;

	langs = g_get_language_names ();
	for (i = 0; langs[i] != NULL; i++) {
		char *file;
		file = g_build_filename (datadir,
					 "genius",
					 "help",
					 langs[i],
					 "html",
					 "index.html",
					 NULL);
		if (access (file, F_OK) == 0) {
			return file;
		}
		g_free (file);
	}
	return g_build_filename (datadir,
				 "genius",
				 "help",
				 "C",
				 "html",
				 "index.html",
				 NULL);
}

void
gel_call_help (const char *function)
{
	char *file;
	char *str;

	if (function != NULL) {
		/* No extra processing of help,
		 * printout was done in funclib */
		return;
	}

	str = gbr_find_data_dir (DATADIR);
	file = get_help_index (str);
	g_free (str);

	if G_UNLIKELY (access (file, R_OK) != 0) {
		g_free (file);
		file = get_help_index (DATADIR);
		if G_UNLIKELY (access (file, R_OK) != 0) {
			puterror (_("Cannot locate the manual"));
			g_free (file);
			return;
		}
	}

	str = g_find_program_in_path ("lynx");
	if (str == NULL)
		str = g_find_program_in_path ("links");
	if (str == NULL)
		str = g_find_program_in_path ("w3m");

	if G_LIKELY (str != NULL) {
		char *argv[3];

		argv[0] = str;
		argv[1] = file;
		argv[2] = NULL;
		g_spawn_sync  (NULL /* wd */,
			       argv,
			       NULL /* envp */,
			       G_SPAWN_CHILD_INHERITS_STDIN /* flags */,
			       NULL /* child_setup */,
			       NULL /* user_data */,
			       NULL /*stdout*/,
			       NULL /*stderr*/,
			       NULL /* status */,
			       NULL /* error */);

		g_free(str);

	} else {
		puterror (_("Cannot run lynx/links/w3m to show the manual"));
	}
	g_free (file);
}

char *
gel_ask_string (const char *query, const char *def)
{
	char *txt = NULL;

	g_print ("\n%s\n", ve_sure_string (query));
	if (use_readline) {
		char *s;
		if ( ! ve_string_empty (def)) {
			const char *p;
			for (p = def; *p != '\0'; p++)
				rl_stuff_char (*p);
		}
		s = readline (">");
		if (s != NULL) {
			txt = g_strdup (s);
			free (s);
		}
	} else {
		char buf[256];
		if ( ! ve_string_empty (def))
			g_print (_("Suggested: %s\n"), ve_sure_string (def));
		if (fgets (buf, sizeof (buf), stdin) != NULL) {
			int len = strlen (buf);
			if (buf[len-1] == '\n')
				buf[len-1] = '\0';
			txt = g_strdup (buf);
		}
	}
	return txt;
}

int
gel_ask_buttons (const char *query, GSList *buttonlist)
{
	int ret;
	GSList *li;
	int i;
	int max;

reread_buttons:
	g_print ("\n%s\n", ve_sure_string (query));
	i = 1;
	for (li = buttonlist; li != NULL; li = li->next) {
		g_print ("%d) %s\n", i, ve_sure_string ((char *)li->data));
		i++;
	}
	max = i-1;
	if (use_readline) {
		char *s;
		s = readline (">");
		ret = -1;
		if ( ! ve_string_empty (s)) {
			if (sscanf (s, "%d", &ret) != 1) {
				ret = -1;
			}
			free (s);
		}
	} else {
		char buf[256];
		ret = -1;
		if (fgets (buf, sizeof (buf), stdin) != NULL) {
			if (sscanf (buf, "%d", &ret) != 1) {
				ret = -1;
			}
		}
	}
	if (ret == 0 || ret > max) {
		g_print (_("Out of range!\n"));
		goto reread_buttons;
	}

	return ret;
}

static int
long_get_term_width (void)
{
	char buf[2048];
	const char *term = g_getenv("TERM");

	if(!term) return 80;

	if(tgetent(buf,term)<=0)
		return 80;

	return tgetnum("co");
}

/* hack! */
static int line_len_cache = -1;

static int
get_term_width (GelOutput *gelo)
{
	if (line_len_cache < 0)
		line_len_cache = long_get_term_width ();
	return line_len_cache;
}


void
gel_set_state(GelCalcState state)
{
	curstate = state;

	if (state.full_expressions ||
	    state.output_style == GEL_OUTPUT_LATEX ||
	    state.output_style == GEL_OUTPUT_MATHML ||
	    state.output_style == GEL_OUTPUT_TROFF)
		gel_output_set_length_limit (gel_main_out, FALSE);
	else
		gel_output_set_length_limit (gel_main_out, TRUE);
}

static void
interrupt (int sig)
{
	gel_interrupted = TRUE;
	if (use_readline)
		rl_stuff_char ('\n');
	signal (SIGINT, interrupt);
}

static const char *
get_version_details (void)
{
	return "";
}

int
main(int argc, char *argv[])
{
	int i;
	int inter;
	gboolean lastarg = FALSE;
	GSList *files = NULL;
	char *file;
	FILE *fp;
	gboolean do_compile = FALSE;
	gboolean do_gettext = FALSE;
	gboolean be_quiet = FALSE;
	char *exec = NULL;

	g_set_prgname ("genius");
	g_set_application_name (_("Genius"));

	/* kind of a hack to find out if we are being run from the
	 * directory we were built in */
	file = g_get_current_dir ();
	if (file != NULL &&
	    strcmp (file, BUILDDIR "/src") == 0 &&
	    access ("genius.c", F_OK) == 0 &&
	    access ("../lib/lib.cgel", F_OK) == 0) {
		genius_in_dev_dir = TRUE;
	} else {
		genius_in_dev_dir = FALSE;
		gbr_init (NULL);
	}
	g_free (file);

	/* Hmmm, everything in UTF-8? */
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	setlocale (LC_ALL, "");

	signal (SIGINT, interrupt);

	for(i=1;i<argc;i++) {
		int val;
		if(lastarg || argv[i][0]!='-')
			files = g_slist_append(files,argv[i]);
		else if(strcmp(argv[i],"--")==0)
			lastarg = TRUE;
		else if(sscanf(argv[i],"--precision=%d",&val)==1) {
			if (val < 60 || val > 16384) {
				g_printerr (_("%s should be between %d and %d, using %d"),
					    "--precision", 60, 16384, 128);
				val = 128;
			}
			curstate.float_prec = val;
		} else if (strcmp (argv[i], "--precision")==0 && i+1 < argc) {
			val = 0;
			sscanf (argv[++i],"%d",&val);
			if (val < 60 || val > 16384) {
				g_printerr (_("%s should be between %d and %d, using %d"),
					    "--precision", 60, 16384, 128);
				val = 128;
			}
			curstate.float_prec = val;
		} else if(sscanf(argv[i],"--maxdigits=%d",&val)==1) {
			if (val < 0 || val > 256) {
				g_printerr (_("%s should be between %d and %d, using %d"),
					    "--maxdigits", 0, 256, 12);
				val = 12;
			}
			curstate.max_digits = val;
		} else if (strcmp (argv[i], "--maxdigits")==0 && i+1 < argc) {
			val = -1;
			sscanf (argv[++i],"%d",&val);
			if (val < 0 || val > 256) {
				g_printerr (_("%s should be between %d and %d, using %d"),
					    "--maxdigits", 0, 256, 12);
				val = 12;
			}
			curstate.max_digits = val;
		} else if(strcmp(argv[i],"--floatresult")==0)
			curstate.results_as_floats = TRUE;
		else if(strcmp(argv[i],"--nofloatresult")==0)
			curstate.results_as_floats = FALSE;
		else if(strcmp(argv[i],"--scinot")==0)
			curstate.scientific_notation = TRUE;
		else if(strcmp(argv[i],"--noscinot")==0)
			curstate.scientific_notation = FALSE;
		else if(strcmp(argv[i],"--fullexp")==0)
			curstate.full_expressions = TRUE;
		else if(strcmp(argv[i],"--nofullexp")==0)
			curstate.full_expressions = FALSE;
		else if(sscanf(argv[i],"--maxerrors=%d",&val)==1) {
			if (val < 0) {
				g_printerr (_("%s should be greater then or equal to %d, using %d"),
					    "--maxerrors", 0, 5);
				val = 5;
			}
			curstate.max_errors = val;
		} else if (strcmp (argv[i], "--maxerrors")==0 && i+1 < argc) {
			val = -1;
			sscanf (argv[++i],"%d",&val);
			if (val < 0) {
				g_printerr (_("%s should be greater then or equal to %d, using %d"),
					    "--maxerrors", 0, 5);
				val = 5;
			}
			curstate.max_errors = val;
		} else if(strcmp(argv[i],"--mixed")==0)
			curstate.mixed_fractions = TRUE;
		else if(strcmp(argv[i],"--nomixed")==0)
			curstate.mixed_fractions = FALSE;
		else if(sscanf(argv[i],"--intoutbase=%d",&val)==1) {
			curstate.integer_output_base = val;
		} else if (strcmp (argv[i], "--intoutbase")==0 && i+1 < argc) {
			val = 10;
			sscanf (argv[++i],"%d",&val);
			curstate.integer_output_base = val;
		} else if(sscanf(argv[i],"--chop=%d",&val)==1) {
			curstate.chop = val;
		} else if (strcmp (argv[i], "--chop")==0 && i+1 < argc) {
			val = 20;
			sscanf (argv[++i],"%d",&val);
			curstate.chop = val;
		} else if(sscanf(argv[i],"--chopwhen=%d",&val)==1) {
			curstate.chop_when = val;
		} else if (strcmp (argv[i], "--chopwhen")==0 && i+1 < argc) {
			val = 10;
			sscanf (argv[++i],"%d",&val);
			curstate.chop_when = val;
		} else if(strcmp(argv[i],"--readline")==0)
			use_readline = TRUE;
		else if(strcmp(argv[i],"--noreadline")==0)
			use_readline = FALSE;
		else if(strcmp(argv[i],"--compile")==0)
			do_compile = TRUE;
		else if(strcmp(argv[i],"--nocompile")==0)
			do_compile = FALSE;
		else if(strcmp(argv[i],"--gettext")==0)
			do_gettext = TRUE;
		else if(strcmp(argv[i],"--nogettext")==0)
			do_gettext = FALSE;
		else if(strcmp(argv[i],"--quiet")==0)
			be_quiet = TRUE;
		else if(strcmp(argv[i],"--noquiet")==0)
			be_quiet = FALSE;
		else if (strncmp (argv[i], "--exec=", strlen ("--exec=")) == 0) {
			exec = g_strdup ((argv[i])+strlen("--exec="));
		} else if (strcmp (argv[i], "--exec") == 0 && i+1 < argc) {
			exec = g_strdup (argv[++i]);
		} else if (strcmp (argv[i], "--version") == 0) {
			g_print (_("Genius %s\n"
				   "%s%s\n"),
				 VERSION,
				 _(GENIUS_COPYRIGHT_STRING),
				 get_version_details ());
			exit (0);
		} else {
			if (strcmp (argv[i], "--help") != 0) {
				g_printerr ("Unknown argument '%s'!\n\n",
					argv[i]);
			}
			g_print (_("Genius %s usage:\n\n"
				   "genius [options] [files]\n\n"
				   "\t--help            \tPrint this help\n"
				   "\t--version         \tPrint version number\n"
				   "\t--precision=num   \tFloating point precision [128]\n"
				   "\t--maxdigits=num   \tMaximum digits to display (0=no limit) [0]\n"
				   "\t--[no]floatresult \tAll results as floats [OFF]\n"
				   "\t--[no]scinot      \tResults in scientific notation [OFF]\n"
				   "\t--[no]fullexp     \tAlways print full expressions [OFF]\n"
				   "\t--maxerrors=num   \tMaximum errors to display (0=no limit) [5]\n"
				   "\t--[no]mixed       \tPrint fractions in mixed format\n"
				   "\t--intoutbase=num  \tBase to use to print out integers [10]\n"
				   "\t--chop=num        \tChop small numbers less than 10^-num [20]\n"
				   "\t--chopwhen=num    \tBut only when other numbers 10^-num or more [5]\n"
				   "\t--[no]readline    \tUse readline if it is available [ON]\n"
				   "\t--[no]compile     \tCompile everything and dump it to stdout [OFF]\n"
				   "\t--[no]gettext     \tDump help/error strings in fake .c file to\n"
				   "\t                  \tstdout (for use with gettext) [OFF]\n"
				   "\t--[no]quiet       \tBe quiet during non-interactive mode,\n"
				   "\t                  \t(always on when compiling) [OFF]\n"
				   "\t--exec=expr       \tExecute an expression\n\n"),
				 VERSION);
			if (strcmp (argv[i], "--help") != 0)
				exit (1);
			else
				exit (0);
		}
	}

	if (files != NULL && exec != NULL) {
		g_printerr (_("Can't specify both an expression and files to execute on the command line"));
		exit (1);
	}

	/* ensure the directory, if it is a file, no worries not saving the properties is not fatal at all */
	file = g_build_filename (g_get_home_dir (), ".genius", NULL);
	if (access (file, F_OK) != 0) {
		mkdir (file, 0755);
	}
	g_free (file);

	gel_read_plugin_list();

	if (do_compile || do_gettext)
		be_quiet = TRUE;
	inter = isatty(0) && !files && !exec && !(do_compile || do_gettext);
	/*interactive mode, print welcome message*/
	if (inter) {
		g_print (_("Genius %s\n"
			   "%s\n"
			   "This is free software with ABSOLUTELY NO WARRANTY.\n"
			   "For license details type `warranty'.\n"
			   "For help type `manual' or `help'.%s\n\n"),
			 VERSION,
			 _(GENIUS_COPYRIGHT_STRING),
			 get_version_details ());
		be_quiet = FALSE;
	}

	gel_main_out = gel_output_new();
	if(!be_quiet)
		gel_output_setup_file(gel_main_out, stdout, 80,
				      get_term_width);
	else
		gel_output_setup_black_hole(gel_main_out);


	gel_set_new_calcstate(curstate);
	gel_set_new_errorout(calc_puterror);
	gel_set_new_infoout(puterror);

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext ();

	gel_init ();

	if ( ! (do_compile || do_gettext)) {
		/*
		 * Read main library
		 */
		if (genius_in_dev_dir) {
			/*try the library file in the current/../lib directory*/
			gel_load_compiled_file (NULL, "../lib/lib.cgel", FALSE);
		} else {
			char *datadir = gbr_find_data_dir (DATADIR);
			file = g_build_filename (datadir,
						 "genius",
						 "gel",
						 "lib.cgel",
						 NULL);
			if (access (file, F_OK) != 0) {
				g_free (file);
				file = g_build_filename (DATADIR,
							 "genius",
							 "gel",
							 "lib.cgel",
							 NULL);
			}
			gel_load_compiled_file (NULL,
						file,
						FALSE);
			g_free (file);
			g_free (datadir);
		}

		/*
		 * Read init files
		 */
		file = g_build_filename (g_get_home_dir (), ".geniusinit",NULL);
		if(file)
			gel_load_file(NULL, file, FALSE);
		g_free(file);

		gel_load_file (NULL, "geniusinit.gel", FALSE);

		/* Add a default last answer */
		d_addfunc (d_makevfunc (d_intern ("Ans"),
					gel_makenum_string
					(_("The only thing that "
					   "interferes with my "
					   "learning is my education.  "
					   "-- Albert Einstein"))));

		/*
		 * Restore plugins
		 */
		gel_restore_plugins ();
	}

	if (files != NULL) {
		GSList *t;
		do {
			fp = fopen(files->data,"r");
			gel_push_file_info(files->data,1);
			t = files;
			files = g_slist_remove_link(files,t);
			g_slist_free_1(t);
			if(!fp) {
				gel_pop_file_info();
				puterror(_("Can't open file"));
			}
		} while(!fp && files);
		if(!fp && !files) {
			if ( ! (do_compile || do_gettext))
				gel_save_plugins ();
			return 0;
		}
	} else if (exec != NULL) {
		fp = NULL;
		gel_push_file_info("expr",1);
	} else {
		fp = stdin;
		gel_push_file_info(NULL,1);
	}
	if (fp != NULL)
		gel_lexer_open(fp);
	if(inter && use_readline) {
		init_inter();
	}

	gel_printout_infos ();
	
	if (exec != NULL) {
		line_len_cache = -1;
		gel_evalexp (exec, NULL, gel_main_out, NULL, FALSE, NULL);
		gel_test_max_nodes_again ();
		line_len_cache = -1;
		goto after_exec;
	}

	for(;;) {
		for(;;) {
			if(inter && use_readline) /*use readline mode*/ {
				GelETree *e;
				gel_rewind_file_info();
				line_len_cache = -1;
				e = get_p_expression();
				line_len_cache = -1;
				if(e) gel_evalexp_parsed(e,gel_main_out,"= ",TRUE);
				gel_test_max_nodes_again ();
				line_len_cache = -1;
			} else {
				line_len_cache = -1;
				gel_evalexp(NULL, fp, gel_main_out, NULL, FALSE, NULL);
				line_len_cache = -1;
				if G_UNLIKELY (gel_interrupted)
					gel_got_eof = TRUE;
			}
			if(inter)
				gel_printout_infos ();

			if (gel_got_eof) {
				if(inter)
					puts("");
				gel_got_eof = FALSE;
				break;
			}
		}
		if(files) {
			GSList *t;
			gel_lexer_close(fp);
			/*fclose(fp);*/
			do {
				fp = fopen(files->data,"r");
				gel_push_file_info(files->data,1);
				t = files;
				files = g_slist_remove_link(files,t);
				g_slist_free_1(t);
				if(!fp) {
					gel_pop_file_info();
					puterror(_("Can't open file"));
				}
			} while(!fp && files);
			if(!fp && !files) {
				goto after_exec;
			}
			gel_lexer_open(fp);
		} else
			break;
	}

after_exec:
	gel_test_max_nodes_again ();

	gel_printout_infos ();
	
	if (fp != NULL)
		gel_lexer_close(fp);
	/*if(fp != stdin)
		fclose(fp);*/
	
	if (do_compile) {
		gel_push_file_info(NULL,0);
		gel_compile_all_user_funcs(stdout);
		gel_pop_file_info();
		/* if we have gotten errors then
		   signal by returning a 1 */
		if(total_errors_printed)
			return 1;
	} else if (do_gettext) {
		gel_push_file_info (NULL, 0);
		gel_dump_strings_from_help (stdout);
		gel_dump_strings_from_user_funcs (stdout);
		gel_pop_file_info ();
		/* if we have gotten errors then
		   signal by returning a 1 */
		if (total_errors_printed)
			return 1;
	} else {
		gel_save_plugins ();
	}

	return 0;
}

