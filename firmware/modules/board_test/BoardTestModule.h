/**
 * @file BoardTestModule.h
 * @brief Board Test module — verifies display, touch, SD, and Wi-Fi.
 *
 * Runs a suite of hardware self-tests and displays pass/fail results
 * on the LVGL screen. Useful for first-boot validation and debugging.
 *
 * Tests performed:
 *   1. Display: fills screen with a colour pattern
 *   2. Touch:   prompts user to touch 3 calibration points
 *   3. SD:      writes and reads back a test file
 *   4. Wi-Fi:   performs a passive scan and reports AP count
 */

#pragma once

#include "../../core/ModuleManager.h"

namespace skullgate {

class BoardTestModule : public IModule {
public:
    BoardTestModule();
    ~BoardTestModule() override = default;

    bool init(CoreAPI& api) override;
    void start() override;
    void loop() override;
    void stop() override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _runTests();
    void _setResult(int row, const char* label, bool pass, const String& detail = "");

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;
    bool           _testsDone;

    // LVGL screen
    lv_obj_t* _screen;
    lv_obj_t* _table;  // Results table
    lv_obj_t* _lblTitle;
};

} // namespace skullgate

// Self-registration
namespace {
    REGISTER_MODULE("board_test", []() -> skullgate::IModule* {
        return new skullgate::BoardTestModule();
    });
}
