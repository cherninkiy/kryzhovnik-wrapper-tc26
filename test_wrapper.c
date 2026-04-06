#include "kryzhovnik_wrapper.h"
#include <stdio.h>
#include <string.h>

int main() {
    uint8_t sk[KRYZHOVNIK_SECRET_KEY_BYTES];
    uint8_t pk[KRYZHOVNIK_PUBLIC_KEY_BYTES];
    kryzhovnik_generate_keys(sk, pk);

    const char *msg = "Hello, LWR!";
    size_t msg_len = strlen(msg);
    uint8_t sig[KRYZHOVNIK_SIGNATURE_BYTES];
    size_t sig_len = 0;

    kryzhovnik_sign(sk, pk, (const uint8_t*)msg, msg_len, sig, &sig_len);

    int ok = kryzhovnik_verify(pk, sig, (const uint8_t*)msg, msg_len);
    if (ok == 0) {
        printf("Verification successful\n");
        return 0;
    } else {
        printf("Verification failed\n");
        return 1;
    }
}
