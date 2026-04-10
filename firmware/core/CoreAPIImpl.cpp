/**
 * @file CoreAPIImpl.cpp
 * @brief Concrete implementation of CoreAPI.
 */

#include "CoreAPI.h"
#include "../drivers/WifiManager.h"
#include "../drivers/SdManager.h"
#include "../drivers/BleManager.h"
#include "UiManager.h"

#include <esp_now.h>
#include <WiFi.h>

namespace skullgate {

// ── ESP-NOW receive callback (global) ────────────────────────────────────────
// CoreAPIImpl registers itself here so the C-style callback can reach it.
static CoreAPIImpl* s_espNowInstance = nullptr;

static void _espNowRecvCb(const uint8_t* mac, const uint8_t* data, int len) {
    if (s_espNowInstance) {
        s_espNowInstance->_espNowEnqueue(mac, data, len);
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────

CoreAPIImpl::CoreAPIImpl(WifiManager* wifi, SdManager* sd, UiManager* ui,
                         BleManager* ble, bool labMode)
    : _wifi(wifi), _sd(sd), _ui(ui), _ble(ble)
    , _labMode(labMode), _espNowReady(false)
{}

void CoreAPIImpl::setPermissions(const std::vector<String>& perms) {
    _permissions = perms;
}

bool CoreAPIImpl::hasPermission(const String& perm) const {
    for (const auto& p : _permissions) {
        if (p == perm) return true;
    }
    return false;
}

// ── Logging ───────────────────────────────────────────────────────────────────

void CoreAPIImpl::log(const String& tag, const String& msg) {
    Serial.printf("[%s] %s\n", tag.c_str(), msg.c_str());
    // If SD is available and write permission is held, also log to file.
    if (_sd && _sd->isReady() && hasPermission("sd_write")) {
        String line = "[" + tag + "] " + msg + "\n";
        _sd->appendFile("/skullgate.log", line);
    }
}

// ── Storage ───────────────────────────────────────────────────────────────────

String CoreAPIImpl::readFile(const String& path) {
    if (!hasPermission("sd_read")) {
        Serial.println("[CoreAPI] readFile denied: missing sd_read permission");
        return "";
    }
    if (!_sd || !_sd->isReady()) return "";
    return _sd->readFile(path);
}

bool CoreAPIImpl::writeFile(const String& path, const String& data,
                             bool append) {
    if (!hasPermission("sd_write")) {
        Serial.println("[CoreAPI] writeFile denied: missing sd_write permission");
        return false;
    }
    if (!_sd || !_sd->isReady()) return false;
    return append ? _sd->appendFile(path, data) : _sd->writeFile(path, data);
}

bool CoreAPIImpl::deleteFile(const String& path) {
    if (!hasPermission("sd_write")) {
        Serial.println("[CoreAPI] deleteFile denied: missing sd_write permission");
        return false;
    }
    if (!_sd || !_sd->isReady()) return false;
    return _sd->deleteFile(path);
}

// ── Wi-Fi ─────────────────────────────────────────────────────────────────────

std::vector<ApInfo> CoreAPIImpl::wifiScan() {
    if (!hasPermission("wifi_scan")) {
        Serial.println("[CoreAPI] wifiScan denied: missing wifi_scan permission");
        return {};
    }
    if (!_wifi) return {};
    return _wifi->scan();
}

bool CoreAPIImpl::wifiConnect(const String& ssid, const String& password,
                               uint32_t timeoutMs) {
    if (!hasPermission("wifi_connect")) {
        Serial.println("[CoreAPI] wifiConnect denied: missing wifi_connect permission");
        return false;
    }
    if (!_labMode) {
        Serial.println("[CoreAPI] wifiConnect denied: Lab Mode not active");
        return false;
    }
    if (!_wifi) return false;
    return _wifi->connect(ssid, password, timeoutMs);
}

void CoreAPIImpl::wifiDisconnect() {
    if (!hasPermission("wifi_connect")) return;
    if (_wifi) _wifi->disconnect();
}

bool CoreAPIImpl::wifiIsConnected() const {
    if (!_wifi) return false;
    return _wifi->isConnected();
}

String CoreAPIImpl::wifiIpAddress() const {
    if (!_wifi) return "";
    return _wifi->ipAddress();
}

// ── BLE ───────────────────────────────────────────────────────────────────────

std::vector<BleDevice> CoreAPIImpl::bleScan(uint32_t durationMs) {
    if (!hasPermission("ble_scan")) {
        Serial.println("[CoreAPI] bleScan denied: missing ble_scan permission");
        return {};
    }
    if (!_ble || !_ble->isReady()) {
        Serial.println("[CoreAPI] bleScan: BLE driver not available");
        return {};
    }
    return _ble->scan(durationMs);
}

// ── ESP-NOW ───────────────────────────────────────────────────────────────────

bool CoreAPIImpl::espNowInit() {
    if (!hasPermission("esp_now")) {
        Serial.println("[CoreAPI] espNowInit denied: missing esp_now permission");
        return false;
    }
    if (_espNowReady) return true; // Already initialised

    // ESP-NOW requires WiFi to be active (STA mode is fine)
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("[CoreAPI] esp_now_init() failed");
        return false;
    }

    s_espNowInstance = this;
    esp_now_register_recv_cb(_espNowRecvCb);

    _espNowReady = true;
    Serial.println("[CoreAPI] ESP-NOW ready");
    return true;
}

bool CoreAPIImpl::espNowSend(const uint8_t mac[6],
                              const uint8_t* data, size_t len) {
    if (!hasPermission("esp_now")) {
        Serial.println("[CoreAPI] espNowSend denied: missing esp_now permission");
        return false;
    }
    if (!_espNowReady) {
        Serial.println("[CoreAPI] espNowSend: ESP-NOW not initialised");
        return false;
    }
    if (len > 250) len = 250;

    // Register peer (broadcast address is always registered implicitly)
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer if not already added (ignore error if already exists)
    esp_now_add_peer(&peerInfo);

    esp_err_t result = esp_now_send(mac, data, len);
    return result == ESP_OK;
}

std::vector<EspNowMessage> CoreAPIImpl::espNowReceive() {
    if (!hasPermission("esp_now")) return {};
    std::vector<EspNowMessage> out;
    out.swap(_espNowQueue); // Move and clear atomically (single-threaded loop)
    return out;
}

void CoreAPIImpl::_espNowEnqueue(const uint8_t src[6],
                                  const uint8_t* data, int len) {
    if (len <= 0) return;
    EspNowMessage msg;
    memcpy(msg.src_mac, src, 6);
    size_t copy = min((int)sizeof(msg.data), len);
    memcpy(msg.data, data, copy);
    msg.len = static_cast<uint8_t>(copy);
    _espNowQueue.push_back(msg);
}

// ── UI ────────────────────────────────────────────────────────────────────────

void CoreAPIImpl::registerScreen(const String& id, lv_obj_t* screen) {
    if (!hasPermission("ui")) {
        Serial.println("[CoreAPI] registerScreen denied: missing ui permission");
        return;
    }
    if (_ui) _ui->registerScreen(id, screen);
}

void CoreAPIImpl::showScreen(const String& id) {
    if (!hasPermission("ui")) return;
    if (_ui) _ui->showScreen(id);
}

} // namespace skullgate
