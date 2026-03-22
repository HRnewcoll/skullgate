/**
 * @file MeshChatModule.cpp
 * @brief ESP-NOW mesh chat implementation.
 */

#include "MeshChatModule.h"
#include <ArduinoJson.h>
#include <WiFi.h>

namespace skullgate {

// Suppress unused-variable warning for constexpr array definition
constexpr uint8_t MeshChatModule::BROADCAST_MAC[6];

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest MESH_MANIFEST = {
    "mesh_chat",
    "Mesh Chat",
    "1.0.0",
    "SkullGate",
    "ESP-NOW peer-to-peer mesh chat between SkullGate devices",
    {"esp_now", "sd_write", "ui"},
    false, // Lab Mode not required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

MeshChatModule::MeshChatModule()
    : _api(nullptr)
    , _manifest(MESH_MANIFEST)
    , _running(false)
    , _espNowUp(false)
    , _lastDiscover(0)
    , _screen(nullptr)
    , _textArea(nullptr)
    , _lblPeers(nullptr)
    , _lblStatus(nullptr)
    , _btnSend(nullptr)
    , _taInput(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool MeshChatModule::init(CoreAPI& api) {
    _api = &api;

    // Build device name from last 2 MAC bytes
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char nameBuf[20];
    snprintf(nameBuf, sizeof(nameBuf), "SkullGate-%02X%02X", mac[4], mac[5]);
    _deviceName = String(nameBuf);

    _buildScreen();
    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("mesh_chat", _screen);
    }
    _api->log("mesh_chat", "Module initialised as " + _deviceName);
    return true;
}

void MeshChatModule::start() {
    _running = true;

    if (_screen) _api->showScreen("mesh_chat");

    // Initialise ESP-NOW
    if (!_espNowUp) {
        _espNowUp = _api->espNowInit();
        if (!_espNowUp) {
            if (_lblStatus) lv_label_set_text(_lblStatus, "ESP-NOW init failed");
            _api->log("mesh_chat", "ESP-NOW init failed");
            return;
        }
    }

    if (_lblStatus) lv_label_set_text(_lblStatus, "Ready — broadcasting...");
    _lastDiscover = 0; // Force immediate discovery broadcast
    _api->log("mesh_chat", "Started");
}

void MeshChatModule::loop() {
    if (!_running || !_espNowUp) return;

    // Periodic discovery broadcast
    uint32_t now = millis();
    if (now - _lastDiscover >= DISCOVER_INTERVAL_MS) {
        _lastDiscover = now;
        _sendDiscovery();
    }

    // Process incoming messages
    _processMessages();
}

void MeshChatModule::stop() {
    _running = false;
    _api->log("mesh_chat", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

String MeshChatModule::_macStr() const {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void MeshChatModule::_sendDiscovery() {
    // {"t":"disc","f":"AA:BB:...","n":"SkullGate-AABB"}
    StaticJsonDocument<128> doc;
    doc["t"] = "disc";
    doc["f"] = _macStr();
    doc["n"] = _deviceName;

    String json;
    serializeJson(doc, json);

    _api->espNowSend(BROADCAST_MAC,
                     reinterpret_cast<const uint8_t*>(json.c_str()),
                     json.length());
}

void MeshChatModule::_sendText(const String& text) {
    if (text.isEmpty()) return;

    // {"t":"msg","f":"AA:BB:...","n":"SkullGate-AABB","m":"Hello!"}
    StaticJsonDocument<256> doc;
    doc["t"] = "msg";
    doc["f"] = _macStr();
    doc["n"] = _deviceName;
    doc["m"] = text.substring(0, MSG_PAYLOAD_MAX);

    String json;
    serializeJson(doc, json);

    _api->espNowSend(BROADCAST_MAC,
                     reinterpret_cast<const uint8_t*>(json.c_str()),
                     json.length());

    // Add outgoing message to local log
    ChatMessage out;
    out.from     = _macStr();
    out.name     = _deviceName;
    out.text     = text.substring(0, MSG_PAYLOAD_MAX);
    out.ts       = millis();
    out.outgoing = true;
    _appendChatLog(out);

    _api->log("mesh_chat", "Sent: " + text.substring(0, 40));
}

void MeshChatModule::_processMessages() {
    auto msgs = _api->espNowReceive();
    if (msgs.empty()) return;

    for (const auto& raw : msgs) {
        // Parse JSON payload
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(
            doc, reinterpret_cast<const char*>(raw.data), raw.len);
        if (err) continue;

        const char* type = doc["t"];
        const char* from = doc["f"];
        const char* name = doc["n"];
        if (!type || !from) continue;

        String fromStr = String(from);
        String nameStr = name ? String(name) : fromStr;

        if (strcmp(type, "disc") == 0 || strcmp(type, "ack") == 0) {
            // Update peer list
            bool found = false;
            for (auto& p : _peers) {
                char pMac[18];
                snprintf(pMac, sizeof(pMac), "%02X:%02X:%02X:%02X:%02X:%02X",
                         p.mac[0], p.mac[1], p.mac[2], p.mac[3],
                         p.mac[4], p.mac[5]);
                if (fromStr.equalsIgnoreCase(pMac)) {
                    p.name     = nameStr;
                    p.lastSeen = millis();
                    found = true;
                    break;
                }
            }
            if (!found) {
                ChatPeer peer;
                // Parse MAC from hex string
                sscanf(fromStr.c_str(),
                       "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                       &peer.mac[0], &peer.mac[1], &peer.mac[2],
                       &peer.mac[3], &peer.mac[4], &peer.mac[5]);
                peer.name     = nameStr;
                peer.lastSeen = millis();
                _peers.push_back(peer);

                _api->log("mesh_chat", "Peer discovered: " + nameStr);
            }

            // Reply to discovery with ack
            if (strcmp(type, "disc") == 0) {
                StaticJsonDocument<128> ack;
                ack["t"] = "ack";
                ack["f"] = _macStr();
                ack["n"] = _deviceName;
                String ackJson;
                serializeJson(ack, ackJson);
                // Send back directly to source (would need peer's MAC)
                _api->espNowSend(BROADCAST_MAC,
                                 reinterpret_cast<const uint8_t*>(ackJson.c_str()),
                                 ackJson.length());
            }

            // Update peer count label
            if (_lblPeers) {
                char buf[24];
                snprintf(buf, sizeof(buf), "Peers: %d", (int)_peers.size());
                lv_label_set_text(_lblPeers, buf);
            }
        }
        else if (strcmp(type, "msg") == 0) {
            const char* msgText = doc["m"];
            if (!msgText) continue;

            ChatMessage cm;
            cm.from     = fromStr;
            cm.name     = nameStr;
            cm.text     = String(msgText);
            cm.ts       = millis();
            cm.outgoing = false;
            _appendChatLog(cm);

            _api->log("mesh_chat", nameStr + ": " + cm.text.substring(0, 40));
        }
    }
}

void MeshChatModule::_appendChatLog(const ChatMessage& msg) {
    // Keep ring buffer of messages
    if (_messages.size() >= MAX_MESSAGES) {
        _messages.erase(_messages.begin());
    }
    _messages.push_back(msg);

    // Rebuild the text area content
    if (!_textArea) return;
    String log;
    for (const auto& m : _messages) {
        log += m.outgoing ? "[Me] " : ("[" + m.name + "] ");
        log += m.text;
        log += "\n";
    }
    lv_textarea_set_text(_textArea, log.c_str());
    // Scroll to bottom
    lv_obj_scroll_to_y(_textArea, LV_COORD_MAX, LV_ANIM_OFF);
}

void MeshChatModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "Mesh Chat");
    lv_obj_set_style_text_color(title, lv_color_make(0x00, 0xFF, 0x88), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Status / peer count
    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Idle");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_LEFT, 4, 22);

    _lblPeers = lv_label_create(_screen);
    lv_label_set_text(_lblPeers, "Peers: 0");
    lv_obj_set_style_text_color(_lblPeers, lv_color_make(0x00, 0xFF, 0x88), 0);
    lv_obj_align(_lblPeers, LV_ALIGN_TOP_RIGHT, -4, 22);

    // Chat log text area (read-only)
    _textArea = lv_textarea_create(_screen);
    lv_obj_set_size(_textArea, 230, 140);
    lv_obj_align(_textArea, LV_ALIGN_TOP_MID, 0, 40);
    lv_textarea_set_text(_textArea, "");
    lv_obj_set_style_bg_color(_textArea, lv_color_make(0x08, 0x08, 0x10), 0);
    lv_obj_set_style_text_color(_textArea, lv_color_white(), 0);
    lv_textarea_set_cursor_pos(_textArea, LV_TEXTAREA_CURSOR_LAST);

    // Message input field
    _taInput = lv_textarea_create(_screen);
    lv_obj_set_size(_taInput, 160, 32);
    lv_obj_align(_taInput, LV_ALIGN_BOTTOM_LEFT, 4, -36);
    lv_textarea_set_one_line(_taInput, true);
    lv_textarea_set_placeholder_text(_taInput, "Type a message...");
    lv_obj_set_style_bg_color(_taInput, lv_color_make(0x15, 0x15, 0x25), 0);

    // Send button
    _btnSend = lv_btn_create(_screen);
    lv_obj_set_size(_btnSend, 60, 32);
    lv_obj_align(_btnSend, LV_ALIGN_BOTTOM_RIGHT, -4, -36);
    lv_obj_set_style_bg_color(_btnSend, lv_color_make(0x00, 0x77, 0x00), 0);
    lv_obj_t* lblSend = lv_label_create(_btnSend);
    lv_label_set_text(lblSend, "Send");

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_size(btnBack, 70, 28);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* m = static_cast<MeshChatModule*>(lv_event_get_user_data(e));
        if (m && m->_api) m->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(_btnSend, [](lv_event_t* e) {
        auto* m = static_cast<MeshChatModule*>(lv_event_get_user_data(e));
        if (!m || !m->_running) return;
        const char* txt = lv_textarea_get_text(m->_taInput);
        if (txt && strlen(txt) > 0) {
            m->_sendText(String(txt));
            lv_textarea_set_text(m->_taInput, "");
        }
    }, LV_EVENT_CLICKED, this);
}

} // namespace skullgate
