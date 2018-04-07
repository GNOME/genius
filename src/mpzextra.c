/* GENIUS Calculator
 * Copyright (C) 1997-2017 Jiri (George) Lebl
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

#include "config.h"

#include <string.h>
#include <glib.h>

#include "calc.h" /* for gel_evalnode_hook and i18n stuff */

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
	  if (gel_evalnode_hook != NULL) {
		  (*gel_evalnode_hook)();
	  }
	  if G_UNLIKELY (gel_interrupted) {
		  is_prime = 0;
		  break;
	  }
  } while (mpz_cmp (b, m) <= 0);

  mpz_clear (b);
  mpz_clear (m);
  mpz_clear (nm1);
  mpz_clear (y);
  mpz_clear (q);

  return is_prime;
}

/* we will really do 22+4 since we always test 2,3,5,7 as well,
   so the probability that a wrong result is returned is
   about 1/(4^26) which is about 8.9*10^-16.  Knuth says
   25 passes are reasonable (according to gmp source),
   and Knuth is never wrong. */
long int mympz_is_prime_miller_rabin_reps = 22;

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

	if G_UNLIKELY ( ! inited_test) {
		/* set test to 25*10^9 */
		mpz_init_set_ui (test, 10);
		mpz_pow_ui (test, test, 9);
		mpz_mul_ui (test, test, 25);

		inited_test = TRUE;
	}

	/* if n < 25*10^9, we are now sure this
	   is a prime since the only n less than that
	   that is a composite and strong pseudoprime to 2,3,5,7
	   is n = 3215031751
        ref:
           Neil Koblitz, A Course in Number Theory and Cryptography,
	   Springer, 1987 */
	if (mpz_cmp (n, test) <= 0)
		return 1;

	if (gel_evalnode_hook != NULL)
		(*gel_evalnode_hook)();
	if G_UNLIKELY (gel_interrupted)
		return 0;

	return mpz_millerrabin (n, miller_rabin_reps-1);
}

static void
append_factor (GArray *fact, mpz_srcptr num)
{
	GelFactor f;
	guint i;
	/* FIXME: implement faster search, this is sorted */
	/* We start at 1 since the 0 entry is always -1 or 1 */
	for (i = 1; i < fact->len; i++) {
		int cmp = mpz_cmp (g_array_index (fact, GelFactor, i).num, num);
		if (cmp > 0) {
			/* must add */
			mpz_init_set (f.num, num);
			f.exp = 1;
			g_array_insert_val (fact, i, f);
			return;
		} else if (cmp == 0) {
			/* already in the factorization */
			g_array_index (fact, GelFactor, i).exp++;
			return;
		}
	}
	/* not found, must add */
	mpz_init_set (f.num, num);
	f.exp = 1;
	g_array_append_val (fact, f);
}

static void
append_factor_uint (GArray *fact, unsigned long num)
{
	mpz_t znum;
	mpz_init_set_ui (znum, num);
	append_factor (fact, znum);
	mpz_clear (znum);
}

/* Pollard-Rho factorization code snarfed from the gmp examples */

static unsigned add[] = {4, 2, 4, 2, 4, 6, 2, 6};

static void
factor_using_division (GArray *fact, mpz_t t, unsigned int limit)
{
  mpz_t q, r;
  unsigned long int f;
  int ai;
  unsigned *addv = add;
  unsigned int failures;

  mpz_init (q);
  mpz_init (r);

  f = mpz_scan1 (t, 0);
  mpz_div_2exp (t, t, f);
  while (f)
    {
      append_factor_uint (fact, 2);
      --f;
    }

  for (;;)
    {
      mpz_tdiv_qr_ui (q, r, t, 3);
      if (mpz_cmp_ui (r, 0) != 0)
	break;
      mpz_set (t, q);
      append_factor_uint (fact, 3);
    }

  for (;;)
    {
      mpz_tdiv_qr_ui (q, r, t, 5);
      if (mpz_cmp_ui (r, 0) != 0)
	break;
      mpz_set (t, q);
      append_factor_uint (fact, 5);
    }

  failures = 0;
  f = 7;
  ai = 0;
  while (mpz_cmp_ui (t, 1) != 0)
    {
      mpz_tdiv_qr_ui (q, r, t, f);
      if (mpz_cmp_ui (r, 0) != 0)
	{
	  f += addv[ai];
	  if (mpz_cmp_ui (q, f) < 0)
	    break;
	  ai = (ai + 1) & 7;
	  failures++;
	  if (failures > limit)
	    break;
	}
      else
	{
	  mpz_swap (t, q);
	  append_factor_uint (fact, f);
	  failures = 0;
	}
    }

  mpz_clear (q);
  mpz_clear (r);
}

static void
factor_using_pollard_rho (GArray *fact, mpz_t n, int a_int)
{
  mpz_t x, x1, y, P;
  mpz_t a;
  mpz_t g;
  mpz_t t1, t2;
  int k, l, c, i;

  mpz_init (g);
  mpz_init (t1);
  mpz_init (t2);

  mpz_init_set_si (a, a_int);
  mpz_init_set_si (y, 2);
  mpz_init_set_si (x, 2);
  mpz_init_set_si (x1, 2);
  k = 1;
  l = 1;
  mpz_init_set_ui (P, 1);
  c = 0;

  while (mpz_cmp_ui (n, 1) != 0)
    {
S2:
      if (gel_evalnode_hook != NULL) {
	      static int ii = 0;
	      if G_UNLIKELY ((ii++ & GEL_RUN_HOOK_EVERY_MASK) == GEL_RUN_HOOK_EVERY_MASK) {
		      (*gel_evalnode_hook)();
		      ii = 0;
	      }
      }
      if G_UNLIKELY (gel_interrupted) {
	      mpz_set_ui (n, 1);
	      continue;
      }
      mpz_mul (x, x, x); mpz_add (x, x, a); mpz_mod (x, x, n);
      mpz_sub (t1, x1, x); mpz_mul (t2, P, t1); mpz_mod (P, t2, n);
      c++;
      if (c == 20)
	{
	  c = 0;
	  mpz_gcd (g, P, n);
	  if (mpz_cmp_ui (g, 1) != 0)
	    goto S4;
	  mpz_set (y, x);
	}
      k--;
      if (k != 0)
	goto S2;

      mpz_gcd (g, P, n);
      if (mpz_cmp_ui (g, 1) != 0)
	goto S4;

      mpz_set (x1, x);
      k = l;
      l = 2 * l;
      for (i = 0; i < k; i++)
	{
	  mpz_mul (x, x, x); mpz_add (x, x, a); mpz_mod (x, x, n);
	}
      mpz_set (y, x);
      c = 0;
      goto S2;
S4:
      do
	{
	  mpz_mul (y, y, y); mpz_add (y, y, a); mpz_mod (y, y, n);
	  mpz_sub (t1, x1, y); mpz_gcd (g, t1, n);
	}
      while (mpz_cmp_ui (g, 1) == 0);

      if (!mympz_is_prime (g, -1))
	{
	  do
            {
              mp_limb_t a_limb;
              mpn_random (&a_limb, (mp_size_t) 1);
              a_int = (int) a_limb;
            }
	  while (a_int == -2 || a_int == 0);

	  factor_using_pollard_rho (fact, g, a_int);
	  break;
	}
      else
	{
	  append_factor (fact, g);
	}
      mpz_div (n, n, g);
      mpz_mod (x, x, n);
      mpz_mod (x1, x1, n);
      mpz_mod (y, y, n);
      if (mympz_is_prime (n, -1))
	{
	  append_factor (fact, n);
	  break;
	}
    }

  mpz_clear (g);
  mpz_clear (P);
  mpz_clear (t2);
  mpz_clear (t1);
  mpz_clear (a);
  mpz_clear (x1);
  mpz_clear (x);
  mpz_clear (y);
}

static void
factor_number (GArray *fact, mpz_t t)
{
  unsigned int division_limit;

  /* Set the trial division limit according the size of t.  */
  division_limit = mpz_sizeinbase (t, 2);
  if (division_limit > 1000)
    division_limit = 1000 * 1000;
  else
    division_limit = division_limit * division_limit;

  factor_using_division (fact, t, division_limit);

  if (mpz_cmp_ui (t, 1) != 0)
    {
      if (mympz_is_prime (t, -1))
	append_factor (fact, t);
      else
	factor_using_pollard_rho (fact, t, 1);
    }
}

GArray *
mympz_pollard_rho_factorize (mpz_srcptr t)
{
	GArray *fact;
	mpz_t n;
	int sgn = mpz_sgn (t);

	fact = g_array_new (FALSE /* zero_terminated */,
			    FALSE /* clear */,
			    sizeof (GelFactor) /* element_size */);

	mpz_init_set (n, t);

	if (sgn == 0) {
		append_factor_uint (fact, 0);
	} if (sgn < 0) {
		GelFactor f;
		/* for negative numbers, add -1 to the factors
		   and factorize -t */
		mpz_neg (n, n);

		mpz_init_set_si (f.num, -1);
		f.exp = 1;
		g_array_append_val (fact, f);

		factor_number (fact, n);
	} else {
		append_factor_uint (fact, 1);
		/* just factor the number */
		factor_number (fact, n);
	}

	mpz_clear (n);

	if G_UNLIKELY (gel_interrupted) {
		mympz_factorization_free (fact);
		return NULL;
	}

	return fact;

}

void
mympz_factorization_free (GArray *fact)
{
	guint i;
	for (i = 0; i < fact->len; i++) {
		mpz_clear (g_array_index (fact, GelFactor, i).num);
	}
	g_array_free (fact, TRUE /* free_segment */);
}
