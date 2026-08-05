#!/usr/bin/env python3
"""Validate captured Protocol v1 telemetry without invoking subprocesses."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True


FRAME_PATTERN = re.compile(r"^TLFRAME ([0-9]{4}) ([0-9A-F]{68})$")
SUMMARY = "TLFIRMWARE SUMMARY produced=8 transmitted=8 queue_errors=0"
DONE = "TLFIRMWARE DONE"
EXPECTED_FRAME_COUNT = 8
FRAME_SIZE = 34
CRC_OFFSET = 30
CRC_POLYNOMIAL = 0xEDB88320
CRC_INITIAL = 0xFFFFFFFF
CRC_FINAL_XOR = 0xFFFFFFFF


def _lines(path: str | Path) -> list[str]:
    return Path(path).read_text(encoding="utf-8").replace("\r", "").splitlines()


def _frames(path: str | Path) -> list[tuple[str, str]]:
    frames: list[tuple[str, str]] = []
    for line in _lines(path):
        if line.startswith("TLFRAME "):
            match = FRAME_PATTERN.fullmatch(line)
            if match is None:
                raise AssertionError(f"Malformed TLFRAME line: {line}")
            frames.append((match.group(1), match.group(2)))
    return frames


def calculate_crc32(data: bytes) -> int:
    """Replicate the public reflected CRC-32 contract used by Protocol v1."""
    crc = CRC_INITIAL
    for byte in data:
        crc ^= byte
        for _ in range(8):
            mask = -(crc & 1) & 0xFFFFFFFF
            crc = ((crc >> 1) ^ (CRC_POLYNOMIAL & mask)) & 0xFFFFFFFF
    return crc ^ CRC_FINAL_XOR


class TelemetryValidation:
    """Robot Framework library and command-line validation facade."""

    ROBOT_LIBRARY_SCOPE = "SUITE"

    def validate_zephyr_boot(self, capture_path: str) -> None:
        if not any(line.startswith("*** Booting Zephyr OS build ") for line in _lines(capture_path)):
            raise AssertionError("Zephyr boot banner was not captured")

    def validate_frame_count(self, capture_path: str) -> None:
        count = len(_frames(capture_path))
        if count != EXPECTED_FRAME_COUNT:
            raise AssertionError(f"Expected 8 TLFRAME lines, observed {count}")

    def validate_frame_encoding_and_headers(self, capture_path: str) -> None:
        for index, (_, encoded) in enumerate(_frames(capture_path), start=1):
            if len(encoded) != FRAME_SIZE * 2:
                raise AssertionError(f"Frame {index} does not contain 68 hexadecimal characters")
            raw = bytes.fromhex(encoded)
            if raw[0:2] != b"TL":
                raise AssertionError(f"Frame {index} has invalid magic")
            if raw[2] != 1:
                raise AssertionError(f"Frame {index} has invalid protocol version")
            if raw[3] != 1:
                raise AssertionError(f"Frame {index} has invalid message type")
            if int.from_bytes(raw[4:6], "big") != 12:
                raise AssertionError(f"Frame {index} has invalid payload size")

    def validate_indices_sequences_and_crc(self, capture_path: str) -> None:
        frames = _frames(capture_path)
        expected_indices = [f"{index:04d}" for index in range(1, 9)]
        indices = [index for index, _ in frames]
        if indices != expected_indices:
            raise AssertionError(f"Unexpected transmission indices: {indices}")

        for offset, (_, encoded) in enumerate(frames):
            raw = bytes.fromhex(encoded)
            sequence = int.from_bytes(raw[6:10], "big")
            expected_sequence = 1000 + offset
            if sequence != expected_sequence:
                raise AssertionError(
                    f"Expected sequence {expected_sequence}, observed {sequence}"
                )
            calculated = calculate_crc32(raw[:CRC_OFFSET])
            expected = int.from_bytes(raw[CRC_OFFSET:], "big")
            if calculated != expected:
                raise AssertionError(
                    f"Frame {offset + 1} CRC mismatch: {calculated:08X} != {expected:08X}"
                )

    def validate_firmware_summary(self, capture_path: str) -> None:
        summaries = [line for line in _lines(capture_path) if line.startswith("TLFIRMWARE SUMMARY")]
        if summaries != [SUMMARY]:
            raise AssertionError(f"Unexpected firmware summary records: {summaries}")

    def validate_firmware_done(self, capture_path: str) -> None:
        lines = _lines(capture_path)
        done_positions = [index for index, line in enumerate(lines) if line == DONE]
        if len(done_positions) != 1:
            raise AssertionError(f"Expected one DONE record, observed {len(done_positions)}")
        if any(line.startswith("TLFRAME ") for line in lines[done_positions[0] + 1 :]):
            raise AssertionError("A TLFRAME record appeared after DONE")
        controlled = [line for line in lines if line.startswith(("TLFRAME", "TLFIRMWARE"))]
        if not controlled or controlled[-1] != DONE:
            raise AssertionError("DONE is not the final controlled firmware record")

    def validate_fault_free(self, capture_path: str, monitor_path: str = "") -> None:
        capture = "\n".join(_lines(capture_path)).upper()
        capture_markers = (
            "ASSERTION FAIL",
            "HARD FAULT",
            "KERNEL PANIC",
            "FATAL ERROR",
            "UNHANDLED EXCEPTION",
        )
        for marker in capture_markers:
            if marker in capture:
                raise AssertionError(f"Firmware fault marker detected: {marker}")

        if monitor_path:
            monitor = "\n".join(_lines(monitor_path))
            monitor_markers = ("[ERROR]", "CPU was halted", "Unhandled exception")
            for marker in monitor_markers:
                if marker.lower() in monitor.lower():
                    raise AssertionError(f"Renode fatal marker detected: {marker}")

    def validate_capture(self, capture_path: str, monitor_path: str = "") -> None:
        self.validate_zephyr_boot(capture_path)
        self.validate_frame_count(capture_path)
        self.validate_frame_encoding_and_headers(capture_path)
        self.validate_indices_sequences_and_crc(capture_path)
        self.validate_firmware_summary(capture_path)
        self.validate_firmware_done(capture_path)
        self.validate_fault_free(capture_path, monitor_path)


_ROBOT_VALIDATOR = TelemetryValidation()


def validate_zephyr_boot(capture_path: str) -> None:
    _ROBOT_VALIDATOR.validate_zephyr_boot(capture_path)


def validate_frame_count(capture_path: str) -> None:
    _ROBOT_VALIDATOR.validate_frame_count(capture_path)


def validate_frame_encoding_and_headers(capture_path: str) -> None:
    _ROBOT_VALIDATOR.validate_frame_encoding_and_headers(capture_path)


def validate_indices_sequences_and_crc(capture_path: str) -> None:
    _ROBOT_VALIDATOR.validate_indices_sequences_and_crc(capture_path)


def validate_firmware_summary(capture_path: str) -> None:
    _ROBOT_VALIDATOR.validate_firmware_summary(capture_path)


def validate_firmware_done(capture_path: str) -> None:
    _ROBOT_VALIDATOR.validate_firmware_done(capture_path)


def validate_fault_free(capture_path: str, monitor_path: str = "") -> None:
    _ROBOT_VALIDATOR.validate_fault_free(capture_path, monitor_path)


def _main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="USART2 capture file")
    parser.add_argument("--monitor-log", default="", help="optional Renode monitor log")
    args = parser.parse_args()

    validator = TelemetryValidation()
    try:
        validator.validate_capture(args.capture, args.monitor_log)
    except (AssertionError, OSError, ValueError) as error:
        print(f"Telemetry validation failed: {error}", file=sys.stderr)
        return 1

    print("TLFRAME count: 8/8 PASS")
    print("CRC validation: 8/8 PASS")
    print("Firmware summary: PASS")
    print("Firmware done: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(_main())
