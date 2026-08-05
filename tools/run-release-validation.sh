#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C
export PYTHONDONTWRITEBYTECODE=1

readonly expected_host_tests=97
readonly expected_hash='d7407bc99a90236df51892aee25428c5066c529681f3e0c3e43458ba85df535e'

usage() {
    printf 'Usage: %s [--rebuild] [--keep-results] [--verbose]\n' "$(basename -- "$0")"
}

rebuild=false
keep_results=false
verbose=false
for argument in "$@"; do
    case "${argument}" in
        --rebuild) rebuild=true ;;
        --keep-results) keep_results=true ;;
        --verbose) verbose=true ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

for command_name in \
    awk bash cmake cmp ctest file find git grep install mktemp python3 \
    sha256sum shellcheck west; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

if [[ -z "${ZEPHYR_BASE:-}" || ! -d "${ZEPHYR_BASE}" ]]; then
    printf 'ZEPHYR_BASE must name an accessible Zephyr directory.\n' >&2
    exit 1
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}" rev-parse --show-toplevel)"
results_dir="${RELEASE_RESULTS_DIR:-/tmp/cpp-embedded-telemetry-release-results}"
host_build_dir="${HOST_BUILD_DIR:-${repo_root}/build-v030-host-regression}"
renode_build_dir="${RENODE_BUILD_DIR:-/tmp/cpp-embedded-telemetry-renode-build}"
renode_results_dir="${results_dir}/renode-runs"
twister_outdir="${results_dir}/twister"
report_path="${results_dir}/release-validation.txt"
controlled_output_path="${results_dir}/controlled-output.txt"
markdown_summary_path="${results_dir}/validation-summary.md"
native_output_path="${results_dir}/native-sim-controlled-output.txt"

if [[ "${results_dir}" != /tmp/* || "${results_dir}" == /tmp/ || -L "${results_dir}" ]]; then
    printf 'RELEASE_RESULTS_DIR must be a non-symlinked directory below /tmp.\n' >&2
    exit 1
fi
if [[ "${renode_build_dir}" != /tmp/* || "${renode_build_dir}" == /tmp/ || -L "${renode_build_dir}" ]]; then
    printf 'RENODE_BUILD_DIR must be a non-symlinked directory below /tmp.\n' >&2
    exit 1
fi
if [[ ! -f "${host_build_dir}/CTestTestfile.cmake" ]]; then
    printf 'HOST_BUILD_DIR must name a configured build containing 97 CTest cases.\n' >&2
    exit 1
fi

mkdir -p -- "${results_dir}" "${renode_results_dir}"

declare -a stage_names=()
declare -a stage_durations=()

run_timed() {
    local stage_name="$1"
    local stage_function="$2"
    local started_at completed_at

    printf 'Running %s...\n' "${stage_name}"
    started_at="$(date +%s)"
    "${stage_function}"
    completed_at="$(date +%s)"
    stage_names+=("${stage_name}")
    stage_durations+=("$((completed_at - started_at))")
    printf '%s: PASS (%ss)\n' "${stage_name}" "$((completed_at - started_at))"
}

run_quality_checks() {
    local python_cache="${results_dir}/python-cache"
    local private_path_pattern attribution_pattern dangerous_pattern
    local -a scripts=(
        "${repo_root}/tools/build-renode-firmware.sh"
        "${repo_root}/tools/run-renode-validation.sh"
        "${repo_root}/tools/run-renode-robot.sh"
        "${repo_root}/tools/run-firmware-demo.sh"
        "${repo_root}/tools/run-portfolio-validation.sh"
        "${repo_root}/tools/present-firmware-demo.sh"
        "${repo_root}/tools/run-release-validation.sh"
        "${repo_root}/tools/present-release-validation.sh"
    )

    git -C "${repo_root}" diff --check
    bash -n "${scripts[@]}"
    shellcheck "${scripts[@]}"

    PYTHONPYCACHEPREFIX="${python_cache}" \
        python3 -m py_compile \
        "${repo_root}/renode/tests/resources/telemetry_validation.py"
    PYTHONPYCACHEPREFIX="${python_cache}" \
        python3 -m compileall -q \
        "${repo_root}/renode/tests/resources"
    rm -rf -- "${python_cache}"
    if find "${repo_root}/renode/tests/resources" \
        -type d -name __pycache__ -print -quit | grep -q .; then
        printf 'Python bytecode cache remained in the repository.\n' >&2
        return 1
    fi

    private_path_pattern='/ho''me/[^/]+/(Doc''umentos|Doc''uments|To''ols)|/Us''ers/[^/]+'
    if grep -RniE "${private_path_pattern}" \
        "${repo_root}/.github" \
        "${repo_root}/docs/CI.md" \
        "${repo_root}/tools/run-release-validation.sh" \
        "${repo_root}/tools/present-release-validation.sh" \
        "${repo_root}/README.md"; then
        printf 'Private developer path detected.\n' >&2
        return 1
    fi

    attribution_pattern='co-authored''-by|generated''-by|assisted''-by|cur''sor|co''dex|open''ai|ai-''generated|artificial'' intelligence|signed-off''-by'
    if grep -RniE "${attribution_pattern}" \
        "${repo_root}/.github" \
        "${repo_root}/docs/CI.md" \
        "${repo_root}/tools/run-release-validation.sh" \
        "${repo_root}/tools/present-release-validation.sh" \
        "${repo_root}/README.md"; then
        printf 'Prohibited attribution detected.\n' >&2
        return 1
    fi

    dangerous_pattern='su''do|west fla''sh|west deb''ug|west atta''ch|sh''ell=True|os.''system|subprocess.*sh''ell|ev''al '
    if grep -RniE "${dangerous_pattern}" \
        "${repo_root}/.github" \
        "${repo_root}/tools/run-release-validation.sh" \
        "${repo_root}/tools/present-release-validation.sh"; then
        printf 'Dangerous command detected.\n' >&2
        return 1
    fi
}

run_host_ctest() {
    local discovery_log="${results_dir}/host-ctest-discovery.log"
    local test_log="${results_dir}/host-ctest.log"
    local junit_path="${results_dir}/host-ctest.xml"
    local discovered

    if [[ "${rebuild}" == true ]]; then
        cmake --build "${host_build_dir}" --parallel \
            > "${results_dir}/host-build.log" 2>&1
    fi
    ctest --test-dir "${host_build_dir}" -N > "${discovery_log}" 2>&1
    discovered="$(awk '/Total Tests:/ { count = $3 } END { print count + 0 }' "${discovery_log}")"
    if [[ "${discovered}" != "${expected_host_tests}" ]]; then
        printf 'Expected 97 CTest cases, discovered %s.\n' "${discovered}" >&2
        return 1
    fi
    ctest --test-dir "${host_build_dir}" \
        --output-on-failure \
        --output-junit "${junit_path}" \
        > "${test_log}" 2>&1
    python3 - "${junit_path}" <<'PYTHON'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
observed = (
    int(root.attrib.get("tests", 0)),
    int(root.attrib.get("failures", 0)),
    int(root.attrib.get("errors", 0)),
)
if observed != (97, 0, 0):
    raise SystemExit(f"Unexpected CTest result: {observed}")
PYTHON
}

run_firmware_twister() {
    local temporary_conf="${results_dir}/firmware-test-prj.conf"

    install -m 0644 "${repo_root}/tests/firmware/prj.conf" "${temporary_conf}"
    west twister \
        -T "${repo_root}/tests/firmware" \
        -p native_sim \
        --outdir "${twister_outdir}" \
        --clobber-output \
        --short-build-path \
        --extra-args "CONF_FILE=${temporary_conf}" \
        > "${results_dir}/twister.log" 2>&1
    python3 - "${twister_outdir}/twister.json" <<'PYTHON'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as report:
    data = json.load(report)
cases = [case for suite in data["testsuites"] for case in suite["testcases"]]
passed = sum(case["status"] == "passed" for case in cases)
if (len(cases), passed) != (43, 43):
    raise SystemExit(f"Expected 43/43 firmware cases, observed {passed}/{len(cases)}")
PYTHON
}

run_native_sim() {
    local -a arguments=()
    local native_raw_path="${results_dir}/native-sim-run.log"

    if [[ "${rebuild}" == true ]]; then
        arguments+=(--rebuild)
    fi
    "${script_dir}/run-firmware-demo.sh" "${arguments[@]}" \
        > "${native_raw_path}"
    awk '/^(TLFRAME|TLFIRMWARE)/ { sub(/\r$/, ""); print }' \
        "${native_raw_path}" > "${native_output_path}"
    test "$(grep -c '^TLFRAME ' "${native_output_path}")" = 8
    grep -qx 'TLFIRMWARE SUMMARY produced=8 transmitted=8 queue_errors=0' \
        "${native_output_path}"
    grep -qx 'TLFIRMWARE DONE' "${native_output_path}"
    test "$(sha256sum "${native_output_path}" | awk '{ print $1 }')" = "${expected_hash}"
}

run_nucleo_build() {
    local -a arguments=()
    local elf_path memory_summary

    if [[ "${rebuild}" == true ]]; then
        arguments+=(--rebuild)
    fi
    RENODE_BUILD_DIR="${renode_build_dir}" \
        "${script_dir}/build-renode-firmware.sh" "${arguments[@]}" \
        > "${results_dir}/nucleo-elf-path.txt" \
        2> "${results_dir}/nucleo-build.log"
    elf_path="$(tail -n 1 "${results_dir}/nucleo-elf-path.txt")"
    test -f "${elf_path}"
    file "${elf_path}" > "${results_dir}/nucleo-elf-description.txt"
    grep -Eq 'ELF 32-bit.*ARM' "${results_dir}/nucleo-elf-description.txt"

    memory_summary="${renode_build_dir}/renode-memory-usage.txt"
    test -s "${memory_summary}"
    install -m 0644 "${memory_summary}" "${results_dir}/firmware-memory-usage.txt"
    grep -Eq 'FLASH:[[:space:]]+30624 B[[:space:]]+512 KB[[:space:]]+5.84%' \
        "${memory_summary}"
    grep -Eq 'RAM:[[:space:]]+12428 B[[:space:]]+96 KB[[:space:]]+12.64%' \
        "${memory_summary}"
}

first_validation_dir=''
run_renode_once() {
    local run_log="${results_dir}/renode-validation-1.log"

    RENODE_BUILD_DIR="${renode_build_dir}" \
    RENODE_RESULTS_DIR="${renode_results_dir}" \
        "${script_dir}/run-renode-validation.sh" --keep-results \
        > "${run_log}" 2>&1
    first_validation_dir="$(awk '/^Results: / { print substr($0, 10) }' "${run_log}")"
    test -n "${first_validation_dir}"
    test -s "${first_validation_dir}/controlled-output.txt"
    install -m 0644 "${first_validation_dir}/controlled-output.txt" \
        "${controlled_output_path}"
    install -m 0644 "${first_validation_dir}/usart2.log" \
        "${results_dir}/renode-usart2.log"
    install -m 0644 "${first_validation_dir}/renode-monitor.log" \
        "${results_dir}/renode-monitor.log"
    cmp "${native_output_path}" "${controlled_output_path}"
}

robot_reports_dir=''
run_robot_framework() {
    local robot_log="${results_dir}/robot.log"
    local output_xml

    RENODE_BUILD_DIR="${renode_build_dir}" \
    RENODE_RESULTS_DIR="${renode_results_dir}" \
        "${script_dir}/run-renode-robot.sh" --keep-results \
        > "${robot_log}" 2>&1
    output_xml="$(awk '/^output.xml: / { print substr($0, 13) }' "${robot_log}")"
    test -f "${output_xml}"
    robot_reports_dir="$(dirname -- "${output_xml}")"
    python3 - "${output_xml}" <<'PYTHON'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
tests = root.findall(".//test")
passed = sum(
    test.find("status") is not None
    and test.find("status").attrib.get("status") == "PASS"
    for test in tests
)
if (len(tests), passed) != (7, 7):
    raise SystemExit(f"Expected 7/7 Robot cases, observed {passed}/{len(tests)}")
PYTHON
    for report_name in robot_output.xml output.xml log.html report.html; do
        test -f "${robot_reports_dir}/${report_name}"
        install -m 0644 "${robot_reports_dir}/${report_name}" \
            "${results_dir}/${report_name}"
    done
}

run_determinism() {
    local run_number run_log run_dir
    local -a captures=("${controlled_output_path}")

    for run_number in 2 3; do
        run_log="${results_dir}/renode-validation-${run_number}.log"
        RENODE_BUILD_DIR="${renode_build_dir}" \
        RENODE_RESULTS_DIR="${renode_results_dir}" \
            "${script_dir}/run-renode-validation.sh" --keep-results \
            > "${run_log}" 2>&1
        run_dir="$(awk '/^Results: / { print substr($0, 10) }' "${run_log}")"
        test -n "${run_dir}"
        test -s "${run_dir}/controlled-output.txt"
        install -m 0644 "${run_dir}/controlled-output.txt" \
            "${results_dir}/controlled-output-${run_number}.txt"
        captures+=("${results_dir}/controlled-output-${run_number}.txt")
    done

    cmp "${captures[0]}" "${captures[1]}"
    cmp "${captures[0]}" "${captures[2]}"
    sha256sum "${captures[@]}" > "${results_dir}/determinism-sha256.txt"
    test "$(sha256sum "${captures[0]}" | awk '{ print $1 }')" = "${expected_hash}"
}

write_reports() {
    local index

    {
        printf '%s\n' \
            '============================================================' \
            'cpp-embedded-telemetry-lab — Release Validation' \
            '============================================================' \
            '' \
            'Quality checks:                 PASS' \
            'Host CTest:                     97/97 PASS' \
            'Firmware Twister/Ztest:         43/43 PASS' \
            'native_sim execution:           PASS' \
            'NUCLEO-F401RE build:            PASS' \
            'Renode ELF load:                PASS' \
            'Zephyr boot:                    PASS' \
            'USART2 capture:                 PASS' \
            'TLFRAME validation:             8/8 PASS' \
            'CRC-32 validation:              8/8 PASS' \
            'Robot Framework:                7/7 PASS' \
            'Deterministic executions:       3/3 PASS' \
            '' \
            'Controlled output SHA-256:' \
            "${expected_hash}" \
            '' \
            'Stage durations:'
        for index in "${!stage_names[@]}"; do
            printf '%-32s %ss\n' "${stage_names[${index}]}:" "${stage_durations[${index}]}"
        done
        printf '%s\n' \
            '' \
            'Overall validation:             PASS' \
            '============================================================'
    } > "${report_path}"

    {
        printf '# Embedded Telemetry Release Validation\n\n'
        printf '| Validation | Result |\n'
        printf '|---|---|\n'
        printf '| Quality checks | PASS |\n'
        printf '| Host CTest | 97/97 PASS |\n'
        printf '| Firmware Twister/Ztest | 43/43 PASS |\n'
        printf '| native_sim execution | PASS |\n'
        printf '| NUCLEO-F401RE build | PASS |\n'
        printf '| Renode ELF load | PASS |\n'
        printf '| Zephyr boot | PASS |\n'
        printf '| USART2 capture | PASS |\n'
        printf '| TLFRAME validation | 8/8 PASS |\n'
        printf '| CRC-32 validation | 8/8 PASS |\n'
        printf '| Robot Framework | 7/7 PASS |\n'
        printf '| Deterministic executions | 3/3 identical |\n\n'
        printf "Controlled output SHA-256: \`%s\`\n\n" "${expected_hash}"
        printf '## Stage durations\n\n'
        for index in "${!stage_names[@]}"; do
            printf -- '- %s: %s seconds\n' \
                "${stage_names[${index}]}" "${stage_durations[${index}]}"
        done
    } > "${markdown_summary_path}"
}

run_timed 'Quality checks' run_quality_checks
run_timed 'Host CTest' run_host_ctest
run_timed 'Firmware Twister/Ztest' run_firmware_twister
run_timed 'native_sim execution' run_native_sim
run_timed 'NUCLEO-F401RE build' run_nucleo_build
run_timed 'Renode validation' run_renode_once
run_timed 'Robot Framework' run_robot_framework
run_timed 'Deterministic executions' run_determinism

write_reports

if [[ "${keep_results}" != true ]]; then
    if [[ -d "${renode_results_dir}" && ! -L "${renode_results_dir}" ]]; then
        rm -rf -- "${renode_results_dir}"
    fi
fi

if [[ "${verbose}" == true ]]; then
    printf 'Detailed validation logs were retained in the configured results directory.\n\n'
fi
awk '{ print }' "${report_path}"
