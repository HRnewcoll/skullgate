# SkullGate — Command the Dark

> Modular, board-agnostic ESP cyber-deck OS for handheld devices.  
> Built for **local lab testing**, **education**, and **modular expansion**.

---

## ⚠ Safety Notice

SkullGate is designed exclusively for:
- **Your own devices** and networks
- **Explicitly permitted lab environments**
- **Educational and research purposes**

Using SkullGate on networks or devices you do not own or have explicit written permission to test is **illegal** in most jurisdictions. The authors accept no liability for misuse.

**By using SkullGate you agree to comply with all applicable laws and regulations.**

---

## Features

| Feature | Status |
|---------|--------|
| Board-agnostic HAL (JSON profiles) | ✅ Phase 1 |
| Modular plugin system (SD-based) | ✅ Phase 1 |
| LovyanGFX + LVGL UI | ✅ Phase 1 |
| Wi-Fi passive scanner module | ✅ Phase 1 |
| Board self-test module | ✅ Phase 1 |
| Recon-Only mode (safe default) | ✅ Phase 1 |
| Lab Mode gating (SD flag + PIN) | ✅ Phase 1 |
| Web flasher (WebSerial) | ✅ Phase 1 |
| GitHub Actions CI | ✅ Phase 1 |
| BLE scanner | ✅ Phase 2 |
| LoRa / CC1101 radio modules | ✅ Phase 2 |
| Mesh chat (ESP-NOW) | ✅ Phase 2 |
| GPS / wardriving | 🔄 Phase 3 |
| WireGuard VPN | 🔄 Phase 3 |
| PCAP-lite logging | ✅ Phase 2 |
| OTA update | ✅ Phase 2 |

---

## Repository Structure

```
skullgate/
├── firmware/
│   ├── src/
│   │   └── main.cpp              ← Entry point (setup + loop)
│   ├── core/
│   │   ├── BoardProfile.h/.cpp   ← JSON board profile loader
│   │   ├── DriverRegistry.h/.cpp ← Hardware driver instantiation
│   │   ├── CoreAPI.h/.cpp        ← Module-facing API (permissions enforced)
│   │   ├── ModuleManager.h/.cpp  ← Module loader + lifecycle
│   │   ├── UiManager.h/.cpp      ← LVGL UI skeleton
│   │   └── lv_conf.h             ← LVGL configuration
│   ├── drivers/
│   │   ├── DisplayAdapter.h/.cpp ← LovyanGFX display driver
│   │   ├── TouchAdapter.h/.cpp   ← XPT2046 / GT911 touch driver
│   │   ├── SdManager.h/.cpp      ← SD card file operations
│   │   ├── WifiManager.h/.cpp    ← Wi-Fi scan + connect
│   │   └── BusManager.h/.cpp     ← SPI/I2C bus arbitration
│   ├── boards/
│   │   ├── 2432S022.json         ← CYD board profile
│   │   └── 2432S022_default.h    ← Compiled-in fallback profile
│   └── modules/
│       ├── wifi_scanner/
│       │   ├── manifest.json
│       │   ├── WifiScannerModule.h
│       │   └── WifiScannerModule.cpp
│       └── board_test/
│           ├── manifest.json
│           ├── BoardTestModule.h
│           └── BoardTestModule.cpp
├── tools/
│   └── web_flasher/
│       ├── index.html            ← Web flasher UI
│       ├── app.js                ← WebSerial flash logic
│       └── style.css             ← Dark cyber-deck styling
├── docs/
│   ├── design/                   ← Architecture documents
│   ├── branding/                 ← Logos and assets
│   └── safety/                   ← Safety policy and legal notes
├── .github/
│   └── workflows/
│       └── build.yml             ← PlatformIO CI build
├── platformio.ini                ← Build configuration
├── LICENSE                       ← MIT
└── README.md
```

---

## Quick Start

### Hardware Required
- Any ESP32 / ESP32-S3 / ESP32-C3 board
- Display with touch (tested: CYD 2432S022 — ST7789 + XPT2046)
- SD card (optional but recommended for module logging)

### Building with PlatformIO

```bash
# Clone
git clone https://github.com/HRnewcoll/skullgate.git
cd skullgate

# Build for ESP32 dev board
pio run -e esp32dev

# Build for ESP32-S3
pio run -e esp32-s3

# Flash
pio run -e esp32dev -t upload

# Monitor serial output
pio device monitor --baud 115200
```

### Web Flasher

Open `tools/web_flasher/index.html` in Chrome or Edge (89+):
1. Click **Connect to ESP32** and select the serial port
2. Select your `firmware.bin` file
3. Set the flash address (`0x10000` for application firmware)
4. Click **Flash Firmware**

---

## Board Profiles

Board hardware is described in JSON files under `firmware/boards/`.
The firmware loads the profile from SD at boot; the compiled-in default is used if no SD card is present.

Example profile (2432S022.json):
```json
{
  "board_id": "2432S022",
  "display": { "driver": "ST7789", "width": 240, "height": 320 },
  "touch":   { "driver": "XPT2046" },
  "sd":      { "bus": "SPI", "spi_host": "HSPI" },
  "capabilities": ["display", "touch", "sd", "wifi", "ble"]
}
```

---

## Module System

Modules are compiled into the firmware and registered at static-init time.
Each module declares a `manifest.json` with its ID, permissions, and metadata.

### Permission Model

| Permission | Description | Mode Required |
|------------|-------------|---------------|
| `wifi_scan` | Passive Wi-Fi scan | Recon-Only |
| `ble_scan` | Passive BLE scan | Recon-Only |
| `sd_read` | Read files from SD | Recon-Only |
| `sd_write` | Write files to SD | Recon-Only |
| `ui` | Register LVGL screen | Recon-Only |
| `serial` | Access Serial | Recon-Only |
| `wifi_connect` | Associate with AP | **Lab Mode** |
| `wifi_tx` | Transmit frames | **Lab Mode** |
| `ble_tx` | BLE advertising/connect | **Lab Mode** |
| `net_proxy` | Local proxy features | **Lab Mode** |

### Lab Mode

Lab Mode unlocks active features. It requires:
1. A file `/lab_mode.flag` present on the SD card
2. The correct PIN entered at the prompt

Without the SD flag, Lab Mode **cannot** be activated regardless of what PIN is entered.

---

## Architecture

```
┌─────────────────────────────────────────────┐
│               main.cpp                      │
│   setup() → load profile → init drivers     │
│   loop()  → ui.loop() → modules.loop()      │
└───────────────────┬─────────────────────────┘
                    │
      ┌─────────────▼──────────────┐
      │       ModuleManager        │
      │  permissions + lifecycle   │
      └─────────────┬──────────────┘
                    │ CoreAPI (permission-gated)
      ┌─────────────▼──────────────┐
      │         CoreAPIImpl        │
      └──┬──────────┬──────────┬───┘
         │          │          │
   WifiManager  SdManager  UiManager (LVGL+LovyanGFX)
                               │
                         DisplayAdapter + TouchAdapter
                               │
                         BusManager (SPI/I2C)
                               │
                         BoardProfile (JSON)
```

---

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Ensure your code follows the existing style (clean C++, heavy comments)
4. Verify the CI build passes
5. Submit a pull request

---

## License

MIT — see [LICENSE](LICENSE)

---

## Acknowledgements

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — display driver
- [LVGL](https://lvgl.io/) — UI framework
- [ArduinoJson](https://arduinojson.org/) — JSON parsing
- [espressif/esptool-js](https://github.com/espressif/esptool-js) — web flasher reference
# skullgate
SkullGate — Command the Dark. Modular, board‑agnostic ESP cyber‑deck OS. 
# SkullGate — Command the Dark

SkullGate is a modular, board‑agnostic ESP cyber‑deck OS designed for handheld devices.  
It is built for **local lab testing**, **education**, and **modular expansion** — not for misuse.

## Core Principles
- Modular plugin system (SD‑based modules)
- Board‑agnostic HAL (works on any ESP board)
- Safe defaults (Recon‑Only on boot)
- Lab Mode gating for active features
- Touchscreen UI + headless mode
- Radio expansion (LoRa, Sub‑GHz, NFC, IR)
- Local‑only proxy/VPN tools for your own devices
- Mesh chat, BLE tools, dashboards, games

## Status
Phase‑1 development: HAL, module loader, UI skeleton, Wi‑Fi scanner.

## Safety Notice
SkullGate is for **your own devices**, **your own networks**, and **explicitly permitted lab environments** only.

SkullGate: A Modular, Board-Agnostic ESP Cyber-Deck OS for Handheld Devices
Executive Summary

SkullGate aspires to be the definitive open-source, modular firmware platform for ESP32-based cyber-decks and handhelds, focusing on extensibility, board-agnostic design, and robust support for wireless reconnaissance, mesh networking, radio modules, and advanced UI. This report delivers a comprehensive, actionable blueprint for building SkullGate, synthesizing the best practices, code patterns, and hardware integration strategies from the current ESP32 ecosystem. It covers project-by-project analysis of relevant open-source repositories, hardware driver research, networking and security feasibility, modular firmware architecture, web flashing tooling, and a prioritized implementation roadmap. The recommendations emphasize stability, maintainability, and extensibility, targeting the "golden path" of the ESP32-S3 ecosystem. All findings are grounded in up-to-date, credible references and are tailored for firmware developers seeking to build or contribute to SkullGate.
