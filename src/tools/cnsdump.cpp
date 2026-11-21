#include "parser/cns_parser.h"
#include <cstdio>
// Uso: cnsdump arquivo.cns|cmd [-v]
int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: cnsdump arquivo.cns|cmd [-v]"); return 1; }
    CnsFile f; std::vector<std::string> w;
    if (!cns_load(argv[1], f, &w)) { std::fprintf(stderr, "nao abriu %s\n", argv[1]); return 2; }
    size_t ctrls = 0;
    for (size_t i = 0; i < f.states.size(); ++i) {
        const CnsStatedef& s = f.states[i]; ctrls += s.controllers.size();
        if (argc > 2) {
            std::printf("Statedef %d: type=%s movetype=%s physics=%s anim=%s, %zu controllers\n", s.number,
                        s.param("type").c_str(), s.param("movetype").c_str(), s.param("physics").c_str(),
                        s.param("anim").c_str(), s.controllers.size());
            for (size_t j = 0; j < s.controllers.size(); ++j) {
                const CnsController& c = s.controllers[j];
                std::printf("  [%s] triggerall=%zu grupos=%zu\n", c.type.c_str(), c.triggerAll.size(), c.triggers.size());
            }
        }
    }
    for (size_t i = 0; i < w.size(); ++i) std::fprintf(stderr, "aviso %s\n", w[i].c_str());
    std::printf("%s: %zu states, %zu controllers, %zu secoes de constantes, %zu avisos\n",
                argv[1], f.states.size(), ctrls, f.others.size(), w.size());
    return 0;
}
