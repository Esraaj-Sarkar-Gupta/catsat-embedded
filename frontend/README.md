# Python ground station

The Python host opens the dashboard in `UI/index.html` and connects its serial
console to the ground-station ESP32 with `pyserial`.

```sh
python -m venv .venv
source .venv/bin/activate  # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python app.py
```

Connect the ESP32 over USB, choose its serial port in the dashboard, and press
**Connect ESP32 (USB)**. The firmware uses 115200 baud. On Linux, pywebview
requires a supported WebKit or Qt webview runtime.

The Python-to-firmware serial command format is `<recipient>,<command>` followed
by a newline. Recipient `1` is the PocketQube and `2` is the CanSat; for example,
`1,ping`. The firmware converts this to an ESP-NOW command packet and sends it
to the unicast peer learned from that author's telemetry. Firmware sends
telemetry to the UI as `key:value` comma-separated samples. Reply, error, TX, and
station status lines are prefixed with `REPLY:`, `ERROR:`, `TX:`, and `INFO:` and
appear directly in the console.

The ESP-NOW payload definitions in `ground-station/main/espnow.h` must remain
layout-compatible with `pocketQube/main/espnow.h`.
