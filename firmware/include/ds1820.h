#pragma once

#include <Arduino.h>

// One-Wire temp on J5 (GPIO PIN_ONEWIRE). DS1820/S20 (0x10) and DS18B20 (0x28).
// Many cheap "DS1820" clones need strong pull-up during convert even if 3V3
// is wired to VDD (internal VDD often unbonded).

bool ds1820Begin(int pin);
bool ds1820ReadCelsius(float* out_c);
bool ds1820LastRaw(int16_t* out_raw);
void ds1820SetOffsetC(float offset_c);
float ds1820GetOffsetC();
