/**
 * @file IrBlasterModule.cpp
 * @brief IR blaster using ESP32 RMT peripheral (stub when IR pin absent).
 *
 * TX uses ESP32 RMT in NEC / SONY protocol mode.  A standard 38 kHz IR LED
 * (or Vishay TSOP receiver for learning) is expected on the IR TX pin defined
 * in the board profile.  The default is GPIO 4.
 *
 * When BOARD_IR_TX_PIN is not defined, the module shows a "no hardware"
 * screen and returns cleanly.
 */

#include "IrBlasterModule.h"
#include <ArduinoJson.h>

// ── RMT / IR guard ────────────────────────────────────────────────────────────
#ifdef BOARD_IR_TX_PIN
#  include <IRsend.h>   // IRremoteESP8266 / IRremote library
static IRsend s_irSend(BOARD_IR_TX_PIN);
#endif

namespace skullgate {

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest IR_MANIFEST = {
    "ir_blaster",
    "IR Blaster",
    "1.0.0",
    "SkullGate",
    "IR code transmitter. Send saved codes to your own devices. Requires BOARD_IR_TX_PIN.",
    {"ir_tx", "sd_read", "sd_write", "ui"},
    false, // Lab Mode not required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

IrBlasterModule::IrBlasterModule()
    : _api(nullptr)
    , _manifest(IR_MANIFEST)
    , _running(false)
    , _hwPresent(false)
    , _selectedDevice(-1)
    , _screen(nullptr)
    , _lblStatus(nullptr)
    , _deviceList(nullptr)
    , _cmdList(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool IrBlasterModule::init(CoreAPI& api) {
    _api = &api;
    _hwPresent = _initRmt();

    if (_hwPresent) {
        _loadDevices();
        _buildScreen();
    } else {
        _buildNoHwScreen();
    }

    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("ir_blaster", _screen);
    }

    _api->log("ir_blaster",
              _hwPresent ? String("IR ready, ") + _devices.size() + " device(s) loaded"
                         : "No IR hardware (BOARD_IR_TX_PIN not defined)");
    return true;
}

void IrBlasterModule::start() {
    _running = true;
    if (_screen) _api->showScreen("ir_blaster");
    _api->log("ir_blaster", "Started");
}

void IrBlasterModule::loop() {
    // UI-driven — no periodic work required.
}

void IrBlasterModule::stop() {
    _running = false;
    _api->log("ir_blaster", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool IrBlasterModule::_initRmt() {
#ifdef BOARD_IR_TX_PIN
    s_irSend.begin();
    Serial.printf("[ir_blaster] IR TX on GPIO %d\n", BOARD_IR_TX_PIN);
    return true;
#else
    return false;
#endif
}

bool IrBlasterModule::_loadDevices() {
    if (!_api->hasPermission("sd_read")) return false;

    // List /ir/*.json files.  We read up to 8 device files.
    const char* paths[] = {
        "/ir/device0.json", "/ir/device1.json", "/ir/device2.json",
        "/ir/device3.json", "/ir/device4.json", "/ir/device5.json",
        "/ir/device6.json", "/ir/device7.json"
    };

    for (const char* p : paths) {
        String content = _api->readFile(p);
        if (!content.isEmpty()) {
            _loadDevice(content);
        }
    }

    if (_devices.empty()) {
        // Create a demo device so the UI shows something useful.
        IrDevice demo;
        demo.filename   = "(demo)";
        demo.deviceName = "Sample TV";
        demo.commands.push_back({"power",   "NEC", 0xE0E040BF, 32});
        demo.commands.push_back({"vol_up",  "NEC", 0xE0E0E01F, 32});
        demo.commands.push_back({"vol_dn",  "NEC", 0xE0E0D02F, 32});
        demo.commands.push_back({"mute",    "NEC", 0xE0E0F00F, 32});
        _devices.push_back(demo);
    }

    _selectedDevice = 0;
    return true;
}

void IrBlasterModule::_loadDevice(const String& json) {
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    IrDevice dev;
    dev.deviceName = doc["device"] | "Unknown";
    dev.filename   = doc["file"]   | "";

    JsonArray cmds = doc["commands"].as<JsonArray>();
    for (JsonObject c : cmds) {
        IrCommand cmd;
        cmd.name     = c["name"]     | "";
        cmd.protocol = c["protocol"] | "NEC";
        cmd.code     = c["code"]     | 0UL;
        cmd.bits     = c["bits"]     | 32;
        if (!cmd.name.isEmpty()) dev.commands.push_back(cmd);
    }
    _devices.push_back(dev);
}

bool IrBlasterModule::_sendCommand(const IrCommand& cmd) {
#ifdef BOARD_IR_TX_PIN
    if (cmd.protocol == "NEC") {
        s_irSend.sendNEC(cmd.code, cmd.bits);
        return true;
    } else if (cmd.protocol == "SONY") {
        s_irSend.sendSony(cmd.code, cmd.bits);
        return true;
    }
    return false;
#else
    return false;
#endif
}

void IrBlasterModule::_populateList() {
    if (!_cmdList || _selectedDevice < 0
        || _selectedDevice >= static_cast<int>(_devices.size())) return;

    lv_obj_clean(_cmdList);

    const auto& dev = _devices[_selectedDevice];
    for (size_t i = 0; i < dev.commands.size(); i++) {
        const auto& cmd = dev.commands[i];
        lv_obj_t* btn = lv_list_add_btn(_cmdList, NULL, cmd.name.c_str());

        // Capture index for the event callback via user data on the button.
        // We store the module pointer in the list, index via closure over i.
        // Since lambdas can't capture structured bindings in older C++, use
        // a struct trick via button user_data.
        struct BtnData { IrBlasterModule* mod; size_t idx; };
        BtnData* bd = new BtnData{this, i};
        lv_obj_set_user_data(btn, bd);

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto* bd = static_cast<BtnData*>(lv_obj_get_user_data(
                            static_cast<lv_obj_t*>(lv_event_get_target(e))));
            if (!bd) return;
            IrBlasterModule* mod = bd->mod;
            size_t idx = bd->idx;
            if (mod->_selectedDevice < 0) return;
            const auto& d = mod->_devices[mod->_selectedDevice];
            if (idx < d.commands.size()) {
                const auto& c = d.commands[idx];
                bool ok = mod->_sendCommand(c);
                mod->_api->log("ir_blaster",
                    (ok ? "Sent: " : "TX failed: ") + c.name);
                if (mod->_lblStatus) {
                    lv_label_set_text(mod->_lblStatus,
                        (ok ? "Sent: " + c.name : "TX error").c_str());
                }
            }
        }, LV_EVENT_CLICKED, nullptr);
    }
}

void IrBlasterModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "IR Blaster");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0xCC, 0x00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Ready");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_RIGHT, -5, 6);

    // Device selector (compact, horizontal strip at top)
    _deviceList = lv_list_create(_screen);
    lv_obj_set_size(_deviceList, 230, 50);
    lv_obj_align(_deviceList, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_flex_flow(_deviceList, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(_deviceList, lv_color_make(0x0A, 0x0A, 0x18), 0);

    for (size_t i = 0; i < _devices.size(); i++) {
        lv_obj_t* btn = lv_list_add_btn(_deviceList, NULL,
                                        _devices[i].deviceName.c_str());
        struct DevBtnData { IrBlasterModule* mod; size_t idx; };
        DevBtnData* dbd = new DevBtnData{this, i};
        lv_obj_set_user_data(btn, dbd);

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto* dbd = static_cast<DevBtnData*>(lv_obj_get_user_data(
                            static_cast<lv_obj_t*>(lv_event_get_target(e))));
            if (!dbd) return;
            dbd->mod->_selectedDevice = static_cast<int>(dbd->idx);
            dbd->mod->_populateList();
        }, LV_EVENT_CLICKED, nullptr);
    }

    // Command list
    _cmdList = lv_list_create(_screen);
    lv_obj_set_size(_cmdList, 230, 160);
    lv_obj_align(_cmdList, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(_cmdList, lv_color_make(0x0A, 0x0A, 0x18), 0);
    _populateList();

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<IrBlasterModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void IrBlasterModule::_buildNoHwScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "IR Blaster");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0xCC, 0x00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t* lbl = lv_label_create(_screen);
    lv_label_set_text(lbl, "No IR hardware.\n\nConnect an IR LED to a GPIO\npin and define\nBOARD_IR_TX_PIN=<pin>\nin your build flags.");
    lv_obj_set_style_text_color(lbl, lv_color_make(0xFF, 0x88, 0x00), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 200);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<IrBlasterModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

} // namespace skullgate
