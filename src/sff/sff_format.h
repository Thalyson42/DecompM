#pragma once
#include <cstdint>

// Layouts do formato SFF, confirmados empiricamente nos arquivos do MUGEN 1.1b1:
//  - v1: data/system.sff (versao 0.1.0.1)  - 17 grupos, 30 imagens, primeiro subarquivo em 512
//  - v2: chars/kfm/kfm.sff e kfm720.sff (versao 0.1.0.2) - todos os offsets/tamanhos fecham
// Ainda falta confirmar contra o sff2png.exe no Ghidra (campos "reservados" e flags).
#pragma pack(push, 1)

// ---------- v1 ----------
struct SffHeaderV1 {
    char     signature[12];     // "ElecbyteSpr\0"
    uint8_t  version[4];        // [3] == 1
    uint32_t numGroups;
    uint32_t numImages;
    uint32_t firstSubfileOffset;
    uint32_t subheaderSize;
    uint8_t  paletteType;
    uint8_t  blank[3];
    char     comments[476];
};
static_assert(sizeof(SffHeaderV1) == 512, "SffHeaderV1 deve ter 512 bytes");

struct SffSubheaderV1 {
    uint32_t nextOffset;
    uint32_t length;
    int16_t  axisX;
    int16_t  axisY;
    uint16_t group;
    uint16_t image;
    uint16_t linkedIndex;
    uint8_t  samePalette;
    char     comments[13];
};
static_assert(sizeof(SffSubheaderV1) == 32, "SffSubheaderV1 deve ter 32 bytes");

// ---------- v2 ----------
struct SffHeaderV2 {
    char     signature[12];     // "ElecbyteSpr\0"
    uint8_t  version[4];        // [3] == 2
    uint32_t reserved0;         // 0 nos arquivos testados
    uint32_t reserved1;         // 0
    uint8_t  compatVersion[4];  // 0,0,0,2 nos arquivos testados
    uint32_t reserved2;         // 0
    uint32_t reserved3;         // 0
    uint32_t spriteTableOffset; // offset do primeiro SffSpriteNodeV2
    uint32_t numSprites;
    uint32_t paletteTableOffset;// offset do primeiro SffPaletteNodeV2
    uint32_t numPalettes;
    uint32_t ldataOffset;       // dados "literais" (maioria dos sprites)
    uint32_t ldataLength;
    uint32_t tdataOffset;       // dados "translated" (flag bit 0 do sprite)
    uint32_t tdataLength;
};
static_assert(sizeof(SffHeaderV2) == 68, "SffHeaderV2 deve ter 68 bytes");

// Formatos de compressao vistos: 2 = RLE8 (coldepth 8), 4 = LZ5 (coldepth 5).
// Pela documentacao da comunidade tambem existem 0 (raw), 3 (RLE5) e 10/11/12 (PNG);
// ainda nao aparecem nos arquivos de teste.
struct SffSpriteNodeV2 {
    uint16_t group;
    uint16_t item;
    uint16_t width;
    uint16_t height;
    int16_t  axisX;
    int16_t  axisY;
    uint16_t linkedIndex;       // 0 quando nao e link (a confirmar)
    uint8_t  format;
    uint8_t  colorDepth;
    uint32_t dataOffset;        // relativo a ldata (ou tdata se flags & 1)
    uint32_t dataLength;
    uint16_t paletteIndex;
    uint16_t flags;             // bit 0: dados em tdata
};
static_assert(sizeof(SffSpriteNodeV2) == 28, "SffSpriteNodeV2 deve ter 28 bytes");

struct SffPaletteNodeV2 {
    uint16_t group;
    uint16_t item;
    uint16_t numColors;
    uint16_t linkedIndex;
    uint32_t dataOffset;        // relativo a ldata
    uint32_t dataLength;        // numColors * 4 (RGBA)
};
static_assert(sizeof(SffPaletteNodeV2) == 16, "SffPaletteNodeV2 deve ter 16 bytes");
#pragma pack(pop)
