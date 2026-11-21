#include "sff/sff_format.h"
#include "sff/sff_decode.h"
#include "sff/png_writer.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Extrai todos os sprites de um SFF v2 como PNG indexado em outdir.
// Retorna 0 se tudo foi decodificado; senao o numero de sprites que falharam (>0) ou codigo negativo.
int sff_extract_v2(const char* path, const char* outdir) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    std::vector<uint8_t> d;
    uint8_t buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    if (d.size() < sizeof(SffHeaderV2) || std::memcmp(&d[0], "ElecbyteSpr", 11) != 0 || d[15] != 2) return -2;
    SffHeaderV2 h; std::memcpy(&h, &d[0], sizeof h);
    if ((size_t)h.spriteTableOffset + (size_t)h.numSprites * sizeof(SffSpriteNodeV2) > d.size() ||
        (size_t)h.paletteTableOffset + (size_t)h.numPalettes * sizeof(SffPaletteNodeV2) > d.size()) return -3;

    std::vector<SffSpriteNodeV2> spr(h.numSprites);
    std::vector<SffPaletteNodeV2> pal(h.numPalettes);
    if (h.numSprites)  std::memcpy(&spr[0], &d[h.spriteTableOffset], h.numSprites * sizeof(SffSpriteNodeV2));
    if (h.numPalettes) std::memcpy(&pal[0], &d[h.paletteTableOffset], h.numPalettes * sizeof(SffPaletteNodeV2));

    int failed = 0, written = 0;
    for (size_t i = 0; i < spr.size(); ++i) {
        const SffSpriteNodeV2* s = &spr[i];
        // sprite linkado: sem dados proprios, usa os do alvo (a confirmar no exe se herda mais algo)
        const SffSpriteNodeV2* src = s;
        int guard = 0;
        while (src->dataLength == 0 && src->linkedIndex < spr.size() && guard++ < 16) src = &spr[src->linkedIndex];
        unsigned w = src->width, hh = src->height;
        std::vector<uint8_t> px((size_t)w * hh);
        bool ok = false;
        size_t base = (src->flags & 1) ? h.tdataOffset : h.ldataOffset;
        size_t lim  = (src->flags & 1) ? h.tdataLength : h.ldataLength;
        if (src->dataLength >= 4 && (size_t)src->dataOffset + src->dataLength <= lim && base + src->dataOffset + src->dataLength <= d.size()) {
            const uint8_t* p = &d[base + src->dataOffset] + 4;
            size_t len = src->dataLength - 4;
            if (src->format == 2)      ok = sff_decode_rle8(p, len, &px[0], px.size());
            else if (src->format == 4) ok = sff_decode_lz5 (p, len, &px[0], px.size());
        }
        // paleta
        uint8_t rgba[256 * 4]; std::memset(rgba, 0, sizeof rgba);
        unsigned nc = 0;
        if (ok && s->paletteIndex < pal.size()) {
            const SffPaletteNodeV2* pn = &pal[s->paletteIndex];
            int g2 = 0;
            while (pn->dataLength == 0 && pn->linkedIndex < pal.size() && g2++ < 16) pn = &pal[pn->linkedIndex];
            nc = pn->numColors > 256 ? 256 : pn->numColors;
            if ((size_t)h.ldataOffset + pn->dataOffset + nc * 4 <= d.size())
                std::memcpy(rgba, &d[h.ldataOffset + pn->dataOffset], nc * 4);
            else ok = false;
        } else ok = false;
        char name[512];
        std::snprintf(name, sizeof name, "%s/%u_%u.png", outdir, (unsigned)s->group, (unsigned)s->item);
        if (ok && png_write_indexed(name, w, hh, px, rgba, nc)) ++written;
        else { ++failed; std::fprintf(stderr, "falhou: sprite %zu (%u,%u) formato %u\n", i, (unsigned)s->group, (unsigned)s->item, (unsigned)src->format); }
    }
    std::printf("extraidos: %d, falhas: %d\n", written, failed);
    return failed;
}
