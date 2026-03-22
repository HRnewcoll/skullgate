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
        false /* labMode off */
    );
    // Default permissions available to all modules in Recon-Only mode
    gCoreAPI->setPermissions({"wifi_scan", "sd_read", "sd_write", "ui", "serial"});

    // ── 5. Module manager ─────────────────────────────────────────────────────
    gModules = new ModuleManager(*gCoreAPI);
    int loaded = gModules->loadAll();
    Serial.printf("[main] Modules loaded: %d\n", loaded);

    // Update UI with discovered APs (quick initial scan)
    auto aps = gCoreAPI->wifiScan();
    gUi->setWifiStatus(false, aps.size());

    Serial.println("[main] Boot complete — entering main loop");
}

// ── loop() ────────────────────────────────────────────────────────────────────

void loop() {
    // Drive LVGL task handler (handles rendering + touch events)
    if (gUi) gUi->loop();

    // Drive the active module's loop()
    if (gModules) gModules->loop();

    // Yield to FreeRTOS / watchdog
    delay(1);
}
