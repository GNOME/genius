/* GENIUS Calculator
 * Copyright (C) 1997-2004 Jiri (George) Lebl
 *
 * Author: Jiri (George) Lebl
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

#include "genius-i18n.h"

#include "structs.h"

#define COPYRIGHT_STRING "Copyright (C) 1997-2004 Jiri (George) Lebl"

typedef enum {
	NO_ERROR = 0,
	PARSE_ERROR,
	INTERNAL_MPW_ERROR,
	NUMERICAL_MPW_ERROR,
	EOF_ERROR,
	EOE_ERROR, /*end of expression*/
	IGNORE_ERROR /*set this if you want to ignore the result*/
} GeniusError;

/* FIXME: This should be nicer */
extern GeniusError error_num;

extern gboolean interrupted;

typedef enum {
	GEL_OUTPUT_NORMAL,
	GEL_OUTPUT_TROFF,
	GEL_OUTPUT_LATEX,
	GEL_OUTPUT_MATHML
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
void	gel_print_etree		(GelOutput *gelo,
				 GelETree *n,
				 gboolean toplevel);
/*make a string representation of an expression, with the first node prettied
  (currently only for matrix)*/
void	gel_pretty_print_etree	(GelOutput *gelo,
				 GelETree *n);
/* make a newly allocated string for this */
char *	gel_string_print_etree	(GelETree *n);

/* Note that infile must be gel_lexer_open'ed */
/*this is the function to be mostly called outsied of calc.c
  evaluate the xpression string and give back a string with the
  result, expression is in str or if str is NULL then in infd,
  pretty will use pretty_print_etree*/
void gel_evalexp (const char *str /* string to evaluate */,
		  FILE *infile /* file to evaluate */,
		  GelOutput *gelo /* output to print to */,
		  const char *prefix /* prefix to print before result */,
		  gboolean pretty /* result should be in pretty print */,
		  const char *dirprefix /* directory prefix where we are */);
/*this is the normal evaluation for the frontends if they already parsed,
  it frees the parsed tree after use so don't use it afterwards*/
void gel_evalexp_parsed (GelETree *parsed /* parsed tree to evaluate */,
			 GelOutput *gelo /* output to print to */,
			 const char *prefix /* prefix to print before result */,
			 gboolean pretty /* result should be in pretty print */);

/* This function should be called at an end of running something to print
 * out all errors and all such nonsense.  It is implemented in the particular
 * frontend */
void gel_printout_infos (void);

/* implemented in the frontend (function can be NULL) */
void gel_call_help (const char *function);

void gel_help_on (const char *text);

/*these are parts of the above*/
/*note that parseexp will actually load AND execute files if there are load
  toplevel instructions, as those don't translate into an GelETree*/
/* Note that infile must be gel_lexer_open'ed */
GelETree * gel_parseexp (const char *str,
			 FILE *infile,
			 gboolean exec_commands,
			 gboolean testparse,
			 gboolean *finished,
			 const char *dirprefix);
/* This EATS the expression!  May even return the same node,
 * but the expression is WHACKED. */
GelETree * gel_runexp (GelETree *exp);

void gel_compile_all_user_funcs (FILE *outfile);
void gel_load_compiled_file (const char *dirprefix,
			     const char *file,
			     gboolean warn);
void gel_load_file (const char *dirprefix,
		    const char *file,
		    gboolean warn);
void gel_load_guess_file (const char *dirprefix,
			  const char *file,
			  gboolean warn);

void gel_dump_strings_from_user_funcs (FILE *outfile);
void gel_dump_strings_from_help (FILE *outfile);

void set_new_calcstate(calcstate_t state);
void set_new_errorout(void (*func)(const char *));
void set_new_infoout(void (*func)(const char *));

void gel_errorout   (const char *format, ...) G_GNUC_PRINTF (1, 2);
void gel_infoout   (const char *format, ...) G_GNUC_PRINTF (1, 2);
/* this is bad */
extern void (*errorout)(const char *);
extern void (*infoout)(const char *);

/*This is for file/line info for errors*/
void gel_push_file_info(const char *file,int line);
void gel_pop_file_info(void);
void gel_incr_file_info(void);
void gel_rewind_file_info(void);
void gel_get_file_info(char **file, int *line);

extern FILE *outputfp;
extern void (*evalnode_hook)(void);
#define RUN_HOOK_EVERY_MASK 0x3FF
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

void new_category (const char *category, const char *name, gboolean internal);

GelHelp *get_help (const char *func, gboolean insert);

void add_description (const char *func, const char *desc);
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

extern gboolean gel_got_eof;

#endif /* CALC_H_ */
