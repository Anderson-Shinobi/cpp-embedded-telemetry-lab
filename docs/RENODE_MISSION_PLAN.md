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

Mission 4.0 qualified Renode 1.16.1 with its bundled .NET 8 runtime and Robot
Framework 6.1 in an isolated environment. A pristine NUCLEO-F401RE build
produced a loadable ELF, and the upstream `platforms/cpus/stm32f4.repl` model
booted it headlessly through the complete deterministic workload. Its USART2
file backend captured exactly eight ordered 68-character `TLFRAME` values, the
expected summary, and the final completion marker.

Readiness remains partial at the mission level because the repository still
has no durable Renode scenario, Robot acceptance suite, Protocol Core capture
validator, or CI integration. The upstream STM32F4 model is a viable base but
is not an exact STM32F401RE board description: its modeled flash, RAM, CPU
variant label, and SysTick frequency are broader or different from the target.

Readiness gates:

| Gate | Initial state | Required Mission 4 result |
|---|---|---|
| NUCLEO-F401RE ELF | READY | Reproduce the pristine build and record the `zephyr.elf` checksum in the validation run. |
| Deterministic frames | READY | Preserve exactly eight ordered frames. |
| Protocol and CRC | READY | Reuse Protocol Core as the validation authority. |
| Headless execution | READY | Productize the proven non-interactive command with a finite timeout. |
| Console capture | READY | Productize capture from USART2 at `0x40004400`, IRQ 38, 115200 baud. |
| Robot automation | PARTIALLY READY | The official runner passed a packaged smoke test; project suites, keywords, failure behavior, and reports remain to be added. |

Preparation evidence and constraints:

- The target is ARM ELF32, little-endian, EABI5, with entry point `0x08001079`;
  Renode initialized the CPU at that entry point with stack pointer
  `0x20002FC0`.
- Loadable content occupies flash from `0x08000000` and RAM from `0x20000000`.
  The build reported 30,624 bytes of flash and 12,428 bytes of RAM.
- Zephyr selects USART2 as `zephyr,console`; the generated device tree records
  base address `0x40004400`, IRQ 38, and 115200 baud. These match the USART2
  peripheral in the upstream STM32F4 model.
- The upstream model uses a Cortex-M4 CPU label, 2 MiB flash, 256 KiB RAM, and a
  72 MHz SysTick, while STM32F401RE requires Cortex-M4F semantics, 512 KiB
  flash, 96 KiB RAM, and an 84 MHz board clock. A minimal project wrapper must
  constrain or document these differences instead of copying the generic model
  unchanged.
- Platform loading requires the official STM32F40x SVD resource. The future
  workflow must pin, cache, or otherwise provision it reproducibly for offline
  and CI execution.
- The capability run reached Zephyr boot, emitted all expected records, and did
  not halt the CPU during the validation interval. Model warnings for partially
  implemented NVIC, flash-controller, RCC, and debug-register writes must be
  reviewed when defining the supported validation boundary.

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

1. Pin Renode 1.16.1 and use the qualified upstream STM32F4 model as the base
   for a minimal STM32F401RE project wrapper with explicit memory and clock
   constraints.
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
2. Renode 1.16.1 loads that ELF with no unmapped-memory or fatal CPU error
   during the validation interval; any accepted model warning is documented
   and narrowly justified.
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
