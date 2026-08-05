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

for command_name in git west file mktemp install awk size; do
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
build_dir="${RENODE_BUILD_DIR:-/tmp/cpp-embedded-telemetry-renode-build}"
board="${ZEPHYR_BOARD:-nucleo_f401re}"

if [[ "${build_dir}" != /tmp/* || "${build_dir}" == /tmp/ || -L "${build_dir}" ]]; then
    printf 'RENODE_BUILD_DIR must be a non-symlinked directory below /tmp.\n' >&2
    exit 1
fi

elf_path="${build_dir}/zephyr/zephyr.elf"
memory_summary="${build_dir}/renode-memory-usage.txt"
build_log="$(mktemp /tmp/telemetry-renode-build-log.XXXXXX)"
temporary_conf="$(mktemp /tmp/telemetry-renode-prj.XXXXXX.conf)"

cleanup() {
    rm -f -- "${build_log}" "${temporary_conf}"
}
trap cleanup EXIT

if [[ "${rebuild}" == true || ! -f "${elf_path}" ]]; then
    install -m 0644 "${repo_root}/firmware/prj.conf" "${temporary_conf}"
    printf 'Building Zephyr firmware for %s...\n' "${board}" >&2
    if ! west build \
        -b "${board}" \
        "${repo_root}/firmware" \
        -d "${build_dir}" \
        --pristine \
        -- \
        -DCONF_FILE="${temporary_conf}" \
        >"${build_log}" 2>&1; then
        printf 'Zephyr firmware build failed.\n' >&2
        awk '{ print }' "${build_log}" >&2
        exit 1
    fi
    grep -E 'Memory region|^[[:space:]]*(FLASH|RAM|SRAM[0-9]*):' "${build_log}" \
        >"${memory_summary}" || true
fi

if [[ ! -f "${elf_path}" ]]; then
    printf 'Zephyr ELF was not generated: %s\n' "${elf_path}" >&2
    exit 1
fi

elf_description="$(file -- "${elf_path}")"
if [[ "${elf_description}" != *"ELF 32-bit"* || "${elf_description}" != *"ARM"* ]]; then
    printf 'Generated artifact is not the expected ARM ELF: %s\n' \
        "${elf_description}" >&2
    exit 1
fi

printf '%s\n' "${elf_description}" >&2
if [[ -s "${memory_summary}" ]]; then
    awk '{ print }' "${memory_summary}" >&2
else
    printf 'ELF section usage (text/data/bss):\n' >&2
    size "${elf_path}" >&2
fi

printf '%s\n' "${elf_path}"
