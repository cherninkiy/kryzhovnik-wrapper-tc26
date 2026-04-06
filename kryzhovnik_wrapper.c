#include <string.h>
#include "kryzhovnik_wrapper.h"
#include "params.h"
#include "sign.h"

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

void kryzhovnik_generate_keys(uint8_t *sk, uint8_t *pk) {
    vk_t vk;
    sk_t s;
    PQS_keygen(vk, s);
    unpack_vk(&vk, pk);
    unpack_sk(&s, sk);
}

void kryzhovnik_sign(const uint8_t *sk, const uint8_t *pk,
                     const uint8_t *msg, size_t msg_len,
                     uint8_t *sig, size_t *sig_len) {
    sk_t s;
    vk_t vk;
    signat_t signature;

    pack_sk(&s, sk);
    pack_vk(&vk, pk);
    PQS_sign(signature, msg, (uint32_t)msg_len, s, vk);
    unpack_sig(&signature, sig);
    if (sig_len) {
        *sig_len = KRYZHOVNIK_SIGNATURE_BYTES;
    }
}

int kryzhovnik_verify(const uint8_t *pk, const uint8_t *sig,
                      const uint8_t *msg, size_t msg_len) {
    vk_t vk;
    signat_t signature;

    pack_vk(&vk, pk);
    pack_sig(&signature, sig);

    return PQS_verify(signature, msg, (uint32_t)msg_len, vk) ? 0 : 1;
}
