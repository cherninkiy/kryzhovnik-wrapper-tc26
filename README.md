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

## 🧰 Prerequisites

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
	git jq cmake make python3 gcc-12 g++-12 libntl-dev
```

macOS (Homebrew baseline):

```bash
brew install git jq cmake python
```

For `stdc++-*` profiles you also need NTL headers/libraries available to the toolchain.

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
./benchmark_tools/run_benchmark.sh --keep-going
./benchmark_tools/run_benchmark.sh --strict --branch stdc++-origin
./benchmark_tools/run_benchmark.sh --compare-commits <commit1> <commit2> --profile pure-c-local
python3 ./benchmark_tools/compare_commits.py <commit1> <commit2> --profile pure-c-origin
```

Flags:
- `--strict`: fail fast when a profile has missing dependencies or is otherwise non-runnable.
- `--keep-going`: continue running remaining profiles even if one profile fails; script exits non-zero at the end if any profile failed.
- `--compare-commits`: benchmark two commits with selected profile and generate comparison markdown.

`large` profile notes:
- Standard benchmark flow intentionally skips `paramset=large` in automation.
- To run it manually, add/edit a profile in [benchmark_tools/config.json](benchmark_tools/config.json) with `"paramset": "large"` and invoke `--branch <that-profile>` in a dedicated environment.

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

CSV schema (`benchmark_history.csv`):

| Column | Meaning | Unit |
|---|---|---|
| `timestamp` | benchmark timestamp (UTC) | ISO-8601 |
| `profile` | profile name from config | text |
| `branch` | branch/ref label used for run | text |
| `commit` | short commit hash benchmarked | text |
| `paramset` | selected parameter set | text |
| `iters` | iteration count reported by benchmark binary | count |
| `msg_len` | message length used in benchmark | bytes |
| `keygen_med_us` | median key generation time | microseconds |
| `keygen_avg_us` | mean key generation time | microseconds |
| `sign_med_us` | median signing time | microseconds |
| `sign_avg_us` | mean signing time | microseconds |
| `verify_med_us` | median verification time | microseconds |
| `verify_avg_us` | mean verification time | microseconds |
| `sign_ops_s` | signing throughput derived from average | ops/sec |
| `raw_log` | path to raw benchmark log | relative path |

Interpretation notes:
- `*_med_us` is robust to outliers and preferred for commit-to-commit comparisons.
- `*_avg_us` and `sign_ops_s` are useful for throughput trends and CI drift monitoring.
- Benchmark binary output must include a structured `RESULT ...` line for ingestion.

CI policy summary:
- Trusted PRs (same repository): full matrix benchmark pipeline.
- Fork PRs: restricted smoke benchmark path.
- Workflow uses path filtering to avoid heavy runs for unrelated changes.

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
