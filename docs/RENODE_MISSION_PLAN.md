# Mission 4 — Renode Integration and Automated Firmware Validation

## Objective

Add a reproducible, headless Renode validation path for the existing
NUCLEO-F401RE Zephyr firmware. The mission will boot the firmware ELF, capture
its console output, extract exactly eight `TLFRAME` records, validate their
Protocol v1 contents and CRC, verify deterministic completion markers, and
produce a Robot Framework report without changing the protocol or existing
Mission 1–3 behavior.

## Current Readiness

The firmware already has a finite deterministic workload, stable console
records, a 34-byte Protocol v1 serializer, a 68-character hexadecimal formatter,
explicit summary and completion markers, deterministic `native_sim` exit, 43
Firmware tests, 29 Protocol tests, 67 Host tests, and one Host smoke test.

Readiness is partial because the current checkout has no NUCLEO-F401RE ELF,
Renode and Robot Framework are not available in the current environment, and
the exact STM32F401RE platform/UART mapping has not been proven. Mission 4 must
start with tool-version qualification and a reproducible ELF build.

Readiness gates:

| Gate | Initial state | Required Mission 4 result |
|---|---|---|
| NUCLEO-F401RE ELF | BLOCKED | Rebuild and record the loadable `zephyr.elf` path and checksum. |
| Deterministic frames | READY | Preserve exactly eight ordered frames. |
| Protocol and CRC | READY | Reuse Protocol Core as the validation authority. |
| Headless execution | PARTIALLY READY | Define and validate a non-interactive Renode command. |
| Console capture | PARTIALLY READY | Prove the modeled UART and capture boundaries. |
| Robot automation | PARTIALLY READY | Add suites, keywords, exit behavior, and reports. |

## Target Architecture

```text
Zephyr Firmware ELF
        ↓
Renode Platform
        ↓
Console Capture
        ↓
TLFRAME Extraction
        ↓
Frame Validation
        ↓
Robot Framework
        ↓
CI Report
```

The Renode layer will consume the existing ELF and must not become a dependency
of Protocol, Host, or Firmware code. Console extraction will accept only complete
`TLFRAME`, `TLFIRMWARE SUMMARY`, and `TLFIRMWARE DONE` records. Deep frame
validation will decode hexadecimal bytes through a small host-side adapter that
links the existing Protocol Core, so Robot assertions do not duplicate the
binary layout or CRC implementation.

The planned execution sequence is:

1. Pin a supported Renode version and verify whether an upstream STM32F401RE or
   NUCLEO-F401RE platform description is sufficient.
2. Rebuild the Zephyr ELF with deterministic configuration and capture its
   checksum and memory-map inputs.
3. Define the platform, CPU, flash/RAM regions, clock, and console UART.
4. Create a `.resc` script that loads the ELF, exposes the console, starts
   emulation, and enforces a finite timeout.
5. Capture console output headlessly and isolate telemetry records from Zephyr
   boot logs.
6. Validate count, order, line format, completion markers, decoded fields, and
   CRC through reusable Robot keywords and Protocol Core.
7. Return nonzero on boot, timeout, capture, format, semantic, CRC, ordering, or
   completion failure.
8. Re-run all Mission 1–3 test suites and publish Robot results in CI.

## Proposed Files

Initial required files:

- `renode/platforms/nucleo_f401re.repl` — platform description or a minimal
  project wrapper around a verified upstream platform.
- `renode/scripts/telemetry_firmware.resc` — ELF loading, UART analyzer,
  execution, timeout, and shutdown scenario.
- `renode/tests/telemetry_firmware.robot` — end-to-end acceptance scenarios.
- `renode/tests/resources/telemetry_keywords.robot` — reusable boot, capture,
  extraction, ordering, and assertion keywords.
- `renode/README.md` — concise local execution entrypoint and prerequisites.
- `docs/RENODE.md` — versioned architecture, platform assumptions,
  troubleshooting, and validation evidence.

Expected supporting files, subject to the first design checkpoint:

- `renode/validation/CMakeLists.txt` — isolated host validator build linked to
  `telemetry::protocol`.
- `renode/validation/validate_capture.cpp` — capture validator that invokes
  Protocol Core deserialization and returns machine-readable failures.

No planned Renode file will be added to the root Host build by default. The
validator may be enabled only by the Renode validation workflow so that the
normal Host and Zephyr configure paths remain independent.

## Validation Goals

- Load the Zephyr NUCLEO-F401RE ELF.
- Start the Renode platform without interactive UI requirements.
- Capture the configured console reliably.
- Find exactly eight `TLFRAME` lines.
- Validate `TLFIRMWARE SUMMARY produced=8 transmitted=8 queue_errors=0`.
- Validate a final `TLFIRMWARE DONE` record.
- Validate frame order and transmission indices `0001` through `0008`.
- Validate exactly 68 uppercase hexadecimal characters per frame.
- Decode 34 bytes and validate magic, version, message type, and payload size.
- Validate sequence order, deterministic timestamps, and payload values.
- Validate CRC through Protocol Core.
- Execute headlessly with a finite timeout.
- Return a nonzero error code for every failed acceptance condition.
- Generate Robot Framework `output.xml`, `report.html`, and `log.html` artifacts.
- Preserve all 29 Protocol tests, 67 Host tests, one Host smoke test, and 43
  Firmware tests.

## Risks

- Compatibility of the available Renode model with STM32F401RE and the NUCLEO
  board memory map.
- Correct UART instance, base address, interrupts, pin routing, baud behavior,
  and console attachment.
- ELF sections and symbols generated by the selected Zephyr version and
  toolchain.
- Differences between the `native_sim` console and a simulated STM32 UART.
- Need for a custom platform description or peripheral substitution.
- Incomplete boot caused by clocks, flash layout, interrupts, or unsupported
  peripherals.
- Timing races between console attachment, firmware boot, and Robot assertions.
- Loss, buffering, duplication, or interleaving of captured log records.
- Renode and Robot Framework version drift between developer and CI machines.
- CI image size, installation method, caching, runtime, and report retention.
- Checkout paths containing spaces and Zephyr build-directory constraints.
- Accidental duplication of Protocol parsing or CRC logic in Robot keywords.

Mitigations will include pinned tool versions, an explicit platform smoke gate,
finite timeouts, capture normalization limited to telemetry prefixes, Protocol
Core reuse, artifact checksums, and a clean CI environment.

## Acceptance Criteria

Mission 4 will be accepted only when all of the following are true:

1. A documented command reproducibly builds the NUCLEO-F401RE `zephyr.elf` from
   the current Firmware sources.
2. A pinned Renode version loads that ELF with no unmapped-memory or fatal CPU
   error during the validation interval.
3. The selected UART console is documented and captured in headless execution.
4. One command runs the Robot suite without manual interaction and with a finite
   timeout.
5. A passing run captures exactly eight ordered `TLFRAME` lines, one exact
   summary line, and one final done line.
6. Every frame contains exactly 68 uppercase hexadecimal characters and decodes
   to exactly 34 bytes.
7. Protocol Core accepts every captured frame and confirms magic `TL`, version
   1, telemetry-sample type, payload size 12, deterministic sequence/timestamp,
   payload values, and CRC.
8. Missing, duplicated, reordered, malformed, truncated, corrupt, or
   CRC-invalid frames cause a nonzero result with a precise diagnostic.
9. Missing summary, wrong counts, queue errors, missing done marker, boot
   failure, or timeout causes a nonzero result.
10. Robot Framework generates machine-readable and human-readable reports.
11. Existing Protocol, Host, smoke, and Firmware test counts remain unchanged
    and all tests pass.
12. Host CMake remains usable without Zephyr or Renode, and Firmware remains
    independent of Host and `cpp-safe-concurrent-buffer`.
13. No absolute developer path, SDK path, token, credential, or private data is
    embedded in scripts, logs selected for publication, or reports.
14. Protocol v1 bytes, semantics, and public interfaces remain unchanged.

## Out of Scope

- Physical flashing.
- Hardware-in-the-loop.
- Real sensors.
- Network transport.
- Advanced fault injection.
- Graphical interfaces.
- Protocol changes.
- Performance benchmarking beyond a practical CI timeout.
- Validation of electrical characteristics or real UART signal integrity.
