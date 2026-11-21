#include "sff/sff_format.h"
#include <cstdio>
#include <cstring>

// Lê e valida o cabeçalho de um SFF. Retorna 0 em caso de sucesso.
int sff_check_header(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return 1;
    SffHeaderV1 h;
    size_t n = std::fread(&h, 1, sizeof h, f);
    std::fclose(f);
    if (n != sizeof h) return 2;
    if (std::memcmp(h.signature, "ElecbyteSpr", 11) != 0) return 3;
    std::printf("versao %u.%u.%u.%u grupos=%u imagens=%u primeiro=%u\n",
                h.version[0], h.version[1], h.version[2], h.version[3],
                h.numGroups, h.numImages, h.firstSubfileOffset);
    return 0;
}
