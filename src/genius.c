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

/*
 * this is a command line genius interface program!
 */

#include "config.h"

#include <gnome.h>

#include <glib.h>

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

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

#include "calc.h"
#include "eval.h"
#include "util.h"
#include "dict.h"
#include "inter.h"
#include "geloutput.h"
#include "lexer.h"

#include "plugin.h"

#include <readline/readline.h>
#include <readline/history.h>

/*Globals:*/

/*calculator state*/
calcstate_t curstate={
	128,
	12,
	FALSE,
	FALSE,
	FALSE,
	5,
	TRUE,
	10
	};
	
extern int got_eof;
extern int parenth_depth;

extern int interrupted;

static int use_readline = TRUE;

static int errors_printed = 0;
static long total_errors_printed = 0;

static void
puterror(const char *s)
{
	char *file;
	int line;
	gel_get_file_info(&file,&line);
	if(file)
		fprintf(stderr,"%s:%d: %s\n",file,line,s);
	else if(line>0)
		fprintf(stderr,_("line %d: %s\n"),line,s);
	else
		fprintf(stderr,"%s\n",s);
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
		fprintf(stderr,_("Too many errors! (%d followed)\n"),
			errors_printed-curstate.max_errors);
	errors_printed = 0;
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


static void
set_state(calcstate_t state)
{
	curstate = state;

	if (state.full_expressions ||
	    state.output_style == GEL_OUTPUT_LATEX ||
	    state.output_style == GEL_OUTPUT_MATHML ||
	    state.output_style == GEL_OUTPUT_TROFF)
		gel_output_set_length_limit (main_out, FALSE);
	else
		gel_output_set_length_limit (main_out, TRUE);
}

static void
interrupt(int sig)
{
	interrupted = TRUE;
	if(use_readline)
		rl_stuff_char('\n');
	signal(SIGINT,interrupt);
}

static int
nop (void)
{
	usleep(10000);
	return 0;
}

static const char *
get_version_details (void)
{
#ifndef HAVE_MPFR
	return _("\nNote: Compiled without MPFR (some operations may be slow) "
		 "see www.mpfr.org");
#else
	return "";
#endif
}


int
main(int argc, char *argv[])
{
	int i;
	int inter;
	int lastarg = FALSE;
	GSList *files = NULL;
	char *file;
	FILE *fp;
	int do_compile = FALSE;
	int be_quiet = FALSE;

	genius_is_gui = FALSE;

	bindtextdomain(PACKAGE,GNOMELOCALEDIR);
	textdomain(PACKAGE);

	signal(SIGINT,interrupt);

	statechange_hook = set_state;

	for(i=1;i<argc;i++) {
		int val;
		if(lastarg || argv[i][0]!='-')
			files = g_slist_append(files,argv[i]);
		else if(strcmp(argv[i],"--")==0)
			lastarg = TRUE;
		else if(sscanf(argv[i],"--precision=%d",&val)==1)
			curstate.float_prec = val;
		else if(sscanf(argv[i],"--maxdigits=%d",&val)==1)
			curstate.max_digits = val;
		else if(strcmp(argv[i],"--floatresult")==0)
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
		else if(sscanf(argv[i],"--maxerrors=%d",&val)==1)
			curstate.max_errors = val;
		else if(strcmp(argv[i],"--mixed")==0)
			curstate.mixed_fractions = TRUE;
		else if(strcmp(argv[i],"--nomixed")==0)
			curstate.mixed_fractions = FALSE;
		else if(sscanf(argv[i],"--intoutbase=%d",&val)==1)
			curstate.integer_output_base = val;
		else if(strcmp(argv[i],"--readline")==0)
			use_readline = TRUE;
		else if(strcmp(argv[i],"--noreadline")==0)
			use_readline = FALSE;
		else if(strcmp(argv[i],"--compile")==0)
			do_compile = TRUE;
		else if(strcmp(argv[i],"--nocompile")==0)
			do_compile = FALSE;
		else if(strcmp(argv[i],"--quiet")==0)
			be_quiet = TRUE;
		else if(strcmp(argv[i],"--noquiet")==0)
			be_quiet = FALSE;
		else {
			if(strcmp(argv[i],"--help")!=0) {
				fprintf(stderr,"Unknown argument '%s'!\n\n",
					argv[i]);
			}
			printf(_("Genius %s usage:\n\n"
			       "genius [options] [files]\n\n"
			       "\t--precision=num   \tFloating point precision [128]\n"
			       "\t--maxdigits=num   \tMaximum digits to display (0=no limit) [0]\n"
			       "\t--[no]floatresult \tAll results as floats [OFF]\n"
			       "\t--[no]scinot      \tResults in scientific notation [OFF]\n"
			       "\t--[no]fullexp     \tAlways print full expressions [OFF]\n"
			       "\t--maxerrors=num   \tMaximum errors to display (0=no limit) [5]\n"
			       "\t--[no]mixed       \tPrint fractions in mixed format\n"
			       "\t--intoutbase=num  \tBase to use to print out integers [10]\n"
			       "\t--[no]readline    \tUse readline if it is available [ON]\n"
			       "\t--[no]compile     \tCompile everything and dump it to stdout [OFF]\n"
			       "\t--[no]quiet       \tBe quiet during non-interactive mode,\n"
			       "\t                  \t(always on when compiling) [OFF]\n\n"),
			      VERSION);
			exit(1);
		}
	}

	{
		/* FIXME: use this for option parsing,
		 * we really only need gnome-program for
		 * gnome_config, perhaps we should switch to
		 * ve-config anyway */
		char *fakeargv[] = { argv[0], NULL };
		gnome_program_init ("genius", VERSION, 
				    LIBGNOME_MODULE /* module_info */,
				    1, fakeargv,
				    /* GNOME_PARAM_POPT_TABLE, options, */
				    NULL);
	}

	gel_read_plugin_list();

	if (do_compile)
		be_quiet = TRUE;
	inter = isatty(0) && !files && !do_compile;
	/*interactive mode, print welcome message*/
	if(inter) {
		printf(_("Genius %s\n"
			 "%s\n"
			 "This is free software with ABSOLUTELY NO WARRANTY.\n"
			 "For license details type `warranty'.\n"
			 "For help type 'manual' or 'help'.%s\n\n"),
		       VERSION,
		       COPYRIGHT_STRING,
		       get_version_details ());
		be_quiet = FALSE;
	}

	main_out = gel_output_new();
	if(!be_quiet)
		gel_output_setup_file(main_out, stdout, 80,
				      get_term_width);
	else
		gel_output_setup_black_hole(main_out);


	set_new_calcstate(curstate);
	set_new_errorout(calc_puterror);
	set_new_infoout(puterror);

	/*init the context stack and clear out any stale dictionaries
	  except the global one, if this is the first time called it
	  will also register the builtin routines with the global
	  dictionary*/
	d_singlecontext ();

	if(!do_compile) {
		/*
		 * Read main library
		 */
		if (access ("../lib/lib.cgel", F_OK) == 0) {
			/*try the library file in the current/../lib directory*/
			gel_load_compiled_file (NULL, "../lib/lib.cgel", FALSE);
		} else {
			gel_load_compiled_file (NULL,
						LIBRARY_DIR "/gel/lib.cgel",
						FALSE);
		}

		/*
		 * Read init files
		 */
		file = g_strconcat(g_getenv("HOME"),"/.geniusinit",NULL);
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
			if ( ! do_compile)
				gel_save_plugins ();
			return 0;
		}
	} else {
		fp = stdin;
		gel_push_file_info(NULL,1);
	}
	my_yy_open(fp);
	if(inter && use_readline) {
		init_inter();
	}

	gel_printout_infos ();
	
	rl_event_hook = nop;

	for(;;) {
		for(;;) {
			if(inter && use_readline) /*use readline mode*/ {
				GelETree *e;
				gel_rewind_file_info();
				line_len_cache = -1;
				e = get_p_expression();
				line_len_cache = -1;
				if(e) gel_evalexp_parsed(e,main_out,"= ",TRUE);
				line_len_cache = -1;
			} else {
				line_len_cache = -1;
				gel_evalexp(NULL, fp, main_out, NULL, FALSE, NULL);
				line_len_cache = -1;
				if(interrupted)
					got_eof = TRUE;
			}
			if(inter)
				gel_printout_infos ();

			if(got_eof) {
				if(inter)
					puts("");
				got_eof = FALSE;
				break;
			}
		}
		if(files) {
			GSList *t;
			my_yy_close(fp);
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
				if(do_compile) {
					gel_push_file_info(NULL,0);
					gel_compile_all_user_funcs(stdout);
					gel_pop_file_info();
					/* if we have gotten errors then
					   signal by returning a 1 */
					if(total_errors_printed)
						return 1;
				} else {
					gel_save_plugins ();
				}
				return 0;
			}
			my_yy_open(fp);
		} else
			break;
	}

	gel_printout_infos ();
	
	my_yy_close(fp);
	/*if(fp != stdin)
		fclose(fp);*/
	
	if(do_compile) {
		gel_push_file_info(NULL,0);
		gel_compile_all_user_funcs(stdout);
		gel_pop_file_info();
		/* if we have gotten errors then
		   signal by returning a 1 */
		if(total_errors_printed)
			return 1;
	} else {
		gel_save_plugins ();
	}

	return 0;
}

