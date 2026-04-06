#ifndef KRYZHOVNIK_WRAPPER_H
#define KRYZHOVNIK_WRAPPER_H

#include <stddef.h>
#include <stdint.h>


// Actual sizes of key and signature structures (in bytes)
#define KRYZHOVNIK_PUBLIC_KEY_BYTES   4128
#define KRYZHOVNIK_SECRET_KEY_BYTES   3072
#define KRYZHOVNIK_SIGNATURE_BYTES    4096

#ifdef __cplusplus
extern "C" {
#endif

void kryzhovnik_generate_keys(uint8_t *sk, uint8_t *pk);
void kryzhovnik_sign(const uint8_t *sk, const uint8_t *pk,
                     const uint8_t *msg, size_t msg_len,
                     uint8_t *sig, size_t *sig_len);
int kryzhovnik_verify(const uint8_t *pk, const uint8_t *sig,
                      const uint8_t *msg, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif // KRYZHOVNIK_WRAPPER_H
