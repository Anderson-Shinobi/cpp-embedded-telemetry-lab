# Continuous Integration

## Objective

`Embedded Telemetry CI` validates the complete repository boundary: C++20
Protocol and Host tests, Zephyr firmware tests and builds, deterministic
`native_sim` execution, the NUCLEO-F401RE ARM ELF, headless Renode execution,
USART2 capture, Protocol v1 and CRC-32 checks, and the Robot Framework suite.
The workflow never flashes hardware, creates a tag, publishes a release, or
writes to the repository.

## Triggers and concurrency

The workflow runs for:

- pushes to `main`;
- pull requests targeting `main`;
- manual `workflow_dispatch` runs;
- pushed tags matching `v*`.

Concurrency is grouped by workflow and Git ref. In-progress branch and pull
request runs are cancelled when a newer run supersedes them; tag runs are not
cancelled. Workflow permissions are restricted to `contents: read`.

## Jobs

| Job | Responsibility | Timeout |
|---|---|---:|
| `quality` | Whitespace, Bash syntax, ShellCheck, Python compilation, repository integrity, security scans and action pin checks | 15 minutes |
| `host-tests` | Installed `cpp-safe-concurrent-buffer` v0.3.0 package, strict-warning build and 97 CTest cases | 30 minutes |
| `firmware-tests` | 43 Twister/Ztest cases on `native_sim` | 45 minutes |
| `firmware-build` | `native_sim` build and execution, deterministic output, NUCLEO-F401RE build, ELF and memory checks | 45 minutes |
| `renode-validation` | Prebuilt ELF download, verified Renode install, three headless executions and seven Robot cases | 30 minutes |
| `ci-summary` | Success-only GitHub Step Summary and final Markdown report | 5 minutes |

`renode-validation` depends on `firmware-build` and consumes its ELF artifact;
it does not rebuild the NUCLEO firmware. `ci-summary` depends on all five
validation jobs and runs only when all of them succeed. Every failed validation
keeps its job in a failed state.

## Reproducible toolchains

The workflow creates a temporary west manifest that fixes Zephyr to revision
`746eb4060b3e573a8d25f4d3e5ac43a721876afc`. The official Zephyr setup action
installs west 1.5.0, Zephyr SDK 1.0.1 and the `arm-zephyr-eabi` toolchain. The
temporary manifest imports the module revisions declared by that exact Zephyr
commit.

The Host job checks out `cpp-safe-concurrent-buffer` v0.3.0 and verifies its
resolved commit as `93662e741ada326ebd31f856163036c5c2c1625f` before building
and installing the public CMake package.

Renode is downloaded only from the official v1.16.1 release as
`renode-1.16.1.linux-portable-dotnet.tar.gz`. Extraction occurs only after this
SHA-256 check succeeds:

```text
00e113cdbd0f5354cf2f64bbe3f5a070d8958409542fca66e45ac97d982938c0
```

The portable archive is extracted without a system-wide installation. Its
official Python requirements are installed in a temporary virtual environment.

## Action security

Every external action is pinned by a full commit SHA from its official
repository:

| Action | Human version | Commit SHA |
|---|---|---|
| `actions/checkout` | v4.2.2 | `11bd71901bbe5b1630ceea73d27597364c9af683` |
| `actions/upload-artifact` | v4.6.2 | `ea165f8d65b6e75b540449e92b4886f43607fa02` |
| `actions/download-artifact` | v4.3.0 | `d3f86a106a0bac45b974a628896c90dbdf5c8093` |
| `zephyrproject-rtos/action-zephyr-setup` | v1.0.15 | `be8136a8bba01580485d98b7ad2d32477c36a49a` |

Action updates are deliberate: review the official release and repository,
resolve the version to its complete commit SHA, update the inline version
comment and this table together, then run all local quality checks. Tags or
branches must not replace the pinned SHA.

## Validation artifacts

Artifacts are uploaded with `if: always()` so diagnostics remain available
when a validation fails. A missing required artifact is itself an error, and
test commands still determine the job result.

| Artifact | Contents |
|---|---|
| `host-test-results-<sha>` | Dependency, configure, build and CTest logs plus CTest XML |
| `firmware-twister-results-<sha>` | `twister.json`, `twister.xml`, console output and per-test logs |
| `nucleo-f401re-firmware-<sha>` | ELF, BIN, HEX, MAP when generated, memory/build logs and controlled `native_sim` output |
| `renode-robot-results-<sha>` | USART2 captures, controlled output, Renode logs, determinism hashes and Robot XML/HTML reports |
| `final-validation-report-<sha>` | Markdown summary copied to the GitHub Step Summary |

All artifacts are retained for 14 days. The downloaded Renode archive is never
uploaded.

## Local release validation

Activate the qualified Zephyr environment, define `ZEPHYR_BASE`, make Renode
available through `RENODE_HOME`, and point `HOST_BUILD_DIR` at a configured
97-test Host build. Then run:

```sh
./tools/run-release-validation.sh --rebuild
```

The unified runner stops on the first failure and writes:

- `/tmp/cpp-embedded-telemetry-release-results/release-validation.txt`;
- `/tmp/cpp-embedded-telemetry-release-results/controlled-output.txt`;
- `/tmp/cpp-embedded-telemetry-release-results/validation-summary.md`.

Set `RELEASE_RESULTS_DIR` to another non-symlinked directory below `/tmp` when
an isolated result location is needed. Use `--keep-results` to retain detailed
Renode and Robot intermediates and `--verbose` to acknowledge retained logs.

Present only an approved report with:

```sh
./tools/present-release-validation.sh
./tools/present-release-validation.sh --frames --recording-mode
```

The presentation script refuses incomplete reports, validates the controlled
output checksum before displaying frames, and does not rerun the full pipeline.

## Branch protection recommendation

Protect `main` and require the six `Embedded Telemetry CI` jobs before merge.
Require pull requests, dismiss stale approvals after new commits, prevent force
pushes and deletion, and restrict bypass permissions. Keep the workflow token
at read-only repository access.

## Failure and scope boundaries

The validation is fail-fast at command and job boundaries. Artifact upload on
failure preserves evidence but does not turn failures into success. The final
summary is emitted only after every dependency succeeds, so it never reports
an ignored or failed job as passing.

The Renode model covers the CPU, target memory, USART2 and peripherals required
by the current deterministic workload. It is not complete silicon validation,
hardware-in-the-loop, electrical UART validation, real sensor acquisition or
physical flashing. Detailed emulator limitations remain in
[`docs/RENODE.md`](RENODE.md).
