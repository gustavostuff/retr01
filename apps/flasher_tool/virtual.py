"""In-memory chips. A burn writes these instead of a USB programmer."""

from __future__ import annotations

from pathlib import Path

from catalog import Target
from probe import BEAM_X_MOD, beam_x_hsync, check_beam_x_count

PROM_SIZE = 32 * 1024
BLANK = 0xFF


class VirtualProm:
    """OTP image. A second write is refused while any byte is programmed."""

    def __init__(self, size: int = PROM_SIZE) -> None:
        self.data = bytearray([BLANK] * size)
        self.writes = 0

    def is_blank(self) -> bool:
        return all(byte == BLANK for byte in self.data)

    def program(self, payload: bytes) -> None:
        if not self.is_blank():
            raise RuntimeError("chip is not blank")
        if len(payload) > len(self.data):
            raise RuntimeError("image is larger than the chip")
        self.data[: len(payload)] = payload
        self.writes += 1

    def matches(self, payload: bytes) -> bool:
        if self.data[: len(payload)] != payload:
            return False
        return all(byte == BLANK for byte in self.data[len(payload) :])


class VirtualPld:
    def __init__(self) -> None:
        self.image: bytes | None = None
        self.writes = 0

    def program(self, payload: bytes) -> None:
        self.image = bytes(payload)
        self.writes += 1

    def matches(self, payload: bytes) -> bool:
        return self.image == payload


class VirtualAvr:
    def __init__(self) -> None:
        self.image: bytes | None = None
        self.port: str | None = None
        self.writes = 0

    def program(self, payload: bytes, port: str) -> None:
        self.image = bytes(payload)
        self.port = port
        self.writes += 1

    def matches(self, payload: bytes) -> bool:
        return self.image == payload


class VirtualBeamPins:
    """Beam X counter. stuck_hsync forces the pin when set."""

    def __init__(self, stuck_hsync: bool | None = None) -> None:
        self.count = 0
        self.res_is_high = True
        self.edges = 0
        self.stuck_hsync = stuck_hsync

    def clock_low(self) -> None:
        return None

    def res_low(self) -> None:
        self.res_is_high = False
        self.count = 0

    def res_high(self) -> None:
        self.res_is_high = True

    def rising_edge(self) -> None:
        self.edges += 1
        if self.res_is_high:
            self.count = (self.count + 1) % BEAM_X_MOD

    def hsync(self) -> bool:
        if self.stuck_hsync is not None:
            return self.stuck_hsync
        return self.res_is_high and beam_x_hsync(self.count)


class VirtualBench:
    """Stand-in for the T48 and the CH340 lead."""

    def __init__(self) -> None:
        self.prom = VirtualProm()
        self.plds: dict[str, VirtualPld] = {}
        self.avr = VirtualAvr()
        self.minipro_installed = True
        self.programmer_connected = True
        self.ports = ["/dev/ttyUSB0"]
        self.fail_verify = False
        self.stuck_hsync: bool | None = None
        self.count_ports: list[str] = []

    def zif_status(self) -> str:
        if not self.minipro_installed:
            return "minipro missing"
        if not self.programmer_connected:
            return "no programmer"
        return "ready"

    def updi_ports(self) -> list[str]:
        return list(self.ports)

    def blank_prom(self, _target: Target) -> str | None:
        if self.prom.is_blank():
            return None
        return "chip is not blank"

    def program(self, target: Target, path: Path, port: str | None = None) -> str | None:
        payload = path.read_bytes()
        if target.kind == "prom":
            self.prom.program(payload)
            ok = self.prom.matches(payload)
        elif target.kind == "pld":
            chip = self.plds.setdefault(target.name, VirtualPld())
            chip.program(payload)
            ok = chip.matches(payload)
        elif target.kind == "avr":
            if not port:
                raise RuntimeError("AVR burn needs a serial port")
            self.avr.program(payload, port)
            ok = self.avr.matches(payload)
        else:
            raise RuntimeError(f"unknown kind {target.kind}")
        if self.fail_verify or not ok:
            return "verify failed\nchip does not match the image"
        return None

    def verify(self, target: Target, path: Path, port: str | None = None) -> str | None:
        payload = path.read_bytes()
        if target.kind == "prom":
            ok = self.prom.matches(payload)
        elif target.kind == "pld":
            chip = self.plds.get(target.name)
            ok = chip is not None and chip.matches(payload)
        elif target.kind == "avr":
            if not port:
                raise RuntimeError("AVR verify needs a serial port")
            ok = self.avr.port == port and self.avr.matches(payload)
        else:
            raise RuntimeError(f"unknown kind {target.kind}")
        if self.fail_verify or not ok:
            return "mismatch\nchip does not match the image"
        return None

    def count_beam_x(self, port: str) -> str | None:
        self.count_ports.append(port)
        return check_beam_x_count(VirtualBeamPins(self.stuck_hsync))
