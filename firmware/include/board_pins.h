#pragma once

// Board selection via PlatformIO build_flags: BOARD_E22 or BOARD_HELTEC_E290

#if defined(BOARD_HELTEC_E290)

// Heltec Vision Master E290 (ESP32-S3 + SX1262 + 2.9" e-ink)
static constexpr int PIN_LORA_NSS  = 8;
static constexpr int PIN_LORA_SCK  = 9;
static constexpr int PIN_LORA_MOSI = 10;
static constexpr int PIN_LORA_MISO = 11;
static constexpr int PIN_LORA_NRST = 12;
static constexpr int PIN_LORA_BUSY = 13;
static constexpr int PIN_LORA_DIO1 = 14;
static constexpr int PIN_VEXT      = 18;  // peripheral power (active HIGH)
static constexpr int PIN_USER_BTN  = 0;   // BOOT
// E-ink (separate HSPI bus: SDI=1 CLK=2)
static constexpr int PIN_EINK_SDI  = 1;
static constexpr int PIN_EINK_CLK  = 2;
static constexpr int PIN_EINK_CS   = 3;
static constexpr int PIN_EINK_DC   = 4;
static constexpr int PIN_EINK_RST  = 5;
static constexpr int PIN_EINK_BUSY = 6;
static constexpr bool LORA_DIO2_AS_RF_SWITCH = true;
static constexpr bool LORA_HAS_RX_TX_EN = false;
static constexpr bool HAS_EINK = true;
static constexpr const char* BOARD_NAME = "Heltec Vision Master E290";

#elif defined(BOARD_E22)

// ESP32-E22 Carrier Board V2.1.0 — PCB nets (see AGENTS.md)
static constexpr int PIN_LORA_NSS  = 5;
static constexpr int PIN_LORA_SCK  = 18;
static constexpr int PIN_LORA_MISO = 19;
static constexpr int PIN_LORA_MOSI = 23;
static constexpr int PIN_LORA_BUSY = 26;
static constexpr int PIN_LORA_NRST = 27;
static constexpr int PIN_LORA_DIO1 = 33;
static constexpr int PIN_LORA_RXEN = 14;
static constexpr int PIN_LORA_TXEN = 13;
static constexpr int PIN_USER_BTN  = 12;  // SW1
static constexpr bool LORA_DIO2_AS_RF_SWITCH = false;
static constexpr bool LORA_HAS_RX_TX_EN = true;
static constexpr bool HAS_EINK = false;
static constexpr const char* BOARD_NAME = "ESP32-E22 V2.1.0";

#else
#error "Define BOARD_E22 or BOARD_HELTEC_E290 in platformio.ini"
#endif
