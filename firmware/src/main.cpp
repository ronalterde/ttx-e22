#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include "board_pins.h"
#include "eink_ui.h"

#if defined(BOARD_HELTEC_E290)
// LoRa on FSPI; e-ink library uses a separate HSPI instance
static SPIClass loraSpi(FSPI);
SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY, loraSpi);
#else
SX1268 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);
#endif

// Must match on both ends. E22 is 410–493 MHz — Heltec needs 433 MHz HW variant.
static float g_freq_mhz = 433.775f;
static float g_bw_khz   = 125.0f;
static uint8_t g_sf     = 7;
static uint8_t g_cr     = 5;   // 4/5
static int8_t g_power   = 10;

static volatile bool g_rx_flag = false;
static String g_line;

#if defined(BOARD_E22)
static const char* kBeaconPrefix = "DL6SR";
static const uint32_t kBeaconIntervalMs = 30000;
static bool g_beacon_on = true;
static uint32_t g_last_beacon_ms = 0;
static uint32_t g_beacon_seq = 0;
#endif

static void setRxFlag() { g_rx_flag = true; }

static void printHelp() {
  Serial.println(F(
    "\nCommands:\n"
    "  help                 this text\n"
    "  status               radio / link params\n"
    "  freq <MHz>           e.g. freq 433.775\n"
    "  bw <kHz>             7.8 .. 500  (e.g. 125)\n"
    "  sf <7..12>\n"
    "  cr <5..8>            coding rate 4/5 .. 4/8\n"
    "  pwr <dBm>            chip TX power (-9 .. 22)\n"
    "  tx <text>            send UTF-8 / ASCII bytes\n"
    "  txhex <aa bb ..>     send raw hex bytes\n"
    "  rx                   (re)enter continuous receive\n"
#if defined(BOARD_E22)
    "  pause                stop DL6SR beacon\n"
    "  resume               start DL6SR beacon (30s)\n"
#endif
  ));
}

static void printStatus() {
  Serial.printf("board=%s\n", BOARD_NAME);
  Serial.printf("freq=%.3f MHz  bw=%.1f kHz  sf=%u  cr=4/%u  pwr=%d dBm\n",
                g_freq_mhz, g_bw_khz, g_sf, g_cr, g_power);
  Serial.printf("pins NSS=%d DIO1=%d RST=%d BUSY=%d SCK=%d MISO=%d MOSI=%d\n",
                PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY,
                PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);
#if LORA_HAS_RX_TX_EN
  Serial.printf("RF switch RXEN=%d TXEN=%d\n", PIN_LORA_RXEN, PIN_LORA_TXEN);
#else
  Serial.println(F("RF switch: DIO2"));
#endif
#if defined(BOARD_E22)
  Serial.printf("beacon=%s  %s #<n>  every %lu ms  next=#%lu\n",
                g_beacon_on ? "on" : "paused", kBeaconPrefix,
                (unsigned long)kBeaconIntervalMs,
                (unsigned long)(g_beacon_seq + 1));
#endif
}

static int16_t applyModem() {
  int16_t st;
  st = radio.setFrequency(g_freq_mhz);
  if (st != RADIOLIB_ERR_NONE) return st;
  st = radio.setBandwidth(g_bw_khz);
  if (st != RADIOLIB_ERR_NONE) return st;
  st = radio.setSpreadingFactor(g_sf);
  if (st != RADIOLIB_ERR_NONE) return st;
  st = radio.setCodingRate(g_cr);
  if (st != RADIOLIB_ERR_NONE) return st;
  st = radio.setOutputPower(g_power);
  if (st != RADIOLIB_ERR_NONE) return st;
  return RADIOLIB_ERR_NONE;
}

static int16_t startRx() {
  g_rx_flag = false;
  return radio.startReceive();
}

static void printPayload(const uint8_t* data, size_t len, float rssi, float snr) {
  Serial.printf("RX %u B  RSSI=%.1f dBm  SNR=%.1f dB\n  hex:", (unsigned)len, rssi, snr);
  for (size_t i = 0; i < len; i++) {
    Serial.printf(" %02X", data[i]);
  }
  Serial.print(F("\n  asc: "));
  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    Serial.print((c >= 32 && c < 127) ? c : '.');
  }
  Serial.println();
}

static bool parseHexBytes(const String& s, uint8_t* out, size_t maxLen, size_t* outLen) {
  *outLen = 0;
  size_t i = 0;
  while (i < s.length()) {
    while (i < s.length() && isSpace(s[i])) i++;
    if (i >= s.length()) break;
    if (*outLen >= maxLen) return false;
    if (i + 1 >= s.length() || !isxdigit(s[i]) || !isxdigit(s[i + 1])) return false;
    char tmp[3] = { (char)s[i], (char)s[i + 1], 0 };
    out[(*outLen)++] = (uint8_t)strtoul(tmp, nullptr, 16);
    i += 2;
  }
  return *outLen > 0;
}

static void sendBytes(const uint8_t* data, size_t len) {
  if (len == 0) {
    Serial.println(F("empty payload"));
    return;
  }
  if (len > 253) {
    Serial.println(F("max 253 bytes"));
    return;
  }

  Serial.printf("TX %u B ... ", (unsigned)len);
  int16_t st = radio.transmit(const_cast<uint8_t*>(data), len);
  if (st == RADIOLIB_ERR_NONE) {
    Serial.println(F("ok"));
  } else {
    Serial.printf("fail %d\n", st);
  }

  st = startRx();
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("RX re-arm fail %d\n", st);
  }
}

static void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;

  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String arg = (sp < 0) ? String() : line.substring(sp + 1);
  cmd.toLowerCase();
  arg.trim();

  if (cmd == "help" || cmd == "?") {
    printHelp();
    return;
  }
  if (cmd == "status") {
    printStatus();
    return;
  }
  if (cmd == "rx") {
    int16_t st = startRx();
    Serial.printf("RX %s (%d)\n", st == RADIOLIB_ERR_NONE ? "on" : "fail", st);
    return;
  }
  if (cmd == "freq") {
    float v = arg.toFloat();
    // SX126x can tune broadly; antenna match is the real limit
    if (v < 150.0f || v > 960.0f) {
      Serial.println(F("freq out of SX126x range 150..960 MHz"));
      return;
    }
    g_freq_mhz = v;
    int16_t st = applyModem();
    if (st == RADIOLIB_ERR_NONE) st = startRx();
    Serial.printf("freq=%.3f  st=%d\n", g_freq_mhz, st);
    return;
  }
  if (cmd == "bw") {
    g_bw_khz = arg.toFloat();
    int16_t st = applyModem();
    if (st == RADIOLIB_ERR_NONE) st = startRx();
    Serial.printf("bw=%.1f  st=%d\n", g_bw_khz, st);
    return;
  }
  if (cmd == "sf") {
    g_sf = (uint8_t)arg.toInt();
    int16_t st = applyModem();
    if (st == RADIOLIB_ERR_NONE) st = startRx();
    Serial.printf("sf=%u  st=%d\n", g_sf, st);
    return;
  }
  if (cmd == "cr") {
    g_cr = (uint8_t)arg.toInt();
    int16_t st = applyModem();
    if (st == RADIOLIB_ERR_NONE) st = startRx();
    Serial.printf("cr=4/%u  st=%d\n", g_cr, st);
    return;
  }
  if (cmd == "pwr") {
    g_power = (int8_t)arg.toInt();
    int16_t st = applyModem();
    if (st == RADIOLIB_ERR_NONE) st = startRx();
    Serial.printf("pwr=%d  st=%d\n", g_power, st);
    return;
  }
  if (cmd == "tx") {
    sendBytes(reinterpret_cast<const uint8_t*>(arg.c_str()), arg.length());
    return;
  }
  if (cmd == "txhex") {
    uint8_t buf[253];
    size_t n = 0;
    if (!parseHexBytes(arg, buf, sizeof(buf), &n)) {
      Serial.println(F("usage: txhex aa bb cc"));
      return;
    }
    sendBytes(buf, n);
    return;
  }
#if defined(BOARD_E22)
  if (cmd == "pause") {
    g_beacon_on = false;
    Serial.println(F("beacon paused"));
    return;
  }
  if (cmd == "resume") {
    g_beacon_on = true;
    g_last_beacon_ms = 0;  // send on next loop tick
    Serial.println(F("beacon resumed"));
    return;
  }
#endif

  Serial.println(F("unknown — try help"));
}

static void haltForever(const char* why, int16_t code) {
  Serial.printf("HALT: %s (code %d)\n", why, code);
  Serial.println(F("Check power, antenna match band, SPI. Heartbeat every 2s ..."));
  Serial.flush();
  while (true) {
    Serial.print('.');
    Serial.flush();
    delay(2000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.printf("%s  raw LoRa\n", BOARD_NAME);
#if defined(BOARD_HELTEC_E290)
  Serial.println(F("NOTE: E22 TX is 433 MHz — Heltec must be 433 MHz SKU (not 868)."));
#endif
  Serial.flush();

  pinMode(PIN_USER_BTN, INPUT_PULLUP);

#if defined(BOARD_HELTEC_E290)
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, HIGH);  // power LoRa / e-ink
  delay(50);
  Serial.println(F("e-ink init..."));
  Serial.flush();
  einkBegin();
  Serial.println(F("e-ink OK"));
#endif

#if LORA_HAS_RX_TX_EN
  pinMode(PIN_LORA_RXEN, OUTPUT);
  pinMode(PIN_LORA_TXEN, OUTPUT);
  digitalWrite(PIN_LORA_RXEN, HIGH);
  digitalWrite(PIN_LORA_TXEN, LOW);
#endif

  pinMode(PIN_LORA_BUSY, INPUT);
  Serial.printf("BUSY=%d\n", digitalRead(PIN_LORA_BUSY));

#if defined(BOARD_HELTEC_E290)
  loraSpi.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
#else
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
#endif
  delay(20);

#if LORA_HAS_RX_TX_EN
  radio.setRfSwitchPins(PIN_LORA_RXEN, PIN_LORA_TXEN);
#endif
  radio.setDio2AsRfSwitch(LORA_DIO2_AS_RF_SWITCH);

  Serial.println(F("radio.begin TCXO 1.8V ..."));
  Serial.flush();
  int16_t st = radio.begin(g_freq_mhz, g_bw_khz, g_sf, g_cr,
                           RADIOLIB_SX126X_SYNC_WORD_PRIVATE, g_power,
                           8, 1.8, false);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("TCXO begin failed: %d — retry without TCXO\n", st);
    st = radio.begin(g_freq_mhz, g_bw_khz, g_sf, g_cr,
                     RADIOLIB_SX126X_SYNC_WORD_PRIVATE, g_power,
                     8, 0.0, false);
  }
  if (st != RADIOLIB_ERR_NONE) {
    haltForever("radio.begin / chip not found (-2 = SPI/power)", st);
  }

  Serial.println(F("radio OK"));
  radio.setPacketReceivedAction(setRxFlag);

  st = startRx();
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("startReceive failed: %d\n", st);
  }

  printStatus();
  printHelp();
#if defined(BOARD_E22)
  Serial.println(F("beacon DL6SR #<n> every 30s — pause / resume"));
  g_last_beacon_ms = millis();  // first TX after one full interval
#else
  Serial.println(F("listening — type tx hello"));
#endif
  Serial.flush();
}

void loop() {
#if defined(BOARD_E22)
  if (g_beacon_on && (millis() - g_last_beacon_ms) >= kBeaconIntervalMs) {
    g_last_beacon_ms = millis();
    g_beacon_seq++;
    char msg[24];
    const int n = snprintf(msg, sizeof(msg), "%s #%lu", kBeaconPrefix,
                           (unsigned long)g_beacon_seq);
    if (n > 0) {
      sendBytes(reinterpret_cast<const uint8_t*>(msg), (size_t)n);
    }
  }
#endif

  if (g_rx_flag) {
    g_rx_flag = false;
    uint8_t buf[256];
    const size_t len = radio.getPacketLength(true);
    const size_t n = len > sizeof(buf) ? sizeof(buf) : len;
    const int16_t st = radio.readData(buf, n);
    if (st == RADIOLIB_ERR_NONE) {
      const float rssi = radio.getRSSI();
      const float snr = radio.getSNR();
      printPayload(buf, n, rssi, snr);
      einkShowRx(buf, n, rssi, snr);
    } else if (st == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial.println(F("RX CRC error"));
    } else {
      Serial.printf("RX err %d (len=%u)\n", st, (unsigned)len);
    }
    startRx();
  }

  static bool swPrev = true;
  bool sw = digitalRead(PIN_USER_BTN);
  if (swPrev && !sw) {
    delay(30);
    if (!digitalRead(PIN_USER_BTN)) {
      const char* msg = "BTN";
      sendBytes(reinterpret_cast<const uint8_t*>(msg), 3);
    }
  }
  swPrev = sw;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(g_line);
      g_line = "";
    } else if (g_line.length() < 512) {
      g_line += c;
    }
  }
}
