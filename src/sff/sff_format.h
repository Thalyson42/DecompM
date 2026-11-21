#pragma once
#include <cstdint>

// RASCUNHO baseado no formato SFF v1 documentado pela comunidade.
// CONFIRMAR contra o sff2png.exe no Ghidra antes de dar como certo.
// O MUGEN 1.1 também lê SFF v2, que tem outro layout (ainda não mapeado).
#pragma pack(push, 1)
struct SffHeaderV1 {
    char     signature[12];     // "ElecbyteSpr\0"
    uint8_t  version[4];
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
#pragma pack(pop)
