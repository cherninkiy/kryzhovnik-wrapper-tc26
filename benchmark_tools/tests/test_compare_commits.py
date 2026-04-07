import importlib.util
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "benchmark_tools" / "compare_commits.py"


spec = importlib.util.spec_from_file_location("compare_commits", MODULE_PATH)
compare_commits = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare_commits)


def test_load_rows_requires_columns(tmp_path):
    history = tmp_path / "history.csv"
    history.write_text("timestamp,commit\n2026-01-01T00:00:00Z,deadbee\n", encoding="utf-8")

    with pytest.raises(RuntimeError, match="missing required columns"):
        compare_commits.load_rows(history)


def test_pick_latest_row_with_profile_filter():
    rows = [
        {"timestamp": "2026-01-01T00:00:00Z", "commit": "abc123", "profile": "pure-c-origin"},
        {"timestamp": "2026-01-01T01:00:00Z", "commit": "abc123", "profile": "pure-c-origin"},
        {"timestamp": "2026-01-01T02:00:00Z", "commit": "abc123", "profile": "stdc++-origin"},
    ]

    row = compare_commits.pick_latest_row(rows, "abc123", "pure-c-origin")
    assert row is not None
    assert row["timestamp"] == "2026-01-01T01:00:00Z"


def test_build_report_contains_ratios():
    left = {
        "commit": "abc123",
        "keygen_med_us": "100",
        "sign_med_us": "200",
        "verify_med_us": "300",
    }
    right = {
        "commit": "def456",
        "keygen_med_us": "200",
        "sign_med_us": "100",
        "verify_med_us": "600",
    }

    report = compare_commits.build_report("abc123", "def456", left, right)
    assert "| keygen_med_us | 100.000000 | 200.000000 | 2.000000 |" in report
    assert "| sign_med_us | 200.000000 | 100.000000 | 0.500000 |" in report
    assert "| verify_med_us | 300.000000 | 600.000000 | 2.000000 |" in report
