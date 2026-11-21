#include "parser/expr.h"
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------- lexer
namespace {
enum TokType { T_END, T_INT, T_FLOAT, T_STR, T_IDENT, T_OP };
struct Tok { TokType t; std::string s; int i; float f; };

bool lex(const std::string& src, std::vector<Tok>& out, std::string* err) {
    size_t p = 0, n = src.size();
    while (p < n) {
        unsigned char c = (unsigned char)src[p];
        if (std::isspace(c)) { ++p; continue; }
        Tok t; t.t = T_OP; t.i = 0; t.f = 0;
        if (std::isdigit(c) || (c == '.' && p + 1 < n && std::isdigit((unsigned char)src[p + 1]))) {
            size_t q = p; bool isf = false;
            while (q < n && std::isdigit((unsigned char)src[q])) ++q;
            if (q < n && src[q] == '.') { isf = true; ++q; while (q < n && std::isdigit((unsigned char)src[q])) ++q; }
            std::string num = src.substr(p, q - p);
            if (isf) { t.t = T_FLOAT; t.f = (float)std::atof(num.c_str()); }
            else     { t.t = T_INT; long long v = std::atoll(num.c_str()); t.i = (int)(v > INT_MAX ? INT_MAX : v); }
            t.s = num; p = q;
        } else if (std::isalpha(c) || c == '_') {
            size_t q = p;
            while (q < n && (std::isalnum((unsigned char)src[q]) || src[q] == '_' || src[q] == '.')) ++q;
            t.t = T_IDENT; t.s = src.substr(p, q - p); p = q;
        } else if (c == '"') {
            size_t q = src.find('"', p + 1);
            if (q == std::string::npos) { if (err) *err = "string sem fechar"; return false; }
            t.t = T_STR; t.s = src.substr(p + 1, q - p - 1); p = q + 1;
        } else {
            static const char* ops3[] = { 0 };
            (void)ops3;
            static const char* ops2[] = { "**", "&&", "||", "^^", "!=", "<=", ">=", ":=", 0 };
            std::string two = src.substr(p, 2);
            bool found = false;
            for (int k = 0; ops2[k]; ++k) if (two == ops2[k]) { t.s = two; p += 2; found = true; break; }
            if (!found) {
                if (std::strchr("+-*/%!~&|^=<>(),[]", c)) { t.s = std::string(1, (char)c); ++p; }
                else { if (err) *err = std::string("caractere invalido '") + (char)c + "'"; return false; }
            }
        }
        out.push_back(t);
    }
    Tok e; e.t = T_END; e.i = 0; e.f = 0; out.push_back(e);
    return true;
}

bool ieq(const std::string& a, const char* b) {
    size_t n = std::strlen(b);
    if (a.size() != n) return false;
    for (size_t i = 0; i < n; ++i) if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}
bool is_redirect_name(const std::string& s) {
    static const char* names[] = { "parent", "root", "helper", "target", "partner", "enemy", "enemynear", "playerid", 0 };
    for (int i = 0; names[i]; ++i) if (ieq(s, names[i])) return true;
    return false;
}

// ---------------------------------------------------------------- parser
struct Parser {
    std::vector<Tok> tk; size_t p; std::string err;
    Parser() : p(0) {}
    const Tok& cur() const { return tk[p]; }
    const Tok& at(size_t k) const { return tk[p + k < tk.size() ? p + k : tk.size() - 1]; }
    bool isop(const char* s, size_t k = 0) const { return at(k).t == T_OP && at(k).s == s; }
    bool fail(const std::string& m) { if (err.empty()) err = m; return false; }

    static ExprPtr mk(ExprNode::Type t) { ExprPtr n(new ExprNode()); n->type = t; return n; }

    static int prec(const std::string& op) {
        if (op == "||") return 1;
        if (op == "^^") return 2;
        if (op == "&&") return 3;
        if (op == "|")  return 4;
        if (op == "^")  return 5;
        if (op == "&")  return 6;
        if (op == ":=") return 7;
        if (op == "=" || op == "!=") return 8;
        if (op == "<" || op == "<=" || op == ">" || op == ">=") return 9;
        if (op == "+" || op == "-") return 10;
        if (op == "*" || op == "/" || op == "%") return 11;
        if (op == "**") return 12;
        return -1;
    }

    // '(' na posicao k..: ha virgula no nivel superior antes do fecha-parenteses correspondente?
    bool paren_has_top_comma(size_t k) const {
        int depth = 0;
        for (size_t i = p + k; i < tk.size(); ++i) {
            if (tk[i].t != T_OP) continue;
            const std::string& s = tk[i].s;
            if (s == "(" || s == "[") ++depth;
            else if (s == ")" || s == "]") { if (--depth == 0) return false; }
            else if (s == "," && depth == 1) return true;
        }
        return false;
    }

    ExprPtr parse_binary(int minPrec) {
        ExprPtr lhs = parse_unary();
        if (!lhs) return nullptr;
        for (;;) {
            if (cur().t != T_OP) break;
            std::string op = cur().s;
            int pr = prec(op);
            if (pr < 0 || pr < minPrec) break;
            // intervalo: "= [y,z]" / "= (y,z)" / "!= ..."
            if ((op == "=" || op == "!=") &&
                (isop("[", 1) || (isop("(", 1) && paren_has_top_comma(1)))) {
                ++p;
                ExprPtr iv = mk(ExprNode::INTERVAL);
                iv->str = op; iv->lo = cur().s[0]; ++p;
                ExprPtr y = parse_binary(1); if (!y) return nullptr;
                if (!isop(",")) { fail("esperava ',' no intervalo"); return nullptr; }
                ++p;
                ExprPtr z = parse_binary(1); if (!z) return nullptr;
                if (!(isop(")") || isop("]"))) { fail("esperava fim de intervalo"); return nullptr; }
                iv->hi = cur().s[0]; ++p;
                iv->kids.push_back(std::move(lhs)); iv->kids.push_back(std::move(y)); iv->kids.push_back(std::move(z));
                lhs = std::move(iv);
                // Doc: intervalo so pode aparecer no fim da expressao (ou do trecho entre parenteses).
                if (cur().t == T_OP && prec(cur().s) >= 0 && prec(cur().s) >= minPrec) {
                    fail("intervalo so pode aparecer no fim da expressao"); return nullptr;
                }
                continue;
            }
            ++p;
            bool right = (op == ":=");
            ExprPtr rhs = parse_binary(right ? pr : pr + 1);
            if (!rhs) return nullptr;
            ExprPtr b = mk(ExprNode::BINARY); b->str = op;
            b->kids.push_back(std::move(lhs)); b->kids.push_back(std::move(rhs));
            lhs = std::move(b);
        }
        return lhs;
    }

    ExprPtr parse_unary() {
        if (cur().t == T_OP && (cur().s == "-" || cur().s == "!" || cur().s == "~")) {
            ExprPtr u = mk(ExprNode::UNARY); u->str = cur().s; ++p;
            ExprPtr k = parse_unary(); if (!k) return nullptr;
            u->kids.push_back(std::move(k));
            return u;
        }
        return parse_primary();
    }

    // lista separada por virgulas; elemento pode comecar com operador relacional ("... , >= 0")
    ExprPtr parse_list(bool* wasList) {
        std::vector<ExprPtr> items;
        for (;;) {
            ExprPtr e;
            if (!items.empty() && cur().t == T_OP && prec(cur().s) >= 8 && cur().s != ":=") {
                ExprPtr pt = mk(ExprNode::PARTIAL); pt->str = cur().s; ++p;
                ExprPtr r = parse_binary(1); if (!r) return nullptr;
                pt->kids.push_back(std::move(r)); e = std::move(pt);
            } else e = parse_binary(1);
            if (!e) return nullptr;
            items.push_back(std::move(e));
            if (isop(",")) { ++p; continue; }
            break;
        }
        if (wasList) *wasList = items.size() > 1;
        if (items.size() == 1) return std::move(items[0]);
        ExprPtr l = mk(ExprNode::LIST);
        for (size_t i = 0; i < items.size(); ++i) l->kids.push_back(std::move(items[i]));
        return l;
    }

    ExprPtr parse_primary() {
        const Tok& t = cur();
        if (t.t == T_INT)   { ExprPtr n = mk(ExprNode::INT);   n->ival = t.i; ++p; return n; }
        if (t.t == T_FLOAT) { ExprPtr n = mk(ExprNode::FLOAT); n->fval = t.f; ++p; return n; }
        if (t.t == T_STR)   { ExprPtr n = mk(ExprNode::STRING); n->str = t.s; ++p; return n; }
        if (t.t == T_OP && t.s == "(") {
            ++p;
            bool isList = false;
            ExprPtr e = parse_list(&isList); if (!e) return nullptr;
            if (!isop(")")) { fail("esperava ')'"); return nullptr; }
            ++p;
            if (isList && e->kids.size() == 2 && e->kids[0]->type == ExprNode::CALL && is_redirect_name(e->kids[0]->str)) {
                ExprPtr r = mk(ExprNode::REDIRECT);
                r->kids.push_back(std::move(e->kids[0])); r->kids.push_back(std::move(e->kids[1]));
                return r;
            }
            return e;
        }
        if (t.t == T_IDENT) {
            ExprPtr c = mk(ExprNode::CALL); c->str = t.s; ++p;
            // triggers de duas palavras: "Vel X", "Pos Y"
            if (cur().t == T_IDENT && (ieq(cur().s, "x") || ieq(cur().s, "y") || ieq(cur().s, "z"))) {
                c->str += " " + cur().s; ++p;
            }
            if (isop("(")) {
                ++p;
                if (!isop(")")) {
                    bool dummy = false;
                    ExprPtr a = parse_list(&dummy); if (!a) return nullptr;
                    if (a->type == ExprNode::LIST) for (size_t i = 0; i < a->kids.size(); ++i) c->kids.push_back(std::move(a->kids[i]));
                    else c->kids.push_back(std::move(a));
                }
                if (!isop(")")) { fail("esperava ')' depois dos argumentos de " + c->str); return nullptr; }
                ++p;
            }
            // "parent, Time" / "target(3), Life" sem parenteses externos
            if (is_redirect_name(c->str) && isop(",")) {
                ++p;
                ExprPtr inner = parse_binary(1); if (!inner) return nullptr;
                ExprPtr r = mk(ExprNode::REDIRECT);
                r->kids.push_back(std::move(c)); r->kids.push_back(std::move(inner));
                return r;
            }
            return c;
        }
        fail(t.t == T_END ? "expressao incompleta" : "token inesperado '" + t.s + "'");
        return nullptr;
    }
};
} // namespace

ExprPtr expr_parse(const std::string& text, std::string* error) {
    Parser ps;
    if (!lex(text, ps.tk, error)) return nullptr;
    if (ps.tk.size() == 1) { if (error) *error = "expressao vazia"; return nullptr; }
    bool isList = false;
    ExprPtr e = ps.parse_list(&isList);
    if (e && ps.cur().t != T_END) { ps.fail("sobrou texto: '" + ps.cur().s + "'"); e.reset(); }
    if (!e && error && error->empty()) *error = ps.err;
    return e;
}

std::string expr_to_string(const ExprNode* n) {
    char b[64];
    switch (n->type) {
    case ExprNode::INT:    std::snprintf(b, sizeof b, "%d", n->ival); return b;
    case ExprNode::FLOAT:  std::snprintf(b, sizeof b, "%g", (double)n->fval); { std::string s = b; if (s.find_first_of(".eEn") == std::string::npos) s += ".0"; return s; }
    case ExprNode::STRING: return "\"" + n->str + "\"";
    default: break;
    }
    std::string s = "(";
    switch (n->type) {
    case ExprNode::CALL: s += "call " + n->str; break;
    case ExprNode::UNARY: s += "u" + n->str; break;
    case ExprNode::BINARY: s += n->str; break;
    case ExprNode::INTERVAL: s += n->str + std::string(1, n->lo) + std::string(1, n->hi); break;
    case ExprNode::PARTIAL: s += "partial " + n->str; break;
    case ExprNode::REDIRECT: s += "redirect"; break;
    case ExprNode::LIST: s += "list"; break;
    default: break;
    }
    for (size_t i = 0; i < n->kids.size(); ++i) s += " " + expr_to_string(n->kids[i].get());
    return s + ")";
}

// ---------------------------------------------------------------- avaliador
bool ExprValue::nonzero() const {
    switch (kind) { case INT: return i != 0; case FLOAT: return f != 0.0f; default: return false; }
}

namespace {
typedef ExprValue V;
bool isnum(const V& v) { return v.kind == V::INT || v.kind == V::FLOAT; }
float tof(const V& v) { return v.kind == V::INT ? (float)v.i : v.f; }

V arith(const std::string& op, const V& a, const V& b) {
    if (!isnum(a) || !isnum(b)) return V::Bottom();
    bool ints = a.kind == V::INT && b.kind == V::INT;
    if (op == "+") return ints ? V::Int((int)((unsigned)a.i + (unsigned)b.i)) : V::Float(tof(a) + tof(b));
    if (op == "-") return ints ? V::Int((int)((unsigned)a.i - (unsigned)b.i)) : V::Float(tof(a) - tof(b));
    if (op == "*") return ints ? V::Int((int)((unsigned)a.i * (unsigned)b.i)) : V::Float(tof(a) * tof(b));
    if (op == "/") {
        if (ints) { if (b.i == 0 || (a.i == INT_MIN && b.i == -1)) return V::Bottom(); return V::Int(a.i / b.i); }
        if (tof(b) == 0.0f) return V::Bottom();
        return V::Float(tof(a) / tof(b));
    }
    if (op == "%") { if (!ints || b.i == 0 || (a.i == INT_MIN && b.i == -1)) return V::Bottom(); return V::Int(a.i % b.i); }
    if (op == "**") {
        if (ints && a.i >= 0 && b.i >= 0) {
            if (b.i == 0) return V::Int(1);                  // 0**0 = 1
            if (a.i == 0) return V::Int(0);
            if (a.i == 1) return V::Int(1);
            long long r = 1;
            for (int k = 0; k < b.i; ++k) { r *= a.i; if (r > INT_MAX) return V::Int(INT_MAX); }
            return V::Int((int)r);
        }
        double r = std::pow((double)tof(a), (double)tof(b));
        if (r != r) return V::Bottom();
        return V::Float((float)r);
    }
    return V::Bottom();
}

V compare(const std::string& op, const V& a, const V& b) {
    if (a.kind == V::STR || b.kind == V::STR) {
        if (a.kind == V::STR && b.kind == V::STR && (op == "=" || op == "!=")) {
            bool eq = a.s.size() == b.s.size();
            for (size_t i = 0; eq && i < a.s.size(); ++i) eq = std::tolower((unsigned char)a.s[i]) == std::tolower((unsigned char)b.s[i]);
            return V::Int((op == "=") == eq ? 1 : 0);
        }
        return V::Bottom();
    }
    if (!isnum(a) || !isnum(b)) return V::Bottom();
    bool r;
    if (a.kind == V::INT && b.kind == V::INT) {
        if (op == "=") r = a.i == b.i; else if (op == "!=") r = a.i != b.i; else if (op == "<") r = a.i < b.i;
        else if (op == "<=") r = a.i <= b.i; else if (op == ">") r = a.i > b.i; else r = a.i >= b.i;
    } else {
        float x = tof(a), y = tof(b);
        if (op == "=") r = x == y; else if (op == "!=") r = x != y; else if (op == "<") r = x < y;
        else if (op == "<=") r = x <= y; else if (op == ">") r = x > y; else r = x >= y;
    }
    return V::Int(r ? 1 : 0);
}

V bitop(const std::string& op, const V& a, const V& b) {
    if (a.kind != V::INT || b.kind != V::INT) return V::Bottom();
    if (op == "&") return V::Int(a.i & b.i);
    if (op == "|") return V::Int(a.i | b.i);
    return V::Int(a.i ^ b.i);
}
} // namespace

ExprValue expr_eval(const ExprNode* n, ExprContext& ctx) {
    typedef ExprNode N;
    switch (n->type) {
    case N::INT:    return V::Int(n->ival);
    case N::FLOAT:  return V::Float(n->fval);
    case N::STRING: return V::Str(n->str);
    case N::CALL: {
        std::vector<V> args;
        for (size_t i = 0; i < n->kids.size(); ++i) args.push_back(expr_eval(n->kids[i].get(), ctx));
        V out;
        if (!ctx.call(n->str, args, out)) return V::Bottom();
        return out;
    }
    case N::UNARY: {
        V v = expr_eval(n->kids[0].get(), ctx);
        if (n->str == "!") return isnum(v) ? V::Int(v.nonzero() ? 0 : 1) : V::Bottom();
        if (n->str == "-") return v.kind == V::INT ? V::Int((int)(0u - (unsigned)v.i)) : v.kind == V::FLOAT ? V::Float(-v.f) : V::Bottom();
        if (n->str == "~") return v.kind == V::INT ? V::Int(~v.i) : V::Bottom();
        return V::Bottom();
    }
    case N::BINARY: {
        const std::string& op = n->str;
        if (op == ":=") {
            const N* l = n->kids[0].get();
            V r = expr_eval(n->kids[1].get(), ctx);
            if (l->type != N::CALL) return V::Bottom();
            std::vector<V> args;
            for (size_t i = 0; i < l->kids.size(); ++i) args.push_back(expr_eval(l->kids[i].get(), ctx));
            V out;
            if (!ctx.assign(l->str, args, r, out)) return V::Bottom();
            return out;
        }
        if ((op == "=" || op == "!=") && n->kids[0]->type == N::CALL && n->kids[1]->type == N::STRING) {
            const N* l = n->kids[0].get();
            std::vector<V> args;
            for (size_t i = 0; i < l->kids.size(); ++i) args.push_back(expr_eval(l->kids[i].get(), ctx));
            V out;
            if (ctx.call_eq_str(l->str, args, n->kids[1]->str, out) && out.kind == V::INT)
                return V::Int(op == "=" ? out.i : (out.i ? 0 : 1));
        }
        if ((op == "=" || op == "!=") && n->kids[0]->type == N::CALL) {
            const N* l = n->kids[0].get();
            std::vector<V> args;
            for (size_t i = 0; i < l->kids.size(); ++i) args.push_back(expr_eval(l->kids[i].get(), ctx));
            V rhs = expr_eval(n->kids[1].get(), ctx), out;
            if (n->kids[1]->type != N::STRING && ctx.call_eq(l->str, args, rhs, out) && out.kind == V::INT)
                return V::Int(op == "=" ? out.i : (out.i ? 0 : 1));
        }
        V a = expr_eval(n->kids[0].get(), ctx), b = expr_eval(n->kids[1].get(), ctx);
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "**") return arith(op, a, b);
        if (op == "&&" || op == "||" || op == "^^") {
            if (!isnum(a) || !isnum(b)) return V::Bottom();   // excecoes do "bottom" ainda nao tratadas
            bool x = a.nonzero(), y = b.nonzero();
            return V::Int(op == "&&" ? (x && y) : op == "||" ? (x || y) : (x != y));
        }
        if (op == "&" || op == "|" || op == "^") return bitop(op, a, b);
        return compare(op, a, b);
    }
    case N::INTERVAL: {
        V x = expr_eval(n->kids[0].get(), ctx), y = expr_eval(n->kids[1].get(), ctx), z = expr_eval(n->kids[2].get(), ctx);
        if (!isnum(x) || !isnum(y) || !isnum(z)) return V::Bottom();
        if (x.kind == V::FLOAT || y.kind == V::FLOAT || z.kind == V::FLOAT) { x = V::Float(tof(x)); y = V::Float(tof(y)); z = V::Float(tof(z)); }
        bool inside = (n->lo == '[' ? compare(">=", x, y) : compare(">", x, y)).i == 1 &&
                      (n->hi == ']' ? compare("<=", x, z) : compare("<", x, z)).i == 1;
        return V::Int((n->str == "=") == inside ? 1 : 0);
    }
    case N::REDIRECT: {
        const N* r = n->kids[0].get();
        std::vector<V> args;
        for (size_t i = 0; i < r->kids.size(); ++i) args.push_back(expr_eval(r->kids[i].get(), ctx));
        V out;
        if (!ctx.redirect(r->str, args, n->kids[1].get(), out)) return V::Bottom();
        return out;
    }
    case N::LIST: {
        // "Nome = valor, extra..." (sintaxe antiga). Qualquer outra lista (parametros) nao tem valor unico.
        if (n->kids.empty() || n->kids[0]->type != N::BINARY || n->kids[0]->str != "=" || n->kids[0]->kids[0]->type != N::CALL) return V::Bottom();
        const N* b0 = n->kids[0].get(); const N* l = b0->kids[0].get();
        std::vector<V> args;
        for (size_t i = 0; i < l->kids.size(); ++i) args.push_back(expr_eval(l->kids[i].get(), ctx));
        V rhs = expr_eval(b0->kids[1].get(), ctx);
        std::vector<ExprContext::Extra> ex;
        for (size_t i = 1; i < n->kids.size(); ++i) {
            ExprContext::Extra e;
            const N* k = n->kids[i].get();
            if (k->type == N::PARTIAL) { e.op = k->str; e.value = expr_eval(k->kids[0].get(), ctx); }
            else e.value = expr_eval(k, ctx);
            ex.push_back(e);
        }
        V out;
        if (ctx.call_list(l->str, args, rhs, ex, out)) return out;
        return V::Bottom();
    }
    default: break;   // PARTIAL so existe dentro de listas
    }
    return V::Bottom();
}
