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

#ifndef MPZEXTRA_H
#define MPZEXTRA_H

#include "mpwrap.h"

gboolean mympz_strong_pseudoprime_test (mpz_srcptr n, mpz_srcptr b);
gboolean mympz_is_prime (mpz_srcptr n, int miller_rabin_reps);
gboolean mympz_miller_rabin_test_sure (mpz_srcptr n);

extern long int mympz_is_prime_miller_rabin_reps;

#endif /* MPZEXTRA_H */
