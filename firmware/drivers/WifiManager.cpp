/**
 * @file WifiManager.cpp
 * @brief Wi-Fi driver — passive scan + optional Lab Mode connect.
 *
 * SAFETY: init() sets Wi-Fi to STA mode with no AP connection.
 * scan() uses passive scan type — no probe requests are transmitted.
 * connect() may only be called when Lab Mode is active and the
 * "wifi_connect" permission has been granted to the calling module.
 */

#include "WifiManager.h"

namespace skullgate {

WifiManager::WifiManager() : _ready(false) {}

bool WifiManager::init() {
    // Set station mode — no AP created, no connection attempted.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    _ready = true;
    Serial.println("[WifiManager] STA mode ready (Recon-Only)");
    return true;
}

std::vector<ApInfo> WifiManager::scan() {
    std::vector<ApInfo> results;
    if (!_ready) return results;

    Serial.println("[WifiManager] Passive scan started...");

    // Use passive async scan: ESP32 listens for beacon frames only.
    // async=true starts the scan in the background; we poll with a timeout
    // to avoid blocking the UI loop for several seconds.
    WiFi.scanNetworks(
        /*async*/       true,
        /*showHidden*/  true,
        /*passive*/     true,
        /*max_ms_per_chan*/ 300
    );

    // Poll for scan completion (up to 10 seconds)
    const uint32_t SCAN_TIMEOUT_MS = 10000;
    uint32_t start = millis();
    int n = WIFI_SCAN_RUNNING;

    while (n == WIFI_SCAN_RUNNING && millis() - start < SCAN_TIMEOUT_MS) {
        n = WiFi.scanComplete();
        delay(50); // Yield to LVGL / FreeRTOS
    }

    if (n == WIFI_SCAN_RUNNING) {
        Serial.println("[WifiManager] Scan timeout — cancelling");
        WiFi.scanDelete();
        return results;
    }

    if (n == WIFI_SCAN_FAILED) {
        Serial.println("[WifiManager] Scan failed");
        return results;
    }

    results.reserve(n);
    for (int i = 0; i < n; i++) {
        ApInfo ap;
        ap.ssid       = WiFi.SSID(i);
        ap.bssid      = WiFi.BSSIDstr(i);
        ap.rssi       = WiFi.RSSI(i);
        ap.channel    = WiFi.channel(i);
        ap.encryption = WiFi.encryptionType(i);
        results.push_back(ap);
    }

    // Sort by RSSI (strongest first)
    std::sort(results.begin(), results.end(),
              [](const ApInfo& a, const ApInfo& b) {
                  return a.rssi > b.rssi;
              });

    WiFi.scanDelete();

    Serial.printf("[WifiManager] Scan complete: %d APs\n", (int)results.size());
    return results;
}

bool WifiManager::connect(const String& ssid, const String& password,
                           uint32_t timeoutMs) {
    // Lab Mode callers only — permission already checked by CoreAPI.
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(200);
    }

    bool ok = (WiFi.status() == WL_CONNECTED);
    if (ok) {
        Serial.printf("[WifiManager] Connected to %s, IP: %s\n",
                      ssid.c_str(), WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("[WifiManager] Failed to connect to %s\n", ssid.c_str());
    }
    return ok;
}

void WifiManager::disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("[WifiManager] Disconnected");
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WifiManager::ipAddress() const {
    if (!isConnected()) return "";
    return WiFi.localIP().toString();
}

} // namespace skullgate
