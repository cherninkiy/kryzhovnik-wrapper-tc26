#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CONFIG_FILE="${ROOT_DIR}/benchmark_tools/config.json"
HISTORY_FILE="${ROOT_DIR}/benchmark_history.csv"
REPORT_FILE="${ROOT_DIR}/benchmark_report.md"
RAW_DIR="${ROOT_DIR}/benchmark_tools/raw_logs"
STRICT_MODE=0
KEEP_GOING=0
CURRENT_WT_DIR=""
BENCH_BUILD_CACHE_DIR="${BENCH_BUILD_CACHE_DIR:-}"

mkdir -p "${RAW_DIR}"

usage() {
  cat <<'USAGE'
Usage:
  ./benchmark_tools/run_benchmark.sh
  ./benchmark_tools/run_benchmark.sh --branch <profile_name>
  ./benchmark_tools/run_benchmark.sh --compare-commits <commit1> <commit2> [--profile <profile_name>]
  ./benchmark_tools/run_benchmark.sh --strict [other options]
  ./benchmark_tools/run_benchmark.sh --keep-going [other options]
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

cleanup_worktree() {
  if [[ -n "${CURRENT_WT_DIR}" && -d "${CURRENT_WT_DIR}" ]]; then
    git -C "${ROOT_DIR}" worktree remove --force "${CURRENT_WT_DIR}" >/dev/null 2>&1 || true
    CURRENT_WT_DIR=""
  fi
}

trap cleanup_worktree EXIT INT TERM

csv_escape() {
  local val="$1"
  val="${val//$'\r'/\\r}"
  val="${val//$'\n'/\\n}"
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

check_ref_exists() {
  local ref="$1"
  if ! git -C "${ROOT_DIR}" rev-parse --verify "${ref}^{commit}" >/dev/null 2>&1; then
    echo "Reference not found: ${ref}" >&2
    return 1
  fi
}

validate_profile_config() {
  local profile_name="$1"
  local cfg_json="$2"
  local errors=()

  if [[ "$(jq -r '.name // empty' <<<"${cfg_json}")" != "${profile_name}" ]]; then
    errors+=("name mismatch")
  fi
  if [[ -z "$(jq -r '.ref // empty' <<<"${cfg_json}")" ]]; then
    errors+=("missing ref")
  fi
  if ! jq -e '.configure | type == "array" and length > 0' <<<"${cfg_json}" >/dev/null; then
    errors+=("configure must be a non-empty array")
  fi
  if ! jq -e '.build | type == "array" and length > 0' <<<"${cfg_json}" >/dev/null; then
    errors+=("build must be a non-empty array")
  fi
  if ! jq -e '.benchmark_command | type == "array" and length > 0' <<<"${cfg_json}" >/dev/null; then
    errors+=("benchmark_command must be a non-empty array")
  fi
  if [[ -z "$(jq -r '.paramset // empty' <<<"${cfg_json}")" ]]; then
    errors+=("missing paramset")
  fi
  if [[ -z "$(jq -r '.msg_len // empty' <<<"${cfg_json}")" ]]; then
    errors+=("missing msg_len")
  fi

  if [[ ${#errors[@]} -ne 0 ]]; then
    echo "Invalid config for profile ${profile_name}: ${errors[*]}" >&2
    return 1
  fi
}

load_cmd_array() {
  local field="$1"
  local cfg_json="$2"
  local __resultvar="$3"
  local -n out_ref="${__resultvar}"
  local arr=()
  mapfile -t arr < <(jq -r --arg field "${field}" '.[$field][]' <<<"${cfg_json}")
  if [[ ${#arr[@]} -eq 0 ]]; then
    echo "Invalid profile config: ${field} command array is empty" >&2
    return 1
  fi
  out_ref=("${arr[@]}")
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

  local result_line
  result_line="$(grep '^RESULT ' "${raw_file}" | tail -n 1 || true)"
  if [[ -n "${result_line}" ]]; then
    echo "${result_line}"
    return 0
  fi

  echo "Missing structured RESULT line in benchmark output: ${raw_file}" >&2
  return 1
}

run_profile_ref() {
  local profile_name="$1"
  local ref="$2"
  local branch_label="$3"

  local cfg
  cfg="$(jq -r --arg name "${profile_name}" '.profiles[] | select(.name == $name)' "${CONFIG_FILE}")"
  if [[ -z "${cfg}" ]]; then
    echo "Unknown profile: ${profile_name}" >&2
    return 1
  fi

  validate_profile_config "${profile_name}" "${cfg}" || return 1
  check_ref_exists "${ref}" || return 1

  local -a configure_cmd=() build_cmd=() bench_cmd=()
  load_cmd_array configure "${cfg}" configure_cmd || return 1
  load_cmd_array build "${cfg}" build_cmd || return 1
  load_cmd_array benchmark_command "${cfg}" bench_cmd || return 1

  local declared_paramset
  declared_paramset="$(jq -r '.paramset // ""' <<<"${cfg}")"
  if [[ "${declared_paramset}" == "large" ]] || printf '%s\n' "${configure_cmd[@]}" | grep -Eq -- '-DKRYZHOVNIK_PARAMSET=large\b'; then
    echo "Profile ${profile_name}: paramset 'large' is intentionally excluded from standard benchmark scenarios because it is very resource-intensive on CI/dev VMs." >&2
    if [[ ${STRICT_MODE} -eq 1 ]]; then
      return 1
    fi
    return 2
  fi

  if ! check_requirements "${profile_name}" "${cfg}"; then
    if [[ ${STRICT_MODE} -eq 1 ]]; then
      echo "Profile ${profile_name} (${ref}): strict mode enabled, stopping due to missing dependencies." >&2
      return 1
    fi
    echo "Skipping profile ${profile_name} (${ref}) due to missing dependencies."
    return 2
  fi

  local wt_suffix ts wt_dir raw_file build_log commit_short profile_cache_dir
  ts="$(date -u +%Y%m%dT%H%M%SZ)"
  wt_suffix="$(safe_name "${profile_name}_${ref}_${ts}_$$")"
  wt_dir="${ROOT_DIR}/benchmark_tools/worktree_${wt_suffix}"

  check_ref_exists "${ref}" || return 1

  git -C "${ROOT_DIR}" worktree add --detach "${wt_dir}" "${ref}" >/dev/null
  CURRENT_WT_DIR="${wt_dir}"

  commit_short="$(git -C "${wt_dir}" rev-parse --short HEAD)"
  raw_file="${RAW_DIR}/${profile_name}_${commit_short}_${ts}.log"
  build_log="${RAW_DIR}/${profile_name}_${commit_short}_${ts}.build.log"

  profile_cache_dir=""
  if [[ -n "${BENCH_BUILD_CACHE_DIR}" ]]; then
    profile_cache_dir="${BENCH_BUILD_CACHE_DIR}/$(safe_name "${profile_name}")"
    mkdir -p "${profile_cache_dir}"
    if [[ -d "${profile_cache_dir}/build-bench" ]]; then
      rm -rf "${wt_dir}/build-bench"
      cp -a "${profile_cache_dir}/build-bench" "${wt_dir}/build-bench"
    fi
  fi

  local status=0
  set +e
  (
    set -e
    cd "${wt_dir}"
    "${configure_cmd[@]}" >"${build_log}" 2>&1
    "${build_cmd[@]}" >>"${build_log}" 2>&1
    "${bench_cmd[@]}" >"${raw_file}" 2>>"${build_log}"
  )
  status=$?
  set -e

  if [[ ${status} -ne 0 ]]; then
    cleanup_worktree
    echo "Profile ${profile_name} failed. Build/runtime log: ${build_log}" >&2
    if [[ -f "${build_log}" ]]; then
      tail -n 120 "${build_log}" >&2 || true
    fi
    return ${status}
  fi

  if [[ -n "${profile_cache_dir}" && -d "${wt_dir}/build-bench" ]]; then
    rm -rf "${profile_cache_dir}/build-bench"
    cp -a "${wt_dir}/build-bench" "${profile_cache_dir}/build-bench"
  fi

  cleanup_worktree

  local result_line
  result_line="$(parse_result_line "${raw_file}")"
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
  local name ref rc
  local had_errors=0
  while IFS= read -r name; do
    ref="$(jq -r --arg n "${name}" '.profiles[] | select(.name == $n) | .ref' "${CONFIG_FILE}")"
    set +e
    run_profile_ref "${name}" "${ref}" "${name}"
    rc=$?
    set -e

    if [[ ${rc} -eq 0 || ${rc} -eq 2 ]]; then
      continue
    fi

    had_errors=1
    if [[ ${KEEP_GOING} -eq 0 ]]; then
      return ${rc}
    fi
  done <<<"${names}"

  if [[ ${had_errors} -eq 1 ]]; then
    return 1
  fi
}

run_compare_commits() {
  local left="$1"
  local right="$2"
  local profile="$3"
  local left_rc right_rc

  set +e
  run_profile_ref "${profile}" "${left}" "commit:${left}"
  left_rc=$?
  set -e
  if [[ ${left_rc} -ne 0 && ${left_rc} -ne 2 ]]; then
    echo "Compare failed: could not benchmark left commit ${left} with profile ${profile}." >&2
    return 1
  fi

  set +e
  run_profile_ref "${profile}" "${right}" "commit:${right}"
  right_rc=$?
  set -e
  if [[ ${right_rc} -ne 0 && ${right_rc} -ne 2 ]]; then
    echo "Compare failed: could not benchmark right commit ${right} with profile ${profile}." >&2
    return 1
  fi

  if [[ ${left_rc} -eq 2 || ${right_rc} -eq 2 ]]; then
    echo "Compare aborted: at least one commit run was skipped due to missing dependencies for profile ${profile}." >&2
    echo "Install missing dependencies or rerun with --strict to fail fast." >&2
    return 2
  fi

  python3 "${ROOT_DIR}/benchmark_tools/compare_commits.py" "${left}" "${right}" --profile "${profile}"
}

main() {
  require_deps
  init_history

  local branch_arg=""
  local compare_left=""
  local compare_right=""
  local compare_profile=""
  local run_status=0

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
      --keep-going)
        KEEP_GOING=1
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
    set +e
    run_compare_commits "${compare_left}" "${compare_right}" "${compare_profile}"
    run_status=$?
    set -e
  elif [[ -n "${branch_arg}" ]]; then
    local ref
    ref="$(jq -r --arg n "${branch_arg}" '.profiles[] | select(.name == $n) | .ref' "${CONFIG_FILE}")"
    if [[ -z "${ref}" || "${ref}" == "null" ]]; then
      echo "Unknown profile: ${branch_arg}" >&2
      exit 1
    fi
    set +e
    run_profile_ref "${branch_arg}" "${ref}" "${branch_arg}"
    run_status=$?
    set -e
    if [[ ${run_status} -eq 2 ]]; then
      run_status=0
    fi
  else
    set +e
    run_all_profiles
    run_status=$?
    set -e
  fi

  python3 "${ROOT_DIR}/benchmark_tools/generate_report.py" \
    --history "${HISTORY_FILE}" \
    --output "${REPORT_FILE}" \
    --config "${CONFIG_FILE}"

  echo "History updated: ${HISTORY_FILE}"
  echo "Report updated:  ${REPORT_FILE}"

  exit ${run_status}
}

main "$@"
