#include "kryzhovnik_wrapper.h"
#include <stdio.h>
#include <string.h>

int main() {
    uint8_t sk[KRYZHOVNIK_SECRET_KEY_BYTES];
    uint8_t pk[KRYZHOVNIK_PUBLIC_KEY_BYTES];
    if (kryzhovnik_keygen(sk, sizeof(sk), pk, sizeof(pk)) != KRYZHOVNIK_OK) {
        printf("Key generation failed\n");
        return 1;
    }

    const char *msg = "Hello, LWR!";
    size_t msg_len = strlen(msg);
    uint8_t sig[KRYZHOVNIK_SIGNATURE_BYTES];
    size_t sig_len = 0;

    if (kryzhovnik_sign(sk, sizeof(sk), pk, sizeof(pk),
                        (const uint8_t*)msg, msg_len,
                        sig, sizeof(sig), &sig_len) != KRYZHOVNIK_OK) {
        printf("Signing failed\n");
        return 1;
    }

    int ok = kryzhovnik_verify(pk, sizeof(pk), sig, sig_len,
                               (const uint8_t*)msg, msg_len);
    if (ok == KRYZHOVNIK_OK) {
        printf("Verification successful\n");
        return 0;
    } else {
        printf("Verification failed\n");
        return 1;
    }
}
