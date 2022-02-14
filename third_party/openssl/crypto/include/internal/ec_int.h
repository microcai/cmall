/*
 * Copyright 2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Internal EC functions for other submodules: not for application use */

#ifndef HEADER_OSSL_EC_INTERNAL_H
# define HEADER_OSSL_EC_INTERNAL_H
# include <openssl/opensslconf.h>

# ifndef OPENSSL_NO_EC

#  include <openssl/ec.h>

/*-
 * Computes the multiplicative inverse of x in the range
 * [1,EC_GROUP::order), where EC_GROUP::order is the cardinality of the
 * subgroup generated by the generator G:
 *
 *         res := x^(-1) (mod EC_GROUP::order).
 *
 * This function expects the following two conditions to hold:
 *  - the EC_GROUP order is prime, and
 *  - x is included in the range [1, EC_GROUP::order).
 *
 * This function returns 1 on success, 0 on error.
 *
 * If the EC_GROUP order is even, this function explicitly returns 0 as
 * an error.
 * In case any of the two conditions stated above is not satisfied,
 * the correctness of its output is not guaranteed, even if the return
 * value could still be 1 (as primality testing and a conditional modular
 * reduction round on the input can be omitted by the underlying
 * implementations for better SCA properties on regular input values).
 */
__owur int ec_group_do_inverse_ord(const EC_GROUP *group, BIGNUM *res,
                                   const BIGNUM *x, BN_CTX *ctx);

/*-
 * ECDH Key Derivation Function as defined in ANSI X9.63
 */
int ecdh_KDF_X9_63(unsigned char *out, size_t outlen,
                   const unsigned char *Z, size_t Zlen,
                   const unsigned char *sinfo, size_t sinfolen,
                   const EVP_MD *md);

# endif /* OPENSSL_NO_EC */
#endif
