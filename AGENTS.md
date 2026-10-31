# AGENTS.md — ttx_e22

## Aktuelle Hardware (Stand: verbindlich)

**Richtige Version: V2.1.0** (einseitig bestückt).  
Pfad: `hardware/ESP32-E22_V2.1/`  
Quellen der Wahrheit: `KiCad/esp32-e22.kicad_pcb` (Netze/Pins) und `KiCad/esp32-e22.kicad_sch` (Rev `V2.1.0`).  
PDF: `hardware/ESP32-E22_V2.1/pdf/ESP32-E22_V2.1.0_SCH.pdf`

**Nicht verwenden:** V2.1.1 lag zeitweise fälschlich im Repo — verwerfen, falls wieder auf.

Überblick: Carrier für **ESP32-DevKitC (U1)** + **Ebyte E22-400M33S (U3)** + **0.96" OLED (U2)**, IP65-Gehäuse 120×80×60. PCB-Bestellung: https://aisler.net/p/LMKJXZAY

Dokumentation/Stückliste/Gehäusehinweise: `hardware/README.md`, `hardware/ESP32-E22_V2.1/README.md`, `hardware/datasheet/`.

---

## Architektur

| Ref | Teil | Rolle |
|-----|------|--------|
| U1 | ESP32-DevKitC | MCU (3V3-Logik vom Modul) |
| U3 | E22-400M33S | LoRa ~400 MHz, **VCC = +5V**, ANT → R8 (0Ω) → SMA J8 |
| U2 | DM-OLED096-636 | I2C-OLED; JP1/JP2 zum ggf. Auskreuzen |
| J2 | USB-C vert. | 5V-Versorgung (CC1/CC2 je 5k1 → 5V/3A, kein PD-IC) |
| J3 | 2-pol | externe **~5V** (nicht 6–24V — kein Buck in V2.1.0) |
| D1/D2 | 1N5817 | Schottky-OR der Versorgungseingänge auf +5V |
| D3 | LESD5D5.0CT1G | TVS auf +5V |
| C3/C6 | 470µF | + Elkos; C4/C5 100nF am +5V |
| J4 | GPS | 3V3, GND, GPS_TX, GPS_RX |
| J5 | OneWire | 3V3, GND, Data (+ R5 4k7 Pull-up) |
| J6/J7 | I2C | 3V3, GND, SCL, SDA (+ R6/R7 10k, C7/C8) |
| J9 | DIO2 | E22 DIO2 herausgeführt |
| J1 | ADC-Eingang | über R1 5k1 auf GPIO32; R2 5k1 + C1 100nF nach GND |
| SW1 | Taster | nach **GPIO12** (nicht EN/Reset) |
| C2 | 10µF | an CHIP_PU (EN) nach GND |

**V2.1.0 hat keinen HW-613-Buck, keine Feinsicherung, kein 6–24V-Eingang** (das war V2.1.1-Idee). E22-TX braucht ~650 mA extra — USB am DevKit oft zu schwach; USB-C/J3 bevorzugen.

---

## ESP32-Pinning (aus PCB-Netzen U1)

Flash-Pins GPIO6–11: unverbunden lassen.

### E22 / SPI / RF-Steuerung

| Signal | GPIO | DevKit-Pad |
|--------|------|------------|
| E22_NSS | 5 | 29 |
| E22_SCK | 18 | 30 |
| MISO | 19 | 31 |
| MOSI | 23 | 37 |
| E22_BUSY | 26 | 10 |
| E22_NRST | 27 | 11 |
| E22_DIO1 | 33 | 8 |
| E22_RXEN | 14 | 12 |
| E22_TXEN | 13 | 15 |
| E22 DIO2 | — | nur J9 |

### Peripherie

| Signal | GPIO | DevKit-Pad |
|--------|------|------------|
| SDA | 21 | 33 |
| SCL | 22 | 36 |
| GPS_RX (Netzname) | 16 | 27 |
| GPS_TX (Netzname) | 17 | 28 |
| OneWire | 25 | 9 |
| SW1 | 12 | 13 |
| ADC (J1 über Teiler) | 32 | 7 |

Header J4: Pin3=`GPS_TX`, Pin4=`GPS_RX` (Netzname = GPS-Seite; Firmware-UART kreuzt üblicherweise TX↔RX).

---

## Firmware / Agent-Hinweise

- Firmware: `firmware/` (PlatformIO, RadioLib), Envs `esp32-e22` und `heltec-e290`.
- Pins: `firmware/include/board_pins.h` (`BOARD_E22` / `BOARD_HELTEC_E290`).
- E22: SX1268, **TCXO 1.8 V**, RF-Switch **RXEN/TXEN**, VCC **5 V**, Default **433.775 MHz**.
- Heltec Vision Master E290: ESP32-S3 + **SX1262**, DIO2=RF-Switch, Vext=GPIO18 HIGH, USB-CDC.
  - LoRa-Pins: NSS8 SCK9 MOSI10 MISO11 RST12 BUSY13 DIO1=14.
  - **Muss 433‑MHz-SKU sein**, um mit dem E22 zu sprechen (nicht EU868/US915).
- Keine Annahmen aus V2.1.1 (Buck, Schraubklemme 6–24V, Deckel-J10).
- Bei Pin-Fragen: PCB-Netzliste prüfen, nicht nur Schaltplan-Labels/Vision auf PDFs.
- Weitere Versionen laut `hardware/README.md`: V2.1.2 (quer/außen), V2.2.x (beidseitig) — separat behandeln, wenn Ordner existieren.
