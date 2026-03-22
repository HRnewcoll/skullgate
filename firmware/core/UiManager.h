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

private:
    // LVGL flush + input callbacks (static, required by LVGL C API)
    static void _flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                          lv_color_t* color_p);
    static void _touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data);

    void _buildHomeDashboard();
    void _buildLauncherScreen();
    void _buildLogViewer();

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

    // Home dashboard widgets
    lv_obj_t* _lblWifi;
    lv_obj_t* _lblSd;
    lv_obj_t* _lblMode;

    // Module-registered screens
    std::vector<ScreenEntry> _screens;

    uint32_t _lastTick;
};

} // namespace skullgate
