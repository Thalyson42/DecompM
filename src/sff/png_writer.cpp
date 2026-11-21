#include "sff/png_writer.h"
#include <cstdio>
#include <cstring>

static uint32_t crc_table[256];
static void init_crc() {
    static bool done = false; if (done) return;
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    done = true;
}
static uint32_t crc_update(uint32_t c, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}
static void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}
static void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hdr; put32(hdr, (uint32_t)data.size());
    std::fwrite(&hdr[0], 1, 4, f);
    uint32_t c = 0xFFFFFFFFu;
    c = crc_update(c, (const uint8_t*)type, 4);
    if (!data.empty()) { std::fwrite(type, 1, 4, f); std::fwrite(&data[0], 1, data.size(), f); c = crc_update(c, &data[0], data.size()); }
    else std::fwrite(type, 1, 4, f);
    std::vector<uint8_t> t; put32(t, c ^ 0xFFFFFFFFu);
    std::fwrite(&t[0], 1, 4, f);
}

bool png_write_indexed(const char* path, unsigned w, unsigned h,
                       const std::vector<uint8_t>& px, const uint8_t* pal, unsigned nc) {
    if (w == 0 || h == 0 || px.size() < (size_t)w * h) return false;
    init_crc();
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    static const uint8_t sig[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> d;
    put32(d, w); put32(d, h); d.push_back(8); d.push_back(3); d.push_back(0); d.push_back(0); d.push_back(0);
    chunk(f, "IHDR", d);
    std::vector<uint8_t> plte, trns;
    if (nc > 256) nc = 256;
    for (unsigned i = 0; i < nc; ++i) { plte.push_back(pal[4*i]); plte.push_back(pal[4*i+1]); plte.push_back(pal[4*i+2]); }
    chunk(f, "PLTE", plte);
    trns.push_back(0);                       // indice 0 transparente
    chunk(f, "tRNS", trns);
    // IDAT: zlib com blocos stored
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w + 1) * h);
    for (unsigned y = 0; y < h; ++y) { raw.push_back(0); raw.insert(raw.end(), px.begin() + (size_t)y*w, px.begin() + (size_t)(y+1)*w); }
    std::vector<uint8_t> z; z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    do {
        size_t n = raw.size() - pos; if (n > 65535) n = 65535;
        bool last = (pos + n == raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    } while (pos < raw.size());
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw.size(); ++i) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    put32(z, (b << 16) | a);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", std::vector<uint8_t>());
    std::fclose(f);
    return true;
}
