/* GENIUS Calculator
 * Copyright (C) 1997-2002 George Lebl
 */
#ifndef __LEXER_H__
#define __LEXER_H__

void my_yy_open(FILE *fp);
void my_yy_parse(FILE *fp);
void my_yy_close(FILE *fp);

#endif
