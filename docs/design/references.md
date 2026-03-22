# SkullGate — Research References

A curated list of open-source projects, libraries, and resources referenced during SkullGate design.
For a detailed analysis of what they share and how SkullGate adopts from each, see
[inspiration-analysis.md](inspiration-analysis.md).

---

## Reference Architectures

| Project | URL | License | Role in SkullGate |
|---------|-----|---------|-------------------|
| **ESP32 Marauder** | https://github.com/justcallmekoko/ESP32Marauder | GPL-3.0 | Closest ancestor — LVGL UI, Wi-Fi scanning, SD logging patterns |
| **Zephyr RTOS** | https://github.com/zephyrproject-rtos/zephyr | Apache-2.0 | Board-profile model (device-tree → SkullGate JSON) |
| **MicroEJ VEE** | https://www.microej.com/ | Proprietary | Module sandboxing concepts |

---

## Display & UI

| Project | URL | License | Integration |
|---------|-----|---------|------------|
| **LovyanGFX** | https://github.com/lovyan03/LovyanGFX | FreeBSD | `DisplayAdapter.cpp` — primary TFT backend |
| **LVGL** | https://lvgl.io/ | MIT | `UiManager.cpp` — all screens, widgets, animations |
| **TFT_eSPI** | https://github.com/Bodmer/TFT_eSPI | — | Fallback display driver (stub in `DisplayAdapter`) |

---

## Wi-Fi & Networking

| Project | URL | License | Integration |
|---------|-----|---------|------------|
| **ESP-IDF Wi-Fi** | https://github.com/espressif/esp-idf | Apache-2.0 | `WifiManager.cpp` — `esp_wifi_scan_start()` passive scan |
| **WireGuard-ESP32-Arduino** | https://github.com/ciniml/WireGuard-ESP32-Arduino | BSD-3 | Phase 2 VPN module; curve25519 key exchange for esp-now mesh auth |
| **esp32_https_server** | https://github.com/fhessel/esp32_https_server | MIT | Phase 2 local proxy / management web UI |

---

## Radio Modules

| Project | URL | License | Integration |
|---------|-----|---------|------------|
| **RadioLib** | https://github.com/jgromes/RadioLib | MIT | Phase 2 `IRadioModule` API — LoRa SX127x, CC1101, NRF24 |
| **Adafruit PN532** | https://github.com/adafruit/Adafruit-PN532 | BSD | Phase 2 NFC/RFID module |

---

## Mesh Networking

| Project | URL | License | Integration |
|---------|-----|---------|------------|
| **espressif/esp-now** | https://github.com/espressif/esp-now | Apache-2.0 | Phase 2 `mesh_chat` module base — MAC-addressed, encrypted, no AP |
| **ESPNowMesh** | https://github.com/hsbharath22/ESPNowMesh | MIT | Multi-hop routing layer over esp-now |

---

## Web Flasher

| Project | URL | License | Integration |
|---------|-----|---------|------------|
| **esptool-js** | https://github.com/espressif/esptool-js | Apache-2.0 | `tools/web_flasher/app.js` — CDN integration point documented in code |
| **esp-web-tools** | https://github.com/esphome/esp-web-tools | Apache-2.0 | WebSerial/WebUSB transport patterns |

---

## JSON & Storage

| Project | URL | License | Integration |
|---------|-----|---------|------------|
| **ArduinoJson** | https://arduinojson.org/ | MIT | `BoardProfile.cpp`, `WifiScannerModule.cpp` — all JSON parsing |
| **arduino-esp32 SD** | https://github.com/espressif/arduino-esp32 | Apache-2.0 | `SdManager.cpp` — SD card driver |

---

## What All These Repos Have In Common

See the full analysis in [inspiration-analysis.md](inspiration-analysis.md). Short version:

1. **Driver-per-subsystem HAL** — one class, one job (display, touch, Wi-Fi, SD)
2. **Factory/registration pattern** for pluggable modules
3. **Arduino `String` + `std::vector`**, no exceptions, no smart pointers
4. **Passive-by-default** behaviour (scan before you transmit)
5. **JSON or compile-time configuration** for hardware differences
6. **`setup()` / `loop()` cooperative scheduling** — no FreeRTOS tasks in the core path
