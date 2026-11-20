"""PocketQube ground station UI: pywebview dashboard backed by pyserial."""

from __future__ import annotations

import json
import threading
import time
from pathlib import Path

import serial
from serial.tools import list_ports
import webview


BAUD_RATE = 115200
UI_FILE = Path(__file__).resolve().parents[1] / "UI" / "index.html"


class GroundStation:
    def __init__(self) -> None:
        self.window = None
        self.port: serial.Serial | None = None
        self.reader_thread: threading.Thread | None = None
        self.stop_reader = threading.Event()
        self.lock = threading.Lock()

    def bind_window(self, window) -> None:
        self.window = window

    def list_ports(self) -> dict:
        return {
            "ports": [
                {"device": item.device, "description": item.description or ""}
                for item in list_ports.comports()
            ]
        }

    def connect_serial(self, device: str) -> dict:
        self.disconnect_serial()
        try:
            opened = serial.Serial(device, BAUD_RATE, timeout=0.2, write_timeout=1)
            opened.reset_input_buffer()
            with self.lock:
                self.port = opened
            self.stop_reader.clear()
            self.reader_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.reader_thread.start()
            return {"ok": True}
        except (serial.SerialException, OSError) as exc:
            return {"ok": False, "error": str(exc)}

    def disconnect_serial(self) -> dict:
        self.stop_reader.set()
        with self.lock:
            port, self.port = self.port, None
        if port is not None:
            try:
                port.close()
            except (serial.SerialException, OSError):
                pass
        return {"ok": True}

    def send_command(self, recipient: str, command: str) -> dict:
        command = command.strip()
        if recipient not in {"1", "2"}:
            return {"ok": False, "error": "Choose PocketQube (1) or CanSat (2)."}
        if not command:
            return {"ok": False, "error": "Command is empty."}
        if "\n" in command or "\r" in command:
            return {"ok": False, "error": "Send one command at a time."}
        data = f"{recipient},{command}\n".encode("utf-8")
        with self.lock:
            port = self.port
            if port is None or not port.is_open:
                return {"ok": False, "error": "ESP32 is not connected."}
            try:
                port.write(data)
                port.flush()
            except (serial.SerialException, OSError) as exc:
                return {"ok": False, "error": f"Serial write failed: {exc}"}
        return {"ok": True}

    def _read_loop(self) -> None:
        while not self.stop_reader.is_set():
            with self.lock:
                port = self.port
            if port is None:
                return
            try:
                raw = port.readline()
            except (serial.SerialException, OSError) as exc:
                self._deliver(f"ERROR: Serial read failed: {exc}")
                self.disconnect_serial()
                return
            if raw:
                self._deliver(raw.decode("utf-8", errors="replace").strip())

    def _deliver(self, line: str) -> None:
        if self.window is None or not line:
            return
        try:
            script = f"window.onGroundStationLine({json.dumps(line)});"
            self.window.evaluate_js(script)
        except Exception:
            # The page may still be loading or the window may be closing.
            time.sleep(0.05)


def main() -> None:
    api = GroundStation()
    window = webview.create_window(
        "PocketQube Ground Station",
        UI_FILE.as_uri(),
        js_api=api,
        width=1500,
        height=1000,
        min_size=(900, 650),
    )
    api.bind_window(window)
    try:
        webview.start(debug=False)
    finally:
        api.disconnect_serial()


if __name__ == "__main__":
    main()
