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

#ifndef CALC_H_
#define CALC_H_

#include <stdio.h>
#include <glib.h>

#include "structs.h"

#define COPYRIGHT_STRING "Copyright (C) 1997-2002 George Lebl"

typedef enum {	NO_ERROR=0,
		PARSE_ERROR,
		INTERNAL_MPW_ERROR,
		NUMERICAL_MPW_ERROR,
		EOF_ERROR,
		EOE_ERROR, /*end of expression*/
		IGNORE_ERROR /*set this if you want to ignore the result*/
	} calc_error_t;

typedef enum {
	GEL_OUTPUT_NORMAL,
	GEL_OUTPUT_TROFF,
	GEL_OUTPUT_LATEX
} GelOutputStyle;

typedef struct _calcstate_t {
	/*about incoming stuff*/
	int float_prec;        /*precision of floats to use*/
	/*about outgoing stuff*/
	int max_digits; /*max digits in the display 0= as many as I got*/
	gboolean results_as_floats;  /*give result as float*/
	gboolean scientific_notation; /*always scientific notation*/
	gboolean full_expressions; /*print out expressions longer then a line*/
	int max_errors;	/*maximum error lines*/
	gboolean mixed_fractions; /*if we should print fractions in mixed format*/
	int integer_output_base; /*output base for integers*/
	GelOutputStyle output_style;
} calcstate_t;

/*so we can use and set the yyparse function for parse errors*/
int yyparse(void);

/*make a string representation of an expression*/
void print_etree(GelOutput *gelo, GelETree *n, gboolean toplevel);
/*make a string representation of an expression, with the first node prettied
  (currently only for matrix)*/
void pretty_print_etree(GelOutput *gelo, GelETree *n);
/* make a newly allocated string for this */
char * string_print_etree(GelETree *n);

/*add the right parenthesis and brackets to the end of the expression*/
char * addparenth(char *s);

/*this is the function to be mostly called outsied of calc.c
  evaluate the xpression string and give back a string with the
  result, expression is in str or if str is NULL then in infd,
  pretty will use pretty_print_etree*/
void evalexp(const char * str, FILE *infile, GelOutput *gelo, const char *prefix, gboolean pretty, const char *dirprefix);
/*this is the normal evaluation for the frontends if they already parsed,
  it free's the parsed tree after use*/
void evalexp_parsed(GelETree *parsed, GelOutput *gelo, const char *prefix, gboolean pretty);

/*these are parts of the above*/
/*note that parseexp will actually load AND execute files if there are load
  toplevel instructions, as those don't translate into an GelETree*/
GelETree * parseexp(const char *str, FILE *infile, gboolean exec_commands, gboolean testparse, gboolean *finished, const char *dirprefix);
GelETree * runexp(GelETree *exp);

void compile_all_user_funcs(FILE *outfile);
void load_compiled_file(const char *dirprefix, const char *file, gboolean warn);
void load_file(const char *dirprefix, const char *file, gboolean warn);
void load_guess_file(const char *dirprefix, const char *file, gboolean warn);
void set_new_calcstate(calcstate_t state);
void set_new_errorout(void (*func)(const char *));
void set_new_infoout(void (*func)(const char *));

extern void (*errorout)(const char *);
extern void (*infoout)(const char *);

/*This is for file/line info for errors*/
void push_file_info(char *file,int line);
void pop_file_info(void);
void incr_file_info(void);
void rewind_file_info(void);
void get_file_info(char **file, int *line);

extern FILE *outputfp;
extern void (*evalnode_hook)(void);
extern int run_hook_every;
extern void (*statechange_hook)(calcstate_t);

typedef struct {
	char *func;

	/* normally NULL, if not NULL, then the below
	 * will be ignored */
	char *aliasfor;

	char *category;
	char *description;
	GSList *aliases;
	/* should be ONE of the below */
	char *help_link;
	char *help_html;
} GelHelp;

/* well sorted */
GSList *get_categories (void);
const char *get_category_name (const char *category);
/* null for uncategorized */
GSList *get_helps (const char *category);
/* gets undocumented functions */
GSList *get_undocumented (void);

void new_category (const char *category, const char *name);

GelHelp *get_help (const char *func, gboolean insert);

void add_description (const char *func, const char *desc);
const char *get_description (const char *func);
void add_category (const char *func, const char *category);
void add_alias (const char *func, const char *alias);
void add_help_link (const char *func, const char *link);
void add_help_html (const char *func, const char *html);
void whack_help (const char *func);

extern gboolean genius_is_gui;

typedef enum {
	GEL_NO_COMMAND,
	GEL_LOADFILE,
	GEL_LOADFILE_GLOB,
	GEL_LOADPLUGIN,
	GEL_LS,
	GEL_LS_ARG,
	GEL_PWD,
	GEL_CHANGEDIR,
	GEL_HELP,
	GEL_HELP_ARG
} GelCommand;

extern GelCommand gel_command;
extern char *gel_command_arg;

extern GelOutput *main_out;

#endif /* CALC_H_ */
