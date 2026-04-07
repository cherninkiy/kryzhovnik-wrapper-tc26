import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "benchmark_tools" / "generate_report.py"


spec = importlib.util.spec_from_file_location("generate_report", MODULE_PATH)
generate_report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate_report)


def test_build_comparisons_prefers_config_pairs():
    latest = {
        "pure-c-origin": {},
        "stdc++-origin": {},
        "opt-simd-origin": {},
    }
    config = {
        "comparison_pairs": [
            {
                "base": "pure-c-origin",
                "contender": "opt-simd-origin",
                "label": "origin optimized",
            }
        ]
    }

    pairs = generate_report.build_comparisons(latest, config)
    assert pairs == [("pure-c-origin", "opt-simd-origin", "origin optimized")]


def test_infer_dynamic_pairs_from_suffix():
    pairs = generate_report.infer_dynamic_pairs(
        ["pure-c-origin", "stdc++-origin", "opt-simd-origin", "pure-c-local", "stdc++-local"]
    )

    assert ("pure-c-origin", "opt-simd-origin", "origin: pure-c-origin vs opt-simd-origin") in pairs
    assert ("pure-c-origin", "stdc++-origin", "origin: pure-c-origin vs stdc++-origin") in pairs
    assert ("pure-c-local", "stdc++-local", "local: pure-c-local vs stdc++-local") in pairs


def test_build_report_uses_dynamic_comparison_section():
    rows = [
        {
            "timestamp": "2026-01-01T00:00:00Z",
            "profile": "pure-c-origin",
            "branch": "pure-c-origin",
            "commit": "abc123",
            "paramset": "medium",
            "iters": "10",
            "msg_len": "20",
            "keygen_avg_us": "100",
            "sign_avg_us": "120",
            "verify_avg_us": "80",
            "keygen_med_us": "100",
            "sign_med_us": "120",
            "verify_med_us": "80",
            "sign_ops_s": "8333",
            "raw_log": "benchmark_tools/raw_logs/a.log",
        },
        {
            "timestamp": "2026-01-01T00:00:00Z",
            "profile": "opt-simd-origin",
            "branch": "opt-simd-origin",
            "commit": "def456",
            "paramset": "medium",
            "iters": "10",
            "msg_len": "20",
            "keygen_avg_us": "90",
            "sign_avg_us": "100",
            "verify_avg_us": "70",
            "keygen_med_us": "90",
            "sign_med_us": "100",
            "verify_med_us": "70",
            "sign_ops_s": "10000",
            "raw_log": "benchmark_tools/raw_logs/b.log",
        },
    ]

    report = generate_report.build_report(rows, {})
    assert "## Profile Comparison (origin: pure-c-origin vs opt-simd-origin)" in report
    assert "- Base profile: pure-c-origin" in report
    assert "- Contender profile: opt-simd-origin" in report
