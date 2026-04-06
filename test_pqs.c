#include <stdio.h>
#include <string.h>
#include "kryzhovnik_wrapper.h"
#include <time.h>


#ifndef NTESTS
#define NTESTS 10
#endif
#ifndef MLEN_TEST
#define MLEN_TEST 20
#endif
#define MSECS(t) ((double)(t)/1e6)

#ifndef TEST_MODE
#define TEST_MODE 1
#endif

static double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}


#if TEST_MODE == 1
int main() {
    double tm;
    for (int loop = 0; loop < NTESTS; ++loop) {
        uint8_t sk[KRYZHOVNIK_SECRET_KEY_BYTES] = {0};
        uint8_t pk[KRYZHOVNIK_PUBLIC_KEY_BYTES] = {0};
        printf("Generating keypair ... ");
        tm = get_time();
        if (kryzhovnik_keygen(sk, sizeof(sk), pk, sizeof(pk)) != KRYZHOVNIK_OK) {
            printf("Key generation failed\n");
            return 1;
        }
        printf("Ok\n");
        printf("Keypair is generated in %.6f sec.\n", get_time() - tm);

        const char *msg = "My test message";
        size_t msg_len = strlen(msg);
        uint8_t sig[KRYZHOVNIK_SIGNATURE_BYTES] = {0};
        size_t sig_len = 0;

        tm = get_time();
        if (kryzhovnik_sign(sk, sizeof(sk), pk, sizeof(pk),
                            (const uint8_t*)msg, msg_len,
                            sig, sizeof(sig), &sig_len) != KRYZHOVNIK_OK) {
            printf("Signing failed\n");
            return 1;
        }
        printf("Signed Ok\n");
        printf("Signing is completed in %.6f sec.\n", get_time() - tm);

        tm = get_time();
        int success = kryzhovnik_verify(pk, sizeof(pk), sig, sig_len,
                        (const uint8_t*)msg, msg_len);
        if (success == KRYZHOVNIK_OK) {
            printf("Verify Ok\n");
        } else {
            printf("Verify fail\n");
            return 1;
        }
        printf("Verification is completed in %.6f sec.\n", get_time() - tm);
    }
    return 0;
}
#elif TEST_MODE == 2
int main() {
    double tm;
    tm = get_time();
    for (int loop = 0; loop < NTESTS; ++loop) {
        uint8_t sk[KRYZHOVNIK_SECRET_KEY_BYTES] = {0};
        uint8_t pk[KRYZHOVNIK_PUBLIC_KEY_BYTES] = {0};
        if (kryzhovnik_keygen(sk, sizeof(sk), pk, sizeof(pk)) != KRYZHOVNIK_OK) {
            printf("Key generation failed\n");
            return 1;
        }

        const char *msg = "My test message";
        size_t msg_len = strlen(msg);
        uint8_t sig[KRYZHOVNIK_SIGNATURE_BYTES] = {0};
        size_t sig_len = 0;

        if (kryzhovnik_sign(sk, sizeof(sk), pk, sizeof(pk),
                            (const uint8_t*)msg, msg_len,
                            sig, sizeof(sig), &sig_len) != KRYZHOVNIK_OK) {
            printf("Signing failed\n");
            return 1;
        }

        int success = kryzhovnik_verify(pk, sizeof(pk), sig, sig_len,
                        (const uint8_t*)msg, msg_len);
        if (success != KRYZHOVNIK_OK) {
            printf("Verify fail\n");
            return 1;
        }

        if (loop % 1000 == 0) {
            printf("%dk tests ok in %.6f sec.\n", loop / 1000, get_time() - tm);
            tm = get_time();
        }
    }
    return 0;
}
#elif TEST_MODE == 3
#include <stdlib.h>
static int cmp_llu(const void *a, const void *b) {
    if (*(unsigned long long *)a < *(unsigned long long *)b) return -1;
    if (*(unsigned long long *)a > *(unsigned long long *)b) return 1;
    return 0;
}
static unsigned long long median(unsigned long long *l, size_t llen) {
    qsort(l, llen, sizeof(unsigned long long), cmp_llu);
    if (llen % 2) return l[llen / 2];
    else return (l[llen / 2 - 1] + l[llen / 2]) / 2;
}
static unsigned long long average(unsigned long long *t, size_t tlen) {
    unsigned long long acc = 0;
    for (size_t i = 0; i < tlen; i++) acc += t[i];
    return acc / tlen;
}
static void print_results(const char *s, unsigned long long *t, size_t tlen) {
    unsigned long long tmp;
    printf("%s\n", s);
    tmp = median(t, tlen);
    printf("median: %llu ticks (%.4g msecs)\n", tmp, MSECS(tmp));
    tmp = average(t, tlen);
    printf("average: %llu ticks (%.4g msecs)\n", tmp, MSECS(tmp));
    printf("\n");
}
extern unsigned long long cpucycles_start();
extern unsigned long long cpucycles_stop();
extern unsigned long long cpucycles_overhead();
int main() {
    unsigned long long tkeygen[NTESTS], tsign[NTESTS], tverify[NTESTS];
    uint8_t m[MLEN_TEST] = {0};
    unsigned int i;
    int ret;
    unsigned long long timing_overhead = 0;
    timing_overhead = cpucycles_overhead();
    for (i = 0; i < NTESTS; ++i) {
        for (size_t j = 0; j < MLEN_TEST; ++j) m[j] = rand() & 0xFF;
        tkeygen[i] = cpucycles_start();
        uint8_t sk[KRYZHOVNIK_SECRET_KEY_BYTES] = {0};
        uint8_t pk[KRYZHOVNIK_PUBLIC_KEY_BYTES] = {0};
        if (kryzhovnik_keygen(sk, sizeof(sk), pk, sizeof(pk)) != KRYZHOVNIK_OK) {
            printf("Key generation failed\n");
            return -1;
        }
        tkeygen[i] = cpucycles_stop() - tkeygen[i] - timing_overhead;

        tsign[i] = cpucycles_start();
        uint8_t sig[KRYZHOVNIK_SIGNATURE_BYTES] = {0};
        size_t sig_len = 0;
        if (kryzhovnik_sign(sk, sizeof(sk), pk, sizeof(pk),
                            m, MLEN_TEST, sig, sizeof(sig),
                            &sig_len) != KRYZHOVNIK_OK) {
            printf("Signing failed\n");
            return -1;
        }
        tsign[i] = cpucycles_stop() - tsign[i] - timing_overhead;

        tverify[i] = cpucycles_start();
        ret = kryzhovnik_verify(pk, sizeof(pk), sig, sig_len, m, MLEN_TEST);
        tverify[i] = cpucycles_stop() - tverify[i] - timing_overhead;
        if (ret != KRYZHOVNIK_OK) {
            printf("Verification failed\n");
            return -1;
        }
    }
    print_results("keygen:", tkeygen, NTESTS);
    print_results("sign: ", tsign, NTESTS);
    print_results("verify: ", tverify, NTESTS);
    return 0;
}
#endif
