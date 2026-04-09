/**
 * @file main.cpp
 * @brief SkullGate firmware entry point.
 *
 * Boot sequence:
 *   1. Serial init
 *   2. Load board profile from SD (or fallback to compiled-in default)
 *   3. Init DriverRegistry (display, touch, SD, Wi-Fi, buses)
 *   4. Init UiManager (LVGL dashboard)
 *   5. Init ModuleManager (scan & load permitted modules)
 *   6. Enter main loop (LVGL task handler + active module loop)
 *
 * ── Safety ────────────────────────────────────────────────────────────────────
 * Device boots in Recon-Only mode.
 * Lab Mode requires /lab_mode.flag on SD + PIN entry.
 * Active features (transmit, connect) are NEVER enabled without Lab Mode.
 */

#include <Arduino.h>

#include "core/BoardProfile.h"
#include "core/DriverRegistry.h"
#include "core/CoreAPI.h"
#include "core/UiManager.h"
#include "core/ModuleManager.h"

// ── Module self-registration (include to pull in REGISTER_MODULE statics) ────
#include "modules/wifi_scanner/WifiScannerModule.h"
#include "modules/board_test/BoardTestModule.h"
#include "modules/ble_scanner/BleScannerModule.h"
#include "modules/pcap_logger/PcapLoggerModule.h"
#include "modules/mesh_chat/MeshChatModule.h"
#include "modules/ota_update/OtaUpdateModule.h"
#include "modules/lora_scanner/LoraScannerModule.h"
#include "modules/nfc_reader/NfcReaderModule.h"
#include "modules/ir_blaster/IrBlasterModule.h"

using namespace skullgate;

// ── Compiled-in fallback board profile ───────────────────────────────────────
// This JSON mirrors /firmware/boards/2432S022.json so the firmware works
// even if the SD card is absent at first boot.
#include "boards/2432S022_default.h"

// ── Global singletons ─────────────────────────────────────────────────────────
static BoardProfile    gProfile;
static DriverRegistry* gDrivers  = nullptr;
static UiManager*      gUi       = nullptr;
static CoreAPIImpl*    gCoreAPI  = nullptr;
static ModuleManager*  gModules  = nullptr;

// ── setup() ───────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║   SkullGate — Command the Dark ║");
    Serial.println("╚══════════════════════════════╝");
    Serial.println("Boot sequence starting...");
    Serial.println("Mode: RECON-ONLY (Lab Mode locked)");

    // ── 1. Board profile ──────────────────────────────────────────────────────
    // Attempt to load from SD first; fall back to compiled-in default.
    // SD isn't mounted yet so we load from string. SD re-load happens
    // after the driver registry mounts the card.
    if (!gProfile.loadFromString(BOARD_PROFILE_2432S022_JSON)) {
        Serial.println("[main] FATAL: default board profile failed to parse");
        while (true) delay(1000);
    }

    // ── 2. Driver registry ────────────────────────────────────────────────────
    gDrivers = new DriverRegistry(gProfile);
    if (!gDrivers->init()) {
        Serial.println("[main] WARNING: Some drivers failed to init");
        // Non-fatal — we continue in degraded mode
    }

    // Attempt to reload profile from SD if it's now mounted
    if (gDrivers->sd() && gDrivers->sd()->isReady()) {
        String sdPath = "/boards/" + gProfile.boardId() + ".json";
        if (gDrivers->sd()->exists(sdPath)) {
            Serial.printf("[main] Reloading profile from SD: %s\n", sdPath.c_str());
            String json = gDrivers->sd()->readFile(sdPath);
            gProfile.loadFromString(json.c_str());
        }
    }

    // ── 3. UI manager ─────────────────────────────────────────────────────────
    gUi = new UiManager(gDrivers->display(), gDrivers->touch());
    gUi->init();
    gUi->setSdStatus(gDrivers->sd() && gDrivers->sd()->isReady());
    gUi->setModeLabel("Recon-Only");

    // ── 4. CoreAPI ────────────────────────────────────────────────────────────
    gCoreAPI = new CoreAPIImpl(
        gDrivers->wifi(),
        gDrivers->sd(),
        gUi,
        gDrivers->ble(),   // Pass BleManager so bleScan() works
        false /* labMode off */
    );
    // Default permissions available to all modules in Recon-Only mode
    gCoreAPI->setPermissions({
        "wifi_scan", "sd_read", "sd_write", "ui", "serial",
        "ble_scan", "esp_now",
        "radio_rx", "nfc_read", "ir_tx"
    });

    // Wire the Lab Mode PIN screen: when the user submits a PIN in the UI,
    // the ModuleManager validates the SD flag + PIN and unlocks Lab Mode.
    // We set the callback after gModules is constructed below.

    // ── 5. Module manager ─────────────────────────────────────────────────────
    gModules = new ModuleManager(*gCoreAPI);
    int loaded = gModules->loadAll();
    Serial.printf("[main] Modules loaded: %d\n", loaded);

    // Now register the PIN callback so the UI can trigger Lab Mode unlock.
    gUi->setPinCallback([](const String& pin) {
        if (!gModules || !gCoreAPI || !gUi) return;
        if (gModules->unlockLabMode(pin)) {
            gCoreAPI->setLabMode(true);
            gUi->setModeLabel("Lab Mode");
            Serial.println("[main] Lab Mode UNLOCKED");
        } else {
            gUi->setModeLabel("Recon-Only (bad PIN)");
            Serial.println("[main] Lab Mode unlock failed (wrong PIN or no SD flag)");
        }
        gUi->showHome(); // Return to dashboard regardless of outcome
    });
    // The PIN screen is now registered — it won't show until the user taps "Lab".

    // Update UI with discovered APs (quick initial scan)
    auto aps = gCoreAPI->wifiScan();
    gUi->setWifiStatus(false, aps.size());

    // Initial battery reading (ADC pin 35 is common for battery on CYD boards)
    // The voltage divider typically gives Vbat/2 on the ADC pin.
    // ADC range 0–4095 → 0–3.3 V (1 V ref × 3.3 V supply), × 2 for divider.
    float batV = static_cast<float>(analogRead(35)) / 4095.0f * 3.3f * 2.0f;
    gUi->setBatteryVoltage(batV > 2.0f ? batV : 0.0f); // filter bogus readings

    Serial.println("[main] Boot complete — entering main loop");
}

// ── loop() ────────────────────────────────────────────────────────────────────

void loop() {
    // Drive LVGL task handler (handles rendering + touch events)
    if (gUi) gUi->loop();

    // Drive the active module's loop()
    if (gModules) gModules->loop();

    // Update battery voltage every ~10 s
    static uint32_t lastBatUpdate = 0;
    uint32_t now = millis();
    if (now - lastBatUpdate >= 10000) {
        lastBatUpdate = now;
        float batV = static_cast<float>(analogRead(35)) / 4095.0f * 3.3f * 2.0f;
        if (gUi) gUi->setBatteryVoltage(batV > 2.0f ? batV : 0.0f);
    }

    // Yield to FreeRTOS / watchdog
    delay(1);
}
