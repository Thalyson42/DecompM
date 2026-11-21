#include "engine/player.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static std::string lower(std::string s) { for (size_t i = 0; i < s.size(); ++i) s[i] = (char)std::tolower((unsigned char)s[i]); return s; }

Player::Player() : stateNo(0), prevStateNo(0), time(0), ctrl(false), anim(0), animElapsed(0),
    posX(0), posY(0), velX(0), velY(0), facing(1), life(1000), power(0),
    stateType('S'), moveType('I'), physics('N') {
    for (int i = 0; i < 60; ++i) var[i] = 0;
    for (int i = 0; i < 40; ++i) fvar[i] = 0;
}

// ---------------------------------------------------------------- contexto dos triggers
struct PlayerCtx : ExprContext {
    Player& p; Engine& e;
    PlayerCtx(Player& pp, Engine& ee) : p(pp), e(ee) {}

    static bool want(const std::vector<ExprValue>& a, size_t n) { return a.size() == n; }
    static float num(const ExprValue& v) { return v.kind == ExprValue::INT ? (float)v.i : v.kind == ExprValue::FLOAT ? v.f : 0.0f; }

    bool call(const std::string& rawName, const std::vector<ExprValue>& a, ExprValue& out) {
        std::string n = lower(rawName);
        typedef ExprValue V;
        if (n == "time")        { out = V::Int(p.time); return true; }
        if (n == "stateno")     { out = V::Int(p.stateNo); return true; }
        if (n == "prevstateno") { out = V::Int(p.prevStateNo); return true; }
        if (n == "ctrl")        { out = V::Int(p.ctrl ? 1 : 0); return true; }
        if (n == "anim")        { out = V::Int(p.anim); return true; }
        if (n == "animtime") {
            int lt = e.anim_looptime(p.anim);
            out = V::Int(lt > 0 ? p.animElapsed - lt : -1);   // lt<0 (infinito): valor provisorio, a confirmar
            return true;
        }
        if (n == "vel x") { out = V::Float(p.velX * (float)p.facing); return true; }   // guardada em coordenada absoluta
        if (n == "vel y") { out = V::Float(p.velY); return true; }
        if (n == "pos x") { out = V::Float(p.posX); return true; }
        if (n == "pos y") { out = V::Float(p.posY); return true; }
        if (n == "life")  { out = V::Int(p.life); return true; }
        if (n == "power") { out = V::Int(p.power); return true; }
        if (n == "alive") { out = V::Int(p.life > 0 ? 1 : 0); return true; }
        if (n == "facing"){ out = V::Int(p.facing); return true; }
        if (n == "statetype") { out = V::Str(std::string(1, p.stateType)); return true; }
        if (n == "movetype")  { out = V::Str(std::string(1, p.moveType)); return true; }
        if (n == "physics")   { out = V::Str(std::string(1, p.physics)); return true; }
        if (n == "var" && want(a, 1) && a[0].kind == V::INT) { out = (a[0].i >= 0 && a[0].i < 60) ? V::Int(p.var[a[0].i]) : V::Bottom(); return true; }
        if (n == "fvar" && want(a, 1) && a[0].kind == V::INT) { out = (a[0].i >= 0 && a[0].i < 40) ? V::Float(p.fvar[a[0].i]) : V::Bottom(); return true; }
        if (n == "const" && !a.empty()) return false;   // Const(nome) chega aqui como CALL sem args em "nome"; ver abaixo
        if (n == "abs" && want(a, 1)) {
            if (a[0].kind == V::INT) out = V::Int(a[0].i < 0 ? -a[0].i : a[0].i);
            else if (a[0].kind == V::FLOAT) out = V::Float(std::fabs(a[0].f));
            else out = V::Bottom();
            return true;
        }
        if (n == "floor" && want(a, 1) && (a[0].kind == V::INT || a[0].kind == V::FLOAT)) { out = V::Int((int)std::floor(num(a[0]))); return true; }
        if (n == "ceil"  && want(a, 1) && (a[0].kind == V::INT || a[0].kind == V::FLOAT)) { out = V::Int((int)std::ceil(num(a[0]))); return true; }
        if (n == "ifelse" && want(a, 3)) {
            bool c = a[0].nonzero();
            const V& r = c ? a[1] : a[2];
            if (a[1].kind == V::FLOAT || a[2].kind == V::FLOAT) out = r.kind == V::INT ? V::Float((float)r.i) : r;  // promocao
            else out = r;
            return true;
        }
        if (n == "pi") { out = V::Float(3.14159265f); return true; }
        // letras de atributo usadas como valores ("StateType = S", "MoveType = A")
        if (a.empty() && (n == "s" || n == "c" || n == "a" || n == "l" || n == "i" || n == "h" || n == "n" || n == "u")) {
            out = V::Str(rawName); return true;
        }
        e.cache_.size();   // (sem efeito; mantem 'e' usado)
        p.skipped["trigger:" + rawName]++;
        return false;
    }
    bool call_eq_str(const std::string& rawName, const std::vector<ExprValue>&, const std::string& rhs, ExprValue& out) {
        if (lower(rawName) == "command") { out = ExprValue::Int(p.commands.count(lower(rhs)) ? 1 : 0); return true; }
        return false;
    }
    bool assign(const std::string& rawName, const std::vector<ExprValue>& a, const ExprValue& v, ExprValue& out) {
        std::string n = lower(rawName);
        if (a.size() != 1 || a[0].kind != ExprValue::INT) return false;
        if (n == "var" && a[0].i >= 0 && a[0].i < 60) { int x = v.kind == ExprValue::FLOAT ? (int)v.f : v.i; if (!(v.kind == ExprValue::INT || v.kind == ExprValue::FLOAT)) return false; p.var[a[0].i] = x; out = ExprValue::Int(x); return true; }
        if (n == "fvar" && a[0].i >= 0 && a[0].i < 40) { if (!(v.kind == ExprValue::INT || v.kind == ExprValue::FLOAT)) return false; float x = num(v); p.fvar[a[0].i] = x; out = ExprValue::Float(x); return true; }
        return false;
    }
    bool redirect(const std::string& n, const std::vector<ExprValue>&, const ExprNode*, ExprValue&) {
        p.skipped["redirect:" + n]++; return false;
    }
};

// Const(nome) chega do parser como CALL "Const" com um argumento CALL "nome" (sem args). Tratamos aqui:
// o avaliador entrega o argumento ja avaliado (bottom), entao Const e resolvido por um pre-passo no Engine::eval.

// ---------------------------------------------------------------- Engine
const ExprNode* Engine::parse(const std::string& text) {
    std::map<std::string, std::shared_ptr<ExprNode> >::iterator it = cache_.find(text);
    if (it != cache_.end()) return it->second.get();
    std::string err; ExprPtr e = expr_parse(text, &err);
    std::shared_ptr<ExprNode> sp(e.release());
    cache_[text] = sp;
    return sp.get();
}

// Substitui Const(nome) por literais antes de avaliar (o argumento e um nome, nao uma expressao).
static void fold_consts(ExprNode* n, const Character& ch) {
    for (size_t i = 0; i < n->kids.size(); ++i) fold_consts(n->kids[i].get(), ch);
    if (n->type == ExprNode::CALL && lower(n->str) == "const" && n->kids.size() == 1 && n->kids[0]->type == ExprNode::CALL) {
        float v = 0;
        std::string name = n->kids[0]->str;
        if (ch.constant(name, v)) {
            n->type = ExprNode::FLOAT; n->fval = v; n->kids.clear(); n->str.clear();
        }
    }
}

bool Engine::eval(Player& p, const std::string& text, ExprValue& out) {
    const ExprNode* n = parse(text);
    if (!n) return false;
    // o cache guarda a arvore ja com Const resolvido; resolvemos uma unica vez
    static std::set<const ExprNode*> done;
    if (!done.count(n)) { fold_consts(const_cast<ExprNode*>(n), ch_); done.insert(n); }
    PlayerCtx ctx(p, *this);
    out = expr_eval(n, ctx);
    return true;
}

int Engine::anim_looptime(int action) const {
    std::map<int, AirAction>::const_iterator it = ch_.anims.find(action);
    if (it == ch_.anims.end()) return 0;
    return it->second.looptime();
}

void Engine::set_anim(Player& p, int action, int elem) {
    p.anim = action; p.animElapsed = 0;
    if (elem > 1) {
        std::map<int, AirAction>::const_iterator it = ch_.anims.find(action);
        if (it != ch_.anims.end())
            for (int i = 0; i < elem - 1 && i < (int)it->second.frames.size(); ++i)
                if (it->second.frames[i].time > 0) p.animElapsed += it->second.frames[i].time;
    }
}

bool Engine::param_values(Player& p, const CnsController& c, const char* key, std::vector<ExprValue>& out) {
    for (size_t i = 0; i < c.params.size(); ++i) {
        if (!ini_iequals(c.params[i].key, key)) continue;
        const ExprNode* n = parse(c.params[i].value);
        if (!n) return false;
        static std::set<const ExprNode*> done;
        if (!done.count(n)) { fold_consts(const_cast<ExprNode*>(n), ch_); done.insert(n); }
        PlayerCtx ctx(p, *this);
        out.clear();
        if (n->type == ExprNode::LIST) for (size_t k = 0; k < n->kids.size(); ++k) out.push_back(expr_eval(n->kids[k].get(), ctx));
        else out.push_back(expr_eval(n, ctx));
        return true;
    }
    return false;
}
static float to_f(const ExprValue& v) { return v.kind == ExprValue::INT ? (float)v.i : v.kind == ExprValue::FLOAT ? v.f : 0.0f; } // bottom -> 0 (doc)
static int   to_i(const ExprValue& v) { return v.kind == ExprValue::INT ? v.i : v.kind == ExprValue::FLOAT ? (int)v.f : 0; }
bool Engine::param_int(Player& p, const CnsController& c, const char* key, int& out) {
    std::vector<ExprValue> v; if (!param_values(p, c, key, v) || v.empty()) return false; out = to_i(v[0]); return true;
}
bool Engine::param_float(Player& p, const CnsController& c, const char* key, float& out) {
    std::vector<ExprValue> v; if (!param_values(p, c, key, v) || v.empty()) return false; out = to_f(v[0]); return true;
}

bool Engine::enter_state(Player& p, int n) {
    const CnsStatedef* sd = ch_.state(n);
    if (!sd) return false;
    p.prevStateNo = p.stateNo; p.stateNo = n; p.time = 0; p.persist.clear();
    // Parametros do Statedef; omitidos assumem S / I / N (doc). "U" = sem mudanca.
    std::string v;
    v = sd->param("type");     p.stateType = v.empty() ? 'S' : (char)std::toupper((unsigned char)v[0]) == 'U' ? p.stateType : (char)std::toupper((unsigned char)v[0]);
    v = sd->param("movetype"); p.moveType  = v.empty() ? 'I' : (char)std::toupper((unsigned char)v[0]) == 'U' ? p.moveType  : (char)std::toupper((unsigned char)v[0]);
    v = sd->param("physics");  p.physics   = v.empty() ? 'N' : (char)std::toupper((unsigned char)v[0]) == 'U' ? p.physics   : (char)std::toupper((unsigned char)v[0]);
    // Os demais parametros sao expressoes; reutiliza o mesmo caminho dos controllers via um controller sintetico.
    CnsController tmp; tmp.stateNumber = n; tmp.line = sd->line;
    tmp.params.assign(sd->params.begin(), sd->params.end());
    int iv; std::vector<ExprValue> vv;
    if (param_int(p, tmp, "anim", iv)) set_anim(p, iv, 1);
    if (param_values(p, tmp, "velset", vv)) { if (vv.size() > 0) p.velX = to_f(vv[0]) * (float)p.facing; if (vv.size() > 1) p.velY = to_f(vv[1]); }
    if (param_int(p, tmp, "ctrl", iv)) p.ctrl = iv != 0;
    if (param_int(p, tmp, "poweradd", iv)) p.power += iv;
    return true;
}

bool Engine::triggered(Player& p, const CnsController& c) {
    ExprValue v;
    for (size_t i = 0; i < c.triggerAll.size(); ++i) {
        if (!eval(p, c.triggerAll[i], v) || !v.nonzero()) return false;
    }
    for (size_t g = 0; g < c.triggers.size(); ++g) {
        if (c.triggers[g].empty()) break;           // lacuna: os seguintes sao ignorados (doc)
        bool all = true;
        for (size_t i = 0; i < c.triggers[g].size() && all; ++i)
            if (!eval(p, c.triggers[g][i], v) || !v.nonzero()) all = false;
        if (all) return true;
    }
    return false;
}

bool Engine::exec(Player& p, const CnsController& c) {
    std::string t = lower(c.type);
    int iv; float fv; std::vector<ExprValue> vv;
    if (t == "changestate" || t == "selfstate") {
        if (!param_int(p, c, "value", iv)) return false;
        if (!enter_state(p, iv)) { p.skipped["state-inexistente:" + std::to_string(iv)]++; return false; }
        int x;
        if (param_int(p, c, "ctrl", x)) p.ctrl = x != 0;
        if (param_int(p, c, "anim", x)) set_anim(p, x, 1);
        return true;
    }
    if (t == "ctrlset")   { if (param_int(p, c, "value", iv)) p.ctrl = iv != 0; return false; }
    if (t == "changeanim"){ int el = 1; param_int(p, c, "elem", el); if (param_int(p, c, "value", iv)) set_anim(p, iv, el); return false; }
    if (t == "velset")    { if (param_float(p, c, "x", fv)) p.velX = fv * (float)p.facing; if (param_float(p, c, "y", fv)) p.velY = fv; return false; }
    if (t == "veladd")    { if (param_float(p, c, "x", fv)) p.velX += fv * (float)p.facing; if (param_float(p, c, "y", fv)) p.velY += fv; return false; }
    if (t == "velmul")    { if (param_float(p, c, "x", fv)) p.velX *= fv; if (param_float(p, c, "y", fv)) p.velY *= fv; return false; }
    if (t == "posset")    { if (param_float(p, c, "x", fv)) p.posX = fv; if (param_float(p, c, "y", fv)) p.posY = fv; return false; }
    // PosAdd x relativo ao lado: suposicao baseada na documentacao (a confirmar no exe)
    if (t == "posadd")    { if (param_float(p, c, "x", fv)) p.posX += fv * (float)p.facing; if (param_float(p, c, "y", fv)) p.posY += fv; return false; }
    if (t == "poweradd")  { if (param_int(p, c, "value", iv)) p.power += iv; return false; }
    if (t == "powerset")  { if (param_int(p, c, "value", iv)) p.power = iv; return false; }
    if (t == "lifeadd")   { if (param_int(p, c, "value", iv)) p.life += iv; return false; }
    if (t == "lifeset")   { if (param_int(p, c, "value", iv)) p.life = iv; return false; }
    if (t == "varset" || t == "varadd") {
        int idx;
        if (param_int(p, c, "v", idx) && idx >= 0 && idx < 60) {
            if (param_int(p, c, "value", iv)) { if (t == "varset") p.var[idx] = iv; else p.var[idx] += iv; }
        } else if (param_int(p, c, "fv", idx) && idx >= 0 && idx < 40) {
            if (param_float(p, c, "value", fv)) { if (t == "varset") p.fvar[idx] = fv; else p.fvar[idx] += fv; }
        }
        return false;
    }
    if (t == "statetypeset") {
        std::string s;
        for (size_t i = 0; i < c.params.size(); ++i) {
            if (c.params[i].value.empty()) continue;
            char ch = (char)std::toupper((unsigned char)c.params[i].value[0]);
            if (ini_iequals(c.params[i].key, "statetype")) p.stateType = ch;
            else if (ini_iequals(c.params[i].key, "movetype")) p.moveType = ch;
            else if (ini_iequals(c.params[i].key, "physics")) p.physics = ch;
        }
        return false;
    }
    if (t == "null") return false;
    p.skipped["controller:" + c.type]++;
    return false;
}

// Porte de FUN_00434db0 (mugen.exe 1.1b1), parte que escolhe a "intencao" de movimento.
// Entradas lidas dos comandos padrao holdfwd/holdback/holdup/holddown (o exe le as teclas direto).
// NAO portado ainda: guarda automatica (intencao 0x28 -> estado 120), pulo no ar (intencao 10 -> estado 45),
// a limpeza do campo +0x248, o flag +0x2b2 (bloqueio de andar) e a trava de rodada (+0x12754 == 3).
void Engine::default_movement(Player& p) {
    bool fwd  = p.commands.count("holdfwd")  != 0;
    bool back = p.commands.count("holdback") != 0;
    bool up   = p.commands.count("holdup")   != 0;
    bool down = p.commands.count("holddown") != 0;
    int intent = -1;
    bool keepCrouch = false;
    if (p.stateType == 'S') {
        if (fwd != back) intent = 1;     // so uma direcao horizontal: andar
        if (down) intent = 11;           // 0xb: agachar (sobrepoe andar)
        if (up)   intent = 7;            // pular (sobrepoe agachar)
    }
    if (p.stateType == 'C' && down) keepCrouch = true;
    if (fwd && back) intent = -1;        // as duas direcoes juntas cancelam qualquer intencao
    bool handled = false;
    if (intent == 11) {
        if (p.ctrl && p.stateNo != 10 && p.stateNo != 11) enter_state(p, 10);
        handled = true;
    } else if (intent == 1) {
        if (p.ctrl && p.stateNo != 20) enter_state(p, 20);
        handled = true;
    } else if (intent == 7) {
        if (p.ctrl) enter_state(p, 40);
        handled = true;
    }
    if (handled) return;
    // Sem intencao tratada: andando volta para o estado 0 (so troca os campos, sem validar o estado)
    if (p.stateNo == 20) { p.prevStateNo = 20; p.stateNo = 0; p.time = 0; p.persist.clear(); }
    // Agachado, com controle e sem segurar para baixo: levantar (estado 12)
    if (p.stateType == 'C' && p.ctrl && !keepCrouch) enter_state(p, 12);
}

// Porte da parte de fisica de FUN_0043c480 (mugen.exe 1.1b1), na ordem do exe:
//  1) pos += vel (a velocidade guardada ja inclui o lado; VelSet/VelAdd multiplicam por facing)
//  2) physics S: vel.x *= movement.stand.friction;  C: vel.x *= movement.crouch.friction
//     A: vel.y += movement.yaccel; se vel.y > 0, pos.y > 0 e stateno != 105 -> estado 52 (pouso), Time = 0
// NAO portado: hitpause (decrementa em vez de mover), escala entre localcoords diferentes, o limiar de atrito
// do exe (o common1.cns ja zera vel x abaixo de movement.stand.friction.threshold) e o resto do pouso.
void Engine::physics(Player& p) {
    p.posX += p.velX;
    p.posY += p.velY;
    float k = 0;
    if (p.physics == 'S') { if (ch_.constant("movement.stand.friction", k))  p.velX *= k; }
    else if (p.physics == 'C') { if (ch_.constant("movement.crouch.friction", k)) p.velX *= k; }
    else if (p.physics == 'A') {
        if (ch_.constant("movement.yaccel", k)) p.velY += k;
        if (p.stateNo != 105 && p.velY > 0 && p.posY > 0) {
            if (ch_.state(52)) { p.prevStateNo = p.stateNo; p.stateNo = 52; p.time = 0; p.persist.clear(); }
        }
    }
}

void Engine::tick(Player& p) {
    default_movement(p);
    physics(p);
    // 1) estados especiais -3, -2, -1: uma passada; ChangeState aborta so o restante daquele estado
    for (int sn = -3; sn <= -1; ++sn) {
        const CnsStatedef* sd = ch_.state(sn);
        if (!sd) continue;
        for (size_t i = 0; i < sd->controllers.size(); ++i) {
            const CnsController& c = sd->controllers[i];
            if (!triggered(p, c)) continue;
            if (exec(p, c)) break;
        }
    }
    // 2) estado atual; apos ChangeState recomeca do inicio do novo estado (limite contra lacos infinitos)
    int transitions = 0;
    bool restart = true;
    while (restart && transitions <= 2500) {   // o exe aborta com "State machine stuck in loop" apos 2501 voltas
        restart = false;
        const CnsStatedef* sd = ch_.state(p.stateNo);
        if (!sd) break;
        for (size_t i = 0; i < sd->controllers.size(); ++i) {
            const CnsController& c = sd->controllers[i];
            if (!triggered(p, c)) continue;
            // persistent: 1 = todo tick (padrao); 0 = uma vez por entrada no estado; n>1 = 1 a cada n ticks verdadeiros
            int per = 1;
            if (!c.persistent.empty()) per = std::atoi(c.persistent.c_str());
            if (per != 1) {
                char key[64]; std::snprintf(key, sizeof key, "%d#%zu", p.stateNo, i);
                int& cnt = p.persist[key];
                if (per <= 0) { if (cnt > 0) continue; }
                else if (cnt % per != 0) { ++cnt; continue; }
                ++cnt;
            }
            if (exec(p, c)) { ++transitions; restart = true; break; }
        }
    }
    if (restart) p.skipped["state-machine-stuck"]++;
    // 3) fim do tick
    ++p.time;
    ++p.animElapsed;
    int lt = anim_looptime(p.anim);
    if (lt > 0 && p.animElapsed > lt) p.animElapsed -= lt;   // animacao em loop
}
