#include "ds1820.h"

#include <OneWire.h>
#include <math.h>

namespace {

OneWire* g_ow = nullptr;
uint8_t g_addr[8] = {};
int16_t g_last_raw = 0;
bool g_have_raw = false;
uint32_t g_convert_ms = 750;
float g_offset_c = 0.0f;  // added to reading (serial: tempoff)

constexpr uint8_t kFamilyDs1820 = 0x10;
constexpr uint8_t kFamilyDs18b20 = 0x28;
constexpr uint32_t kConvertMaxMs = 750;

bool isSupportedFamily(uint8_t f) {
  return f == kFamilyDs1820 || f == kFamilyDs18b20;
}

bool scratchpadLooksLike12Bit(const uint8_t sp[9]) {
  const uint8_t cfg = sp[4];
  return cfg == 0x1F || cfg == 0x3F || cfg == 0x5F || cfg == 0x7F;
}

float temp9BitWithCounts(int16_t raw, const uint8_t sp[9]) {
  const int16_t whole = (raw < 0) ? -((-raw) / 2) : (raw / 2);
  const uint8_t count_remain = sp[6];
  const uint8_t count_per_c = sp[7];
  if (count_per_c == 0) {
    return (float)whole;
  }
  return (float)whole - 0.25f +
         (float)(count_per_c - count_remain) / (float)count_per_c;
}

float celsiusFromScratchpad(const uint8_t sp[9], uint8_t family) {
  const int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
  const float t12 = raw / 16.0f;
  const float t9 = temp9BitWithCounts(raw, sp);

  if (family == kFamilyDs18b20 || scratchpadLooksLike12Bit(sp)) {
    return t12;
  }
  if (t9 < -55.0f || t9 > 125.0f ||
      (fabsf(t9) > 85.0f && fabsf(t12) <= 85.0f)) {
    return t12;
  }
  return t9;
}

bool findDevice() {
  if (!g_ow) return false;
  g_ow->reset_search();
  if (!g_ow->search(g_addr)) return false;
  if (OneWire::crc8(g_addr, 7) != g_addr[7]) return false;
  return isSupportedFamily(g_addr[0]);
}

bool readScratchpad(uint8_t sp[9]) {
  if (!g_ow->reset()) return false;
  g_ow->select(g_addr);
  g_ow->write(0xBE);
  for (int i = 0; i < 9; i++) sp[i] = g_ow->read();
  return OneWire::crc8(sp, 8) == sp[8];
}

bool looksLikeFailedConvert(const uint8_t sp[9]) {
  return sp[0] == 0xFF && sp[1] == 0x00;
}

// Many labeled "DS1820" clones leave VDD unbonded — still need strong pull-up.
bool convertThenRead(uint8_t sp[9], uint32_t wait_ms) {
  if (!g_ow->reset()) return false;
  g_ow->select(g_addr);
  g_ow->write(0x44, 1);  // strong pull-up for convert
  delay(wait_ms);
  g_ow->depower();
  return readScratchpad(sp);
}

uint32_t findMinConvertMs() {
  // Prefer shortest strong-pull-up that yields a non-stuck scratchpad
  static const uint32_t kCandidates[] = {94, 150, 200, 300, 400, 500, 750};
  for (uint32_t ms : kCandidates) {
    uint8_t sp[9];
    if (convertThenRead(sp, ms) && !looksLikeFailedConvert(sp)) {
      // Confirm once more
      if (convertThenRead(sp, ms) && !looksLikeFailedConvert(sp)) {
        return ms;
      }
    }
  }
  return kConvertMaxMs;
}

}  // namespace

bool ds1820Begin(int pin) {
  delete g_ow;
  g_ow = new OneWire(pin);
  g_have_raw = false;
  g_convert_ms = kConvertMaxMs;

  if (!findDevice()) {
    Serial.println(F("DS18x20: no 0x10/0x28 on bus (check J5 / wiring)"));
    return false;
  }
  const char* kind = (g_addr[0] == kFamilyDs18b20) ? "DS18B20" : "DS1820/S20";
  Serial.printf("%s: OK  ROM %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", kind,
                g_addr[0], g_addr[1], g_addr[2], g_addr[3], g_addr[4],
                g_addr[5], g_addr[6], g_addr[7]);

  // Probe: external-only convert (no strong pull-up)
  {
    if (!g_ow->reset()) {
      return false;
    }
    g_ow->select(g_addr);
    g_ow->write(0x44, 0);
    delay(kConvertMaxMs);
    uint8_t sp_ext[9];
    const bool ext_ok =
        readScratchpad(sp_ext) && !looksLikeFailedConvert(sp_ext);
    if (ext_ok) {
      Serial.println(F("DS18x20: note — external convert worked without strong pull-up"));
    } else {
      Serial.println(F(
          "DS18x20: VDD present but convert needs strong pull-up "
          "(likely parasite-only clone; VDD pin often unbonded)"));
    }
  }

  g_convert_ms = findMinConvertMs();
  uint8_t sp[9];
  if (!convertThenRead(sp, g_convert_ms) || looksLikeFailedConvert(sp)) {
    Serial.println(F("DS18x20: convert failed"));
    return false;
  }
  g_last_raw = (int16_t)((sp[1] << 8) | sp[0]);
  g_have_raw = true;
  const float t = celsiusFromScratchpad(sp, g_addr[0]) + g_offset_c;
  Serial.printf("DS18x20: convert=%lums  T=%.2f C  (tempoff=%.2f)\n",
                (unsigned long)g_convert_ms, (double)t, (double)g_offset_c);
  return true;
}

bool ds1820ReadCelsius(float* out_c) {
  if (!out_c || !g_ow) return false;

  uint8_t sp[9];
  if (!convertThenRead(sp, g_convert_ms) || looksLikeFailedConvert(sp)) {
    if (!convertThenRead(sp, kConvertMaxMs) || looksLikeFailedConvert(sp)) {
      return false;
    }
    g_convert_ms = kConvertMaxMs;
  }

  g_last_raw = (int16_t)((sp[1] << 8) | sp[0]);
  g_have_raw = true;
  *out_c = celsiusFromScratchpad(sp, g_addr[0]) + g_offset_c;
  return true;
}

bool ds1820LastRaw(int16_t* out_raw) {
  if (!out_raw || !g_have_raw) return false;
  *out_raw = g_last_raw;
  return true;
}

void ds1820SetOffsetC(float offset_c) { g_offset_c = offset_c; }

float ds1820GetOffsetC() { return g_offset_c; }
