#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
HISTORY_FILE="${ROOT_DIR}/benchmark_history.csv"

if [[ $# -ne 2 ]]; then
  echo "Usage: ./benchmark_tools/compare_commits.sh <commit1> <commit2>" >&2
  exit 1
fi

left="$1"
right="$2"

if [[ ! -f "${HISTORY_FILE}" ]]; then
  echo "History file not found: ${HISTORY_FILE}" >&2
  exit 1
fi

left_short="$(git -C "${ROOT_DIR}" rev-parse --short "${left}")"
right_short="$(git -C "${ROOT_DIR}" rev-parse --short "${right}")"

left_row="$(awk -F',' -v c="\"${left_short}\"" '$4==c {line=$0} END{print line}' "${HISTORY_FILE}")"
right_row="$(awk -F',' -v c="\"${right_short}\"" '$4==c {line=$0} END{print line}' "${HISTORY_FILE}")"

if [[ -z "${left_row}" || -z "${right_row}" ]]; then
  echo "Could not find both commit rows in ${HISTORY_FILE}" >&2
  exit 1
fi

stripq() {
  sed 's/^"//; s/"$//' <<<"$1"
}

get_col() {
  local row="$1"
  local idx="$2"
  awk -F',' -v n="${idx}" '{print $n}' <<<"${row}"
}

l_sign="$(stripq "$(get_col "${left_row}" 10)")"
r_sign="$(stripq "$(get_col "${right_row}" 10)")"
l_key="$(stripq "$(get_col "${left_row}" 8)")"
r_key="$(stripq "$(get_col "${right_row}" 8)")"
l_ver="$(stripq "$(get_col "${left_row}" 12)")"
r_ver="$(stripq "$(get_col "${right_row}" 12)")"

report="${ROOT_DIR}/compare_${left_short}_${right_short}.md"

ratio() {
  python3 - "$1" "$2" <<'PY'
import sys

a = float(sys.argv[1])
b = float(sys.argv[2])
if a == 0:
    print("inf")
else:
    print(f"{b / a:.6f}")
PY
}

key_ratio="$(ratio "${l_key}" "${r_key}")"
sign_ratio="$(ratio "${l_sign}" "${r_sign}")"
verify_ratio="$(ratio "${l_ver}" "${r_ver}")"

cat >"${report}" <<EOF
# Commit Comparison

| Metric | ${left_short} | ${right_short} | ${right_short}/${left_short} |
|---|---:|---:|---:|
| keygen_med_us | ${l_key} | ${r_key} | ${key_ratio} |
| sign_med_us | ${l_sign} | ${r_sign} | ${sign_ratio} |
| verify_med_us | ${l_ver} | ${r_ver} | ${verify_ratio} |
EOF

echo "Comparison report: ${report}"
