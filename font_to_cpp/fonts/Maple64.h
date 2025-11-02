#pragma once
#include <stdint.h>

// Waveshare sFONT 호환 선언
typedef struct {
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
} sFONT;

extern const uint8_t Maple64_Table[];
extern const sFONT Maple64;
