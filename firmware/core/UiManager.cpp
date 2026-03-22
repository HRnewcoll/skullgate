/**
 * @file UiManager.cpp
 * @brief LVGL + LovyanGFX UI skeleton implementation.
 *
 * Architecture:
 *   - LVGL 8.x is initialised with a static draw buffer.
 *   - The flush callback forwards pixel data to LovyanGFX (DisplayAdapter).
 *   - The input callback reads from TouchAdapter if available, otherwise
 *     returns no-touch (for headless / button-only boards).
 *   - Built-in screens are constructed with simple LVGL widgets.
 */

#include "UiManager.h"
#include "../drivers/DisplayAdapter.h"
#include "../drivers/TouchAdapter.h"

namespace skullgate {

// ── Static pointer for LVGL callbacks (single display assumed) ───────────────
static UiManager* s_instance = nullptr;

// ── Constructor / init ────────────────────────────────────────────────────────

UiManager::UiManager(DisplayAdapter* display, TouchAdapter* touch)
    : _display(display), _touch(touch)
    , _homeScreen(nullptr), _launcherScreen(nullptr), _logScreen(nullptr)
    , _lblWifi(nullptr), _lblSd(nullptr), _lblMode(nullptr)
    , _drawBuf1(nullptr), _drawBuf2(nullptr)
    , _lastTick(0) // Will be properly set to millis() in init()
{
    s_instance = this;
}

bool UiManager::init() {
    _lastTick = millis(); // Initialise to current time to avoid large tick on first loop()
    if (!_display) {
        Serial.println("[UiManager] No display — headless mode");
        return true; // Headless is valid
    }

    lv_init();

    // Allocate draw buffers (BUF_LINES * display width pixels each)
    uint16_t w = _display->width();
    uint32_t bufSize = w * BUF_LINES;
    _drawBuf1 = new lv_color_t[bufSize];
    _drawBuf2 = new lv_color_t[bufSize];

    // Initialise display buffer
    lv_disp_draw_buf_init(&_dispBuf, _drawBuf1, _drawBuf2, bufSize);

    // Register display driver
    lv_disp_drv_init(&_dispDrv);
    _dispDrv.hor_res  = _display->width();
    _dispDrv.ver_res  = _display->height();
    _dispDrv.flush_cb = _flushCb;
    _dispDrv.draw_buf = &_dispBuf;
    lv_disp_drv_register(&_dispDrv);

    // Register touch input device
    lv_indev_drv_init(&_indevDrv);
    _indevDrv.type    = LV_INDEV_TYPE_POINTER;
    _indevDrv.read_cb = _touchReadCb;
    lv_indev_drv_register(&_indevDrv);

    // Build built-in screens
    _buildHomeDashboard();
    _buildLauncherScreen();
    _buildLogViewer();

    // Start on the home screen
    lv_scr_load(_homeScreen);

    Serial.println("[UiManager] LVGL ready");
    return true;
}

void UiManager::loop() {
    uint32_t now = millis();
    lv_tick_inc(now - _lastTick);
    _lastTick = now;
    lv_task_handler();
}

// ── Screen management ─────────────────────────────────────────────────────────

void UiManager::registerScreen(const String& id, lv_obj_t* screen) {
    // Replace if already registered
    for (auto& e : _screens) {
        if (e.id == id) { e.screen = screen; return; }
    }
    _screens.push_back({id, screen});
    Serial.printf("[UiManager] Registered screen: %s\n", id.c_str());
}

void UiManager::showScreen(const String& id) {
    for (const auto& e : _screens) {
        if (e.id == id) {
            lv_scr_load(e.screen);
            return;
        }
    }
    Serial.printf("[UiManager] Screen not found: %s\n", id.c_str());
}

void UiManager::showHome() {
    if (_homeScreen) lv_scr_load(_homeScreen);
}

void UiManager::showLauncher(const std::vector<String>& moduleIds) {
    if (_launcherScreen) lv_scr_load(_launcherScreen);
    // Dynamically populate launcher buttons
    // (handled in _buildLauncherScreen called again or updated)
    (void)moduleIds; // TODO: populate buttons dynamically
}

void UiManager::showLogViewer() {
    if (_logScreen) lv_scr_load(_logScreen);
}

// ── Status bar updates ────────────────────────────────────────────────────────

void UiManager::setWifiStatus(bool connected, int aps) {
    if (!_lblWifi) return;
    char buf[32];
    snprintf(buf, sizeof(buf), connected ? "WiFi: ON (%d APs)" : "WiFi: OFF", aps);
    lv_label_set_text(_lblWifi, buf);
}

void UiManager::setSdStatus(bool mounted) {
    if (!_lblSd) return;
    lv_label_set_text(_lblSd, mounted ? "SD: OK" : "SD: --");
}

void UiManager::setModeLabel(const String& mode) {
    if (!_lblMode) return;
    lv_label_set_text(_lblMode, mode.c_str());
}

// ── Built-in screen builders ─────────────────────────────────────────────────

void UiManager::_buildHomeDashboard() {
    _homeScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_homeScreen, lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(_homeScreen);
    lv_label_set_text(title, "SkullGate");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0x22, 0x22), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Status labels
    _lblMode = lv_label_create(_homeScreen);
    lv_label_set_text(_lblMode, "Mode: Recon-Only");
    lv_obj_set_style_text_color(_lblMode, lv_color_white(), 0);
    lv_obj_align(_lblMode, LV_ALIGN_TOP_LEFT, 10, 40);

    _lblWifi = lv_label_create(_homeScreen);
    lv_label_set_text(_lblWifi, "WiFi: --");
    lv_obj_set_style_text_color(_lblWifi, lv_color_white(), 0);
    lv_obj_align(_lblWifi, LV_ALIGN_TOP_LEFT, 10, 65);

    _lblSd = lv_label_create(_homeScreen);
    lv_label_set_text(_lblSd, "SD: --");
    lv_obj_set_style_text_color(_lblSd, lv_color_white(), 0);
    lv_obj_align(_lblSd, LV_ALIGN_TOP_LEFT, 10, 90);

    // "Modules" button
    lv_obj_t* btnModules = lv_btn_create(_homeScreen);
    lv_obj_align(btnModules, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_size(btnModules, 100, 40);
    lv_obj_t* lblModules = lv_label_create(btnModules);
    lv_label_set_text(lblModules, "Modules");

    // "Log" button
    lv_obj_t* btnLog = lv_btn_create(_homeScreen);
    lv_obj_align(btnLog, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_size(btnLog, 80, 40);
    lv_obj_t* lblLog = lv_label_create(btnLog);
    lv_label_set_text(lblLog, "Log");

    // Event callbacks — navigate to launcher / log screens
    lv_obj_add_event_cb(btnModules, [](lv_event_t* e) {
        if (s_instance && s_instance->_launcherScreen)
            lv_scr_load(s_instance->_launcherScreen);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_add_event_cb(btnLog, [](lv_event_t* e) {
        if (s_instance && s_instance->_logScreen)
            lv_scr_load(s_instance->_logScreen);
    }, LV_EVENT_CLICKED, nullptr);
}

void UiManager::_buildLauncherScreen() {
    _launcherScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_launcherScreen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_launcherScreen);
    lv_label_set_text(title, "Module Launcher");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0x22, 0x22), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_launcherScreen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_size(btnBack, 80, 35);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        if (s_instance && s_instance->_homeScreen)
            lv_scr_load(s_instance->_homeScreen);
    }, LV_EVENT_CLICKED, nullptr);
}

void UiManager::_buildLogViewer() {
    _logScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_logScreen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_logScreen);
    lv_label_set_text(title, "Log Viewer");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0x22, 0x22), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Scrollable log text area
    lv_obj_t* ta = lv_textarea_create(_logScreen);
    lv_obj_set_size(ta, 220, 180);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    lv_textarea_set_text(ta, "(log output)");

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_logScreen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_size(btnBack, 80, 35);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        if (s_instance && s_instance->_homeScreen)
            lv_scr_load(s_instance->_homeScreen);
    }, LV_EVENT_CLICKED, nullptr);
}

// ── LVGL static callbacks ─────────────────────────────────────────────────────

void UiManager::_flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                           lv_color_t* color_p) {
    if (s_instance && s_instance->_display) {
        s_instance->_display->pushPixels(
            area->x1, area->y1, area->x2, area->y2,
            reinterpret_cast<uint16_t*>(color_p));
    }
    lv_disp_flush_ready(drv);
}

void UiManager::_touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    if (s_instance && s_instance->_touch && s_instance->_touch->isReady()) {
        int16_t tx, ty;
        bool pressed = s_instance->_touch->read(tx, ty);
        data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        data->point.x = tx;
        data->point.y = ty;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

} // namespace skullgate
