/**
 * @file BleManager.cpp
 * @brief BLE passive scanner — ESP32 Arduino BLE (Bluedroid) implementation.
 *
 * SAFETY:
 *   - setActiveScan(false) ensures NO scan request packets are transmitted.
 *   - BLEDevice::init("") initialises the stack with an empty device name;
 *     no advertisement is configured, so the device is not discoverable.
 *   - We never call BLEAdvertising::start() or BLEClient::connect().
 */

#include "BleManager.h"

// Guard: only compile BLE code on platforms that have it.
// ESP32 Arduino SDK ships BLE on all standard variants; the flag lets
// stub-only builds opt out (e.g. unit-test hosts).
#ifndef SKULLGATE_NO_BLE

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

namespace skullgate {

// ── Static guard ──────────────────────────────────────────────────────────────
// BLEDevice::init() must only be called once per firmware run.
static bool s_bleGlobalInit = false;

// ── BleManager ────────────────────────────────────────────────────────────────

BleManager::BleManager() : _ready(false), _bleInited(false) {}

BleManager::~BleManager() {
    deinit();
}

bool BleManager::init() {
    if (!s_bleGlobalInit) {
        // Empty name = not advertising; BLE stack initialised for scanning only.
        BLEDevice::init("");
        s_bleGlobalInit = true;
    }
    _bleInited = true;
    _ready     = true;
    Serial.println("[BleManager] BLE ready (passive scan mode)");
    return true;
}

std::vector<BleDevice> BleManager::scan(uint32_t durationMs) {
    std::vector<BleDevice> results;
    if (!_ready) return results;

    // Clamp duration to safe range
    durationMs = max((uint32_t)500, min(durationMs, (uint32_t)10000));
    uint32_t durSecs = max(1u, durationMs / 1000u);

    BLEScan* scan = BLEDevice::getScan();
    // PASSIVE scan: do NOT send scan-request packets.
    scan->setActiveScan(false);
    // Scan interval and window (units = 0.625 ms; interval ≥ window).
    scan->setInterval(160); // 100 ms
    scan->setWindow(159);   // 99.375 ms

    Serial.printf("[BleManager] Passive BLE scan for %u s...\n", durSecs);
    BLEScanResults found = scan->start(durSecs, false /* blockUntilDone */);
    scan->stop();

    int count = found.getCount();
    results.reserve(count);

    for (int i = 0; i < count; i++) {
        BLEAdvertisedDevice dev = found.getDevice(i);

        BleDevice bd;
        bd.mac         = String(dev.getAddress().toString().c_str());
        bd.name        = dev.haveName() ? String(dev.getName().c_str()) : "";
        bd.rssi        = static_cast<int8_t>(dev.getRSSI());
        bd.connectable = dev.isAdvertisingService(BLEUUID((uint16_t)0x0000)); // simplified check
        bd.addrType    = static_cast<uint8_t>(dev.getAddressType());

        results.push_back(bd);
    }

    scan->clearResults();

    // Sort by RSSI (strongest first)
    std::sort(results.begin(), results.end(),
              [](const BleDevice& a, const BleDevice& b) {
                  return a.rssi > b.rssi;
              });

    Serial.printf("[BleManager] BLE scan complete: %d devices\n", (int)results.size());
    return results;
}

void BleManager::deinit() {
    if (_ready) {
        _ready     = false;
        _bleInited = false;
        // Note: BLEDevice::deinit() can be called but may destabilise the BT stack
        // if called while a scan is running. We leave the global init flag set so
        // re-init in the same firmware run works correctly.
    }
}

} // namespace skullgate

#else // SKULLGATE_NO_BLE stub

namespace skullgate {

BleManager::BleManager() : _ready(false), _bleInited(false) {}
BleManager::~BleManager() {}

bool BleManager::init() {
    Serial.println("[BleManager] BLE not compiled — stub active");
    return false;
}

std::vector<BleDevice> BleManager::scan(uint32_t) {
    return {};
}

void BleManager::deinit() {}

} // namespace skullgate

#endif // SKULLGATE_NO_BLE
