#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C
export PYTHONDONTWRITEBYTECODE=1

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

for command_name in git python3 sed mktemp install; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "${command_name}" >&2
        exit 1
    fi
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "${script_dir}" rev-parse --show-toplevel)"
results_dir="${RENODE_RESULTS_DIR:-/tmp/cpp-embedded-telemetry-renode-results}"

if [[ "${results_dir}" != /tmp/* || "${results_dir}" == /tmp/ || -L "${results_dir}" ]]; then
    printf 'RENODE_RESULTS_DIR must be a non-symlinked directory below /tmp.\n' >&2
    exit 1
fi

if [[ -n "${RENODE_HOME:-}" ]]; then
    renode_test="${RENODE_HOME}/renode-test"
    export PATH="${RENODE_HOME}:${PATH}"
elif command -v renode-test >/dev/null 2>&1; then
    renode_test="$(command -v renode-test)"
else
    printf 'renode-test not found. Define RENODE_HOME or add it to PATH.\n' >&2
    exit 1
fi

if [[ ! -x "${renode_test}" ]]; then
    printf 'renode-test is not usable: %s\n' "${renode_test}" >&2
    exit 1
fi
if ! python3 -c 'import robot' >/dev/null 2>&1; then
    printf 'Robot Framework is unavailable in the active Python environment.\n' >&2
    exit 1
fi

build_arguments=()
if [[ "${rebuild}" == true ]]; then
    build_arguments+=(--rebuild)
fi
elf_path="$("${script_dir}/build-renode-firmware.sh" "${build_arguments[@]}")"

mkdir -p -- "${results_dir}"
run_dir="$(mktemp -d "${results_dir}/robot.XXXXXX")"
reports_dir="${run_dir}/reports"
generated_resc="${run_dir}/telemetry_firmware.resc"
platform_copy="${run_dir}/nucleo_f401re.repl"
capture_path="${run_dir}/usart2.log"
mkdir -p -- "${reports_dir}"

cleanup() {
    if [[ "${keep_results}" != true ]]; then
        rm -f -- "${generated_resc}" "${platform_copy}" "${capture_path}"
        case "${runtime_dir:-}" in
            "${run_dir}"/runtime) ;;
            "") return ;;
            *) printf 'Refusing to clean unexpected runtime path: %s\n' "${runtime_dir}" >&2; return ;;
        esac
        if [[ -d "${runtime_dir}" && ! -L "${runtime_dir}" ]]; then
            rm -rf -- "${runtime_dir}"
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

runtime_dir="${run_dir}/runtime"
mkdir -p -- "${runtime_dir}/tmp" "${runtime_dir}/dotnet" "${runtime_dir}/config"
export TMPDIR="${runtime_dir}/tmp"
export DOTNET_BUNDLE_EXTRACT_BASE_DIR="${runtime_dir}/dotnet"
export XDG_CONFIG_HOME="${runtime_dir}/config"

runner_arguments=(
    --jobs 1
    --stop-on-error
    --results-dir "${reports_dir}"
    --variable "RENODE_SCRIPT:${generated_resc}"
    --variable "CAPTURE_PATH:${capture_path}"
)
if [[ "${verbose}" == true ]]; then
    runner_arguments+=(--keep-renode-output)
fi

set +e
(
    cd /tmp
    "${renode_test}" "${runner_arguments[@]}" \
        "${repo_root}/renode/tests/telemetry_firmware.robot"
)
runner_status=$?
set -e

if ((runner_status != 0)); then
    printf 'Renode Robot validation failed with code %d.\n' \
        "${runner_status}" >&2
    exit "${runner_status}"
fi

if [[ ! -f "${reports_dir}/robot_output.xml" || \
      ! -f "${reports_dir}/log.html" || \
      ! -f "${reports_dir}/report.html" ]]; then
    printf 'Expected Robot reports were not generated.\n' >&2
    exit 1
fi
install -m 0644 "${reports_dir}/robot_output.xml" "${reports_dir}/output.xml"

printf 'Robot Framework validation: PASS\n'
printf 'output.xml: %s\n' "${reports_dir}/output.xml"
printf 'log.html: %s\n' "${reports_dir}/log.html"
printf 'report.html: %s\n' "${reports_dir}/report.html"
