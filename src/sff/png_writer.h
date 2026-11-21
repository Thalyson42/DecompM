#pragma once
#include <cstdint>
#include <vector>

// Grava um PNG indexado (8 bits) sem compressao (blocos "stored"), sem dependencias externas.
// palette: 256 entradas RGBA (4 bytes cada). O indice 0 vira transparente.
bool png_write_indexed(const char* path, unsigned w, unsigned h,
                       const std::vector<uint8_t>& pixels, const uint8_t* paletteRGBA, unsigned numColors);
