#include "sff/sff_format.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int check_v1(FILE* f) {
    SffHeaderV1 h;
    std::fseek(f, 0, SEEK_SET);
    if (std::fread(&h, 1, sizeof h, f) != sizeof h) return 2;
    std::printf("SFF v1: grupos=%u imagens=%u primeiro=%u\n",
                h.numGroups, h.numImages, h.firstSubfileOffset);
    return 0;
}

static int check_v2(FILE* f, long fileSize) {
    SffHeaderV2 h;
    std::fseek(f, 0, SEEK_SET);
    if (std::fread(&h, 1, sizeof h, f) != sizeof h) return 2;
    std::printf("SFF v2: sprites=%u paletas=%u ldata=%u+%u tdata=%u+%u\n",
                h.numSprites, h.numPalettes, h.ldataOffset, h.ldataLength,
                h.tdataOffset, h.tdataLength);

    // Verificacoes de consistencia (todas passam nos arquivos do MUGEN 1.1b1)
    bool ok = h.paletteTableOffset + h.numPalettes * sizeof(SffPaletteNodeV2) <= h.spriteTableOffset
           && h.spriteTableOffset + h.numSprites * sizeof(SffSpriteNodeV2) <= h.ldataOffset
           && (long)(h.ldataOffset + h.ldataLength) <= fileSize
           && (long)(h.tdataOffset + h.tdataLength) <= fileSize;
    if (!ok) { std::puts("aviso: tabelas inconsistentes"); return 4; }

    std::vector<SffSpriteNodeV2> spr(h.numSprites);
    std::fseek(f, h.spriteTableOffset, SEEK_SET);
    if (!spr.empty() && std::fread(&spr[0], sizeof(SffSpriteNodeV2), spr.size(), f) != spr.size()) return 2;
    unsigned fmt[256] = {0};
    for (size_t i = 0; i < spr.size(); ++i) fmt[spr[i].format]++;
    for (int i = 0; i < 256; ++i) if (fmt[i]) std::printf("  formato %d: %u sprites\n", i, fmt[i]);
    return 0;
}

// Valida o cabecalho de um SFF (v1 ou v2). Retorna 0 em caso de sucesso.
int sff_check_header(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return 1;
    char sig[16];
    if (std::fread(sig, 1, 16, f) != 16) { std::fclose(f); return 2; }
    if (std::memcmp(sig, "ElecbyteSpr", 11) != 0) { std::fclose(f); return 3; }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    int rc;
    switch ((unsigned char)sig[15]) {
        case 1:  rc = check_v1(f); break;
        case 2:  rc = check_v2(f, size); break;
        default: std::printf("versao SFF desconhecida: %u\n", (unsigned)(unsigned char)sig[15]); rc = 5;
    }
    std::fclose(f);
    return rc;
}
