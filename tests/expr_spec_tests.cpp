// Testes: exemplos tirados literalmente de docs/cns.html (secao "Expressions") do MUGEN 1.1b1.
#include "parser/expr.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

struct NullCtx : ExprContext {
    bool call(const std::string& n, const std::vector<ExprValue>&, ExprValue& o) {
        if (n == "Time") { o = ExprValue::Int(5); return true; }
        return false;
    }
    bool assign(const std::string&, const std::vector<ExprValue>&, const ExprValue& v, ExprValue& o) { o = v; return true; }
    bool redirect(const std::string&, const std::vector<ExprValue>&, const ExprNode* in, ExprValue& o) {
        NullCtx c; o = expr_eval(in, c); return true;   // so para testar a sintaxe de redirecionamento
    }
};

static int fails = 0, total = 0;
static void expect_int(const char* e, int want) {
    ++total; std::string err; ExprPtr p = expr_parse(e, &err); NullCtx c;
    if (!p) { std::printf("FALHOU  %-40s erro de parse: %s\n", e, err.c_str()); ++fails; return; }
    ExprValue v = expr_eval(p.get(), c);
    if (v.kind != ExprValue::INT || v.i != want) { std::printf("FALHOU  %-40s esperado %d\n", e, want); ++fails; }
}
static void expect_float(const char* e, float want) {
    ++total; std::string err; ExprPtr p = expr_parse(e, &err); NullCtx c;
    if (!p) { std::printf("FALHOU  %-40s erro de parse: %s\n", e, err.c_str()); ++fails; return; }
    ExprValue v = expr_eval(p.get(), c);
    if (v.kind != ExprValue::FLOAT || std::fabs(v.f - want) > 1e-4f) { std::printf("FALHOU  %-40s esperado %g\n", e, (double)want); ++fails; }
}
static void expect_bottom(const char* e) {
    ++total; std::string err; ExprPtr p = expr_parse(e, &err); NullCtx c;
    if (!p) { std::printf("FALHOU  %-40s erro de parse: %s\n", e, err.c_str()); ++fails; return; }
    if (expr_eval(p.get(), c).kind != ExprValue::BOTTOM) { std::printf("FALHOU  %-40s esperado bottom\n", e); ++fails; }
}
static void expect_parse_error(const char* e) {
    ++total; std::string err;
    if (expr_parse(e, &err)) { std::printf("FALHOU  %-40s deveria ser invalida\n", e); ++fails; }
}

int main() {
    // precedencia e associatividade (doc: 3+2*5 = 13, 5.0*5/6, 5/6*5.0, -!0)
    expect_int("3+2*5", 13);
    expect_int("(3+2)*5", 25);
    expect_float("5.0*5/6", 25.0f / 6.0f);
    expect_float("5/6*5.0", 0.0f);
    expect_int("-!0", -1);
    // divisao e resto
    expect_int("7/2", 3);
    expect_float("7.0/2.0", 3.5f);
    expect_bottom("1/0");
    expect_int("7%2", 1);
    expect_bottom("7%0");
    expect_bottom("7.0%2");
    // exponenciacao
    expect_int("2**3", 8);
    expect_int("0**0", 1);
    expect_float("2**-1", 0.5f);
    expect_bottom("-1 ** .5");
    // logicos e bit a bit
    expect_int("1 && 2", 1);
    expect_int("0 || 0", 0);
    expect_int("1 ^^ 1", 0);
    expect_int("6 & 3", 2);
    expect_int("6 | 3", 7);
    expect_int("6 ^ 3", 5);
    expect_int("~0", -1);
    expect_bottom("~1.0");
    // comparacoes
    expect_int("1 = 1.0", 1);
    expect_int("3 != 3", 0);
    expect_int("2 <= 2", 1);
    // Exemplo de docs/cns.html: 1.0 = (2 = (1 > 0) + !(0 < 1)).
    // A documentacao afirma que "0 < 1" vale 0 e conclui 1, mas 0 < 1 e verdadeiro (1): o resultado correto
    // pelas regras de operadores e 0 (1 + !1 = 1; 2 = 1 -> 0; 1.0 = 0 -> 0). Erro no exemplo da doc.
    // O comportamento real do mugen.exe ainda precisa ser conferido.
    expect_int("1.0 = (2 = (1 > 0) + !(0 < 1))", 0);
    expect_int("1.0 = (2 = (1 > 0) + !(0 > 1))", 1);
    // intervalos
    expect_int("1 = [0,2]", 1);
    expect_int("2 = [0,2)", 0);
    expect_int("5 != [0,2]", 1);
    expect_int("(1 = [0,2]) = (0,1)", 0);          // exemplo do doc: avalia a 0
    expect_parse_error("1 = [0,2] = (0,1)");       // doc: nao e bem formada
    // atribuicao e redirecionamento (so sintaxe, avaliados pelo contexto)
    expect_int("5 + (root, Time)", 10);
    expect_int("Time + 5", 10);
    // erros de sintaxe
    expect_parse_error("1 +");
    expect_parse_error("(1 + 2");
    expect_parse_error("1 $ 2");
    std::printf("%d/%d testes passaram\n", total - fails, total);
    return fails ? 1 : 0;
}
