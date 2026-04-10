/**
 * @file SdManager.h
 * @brief SD card access layer.
 *
 * Wraps the Arduino SD library with simple, safe file operations.
 * All module SD access must go through CoreAPI which calls these methods
 * after validating permissions.
 */

#pragma once

#include <Arduino.h>
#include <SD.h>
#include "../core/BoardProfile.h"

namespace skullgate {

class SdManager {
public:
    explicit SdManager(const SdPins& pins);

    /// Mount the SD card. Returns false if no card is detected.
    bool init();

    bool isReady() const { return _ready; }

    /// Read entire file into a String. Returns "" on error.
    String readFile(const String& path);

    /// Write (overwrite) a file. Returns false on error.
    bool writeFile(const String& path, const String& data);

    /// Append to a file (creates it if not present). Returns false on error.
    bool appendFile(const String& path, const String& data);

    /// Delete a file. Returns false on error.
    bool deleteFile(const String& path);

    /// Returns true if the path exists.
    bool exists(const String& path);

    /// List directory contents to Serial (debug utility).
    void listDir(const String& dir, int levels = 0);

private:
    SdPins _pins;
    bool   _ready;
};

} // namespace skullgate
