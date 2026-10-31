# ESP32-E22 Firmware (V2.1.0)

Raw LoRa payloads (wenige Bytes bis ~253) über das E22-400M33S (SX1268).

## Build / Flash

```bash
cd firmware
pio run -t upload
pio device monitor
```

Stromversorgung: für TX USB-C (J2) oder externe ~5 V (J3) bevorzugen — DevKit-USB oft zu schwach.

## Serial (115200)

Beide Boards müssen **gleiche** `freq` / `bw` / `sf` / `cr` haben.

```
tx hello
txhex 01 02 fe
freq 433.775
pwr 10
status
help
```

Empfang läuft dauerhaft; RX-Pakete erscheinen als Hex + ASCII. **SW1** sendet `BTN`.

Default: 433.775 MHz, BW 125 kHz, SF7, CR 4/5, 10 dBm (Chip). Frequenznutzung lokal prüfen (EU 433-ISM).
