# ESP32 ↔ Heltec raw LoRa

Gleiche Air-Parameter auf beiden Seiten (Default: **433.775 MHz**, BW 125, SF7, CR 4/5).

| Env | Board | Chip |
|-----|--------|------|
| `esp32-e22` | ESP32-E22 Carrier V2.1.0 | SX1268 (E22-400M33S) |
| `heltec-e290` | [Heltec Vision Master E290](https://heltec.org/project/vision-master-e290/) | SX1262 |

**Wichtig:** Das E22 sendet im **433‑MHz-Band**. Die Heltec muss die **433 MHz-SKU** sein (nicht 868/915) — sonst kein Empfang trotz Software-Freq.

## Flash

```bash
cd firmware

# Carrier (TX/RX)
pio run -e esp32-e22 -t upload
pio device monitor -e esp32-e22

# Heltec Empfänger (anderes Terminal / Port; USB-C am Heltec)
pio run -e heltec-e290 -t upload
pio device monitor -e heltec-e290
```

`heltec-e290` nutzt Board-ID `esp32-s3-devkitc-1` (Pins fest in `board_pins.h`), damit ältere PlatformIO-Installationen ohne Heltec-Board-Definition funktionieren.


Strom E22: USB-C (J2) oder J3 (~5 V) bevorzugen.

## Serial (115200)

```
tx hello
txhex 01 02 fe
freq 433.775
status
help
```

User-Button sendet `BTN` (E22: SW1, Heltec: BOOT).

**E22-Beacon:** sendet automatisch `DL6SR #<n>` alle 30 s (Sequenznummer steigt). Serial: `pause` / `resume`.
