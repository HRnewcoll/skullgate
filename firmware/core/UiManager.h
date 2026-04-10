/**
 * @file UiManager.h
 * @brief UI skeleton using LovyanGFX + LVGL.
 *
 * Manages:
 *   - LVGL tick and display flush
 *   - Named screen registry (modules register their screens here)
 *   - Built-in screens: Home dashboard, Module launcher, Log viewer
 *
 * Board-agnostic: all display access goes through LovyanGFX via DisplayAdapter.
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>

// lvgl header — lv_conf.h must be reachable on the include path.
#include <lvgl.h>

namespace skullgate {

class DisplayAdapter;
class TouchAdapter;

// ── Named screen entry ────────────────────────────────────────────────────────

struct ScreenEntry {
    String     id;
    lv_obj_t*  screen;
};

// ── UiManager ────────────────────────────────────────────────────────────────

class UiManager {
public:
    UiManager(DisplayAdapter* display, TouchAdapter* touch);

    /// Initialise LVGL, flush callbacks, input device.
    bool init();

    /// Must be called every loop iteration (drives LVGL task handler + tick).
    void loop();

    // ── Screen navigation ─────────────────────────────────────────────────────
    void registerScreen(const String& id, lv_obj_t* screen);
    void showScreen(const String& id);

    /// Show the built-in home dashboard.
    void showHome();

    /// Show the module launcher overlay.
    void showLauncher(const std::vector<String>& moduleIds);

    /// Show the log viewer screen.
    void showLogViewer();

    // ── Status bar helpers (called by firmware core) ──────────────────────────
    void setWifiStatus(bool connected, int aps);
    void setSdStatus(bool mounted);
    void setModeLabel(const String& mode);

    /// Update battery voltage display (0.0 = unknown / no reading).
    void setBatteryVoltage(float voltage);

    /**
     * @brief Register the PIN submit callback without navigating to the PIN screen.
     *
     * The PIN screen is shown when the user taps "Lab" on the home dashboard.
     * Use this to register the callback during setup before entering the main loop.
     *
     * @param onSubmit  Callback invoked with the entered PIN string on confirmation.
     */
    void setPinCallback(std::function<void(const String&)> onSubmit);

    /**
     * @brief Show the Lab Mode PIN entry screen.
     *
     * Displays a numeric keypad.  When the user submits a PIN, @p onSubmit
     * is called with the entered string.  The caller is responsible for
     * validating the PIN and calling setModeLabel() / showHome() as needed.
     *
     * @param onSubmit  Callback invoked with the entered PIN string.
     */
    void showPinScreen(std::function<void(const String&)> onSubmit);

private:
    // LVGL flush + input callbacks (static, required by LVGL C API)
    static void _flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                          lv_color_t* color_p);
    static void _touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);

    void _buildHomeDashboard();
    void _buildLauncherScreen();
    void _buildLogViewer();
    void _buildPinScreen();

    DisplayAdapter* _display;
    TouchAdapter*   _touch;

    // LVGL driver structs
    lv_disp_drv_t      _dispDrv;
    lv_disp_draw_buf_t _dispBuf;   // LVGL 8.x draw buffer (lv_disp_draw_buf_t)
    lv_indev_drv_t     _indevDrv;

    // Draw buffer — two lines of pixels for double-buffered flush
    // Size: width * 2 * sizeof(lv_color_t)
    static constexpr int BUF_LINES = 10;
    lv_color_t* _drawBuf1;
    lv_color_t* _drawBuf2;

    // Built-in screens
    lv_obj_t* _homeScreen;
    lv_obj_t* _launcherScreen;
    lv_obj_t* _logScreen;
    lv_obj_t* _pinScreen;

    // Home dashboard widgets
    lv_obj_t* _lblWifi;
    lv_obj_t* _lblSd;
    lv_obj_t* _lblMode;
    lv_obj_t* _lblBattery;  ///< Battery voltage indicator

    // PIN screen state
    lv_obj_t* _pinDisplay;  ///< Shows masked PIN digits
    String    _pinBuffer;   ///< Accumulated PIN digits
    std::function<void(const String&)> _pinCallback;

    // Module-registered screens
    std::vector<ScreenEntry> _screens;

    uint32_t _lastTick;
};

} // namespace skullgate
