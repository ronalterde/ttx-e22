#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include "board_pins.h"

// E22-400M33S: SX1268 + TCXO 1.8 V (DIO3), RF-Switch über RXEN/TXEN
SX1268 radio = new Module(PIN_E22_NSS, PIN_E22_DIO1, PIN_E22_NRST, PIN_E22_BUSY);

// EU 433-ISM; bei Bedarf per "freq" ändern
static float g_freq_mhz = 433.775f;
static float g_bw_khz   = 125.0f;
static uint8_t g_sf     = 7;
static uint8_t g_cr     = 5;   // 4/5
static int8_t g_power   = 10;  // dBm am Chip; PA des E22 verstärkt weiter

static volatile bool g_rx_flag = false;
static String g_line;

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
  ));
}

static void printStatus() {
  Serial.printf("freq=%.3f MHz  bw=%.1f kHz  sf=%u  cr=4/%u  pwr=%d dBm\n",
                g_freq_mhz, g_bw_khz, g_sf, g_cr, g_power);
  Serial.printf("pins NSS=%d DIO1=%d RST=%d BUSY=%d RXEN=%d TXEN=%d\n",
                PIN_E22_NSS, PIN_E22_DIO1, PIN_E22_NRST, PIN_E22_BUSY,
                PIN_E22_RXEN, PIN_E22_TXEN);
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
    if (v < 410.0f || v > 493.0f) {
      Serial.println(F("freq out of E22 range 410..493 MHz"));
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

  Serial.println(F("unknown — try help"));
}

static void haltForever(const char* why, int16_t code) {
  Serial.printf("HALT: %s (code %d)\n", why, code);
  Serial.println(F("Check: E22 powered (5V), antenna optional for init, SPI pins."));
  Serial.println(F("Press EN/RESET to retry. Heartbeat every 2s ..."));
  Serial.flush();
  while (true) {
    Serial.print('.');
    Serial.flush();
    delay(2000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);  // Zeit, bis der Monitor nach Upload wieder verbunden ist
  Serial.println();
  Serial.println(F("ESP32-E22 V2.1.0  raw LoRa"));
  Serial.flush();

  pinMode(PIN_SW1, INPUT_PULLUP);

  // RF-Switch in bekannten Zustand (RX aktiv), bevor SPI startet
  pinMode(PIN_E22_RXEN, OUTPUT);
  pinMode(PIN_E22_TXEN, OUTPUT);
  digitalWrite(PIN_E22_RXEN, HIGH);
  digitalWrite(PIN_E22_TXEN, LOW);

  pinMode(PIN_E22_BUSY, INPUT);
  Serial.printf("BUSY=%d (expect 0 shortly after reset)\n", digitalRead(PIN_E22_BUSY));

  SPI.begin(PIN_E22_SCK, PIN_E22_MISO, PIN_E22_MOSI, PIN_E22_NSS);
  delay(20);

  radio.setRfSwitchPins(PIN_E22_RXEN, PIN_E22_TXEN);
  radio.setDio2AsRfSwitch(false);

  // E22: TCXO 1.8 V über DIO3 — bei Fehlschlag XTAL-Fallback
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
  Serial.println(F("listening — type tx hello"));
  Serial.flush();
}

void loop() {
  if (g_rx_flag) {
    g_rx_flag = false;
    uint8_t buf[256];
    const size_t len = radio.getPacketLength(true);
    const size_t n = len > sizeof(buf) ? sizeof(buf) : len;
    const int16_t st = radio.readData(buf, n);
    if (st == RADIOLIB_ERR_NONE) {
      printPayload(buf, n, radio.getRSSI(), radio.getSNR());
    } else if (st == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial.println(F("RX CRC error"));
    } else {
      Serial.printf("RX err %d (len=%u)\n", st, (unsigned)len);
    }
    startRx();
  }

  // SW1: short press sends a tiny test burst
  static bool swPrev = true;
  bool sw = digitalRead(PIN_SW1);
  if (swPrev && !sw) {
    delay(30);
    if (!digitalRead(PIN_SW1)) {
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
