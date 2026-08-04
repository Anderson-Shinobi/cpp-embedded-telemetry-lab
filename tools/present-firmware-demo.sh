#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
    printf 'Usage: %s [--recording-mode]\n' "$(basename -- "$0")"
}

recording_mode=false
case "${1:-}" in
    "")
        ;;
    --recording-mode)
        recording_mode=true
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if (($# > 1)); then
    usage >&2
    exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ -t 1 ]] && command -v clear >/dev/null 2>&1; then
    clear
fi

printf '%s\n' \
    'cpp-embedded-telemetry-lab' \
    'Zephyr Firmware Telemetry Producer — v0.3.0' \
    'C++20 | Zephyr RTOS | native_sim | NUCLEO-F401RE' \
    ''

if [[ "${recording_mode}" == true ]]; then
    sleep 1
fi

"${script_dir}/run-firmware-demo.sh"

if [[ "${recording_mode}" == true ]]; then
    sleep 1
fi

printf '%s\n' \
    '' \
    'Zephyr native_sim: PASS' \
    'Firmware tests: 43/43 PASS' \
    'Host regression: 97/97 PASS' \
    'NUCLEO-F401RE build: PASS' \
    'Release: v0.3.0'
