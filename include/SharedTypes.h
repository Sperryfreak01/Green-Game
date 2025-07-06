#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <Arduino.h>

// LED structure definition
struct LEDstruct {
  uint8_t redBrightness = 0;
  uint8_t greenBrightness = 0;
  uint8_t blueBrightness = 0;
  uint8_t whiteBrightness = 0;
  uint8_t maxBrightness = 100; // Max brightness percentage (0-100) 
  uint8_t nightBrightness = 100; // Night mode brightness percentage (0-100)
  uint8_t nightEnd = 7; // Hour when night mode ends (0-23)
  uint8_t nightStart = 20; // Hour when night mode starts (0-23)
  uint8_t redTarget = 0; // Target red brightness
  uint8_t greenTarget = 0; // Target green brightness
  uint8_t blueTarget = 0; // Target blue brightness
  uint8_t whiteTarget = 0; // Target white brightness
  uint16_t Hue = 0; // Hue value for color
  uint16_t Sat = 0; // Saturation value for color
  uint16_t Bright = 0; // Intensity value for color
  bool override = 0; // Override normal color mode
};

#endif // SHARED_TYPES_H
