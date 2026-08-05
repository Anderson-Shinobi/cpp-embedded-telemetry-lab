# Renode Firmware Validation

## Objective

The Renode integration executes the real NUCLEO-F401RE Zephyr ELF without
physical hardware, captures USART2 headlessly, and validates the complete
deterministic Protocol v1 output. Renode remains outside the Protocol, Host,
and Firmware dependency graphs.

## Qualified environment

- Renode 1.16.1.17033, build `d66b0c2a-202602160923`;
- bundled .NET 8.0.12 runtime;
- Robot Framework 6.1 through the official `renode-test` runner;
- Zephyr 4.4.99 and Zephyr SDK 1.0.1;
- NUCLEO-F401RE target `nucleo_f401re/stm32f401xe`.

The runners resolve Renode through `RENODE_HOME` or `PATH`; no developer path
is embedded in the repository.

## Architecture

```text
Zephyr firmware source
        |
        v
temporary ARM ELF
        |
        v
STM32F401RE Renode platform
        |
        v
USART2 file backend / Robot UART tester
        |
        v
Protocol v1 structure and CRC validation
```

`renode/scripts/telemetry_firmware.resc` is a versioned template. The shell
runners replace only `@ELF_PATH@` and `@PLATFORM_PATH@` in a temporary copy,
leaving the repository file unchanged. Capture is attached before emulation
starts.

## Platform and minimum map

The project platform is derived from the peripheral types and connections in
Renode's upstream `platforms/cpus/stm32f4.repl`, but is intentionally smaller
and does not download or apply an SVD at runtime.

| Component | Model or address |
|---|---|
| CPU | Renode Cortex-M4 |
| NVIC | `0xE000E000`, SysTick 84 MHz |
| Flash | `0x08000000`, 512 KiB |
| SRAM | `0x20000000`, 96 KiB |
| USART1 | `0x40011000`, IRQ 37 |
| USART2 console | `0x40004400`, IRQ 38, 115200 baud |
| RCC | `0x40023800` |
| Flash controller | `0x40023C00` |
| GPIO A/B | `0x40020000` / `0x40020400` |

USART1 and GPIO B are present because the NUCLEO device tree enables and
initializes USART1 even though telemetry is captured only from USART2. RTC and
Timer9 are included as explicit dependencies of the available STM32F4 RCC
model. Bit-band aliases reflect Cortex-M4 memory behavior used by the SoC
family.

## Model limitations

Renode does not provide an exact upstream NUCLEO-F401RE platform in the
qualified release. The project narrows flash and SRAM to the real 512 KiB and
96 KiB capacities and sets the 84 MHz board clock, while reusing generic STM32F4
peripheral models.

The model is sufficient for this firmware path, not a claim of full MCU or
board fidelity. Expected warnings currently concern partially implemented
NVIC status clearing, flash cache-control bits, RCC HSE bypass, and the
unmodeled debug-control register. They remain visible. Fatal Renode errors,
CPU halts, Zephyr faults, and assertions fail validation.

The Cortex-M4 model label does not separately express the STM32F401RE FPU. The
current ELF uses the soft-float ABI and executes without relying on modeled
floating-point hardware.

## Building the ELF

Activate the Zephyr virtual environment and define `ZEPHYR_BASE`, then run:

```sh
./tools/build-renode-firmware.sh --rebuild
```

The build uses `RENODE_BUILD_DIR`, defaulting to
`/tmp/cpp-embedded-telemetry-renode-build`. A temporary copy of `prj.conf`
avoids the Zephyr 4.4.99 checkout-path limitation when the repository path
contains spaces. The script performs a pristine NUCLEO-F401RE build, validates
the ARM ELF with `file`, reports flash/RAM usage, and prints the final temporary
ELF path. It never invokes a flash target.

## Headless validation

```sh
./tools/run-renode-validation.sh --rebuild
```

The runner creates an isolated temporary scenario, loads the ELF, attaches an
USART2 file backend, and executes 250 ms of virtual time under a 30-second wall
clock timeout. This bounded interval is longer than the deterministic workload;
the capture must contain `TLFIRMWARE DONE`. The temporary machine is then
disposed by Renode.

The validator requires:

- one Zephyr boot banner;
- exactly eight `TLFRAME` records;
- indices `0001` through `0008` in order;
- 68 uppercase hexadecimal characters per encoded frame;
- magic `544C`, version `01`, type `01`, and payload size `000C`;
- binary sequences 1000 through 1007;
- a valid CRC for all eight frames;
- the exact summary with produced/transmitted counts of eight and zero queue
  errors;
- exactly one final `TLFIRMWARE DONE`, with no later frame;
- no fatal, halt, panic, or assertion marker.

CRC validation replicates the public Protocol v1 contract: reflected polynomial
`0xEDB88320`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`, the first 30
bytes as input, and a four-byte big-endian expected value. It does not contain a
static list of expected CRCs.

## Robot Framework

```sh
./tools/run-renode-robot.sh --rebuild
```

The permanent suite uses the official Renode UART tester as its primary
synchronization mechanism and waits for the boot banner and completion marker
with a ten-second timeout. Seven focused cases validate boot, frame count,
encoding and headers, order and CRC, summary, completion, and fault absence.

Reports are written below `RENODE_RESULTS_DIR` as `output.xml`, `log.html`, and
`report.html`. They are local artifacts and are not published automatically.

## Determinism

Determinism is checked by running the headless validation three times, retaining
only lines beginning with `TLFRAME` or `TLFIRMWARE`, and comparing their SHA-256
hashes and bytes. Fixed sleeps are not used as the Robot synchronization
mechanism.

## Temporary directories

- `/tmp/cpp-embedded-telemetry-renode-build`: default Zephyr build;
- `/tmp/cpp-embedded-telemetry-renode-results`: default captures and reports;
- runner-owned runtime/configuration directories below each result directory.

These paths can be changed with environment variables but must remain below
`/tmp` to keep cleanup bounded and repository-independent.

## Scope and next steps

This integration performs no physical flash, debug, attach,
hardware-in-the-loop, sensor, or network operation. It does not alter Protocol
v1 or the firmware. Future work may add CI report publication and more precise
STM32F401RE peripheral modeling; CI and fault injection are outside this
mission.
