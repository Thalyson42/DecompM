#pragma once
#include <cstddef>
#include <cstdint>

// Decodificadores dos dados de sprite do SFF v2.
// "src" e "dst" sem o cabecalho de 4 bytes (que guarda o tamanho descomprimido = largura*altura).
// Retornam true se produziram exatamente dstSize bytes sem ler alem do fim de src.
bool sff_decode_rle8(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstSize);
bool sff_decode_lz5 (const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstSize);
