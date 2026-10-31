#pragma once

// ESP32-E22 Carrier Board V2.1.0 — from PCB nets (see AGENTS.md)

// E22-400M33S (SX1268) via VSPI
static constexpr int PIN_E22_NSS  = 5;
static constexpr int PIN_E22_SCK  = 18;
static constexpr int PIN_E22_MISO = 19;
static constexpr int PIN_E22_MOSI = 23;
static constexpr int PIN_E22_BUSY = 26;
static constexpr int PIN_E22_NRST = 27;
static constexpr int PIN_E22_DIO1 = 33;
static constexpr int PIN_E22_RXEN = 14;
static constexpr int PIN_E22_TXEN = 13;

// On-board user button (active low)
static constexpr int PIN_SW1 = 12;
