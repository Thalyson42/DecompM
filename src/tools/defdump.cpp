#include "parser/ini_parser.h"
#include <cstdio>
// Uso: defdump arquivo [-v]   -> resumo (e, com -v, todas as secoes e chaves)
int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: defdump arquivo.def|cns|cmd|air [-v]"); return 1; }
    IniFile f;
    if (!ini_load(argv[1], f)) { std::fprintf(stderr, "nao abriu %s\n", argv[1]); return 2; }
    size_t entries = 0;
    for (size_t i = 0; i < f.sections.size(); ++i) {
        entries += f.sections[i].entries.size();
        if (argc > 2) {
            std::printf("[%s] (linha %d)\n", f.sections[i].name.c_str(), f.sections[i].line);
            for (size_t j = 0; j < f.sections[i].entries.size(); ++j)
                std::printf("  %s = %s\n", f.sections[i].entries[j].key.c_str(), f.sections[i].entries[j].value.c_str());
        }
    }
    std::printf("%s: %zu secoes, %zu entradas\n", argv[1], f.sections.size(), entries);
    return 0;
}
