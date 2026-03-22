# SkullGate — Architecture Design

## Overview

SkullGate is structured as a layered firmware OS:

```
┌──────────────────────────────────────────────────────────┐
│                     User-facing Modules                   │
│  wifi_scanner │ board_test │ (future: ble_scanner, etc.)  │
├──────────────────────────────────────────────────────────┤
│                       Module API (CoreAPI)                │
│         Permission-gated access to all subsystems         │
├────────────┬───────────────────┬────────────────────────-┤
│ UiManager  │   WifiManager     │       SdManager          │
│  (LVGL)    │ (passive scan)    │  (file read/write)       │
├────────────┴───────────────────┴─────────────────────────┤
│                    DriverRegistry                         │
│   Display │ Touch │ SD │ WiFi │ Bus (SPI/I2C arbitration) │
├──────────────────────────────────────────────────────────┤
│                      BoardProfile                         │
│         JSON-based pin map and capability registry        │
└──────────────────────────────────────────────────────────┘
```

## Key Design Decisions

### 1. Board-Agnostic via JSON Profiles
All hardware-specific values (pin numbers, SPI host, driver names) live in
`firmware/boards/<BOARD_ID>.json`. The firmware never hardcodes pins.

### 2. Permission-First Module API
Modules access ALL hardware through `CoreAPI`. CoreAPI checks permissions
before every operation. A module that lacks `wifi_scan` cannot scan Wi-Fi.

### 3. Recon-Only Default
On boot, only passive operations are permitted:
- Passive Wi-Fi scan (beacons only, no probe requests)
- Passive BLE scan
- SD card read/write

Active operations (transmit, connect, proxy) require Lab Mode.

### 4. Lab Mode Gating
Lab Mode cannot be activated without:
1. Physical SD card with `/lab_mode.flag`
2. Correct PIN at runtime

This prevents accidental activation and requires physical access.

### 5. Module Self-Registration
Modules use `REGISTER_MODULE(id, factory)` macro to register at
static-init time. No dynamic library loading needed on ESP32.

## Phase 2 Plans

- BLE scanner + GATT explorer
- LoRa SX127x driver
- CC1101 sub-GHz driver
- PN532 NFC driver
- ESP-NOW mesh chat
- PCAP-lite logger
- WireGuard VPN client
- OTA firmware update
- GPS wardriving integration
