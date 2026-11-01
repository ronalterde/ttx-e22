#pragma once

// Heltec Vision Master E290 e-ink UI (compiled only with BOARD_HELTEC_E290)

#if defined(BOARD_HELTEC_E290)

#include <heltec-eink-modules.h>
#include "Fonts/FreeSans9pt7b.h"

static EInkDisplay_VisionMasterE290 g_eink;
static uint32_t g_rx_count = 0;
static bool g_eink_ready = false;
static uint32_t g_eink_start_ms = 0;
static uint32_t g_eink_last_rx_ms = 0;
static bool g_eink_have_rx = false;
static uint32_t g_eink_last_paint_ms = 0;
static uint32_t g_eink_invert_until_ms = 0;  // payload line inverted until this
static bool g_eink_was_invert = false;

static constexpr uint32_t kEinkInvertMs = 2500;  // 2–3 s highlight on new RX
static constexpr int16_t kPayloadBaseline = 66;
static constexpr int16_t kPayloadBandY = 50;
static constexpr int16_t kPayloadBandH = 24;

// Cached last packet for redraw with live timers
static char g_eink_asc[64] = "";
static char g_eink_hex[48] = "";
static float g_eink_rssi = 0;
static float g_eink_snr = 0;
static unsigned g_eink_len = 0;

static void einkFormatElapsed(char* buf, size_t n, uint32_t elapsed_ms) {
  const uint32_t total_s = elapsed_ms / 1000UL;
  const uint32_t h = total_s / 3600UL;
  const uint32_t m = (total_s / 60UL) % 60UL;
  const uint32_t s = total_s % 60UL;
  if (h > 0) {
    snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m,
             (unsigned long)s);
  } else {
    snprintf(buf, n, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
  }
}

static void einkPaint() {
  if (!g_eink_ready) return;

  const uint32_t now = millis();
  char up[16];
  char ago[16];
  einkFormatElapsed(up, sizeof(up), now - g_eink_start_ms);
  if (g_eink_have_rx) {
    einkFormatElapsed(ago, sizeof(ago), now - g_eink_last_rx_ms);
  } else {
    snprintf(ago, sizeof(ago), "--:--");
  }

  const bool invert_payload =
      g_eink_have_rx && (int32_t)(g_eink_invert_until_ms - now) > 0;

  g_eink.clearMemory();
  g_eink.setTextColor(BLACK);
  // FreeSans9pt7b: Y is baseline (~16 px line spacing)
  g_eink.setCursor(4, 18);
  g_eink.printf("up %s   last %s", up, ago);

  if (g_eink_have_rx) {
    g_eink.setCursor(4, 42);
    g_eink.printf("#%lu  RSSI %.0f  SNR %.1f", (unsigned long)g_rx_count,
                  g_eink_rssi, g_eink_snr);

    if (invert_payload) {
      g_eink.fillRect(0, kPayloadBandY, g_eink.width(), kPayloadBandH, BLACK);
      g_eink.setTextColor(WHITE);
    }
    g_eink.setCursor(4, kPayloadBaseline);
    g_eink.printf("%u B  %s", g_eink_len, g_eink_asc);
    g_eink.setTextColor(BLACK);

    g_eink.setCursor(4, 90);
    g_eink.print(g_eink_hex);
  } else {
    g_eink.setCursor(4, 48);
    g_eink.println(F("433.775 MHz  SF7"));
    g_eink.setCursor(4, 76);
    g_eink.println(F("waiting for beacon..."));
  }
  g_eink.update();
  g_eink_last_paint_ms = now;
  g_eink_was_invert = invert_payload;
}

static void einkBegin() {
  g_eink_start_ms = millis();
  g_eink_last_rx_ms = g_eink_start_ms;
  g_eink_have_rx = false;
  g_eink_invert_until_ms = 0;
  g_eink_was_invert = false;

  // landscape() is rotation 1; 3 = same landscape orientation rotated 180°
  g_eink.setRotation(3);
  g_eink.setFont(&FreeSans9pt7b);
  g_eink_ready = true;
  g_eink.clearMemory();
  einkPaint();
  g_eink.fastmodeOn();
}

static void einkShowRx(const uint8_t* data, size_t len, float rssi, float snr) {
  if (!g_eink_ready) return;
  g_rx_count++;
  g_eink_have_rx = true;
  g_eink_last_rx_ms = millis();
  g_eink_invert_until_ms = g_eink_last_rx_ms + kEinkInvertMs;
  g_eink_rssi = rssi;
  g_eink_snr = snr;
  g_eink_len = (unsigned)len;

  size_t n = len < sizeof(g_eink_asc) - 1 ? len : sizeof(g_eink_asc) - 1;
  for (size_t i = 0; i < n; i++) {
    char c = (char)data[i];
    g_eink_asc[i] = (c >= 32 && c < 127) ? c : '.';
  }
  g_eink_asc[n] = 0;

  size_t h = 0;
  const size_t hex_bytes = len < 12 ? len : 12;
  for (size_t i = 0; i < hex_bytes && h + 3 < sizeof(g_eink_hex); i++) {
    h += snprintf(g_eink_hex + h, sizeof(g_eink_hex) - h, "%02X%s", data[i],
                  (i + 1 < hex_bytes) ? " " : "");
  }
  if (len > hex_bytes && h + 3 < sizeof(g_eink_hex)) {
    snprintf(g_eink_hex + h, sizeof(g_eink_hex) - h, "..");
  } else {
    g_eink_hex[h] = 0;
  }

  einkPaint();
}

// Call from loop(): refresh timers ~1 Hz; also clear invert as soon as it expires.
static void einkTick() {
  if (!g_eink_ready) return;
  const uint32_t now = millis();
  const bool invert_now =
      g_eink_have_rx && (int32_t)(g_eink_invert_until_ms - now) > 0;
  const bool invert_ended = g_eink_was_invert && !invert_now;
  if (!invert_ended && (now - g_eink_last_paint_ms) < 1000UL) return;
  einkPaint();
}

#else

static inline void einkBegin() {}
static inline void einkShowRx(const uint8_t*, size_t, float, float) {}
static inline void einkTick() {}

#endif
