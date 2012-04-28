/*
  Part of: Vicare
  Contents: tests for CBLAS level 1 functions
  Date: Sun Dec 18, 2011

  Abstract

        The  purpose of  this program  is to  generate test  vectors and
        matrices for  CBLAS to be reused  in the test suite  of the BLAS
        interface in Vicare.

  Copyright (C) 2011 Marco Maggi <marco.maggi-ipsu@poste.it>

  This program is  free software: you can redistribute  it and/or modify
  it under the  terms of the GNU General Public  License as published by
  the Free Software Foundation, either  version 3 of the License, or (at
  your option) any later version.

  This program  is distributed in the  hope that it will  be useful, but
  WITHOUT   ANY  WARRANTY;   without  even   the  implied   warranty  of
  MERCHANTABILITY  or FITNESS  FOR A  PARTICULAR PURPOSE.   See  the GNU
  General Public License for more details.

  You  should have received  a copy  of the  GNU General  Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/** --------------------------------------------------------------------
 ** Headers.
 ** ----------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cblas.h>


/** --------------------------------------------------------------------
 ** Helpers.
 ** ----------------------------------------------------------------- */

static void
print (const char * template, ...)
{
  va_list       ap;
  va_start(ap, template);
  vfprintf(stderr, template, ap);
  va_end(ap);
}
static void
message (const char * template, ...)
{
  va_list       ap;
  va_start(ap, template);
  fprintf(stderr, "\n\n*** ");
  vfprintf(stderr, template, ap);
  fprintf(stderr, "\n\n");
  va_end(ap);
}
static void
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
static void
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
static void
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
static void
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


int
main (int argc, const char *const argv[])
{
  message("starting BLAS tests");

/* ------------------------------------------------------------------ */

  if (1) {	/* sdot */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    float	R;
    R = cblas_sdot(N, SX, incX, SY, incY);
    print("sdot: %f\n", R);
  }
#if 0 /* this function is broken in BLAS or CBLAS */
  if (1) {	/* sdsdot */
#undef N
#define N	3
    float	SA	= 1.0;
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    float	R;
    R = cblas_sdsdot(N, SA, SX, incX, SY, incY);
    print("sdsdot: %f\n", R);
  }
#endif
  if (1) {	/* dsdot */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    double	R;
    R = cblas_dsdot(N, SX, incX, SY, incY);
    print("dsdot: %f\n", R);
  }
  if (1) {	/* snrm2 */
#undef N
#define N	3
    int         incX	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	R;
    R = cblas_snrm2(N, SX, incX);
    print("snrm2: %f\n", R);
  }
  if (1) {	/* sasum */
#undef N
#define N	3
    int         incX	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	R;
    R = cblas_sasum(N, SX, incX);
    print("sasum: %f\n", R);
  }
  if (1) {	/* isamax */
#undef N
#define N	3
    int         incX	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    int		R;
    R = cblas_isamax(N, SX, incX);
    print("isamax: %d\n", R);
  }
  if (1) {	/* sswap */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    cblas_sswap(N, SX, incX, SY, incY);
    print("sswap: ");
    print_vector_float("SX", N, SX, incX);
  }
  if (1) {	/* scopy */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    cblas_scopy(N, SX, incX, SY, incY);
    print("scopy: ");
    print_vector_float("SY", N, SY, incY);
  }
  if (1) {	/* saxpy */
#undef N
#define N	3
    float	SA	= 9.0;
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    cblas_saxpy(N, SA, SX, incX, SY, incY);
    print("saxpy: ");
    print_vector_float("SY", N, SY, incY);
  }
  if (1) {	/* srotg */
    float	SA	= 1.0;
    float	SB	= 2.0;
    float	SC	= 0.0;
    float	SD	= 0.0;
    print("srogt before: SA=%f, SB=%f, SC=%f, SD=%f\n", SA, SB, SC, SD);
    cblas_srotg(&SA, &SB, &SC, &SD);
    print("srotg after:  SA=%f, SB=%f, SC=%f, SD=%f\n", SA, SB, SC, SD);
    print("check %f, must-be-zero=%f\n",
	  SC * 1.0 + SD * 2.0,
	  - SD * 1.0 + SC * 2.0);
  }
  if (1) {	/* srot */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    float	SC	= 8.0;
    float	SD	= 9.0;
    cblas_srot(N, SX, incX, SY, incY, SC, SD);
    print("srot: ");
    print_vector_float("SX", N, SX, incX);
    print_vector_float("SY", N, SY, incY);
  }
  if (1) {	/* srotmg */
#undef N
#define N	5
    float	SA	= 1.1;
    float	SB	= 2.2;
    float	SC	= 3.3;
    float	SD	= 4.4;
    float	SPARAM[N] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    int		incP	= 1;
    cblas_srotmg(&SA, &SB, &SC, SD, SPARAM);
    print("srotmg:  ");
    print_vector_float("SPARAM", N, SPARAM, incP);
  }
  if (1) {	/* srotm */
#undef N
#define N	3
    float	SA	= 1.1;
    float	SB	= 2.2;
    float	SC	= 3.3;
    float	SD	= 4.4;
    float	SPARAM[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    float	SY[N]	= { 4.0, 5.0, 6.0 };
    int		incX	= 1;
    int		incY	= 1;
    cblas_srotmg(&SA, &SB, &SC, SD, SPARAM);
    cblas_srotm(N, SX, incX, SY, incY, SPARAM);
    print("srotm:\n");
    print_vector_float("SX", N, SX, incX);
    print_vector_float("SY", N, SY, incY);
  }
  if (1) {	/* sscal */
#undef N
#define N	3
    float	SA	= 9.0;
    int         incX	= 1;
    float	SX[N]	= { 1.0, 2.0, 3.0 };
    cblas_sscal(N, SA, SX, incX);
    print("sscal: ");
    print_vector_float("SX", N, SX, incX);
  }

/* ------------------------------------------------------------------ */

  if (1) {	/* ddot */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	SX[N]	= { 1.0, 2.0, 3.0 };
    double	SY[N]	= { 4.0, 5.0, 6.0 };
    double	R;
    R = cblas_ddot(N, SX, incX, SY, incY);
    print("ddot: %f\n", R);
  }
  if (1) {	/* dnrm2 */
#undef N
#define N	3
    int         incX	= 1;
    double	SX[N]	= { 1.0, 2.0, 3.0 };
    double	R;
    R = cblas_dnrm2(N, SX, incX);
    print("dnrm2: %f\n", R);
  }
  if (1) {	/* dasum */
#undef N
#define N	3
    int         incX	= 1;
    double	SX[N]	= { 1.0, 2.0, 3.0 };
    double	R;
    R = cblas_dasum(N, SX, incX);
    print("dasum: %f\n", R);
  }
  if (1) {	/* idamax */
#undef N
#define N	3
    int         incX	= 1;
    double	SX[N]	= { 1.0, 2.0, 3.0 };
    int		R;
    R = cblas_idamax(N, SX, incX);
    print("idamax: %d\n", R);
  }
  if (1) {	/* dswap */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	SX[N]	= { 1.0, 2.0, 3.0 };
    double	SY[N]	= { 4.0, 5.0, 6.0 };
    cblas_dswap(N, SX, incX, SY, incY);
    print("dswap: ");
    print_vector_double("SX", N, SX, incX);
    print_vector_double("SY", N, SY, incY);
  }
  if (1) {	/* dcopy */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	DX[N]	= { 1.0, 2.0, 3.0 };
    double	DY[N]	= { 4.0, 5.0, 6.0 };
    cblas_dcopy(N, DX, incX, DY, incY);
    print("dcopy: ");
    print_vector_double("DX", N, DX, incX);
    print_vector_double("DY", N, DY, incY);
  }
  if (1) {	/* daxpy */
#undef N
#define N	3
    double	DA	= 9.0;
    int         incX	= 1;
    int		incY	= 1;
    double	DX[N]	= { 1.0, 2.0, 3.0 };
    double	DY[N]	= { 4.0, 5.0, 6.0 };
    cblas_daxpy(N, DA, DX, incX, DY, incY);
    print("daxpy: ");
    print_vector_double("DY", N, DY, incY);
  }
  if (1) {	/* drotg */
    double	DA	= 1.0;
    double	DB	= 2.0;
    double	DC	= 0.0;
    double	DD	= 0.0;
    print("drotg before: DA=%f, DB=%f, DC=%f, DD=%f\n", DA, DB, DC, DD);
    cblas_drotg(&DA, &DB, &DC, &DD);
    print("drotg after:  DA=%f, DB=%f, DC=%f, DD=%f\n", DA, DB, DC, DD);
    print("check %f, must-be-zero=%f\n",
	  DC * 1.0 + DD * 2.0,
	  - DD * 1.0 + DC * 2.0);
  }
  if (1) {	/* drot */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	DX[N]	= { 1.0, 2.0, 3.0 };
    double	DY[N]	= { 4.0, 5.0, 6.0 };
    double	DC	= 8.0;
    double	DD	= 9.0;
    cblas_drot(N, DX, incX, DY, incY, DC, DD);
    print("drot: ");
    print_vector_double("DX", N, DX, incX);
    print_vector_double("DY", N, DY, incY);
  }
  if (1) {	/* drotmg */
#undef N
#define N	5
    double	DA	= 1.1;
    double	DB	= 2.2;
    double	DC	= 3.3;
    double	DD	= 4.4;
    double	DPARAM[N] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    int		incP	= 1;
    cblas_drotmg(&DA, &DB, &DC, DD, DPARAM);
    print("drotmg:  ");
    print_vector_double("DPARAM", N, DPARAM, incP);
  }
  if (1) {	/* drotm */
#undef N
#define N	3
    double	DA	= 1.1;
    double	DB	= 2.2;
    double	DC	= 3.3;
    double	DD	= 4.4;
    double	DPARAM[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    double	DX[N]	= { 1.0, 2.0, 3.0 };
    double	DY[N]	= { 4.0, 5.0, 6.0 };
    int		incX	= 1;
    int		incY	= 1;
    cblas_drotmg(&DA, &DB, &DC, DD, DPARAM);
    cblas_drotm(N, DX, incX, DY, incY, DPARAM);
    print("drotm:\n");
    print_vector_double("DX", N, DX, incX);
    print_vector_double("DY", N, DY, incY);
  }
  if (1) {	/* dscal */
#undef N
#define N	3
    double	DA	= 9.0;
    int         incX	= 1;
    double	DX[N]	= { 1.0, 2.0, 3.0 };
    cblas_dscal(N, DA, DX, incX);
    print("dscal: ");
    print_vector_double("DX", N, DX, incX);
  }

/* ------------------------------------------------------------------ */

  if (1) {	/* cdotu */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	CY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    float	R;
    cblas_cdotu_sub(N, CX, incX, CY, incY, &R);
    print("cdotu: %f\n", R);
  }
  if (1) {	/* cdotc */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	CY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    float	R;
    cblas_cdotc_sub(N, CX, incX, CY, incY, &R);
    print("cdotc: %f\n", R);
  }
  if (1) {	/* scnrm2 */
#undef N
#define N	3
    int         incX	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	R;
    R = cblas_scnrm2(N, CX, incX);
    print("scnrm2: %f\n", R);
  }
  if (1) {	/* scasum */
#undef N
#define N	3
    int         incX	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	R;
    R = cblas_scasum(N, CX, incX);
    print("scasum: %f (expected %f)\n", R, 1.+3.+5+2.+4.+6.);
  }
  if (1) {	/* icamax */
#undef N
#define N	3
    int         incX	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    int		R;
    R = cblas_icamax(N, CX, incX);
    print("icamax: %d\n", R);
  }
  if (1) {	/* cswap */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	CY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    cblas_cswap(N, CX, incX, CY, incY);
    print("cswap:\n");
    print_vector_cfloat("CX", N, CX, incX);
    print_vector_cfloat("CY", N, CY, incY);
  }
  if (1) {	/* ccopy */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	CY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    cblas_ccopy(N, CX, incX, CY, incY);
    print("ccopy:\n");
    print_vector_cfloat("CX", N, CX, incX);
    print_vector_cfloat("CY", N, CY, incY);
  }
  if (1) {	/* caxpy */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	CY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    float	CA[2]	= { 8.0, 9.0 };
    cblas_caxpy(N, &CA, CX, incX, CY, incY);
    print("caxpy:\n");
    print_vector_cfloat("CY", N, CY, incY);
  }
  if (1) {	/* cscal */
#undef N
#define N	3
    int         incX	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	CA[2]	= { 8.0, 9.0 };
    cblas_cscal(N, CA, CX, incX);
    print("cscal: ");
    print_vector_cfloat("CX", N, CX, incX);
  }
  if (1) {	/* csscal */
#undef N
#define N	3
    int         incX	= 1;
    float	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    float	SA	= 9.0;
    cblas_csscal(N, SA, CX, incX);
    print("csscal: ");
    print_vector_cfloat("CX", N, CX, incX);
  }

/* ------------------------------------------------------------------ */

  if (1) {	/* zdotu */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	ZY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    double	R;
    cblas_zdotu_sub(N, ZX, incX, ZY, incY, &R);
    print("zdotu: %f\n", R);
  }
  if (1) {	/* zdotc */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	ZY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    double	R;
    cblas_zdotc_sub(N, ZX, incX, ZY, incY, &R);
    print("zdotc: %f\n", R);
  }
  if (1) {	/* dznrm2 */
#undef N
#define N	3
    int         incX	= 1;
    double	CX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	R;
    R = cblas_dznrm2(N, CX, incX);
    print("dznrm2: %f\n", R);
  }
  if (1) {	/* dzasum */
#undef N
#define N	3
    int         incX	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	R;
    R = cblas_dzasum(N, ZX, incX);
    print("dzasum: %f (expected %f)\n", R, 1.+3.+5+2.+4.+6.);
  }
  if (1) {	/* izamax */
#undef N
#define N	3
    int         incX	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    int		R;
    R = cblas_izamax(N, ZX, incX);
    print("izamax: %d\n", R);
  }
  if (1) {	/* zswap */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	ZY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    cblas_zswap(N, ZX, incX, ZY, incY);
    print("zswap:\n");
    print_vector_cdouble("ZX", N, ZX, incX);
    print_vector_cdouble("ZY", N, ZY, incY);
  }
  if (1) {	/* zcopy */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	ZY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    cblas_zcopy(N, ZX, incX, ZY, incY);
    print("zcopy:\n");
    print_vector_cdouble("ZX", N, ZX, incX);
    print_vector_cdouble("ZY", N, ZY, incY);
  }
  if (1) {	/* zaxpy */
#undef N
#define N	3
    int         incX	= 1;
    int		incY	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	ZY[2*N]	= { 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
    double	ZA[2]	= { 8.0, 9.0 };
    cblas_zaxpy(N, &ZA, ZX, incX, ZY, incY);
    print("zaxpy:\n");
    print_vector_cdouble("ZY", N, ZY, incY);
  }
  if (1) {	/* zdscal */
#undef N
#define N	3
    int         incX	= 1;
    double	ZX[2*N]	= { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    double	DA	= 9.0;
    cblas_zdscal(N, DA, ZX, incX);
    print("zdscal: ");
    print_vector_cdouble("ZX", N, ZX, incX);
  }

  message("end BLAS tests");
  exit(EXIT_SUCCESS);
}

/* end of file */
