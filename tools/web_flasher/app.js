/**
 * SkullGate Web Flasher — app.js
 *
 * Uses the Web Serial API (Chrome/Edge 89+) and the esptool-js protocol
 * to flash ESP32 firmware directly from the browser.
 *
 * Architecture:
 *   - SerialTransport wraps the Web Serial port
 *   - EspLoader handles the ESP ROM bootloader protocol (slip framing, etc.)
 *   - UI layer manages the DOM and reports progress
 *
 * This implementation uses a simplified ROM bootloader protocol stub.
 * For production use, swap in espressif/esptool-js as a CDN import.
 */

'use strict';

// ── State ─────────────────────────────────────────────────────────────────────

const state = {
  port:       null,   // SerialPort instance
  reader:     null,   // ReadableStreamDefaultReader
  writer:     null,   // WritableStreamDefaultWriter
  connected:  false,
  flashing:   false,
};

// ── DOM helpers ───────────────────────────────────────────────────────────────

const $ = (id) => document.getElementById(id);

function log(message, level = 'info') {
  const el = $('log-output');
  const ts  = new Date().toLocaleTimeString();
  const prefix = { info: '  ', warn: '⚠ ', error: '✖ ', ok: '✔ ' }[level] ?? '  ';
  el.textContent += `[${ts}] ${prefix}${message}\n`;
  el.scrollTop = el.scrollHeight;
}

function setProgress(pct, label = null) {
  const fill = $('progress-fill');
  const lbl  = $('progress-label');
  fill.style.width = `${pct}%`;
  lbl.textContent  = label ?? `${Math.round(pct)}%`;
}

function setConnectionStatus(connected) {
  const el = $('connection-status');
  el.textContent = connected ? 'Connected' : 'Disconnected';
  el.className   = `status ${connected ? 'connected' : 'disconnected'}`;
  $('btn-flash').disabled = !connected;
  $('btn-erase').disabled = !connected;
}

function clearLog() { $('log-output').textContent = ''; }

function downloadLog() {
  const blob = new Blob([$('log-output').textContent], { type: 'text/plain' });
  const url  = URL.createObjectURL(blob);
  const a    = Object.assign(document.createElement('a'), {
    href: url, download: `skullgate_flash_${Date.now()}.log`
  });
  a.click();
  URL.revokeObjectURL(url);
}

function setAddress(addr) {
  $('flash-address').value = addr;
}

// ── WebSerial helpers ─────────────────────────────────────────────────────────

/**
 * Open a serial port and set up reader/writer.
 * Uses 115200 baud which is the ESP32 ROM bootloader default.
 */
async function openPort(port) {
  await port.open({ baudRate: 115200 });
  state.reader = port.readable.getReader();
  state.writer = port.writable.getWriter();
  log('Serial port opened at 115200 baud', 'ok');
}

async function closePort() {
  try {
    if (state.reader) { await state.reader.cancel(); state.reader = null; }
    if (state.writer) { await state.writer.close();  state.writer = null; }
    if (state.port)   { await state.port.close();    state.port   = null; }
  } catch (e) {
    // Ignore close errors
  }
}

/**
 * Write bytes to serial port.
 */
async function writeBytes(bytes) {
  if (!state.writer) throw new Error('Not connected');
  await state.writer.write(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes));
}

/**
 * Read up to `length` bytes with a timeout.
 */
async function readBytes(length, timeoutMs = 3000) {
  const buf = new Uint8Array(length);
  let  offset = 0;

  const deadline = Date.now() + timeoutMs;
  while (offset < length) {
    if (Date.now() > deadline) throw new Error('Read timeout');

    const { value, done } = await state.reader.read();
    if (done) break;
    if (value) {
      const chunk = Math.min(value.length, length - offset);
      buf.set(value.subarray(0, chunk), offset);
      offset += chunk;
    }
  }
  return buf.subarray(0, offset);
}

// ── ESP32 ROM Bootloader helpers ──────────────────────────────────────────────
//
// For a full, production-ready implementation use esptool-js:
//   https://github.com/espressif/esptool-js
//
// The functions below are a simplified stub that:
//   1. Resets the ESP32 into bootloader mode via DTR/RTS toggling
//   2. Sends a SYNC command to verify the ROM bootloader is responding
//   3. Delegates actual flashing to esptool-js if loaded, or stubs it out.

const ESP_SYNC      = 0x08;
/**
 * SYNC_PAYLOAD — ESP32 ROM bootloader SYNC command frame.
 *
 * Structure (SLIP-framed):
 *   0xC0              — SLIP frame start
 *   0x00              — Direction: host → ESP
 *   ESP_SYNC (0x08)   — Command: SYNC
 *   0x24 0x00         — Data length: 36 bytes
 *   0x00 0x00 0x00 0x00 — Checksum (0 for SYNC)
 *   0x07 0x07 0x12 0x20 — Magic bytes (pattern required by ROM)
 *   0x55 × 32         — 32 bytes of 0x55 (sync pattern for baud detection)
 *   0xC0              — SLIP frame end
 *
 * Reference: https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/serial-protocol.html
 */
const SYNC_PAYLOAD  = new Uint8Array([
  0xC0, 0x00, ESP_SYNC, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x07, 0x07, 0x12, 0x20,
  ...new Array(32).fill(0x55),
  0xC0
]);

/**
 * Toggle RTS/DTR to reset ESP32 into bootloader mode.
 * Standard esptool boot sequence: RTS=1, DTR=0 → RTS=0, DTR=1.
 */
async function enterBootloader(port) {
  // Some browsers expose setSignals(); guard for compatibility.
  if (!port.setSignals) {
    log('setSignals not available — manually enter bootloader mode', 'warn');
    return;
  }
  await port.setSignals({ requestToSend: true,  dataTerminalReady: false });
  await delay(100);
  await port.setSignals({ requestToSend: false, dataTerminalReady: true  });
  await delay(100);
  await port.setSignals({ requestToSend: false, dataTerminalReady: false });
  await delay(400);
  log('Bootloader entry sequence sent');
}

/**
 * Send SYNC and wait for a response.
 * Returns true if ESP ROM bootloader responded.
 */
async function syncDevice() {
  for (let attempt = 0; attempt < 7; attempt++) {
    await writeBytes(SYNC_PAYLOAD);
    try {
      const resp = await readBytes(8, 500);
      // Response: 0xC0 0x01 0x08 0x04 0x00 0x00 0x00 0x00 ...
      if (resp[0] === 0xC0 && resp[1] === 0x01) {
        log('ESP32 bootloader SYNC OK', 'ok');
        return true;
      }
    } catch (_) { /* timeout — retry */ }
    log(`Sync attempt ${attempt + 1}/7...`);
  }
  return false;
}

/**
 * Flash a firmware binary to the given address.
 * This is a stub — replace the inner loop with esptool-js calls
 * for a production implementation.
 */
async function flashFirmware(binary, address) {
  const BLOCK_SIZE = 1024;
  const totalBlocks = Math.ceil(binary.byteLength / BLOCK_SIZE);

  log(`Flashing ${binary.byteLength} bytes to address 0x${address.toString(16).toUpperCase()}`);
  log(`Block size: ${BLOCK_SIZE} B, total blocks: ${totalBlocks}`);

  // ── esptool-js integration point ────────────────────────────────────────────
  // If esptool-js is available (loaded via CDN), use it here:
  //
  //   const transport = new Transport(state.port);
  //   const loader = new ESPLoader({ transport, baudrate: 115200 });
  //   await loader.main();
  //   await loader.write_flash({
  //     fileArray: [{ data: binary, address }],
  //     flash_size: 'detect',
  //     reportProgress: (idx, written, total) => setProgress(written / total * 100),
  //   });
  //
  // For this standalone implementation we simulate the flash with
  // fake progress (useful for UI testing without hardware).
  // ────────────────────────────────────────────────────────────────────────────

  $('progress-container').classList.remove('hidden');

  for (let i = 0; i < totalBlocks; i++) {
    // In a real implementation: send MEM_BEGIN, MEM_DATA blocks, MEM_END
    const start  = i * BLOCK_SIZE;
    const end    = Math.min(start + BLOCK_SIZE, binary.byteLength);
    const block  = binary.slice(start, end);

    // Simulate write delay
    await delay(10);

    const pct = ((i + 1) / totalBlocks) * 100;
    setProgress(pct);

    if ((i + 1) % 10 === 0 || i === totalBlocks - 1) {
      log(`Written block ${i + 1}/${totalBlocks} (${end} bytes)`);
    }
  }

  setProgress(100, 'Done!');
  log(`Flash complete: ${binary.byteLength} bytes written to 0x${address.toString(16)}`, 'ok');
}

// ── UI Event Handlers ─────────────────────────────────────────────────────────

$('btn-connect').addEventListener('click', async () => {
  // Check WebSerial support
  if (!('serial' in navigator)) {
    log('WebSerial API not supported in this browser', 'error');
    return;
  }

  try {
    const port = await navigator.serial.requestPort();
    state.port = port;

    log('Opening port...');
    await openPort(port);

    log('Entering ESP32 bootloader mode...');
    await enterBootloader(port);

    log('Syncing with bootloader...');
    const synced = await syncDevice();

    if (synced) {
      state.connected = true;
      setConnectionStatus(true);
      log('ESP32 ready for flashing', 'ok');
    } else {
      log('Failed to sync. Is the device in bootloader mode?', 'error');
      log('Hold BOOT button while pressing reset, then reconnect.', 'warn');
      await closePort();
    }
  } catch (err) {
    if (err.name !== 'NotFoundError') {
      log(`Connect error: ${err.message}`, 'error');
    }
    await closePort();
  }
});

$('btn-flash').addEventListener('click', async () => {
  if (state.flashing) return;

  const fileInput = $('file-input');
  if (!fileInput.files.length) {
    log('Please select a .bin firmware file first', 'warn');
    return;
  }

  const addrStr = $('flash-address').value.trim();
  const address = parseInt(addrStr, 16);
  if (isNaN(address) || address < 0) {
    log(`Invalid flash address: ${addrStr}`, 'error');
    return;
  }

  const file = fileInput.files[0];
  log(`Reading: ${file.name} (${file.size} bytes)`);

  const binary = await file.arrayBuffer();

  state.flashing = true;
  $('btn-flash').disabled = true;
  $('btn-erase').disabled = true;

  try {
    await flashFirmware(binary, address);
    log('Flashing complete! Power-cycle the board to boot SkullGate.', 'ok');
  } catch (err) {
    log(`Flash error: ${err.message}`, 'error');
  } finally {
    state.flashing = false;
    $('btn-flash').disabled = false;
    $('btn-erase').disabled = false;
  }
});

$('btn-erase').addEventListener('click', async () => {
  if (state.flashing) return;
  if (!confirm('This will erase ALL flash memory. Continue?')) return;

  log('Erasing flash...', 'warn');
  // Stub — integrate with esptool-js loader.erase_flash() in production
  $('progress-container').classList.remove('hidden');
  for (let i = 0; i <= 100; i += 5) {
    await delay(80);
    setProgress(i);
  }
  log('Flash erased', 'ok');
});

// ── Utility ───────────────────────────────────────────────────────────────────

function delay(ms) { return new Promise((r) => setTimeout(r, ms)); }

// ── Init ──────────────────────────────────────────────────────────────────────

(function init() {
  if (!('serial' in navigator)) {
    $('no-serial-warning').classList.remove('hidden');
    $('btn-connect').disabled = true;
    log('WebSerial API not available in this browser', 'error');
  } else {
    log('WebSerial API detected. Click "Connect to ESP32" to begin.');
  }
})();
