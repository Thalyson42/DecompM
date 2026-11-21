#include "parser/cns_parser.h"
#include "parser/expr.h"
#include <cstdio>
// Uso: exprcheck arquivo.cns|cmd [-v]
// Faz o parse de todos os triggers e parametros de controllers/Statedefs e lista as falhas.
int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: exprcheck arquivo.cns|cmd [-v]"); return 1; }
    CnsFile f;
    if (!cns_load(argv[1], f, 0)) { std::fprintf(stderr, "nao abriu %s\n", argv[1]); return 2; }
    size_t total = 0, bad = 0;
    for (size_t i = 0; i < f.states.size(); ++i) {
        const CnsStatedef& s = f.states[i];
        std::vector<CnsParam> all(s.params);
        for (size_t k = 0; k < all.size(); ++k) {
            ++total; std::string err;
            if (all[k].value.empty()) continue;
            if (!expr_parse(all[k].value, &err)) { ++bad; std::printf("linha %d: %s = %s   -> %s\n", all[k].line, all[k].key.c_str(), all[k].value.c_str(), err.c_str()); }
        }
        for (size_t j = 0; j < s.controllers.size(); ++j) {
            const CnsController& c = s.controllers[j];
            std::vector<std::string> exprs(c.triggerAll);
            for (size_t g = 0; g < c.triggers.size(); ++g) exprs.insert(exprs.end(), c.triggers[g].begin(), c.triggers[g].end());
            for (size_t k = 0; k < exprs.size(); ++k) {
                ++total; std::string err;
                ExprPtr e = expr_parse(exprs[k], &err);
                if (!e) { ++bad; std::printf("linha %d: trigger %s   -> %s\n", c.line, exprs[k].c_str(), err.c_str()); }
                else if (argc > 2) std::printf("%s\n   %s\n", exprs[k].c_str(), expr_to_string(e.get()).c_str());
            }
            for (size_t k = 0; k < c.params.size(); ++k) {
                if (c.params[k].value.empty()) continue;
                ++total; std::string err;
                if (!expr_parse(c.params[k].value, &err)) { ++bad; std::printf("linha %d: %s = %s   -> %s\n", c.params[k].line, c.params[k].key.c_str(), c.params[k].value.c_str(), err.c_str()); }
            }
        }
    }
    std::printf("%s: %zu expressoes, %zu falhas\n", argv[1], total, bad);
    return bad ? 1 : 0;
}
