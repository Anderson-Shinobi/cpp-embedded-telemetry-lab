# Renode Validation

This directory contains the permanent, headless validation path for the
NUCLEO-F401RE Zephyr telemetry firmware.

## Quick start

Activate the Zephyr environment, define `ZEPHYR_BASE`, and make Renode plus its
Robot environment available. Then run:

```sh
./tools/build-renode-firmware.sh --rebuild
./tools/run-renode-validation.sh --rebuild
./tools/run-renode-robot.sh --rebuild
```

The commands never flash a physical board. Build artifacts, UART captures, and
Robot reports remain below `/tmp` by default.

## Environment

- `RENODE_HOME`: directory containing `renode` and `renode-test`. When unset,
  the runners resolve those commands from `PATH`.
- `RENODE_BUILD_DIR`: temporary Zephyr build directory; defaults to
  `/tmp/cpp-embedded-telemetry-renode-build`.
- `RENODE_RESULTS_DIR`: temporary captures and reports directory; defaults to
  `/tmp/cpp-embedded-telemetry-renode-results`.
- `ZEPHYR_BASE`: required path to the active Zephyr checkout.
- `ZEPHYR_BOARD`: optional board override; defaults to `nucleo_f401re`.

Use `--keep-results` to retain generated `.resc` files and UART captures for
diagnosis. Robot reports are retained regardless. `--verbose` prints the
headless command or preserves runner diagnostics.

The validation design and known model limitations are documented in
[`docs/RENODE.md`](../docs/RENODE.md).
