# SkullGate — Inspiration Analysis

> **Question**: What did the referenced GitHub repos have in common, code-wise and feature-wise?
>
> This document answers that question and maps the findings to specific SkullGate design decisions.

---

## Repos Analysed

| Repo | URL | Why Referenced |
|------|-----|----------------|
| **ESP32 Marauder** | https://github.com/justcallmekoko/ESP32Marauder | Wi-Fi/BLE recon suite; closest spiritual ancestor |
| **WireGuard-ESP32-Arduino** | https://github.com/ciniml/WireGuard-ESP32-Arduino | VPN tunnel on ESP32; crypto/key-exchange patterns |
| **RadioLib** | https://github.com/jgromes/RadioLib | Unified radio driver API (LoRa, CC1101, NRF24, etc.) |
| **esptool-js** | https://github.com/espressif/esptool-js | WebSerial-based browser flasher |
| **esp-web-tools** | https://github.com/esphome/esp-web-tools | Browser-based firmware deployment |
| **esp-now** | https://github.com/espressif/esp-now | Low-latency peer-to-peer mesh protocol |
| **Zephyr RTOS** | https://github.com/zephyrproject-rtos/zephyr | Board profile / device-tree model |

---

## What They Share: Code Patterns

### 1. Modular Plugin Architecture

Every repo breaks functionality into pluggable units that register themselves:

| Repo | How Modules Register |
|------|----------------------|
| ESP32 Marauder | Static mode objects; menu items selected at runtime |
| RadioLib | Virtual base class; each radio chipset is a subclass |
| esp-now | ESP-IDF component model; stack layers are optional |
| WireGuard-ESP32 | Single optional module bolted onto `lwIP` stack |
| **SkullGate** | `REGISTER_MODULE(id, factory)` macro at static-init time |

SkullGate takes this furthest: modules also carry a `manifest.json` that declares exactly what they're allowed to do. No other project in this list has a permission layer at the module-registration level.

---

### 2. Driver Abstraction (HAL Pattern)

All of them wrap low-level hardware behind an adapter/manager class:

```
Physical hardware (ESP32 pins, SPI registers)
         ↓
  Driver Adapter  (abstracts the chip)
         ↓
  Manager / Registry  (centralises ownership)
         ↓
  Module-facing API  (safe, gated access)
```

| Repo | HAL Style |
|------|-----------|
| ESP32 Marauder | `Display.cpp`, `WiFiScan.cpp`, `SDInterface.cpp` — each file wraps one subsystem |
| RadioLib | `Module` base class → concrete driver subclasses (`SX127x`, `CC1101`, etc.) |
| WireGuard-ESP32 | Tunnel driver wraps UDP socket; key exchange is isolated |
| **SkullGate** | `DisplayAdapter`, `TouchAdapter`, `WifiManager`, `SdManager`, `BusManager`; all instantiated via `DriverRegistry` from a JSON board profile |

**SkullGate's addition**: No driver is ever instantiated by a module directly. All access is through `CoreAPI`, which performs a permission check first.

---

### 3. Board-Level Configuration

| Repo | How Board Differences Are Handled |
|------|-----------------------------------|
| ESP32 Marauder | `#ifdef` per board (`MARAUDER_MINI`, `MARAUDER_V4`, etc.) |
| RadioLib | Constructor arguments |
| WireGuard-ESP32 | Compile-time config in `WireGuardConfig.h` |
| esp-now | ESP-IDF `Kconfig` / `sdkconfig` |
| **SkullGate** | `firmware/boards/2432S022.json` — data file, no recompile needed |

SkullGate is the only project in this group that makes board configuration **data** (JSON), not code. Swap the file on the SD card, reboot — firmware adapts.

---

### 4. C++ Style & Memory Management

All projects share these conventions:

- **No exceptions** — all errors returned as `bool` or `nullptr`
- **Arduino `String` class** for convenience (PSRAM-extended on S3)
- **`std::vector`** for dynamic lists (AP lists, module lists, peer lists)
- **Manual lifecycle** — `init()` / `start()` / `loop()` / `stop()` instead of RAII
- **No `std::shared_ptr`** — too much overhead on 520 KB heap
- **Static/singleton pattern** for drivers (`Display.cpp` in Marauder; `WifiManager` in SkullGate)

**SkullGate follows all of these exactly.**

---

### 5. Factory / Registration Pattern

ESP32 Marauder, RadioLib, and SkullGate all use factory creation so the core never depends on concrete types:

```cpp
// Marauder (simplified)
MenuFunction* createWifiScanMode() { return new WiFiScan(); }

// RadioLib
PhysicalLayer* radio = new SX1278(spi_module);

// SkullGate
REGISTER_MODULE("wifi_scanner", []() -> IModule* {
    return new WifiScannerModule();
});
```

SkullGate extends this with **manifest validation** before the factory is ever called.

---

## What They Share: Features

### Core Feature Matrix

| Feature | Marauder | WireGuard | RadioLib | esptool-js | esp-web-tools | esp-now | SkullGate |
|---------|:--------:|:---------:|:--------:|:----------:|:-------------:|:-------:|:---------:|
| Wi-Fi passive scan | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Wi-Fi active frames | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | Lab Mode |
| BLE scanning | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | Phase 2 |
| LVGL/touchscreen UI | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| SD card logging | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Multi-radio (LoRa, CC1101) | partial | ❌ | ✅ | ❌ | ❌ | ❌ | Phase 2 |
| Mesh networking | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | Phase 2 |
| VPN tunnel | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | Phase 2 |
| Browser flasher | ❌ | ❌ | ❌ | ✅ | ✅ | ❌ | ✅ |
| JSON configuration | ❌ | ❌ | partial | ❌ | ❌ | partial | ✅ |
| Module permissions | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ (unique) |

---

### Specific Shared Sub-Features

#### Wi-Fi Scanning (Marauder → SkullGate)
Both use the ESP32's `esp_wifi_scan_start()` / scan callback.  
Key difference: Marauder can send deauth frames; SkullGate gates all TX behind a `wifi_tx` Lab Mode permission.

#### LVGL UI (Marauder → SkullGate)
Both use LVGL with a dark colour scheme for the ESP32's TFT displays.  
Key difference: Marauder's screens are hardcoded; SkullGate uses a named-screen registry so any module can register its own screen via `CoreAPI::registerScreen()`.

#### SD JSON Logging (Marauder → SkullGate)
Both dump scan results as JSON to SD.  
Key difference: SkullGate's `SdManager` is permission-gated; modules must declare `sd_write` in manifest.

#### Browser-Based Flasher (esptool-js + esp-web-tools → SkullGate)
Both esptool-js and esp-web-tools use the **Web Serial API** in Chrome/Edge.  
SkullGate's `tools/web_flasher/app.js` follows the same pattern:
- `navigator.serial.requestPort()` to get user consent
- DTR/RTS toggle sequence to enter ROM bootloader
- SLIP-framed commands to write flash blocks
- Progress bar + log output during flash

---

## What They Share: Design Philosophy

### A. "Safe-by-Default, Unlock-to-Hack"

| Project | Default state | How to unlock |
|---------|--------------|---------------|
| Marauder | Passive scanner | Select attack mode in menu |
| WireGuard-ESP32 | Tunnel inactive | Call `wg.begin()` in code |
| SkullGate | Recon-Only | SD flag file + PIN = Lab Mode |

All three start in a non-transmitting state. SkullGate makes this the **hardest to unlock accidentally** (requires physical SD card + known PIN).

---

### B. Single Responsibility for Drivers

Each file/class does one thing. From Marauder:

```
Display.cpp     → only LCD operations
WiFiScan.cpp    → only Wi-Fi frame handling
SDInterface.cpp → only file I/O
```

SkullGate mirrors this exactly:

```
DisplayAdapter.cpp → only LCD
TouchAdapter.cpp   → only touch
WifiManager.cpp    → only Wi-Fi
SdManager.cpp      → only files
BusManager.cpp     → only SPI/I2C arbitration
```

---

### C. Separation of Protocol from Transport

RadioLib abstracts radio protocol (LoRa framing, CRC, FEC) away from the physical SPI writes.  
WireGuard-ESP32 abstracts crypto/key-exchange away from the UDP socket.  
SkullGate separates permission checking (CoreAPI) from the actual driver call (WifiManager, SdManager).

In all three, the *mechanism* is in the driver layer; the *policy* is in the manager layer above it.

---

### D. Non-OS / Arduino-Compatible Architecture

All projects target the **Arduino framework on ESP32** (or are ESP-IDF with Arduino compat layer).  
None use FreeRTOS tasks for the core loop; all use `setup()` / `loop()` with cooperative yielding (`delay(1)` or `lv_timer_handler()`).

---

## What SkullGate Takes From Each Repo

### From ESP32 Marauder

| Adopted | Planned |
|---------|---------|
| ✅ LVGL UI with dark theme | 🔄 PCAP-lite frame logging |
| ✅ Wi-Fi passive scan → AP list | 🔄 BLE scan + GATT explorer |
| ✅ SD JSON logging | 🔄 IR blaster module |
| ✅ Module-per-feature decomposition | 🔄 Battery/power widget |

**Key difference from SkullGate**: Marauder lacks a permission layer — a compiled-in mode can always run active attacks. SkullGate prevents this at the CoreAPI level.

---

### From WireGuard-ESP32-Arduino

| Planned |
|---------|
| 🔄 Curve25519 key exchange for esp-now mesh peer authentication |
| 🔄 NVS storage for sensitive keys (instead of plaintext JSON on SD) |
| 🔄 WireGuard VPN module (Phase 2) using the library directly |

**Design pattern to copy**: WireGuard's tunnel abstraction — `Tunnel::encrypt(packet)` / `Tunnel::decrypt(packet)` — is the right model for SkullGate's mesh protocol.

---

### From RadioLib

| Planned |
|---------|
| 🔄 `IRadioModule` base class so LoRa/CC1101/NRF24 share one API |
| 🔄 Interrupt-driven RX (non-blocking; current module loop uses polling) |
| 🔄 Fine-grained radio tuning exposed as module UI sliders |

**Recommended interface to adopt**:
```cpp
class IRadioModule : public IModule {
public:
    virtual bool begin(uint32_t freqHz)                              = 0;
    virtual bool transmit(const uint8_t* data, size_t len)          = 0;
    virtual bool receive(uint8_t* buf, size_t& len, uint32_t tmOut) = 0;
    virtual void setInterruptHandler(void (*cb)())                   = 0;
};
```

---

### From esptool-js + esp-web-tools

| Already Adopted | Planned |
|-----------------|---------|
| ✅ WebSerial port selection via `navigator.serial.requestPort()` | 🔄 Full esptool-js library integration (replace stub) |
| ✅ DTR/RTS bootloader entry sequence | 🔄 WebUSB transport as fallback |
| ✅ Async progress bar + log output | 🔄 Partition table display before flashing |

**Integration path**: Import `esptool-js` via CDN in `tools/web_flasher/index.html` and replace the simulated flash loop in `app.js` with real `ESPLoader.write_flash()` calls.

---

### From esp-now (Espressif)

| Planned |
|---------|
| 🔄 `esp_now_mesh` module: MAC-addressed peer-to-peer, no AP needed |
| 🔄 Peer encryption via `esp_now_set_peer_encrypt()` before send |
| 🔄 Multi-hop routing via ESPNowMesh (in `modules/mesh_chat/`) |

**Key advantage over BLE/WiFi for mesh**: Works with WiFi radio off, sub-1ms latency, encrypted without TLS overhead.

---

### From Zephyr RTOS

| Adopted |
|---------|
| ✅ JSON board profile concept (SkullGate's `firmware/boards/*.json`) |
| ✅ Capability query API (`board.has("display")`) |

**Design difference**: Zephyr uses compiled `.dts` (device tree); SkullGate uses runtime-parsed JSON. Easier to change without recompiling, which is the right trade-off for a field-deployable device.

---

## Summary Table

| Aspect | How All Repos Converge | SkullGate's Addition |
|--------|------------------------|----------------------|
| **Driver model** | Adapter class per subsystem | `DriverRegistry` + JSON board profiles |
| **Module system** | Factory/registration pattern | `manifest.json` + permission gate |
| **Safety model** | "Passive first" by convention | Enforced at CoreAPI; Lab Mode requires physical SD |
| **C++ style** | No exceptions, no smart ptrs, `Arduino String` | Same, plus full docs/comments |
| **Configuration** | Compile-time or runtime C++ args | Data-driven JSON everywhere |
| **Board support** | `#ifdef` per board | Swap a JSON file |
| **Wi-Fi** | ESP-IDF `esp_wifi_scan_start()` | Passive async scan; active TX gated |
| **LVGL UI** | Dark theme, modal screens | Named screen registry; any module registers its screen |
| **Logging** | SD + Serial | SD + Serial + in-memory ring buffer (UiManager log viewer) |
| **Web flasher** | WebSerial, SLIP, progress bar | Integrated in `tools/web_flasher/`; esptool-js hook documented |
