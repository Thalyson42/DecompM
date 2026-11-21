#include "sff/sff_decode.h"

// RLE8: byte com os 2 bits altos == 01 -> run de (byte & 0x3F) repeticoes do proximo byte.
// Qualquer outro byte e um pixel literal.
bool sff_decode_rle8(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstSize) {
    size_t i = 0, j = 0;
    while (j < dstSize) {
        if (i >= srcSize) return false;
        uint8_t ch = src[i++];
        size_t n = 1;
        if ((ch & 0xC0) == 0x40) {
            n = ch & 0x3F;
            if (i >= srcSize) return false;
            ch = src[i++];
        }
        while (n-- > 0 && j < dstSize) dst[j++] = ch;
    }
    return true;
}

// LZ5: um byte de controle a cada 8 operacoes; cada bit escolhe entre
//   0 = run de cor (cor de 5 bits)   1 = copia de bytes ja decodificados.
bool sff_decode_lz5(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstSize) {
    size_t i = 0, j = 0;
    if (srcSize == 0) return false;
    uint8_t ct = src[i++];
    unsigned cts = 0, rbc = 0;
    uint8_t rb = 0;
    while (j < dstSize) {
        if (i >= srcSize) return false;
        unsigned d = src[i++];
        int n;
        if (ct & (1u << cts)) {                       // copia
            if ((d & 0x3F) == 0) {                    // forma longa
                if (i + 1 >= srcSize) return false;
                d = ((d << 2) | src[i]) + 1;
                n = src[i + 1] + 2;
                i += 2;
            } else {                                  // forma curta (bits de distancia espalhados)
                rb |= (uint8_t)((d & 0xC0) >> rbc);
                rbc += 2;
                n = d & 0x3F;
                if (rbc < 8) {
                    if (i >= srcSize) return false;
                    d = src[i++] + 1;
                } else {
                    d = rb + 1;
                    rb = 0; rbc = 0;
                }
            }
            if (d > j) return false;                  // distancia invalida
            do {
                if (j < dstSize) { dst[j] = dst[j - d]; ++j; }
            } while (--n >= 0);
        } else {                                      // run de cor
            if ((d & 0xE0) == 0) {
                if (i >= srcSize) return false;
                n = src[i++] + 8;
            } else {
                n = d >> 5;
            }
            d &= 0x1F;
            for (; n > 0; --n) if (j < dstSize) dst[j++] = (uint8_t)d;
        }
        if (++cts >= 8) {
            if (i >= srcSize && j < dstSize) return false;
            if (i < srcSize) ct = src[i++];
            cts = 0;
        }
    }
    return true;
}
