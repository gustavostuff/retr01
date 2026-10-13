"""Virtual chips stand in for the T48 and the CH340 lead."""

from __future__ import annotations

import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import burn
import catalog
import flasher
import probe
import verify
from catalog import TARGETS, Target
from virtual import VirtualBench


def _target(name: str) -> Target:
    for target in TARGETS:
        if target.name == name:
            return target
    raise KeyError(name)


class VirtualChipTests(unittest.TestCase):
    def test_check_reports_both_virtual_devices(self) -> None:
        text, ok = flasher.device_check_report(VirtualBench())
        self.assertTrue(ok)
        self.assertIn("XGecu T48 or TL866II Plus    ready", text)
        self.assertIn("HiLetgo CH340 SerialUPDI", text)
        self.assertIn("/dev/ttyUSB0", text)

    def test_check_fails_when_either_device_is_missing(self) -> None:
        no_zif = VirtualBench()
        no_zif.programmer_connected = False
        text, ok = flasher.device_check_report(no_zif)
        self.assertFalse(ok)
        self.assertIn("no programmer", text)

        no_serial = VirtualBench()
        no_serial.ports = []
        text, ok = flasher.device_check_report(no_serial)
        self.assertFalse(ok)
        self.assertIn("not found", text)
        self.assertIn("ready", text)

    def test_banner_names_both_programmers(self) -> None:
        text = flasher.format_screen(0, ["ready"] * len(TARGETS))
        self.assertIn("XGecu T48 or TL866II Plus", text)
        self.assertIn("HiLetgo CH340 SerialUPDI", text)
        self.assertLess(
            text.index("Programmers:"),
            text.index("Color PROM"),
        )

    def test_prom_programs_blank_chip(self) -> None:
        bench = VirtualBench()
        target = _target("Color PROM")
        message = burn.burn(target, bench, confirm=lambda: True)
        payload = catalog.image_path(target).read_bytes()
        self.assertEqual(message, "done")
        self.assertEqual(len(payload), 64)
        self.assertEqual(bench.prom.data[:64], payload)
        self.assertEqual(bench.prom.writes, 1)
        self.assertTrue(all(byte == 0xFF for byte in bench.prom.data[64:]))

    def test_prom_refuses_used_chip(self) -> None:
        bench = VirtualBench()
        bench.prom.data[0] = 0x00
        before = bytes(bench.prom.data)
        message = burn.burn(_target("Color PROM"), bench, confirm=lambda: True)
        self.assertEqual(message, "chip is not blank")
        self.assertEqual(bytes(bench.prom.data), before)
        self.assertEqual(bench.prom.writes, 0)

    def test_cancelled_prom_stays_blank(self) -> None:
        bench = VirtualBench()
        message = burn.burn(_target("Color PROM"), bench, confirm=lambda: False)
        self.assertEqual(message, "cancelled")
        self.assertTrue(bench.prom.is_blank())

    def test_pld_programs_jed(self) -> None:
        bench = VirtualBench()
        target = _target("Beam X")
        message = burn.burn(target, bench)
        payload = catalog.image_path(target).read_bytes()
        self.assertEqual(message, "done")
        self.assertEqual(bench.plds["Beam X"].image, payload)
        self.assertGreater(len(payload), 64)

    def test_missing_compositor_leaves_chip_empty(self) -> None:
        bench = VirtualBench()
        target = _target("Compositor")
        message = burn.burn(target, bench)
        self.assertIn("image missing", message)
        self.assertIn("compositor.jed", message)
        self.assertNotIn("Compositor", bench.plds)

    def test_missing_minipro_leaves_prom_blank(self) -> None:
        bench = VirtualBench()
        bench.minipro_installed = False
        message = burn.burn(_target("Color PROM"), bench, confirm=lambda: True)
        self.assertEqual(message, "minipro missing")
        self.assertTrue(bench.prom.is_blank())
        self.assertEqual(bench.prom.writes, 0)

    def test_avr_programs_hex_on_chosen_port(self) -> None:
        bench = VirtualBench()
        bench.ports = ["/dev/ttyUSB0", "/dev/ttyUSB1"]
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            image = repo / "mcu_s1.hex"
            image.write_bytes(b":00000001FF\n")
            target = Target(
                name="MCU-S1",
                chip="AVR128DB28",
                kind="avr",
                image="mcu_s1.hex",
                device="avr128db28",
            )
            chosen: list[list[str]] = []

            def choose(ports: list[str]) -> str:
                chosen.append(ports)
                return ports[1]

            message = burn.burn(target, bench, repo, choose_port=choose)
        self.assertEqual(message, "done")
        self.assertEqual(chosen, [["/dev/ttyUSB0", "/dev/ttyUSB1"]])
        self.assertEqual(bench.avr.port, "/dev/ttyUSB1")
        self.assertEqual(bench.avr.image, b":00000001FF\n")

    def test_verify_failure_is_not_done(self) -> None:
        bench = VirtualBench()
        bench.fail_verify = True
        message = burn.burn(_target("Beam Y"), bench)
        self.assertEqual(message, "verify failed")
        self.assertIsNotNone(bench.plds["Beam Y"].image)

    def test_menu_enter_programs_virtual_prom(self) -> None:
        bench = VirtualBench()
        stdin = io.StringIO("\r" "y\n" "q")
        out = io.StringIO()
        flasher.run_menu(bench, catalog.REPO, stdin, out)
        text = out.getvalue()
        self.assertIn("XGecu T48 or TL866II Plus", text)
        self.assertIn("HiLetgo CH340 SerialUPDI", text)
        self.assertIn("done", text)
        payload = catalog.image_path(_target("Color PROM")).read_bytes()
        self.assertEqual(bench.prom.data[:64], payload)


class VerifyTests(unittest.TestCase):
    def test_banner_names_both_programmers(self) -> None:
        text = flasher.format_screen(0, ["ready"] * len(TARGETS), banner=verify.BANNER)
        self.assertIn("Retr01 verify", text)
        self.assertIn("XGecu T48 or TL866II Plus", text)
        self.assertIn("HiLetgo CH340 SerialUPDI", text)
        self.assertIn("Enter verify", text)
        self.assertLess(text.index("Programmers:"), text.index("Color PROM"))

    def test_blank_prom_is_a_mismatch_and_stays_blank(self) -> None:
        bench = VirtualBench()
        message = verify.verify_chip(_target("Color PROM"), bench)
        self.assertEqual(message, "mismatch")
        self.assertTrue(bench.prom.is_blank())
        self.assertEqual(bench.prom.writes, 0)

    def test_programmed_prom_matches_without_another_write(self) -> None:
        bench = VirtualBench()
        target = _target("Color PROM")
        self.assertEqual(burn.burn(target, bench, confirm=lambda: True), "done")
        message = verify.verify_chip(target, bench)
        self.assertEqual(message, "match")
        self.assertEqual(bench.prom.writes, 1)

    def test_changed_prom_byte_is_a_mismatch(self) -> None:
        bench = VirtualBench()
        target = _target("Color PROM")
        burn.burn(target, bench, confirm=lambda: True)
        bench.prom.data[0] ^= 0xFF
        self.assertEqual(verify.verify_chip(target, bench), "mismatch")

    def test_programmed_pld_matches_and_empty_pld_does_not(self) -> None:
        bench = VirtualBench()
        target = _target("Beam X")
        burn.burn(target, bench)
        self.assertEqual(verify.verify_chip(target, bench), "match")
        self.assertEqual(verify.verify_chip(_target("Beam Y"), bench), "mismatch")
        self.assertNotIn("Beam Y", bench.plds)

    def test_missing_image_does_not_touch_a_chip(self) -> None:
        bench = VirtualBench()
        message = verify.verify_chip(_target("Compositor"), bench)
        self.assertIn("image missing", message)
        self.assertNotIn("Compositor", bench.plds)

    def test_missing_minipro_skips_the_read(self) -> None:
        bench = VirtualBench()
        bench.minipro_installed = False
        bench.prom.data[0] = 0x12
        message = verify.verify_chip(_target("Color PROM"), bench)
        self.assertEqual(message, "minipro missing")
        self.assertEqual(bench.prom.data[0], 0x12)

    def test_avr_matches_only_the_port_it_was_written_on(self) -> None:
        bench = VirtualBench()
        bench.ports = ["/dev/ttyUSB0", "/dev/ttyUSB1"]
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            (repo / "mcu_s1.hex").write_bytes(b":00000001FF\n")
            target = Target(
                name="MCU-S1",
                chip="AVR128DB28",
                kind="avr",
                image="mcu_s1.hex",
                device="avr128db28",
            )
            burn.burn(target, bench, repo, choose_port=lambda ports: ports[1])
            self.assertEqual(
                verify.verify_chip(target, bench, repo, choose_port=lambda ports: ports[1]),
                "match",
            )
            self.assertEqual(
                verify.verify_chip(target, bench, repo, choose_port=lambda ports: ports[0]),
                "mismatch",
            )
        self.assertEqual(bench.avr.writes, 1)

    def test_menu_enter_reports_a_match(self) -> None:
        bench = VirtualBench()
        target = _target("Color PROM")
        burn.burn(target, bench, confirm=lambda: True)
        out = io.StringIO()
        verify.run_menu(bench, catalog.REPO, io.StringIO("\rq"), out)
        self.assertIn("\nmatch\n", "\n" + out.getvalue())
        self.assertIn("Enter verify", out.getvalue())
        self.assertEqual(bench.prom.writes, 1)


class CommandTests(unittest.TestCase):
    def test_id_mismatch_is_not_called_a_programmed_chip(self) -> None:
        text = (
            "Warning: Firmware is out of date.\n"
            "Invalid Chip ID: expected 0x1E8C, got 0x7EFF (unknown)\n"
        )
        message = probe.blank_check_message(1, text)
        self.assertEqual(
            message,
            "Invalid Chip ID: expected 0x1E8C, got 0x7EFF (unknown)",
        )
        self.assertIsNone(probe.blank_check_message(0, "Chip is blank\n"))

    def test_prom_command_sets_vpp_and_skips_protect(self) -> None:
        target = _target("Color PROM")
        write = probe.minipro_write_argv(target, Path("kit.bin"))
        verify = probe.minipro_verify_argv(target, Path("kit.bin"))
        self.assertEqual(
            write,
            ["minipro", "-p", "AT27C256R@DIP28", "-w", "kit.bin", "-s", "-o", "vpp=12.5"],
        )
        self.assertNotIn("-P", write + verify)

    def test_pld_command_has_no_security_fuse(self) -> None:
        write = probe.minipro_write_argv(_target("Beam X"), Path("beam_x.jed"))
        self.assertEqual(write, ["minipro", "-p", "ATF22V10C", "-w", "beam_x.jed"])
        self.assertNotIn("-P", write)

    def test_verify_commands_do_not_write(self) -> None:
        prom = probe.minipro_verify_argv(_target("Color PROM"), Path("kit.bin"))
        self.assertEqual(
            prom,
            ["minipro", "-p", "AT27C256R@DIP28", "-m", "kit.bin", "-s"],
        )
        self.assertNotIn("-w", prom)
        avr = probe.pymcuprog_verify_argv(
            _target("MCU-S1"), Path("mcu.hex"), "/dev/ttyUSB0"
        )
        self.assertEqual(
            avr,
            [
                "pymcuprog",
                "verify",
                "-d",
                "avr128db28",
                "-t",
                "uart",
                "-u",
                "/dev/ttyUSB0",
                "-f",
                "mcu.hex",
            ],
        )

    def test_ch340_filter_keeps_vendor_1a86(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            sysfs = Path(tmp)
            self._tty(sysfs, "ttyUSB0", "1a86")
            self._tty(sysfs, "ttyUSB1", "0403")
            self._tty_via_symlink(sysfs, "ttyUSB2", "1a86")
            self.assertEqual(
                probe.ch340_ports(sysfs),
                ["/dev/ttyUSB0", "/dev/ttyUSB2"],
            )

    def _tty(self, sysfs: Path, name: str, vendor: str) -> None:
        device = sysfs / name / "device"
        device.mkdir(parents=True)
        (device / "idVendor").write_text(vendor + "\n", encoding="ascii")

    def _tty_via_symlink(self, sysfs: Path, name: str, vendor: str) -> None:
        real = sysfs / "usb" / name
        real.mkdir(parents=True)
        (real.parent / "idVendor").write_text(vendor + "\n", encoding="ascii")
        link_dir = sysfs / name
        link_dir.mkdir()
        (link_dir / "device").symlink_to(real)


if __name__ == "__main__":
    unittest.main()
