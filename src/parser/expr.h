#pragma once
#include <memory>
#include <string>
#include <vector>

// Tokenizer + parser + avaliador das expressoes do MUGEN (triggers e parametros de controllers).
// Segue docs/cns.html ("Expressions"): tipos int/float/bottom, precedencia estilo C, intervalos,
// ":=", redirecionamento de trigger e triggers de varias palavras ("Vel X").
// Reimplementacao baseada na documentacao; ainda nao comparada com o mugen.exe.

struct ExprNode;
typedef std::unique_ptr<ExprNode> ExprPtr;

struct ExprNode {
    enum Type {
        INT, FLOAT, STRING,
        CALL,        // name + args (kids). "Vel X" vira name "Vel X". Identificador solto = CALL sem args
        UNARY,       // op = "-", "!", "~"
        BINARY,      // op = "+", "-", "*", "/", "%", "**", "<", ... ":=", "&&" etc.
        INTERVAL,    // kids = [x, y, z]; op = "=" ou "!="; lo = '[' ou '('; hi = ']' ou ')'
        PARTIAL,     // fragmento "op expr" depois de virgula, ex.: o ">= 0" de "AnimElem = 2, >= 0"
        REDIRECT,    // kids = [redirecionador (CALL), expressao]: "(parent, Time)"
        LIST         // lista separada por virgulas (parametros de controllers)
    };
    Type type;
    int   ival;
    float fval;
    std::string str;            // nome (CALL), operador (UNARY/BINARY/INTERVAL/PARTIAL) ou texto (STRING)
    char lo, hi;
    std::vector<ExprPtr> kids;
    ExprNode() : type(INT), ival(0), fval(0), lo(0), hi(0) {}
};

// Converte texto em AST. Em caso de erro retorna nullptr e preenche *error (se nao nulo).
ExprPtr expr_parse(const std::string& text, std::string* error);
// Representacao em S-expression, para testes e depuracao.
std::string expr_to_string(const ExprNode* n);

struct ExprValue {
    enum Kind { BOTTOM, INT, FLOAT, STR };
    Kind kind; int i; float f; std::string s;
    ExprValue() : kind(BOTTOM), i(0), f(0) {}
    static ExprValue Int(int v)            { ExprValue r; r.kind = INT;   r.i = v; return r; }
    static ExprValue Float(float v)        { ExprValue r; r.kind = FLOAT; r.f = v; return r; }
    static ExprValue Str(const std::string& v) { ExprValue r; r.kind = STR; r.s = v; return r; }
    static ExprValue Bottom()              { return ExprValue(); }
    bool nonzero() const;       // bottom conta como zero
};

// O avaliador so conhece aritmetica. Triggers, variaveis e redirecionamentos vem do contexto.
struct ExprContext {
    virtual ~ExprContext() {}
    // name ex.: "Time", "Vel X", "var", "Const". Retorne false se desconhecido (vira bottom).
    virtual bool call(const std::string& name, const std::vector<ExprValue>& args, ExprValue& out) = 0;
    // Atribuicao "var(n) := valor" / "fvar(n) := valor".
    virtual bool assign(const std::string& name, const std::vector<ExprValue>& args, const ExprValue& v, ExprValue& out) = 0;
    // Comparacao "Nome = \"texto\"" (ex.: Command = "holdfwd"). Se retornar true, 'out' e o resultado (0/1) de "=".
    // O avaliador inverte o resultado para "!=". Se retornar false, cai na comparacao normal.
    virtual bool call_eq_str(const std::string&, const std::vector<ExprValue>&, const std::string&, ExprValue&) { return false; }
    // Sintaxe antiga "Nome = valor" onde Nome e um trigger com semantica propria (ex.: AnimElem = 3 significa
    // AnimElemTime(3) = 0). Se retornar true, 'out' e o resultado de "=" (o avaliador inverte para "!=").
    virtual bool call_eq(const std::string&, const std::vector<ExprValue>&, const ExprValue&, ExprValue&) { return false; }
    // Sintaxe antiga com lista: "AnimElem = 2, >= 5" / "TimeMod = 4, 0". 'extras' traz cada item depois da virgula:
    // op vazio para um valor simples, ou o operador ("=", ">=", ...) para um fragmento "op valor".
    struct Extra { std::string op; ExprValue value; };
    virtual bool call_list(const std::string&, const std::vector<ExprValue>&, const ExprValue&, const std::vector<Extra>&, ExprValue&) { return false; }
    // Redirecionamento: avalia 'inner' no contexto de outro personagem. Retorne false se nao existir.
    virtual bool redirect(const std::string& name, const std::vector<ExprValue>& args,
                          const ExprNode* inner, ExprValue& out) = 0;
};

ExprValue expr_eval(const ExprNode* n, ExprContext& ctx);
