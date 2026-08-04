# Graphify Architectural Audit Report

## Audit Baseline

- Repository branch: `main`
- Base commit: `d89b2ef24d63bde7c375195f785f750004d08af6`
- Remote comparison: `HEAD` equals `origin/main` after `git fetch origin --prune`
- Graphify executable: `graphify`
- Graphify version: `0.9.25`
- Graph mode: directed
- Extraction mode: deterministic AST plus semantic extraction for CMake and Twister metadata
- Token cost reported by the graph build: 0 input, 0 output

The input corpus was assembled from repository-relative copies of `protocol/`,
`host/`, `firmware/`, `tests/`, `tools/`, `CMakeLists.txt`, and
`CMakePresets.json`. Build trees, installed dependencies, SDKs, external Zephyr
workspaces, binary artifacts, and previous graph output were excluded.

Graphify detected 49 supported files: 42 code files and 7 document files,
with approximately 8,118 words. The physical corpus also contained four
`.gitkeep` files and two `prj.conf` files that Graphify 0.9.25 does not classify.
Those files were inspected separately for architectural context but do not
contribute nodes or edges.

## Metrics

| Metric | Result |
|---|---:|
| Detected files | 49 |
| AST nodes before merge | 532 |
| Semantic nodes | 27 |
| Final nodes | 559 |
| Raw extracted edges | 1,206 |
| Final directed edges | 724 |
| Communities | 31 |
| Directed simple cycles | 8 |
| Cyclic strongly connected components | 8 |
| Weakly connected components | 7 |
| Strongly connected components | 551 |
| Orphan input files | 1 |
| Isolated graph nodes | 1 |
| Maximum node fan-in | 13 |
| Maximum node fan-out | 52 |
| Maximum file fan-in | 15 |
| Maximum file fan-out | 8 |

The seven weak components have 514, 27, 5, 5, 4, 3, and 1 nodes. The main
component contains production and test code; CMake metadata forms a second
component. Each Shell script is a separate component because this Graphify
version does not resolve commands or script-to-script execution as file edges.
`firmware/src/hex_formatter.cpp` forms a four-node component because its
cross-file type reference was not resolved. The remaining one-node component
is the `crc32.hpp` file node.

`CMakePresets.json` is the single detected input file that produced no nodes.
The isolated node is `protocol/include/telemetry_protocol/crc32.hpp`. Both are
extractor limitations; direct inspection confirms that the preset file is
valid configuration and that `crc32.hpp` is used by production code.

## Graph Health

The official directed multigraph diagnostic reported:

- 1,206 raw edges;
- 1,047 valid candidate edges;
- 159 dangling-endpoint edges;
- zero missing-endpoint edges;
- zero self-loops;
- 3 exact duplicate edges;
- 338 directed same-endpoint collapsed edges;
- 724 post-build edges.

Most collapsed edges are repeated test macro instances, including 43 `ZTEST`
instances and 29 protocol `TEST` instances. Therefore, the final graph is sound
for dependency direction and reachability but understates edge multiplicity.
Counts that depend on repeated macro occurrences must use the source/test
inventory rather than final `DiGraph` multiplicity.

## Dependency Direction

The desired production direction is preserved:

```text
host -> protocol
firmware -> protocol
```

There are 9 final cross-module edges from Host to Protocol and 3 from Firmware
to Protocol. There are no production edges from Protocol to Host or Firmware,
and no edges in either direction between Host and Firmware.

Tests have 48 edges to production: 18 to Protocol, 14 to Host, and 16 to
Firmware. Production has zero edges to Tests. Test dependencies therefore point
in the expected direction.

At the file level, `protocol_types.hpp` has the highest fan-in at 15 files.
This is appropriate for the shared wire-contract header. The highest file
fan-out is `tests/firmware/src/main.cpp` at 8 files. At node level,
`TelemetryFrame` has fan-in 13, and Graphify's aggregated `ZTEST()` node has
fan-out 52. The `ZTEST()` value reflects macro aggregation, not one runtime
function with 52 responsibilities.

## Module Assessment

### Protocol Core

Protocol Core uses standard C++ only. It has no Zephyr, Host, concurrent-buffer,
test, or tool dependency. Serialization is explicit, fixed at 34 bytes,
big-endian, and protected by CRC-32. Its only outgoing file-level edge outside
the module is the semantic CMake relationship to the root warning helper; this
is build configuration, not a functional dependency.

### Host

Host depends on `telemetry::protocol`,
`cpp_safe_concurrent_buffer::concurrent_buffer`, and Threads for its executable.
Its CMake path is independent of Zephyr and does not add the firmware directory.
`host/src/main.cpp` is 72 lines and performs composition and lifecycle control;
domain logic remains in Host components. No excessive entrypoint concentration
was detected.

### Firmware

Firmware depends on Protocol Core and Zephyr APIs. It has no dependency on the
Host or `cpp-safe-concurrent-buffer`. The deterministic sensor model is separate
from queueing and transport; no physical sensor, network, filesystem, or board
driver is mandatory. `firmware/src/main.cpp` is 142 lines and concentrates
thread startup, completion synchronization, final metrics, and process exit.
This is a cohesive application entrypoint, though it is a boundary to watch as
Renode-specific behavior is added.

### Tests

Tests remain separated into Protocol, Host, and Firmware domains. The existing
host build discovers 97 tests: 29 Protocol tests, 67 Host tests, and one Host
smoke test. Firmware source contains 43 `ZTEST` cases. Test code depends on
production; production does not depend on test code.

### Tools

The three Shell scripts orchestrate builds, tests, execution, presentation, and
output checks. They are not linked or imported by functional code. Direct
inspection shows that the scripts do not reimplement binary serialization or
CRC. They do contain a textual acceptance oracle for exactly eight `TLFRAME`
lines, 68 hexadecimal characters, the summary, and the done marker. Graphify
does not resolve the two script-to-script executions, so these relationships
are not represented as graph edges.

### CMake

The root project adds Protocol, then Host, then Tests. Host links Protocol and
its external concurrent-buffer package. Firmware is a separate Zephyr
application and directly compiles the three Protocol source files. Firmware
tests directly compile Firmware and Protocol sources. The Host configure path
does not evaluate `firmware/CMakeLists.txt`, and Firmware CMake does not affect
the Host path.

The file-level graph shows three CMake cycles because the root file adds child
directories while child targets call a helper defined by the root file. These
are valid configure-time parent/helper relationships, not circular target-link
dependencies. No functional or target dependency cycle was detected.

## Renode Readiness

| Requirement | Status | Evidence or blocker |
|---|---|---|
| NUCLEO-F401RE ELF available now | BLOCKED | A prior successful build is documented, but no `zephyr.elf` is present in the current worktree. It must be regenerated. |
| Deterministic `TLFRAME` output | READY | Eight fixed samples, fixed sequence/timestamps, and no real clock or sensor input. |
| Stable serialization | READY | Fixed 34-byte Protocol v1 frame and 68-character uppercase hexadecimal output. |
| Deterministic `native_sim` termination | READY | End-of-stream, thread joins through semaphores, metric checks, and `nsi_exit`. |
| Firmware tests | READY | 43 source-visible Ztests and documented successful Twister execution. |
| Protocol documentation | READY | Layout, endianness, accepted values, parsing order, and CRC are documented. |
| Firmware/hardware separation | READY | Deterministic model and Protocol are separated from Zephyr queue/console adapters. |
| No mandatory physical drivers | READY | Output uses Zephyr console; there is no physical sensor or network requirement. |
| Headless execution | PARTIALLY READY | Firmware is finite and console-driven, but a Renode headless command is not yet defined or validated. |
| Console capture | PARTIALLY READY | `printk` output is stable; the STM32F401RE UART/peripheral mapping remains to be proven in Renode. |
| `.resc` compatibility | PARTIALLY READY | The ELF and console flow fit a `.resc` scenario, but platform and script files do not yet exist. |
| Robot Framework validation | PARTIALLY READY | Assertions are well defined; Robot tooling and resource files are not installed or implemented locally. |
| Compare capture with Protocol Core | PARTIALLY READY | Protocol deserialization and CRC validation exist, but no capture-to-Core adapter exists. |

Overall readiness is **PARTIALLY READY**. Mission 4 can begin with environment
qualification and an ELF rebuild. Execution is blocked until a concrete Renode
version, STM32F401RE platform/UART mapping, and regenerated ELF are available.

## Findings by Severity

### CRITICAL

No critical architectural findings detected.

### HIGH

No high architectural findings detected.

### MEDIUM

#### GRA-M-001 — NUCLEO ELF is not present

- Severity: MEDIUM
- File or module: Firmware build artifact
- Evidence: no `zephyr.elf`, firmware ELF, BIN, or HEX output is present in the current worktree; `docs/FIRMWARE.md` records a prior successful build.
- Impact: Renode cannot load the firmware from the current checkout without a reproducible rebuild.
- Recommendation: make the NUCLEO-F401RE ELF build the first Mission 4 gate and record its exact path and checksum.
- Blocks Mission 4: Yes, blocks Renode execution but not planning.

#### GRA-M-002 — Protocol source lists are repeated in Zephyr CMake

- Severity: MEDIUM
- File or module: `firmware/CMakeLists.txt`, `tests/firmware/CMakeLists.txt`
- Evidence: both files enumerate Protocol `.cpp` sources directly; firmware tests also enumerate Firmware `.cpp` sources.
- Impact: adding or removing a source can make Host, Firmware, and Firmware-test builds drift.
- Recommendation: during a later architecture mission, evaluate a Zephyr-compatible reusable target or a single authoritative source-list definition without coupling Host CMake to Zephyr.
- Blocks Mission 4: No.

#### GRA-M-003 — Graph edge multiplicity is lossy

- Severity: MEDIUM
- File or module: Graphify output
- Evidence: 159 dangling endpoint references and 338 directed same-endpoint collapsed edges were reported by the official diagnostic.
- Impact: final edge counts and macro fan-out cannot represent every repeated test occurrence.
- Recommendation: retain raw diagnostic counts with final metrics and use source inventory for test counts.
- Blocks Mission 4: No.

#### GRA-M-004 — Firmware tests are concentrated in one file

- Severity: MEDIUM
- File or module: `tests/firmware/src/main.cpp`
- Evidence: 553 lines, maximum file fan-out of 8, and 43 `ZTEST` cases aggregated into the graph's maximum node fan-out of 52.
- Impact: future Renode/system tests may blur unit, concurrency, and system validation boundaries if added here.
- Recommendation: keep Robot/Renode scenarios under `renode/tests/` and do not add them to the Ztest translation unit.
- Blocks Mission 4: No.

#### GRA-M-005 — Renode and Robot executables are unavailable locally

- Severity: MEDIUM
- File or module: Mission 4 toolchain
- Evidence: no `renode`, `renode-test`, or `robot` executable is discoverable in the current environment.
- Impact: platform compatibility, UART capture, and headless failure behavior cannot yet be validated.
- Recommendation: select and document supported versions, then provision them explicitly in the future mission and CI environment.
- Blocks Mission 4: Yes, blocks execution and automated validation but not design work.

### LOW

#### GRA-L-001 — Shell acceptance oracle duplicates output literals

- Severity: LOW
- File or module: `tools/run-firmware-demo.sh`
- Evidence: the script repeats the eight-frame count, 68-character length, exact summary, and done marker.
- Impact: an intentional protocol/output change would require synchronized script maintenance.
- Recommendation: in Mission 4, centralize deep frame validation through Protocol Core and keep Shell checks as a lightweight demonstration gate.
- Blocks Mission 4: No.

#### GRA-L-002 — CMake control flow appears cyclic in the semantic graph

- Severity: LOW
- File or module: root, Host, and Protocol CMake files
- Evidence: root-to-child `add_subdirectory` relationships combine with child-to-root warning-helper calls.
- Impact: readers could mistake configure-time helper use for a link dependency cycle.
- Recommendation: document the distinction; no code change is required.
- Blocks Mission 4: No.

### INFORMATIONAL

#### GRA-I-001 — Extractor coverage gaps

- Severity: INFORMATIONAL
- File or module: `CMakePresets.json`, `protocol/include/telemetry_protocol/crc32.hpp`, `firmware/src/hex_formatter.cpp`, Shell scripts
- Evidence: the preset produced zero nodes; the CRC header is isolated; Hex Formatter and each script appear as disconnected components despite source-visible use or orchestration.
- Impact: disconnected-component counts overstate architectural isolation.
- Recommendation: interpret these components with direct source inspection, as done in this report.
- Blocks Mission 4: No.

#### GRA-I-002 — Layer boundaries are preserved

- Severity: INFORMATIONAL
- File or module: Protocol, Host, Firmware, Tests, Tools
- Evidence: Host and Firmware point to Protocol; Host and Firmware do not point to each other; production does not point to Tests; Tools are not functional dependencies.
- Impact: the current structure supports adding a simulator-validation layer without modifying Protocol v1.
- Recommendation: preserve these directions in Mission 4.
- Blocks Mission 4: No.

## Architectural Conclusion

No layer violation or functional circular dependency was detected. Protocol
Core remains portable, Host remains independent of Zephyr, Firmware remains
independent of Host and the concurrent-buffer library, and Tools remain an
orchestration layer. The main prerequisites for Mission 4 are operational:
rebuild the NUCLEO ELF, qualify the Renode STM32F401RE/UART model, establish
headless capture, and connect captured frames to Protocol Core validation.
