#!/usr/bin/env python3
import argparse
import csv
import subprocess
import sys
from pathlib import Path


METRIC_COLUMNS = ("keygen_med_us", "sign_med_us", "verify_med_us")


def resolve_short_commit(root: Path, commit: str) -> str:
    try:
        out = subprocess.check_output(
            ["git", "-C", str(root), "rev-parse", "--short", commit],
            text=True,
            stderr=subprocess.STDOUT,
        )
        return out.strip()
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"Cannot resolve commit '{commit}': {exc.output.strip()}") from exc


def load_rows(history_file: Path) -> list[dict[str, str]]:
    if not history_file.exists():
        raise RuntimeError(f"History file not found: {history_file}")

    with history_file.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise RuntimeError("History CSV has no header")
        missing = [c for c in ("commit", "profile", "timestamp", *METRIC_COLUMNS) if c not in reader.fieldnames]
        if missing:
            raise RuntimeError(f"History CSV missing required columns: {', '.join(missing)}")
        return list(reader)


def pick_latest_row(rows: list[dict[str, str]], commit_short: str, profile: str | None) -> dict[str, str] | None:
    candidates = [r for r in rows if r.get("commit") == commit_short]
    if profile:
        candidates = [r for r in candidates if r.get("profile") == profile]
    if not candidates:
        return None
    candidates.sort(key=lambda r: r.get("timestamp", ""))
    return candidates[-1]


def parse_metric(row: dict[str, str], key: str) -> float:
    raw = row.get(key, "").strip()
    if raw == "":
        raise RuntimeError(f"Empty metric '{key}' in CSV row for commit {row.get('commit', '?')}")
    try:
        return float(raw)
    except ValueError as exc:
        raise RuntimeError(f"Invalid float for '{key}': {raw}") from exc


def ratio(left: float, right: float) -> str:
    if left == 0:
        return "inf"
    return f"{right / left:.6f}"


def build_report(left_short: str, right_short: str, left_row: dict[str, str], right_row: dict[str, str]) -> str:
    l_key = parse_metric(left_row, "keygen_med_us")
    r_key = parse_metric(right_row, "keygen_med_us")
    l_sign = parse_metric(left_row, "sign_med_us")
    r_sign = parse_metric(right_row, "sign_med_us")
    l_ver = parse_metric(left_row, "verify_med_us")
    r_ver = parse_metric(right_row, "verify_med_us")

    return "\n".join(
        [
            "# Commit Comparison",
            "",
            f"| Metric | {left_short} | {right_short} | {right_short}/{left_short} |",
            "|---|---:|---:|---:|",
            f"| keygen_med_us | {l_key:.6f} | {r_key:.6f} | {ratio(l_key, r_key)} |",
            f"| sign_med_us | {l_sign:.6f} | {r_sign:.6f} | {ratio(l_sign, r_sign)} |",
            f"| verify_med_us | {l_ver:.6f} | {r_ver:.6f} | {ratio(l_ver, r_ver)} |",
            "",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("commit1")
    parser.add_argument("commit2")
    parser.add_argument("--profile", default="")
    parser.add_argument("--history", default="")
    parser.add_argument("--output", default="")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    root_dir = script_dir.parent
    history_file = Path(args.history) if args.history else root_dir / "benchmark_history.csv"

    try:
        left_short = resolve_short_commit(root_dir, args.commit1)
        right_short = resolve_short_commit(root_dir, args.commit2)
        rows = load_rows(history_file)

        profile = args.profile.strip() or None
        left_row = pick_latest_row(rows, left_short, profile)
        right_row = pick_latest_row(rows, right_short, profile)

        if left_row is None or right_row is None:
            scoped = f" for profile '{profile}'" if profile else ""
            raise RuntimeError(
                f"Could not find both commit rows in {history_file}{scoped}: "
                f"left={left_short} right={right_short}"
            )

        output = Path(args.output) if args.output else root_dir / f"compare_{left_short}_{right_short}.md"
        output.write_text(
            build_report(left_short, right_short, left_row, right_row),
            encoding="utf-8",
        )
        print(f"Comparison report: {output}")
        return 0
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
