#include "parser/air_parser.h"
#include <cstdio>
// Uso: airdump arquivo.air [-v]   -> uma linha por acao (e, com -v, cada elemento)
int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: airdump arquivo.air [-v]"); return 1; }
    std::vector<AirAction> acts; std::vector<std::string> warns;
    if (!air_load(argv[1], acts, &warns)) { std::fprintf(stderr, "nao abriu %s\n", argv[1]); return 2; }
    size_t frames = 0;
    for (size_t i = 0; i < acts.size(); ++i) {
        const AirAction& a = acts[i]; frames += a.frames.size();
        std::printf("acao %d: %zu elementos, looptime=%d, loopstart=%d\n", a.number, a.frames.size(), a.looptime(), a.loopStart);
        if (argc > 2) for (size_t j = 0; j < a.frames.size(); ++j) {
            const AirFrame& f = a.frames[j];
            std::printf("  %d,%d  off %d,%d  t=%d%s%s  clsn1=%zu clsn2=%zu\n", f.group, f.image, f.x, f.y, f.time,
                        f.flipH ? " H" : "", f.flipV ? " V" : "", f.clsn1.size(), f.clsn2.size());
        }
    }
    for (size_t i = 0; i < warns.size(); ++i) std::fprintf(stderr, "aviso %s\n", warns[i].c_str());
    std::printf("%s: %zu acoes, %zu elementos, %zu avisos\n", argv[1], acts.size(), frames, warns.size());
    return 0;
}
