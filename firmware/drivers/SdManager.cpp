/**
 * @file SdManager.cpp
 * @brief SD card file operations using the Arduino SD library.
 */

#include "SdManager.h"

namespace skullgate {

SdManager::SdManager(const SdPins& pins)
    : _pins(pins), _ready(false)
{}

bool SdManager::init() {
    // Configure CS pin
    if (_pins.cs != 0xFF) {
        pinMode(_pins.cs, OUTPUT);
        digitalWrite(_pins.cs, HIGH);
    }

    // Mount the SD card using the SPI CS pin.
    // The SPI bus itself is managed by BusManager.
    if (!SD.begin(_pins.cs)) {
        Serial.println("[SdManager] Mount failed — no SD card or wiring issue");
        _ready = false;
        return false;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SdManager] SD mounted. Size: %llu MB\n", cardSize);
    _ready = true;
    return true;
}

String SdManager::readFile(const String& path) {
    if (!_ready) return "";

    File f = SD.open(path, FILE_READ);
    if (!f) {
        Serial.printf("[SdManager] readFile failed: %s\n", path.c_str());
        return "";
    }

    String content;
    content.reserve(f.size());
    while (f.available()) content += (char)f.read();
    f.close();
    return content;
}

bool SdManager::writeFile(const String& path, const String& data) {
    if (!_ready) return false;

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[SdManager] writeFile failed: %s\n", path.c_str());
        return false;
    }

    size_t written = f.print(data);
    f.close();
    return written == data.length();
}

bool SdManager::appendFile(const String& path, const String& data) {
    if (!_ready) return false;

    File f = SD.open(path, FILE_APPEND);
    if (!f) {
        Serial.printf("[SdManager] appendFile failed: %s\n", path.c_str());
        return false;
    }

    size_t written = f.print(data);
    f.close();
    return written == data.length();
}

bool SdManager::deleteFile(const String& path) {
    if (!_ready) return false;
    if (!SD.exists(path)) return true; // Already gone
    return SD.remove(path);
}

bool SdManager::exists(const String& path) {
    if (!_ready) return false;
    return SD.exists(path);
}

void SdManager::listDir(const String& dir, int levels) {
    if (!_ready) return;

    File root = SD.open(dir);
    if (!root || !root.isDirectory()) {
        Serial.printf("[SdManager] listDir: not a directory: %s\n", dir.c_str());
        return;
    }

    File entry;
    while ((entry = root.openNextFile())) {
        for (int i = 0; i < levels; i++) Serial.print("  ");
        Serial.printf("%s%s\n", entry.name(),
                      entry.isDirectory() ? "/" : "");
        if (entry.isDirectory() && levels > 0) {
            listDir(String(entry.name()), levels - 1);
        }
        entry.close();
    }
    root.close();
}

} // namespace skullgate
