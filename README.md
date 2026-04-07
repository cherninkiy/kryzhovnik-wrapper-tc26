# 📈 Kryzhovnik Wrapper Benchmark Tooling

Original upstream repository: https://github.com/ElenaKirshanova/pqc_LWR_signature

This repository branch focuses on benchmark automation for comparing implementation performance.
It provides reproducible benchmarking workflows, result history, and comparison reporting.

## 💡 Why It Matters

Target PoC repository:
- [cherninkiy/batch-pqc](https://github.com/cherninkiy/batch-pqc)

`batch-pqc` is a PoC for Merkle-tree-based batch signing across Russian PQC algorithms
(Shipovnik, Hypericum, Kryzhovnik). The goal is to measure practical speedup of
batch signing versus sequential signing under realistic benchmark conditions.

This wrapper repository is used there as the Kryzhovnik integration layer, so
benchmark quality and branch-to-branch reproducibility here directly affect the
quality of results in `batch-pqc`.

## 🔎 Branches of Interest

- [service/benchmark-tooling](https://github.com/cherninkiy/kryzhovnik-wrapper-tc26/tree/service/benchmark-tooling): tooling and orchestration branch (benchmark scripts, CI wiring, reports).
- [pure-c](https://github.com/cherninkiy/kryzhovnik-wrapper-tc26/tree/pure-c): implementation branch used as benchmark target.
- [stdc++](https://github.com/cherninkiy/kryzhovnik-wrapper-tc26/tree/stdc%2B%2B): implementation branch used as benchmark target.

Branch roles:
- service/benchmark-tooling = control plane for benchmark infrastructure.
- pure-c and stdc++ = measured implementation branches.

## ⚙️ Benchmark Scope

Profiles are defined in [benchmark_tools/config.json](benchmark_tools/config.json).

| Profile | Branch/ref | Toolchain/notes |
|---|---|---|
| `pure-c-local` / `pure-c-origin` | `pure-c` / `origin/pure-c` | gcc, C99 |
| `stdc++-local` / `stdc++-origin` | `stdc++` / `origin/stdc++` | g++, C++17, NTL |
| `large` (manual only) | n/a | 💣 heavy |

Note: `large` is a very resource-intensive paramset and is excluded from standard benchmark scenarios.

## 🛣️ Optimization Roadmap (Future Branches)

The current focus is benchmarking and reproducibility. For performance-oriented
future branches, this is the proposed optimization roadmap:

1. Branch `opt/precompute-A`
- Precompute matrix `A` once during key generation and reuse it in sign/verify.
- Goal: reduce repeated setup cost and stabilize timing variance.

2. Branch `opt/poly-mul-karatsuba`
- Introduce faster polynomial multiplication (Karatsuba or hybrid strategy).
- Goal: reduce CPU cost in matrix-vector operations.

3. Branch `opt/simd-avx2`
- Add SIMD paths (AVX2 where available) with portable fallback.
- Goal: speed up coefficient-wise arithmetic on modern CPUs.

4. Branch `opt/sign-loop-rejection`
- Profile and optimize rejection/iteration behavior in signing loop.
- Goal: reduce average signing attempts and improve throughput consistency.

5. Branch `research/ntt-crt`
- Investigate NTT/CRT-compatible multiplication pipeline for this parameter space.
- Goal: evaluate long-term high-impact acceleration path.

Acceptance policy for each branch:
- keep API compatibility for wrapper consumers,
- add benchmark before/after snapshots in `benchmark_history.csv`,
- merge only with reproducible gains on `pure-c` and documented trade-offs.

## 🚀 Quick Start

```bash
./benchmark_tools/run_benchmark.sh
./benchmark_tools/run_benchmark.sh --branch pure-c-origin
./benchmark_tools/run_benchmark.sh --branch stdc++-origin
./benchmark_tools/run_benchmark.sh --compare-commits <commit1> <commit2> --profile pure-c-local
```

## 📊 Latest Snapshot

Source: [benchmark_history.csv](benchmark_history.csv)

| Profile | Timestamp (UTC) | Commit | Paramset | Sign med us | Sign avg us | Sign ops/s |
|---|---|---|---|---:|---:|---:|
| pure-c-origin | 2026-04-07T09:36:04Z | 9b900c0 | medium | 16934.500 | 24912.327 | 40.140770 |
| stdc++-origin | n/a | n/a | n/a | n/a | n/a | n/a |
| pure-c-local | 2026-04-07T08:56:22Z | 9b900c0 | medium | 16849.500 | 25258.568 | 39.590526 |
| stdc++-local | n/a | n/a | n/a | n/a | n/a | n/a |

Artifacts:
- [benchmark_history.csv](benchmark_history.csv)
- [benchmark_report.md](benchmark_report.md)
- [benchmark_tools/raw_logs](benchmark_tools/raw_logs)

## 🙏 Acknowledgments

This benchmark service branch builds on the original PQS implementation and research context from:
- ElenaKirshanova/pqc_LWR_signature: https://github.com/ElenaKirshanova/pqc_LWR_signature
- Paper: https://crypto-kantiana.com/main_papers/main_Signature.pdf

Contributors in upstream project (as listed on GitHub):
- ElenaKirshanova
- summerschool-kld
- kn02262 (Nikita Kolesnikov)
- n7v

The original upstream README is preserved in [README_pqc_LWR_signature.md](README_pqc_LWR_signature.md).
