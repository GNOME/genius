/*
  Part of: ATLAPACK
  Contents: test for DGESVD
  Date: Tue Dec 27, 2011

  Abstract

	This test file is derived from the CLAPACK FAQ.

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

#include "test-utils.h"

#define M		3
#define N		3

int
main (int argc, const char *const argv[])
{
  int		matrix_order	= LAPACK_ROW_MAJOR;
  char		jobu		= 'A';
  char		jobvt		= 'A';
  double	DA[M*N]		= { 1.0, 2.0, 3.0,
				    4.0, 5.0, 6.0,
				    7.0, 8.0, 9.0 };
  int		ldA		= M;
  double	DS[M*N];
  double	DU[M*M];
  int		ldU		= M;
  double	VT[N*N];
  int		ldVT		= N;
  double	superb[M];
  int		info;

  info = LAPACKE_dgesvd(matrix_order, jobu, jobvt, M, N, DA, ldA,
			DS, DU, ldU, VT, ldVT, superb);

  print("dgesvd: info=%d\n", info);
  exit(EXIT_SUCCESS);
}

/* end of file */
