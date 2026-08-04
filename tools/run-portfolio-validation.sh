#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C

for command_name in git west ctest awk mktemp install python3; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

if [[ -z "${ZEPHYR_BASE:-}" ]]; then
    printf 'ZEPHYR_BASE must be defined.\n' >&2
    exit 1
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}" rev-parse --show-toplevel)"
host_build_dir="${HOST_BUILD_DIR:-/tmp/cpp-embedded-telemetry-v030-host}"
twister_outdir="/tmp/cpp-embedded-telemetry-v030-twister"

firmware_log="$(mktemp /tmp/telemetry-firmware-tests.XXXXXX)"
host_log="$(mktemp /tmp/telemetry-host-regression.XXXXXX)"
demo_log="$(mktemp /tmp/telemetry-firmware-validation-demo.XXXXXX)"
test_conf="$(mktemp /tmp/telemetry-firmware-test-prj.XXXXXX.conf)"

cleanup() {
    rm -f -- "${firmware_log}" "${host_log}" "${demo_log}" "${test_conf}"
}
trap cleanup EXIT

fail_stage() {
    local label="$1"
    local log_file="$2"

    printf '%s: FAIL\n' "${label}" >&2
    awk '{ print }' "${log_file}" >&2
    printf 'Overall validation: FAIL\n' >&2
    exit 1
}

install -m 0644 "${repo_root}/tests/firmware/prj.conf" "${test_conf}"
rm -rf -- "${twister_outdir}"

if ! west twister \
    -T "${repo_root}/tests/firmware" \
    -p native_sim \
    --outdir "${twister_outdir}" \
    --short-build-path \
    --extra-args "CONF_FILE=${test_conf}" \
    >"${firmware_log}" 2>&1; then
    fail_stage "Firmware tests" "${firmware_log}"
fi

firmware_counts="$(
    python3 - "${twister_outdir}/twister.json" <<'PYTHON'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as report:
    data = json.load(report)

cases = [
    case
    for suite in data["testsuites"]
    for case in suite["testcases"]
]
passed = sum(case["status"] == "passed" for case in cases)
print(f"{len(cases)} {passed}")
PYTHON
)"
if [[ "${firmware_counts}" != "43 43" ]]; then
    printf 'Expected 43 passing firmware tests; observed %s.\n' \
        "${firmware_counts}" >"${firmware_log}"
    fail_stage "Firmware tests" "${firmware_log}"
fi
printf 'Firmware tests: PASS\n'

if [[ ! -f "${host_build_dir}/CTestTestfile.cmake" ]]; then
    printf 'Host test build not found. Set HOST_BUILD_DIR to a configured build.\n' \
        >"${host_log}"
    fail_stage "Host regression" "${host_log}"
fi

if ! ctest --test-dir "${host_build_dir}" -N >"${host_log}" 2>&1; then
    fail_stage "Host regression" "${host_log}"
fi

host_test_count="$(
    awk '/Total Tests:/ { count = $3 } END { print count + 0 }' "${host_log}"
)"
if [[ "${host_test_count}" != "97" ]]; then
    printf 'Expected 97 host tests, discovered %s.\n' "${host_test_count}" \
        >"${host_log}"
    fail_stage "Host regression" "${host_log}"
fi

if ! ctest \
    --test-dir "${host_build_dir}" \
    --output-on-failure \
    >"${host_log}" 2>&1; then
    fail_stage "Host regression" "${host_log}"
fi
printf 'Host regression: PASS\n'

if ! "${script_dir}/run-firmware-demo.sh" >"${demo_log}" 2>&1; then
    fail_stage "Firmware demo" "${demo_log}"
fi
printf 'Firmware demo: PASS\n'
printf 'Overall validation: PASS\n'
