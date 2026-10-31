#pragma once

// Heltec Vision Master E290 e-ink UI (compiled only with BOARD_HELTEC_E290)

#if defined(BOARD_HELTEC_E290)

#include <heltec-eink-modules.h>

static EInkDisplay_VisionMasterE290 g_eink;
static uint32_t g_rx_count = 0;
static bool g_eink_ready = false;

static void einkBegin() {
  // Vext already enabled in setup(); library also manages it
  // landscape() is rotation 1; 3 = same landscape orientation rotated 180°
  g_eink.setRotation(3);
  g_eink.setFont(NULL);
  g_eink.clearMemory();
  g_eink.setCursor(4, 16);
  g_eink.println(F("LoRa RX ready"));
  g_eink.setCursor(4, 36);
  g_eink.println(F("433.775 MHz  SF7"));
  g_eink.setCursor(4, 56);
  g_eink.println(F("waiting..."));
  g_eink.update();
  g_eink.fastmodeOn();
  g_eink_ready = true;
}

static void einkShowRx(const uint8_t* data, size_t len, float rssi, float snr) {
  if (!g_eink_ready) return;
  g_rx_count++;

  char asc[64];
  size_t n = len < sizeof(asc) - 1 ? len : sizeof(asc) - 1;
  for (size_t i = 0; i < n; i++) {
    char c = (char)data[i];
    asc[i] = (c >= 32 && c < 127) ? c : '.';
  }
  asc[n] = 0;

  char hex[48];
  size_t h = 0;
  const size_t hex_bytes = len < 12 ? len : 12;
  for (size_t i = 0; i < hex_bytes && h + 3 < sizeof(hex); i++) {
    h += snprintf(hex + h, sizeof(hex) - h, "%02X%s", data[i], (i + 1 < hex_bytes) ? " " : "");
  }
  if (len > hex_bytes && h + 3 < sizeof(hex)) {
    snprintf(hex + h, sizeof(hex) - h, "..");
  }

  g_eink.clearMemory();
  g_eink.setCursor(4, 14);
  g_eink.printf("#%lu  RSSI %.0f  SNR %.1f", (unsigned long)g_rx_count, rssi, snr);
  g_eink.setCursor(4, 34);
  g_eink.printf("%u B", (unsigned)len);
  g_eink.setCursor(4, 54);
  g_eink.print(asc);
  g_eink.setCursor(4, 74);
  g_eink.print(hex);
  g_eink.update();
}

#else

static inline void einkBegin() {}
static inline void einkShowRx(const uint8_t*, size_t, float, float) {}

#endif
