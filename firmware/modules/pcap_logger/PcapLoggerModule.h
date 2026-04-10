/**
 * @file PcapLoggerModule.h
 * @brief Wi-Fi frame capture to PCAP files on SD card.
 *
 * ── Safety model ─────────────────────────────────────────────────────────────
 * This module uses ESP32 Wi-Fi promiscuous mode to capture 802.11 frames.
 * It requires Lab Mode AND the "wifi_promiscuous" permission.
 *
 * USE ONLY ON NETWORKS YOU OWN OR HAVE EXPLICIT WRITTEN PERMISSION TO TEST.
 * Capturing frames from networks you do not own is illegal in most jurisdictions.
 *
 * ── Operation ────────────────────────────────────────────────────────────────
 * - Captures 802.11 management frames (beacons, probe responses, auth frames)
 *   on a selected Wi-Fi channel.
 * - Does NOT inject frames of any kind.
 * - Writes standard PCAP files to /pcap/ on the SD card.
 * - A new capture file is created each time the module starts.
 *
 * ── PCAP format ──────────────────────────────────────────────────────────────
 * Link type: LINKTYPE_IEEE802_11 (105)
 * The FCS (last 4 bytes reported by ESP32) is stripped before writing.
 *
 * Permissions: wifi_scan, wifi_promiscuous, sd_write, ui
 * Lab Mode: REQUIRED
 */

#pragma once

#include "../../core/ModuleManager.h"

namespace skullgate {

class PcapLoggerModule : public IModule {
public:
    PcapLoggerModule();
    ~PcapLoggerModule() override;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

    // Called by the global promiscuous callback to enqueue a captured frame.
    static void enqueueFrame(const uint8_t* payload, uint16_t len,
                             uint8_t channel, int8_t rssi);

private:
    void _buildScreen();
    void _writePcapGlobalHeader();
    void _drainCaptureBuf();
    void _updateStats();

    // ── Ring buffer ───────────────────────────────────────────────────────────
    // Shared between the ISR-context promiscuous callback and the main loop.
    // Frame size is capped at MAX_FRAME_BYTES; larger frames are truncated.
    static constexpr uint8_t  CAP_BUF_SIZE     = 24; // number of frames
    static constexpr uint16_t MAX_FRAME_BYTES  = 256;

    struct CapturedFrame {
        uint8_t  data[MAX_FRAME_BYTES];
        uint16_t len;
        uint32_t ts_ms;
        int8_t   rssi;
        uint8_t  channel;
    };

    static CapturedFrame s_ring[CAP_BUF_SIZE];
    static volatile uint8_t s_head; // written by callback
    static volatile uint8_t s_tail; // read by main loop

    // ── State ─────────────────────────────────────────────────────────────────
    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;
    bool           _capturing;
    uint8_t        _channel;    // Currently monitored channel (1–13)

    uint32_t _frameCount;
    uint32_t _byteCount;
    String   _capturePath;

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _lblStatus;
    lv_obj_t* _lblFrames;
    lv_obj_t* _lblChannel;
    lv_obj_t* _btnStart;
    lv_obj_t* _btnStop;
    lv_obj_t* _btnChanUp;
    lv_obj_t* _btnChanDn;
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("pcap_logger", []() -> skullgate::IModule* {
        return new skullgate::PcapLoggerModule();
    });
}
