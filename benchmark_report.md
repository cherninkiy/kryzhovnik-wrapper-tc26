# Kryzhovnik Benchmark Report

Generated: 2026-04-07 19:06 UTC

## Latest Results

| Profile | Branch | Commit | Paramset | KeyGen avg (us) | Sign avg (us) | Verify avg (us) | Sign ops/s |
|---|---|---|---|---:|---:|---:|---:|
| pure-c-local | pure-c-local | 9b900c0 | medium | 787.123 | 22006.661 | 994.245 | 45.441 |
| pure-c-origin | pure-c-origin | 9b900c0 | medium | 842.857 | 23421.898 | 1051.937 | 42.695 |
| stdc++-local | stdc++-local | 7ac29e4 | medium | 1021.200 | 59654.600 | 929.100 | 16.763 |
| stdc++-origin | stdc++-origin | 7ac29e4 | medium | 906.500 | 93750.400 | 1576.700 | 10.667 |

## Profile Comparison (local: pure-c-local vs stdc++-local)

- Base profile: pure-c-local
- Contender profile: stdc++-local
- Sign speedup (base faster if >1): 2.711x
- KeyGen speedup (base faster if >1): 1.297x
- Verify speedup (base faster if >1): 0.934x

## Profile Comparison (origin: pure-c-origin vs stdc++-origin)

- Base profile: pure-c-origin
- Contender profile: stdc++-origin
- Sign speedup (base faster if >1): 4.003x
- KeyGen speedup (base faster if >1): 1.076x
- Verify speedup (base faster if >1): 1.499x

## Recent History (last 5 per profile)

### pure-c-local

| Timestamp | Branch | Commit | Sign avg (us) | Delta |
|---|---|---|---:|---:|
| 2026-04-07T19:05:35Z | pure-c-local | 9b900c0 | 22006.661 | - |
| 2026-04-07T19:02:14Z | pure-c-local | 9b900c0 | 23909.148 | +8.65% |

### pure-c-origin

| Timestamp | Branch | Commit | Sign avg (us) | Delta |
|---|---|---|---:|---:|
| 2026-04-07T19:06:07Z | pure-c-origin | 9b900c0 | 23421.898 | - |
| 2026-04-07T19:02:40Z | pure-c-origin | 9b900c0 | 22327.803 | -4.67% |

### stdc++-local

| Timestamp | Branch | Commit | Sign avg (us) | Delta |
|---|---|---|---:|---:|
| 2026-04-07T19:05:40Z | stdc++-local | 7ac29e4 | 59654.600 | - |

### stdc++-origin

| Timestamp | Branch | Commit | Sign avg (us) | Delta |
|---|---|---|---:|---:|
| 2026-04-07T19:06:11Z | stdc++-origin | 7ac29e4 | 93750.400 | - |

