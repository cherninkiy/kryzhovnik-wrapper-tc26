#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CONFIG_FILE="${ROOT_DIR}/benchmark_tools/config.json"
HISTORY_FILE="${ROOT_DIR}/benchmark_history.csv"
REPORT_FILE="${ROOT_DIR}/benchmark_report.md"
RAW_DIR="${ROOT_DIR}/benchmark_tools/raw_logs"
STRICT_MODE=0
LAST_RUN_SKIPPED=0

mkdir -p "${RAW_DIR}"

usage() {
  cat <<'USAGE'
Usage:
  ./benchmark_tools/run_benchmark.sh
  ./benchmark_tools/run_benchmark.sh --branch <profile_name>
  ./benchmark_tools/run_benchmark.sh --compare-commits <commit1> <commit2> [--profile <profile_name>]
  ./benchmark_tools/run_benchmark.sh --strict [other options]
USAGE
}

require_deps() {
  local deps=(git jq cmake make python3 awk sed grep)
  local dep
  for dep in "${deps[@]}"; do
    if ! command -v "${dep}" >/dev/null 2>&1; then
      echo "Missing dependency: ${dep}" >&2
      exit 1
    fi
  done
}

init_history() {
  if [[ ! -f "${HISTORY_FILE}" ]]; then
    echo "timestamp,profile,branch,commit,paramset,iters,msg_len,keygen_med_us,keygen_avg_us,sign_med_us,sign_avg_us,verify_med_us,verify_avg_us,sign_ops_s,raw_log" >"${HISTORY_FILE}"
  fi
}

csv_escape() {
  local val="$1"
  val="${val//\"/\"\"}"
  printf '"%s"' "${val}"
}

extract_kv() {
  local key="$1"
  local line="$2"
  awk -v key="${key}" '{
    for (i = 1; i <= NF; ++i) {
      if ($i ~ ("^" key "=")) {
        sub("^" key "=", "", $i)
        print $i
        exit
      }
    }
  }' <<<"${line}"
}

safe_name() {
  sed 's/[^a-zA-Z0-9._-]/_/g' <<<"$1"
}

check_requirements() {
  local profile_name="$1"
  local cfg_json="$2"
  local req
  local missing=0

  while IFS= read -r req; do
    [[ -z "${req}" ]] && continue
    case "${req}" in
      ntl)
        if ! command -v c++ >/dev/null 2>&1; then
          echo "Profile ${profile_name}: missing dependency c++ compiler for NTL check" >&2
          missing=1
          continue
        fi
        if ! printf '#include <NTL/ZZ_pEX.h>\nint main(){return 0;}\n' | c++ -x c++ - -fsyntax-only >/dev/null 2>&1; then
          echo "Profile ${profile_name}: missing dependency NTL headers (expected <NTL/ZZ_pEX.h>)" >&2
          missing=1
        fi
        ;;
      *)
        echo "Profile ${profile_name}: unknown requirement '${req}', treating as missing" >&2
        missing=1
        ;;
    esac
  done < <(jq -r '.requires[]? // empty' <<<"${cfg_json}")

  if [[ ${missing} -ne 0 ]]; then
    return 1
  fi
  return 0
}

parse_result_line() {
  local raw_file="$1"
  local cfg_json="$2"

  local result_line
  result_line="$(grep '^RESULT ' "${raw_file}" | tail -n 1 || true)"
  if [[ -n "${result_line}" ]]; then
    echo "${result_line}"
    return 0
  fi

  python3 - "${raw_file}" "${cfg_json}" <<'PY'
import json
import re
import statistics
import sys
from pathlib import Path

raw_file = Path(sys.argv[1])
cfg = json.loads(sys.argv[2])
text = raw_file.read_text(encoding="utf-8", errors="replace")

kg = [float(v) for v in re.findall(r"Keypair is generated in\s+([0-9.]+)\s+sec", text)]
sg = [float(v) for v in re.findall(r"Signing is completed in\s+([0-9.]+)\s+sec", text)]
vf = [float(v) for v in re.findall(r"Verification is completed in\s+([0-9.]+)\s+sec", text)]

if not kg or not sg or not vf:
    sys.exit(2)

iters = min(len(kg), len(sg), len(vf))
kg = kg[:iters]
sg = sg[:iters]
vf = vf[:iters]

def avg(vals):
    return statistics.mean(vals)

def med(vals):
    return statistics.median(vals)

kg_med_us = med(kg) * 1_000_000.0
kg_avg_us = avg(kg) * 1_000_000.0
sg_med_us = med(sg) * 1_000_000.0
sg_avg_us = avg(sg) * 1_000_000.0
vf_med_us = med(vf) * 1_000_000.0
vf_avg_us = avg(vf) * 1_000_000.0
ops = 1_000_000.0 / sg_avg_us if sg_avg_us > 0 else 0.0

paramset = cfg.get("paramset", "unknown")
msg_len = cfg.get("msg_len", "unknown")

print(
    "RESULT "
    f"benchmark=legacy_test_pqs paramset={paramset} iters={iters} msg_len={msg_len} "
    f"keygen_med_us={kg_med_us:.3f} keygen_avg_us={kg_avg_us:.3f} "
    f"sign_med_us={sg_med_us:.3f} sign_avg_us={sg_avg_us:.3f} "
    f"verify_med_us={vf_med_us:.3f} verify_avg_us={vf_avg_us:.3f} "
    f"sign_ops_s={ops:.6f}"
)
PY
}

run_profile_ref() {
  local profile_name="$1"
  local ref="$2"
  local branch_label="$3"
  LAST_RUN_SKIPPED=0

  local cfg
  cfg="$(jq -r --arg name "${profile_name}" '.profiles[] | select(.name == $name)' "${CONFIG_FILE}")"
  if [[ -z "${cfg}" ]]; then
    echo "Unknown profile: ${profile_name}" >&2
    return 1
  fi

  local configure_cmd build_cmd bench_binary bench_args
  configure_cmd="$(jq -r '.configure' <<<"${cfg}")"
  build_cmd="$(jq -r '.build' <<<"${cfg}")"
  bench_binary="$(jq -r '.benchmark_binary' <<<"${cfg}")"
  bench_args="$(jq -r '.benchmark_args // ""' <<<"${cfg}")"

  if ! check_requirements "${profile_name}" "${cfg}"; then
    if [[ ${STRICT_MODE} -eq 1 ]]; then
      echo "Profile ${profile_name} (${ref}): strict mode enabled, stopping due to missing dependencies." >&2
      return 1
    fi
    LAST_RUN_SKIPPED=1
    echo "Skipping profile ${profile_name} (${ref}) due to missing dependencies."
    return 0
  fi

  local wt_suffix ts wt_dir raw_file commit_short
  ts="$(date -u +%Y%m%dT%H%M%SZ)"
  wt_suffix="$(safe_name "${profile_name}_${ref}_${ts}_$$")"
  wt_dir="${ROOT_DIR}/benchmark_tools/worktree_${wt_suffix}"

  git -C "${ROOT_DIR}" worktree add --detach "${wt_dir}" "${ref}" >/dev/null
  commit_short="$(git -C "${wt_dir}" rev-parse --short HEAD)"
  raw_file="${RAW_DIR}/${profile_name}_${commit_short}_${ts}.log"

  local status=0
  set +e
  (
    set -e
    cd "${wt_dir}"
    eval "${configure_cmd}" >/dev/null
    eval "${build_cmd}" >/dev/null
    if [[ -n "${bench_args}" ]]; then
      eval "./${bench_binary} ${bench_args}" >"${raw_file}"
    else
      "./${bench_binary}" >"${raw_file}"
    fi
  )
  status=$?
  set -e

  git -C "${ROOT_DIR}" worktree remove --force "${wt_dir}" >/dev/null || true

  if [[ ${status} -ne 0 ]]; then
    return ${status}
  fi

  local result_line
  result_line="$(parse_result_line "${raw_file}" "${cfg}" || true)"
  if [[ -z "${result_line}" ]]; then
    echo "Cannot parse benchmark output for profile ${profile_name}." >&2
    return 1
  fi

  local paramset iters msg_len keygen_med keygen_avg sign_med sign_avg verify_med verify_avg sign_ops
  paramset="$(extract_kv paramset "${result_line}")"
  iters="$(extract_kv iters "${result_line}")"
  msg_len="$(extract_kv msg_len "${result_line}")"
  keygen_med="$(extract_kv keygen_med_us "${result_line}")"
  keygen_avg="$(extract_kv keygen_avg_us "${result_line}")"
  sign_med="$(extract_kv sign_med_us "${result_line}")"
  sign_avg="$(extract_kv sign_avg_us "${result_line}")"
  verify_med="$(extract_kv verify_med_us "${result_line}")"
  verify_avg="$(extract_kv verify_avg_us "${result_line}")"
  sign_ops="$(extract_kv sign_ops_s "${result_line}")"

  local stamp
  stamp="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$(csv_escape "${stamp}")" \
    "$(csv_escape "${profile_name}")" \
    "$(csv_escape "${branch_label}")" \
    "$(csv_escape "${commit_short}")" \
    "$(csv_escape "${paramset}")" \
    "$(csv_escape "${iters}")" \
    "$(csv_escape "${msg_len}")" \
    "$(csv_escape "${keygen_med}")" \
    "$(csv_escape "${keygen_avg}")" \
    "$(csv_escape "${sign_med}")" \
    "$(csv_escape "${sign_avg}")" \
    "$(csv_escape "${verify_med}")" \
    "$(csv_escape "${verify_avg}")" \
    "$(csv_escape "${sign_ops}")" \
    "$(csv_escape "${raw_file#${ROOT_DIR}/}")" \
    >>"${HISTORY_FILE}"

  echo "${profile_name}: ${commit_short} sign_avg_us=${sign_avg} sign_ops_s=${sign_ops}"
}

run_all_profiles() {
  local names
  names="$(jq -r '.profiles[].name' "${CONFIG_FILE}")"
  local name ref
  while IFS= read -r name; do
    ref="$(jq -r --arg n "${name}" '.profiles[] | select(.name == $n) | .ref' "${CONFIG_FILE}")"
    run_profile_ref "${name}" "${ref}" "${name}"
  done <<<"${names}"
}

run_compare_commits() {
  local left="$1"
  local right="$2"
  local profile="$3"
  local left_skipped=0
  local right_skipped=0

  if ! run_profile_ref "${profile}" "${left}" "commit:${left}"; then
    echo "Compare failed: could not benchmark left commit ${left} with profile ${profile}." >&2
    return 1
  fi
  left_skipped=${LAST_RUN_SKIPPED}

  if ! run_profile_ref "${profile}" "${right}" "commit:${right}"; then
    echo "Compare failed: could not benchmark right commit ${right} with profile ${profile}." >&2
    return 1
  fi
  right_skipped=${LAST_RUN_SKIPPED}

  if [[ ${left_skipped} -eq 1 || ${right_skipped} -eq 1 ]]; then
    echo "Compare aborted: at least one commit run was skipped due to missing dependencies for profile ${profile}." >&2
    echo "Install missing dependencies or rerun with --strict to fail fast." >&2
    return 3
  fi

  "${ROOT_DIR}/benchmark_tools/compare_commits.sh" "${left}" "${right}"
}

main() {
  require_deps
  init_history

  local branch_arg=""
  local compare_left=""
  local compare_right=""
  local compare_profile=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --branch)
        branch_arg="${2:-}"
        shift 2
        ;;
      --compare-commits)
        compare_left="${2:-}"
        compare_right="${3:-}"
        shift 3
        ;;
      --profile)
        compare_profile="${2:-}"
        shift 2
        ;;
      --strict)
        STRICT_MODE=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        echo "Unknown argument: $1" >&2
        usage
        exit 1
        ;;
    esac
  done

  if [[ -n "${compare_left}" || -n "${compare_right}" ]]; then
    if [[ -z "${compare_left}" || -z "${compare_right}" ]]; then
      echo "--compare-commits requires two commit hashes" >&2
      exit 1
    fi
    if [[ -z "${compare_profile}" ]]; then
      compare_profile="$(jq -r '.compare_default_profile' "${CONFIG_FILE}")"
    fi
    run_compare_commits "${compare_left}" "${compare_right}" "${compare_profile}"
  elif [[ -n "${branch_arg}" ]]; then
    local ref
    ref="$(jq -r --arg n "${branch_arg}" '.profiles[] | select(.name == $n) | .ref' "${CONFIG_FILE}")"
    if [[ -z "${ref}" || "${ref}" == "null" ]]; then
      echo "Unknown profile: ${branch_arg}" >&2
      exit 1
    fi
    run_profile_ref "${branch_arg}" "${ref}" "${branch_arg}"
  else
    run_all_profiles
  fi

  python3 "${ROOT_DIR}/benchmark_tools/generate_report.py" \
    --history "${HISTORY_FILE}" \
    --output "${REPORT_FILE}"

  echo "History updated: ${HISTORY_FILE}"
  echo "Report updated:  ${REPORT_FILE}"
}

main "$@"
