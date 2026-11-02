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

static constexpr int16_t kPayloadBaseline = 56;

// RSSI history: X = sample slot, Y = RSSI; skips = seq gaps (no line)
static constexpr size_t kRssiHist = 56;
static constexpr int16_t kRssiPlotMin = -120;
static constexpr int16_t kRssiPlotMax = -20;
static constexpr int8_t kRssiSkip = -128;  // sentinel: missing sample
static int8_t g_rssi_hist[kRssiHist] = {};
static size_t g_rssi_hist_n = 0;
static bool g_have_beacon_seq = false;
static uint32_t g_last_beacon_seq = 0;

static char g_eink_asc[64] = "";
static float g_eink_rssi = 0;
static float g_eink_snr = 0;

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

static void einkHistPush(int8_t v) {
  if (g_rssi_hist_n < kRssiHist) {
    g_rssi_hist[g_rssi_hist_n++] = v;
  } else {
    memmove(g_rssi_hist, g_rssi_hist + 1, kRssiHist - 1);
    g_rssi_hist[kRssiHist - 1] = v;
  }
}

static void einkPushRssi(float rssi) {
  int v = (int)rssi;
  if (v < -127) v = -127;  // reserve -128 for skip
  if (v > 127) v = 127;
  einkHistPush((int8_t)v);
}

static void einkPushSkips(uint32_t n) {
  if (n > kRssiHist) n = kRssiHist;
  for (uint32_t i = 0; i < n; i++) {
    einkHistPush(kRssiSkip);
  }
}

// Parse "… #123 …" (DL6SR beacon). Returns true if found.
static bool einkParseBeaconSeq(const char* asc, uint32_t* out_seq) {
  const char* p = strchr(asc, '#');
  if (!p || !p[1] || !isdigit((unsigned char)p[1])) return false;
  *out_seq = (uint32_t)strtoul(p + 1, nullptr, 10);
  return true;
}

static int16_t einkRssiToY(int16_t rssi, int16_t gy, int16_t gh) {
  if (rssi < kRssiPlotMin) rssi = kRssiPlotMin;
  if (rssi > kRssiPlotMax) rssi = kRssiPlotMax;
  const int16_t span = kRssiPlotMax - kRssiPlotMin;
  const int16_t inner = gh - 2;
  return (int16_t)(gy + gh - 1 -
                   ((int32_t)(rssi - kRssiPlotMin) * inner) / span);
}

static void einkDrawRssiGraph() {
  const int16_t gx = 4;
  const int16_t gy = 78;
  const int16_t gw = (int16_t)(g_eink.width() - 8);
  const int16_t gh = 46;

  g_eink.drawRect(gx, gy, gw, gh, BLACK);
  g_eink.setFont(NULL);
  g_eink.setCursor(gx + 2, gy + 8);
  g_eink.print(F("RSSI"));
  g_eink.setCursor(gx + gw - 28, gy + 8);
  g_eink.printf("%d", (int)kRssiPlotMax);
  g_eink.setCursor(gx + gw - 34, gy + gh - 2);
  g_eink.printf("%d", (int)kRssiPlotMin);
  g_eink.setFont(&FreeSans9pt7b);

  if (g_rssi_hist_n < 1) return;

  // Horizontal padding so first/last points are clear of the frame
  const int16_t pad_x = 14;
  const int16_t x0 = gx + pad_x;
  const int16_t plot_w = gw - 2 * pad_x;
  int16_t prev_x = -1;
  int16_t prev_y = -1;
  int16_t last_good_x = -1;
  int16_t last_good_y = -1;

  for (size_t i = 0; i < g_rssi_hist_n; i++) {
    const int16_t x =
        (int16_t)(x0 + ((int32_t)i * (plot_w - 1)) / (int32_t)(kRssiHist - 1));
    if (g_rssi_hist[i] == kRssiSkip) {
      // Vertical tick = skipped sample; break polyline
      g_eink.drawLine(x, gy + 3, x, gy + gh - 3, BLACK);
      prev_x = -1;
      prev_y = -1;
      continue;
    }
    const int16_t y = einkRssiToY(g_rssi_hist[i], gy, gh);
    if (prev_x >= 0) {
      g_eink.drawLine(prev_x, prev_y, x, y, BLACK);
    }
    prev_x = x;
    prev_y = y;
    last_good_x = x;
    last_good_y = y;
  }
  if (last_good_x >= 0) {
    g_eink.fillRect(last_good_x - 1, last_good_y - 1, 3, 3, BLACK);
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

  g_eink.clearMemory();
  g_eink.setTextColor(BLACK);
  g_eink.setFont(&FreeSans9pt7b);
  g_eink.setCursor(4, 16);
  g_eink.printf("up %s  last %s", up, ago);

  if (g_eink_have_rx) {
    g_eink.setCursor(4, 34);
    g_eink.printf("#%lu %.0fdBm SNR%.1f", (unsigned long)g_rx_count,
                  g_eink_rssi, g_eink_snr);
    g_eink.setCursor(4, kPayloadBaseline);
    g_eink.printf("%s", g_eink_asc);
    einkDrawRssiGraph();
  } else {
    g_eink.setCursor(4, 48);
    g_eink.println(F("433.775 MHz  SF7"));
    g_eink.setCursor(4, 72);
    g_eink.println(F("waiting for beacon..."));
  }
  g_eink.update();
  g_eink_last_paint_ms = now;
}

static void einkBegin() {
  g_eink_start_ms = millis();
  g_eink_last_rx_ms = g_eink_start_ms;
  g_eink_have_rx = false;
  g_rssi_hist_n = 0;
  g_have_beacon_seq = false;
  g_last_beacon_seq = 0;

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
  g_eink_rssi = rssi;
  g_eink_snr = snr;

  size_t n = len < sizeof(g_eink_asc) - 1 ? len : sizeof(g_eink_asc) - 1;
  for (size_t i = 0; i < n; i++) {
    char c = (char)data[i];
    g_eink_asc[i] = (c >= 32 && c < 127) ? c : '.';
  }
  g_eink_asc[n] = 0;

  uint32_t seq = 0;
  if (einkParseBeaconSeq(g_eink_asc, &seq)) {
    if (g_have_beacon_seq && seq > g_last_beacon_seq + 1) {
      einkPushSkips(seq - g_last_beacon_seq - 1);
    }
    g_have_beacon_seq = true;
    g_last_beacon_seq = seq;
  }

  einkPushRssi(rssi);
  einkPaint();
}

static void einkTick() {
  if (!g_eink_ready) return;
  if ((millis() - g_eink_last_paint_ms) < 1000UL) return;
  einkPaint();
}

#else

static inline void einkBegin() {}
static inline void einkShowRx(const uint8_t*, size_t, float, float) {}
static inline void einkTick() {}

#endif
