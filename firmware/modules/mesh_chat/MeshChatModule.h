/**
 * @file MeshChatModule.h
 * @brief ESP-NOW peer-to-peer mesh chat between SkullGate devices.
 *
 * Uses ESP-NOW for low-latency, WiFi-AP-free messaging between ESP32 devices
 * on the same channel.  Works with both AP association and no AP at all.
 *
 * ── Protocol ─────────────────────────────────────────────────────────────────
 * Messages are JSON objects sent as ESP-NOW payloads (max 250 bytes):
 *   {"t":"msg",  "f":"AA:BB:...", "n":"Name", "m":"Hello"}
 *   {"t":"disc", "f":"AA:BB:...", "n":"Name"}   ← broadcast on start
 *   {"t":"ack",  "f":"AA:BB:...", "n":"Name"}   ← reply to discovery
 *
 * Permissions: esp_now, sd_write, ui
 * Lab Mode: NOT required — peer communication between owned devices
 */

#pragma once

#include "../../core/ModuleManager.h"
#include <vector>

namespace skullgate {

struct ChatPeer {
    uint8_t mac[6];
    String  name;
    uint32_t lastSeen; ///< millis() of last received packet
};

struct ChatMessage {
    String from;    ///< MAC address string
    String name;    ///< Peer display name
    String text;
    uint32_t ts;    ///< millis() timestamp
    bool outgoing;
};

class MeshChatModule : public IModule {
public:
    MeshChatModule();
    ~MeshChatModule() override = default;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    static constexpr uint8_t  BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    static constexpr uint32_t DISCOVER_INTERVAL_MS = 30000;
    static constexpr size_t   MAX_MESSAGES       = 20;
    static constexpr uint16_t MSG_PAYLOAD_MAX    = 200; // chars for text field

    void _buildScreen();
    void _sendDiscovery();
    void _processMessages();
    void _appendChatLog(const ChatMessage& msg);
    void _sendText(const String& text);
    String _macStr() const; // Returns this device's MAC as "AA:BB:CC:DD:EE:FF"

    CoreAPI*              _api;
    ModuleManifest        _manifest;
    bool                  _running;
    bool                  _espNowUp;
    uint32_t              _lastDiscover;
    String                _deviceName;   // e.g. "SkullGate-AABB"

    std::vector<ChatPeer>    _peers;
    std::vector<ChatMessage> _messages;

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _textArea;  // Chat log (read-only scrollable)
    lv_obj_t* _lblPeers;
    lv_obj_t* _lblStatus;
    lv_obj_t* _btnSend;
    lv_obj_t* _taInput;   // Text input field
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("mesh_chat", []() -> skullgate::IModule* {
        return new skullgate::MeshChatModule();
    });
}
