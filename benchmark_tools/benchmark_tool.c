#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kryzhovnik_wrapper.h"
#include "params.h"

#ifndef BENCH_ITERS
#define BENCH_ITERS 200
#endif

#ifndef BENCH_MLEN
#define BENCH_MLEN 32
#endif

static int cmp_u64(const void *a, const void *b) {
    uint64_t av = *(const uint64_t *)a;
    uint64_t bv = *(const uint64_t *)b;
    if (av < bv) {
        return -1;
    }
    if (av > bv) {
        return 1;
    }
    return 0;
}

static uint64_t median_u64(uint64_t *vals, size_t n) {
    qsort(vals, n, sizeof(uint64_t), cmp_u64);
    if (n % 2U == 1U) {
        return vals[n / 2U];
    }
    return (vals[(n / 2U) - 1U] + vals[n / 2U]) / 2U;
}

static uint64_t average_u64(const uint64_t *vals, size_t n) {
    uint64_t acc = 0;
    size_t i;
    for (i = 0; i < n; ++i) {
        acc += vals[i];
    }
    return acc / n;
}

static uint64_t elapsed_us(const struct timespec *start, const struct timespec *end) {
    uint64_t start_us = (uint64_t)start->tv_sec * 1000000ULL + (uint64_t)start->tv_nsec / 1000ULL;
    uint64_t end_us = (uint64_t)end->tv_sec * 1000000ULL + (uint64_t)end->tv_nsec / 1000ULL;
    return end_us - start_us;
}

int main(void) {
    uint64_t tkeygen[BENCH_ITERS];
    uint64_t tsign[BENCH_ITERS];
    uint64_t tverify[BENCH_ITERS];
    uint8_t msg[BENCH_MLEN];
    uint8_t sk[KRYZHOVNIK_SECRET_KEY_BYTES];
    uint8_t pk[KRYZHOVNIK_PUBLIC_KEY_BYTES];
    uint8_t sig[KRYZHOVNIK_SIGNATURE_BYTES];
    size_t sig_len = 0;
    size_t i;

    for (i = 0; i < BENCH_MLEN; ++i) {
        msg[i] = (uint8_t)(i + 17U);
    }

    for (i = 0; i < BENCH_ITERS; ++i) {
        struct timespec t0;
        struct timespec t1;

        memset(sk, 0, sizeof(sk));
        memset(pk, 0, sizeof(pk));
        memset(sig, 0, sizeof(sig));

        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (kryzhovnik_keygen(sk, sizeof(sk), pk, sizeof(pk)) != KRYZHOVNIK_OK) {
            fprintf(stderr, "benchmark_error=keygen_failed\n");
            return 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        tkeygen[i] = elapsed_us(&t0, &t1);

        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (kryzhovnik_sign(sk, sizeof(sk), pk, sizeof(pk),
                           msg, sizeof(msg),
                           sig, sizeof(sig), &sig_len) != KRYZHOVNIK_OK) {
            fprintf(stderr, "benchmark_error=sign_failed\n");
            return 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        tsign[i] = elapsed_us(&t0, &t1);

        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (kryzhovnik_verify(pk, sizeof(pk), sig, sig_len, msg, sizeof(msg)) != KRYZHOVNIK_OK) {
            fprintf(stderr, "benchmark_error=verify_failed\n");
            return 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        tverify[i] = elapsed_us(&t0, &t1);
    }

    {
        uint64_t keygen_med = median_u64(tkeygen, BENCH_ITERS);
        uint64_t keygen_avg = average_u64(tkeygen, BENCH_ITERS);
        uint64_t sign_med = median_u64(tsign, BENCH_ITERS);
        uint64_t sign_avg = average_u64(tsign, BENCH_ITERS);
        uint64_t verify_med = median_u64(tverify, BENCH_ITERS);
        uint64_t verify_avg = average_u64(tverify, BENCH_ITERS);
        double sign_ops_s = sign_avg > 0 ? (1000000.0 / (double)sign_avg) : 0.0;

        printf(
            "RESULT benchmark=wrapper paramset=%s iters=%d msg_len=%d "
            "keygen_med_us=%llu keygen_avg_us=%llu "
            "sign_med_us=%llu sign_avg_us=%llu "
            "verify_med_us=%llu verify_avg_us=%llu "
            "sign_ops_s=%.6f\n",
            KRYZHOVNIK_PARAMSET_NAME,
            BENCH_ITERS,
            BENCH_MLEN,
            (unsigned long long)keygen_med,
            (unsigned long long)keygen_avg,
            (unsigned long long)sign_med,
            (unsigned long long)sign_avg,
            (unsigned long long)verify_med,
            (unsigned long long)verify_avg,
            sign_ops_s
        );
    }

    return 0;
}
