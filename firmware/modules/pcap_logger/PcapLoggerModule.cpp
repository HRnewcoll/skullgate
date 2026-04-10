/**
 * @file PcapLoggerModule.cpp
 * @brief Wi-Fi promiscuous capture to PCAP files.
 *
 * SAFETY REMINDER: Only use on networks and devices you own or have explicit
 * written permission to test. See SAFETY_POLICY.md for legal guidance.
 */

#include "PcapLoggerModule.h"
#include <esp_wifi.h>

namespace skullgate {

// ── Static ring-buffer members ────────────────────────────────────────────────
PcapLoggerModule::CapturedFrame PcapLoggerModule::s_ring[CAP_BUF_SIZE];
volatile uint8_t PcapLoggerModule::s_head = 0;
volatile uint8_t PcapLoggerModule::s_tail = 0;

// ── PCAP constants ────────────────────────────────────────────────────────────
static constexpr uint32_t PCAP_MAGIC       = 0xa1b2c3d4;
static constexpr uint16_t PCAP_VER_MAJOR   = 2;
static constexpr uint16_t PCAP_VER_MINOR   = 4;
static constexpr uint32_t PCAP_SNAPLEN     = 65535;
static constexpr uint32_t PCAP_LINK_80211  = 105; // LINKTYPE_IEEE802_11

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest PCAP_MANIFEST = {
    "pcap_logger",
    "PCAP Logger",
    "1.0.0",
    "SkullGate",
    "Wi-Fi 802.11 frame capture to PCAP on SD. Lab Mode required.",
    {"wifi_scan", "wifi_promiscuous", "sd_write", "ui"},
    true,  // Lab Mode required
    true   // valid
};

// ── Global promiscuous callback ───────────────────────────────────────────────
// Called from Wi-Fi interrupt context (IRAM). Must be fast and non-blocking.
static void IRAM_ATTR _promiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    // Only capture management frames to keep volume manageable and legal.
    // Full promiscuous (data frames) would capture traffic from all nearby
    // devices regardless of ownership — always avoid that.
    if (type != WIFI_PKT_MGMT) return;

    const wifi_promiscuous_pkt_t* pkt =
        reinterpret_cast<const wifi_promiscuous_pkt_t*>(buf);

    uint16_t rawLen = pkt->rx_ctrl.sig_len;
    if (rawLen < 4) return; // Must be at least 4 bytes for FCS strip
    // Strip 4-byte FCS appended by ESP32
    uint16_t frameLen = rawLen - 4;
    if (frameLen > PcapLoggerModule::MAX_FRAME_BYTES) {
        frameLen = PcapLoggerModule::MAX_FRAME_BYTES;
    }

    // Ring buffer — drop frame if full
    uint8_t next = (PcapLoggerModule::s_head + 1) % PcapLoggerModule::CAP_BUF_SIZE;
    if (next == PcapLoggerModule::s_tail) return; // Buffer full

    PcapLoggerModule::enqueueFrame(pkt->payload, frameLen,
                                    pkt->rx_ctrl.channel,
                                    pkt->rx_ctrl.rssi);
}

void PcapLoggerModule::enqueueFrame(const uint8_t* payload, uint16_t len,
                                     uint8_t channel, int8_t rssi) {
    uint8_t next = (s_head + 1) % CAP_BUF_SIZE;
    if (next == s_tail) return; // Buffer full

    CapturedFrame& f = s_ring[s_head];
    memcpy(f.data, payload, len);
    f.len     = len;
    f.ts_ms   = millis();
    f.rssi    = rssi;
    f.channel = channel;

    s_head = next;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

PcapLoggerModule::PcapLoggerModule()
    : _api(nullptr)
    , _manifest(PCAP_MANIFEST)
    , _running(false)
    , _capturing(false)
    , _channel(1)
    , _frameCount(0)
    , _byteCount(0)
    , _screen(nullptr)
    , _lblStatus(nullptr)
    , _lblFrames(nullptr)
    , _lblChannel(nullptr)
    , _btnStart(nullptr)
    , _btnStop(nullptr)
    , _btnChanUp(nullptr)
    , _btnChanDn(nullptr)
{}

PcapLoggerModule::~PcapLoggerModule() {
    if (_capturing) {
        esp_wifi_set_promiscuous(false);
    }
}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool PcapLoggerModule::init(CoreAPI& api) {
    _api = &api;

    // Require Lab Mode — checked again in start(), but good to note early
    if (!_api->isLabMode()) {
        _api->log("pcap_logger", "Lab Mode required — module loaded but inactive");
    }

    _buildScreen();
    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("pcap_logger", _screen);
    }
    _api->log("pcap_logger", "Module initialised");
    return true;
}

void PcapLoggerModule::start() {
    _running = true;
    _frameCount = 0;
    _byteCount  = 0;

    if (_screen) _api->showScreen("pcap_logger");
    _updateStats();
    _api->log("pcap_logger", "Started (tap Start Capture to begin)");
}

void PcapLoggerModule::loop() {
    if (!_running) return;
    if (_capturing) {
        _drainCaptureBuf();
    }
}

void PcapLoggerModule::stop() {
    if (_capturing) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        _capturing = false;
        _api->log("pcap_logger",
                  String("Capture stopped. Frames: ") + _frameCount);
    }
    _running = false;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void PcapLoggerModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "PCAP Logger");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0xAA, 0x00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    // Status
    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Stopped");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_LEFT, 5, 30);

    // Frame counter
    _lblFrames = lv_label_create(_screen);
    lv_label_set_text(_lblFrames, "Frames: 0");
    lv_obj_set_style_text_color(_lblFrames, lv_color_white(), 0);
    lv_obj_align(_lblFrames, LV_ALIGN_TOP_LEFT, 5, 50);

    // Channel selector
    _lblChannel = lv_label_create(_screen);
    lv_label_set_text(_lblChannel, "Channel: 1");
    lv_obj_set_style_text_color(_lblChannel, lv_color_make(0x00, 0xFF, 0xAA), 0);
    lv_obj_align(_lblChannel, LV_ALIGN_TOP_LEFT, 5, 70);

    _btnChanDn = lv_btn_create(_screen);
    lv_obj_set_size(_btnChanDn, 35, 28);
    lv_obj_align(_btnChanDn, LV_ALIGN_TOP_LEFT, 110, 66);
    lv_obj_t* lDn = lv_label_create(_btnChanDn);
    lv_label_set_text(lDn, "-");

    _btnChanUp = lv_btn_create(_screen);
    lv_obj_set_size(_btnChanUp, 35, 28);
    lv_obj_align(_btnChanUp, LV_ALIGN_TOP_LEFT, 150, 66);
    lv_obj_t* lUp = lv_label_create(_btnChanUp);
    lv_label_set_text(lUp, "+");

    // Start button
    _btnStart = lv_btn_create(_screen);
    lv_obj_set_size(_btnStart, 100, 36);
    lv_obj_align(_btnStart, LV_ALIGN_BOTTOM_LEFT, 10, -40);
    lv_obj_set_style_bg_color(_btnStart, lv_color_make(0x00, 0x88, 0x00), 0);
    lv_obj_t* lStart = lv_label_create(_btnStart);
    lv_label_set_text(lStart, "Start Capture");

    // Stop button
    _btnStop = lv_btn_create(_screen);
    lv_obj_set_size(_btnStop, 100, 36);
    lv_obj_align(_btnStop, LV_ALIGN_BOTTOM_RIGHT, -10, -40);
    lv_obj_set_style_bg_color(_btnStop, lv_color_make(0x88, 0x00, 0x00), 0);
    lv_obj_t* lStop = lv_label_create(_btnStop);
    lv_label_set_text(lStop, "Stop");

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 70, 28);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    // Event callbacks
    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* m = static_cast<PcapLoggerModule*>(lv_event_get_user_data(e));
        if (m && m->_api) m->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(_btnChanDn, [](lv_event_t* e) {
        auto* m = static_cast<PcapLoggerModule*>(lv_event_get_user_data(e));
        if (!m || m->_capturing) return;
        if (m->_channel > 1) m->_channel--;
        m->_updateStats();
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(_btnChanUp, [](lv_event_t* e) {
        auto* m = static_cast<PcapLoggerModule*>(lv_event_get_user_data(e));
        if (!m || m->_capturing) return;
        if (m->_channel < 13) m->_channel++;
        m->_updateStats();
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(_btnStart, [](lv_event_t* e) {
        auto* m = static_cast<PcapLoggerModule*>(lv_event_get_user_data(e));
        if (!m || m->_capturing) return;

        // Require Lab Mode
        if (!m->_api->isLabMode()) {
            lv_label_set_text(m->_lblStatus, "Needs Lab Mode!");
            return;
        }
        if (!m->_api->hasPermission("wifi_promiscuous")) {
            lv_label_set_text(m->_lblStatus, "No permission");
            return;
        }

        // Create capture file
        m->_capturePath = "/pcap/capture_" + String(millis()) + ".pcap";
        m->_frameCount  = 0;
        m->_byteCount   = 0;
        m->_writePcapGlobalHeader();

        // Start promiscuous mode on selected channel
        esp_wifi_set_channel(m->_channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous_rx_cb(_promiscCb);
        esp_wifi_set_promiscuous(true);

        m->_capturing = true;
        m->s_head = 0;
        m->s_tail = 0;
        lv_label_set_text(m->_lblStatus, "Capturing...");
        m->_api->log("pcap_logger",
                     "Capture started: " + m->_capturePath +
                     " ch=" + String(m->_channel));
    }, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(_btnStop, [](lv_event_t* e) {
        auto* m = static_cast<PcapLoggerModule*>(lv_event_get_user_data(e));
        if (!m || !m->_capturing) return;

        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        m->_capturing = false;
        lv_label_set_text(m->_lblStatus, "Stopped");
        m->_api->log("pcap_logger",
                     String("Capture stopped. Frames: ") + m->_frameCount);
        m->_updateStats();
    }, LV_EVENT_CLICKED, this);
}

void PcapLoggerModule::_writePcapGlobalHeader() {
    if (!_api->hasPermission("sd_write")) return;

    // Pack the 24-byte global header into a binary string
    // (PCAP is binary; we write raw bytes)
    uint8_t hdr[24];
    uint32_t magic   = PCAP_MAGIC;
    uint16_t vmajor  = PCAP_VER_MAJOR;
    uint16_t vminor  = PCAP_VER_MINOR;
    uint32_t tz      = 0;
    uint32_t sig     = 0;
    uint32_t snaplen = PCAP_SNAPLEN;
    uint32_t link    = PCAP_LINK_80211;

    memcpy(hdr + 0,  &magic,   4);
    memcpy(hdr + 4,  &vmajor,  2);
    memcpy(hdr + 6,  &vminor,  2);
    memcpy(hdr + 8,  &tz,      4);
    memcpy(hdr + 12, &sig,     4);
    memcpy(hdr + 16, &snaplen, 4);
    memcpy(hdr + 20, &link,    4);

    // Write as raw binary via SD appendFile using a String wrapper
    String data;
    data.reserve(24);
    for (int i = 0; i < 24; i++) data += (char)hdr[i];
    _api->writeFile(_capturePath, data, false);
}

void PcapLoggerModule::_drainCaptureBuf() {
    // Process all frames currently in the ring buffer
    while (s_tail != s_head) {
        CapturedFrame& f = s_ring[s_tail];
        s_tail = (s_tail + 1) % CAP_BUF_SIZE;

        if (!_api->hasPermission("sd_write")) continue;

        // Build 16-byte PCAP packet record header
        uint32_t ts_sec  = f.ts_ms / 1000;
        uint32_t ts_usec = (f.ts_ms % 1000) * 1000;
        uint32_t incl    = f.len;
        uint32_t orig    = f.len;

        uint8_t pkthdr[16];
        memcpy(pkthdr + 0,  &ts_sec,  4);
        memcpy(pkthdr + 4,  &ts_usec, 4);
        memcpy(pkthdr + 8,  &incl,    4);
        memcpy(pkthdr + 12, &orig,    4);

        // Append header + frame data to PCAP file
        String rec;
        rec.reserve(16 + f.len);
        for (int i = 0; i < 16; i++) rec += (char)pkthdr[i];
        for (int i = 0; i < f.len; i++) rec += (char)f.data[i];
        _api->writeFile(_capturePath, rec, true /*append*/);

        _frameCount++;
        _byteCount += f.len;
    }

    // Update stats display every 10 frames
    if (_frameCount % 10 == 0) {
        _updateStats();
    }
}

void PcapLoggerModule::_updateStats() {
    if (_lblFrames) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Frames: %u", _frameCount);
        lv_label_set_text(_lblFrames, buf);
    }
    if (_lblChannel) {
        char buf[20];
        snprintf(buf, sizeof(buf), "Channel: %d", _channel);
        lv_label_set_text(_lblChannel, buf);
    }
}

} // namespace skullgate
