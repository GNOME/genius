/* GENIUS Calculator
 * Copyright (C) 1997-2003 Jiri (George) Lebl
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

#ifndef MPZEXTRA_H
#define MPZEXTRA_H

#include "mpwrap.h"

gboolean mympz_strong_pseudoprime_test (mpz_srcptr n, mpz_srcptr b);
gboolean mympz_is_prime (mpz_srcptr n, int miller_rabin_reps);
gboolean mympz_miller_rabin_test_sure (mpz_srcptr n);

extern long int mympz_is_prime_miller_rabin_reps;

typedef struct {
	mpz_t num;
	unsigned long exp; 
} GelFactor;

GArray * mympz_pollard_rho_factorize (mpz_srcptr t);
void mympz_factorization_free (GArray *fact);


#endif /* MPZEXTRA_H */
