"""Slow clock on a CH340 TXD pin, and one watched level on another module's RXD."""

from __future__ import annotations

import time
from pathlib import Path


class ModelPins:
    """In-memory counter. `level_at` reports the watched bit after `edges` clocks."""

    def __init__(self, level_at) -> None:
        self.edges = 0
        self.level_at = level_at

    def advance(self, count: int) -> None:
        self.edges += count

    def is_high(self) -> bool:
        return self.level_at(self.edges)


class SerialPins:
    """TXD break is the clock. An empty read on the watch port is a high pin."""

    def __init__(self, clock_port, watch_port, delay: float = 0.001) -> None:
        self.clock_port = clock_port
        self.watch_port = watch_port
        self.delay = delay
        self.clock_port.break_condition = True

    def advance(self, count: int) -> None:
        for _ in range(count):
            self.clock_port.break_condition = False
            time.sleep(self.delay)
            self.clock_port.break_condition = True
            time.sleep(self.delay)

    def is_high(self) -> bool:
        self.watch_port.reset_input_buffer()
        time.sleep(0.02)
        return self.watch_port.read(1) == b""


class BitLog:
    """One text log, replaced from the first byte on every open."""

    def __init__(self, path: Path, title: str) -> None:
        self._file = path.open("w", encoding="utf-8")
        self._file.write(f"{title}\n")
        self._file.flush()

    def reset(self, name: str, pin: int) -> None:
        self._file.write(f"reset\n{name} pin {pin}\n")
        self._file.flush()

    def sample(self, where: str, got: bool, want: bool) -> None:
        line = f"{where} {int(got)}"
        if got != want:
            line += f" want {int(want)}"
        self._file.write(line + "\n")
        self._file.flush()

    def note(self, text: str) -> None:
        self._file.write(text.rstrip("\n") + "\n")
        self._file.flush()

    def close(self) -> None:
        self._file.close()


def walk(pins, stops: tuple[int, ...], level_at, label: str, on_sample=None) -> str | None:
    """Clock from zero through each stop. None when every sample matches."""
    edges = 0
    for stop in stops:
        pins.advance(stop - edges)
        edges = stop
        got = pins.is_high()
        want = level_at(edges)
        if on_sample is not None:
            on_sample(edges, got, want)
        if got != want:
            state = "high" if got else "low"
            need = "high" if want else "low"
            return f"count failed\n{label} {state} at count {edges}, want {need}"
    return None


def record_bits(log: BitLog, stops, bits, level_at, place, make_pins, before_bit=None) -> str | None:
    """Walk each pin from a cleared counter. None when every sample matches."""
    for name, pin, bit in bits:
        if before_bit is not None:
            before_bit(name, pin)
        log.reset(name, pin)
        pins = make_pins(bit)
        reason = walk(
            pins,
            stops,
            lambda edges, bit=bit: level_at(edges, bit),
            name,
            on_sample=lambda edges, got, want, name=name: log.sample(place(edges, name), got, want),
        )
        if reason:
            return reason
    return None


def check_serial(clock_name: str, watch_name: str, stops, bits, level_at, prepare, log: BitLog, place) -> str:
    """Open two CH340 ports, reset between bits, and walk the stops."""
    try:
        import serial
    except ImportError:
        return "count failed\npyserial missing"
    try:
        clock = serial.Serial(clock_name, 9600, timeout=0.05)
        watch = serial.Serial(watch_name, 9600, timeout=0.05)
    except serial.SerialException as exc:
        return f"count failed\n{exc}"
    try:
        pins = SerialPins(clock, watch)
        reason = record_bits(
            log,
            stops,
            bits,
            level_at,
            place,
            lambda _bit: pins,
            before_bit=lambda name, pin: prepare(name, pin),
        )
        if reason:
            return reason
        return "count ok"
    finally:
        clock.break_condition = True
        clock.close()
        watch.close()
