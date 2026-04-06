#ifndef KRYZHOVNIK_WRAPPER_H
#define KRYZHOVNIK_WRAPPER_H

#include <stddef.h>
#include <stdint.h>

// Actual sizes of key and signature structures (in bytes)
#define KRYZHOVNIK_PUBLIC_KEY_BYTES   4128
#define KRYZHOVNIK_SECRET_KEY_BYTES   3072
#define KRYZHOVNIK_SIGNATURE_BYTES    4096

typedef enum kryzhovnik_status {
    KRYZHOVNIK_OK = 0,
    KRYZHOVNIK_BAD_ARG = 1,
    KRYZHOVNIK_VERIFY_FAIL = 2,
    KRYZHOVNIK_INTERNAL = 3,
} kryzhovnik_status;

#ifdef __cplusplus
extern "C" {
#endif

int kryzhovnik_keygen(uint8_t *sk, size_t sk_capacity,
                      uint8_t *pk, size_t pk_capacity);
int kryzhovnik_sign(const uint8_t *sk, size_t sk_len,
                    const uint8_t *pk, size_t pk_len,
                    const uint8_t *msg, size_t msg_len,
                    uint8_t *sig, size_t sig_capacity,
                    size_t *sig_len);
int kryzhovnik_verify(const uint8_t *pk, size_t pk_len,
                      const uint8_t *sig, size_t sig_len,
                      const uint8_t *msg, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif // KRYZHOVNIK_WRAPPER_H
