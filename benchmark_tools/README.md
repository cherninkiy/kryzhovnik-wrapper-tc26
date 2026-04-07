# Benchmark Tools

This directory contains benchmark orchestration, comparison, and report generation scripts.

## Components

- `run_benchmark.sh`: main runner for profile execution, history updates, and report refresh.
- `compare_commits.py`: compare two commits using CSV header-based parsing.
- `compare_commits.sh`: compatibility wrapper for `compare_commits.py`.
- `generate_report.py`: creates `benchmark_report.md` from `benchmark_history.csv`.
- `config.json`: profile definitions, command arrays, and comparison defaults.
- `raw_logs/`: benchmark stdout logs and build/runtime logs.

## Typical Workflows

Run all profiles:

```bash
./benchmark_tools/run_benchmark.sh
```

Run one profile:

```bash
./benchmark_tools/run_benchmark.sh --branch pure-c-origin
```

Continue despite profile failures:

```bash
./benchmark_tools/run_benchmark.sh --keep-going
```

Compare two commits for one profile:

```bash
./benchmark_tools/run_benchmark.sh --compare-commits <commit1> <commit2> --profile pure-c-origin
```

Direct compare utility usage:

```bash
python3 ./benchmark_tools/compare_commits.py <commit1> <commit2> --profile pure-c-origin
```

## Notes

- Profile commands in `config.json` are stored as argument arrays to avoid shell injection.
- `run_benchmark.sh` requires benchmark output to contain a `RESULT ...` line.
- `--strict` fails on missing dependencies; default mode can skip non-runnable profiles.
- In CI, `BENCH_BUILD_CACHE_DIR` can be set to reuse `build-bench` between runs.
