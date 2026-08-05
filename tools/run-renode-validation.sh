#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C

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

for command_name in git python3 sed awk grep mktemp install timeout; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}" rev-parse --show-toplevel)"
results_dir="${RENODE_RESULTS_DIR:-/tmp/cpp-embedded-telemetry-renode-results}"
timeout_seconds="${RENODE_TIMEOUT_SECONDS:-30}"

if [[ "${results_dir}" != /tmp/* || "${results_dir}" == /tmp/ || -L "${results_dir}" ]]; then
    printf 'RENODE_RESULTS_DIR must be a non-symlinked directory below /tmp.\n' >&2
    exit 1
fi

if [[ -n "${RENODE_HOME:-}" ]]; then
    renode_bin="${RENODE_HOME}/renode"
elif command -v renode >/dev/null 2>&1; then
    renode_bin="$(command -v renode)"
else
    printf 'Renode not found. Define RENODE_HOME or add renode to PATH.\n' >&2
    exit 1
fi

if [[ ! -x "${renode_bin}" ]]; then
    printf 'Renode executable is not usable: %s\n' "${renode_bin}" >&2
    exit 1
fi

build_arguments=()
if [[ "${rebuild}" == true ]]; then
    build_arguments+=(--rebuild)
fi
elf_path="$("${script_dir}/build-renode-firmware.sh" "${build_arguments[@]}")"

mkdir -p -- "${results_dir}"
run_dir="$(mktemp -d "${results_dir}/validation.XXXXXX")"
generated_resc="${run_dir}/telemetry_firmware.resc"
platform_copy="${run_dir}/nucleo_f401re.repl"
capture_path="${run_dir}/usart2.log"
controlled_output="${run_dir}/controlled-output.txt"
monitor_log="${run_dir}/renode-monitor.log"

cleanup() {
    if [[ "${keep_results}" != true ]]; then
        case "${run_dir}" in
            "${results_dir}"/validation.*) ;;
            *) printf 'Refusing to clean unexpected result path: %s\n' "${run_dir}" >&2; return ;;
        esac
        if [[ -d "${run_dir}" && ! -L "${run_dir}" ]]; then
            rm -rf -- "${run_dir}"
        fi
    fi
}
trap cleanup EXIT

install -m 0644 "${repo_root}/renode/platforms/nucleo_f401re.repl" "${platform_copy}"
sed \
    -e "s|@ELF_PATH@|@${elf_path}|g" \
    -e "s|@PLATFORM_PATH@|@${platform_copy}|g" \
    "${repo_root}/renode/scripts/telemetry_firmware.resc" \
    >"${generated_resc}"
{
    printf '%s\n' \
        "usart2 CreateFileBackend @${capture_path}" \
        'emulation RunFor "0.250"' \
        "usart2 CloseFileBackend @${capture_path}" \
        'quit'
} >>"${generated_resc}"

runtime_dir="${run_dir}/runtime"
mkdir -p -- "${runtime_dir}/tmp" "${runtime_dir}/dotnet" "${runtime_dir}/config"
export TMPDIR="${runtime_dir}/tmp"
export DOTNET_BUNDLE_EXTRACT_BASE_DIR="${runtime_dir}/dotnet"
export XDG_CONFIG_HOME="${runtime_dir}/config"

if [[ "${verbose}" == true ]]; then
    printf 'Renode command: %s --disable-gui --console %s\n' \
        "${renode_bin}" "${generated_resc}"
fi

if ! timeout --foreground "${timeout_seconds}" \
    "${renode_bin}" --disable-gui --console "${generated_resc}" \
    >"${monitor_log}" 2>&1; then
    printf 'Renode headless execution failed or timed out after %s seconds.\n' \
        "${timeout_seconds}" >&2
    awk '{ print }' "${monitor_log}" >&2
    exit 1
fi

if [[ ! -s "${capture_path}" ]]; then
    printf 'USART2 capture was not produced.\n' >&2
    exit 1
fi

awk '/^(TLFRAME|TLFIRMWARE)/ { sub(/\r$/, ""); print }' \
    "${capture_path}" >"${controlled_output}"

grep -q 'System bus created' "${monitor_log}"
grep -q 'Loading block of .* at 0x8000000' "${monitor_log}"
grep -q '^\*\*\* Booting Zephyr OS build ' "${capture_path}"

printf 'Renode platform: PASS\n'
printf 'ELF load: PASS\n'
printf 'Zephyr boot: PASS\n'
python3 "${repo_root}/renode/tests/resources/telemetry_validation.py" \
    "${capture_path}" --monitor-log "${monitor_log}"
printf 'Overall Renode validation: PASS\n'

if [[ "${keep_results}" == true ]]; then
    printf 'Results: %s\n' "${run_dir}"
fi
