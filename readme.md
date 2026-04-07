# kryzhovnik-wrapper-tc26 — service/benchmark-tooling

This branch contains **benchmark automation tooling** for comparing performance
across implementation branches of the PQS signature scheme wrapper.

The underlying PQS signature algorithm is described in  
https://crypto-kantiana.com/main_papers/main_Signature.pdf  
Original reference implementation: https://github.com/ElenaKirshanova/pqc_LWR_signature  
(authors: ElenaKirshanova, summerschool-kld, kn02262, n7v)

---

## Branches compared

| Profile | Branch | Toolchain |
|---------|--------|-----------|
| `pure-c-local` / `pure-c-origin` | `pure-c` | gcc, C99 |
| `stdc++-local` / `stdc++-origin` | `stdc++` | g++, C++17, NTL |

---

## Requirements

```
cmake >= 3.10
make
gcc / g++
jq
python3
```

For `stdc++` profiles additionally:
```
g++
libntl-dev  (NTL library with <NTL/ZZ_pEX.h>)
```

---

## Quick start

Run all profiles defined in `benchmark_tools/config.json`:
```sh
./benchmark_tools/run_benchmark.sh
```

Run a single profile:
```sh
./benchmark_tools/run_benchmark.sh --branch pure-c-origin
./benchmark_tools/run_benchmark.sh --branch stdc++-origin
```

Strict mode — abort instead of skipping if a required dependency (e.g. NTL) is missing:
```sh
./benchmark_tools/run_benchmark.sh --strict --branch stdc++-origin
```

Compare two commits within a profile:
```sh
./benchmark_tools/run_benchmark.sh \
  --compare-commits <commit1> <commit2> \
  --profile pure-c-local
```

---

## Profiles

Profiles are defined in `benchmark_tools/config.json`.

- **`pure-c-local`** — local branch `pure-c`, cmake medium paramset  
- **`stdc++-local`** — local branch `stdc++`, cmake C++17, requires NTL  
- **`pure-c-origin`** — `origin/pure-c`, same as pure-c-local but resolved from remote  
- **`stdc++-origin`** — `origin/stdc++`, same as stdc++-local but resolved from remote  

Each run uses `git worktree` to check out the target ref in isolation without
touching the working tree.

---

## Artifacts

| File / Path | Description |
|---|---|
| `benchmark_history.csv` | Append-only timing history across all runs |
| `benchmark_report.md` | Generated Markdown report from CSV |
| `benchmark_tools/raw_logs/` | Raw binary output per individual run |

---

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Build, run, or parse error |
| `3` | Compare aborted: at least one run skipped due to missing dependency |

---

## CI

GitHub Actions workflow: `.github/workflows/benchmark-tooling.yml`

Jobs:
- **`pure-c-origin-smoke`** — runs `pure-c-origin` profile, uploads artifacts  
- **`stdcpp-origin-strict`** — runs `stdc++-origin` in strict mode  

Both jobs fetch `pure-c` and `stdc++` branches explicitly before running,
since `actions/checkout` does not fetch non-default branches by default.
