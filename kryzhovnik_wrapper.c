#include <string.h>
#include "kryzhovnik_wrapper.h"
#include "params.h"
#include "sign.h"

#define KRYZHOVNIK_CT_ASSERT(name, cond) typedef char name[(cond) ? 1 : -1]

KRYZHOVNIK_CT_ASSERT(kryzhovnik_sk_size_matches,
    KRYZHOVNIK_SECRET_KEY_BYTES == ((size_t)PQS_l * (size_t)PQS_n * sizeof(uint32_t)));
KRYZHOVNIK_CT_ASSERT(kryzhovnik_pk_size_matches,
    KRYZHOVNIK_PUBLIC_KEY_BYTES == ((size_t)SEEDBYTES + (size_t)PQS_k * (size_t)PQS_n * sizeof(uint32_t)));
KRYZHOVNIK_CT_ASSERT(kryzhovnik_sig_size_matches,
    KRYZHOVNIK_SIGNATURE_BYTES == ((size_t)(1 + PQS_l) * (size_t)PQS_n * sizeof(uint32_t)));

/*
 * The wrapper exposes a byte-oriented API around the original reference code,
 * which operates on nested C/C++ structures. Direct memcpy is unsafe here
 * because the structures may contain padding or rely on layout details.
 *
 * The implementation therefore performs explicit field-by-field serialization
 * for public keys, secret keys, and signatures. Signing also requires the
 * public key because PQS_sign uses vk.seedA and vk.t internally.
 */

// -----------------------------------------------------------------------------
// Serialization helpers (little-endian, explicit field-by-field)

static uint32_t load_le(const uint8_t *p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void store_le(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

// poly
static void pack_poly(uint8_t *dst, const poly *src) {
    for (int j = 0; j < PQS_n; ++j)
        store_le(dst + 4 * j, src->coeffs[j]);
}
static void unpack_poly(poly *dst, const uint8_t *src) {
    for (int j = 0; j < PQS_n; ++j)
        dst->coeffs[j] = load_le(src + 4 * j);
}

// polyvecl
static void pack_polyvecl(uint8_t *dst, const polyvecl *src) {
    for (int i = 0; i < PQS_l; ++i)
        pack_poly(dst + i * PQS_n * 4, &src->polynomial[i]);
}
static void unpack_polyvecl(polyvecl *dst, const uint8_t *src) {
    for (int i = 0; i < PQS_l; ++i)
        unpack_poly(&dst->polynomial[i], src + i * PQS_n * 4);
}

// polyveck
static void pack_polyveck(uint8_t *dst, const polyveck *src) {
    for (int i = 0; i < PQS_k; ++i)
        pack_poly(dst + i * PQS_n * 4, &src->polynomial[i]);
}
static void unpack_polyveck(polyveck *dst, const uint8_t *src) {
    for (int i = 0; i < PQS_k; ++i)
        unpack_poly(&dst->polynomial[i], src + i * PQS_n * 4);
}

// vk_t
static void pack_vk(vk_t *dst, const uint8_t *src) {
    memcpy(dst->seedA, src, SEEDBYTES);
    unpack_polyveck(&dst->t, src + SEEDBYTES);
}
static void unpack_vk(const vk_t *src, uint8_t *dst) {
    memcpy(dst, src->seedA, SEEDBYTES);
    pack_polyveck(dst + SEEDBYTES, &src->t);
}

// sk_t
static void pack_sk(sk_t *dst, const uint8_t *src) {
    unpack_polyvecl(&dst->s, src);
}
static void unpack_sk(const sk_t *src, uint8_t *dst) {
    pack_polyvecl(dst, &src->s);
}

// signat_t
static void pack_sig(signat_t *dst, const uint8_t *src) {
    unpack_poly(&dst->c, src);
    unpack_polyvecl(&dst->z, src + PQS_n * 4);
}
static void unpack_sig(const signat_t *src, uint8_t *dst) {
    pack_poly(dst, &src->c);
    pack_polyvecl(dst + PQS_n * 4, &src->z);
}

// -----------------------------------------------------------------------------
// Wrapper API implementation

int kryzhovnik_keygen(uint8_t *sk, size_t sk_capacity,
                      uint8_t *pk, size_t pk_capacity) {
    if (sk == NULL || pk == NULL) {
        return KRYZHOVNIK_BAD_ARG;
    }
    if (sk_capacity < KRYZHOVNIK_SECRET_KEY_BYTES ||
        pk_capacity < KRYZHOVNIK_PUBLIC_KEY_BYTES) {
        return KRYZHOVNIK_BAD_ARG;
    }

    vk_t vk;
    sk_t s;
    PQS_keygen(&vk, &s);
    unpack_vk(&vk, pk);
    unpack_sk(&s, sk);
    return KRYZHOVNIK_OK;
}

int kryzhovnik_sign(const uint8_t *sk, size_t sk_len,
                    const uint8_t *pk, size_t pk_len,
                    const uint8_t *msg, size_t msg_len,
                    uint8_t *sig, size_t sig_capacity,
                    size_t *sig_len) {
    sk_t s;
    vk_t vk;
    signat_t signature;

    if (sk == NULL || pk == NULL || msg == NULL || sig == NULL || sig_len == NULL) {
        return KRYZHOVNIK_BAD_ARG;
    }
    if (sk_len != KRYZHOVNIK_SECRET_KEY_BYTES ||
        pk_len != KRYZHOVNIK_PUBLIC_KEY_BYTES ||
        sig_capacity < KRYZHOVNIK_SIGNATURE_BYTES) {
        return KRYZHOVNIK_BAD_ARG;
    }

    pack_sk(&s, sk);
    pack_vk(&vk, pk);
    PQS_sign(&signature, msg, (uint32_t)msg_len, &s, &vk);
    unpack_sig(&signature, sig);
    *sig_len = KRYZHOVNIK_SIGNATURE_BYTES;
    return KRYZHOVNIK_OK;
}

int kryzhovnik_verify(const uint8_t *pk, size_t pk_len,
                      const uint8_t *sig, size_t sig_len,
                      const uint8_t *msg, size_t msg_len) {
    vk_t vk;
    signat_t signature;

    if (pk == NULL || sig == NULL || msg == NULL) {
        return KRYZHOVNIK_BAD_ARG;
    }
    if (pk_len != KRYZHOVNIK_PUBLIC_KEY_BYTES ||
        sig_len != KRYZHOVNIK_SIGNATURE_BYTES) {
        return KRYZHOVNIK_BAD_ARG;
    }

    pack_vk(&vk, pk);
    pack_sig(&signature, sig);

    return PQS_verify(&signature, msg, (uint32_t)msg_len, &vk)
               ? KRYZHOVNIK_OK
               : KRYZHOVNIK_VERIFY_FAIL;
}
