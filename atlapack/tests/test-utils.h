/*
  Part of: ATLAPACK
  Contents: tests utilities
  Date: Tue Dec 27, 2011

  Abstract



  Copyright (C) 2011 Marco Maggi <marco.maggi-ipsu@poste.it>

  Redistribution and  use in  source and binary  forms, with  or without
  modification, are permitted provided that the following conditions are
  met:

    * Redistributions  of source  code must  retain the  above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form  must reproduce the above copyright
      notice, this  list of conditions  and the following  disclaimer in
      the  documentation  and/or   other  materials  provided  with  the
      distribution.

    * Neither  the  name of  Intel  Corporation  nor  the names  of  its
      contributors may  be used to  endorse or promote  products derived
      from this software without specific prior written permission.

  THIS SOFTWARE  IS PROVIDED BY  THE COPYRIGHT HOLDERS  AND CONTRIBUTORS
  "AS  IS" AND  ANY EXPRESS  OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT
  LIMITED TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE  ARE DISCLAIMED. IN NO EVENT  SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES (INCLUDING,  BUT  NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS  INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY,  OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


/** --------------------------------------------------------------------
 ** Headers.
 ** ----------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <lapacke.h>


/** --------------------------------------------------------------------
 ** Helpers.
 ** ----------------------------------------------------------------- */

void
print (const char * template, ...)
{
  va_list       ap;
  va_start(ap, template);
  vfprintf(stderr, template, ap);
  va_end(ap);
}
void
message (const char * template, ...)
{
  va_list       ap;
  va_start(ap, template);
  fprintf(stderr, "\n\n*** ");
  vfprintf(stderr, template, ap);
  fprintf(stderr, "\n\n");
  va_end(ap);
}
void
print_vector_float (const char * name, int N, float * X, int incX)
{
  int	i;
  print("vector %s: (", name);
  for (i=0; i<N; ++i) {
    if (0!=i)
      print(", ");
    print("%f", X[i*incX]);
  }
  print(")\n");
}
void
print_vector_double (const char * name, int N, double * X, int incX)
{
  int	i;
  print("vector %s: (", name);
  for (i=0; i<N; ++i) {
    if (0!=i)
      print(", ");
    print("%f", X[i*incX]);
  }
  print(")\n");
}
void
print_vector_cfloat (const char * name, int N, float * X, int incX)
{
  int	i, j;
  print("vector %s: (", name);
  for (i=0; i<2*N; i+=2) {
    if (0!=i)
      print(", ");
    j = i+1;
    print("%f+%fi", X[i*incX], X[j*incX]);
  }
  print(")\n");
}
void
print_vector_cdouble (const char * name, int N, double * X, int incX)
{
  int	i, j;
  print("vector %s: (", name);
  for (i=0; i<2*N; i+=2) {
    if (0!=i)
      print(", ");
    j = i+1;
    print("%f+%fi", X[i*incX], X[j*incX]);
  }
  print(")\n");
}

/* end of file */
