/**
 * @file IrBlasterModule.h
 * @brief IR code transmitter / receiver module using ESP32 RMT peripheral.
 *
 * Sends saved IR codes to your own devices (TV, AC, etc.) and can learn new
 * codes by capturing IR signals.
 *
 * ── Operation ────────────────────────────────────────────────────────────────
 * IR codes are stored in /ir/ on the SD card as JSON files.  Each file
 * describes one device with a list of named commands and their NEC/SONY/raw
 * pulse sequences.
 *
 * ── IR code file format (/ir/my_tv.json) ─────────────────────────────────────
 * {
 *   "device": "Samsung TV",
 *   "protocol": "NEC",
 *   "commands": [
 *     {"name": "power", "code": "0xE0E040BF"},
 *     {"name": "vol_up","code": "0xE0E0E01F"}
 *   ]
 * }
 *
 * ── Safety ───────────────────────────────────────────────────────────────────
 * Only use to control devices you own.
 * IR TX is limited to your own devices / lab environment.
 *
 * Permissions: ir_tx, sd_read, sd_write, ui
 * Lab Mode: NOT required (local IR to your own devices)
 */

#pragma once

#include "../../core/ModuleManager.h"
#include <vector>

namespace skullgate {

struct IrCommand {
    String name;
    String protocol;  ///< "NEC", "SONY", "RAW"
    uint32_t code;    ///< 32-bit code (NEC / SONY)
    uint8_t  bits;    ///< Bit count (32 for NEC, 12/15/20 for SONY)
};

struct IrDevice {
    String              filename;
    String              deviceName;
    std::vector<IrCommand> commands;
};

class IrBlasterModule : public IModule {
public:
    IrBlasterModule();
    ~IrBlasterModule() override = default;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _buildNoHwScreen();
    bool _initRmt();
    bool _loadDevices();
    void _loadDevice(const String& path);
    void _populateList();
    bool _sendCommand(const IrCommand& cmd);

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;
    bool           _hwPresent;

    std::vector<IrDevice> _devices;
    int  _selectedDevice;    ///< Index into _devices

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _lblStatus;
    lv_obj_t* _deviceList;   ///< Device selector list
    lv_obj_t* _cmdList;      ///< Command buttons
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("ir_blaster", []() -> skullgate::IModule* {
        return new skullgate::IrBlasterModule();
    });
}
