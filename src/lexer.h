/* GENIUS Calculator
 * Copyright (C) 1997-2003 George Lebl
 */
#ifndef __LEXER_H__
#define __LEXER_H__

/* If you wish to use a file pointer with genius you must first
   _open it, and you should close it with the _close function below */
void gel_lexer_open (FILE *fp);
void gel_lexer_parse (FILE *fp);
void gel_lexer_close (FILE *fp);

#endif
