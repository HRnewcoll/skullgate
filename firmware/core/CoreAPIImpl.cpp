/**
 * @file CoreAPIImpl.cpp
 * @brief Concrete implementation of CoreAPI.
 */

#include "CoreAPI.h"
#include "../drivers/WifiManager.h"
#include "../drivers/SdManager.h"
#include "UiManager.h"

namespace skullgate {

CoreAPIImpl::CoreAPIImpl(WifiManager* wifi, SdManager* sd, UiManager* ui,
                         bool labMode)
    : _wifi(wifi), _sd(sd), _ui(ui), _labMode(labMode)
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

void CoreAPIImpl::log(const String& tag, const String& msg) {
    Serial.printf("[%s] %s\n", tag.c_str(), msg.c_str());
    // If SD is available and write permission is held, also log to file.
    if (_sd && _sd->isReady() && hasPermission("sd_write")) {
        String line = "[" + tag + "] " + msg + "\n";
        _sd->appendFile("/skullgate.log", line);
    }
}

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

std::vector<ApInfo> CoreAPIImpl::wifiScan() {
    if (!hasPermission("wifi_scan")) {
        Serial.println("[CoreAPI] wifiScan denied: missing wifi_scan permission");
        return {};
    }
    if (!_wifi) return {};
    return _wifi->scan();
}

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
