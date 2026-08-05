#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C

readonly expected_hash='d7407bc99a90236df51892aee25428c5066c529681f3e0c3e43458ba85df535e'

usage() {
    printf 'Usage: %s [--frames] [--recording-mode]\n' "$(basename -- "$0")"
}

show_frames=false
recording_mode=false
for argument in "$@"; do
    case "${argument}" in
        --frames) show_frames=true ;;
        --recording-mode) recording_mode=true ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

for command_name in awk grep sha256sum; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

results_dir="${RELEASE_RESULTS_DIR:-/tmp/cpp-embedded-telemetry-release-results}"
report_path="${RELEASE_VALIDATION_REPORT:-${results_dir}/release-validation.txt}"
report_dir="$(cd -- "$(dirname -- "${report_path}")" 2>/dev/null && pwd || exit 1)"
controlled_output_path="${report_dir}/controlled-output.txt"

if [[ ! -f "${report_path}" ]]; then
    printf 'Approved release validation report not found.\n' >&2
    exit 1
fi

required_results=(
    'Quality checks:                 PASS'
    'Host CTest:                     97/97 PASS'
    'Firmware Twister/Ztest:         43/43 PASS'
    'native_sim execution:           PASS'
    'NUCLEO-F401RE build:            PASS'
    'Renode ELF load:                PASS'
    'Zephyr boot:                    PASS'
    'USART2 capture:                 PASS'
    'TLFRAME validation:             8/8 PASS'
    'CRC-32 validation:              8/8 PASS'
    'Robot Framework:                7/7 PASS'
    'Deterministic executions:       3/3 PASS'
    'Overall validation:             PASS'
)
for required_result in "${required_results[@]}"; do
    if ! grep -Fqx "${required_result}" "${report_path}"; then
        printf 'Validation report is incomplete or not approved.\n' >&2
        exit 1
    fi
done
if ! grep -Fqx "${expected_hash}" "${report_path}"; then
    printf 'Validation report contains an unexpected controlled-output hash.\n' >&2
    exit 1
fi

if [[ "${show_frames}" == true ]]; then
    if [[ ! -f "${controlled_output_path}" ]]; then
        printf 'Controlled output required by --frames was not found.\n' >&2
        exit 1
    fi
    if [[ "$(sha256sum "${controlled_output_path}" | awk '{ print $1 }')" != "${expected_hash}" ]]; then
        printf 'Controlled output checksum does not match the approved report.\n' >&2
        exit 1
    fi
    if [[ "$(grep -c '^TLFRAME ' "${controlled_output_path}")" != 8 ]]; then
        printf 'Controlled output does not contain exactly eight frames.\n' >&2
        exit 1
    fi
fi

pause_between_blocks() {
    if [[ "${recording_mode}" == true ]]; then
        sleep 1
    fi
}

if [[ -t 1 ]] && command -v clear >/dev/null 2>&1; then
    clear
fi

printf '%s\n' \
    '============================================================' \
    'cpp-embedded-telemetry-lab — Release Validation' \
    '============================================================' \
    '' \
    'C++20 | Zephyr RTOS | STM32F4 | Renode | Robot Framework'

pause_between_blocks

printf '%s\n' \
    '' \
    'Validation pipeline' \
    'Protocol + Host -> CTest -> Zephyr -> Renode -> Robot'

pause_between_blocks

printf '\n'
awk '
    /^Stage durations:/ { exit }
    /^(Quality checks|Host CTest|Firmware Twister\/Ztest|native_sim execution|NUCLEO-F401RE build|Renode ELF load|Zephyr boot|USART2 capture|TLFRAME validation|CRC-32 validation|Robot Framework|Deterministic executions):/ { print }
' "${report_path}"

pause_between_blocks

printf '%s\n' \
    '' \
    'Controlled output SHA-256:' \
    "${expected_hash}"

if [[ "${show_frames}" == true ]]; then
    pause_between_blocks
    printf '%s\n' '' 'Deterministic firmware output'
    awk '{ print }' "${controlled_output_path}"
fi

pause_between_blocks

printf '%s\n' \
    '' \
    'Overall validation:             PASS' \
    '============================================================'
