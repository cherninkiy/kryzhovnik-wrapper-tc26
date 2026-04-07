#!/usr/bin/env python3
import argparse
import csv
import json
import re
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


def read_rows(path: Path):
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def to_float(v: str) -> float:
    try:
        return float(v)
    except Exception:
        return 0.0


def pct_delta(old: float, new: float) -> str:
    if old == 0:
        return "-"
    return f"{((new - old) / old) * 100.0:+.2f}%"


def fmt(v: float) -> str:
    return f"{v:.3f}"


def latest_by_profile(rows):
    groups = defaultdict(list)
    for r in rows:
        groups[r["profile"]].append(r)
    out = {}
    for profile, items in groups.items():
        items.sort(key=lambda x: x["timestamp"])
        out[profile] = items[-1]
    return out


def load_config(path: Path) -> dict:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def infer_dynamic_pairs(profile_names: list[str]) -> list[tuple[str, str, str]]:
    grouped: dict[str, list[str]] = defaultdict(list)
    for name in profile_names:
        m = re.match(r"^(.*)-(local|origin)$", name)
        if not m:
            continue
        grouped[m.group(2)].append(name)

    pairs: list[tuple[str, str, str]] = []
    for suffix, names in grouped.items():
        names.sort()
        baseline = f"pure-c-{suffix}" if f"pure-c-{suffix}" in names else names[0]
        for candidate in names:
            if candidate == baseline:
                continue
            label = f"{suffix}: {baseline} vs {candidate}"
            pairs.append((baseline, candidate, label))
    return pairs


def build_comparisons(latest: dict[str, dict[str, str]], config: dict) -> list[tuple[str, str, str]]:
    pairs: list[tuple[str, str, str]] = []
    explicit = config.get("comparison_pairs", []) if isinstance(config, dict) else []

    for item in explicit:
        if not isinstance(item, dict):
            continue
        base = item.get("base", "").strip()
        contender = item.get("contender", "").strip()
        label = item.get("label", f"{base} vs {contender}").strip()
        if base and contender:
            pairs.append((base, contender, label))

    if pairs:
        return pairs

    return infer_dynamic_pairs(list(latest.keys()))


def last_two_by_profile(rows):
    groups = defaultdict(list)
    for r in rows:
        groups[r["profile"]].append(r)
    out = {}
    for profile, items in groups.items():
        items.sort(key=lambda x: x["timestamp"])
        out[profile] = items[-2:] if len(items) >= 2 else items
    return out


def build_report(rows, config):
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    lines = [
        "# Kryzhovnik Benchmark Report",
        "",
        f"Generated: {now}",
        "",
    ]

    if not rows:
        lines.append("No benchmark data yet.")
        return "\n".join(lines) + "\n"

    latest = latest_by_profile(rows)
    lines.extend([
        "## Latest Results",
        "",
        "| Profile | Branch | Commit | Paramset | KeyGen avg (us) | Sign avg (us) | Verify avg (us) | Sign ops/s |",
        "|---|---|---|---|---:|---:|---:|---:|",
    ])

    for profile in sorted(latest.keys()):
        r = latest[profile]
        lines.append(
            "| {profile} | {branch} | {commit} | {paramset} | {kg} | {sg} | {vf} | {ops} |".format(
                profile=profile,
                branch=r["branch"],
                commit=r["commit"],
                paramset=r["paramset"],
                kg=fmt(to_float(r["keygen_avg_us"])),
                sg=fmt(to_float(r["sign_avg_us"])),
                vf=fmt(to_float(r["verify_avg_us"])),
                ops=fmt(to_float(r["sign_ops_s"])),
            )
        )

    comparisons = build_comparisons(latest, config)
    for base_name, contender_name, label in comparisons:
        if base_name in latest and contender_name in latest:
            p = latest[base_name]
            s = latest[contender_name]
            lines.extend([
                "",
                f"## Profile Comparison ({label})",
                "",
            ])
            sign_speedup = to_float(s["sign_avg_us"]) / max(to_float(p["sign_avg_us"]), 1e-9)
            keygen_speedup = to_float(s["keygen_avg_us"]) / max(to_float(p["keygen_avg_us"]), 1e-9)
            verify_speedup = to_float(s["verify_avg_us"]) / max(to_float(p["verify_avg_us"]), 1e-9)
            lines.append(f"- Base profile: {base_name}")
            lines.append(f"- Contender profile: {contender_name}")
            lines.append(f"- Sign speedup (base faster if >1): {sign_speedup:.3f}x")
            lines.append(f"- KeyGen speedup (base faster if >1): {keygen_speedup:.3f}x")
            lines.append(f"- Verify speedup (base faster if >1): {verify_speedup:.3f}x")

    lines.extend(["", "## Recent History (last 5 per profile)", ""])
    history = last_two_by_profile(rows)
    all_by_profile = defaultdict(list)
    for r in rows:
        all_by_profile[r["profile"]].append(r)

    for profile in sorted(all_by_profile.keys()):
        items = sorted(all_by_profile[profile], key=lambda x: x["timestamp"], reverse=True)[:5]
        lines.append(f"### {profile}")
        lines.append("")
        lines.append("| Timestamp | Branch | Commit | Sign avg (us) | Delta |")
        lines.append("|---|---|---|---:|---:|")
        prev = None
        for r in items:
            cur = to_float(r["sign_avg_us"])
            delta = "-" if prev is None else pct_delta(prev, cur)
            lines.append(
                f"| {r['timestamp']} | {r['branch']} | {r['commit']} | {fmt(cur)} | {delta} |"
            )
            prev = cur
        lines.append("")

    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--history", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--config", required=False, default="")
    args = parser.parse_args()

    history = Path(args.history)
    output = Path(args.output)
    config = Path(args.config) if args.config else (Path(__file__).resolve().parent / "config.json")

    rows = read_rows(history)
    report = build_report(rows, load_config(config))
    output.write_text(report, encoding="utf-8")


if __name__ == "__main__":
    main()
