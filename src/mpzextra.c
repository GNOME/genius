/* GENIUS Calculator
 * Copyright (C) 1997-2003 George Lebl
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

#include "config.h"

#include <gnome.h>
#include <locale.h>

#include <string.h>
#include <glib.h>

#include "mpzextra.h"

/* The strong pseudoprime test code copied from GMP */

static int
strongpseudoprimesub (mpz_srcptr n, mpz_srcptr nm1, mpz_srcptr x, mpz_ptr y,
		      mpz_srcptr q, unsigned long int k)
{
  unsigned long int i;

  mpz_powm (y, x, q, n);

  if (mpz_cmp_ui (y, 1L) == 0 || mpz_cmp (y, nm1) == 0)
    return 1;

  for (i = 1; i < k; i++)
    {
      mpz_powm_ui (y, y, 2L, n);
      if (mpz_cmp (y, nm1) == 0)
	return 1;
      if (mpz_cmp_ui (y, 1L) == 0)
	return 0;
    }
  return 0;
}


gboolean
mympz_strong_pseudoprime_test (mpz_srcptr n, mpz_srcptr b)
{
  mpz_t nm1, y, q;
  unsigned long int k;
  int is_prime;
  unsigned long int size_n  = mpz_size (n) * GMP_LIMB_BITS;

  mpz_init2 (nm1, size_n + GMP_LIMB_BITS);
  mpz_sub_ui (nm1, n, 1L);

  mpz_init2 (y, 2 * size_n); /* mpz_powm_ui needs excessive memory!!! */

  mpz_init2 (q, size_n);

  /* Find q and k, where q is odd and n = 1 + 2**k * q.  */
  k = mpz_scan1 (nm1, 0L);
  mpz_tdiv_q_2exp (q, nm1, k);

  is_prime = strongpseudoprimesub (n, nm1, b, y, q, k);

  mpz_clear (nm1);
  mpz_clear (y);
  mpz_clear (q);

  return is_prime;
}

/* return true only if strong pseudoprime to all of 2,3,5 and 7 */
static gboolean
mympz_strong_pseudoprime_test_2_3_5_7 (mpz_srcptr n)
{
  mpz_t nm1, y, q, b;
  unsigned long int k;
  int is_prime;
  unsigned long int size_n  = mpz_size (n) * GMP_LIMB_BITS;

  mpz_init2 (nm1, size_n + GMP_LIMB_BITS);
  mpz_sub_ui (nm1, n, 1L);

  mpz_init2 (y, 2 * size_n); /* mpz_powm_ui needs excessive memory!!! */

  mpz_init2 (q, size_n);

  /* Find q and k, where q is odd and n = 1 + 2**k * q.  */
  k = mpz_scan1 (nm1, 0L);
  mpz_tdiv_q_2exp (q, nm1, k);

  is_prime = 0;
  mpz_init_set_ui (b, 2);
  if (strongpseudoprimesub (n, nm1, b, y, q, k)) {
	  mpz_set_ui (b, 3);
	  if (strongpseudoprimesub (n, nm1, b, y, q, k)) {
		  mpz_set_ui (b, 5);
		  if (strongpseudoprimesub (n, nm1, b, y, q, k)) {
			  mpz_set_ui (b, 7);
			  if (strongpseudoprimesub (n, nm1, b, y, q, k)) {
				  is_prime = 1;
			  }
		  }
	  }
  }

  mpz_clear (b);
  mpz_clear (nm1);
  mpz_clear (y);
  mpz_clear (q);

  return is_prime;
}

/* assuming generalized riemman hypothesis, if we test for
   every base from 2 to 2*lg(n)^2.
ref:
   Neil Koblitz, A Course in Number Theory and Cryptography, Springer, 1987 */
gboolean
mympz_miller_rabin_test_sure (mpz_srcptr n)
{
  mpz_t nm1, y, q, b, m;
  unsigned long int k;
  int is_prime;
  unsigned long int size_n  = mpz_size (n) * GMP_LIMB_BITS;

  mpz_init2 (nm1, size_n + GMP_LIMB_BITS);
  mpz_sub_ui (nm1, n, 1L);

  mpz_init2 (y, 2 * size_n); /* mpz_powm_ui needs excessive memory!!! */

  mpz_init2 (q, size_n);

  /* Find q and k, where q is odd and n = 1 + 2**k * q.  */
  k = mpz_scan1 (nm1, 0L);
  mpz_tdiv_q_2exp (q, nm1, k);

  /* compute 2*lg(n)^2 */
  mpz_init_set_ui (m, mpz_sizeinbase (n, 2));
  mpz_mul (m, m, m);
  mpz_mul_ui (m, m, 2);

  is_prime = 1;
  mpz_init_set_ui (b, 2);
  do {
	  if ( ! strongpseudoprimesub (n, nm1, b, y, q, k)) {
		  is_prime = 0;
		  break;
	  }
	  mpz_add_ui (b, b, 1L);
  } while (mpz_cmp (b, m) <= 0);

  mpz_clear (b);
  mpz_clear (m);
  mpz_clear (nm1);
  mpz_clear (y);
  mpz_clear (q);

  return is_prime;
}

/* we will really do 10+4 since we always test 2,3,5,7 as well,
   so the probability that a wrong result is returned is
   about 1/(4^26) which is about 8.9*10^-16.  Knuth says
   25 passes are reasonable (according to gmp source),
   and Knuth is never wrong. */
long int mympz_is_prime_miller_rabin_reps = 21;

gboolean
mympz_is_prime (mpz_srcptr n, int miller_rabin_reps)
{
	int ret;
	static mpz_t test;
	static gboolean inited_test = FALSE; 
	int sgn;

	sgn = mpz_sgn (n);
	if (sgn == 0)
		return 0;
	else if (sgn < 0) {
		mpz_t nn;
		mpz_init (nn);
		mpz_neg (nn, n);
		ret = mympz_is_prime (nn, miller_rabin_reps);
		mpz_clear (nn);
		return ret;
	}

	if (miller_rabin_reps < 0)
		miller_rabin_reps = mympz_is_prime_miller_rabin_reps;

	/* Use the probab prime for trial divisions and stuff
	   and do one strong pseudoprime test for good meassure */
	ret = mpz_probab_prime_p (n, 1);
	/* if we are sure */
	if (ret == 2)
		return 1;
	else if (ret == 0)
		return 0;

	if ( ! mympz_strong_pseudoprime_test_2_3_5_7 (n))
		return 0;

	if ( ! inited_test) {
		/* set test to 25*10^9 */
		mpz_init_set_ui (test, 10);
		mpz_pow_ui (test, test, 9);
		mpz_mul_ui (test, test, 25);
	}

	/* if n < 25*10^9, we are now sure this
	   is a prime since the only n less then that
	   that is a composite and strong pseudoprime to 2,3,5,7
	   is n = 3215031751
        ref:
           Neil Koblitz, A Course in Number Theory and Cryptography,
	   Springer, 1987 */
	if (mpz_cmp (n, test) <= 0)
		return 1;

	return mpz_millerrabin (n, miller_rabin_reps-1);
}
