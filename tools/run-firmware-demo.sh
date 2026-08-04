#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C

usage() {
    printf 'Usage: %s [--rebuild]\n' "$(basename -- "$0")"
}

rebuild=false
case "${1:-}" in
    "")
        ;;
    --rebuild)
        rebuild=true
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

for command_name in git west awk mktemp install; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

if [[ -z "${ZEPHYR_BASE:-}" ]]; then
    printf 'ZEPHYR_BASE must be defined.\n' >&2
    exit 1
fi

if [[ ! -d "${ZEPHYR_BASE}" ]]; then
    printf 'ZEPHYR_BASE does not name an accessible directory.\n' >&2
    exit 1
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}" rev-parse --show-toplevel)"
build_dir="/tmp/cpp-embedded-telemetry-v030-native"
firmware_executable="${build_dir}/zephyr/zephyr.exe"

raw_output="$(mktemp /tmp/telemetry-firmware-demo-raw.XXXXXX)"
clean_output="$(mktemp /tmp/telemetry-firmware-demo-clean.XXXXXX)"
build_log="$(mktemp /tmp/telemetry-firmware-demo-build.XXXXXX)"
temporary_conf="$(mktemp /tmp/telemetry-firmware-demo-prj.XXXXXX.conf)"

cleanup() {
    rm -f -- \
        "${raw_output}" \
        "${clean_output}" \
        "${build_log}" \
        "${temporary_conf}"
}
trap cleanup EXIT

if [[ "${rebuild}" == true || ! -x "${firmware_executable}" ]]; then
    install -m 0644 "${repo_root}/firmware/prj.conf" "${temporary_conf}"

    if [[ "${rebuild}" == true ]]; then
        rm -rf -- "${build_dir}"
    fi

    printf 'Building Zephyr native_sim firmware...\n'
    if ! west build \
        -b native_sim \
        "${repo_root}/firmware" \
        -d "${build_dir}" \
        --pristine \
        -- \
        -DCONF_FILE="${temporary_conf}" \
        >"${build_log}" 2>&1; then
        printf 'Firmware build failed.\n' >&2
        awk '{ print }' "${build_log}" >&2
        exit 1
    fi
fi

if ! "${firmware_executable}" -stop_at=1 -no-rt >"${raw_output}" 2>&1; then
    printf 'Firmware execution failed.\n' >&2
    exit 1
fi

awk '/^(TLFRAME|TLFIRMWARE)/ { print }' "${raw_output}" >"${clean_output}"

if ! awk '
    BEGIN {
        frame_count = 0
        summary_count = 0
        done_count = 0
        valid = 1
    }
    {
        last_line = $0
    }
    /^TLFRAME / {
        frame_count++
        if (NF != 3 ||
            $2 !~ /^[0-9][0-9][0-9][0-9]$/ ||
            $3 !~ /^[0-9A-F]+$/ ||
            length($3) != 68) {
            valid = 0
        }
        next
    }
    /^TLFIRMWARE SUMMARY / {
        summary_count++
        if ($0 != "TLFIRMWARE SUMMARY produced=8 transmitted=8 queue_errors=0") {
            valid = 0
        }
        next
    }
    /^TLFIRMWARE DONE$/ {
        done_count++
        next
    }
    {
        valid = 0
    }
    END {
        if (frame_count != 8 ||
            summary_count != 1 ||
            done_count != 1 ||
            last_line != "TLFIRMWARE DONE") {
            valid = 0
        }
        exit !valid
    }
' "${clean_output}"; then
    printf 'Firmware demo validation failed.\n' >&2
    exit 1
fi

awk '{ print }' "${clean_output}"
