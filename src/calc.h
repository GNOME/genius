/* GENIUS Calculator
 * Copyright (C) 1997-2021 Jiri (George) Lebl
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

#ifndef CALC_H_
#define CALC_H_

#include <stdio.h>
#include <glib.h>

#include "genius-i18n.h"

#include "structs.h"

#define GENIUS_COPYRIGHT_STRING N_("Copyright (C) 1997-2021 Jiří (George) Lebl")

typedef enum {
	GEL_NO_ERROR = 0,
	GEL_PARSE_ERROR,
	GEL_INTERNAL_MPW_ERROR,
	GEL_NUMERICAL_MPW_ERROR,
	GEL_EOF_ERROR,
	GEL_EOE_ERROR, /*end of expression*/
	GEL_IGNORE_ERROR /*set this if you want to ignore the result*/
} GeniusError;

/* FIXME: This should be nicer */
extern GeniusError gel_error_num;

extern gboolean gel_interrupted;

typedef enum {
	GEL_OUTPUT_NORMAL,
	GEL_OUTPUT_TROFF,
	GEL_OUTPUT_LATEX,
	GEL_OUTPUT_MATHML
} GelOutputStyle;

typedef struct {
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
	int max_nodes;	/*maximum number of nodes +- whatever*/
	int chop;	/* chop floating point numbers 
			   smaller than approximately 10^-chop */
	int chop_when;  /* but only if the object contains a number greater
			   than 10^-chop_when */
} GelCalcState;

/*so we can use and set the yyparse function for parse errors*/
/* int yyparse(void); */

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

/* same as gel_print_etree, but only for function */
void	gel_print_func (GelOutput *gelo,
			GelEFunc *f);

/* Note that infile must be gel_lexer_open'ed */
/*this is the function to be mostly called outsied of calc.c
  evaluate the expression string and give back a string with the
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

/* implemented in the frontend (query can be NULL) */
char *gel_ask_string (const char *query, const char *def);

/* implemented in the frontend, buttons should be a nonempty list of strings */
int gel_ask_buttons (const char *query, GSList *buttonlist);

void gel_help_on (const char *text);

/*these are parts of the above*/
void gel_execinit (void); /* run this before gel_parseexp on
			     a standalone run */
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

void gel_set_new_calcstate(GelCalcState state);
void gel_set_new_errorout(void (*func)(const char *));
void gel_set_new_infoout(void (*func)(const char *));

/* Don't set directly */
extern GelCalcState gel_calcstate;

/* frontend specific function, implemented there */
void gel_set_state (GelCalcState state);

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

extern const GelHookFunc gel_evalnode_hook;
#define GEL_RUN_HOOK_EVERY_MASK 0x3FF


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
GSList *gel_get_categories (void);
const char *gel_get_category_name (const char *category);
/* null for uncategorized */
GSList *gel_get_helps (const char *category);
/* gets undocumented functions */
GSList *gel_get_undocumented (void);

void gel_new_category (const char *category, const char *name, gboolean internal);

GelHelp *gel_get_help (const char *func, gboolean insert);

void gel_add_description (const char *func, const char *desc);
void gel_add_category (const char *func, const char *category);
void gel_add_alias (const char *func, const char *alias);
void gel_add_help_link (const char *func, const char *link);
void gel_add_help_html (const char *func, const char *html);
void gel_whack_help (const char *func);

extern const gboolean genius_is_gui;

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

extern GelOutput *gel_main_out;

extern gboolean gel_got_eof;

extern gboolean genius_in_dev_dir;

#endif /* CALC_H_ */
