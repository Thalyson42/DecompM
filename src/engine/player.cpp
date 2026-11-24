#include "engine/player.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static std::string lower(std::string s) { for (size_t i = 0; i < s.size(); ++i) s[i] = (char)std::tolower((unsigned char)s[i]); return s; }

Player::Player() : stateNo(0), prevStateNo(0), time(0), ctrl(false), anim(0), animElapsed(0),
    posX(0), posY(0), velX(0), velY(0), facing(1), life(1000), power(0),
    pauseTime(0), hitTime(0), moveContact(false), moveHit(false), moveGuarded(false), guarding(false),
    gethitDamage(0), airJumpsLeft(0), inputEvent(0), defenceMul(1.0f), lifeMax(1000), alive(true), enemy(0), defaultGuardDist(-1), life0(1000),
    stateType('S'), moveType('I'), physics('N') {
    for (int i = 0; i < 60; ++i) var[i] = 0;
    for (int i = 0; i < 40; ++i) fvar[i] = 0;
    for (int i = 0; i < 8; ++i) assertFlags[i] = 0;
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
        if (n == "movecontact") { out = V::Int(p.moveContact ? 1 : 0); return true; }
        if (n == "movehit")     { out = V::Int(p.moveHit ? 1 : 0); return true; }
        if (n == "moveguarded") { out = V::Int(p.moveGuarded ? 1 : 0); return true; }
        if (n == "inguarddist") {   // porte de Player_InGuardDist (0x004347A0): oponente atacando e a distancia a frente dele dentro da guard.dist
            const Player* o = p.enemy; int r = 0;
            if (o && o->moveType == 'A') {
                int gd = o->hitdef.guardDist >= 0 ? o->hitdef.guardDist : o->defaultGuardDist;
                float d = (p.posX - o->posX) * (float)o->facing;
                if (d >= 0.0f && d < (float)gd) r = 1;
            }
            out = V::Int(r); return true;
        }
        if (n == "hitshakeover"){ out = V::Int(p.pauseTime <= 0 ? 1 : 0); return true; }
        if (n == "hitover")     { out = V::Int(p.hitTime <= 0 ? 1 : 0); return true; }
        if (n == "hitpausetime"){ out = V::Int(p.pauseTime); return true; }
        if (n == "selfanimexist" && want(a, 1) && a[0].kind == V::INT) { out = V::Int(e.anim_exists(a[0].i) ? 1 : 0); return true; }
        if (n == "animelemtime" && want(a, 1) && a[0].kind == V::INT) {
            int t; if (e.anim_elem_time(p, a[0].i, t)) { out = V::Int(t); return true; } out = V::Bottom(); return true;
        }
        if (n.compare(0, 10, "gethitvar:") == 0) {
            const GetHitVars& g = p.gethit; std::string v = n.substr(10);
            if (v == "animtype") out = V::Int(g.animType);
            else if (v == "groundtype") out = V::Int(g.groundType);
            else if (v == "airtype") out = V::Int(g.airType);
            else if (v == "slidetime") out = V::Int(g.slideTime);
            else if (v == "ctrltime") out = V::Int(g.ctrlTime);
            else if (v == "hittime") out = V::Int(g.hitTime);
            else if (v == "fall") out = V::Int(g.fall);
            else if (v == "damage") out = V::Int(g.damage);
            else if (v == "xvel") out = V::Float(g.xvel * (float)p.facing);   // relativo ao lado de quem apanha (suposicao)
            else if (v == "yvel") out = V::Float(g.yvel);
            else if (v == "yaccel") out = V::Float(g.yaccel);
            else if (v == "fall.yvel") out = V::Float(g.fallYvel);
            else { p.skipped["gethitvar:" + v]++; return false; }
            return true;
        }
        // letras de atributo usadas como valores ("StateType = S", "MoveType = A")
        if (a.empty() && (n == "s" || n == "c" || n == "a" || n == "l" || n == "i" || n == "h" || n == "n" || n == "u")) {
            out = V::Str(rawName); return true;
        }
        e.cache_.size();   // (sem efeito; mantem 'e' usado)
        p.skipped["trigger:" + rawName]++;
        return false;
    }
    // "AnimElem = n" equivale a AnimElemTime(n) = 0
    bool call_eq(const std::string& rawName, const std::vector<ExprValue>&, const ExprValue& rhs, ExprValue& out) {
        if (lower(rawName) == "animelem" && rhs.kind == ExprValue::INT) {
            int t; out = ExprValue::Int(e.anim_elem_time(p, rhs.i, t) && t == 0 ? 1 : 0); return true;
        }
        return false;
    }
    // "AnimElem = n, op v" (AnimElemTime(n) op v) e "TimeMod = a, b" (Time % a = b)
    bool call_list(const std::string& rawName, const std::vector<ExprValue>&, const ExprValue& rhs,
                   const std::vector<ExprContext::Extra>& ex, ExprValue& out) {
        std::string n = lower(rawName);
        if (n == "animelem" && rhs.kind == ExprValue::INT && ex.size() == 1) {
            int t; if (!e.anim_elem_time(p, rhs.i, t)) { out = ExprValue::Bottom(); return true; }
            const ExprContext::Extra& x = ex[0];
            if (x.value.kind != ExprValue::INT) { out = ExprValue::Bottom(); return true; }
            bool r = x.op == "=" ? t == x.value.i : x.op == "!=" ? t != x.value.i : x.op == "<" ? t < x.value.i
                   : x.op == "<=" ? t <= x.value.i : x.op == ">" ? t > x.value.i : t >= x.value.i;
            out = ExprValue::Int(r ? 1 : 0); return true;
        }
        if (n == "timemod" && rhs.kind == ExprValue::INT && ex.size() == 1 && ex[0].op.empty() && ex[0].value.kind == ExprValue::INT && rhs.i != 0) {
            out = ExprValue::Int(p.time % rhs.i == ex[0].value.i ? 1 : 0); return true;
        }
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
    if (n->type == ExprNode::CALL && lower(n->str) == "gethitvar" && n->kids.size() == 1 && n->kids[0]->type == ExprNode::CALL) {
        n->str = "gethitvar:" + lower(n->kids[0]->str); n->kids.clear();
        return;
    }
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

uint32_t hitflags_parse(const std::string& str) {
    uint32_t m = 2;
    for (size_t i = 0; i < str.size(); ++i) {
        switch (std::tolower((unsigned char)str[i])) {
            case '+': m |= 0x100; break;
            case '-': m &= ~2u; break;
            case 'h': m |= 4; break;
            case 'm': m |= 0xc; break;
            case 'l': m |= 8; break;
            case 'a': m |= 0x10; break;
            case 'f': m |= 0x20; break;
            case 'd': m |= 0x40; break;
            case 'p': m |= 0x80; break;
            default: break;
        }
    }
    return m;
}

void Engine::init_player(Player& p) const {
    float k;
    if (ch_.constant("data.life", k)) { p.life = (int)k; p.lifeMax = (int)k; }
    if (ch_.constant("data.defence", k) && k > 0) p.defenceMul = 100.0f / k;
}

const AirFrame* Engine::current_frame(const Player& p) const {
    std::map<int, AirAction>::const_iterator it = ch_.anims.find(p.anim);
    if (it == ch_.anims.end() || it->second.frames.empty()) return 0;
    const AirAction& a = it->second;
    int lt = a.looptime(), t = p.animElapsed;
    if (lt > 0 && t >= lt) t = lt - 1;               // no tick AnimTime = 0 o ultimo elemento ainda aparece
    int acc = 0;
    for (size_t i = 0; i < a.frames.size(); ++i) {
        if (a.frames[i].time < 0) return &a.frames[i];   // tempo infinito
        acc += a.frames[i].time;
        if (t < acc) return &a.frames[i];
    }
    return &a.frames[a.frames.size() - 1];
}

bool Engine::anim_elem_time(const Player& p, int n, int& out) const {
    std::map<int, AirAction>::const_iterator it = ch_.anims.find(p.anim);
    if (it == ch_.anims.end() || n < 1 || n > (int)it->second.frames.size()) return false;
    int start = 0;
    for (int i = 0; i < n - 1; ++i) { if (it->second.frames[i].time < 0) return false; start += it->second.frames[i].time; }
    out = p.animElapsed - start;
    return true;
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
    p.moveContact = p.moveHit = p.moveGuarded = false; p.hitdef.active = false;
    if (p.defaultGuardDist < 0) { float k; p.defaultGuardDist = ch_.constant("size.attack.dist", k) ? (int)k : 0; }   // vale ate trocar de estado
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

std::string Engine::raw_param(const CnsController& c, const char* key) const {
    for (size_t i = 0; i < c.params.size(); ++i) if (ini_iequals(c.params[i].key, key)) return c.params[i].value;
    return "";
}

static std::string first_token(const std::string& v) {
    size_t i = 0; while (i < v.size() && std::isspace((unsigned char)v[i])) ++i;
    size_t j = i; while (j < v.size() && v[j] != ',' && !std::isspace((unsigned char)v[j])) ++j;
    return v.substr(i, j - i);
}
static int hit_type(const std::string& v, int def) {
    std::string t = lower(first_token(v));
    if (t == "high") return 1; if (t == "low") return 2; if (t == "trip") return 3; if (t == "none") return 0;
    return def;
}

void Engine::exec_hitdef(Player& p, const CnsController& c) {
    HitDef h;
    std::string attr = raw_param(c, "attr");
    size_t comma = attr.find(',');
    std::string a1 = lower(first_token(attr));
    for (size_t i = 0; i < a1.size(); ++i) { if (a1[i] == 's') h.attrTypes |= 1; else if (a1[i] == 'c') h.attrTypes |= 2; else if (a1[i] == 'a') h.attrTypes |= 4; }
    if (comma != std::string::npos) h.attrKind = first_token(attr.substr(comma + 1));
    // hitflag omitido -> 0x3E; presente -> HitFlags_Parse; guardflag omitido -> 0 (confirmado em 0x44FE6F-0x44FEAA)
    std::string hf = raw_param(c, "hitflag"); if (!hf.empty()) h.hitflag = hitflags_parse(first_token(hf));
    std::vector<ExprValue> v; int iv; float fv;
    if (param_values(p, c, "damage", v)) { if (v.size() > 0) h.damage = to_i(v[0]); if (v.size() > 1) h.guardDamage = to_i(v[1]); }
    if (param_values(p, c, "pausetime", v)) { if (v.size() > 0) h.pause1 = to_i(v[0]); if (v.size() > 1) h.pause2 = to_i(v[1]); }
    if (param_int(p, c, "ground.hittime", iv)) h.groundHitTime = iv;
    if (param_int(p, c, "ground.slidetime", iv)) h.groundSlideTime = iv;
    if (param_int(p, c, "air.hittime", iv)) h.airHitTime = iv;
    if (param_values(p, c, "ground.velocity", v)) { if (v.size() > 0) h.groundVelX = to_f(v[0]); if (v.size() > 1) { h.groundVelY = to_f(v[1]); h.groundVelYSet = true; } }
    if (param_values(p, c, "air.velocity", v)) { if (v.size() > 0) h.airVelX = to_f(v[0]); if (v.size() > 1) { h.airVelY = to_f(v[1]); h.airVelYSet = true; } }
    h.groundType = hit_type(raw_param(c, "ground.type"), 1);
    h.airType = raw_param(c, "air.type").empty() ? h.groundType : hit_type(raw_param(c, "air.type"), h.groundType);
    std::string at = lower(first_token(raw_param(c, "animtype")));
    h.animType = at == "medium" ? 1 : at == "hard" ? 2 : at == "back" ? 3 : at == "up" ? 4 : at == "diagup" ? 5 : 0;
    if (param_float(p, c, "yaccel", fv)) h.yaccel = fv;
    if (param_int(p, c, "fall", iv)) { h.fall = iv; if (iv != 0) h.hitflag |= 1; }   // bit 0 de hitflag = "sempre cai" (consumo: 0x43EE01)
    if (param_values(p, c, "priority", v) && !v.empty()) h.priority = to_i(v[0]);
    if (param_int(p, c, "hitonce", iv)) h.hitOnce = iv != 0;
    { std::string gf = raw_param(c, "guardflag"); h.guardflag = gf.empty() ? 0 : hitflags_parse(first_token(gf)); }
    if (param_int(p, c, "kill", iv)) h.kill = iv != 0;
    if (param_int(p, c, "guard.kill", iv)) h.guardKill = iv != 0;
    if (param_int(p, c, "guard.dist", iv)) h.guardDist = iv; else h.guardDist = p.defaultGuardDist;
    if (param_values(p, c, "guard.pausetime", v)) { if (v.size() > 0) h.guardPause1 = to_i(v[0]); if (v.size() > 1) h.guardPause2 = to_i(v[1]); }
    if (param_float(p, c, "guard.velocity", fv)) h.guardVelX = fv;
    if (param_int(p, c, "guard.hittime", iv)) h.guardHitTime = iv;
    if (param_int(p, c, "guard.slidetime", iv)) h.guardSlideTime = iv;
    if (param_int(p, c, "guard.ctrltime", iv)) h.guardCtrlTime = iv;
    h.active = true;
    p.hitdef = h;
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
    if (t == "hitdef") { exec_hitdef(p, c); return false; }
    if (t == "hitvelset") {
        // x/y = 1: usa a velocidade do golpe recebido (GetHitVar xvel/yvel). xvel e relativo ao lado de quem apanha.
        if (param_int(p, c, "x", iv) && iv != 0) p.velX = p.gethit.xvel;   // absoluto (HitDef_Apply (0x0043ECE0): valor * -facing do atacante)
        if (param_int(p, c, "y", iv) && iv != 0) p.velY = p.gethit.yvel;
        return false;
    }
    if (t == "assertspecial") {
        // Parser em 0x4592A4 (cadeia de stricmp; ids conferidos: nostandguard 0, nocrouchguard 1, noairguard 2, nowalk 6) e executor em 0x44A0FB:
        // byte [jogador + 0x2AC + id] = 1. Le flag, flag2 e flag3. Os demais nomes ainda NAO estao mapeados.
        static const struct { const char* name; int id; } known[] = { {"nostandguard", 0}, {"nocrouchguard", 1}, {"noairguard", 2}, {"nowalk", 6} };
        for (size_t i = 0; i < c.params.size(); ++i) {
            const std::string& k = c.params[i].key;
            if (!(ini_iequals(k, "flag") || ini_iequals(k, "flag2") || ini_iequals(k, "flag3"))) continue;
            std::string v = lower(c.params[i].value);
            size_t b = v.find_first_not_of(" \t"), e = v.find_last_not_of(" \t");
            v = b == std::string::npos ? std::string() : v.substr(b, e - b + 1);
            bool found = false;
            for (size_t j = 0; j < sizeof known / sizeof known[0]; ++j)
                if (v == known[j].name) { p.assertFlags[known[j].id] = 1; found = true; }
            if (!found && !v.empty()) p.skipped["assertspecial:" + v]++;
        }
        return false;
    }
    if (t == "defencemulset" || t == "forcefeedback" || t == "null") return false;   // sem efeito no motor (por enquanto)
    p.skipped["controller:" + c.type]++;
    return false;
}

// Porte de Player_UpdateInput (0x00434DB0) (mugen.exe 1.1b1), parte que escolhe a "intencao" de movimento.
// Entradas lidas dos comandos padrao holdfwd/holdback/holdup/holddown (o exe le as teclas direto).
// Portado e conferido no assembly: guarda automatica (intencao 0x28 -> estado 120) com os flags nostandguard/nocrouchguard/noairguard
// (+0x2AC/+0x2AD/+0x2AE, 0x435241-0x43526C), pulo no ar (intencao 10 -> estado 45, 0x4351CA e 0x435370-0x4353C4), nowalk (+0x2B2, 0x435404)
// e a limpeza da velocidade X (+0x248, 0x43544D e 0x435548).
// NAO portado: a trava de rodada (global +0x12754 == 3 no pulo), as condicoes globais (+0x11F78 na guarda), guarda contra projeteis,
// a fila de teclas (inputEvent e preenchido pelo chamador) e o limpar de comandos por +0x310.
void Engine::default_movement(Player& p) {
    bool fwd  = p.commands.count("holdfwd")  != 0;
    bool back = p.commands.count("holdback") != 0;
    bool up   = p.commands.count("holdup")   != 0;
    bool down = p.commands.count("holddown") != 0;
    // 0x434DC9-0x434DF3: o exe tira UM valor da fila de teclas por tick (vazia = 0); so o valor 1 interessa ao pulo no ar
    int ev = p.inputEvent; p.inputEvent = 0;
    int intent = -1;
    bool keepCrouch = false;
    if (p.stateType == 'S') {
        if (fwd != back) intent = 1;     // so uma direcao horizontal: andar
        if (down) intent = 11;           // 0xb: agachar (sobrepoe andar)
        if (up)   intent = 7;            // pular (sobrepoe agachar)
    }
    if (p.stateType == 'C' && down) keepCrouch = true;
    if (p.stateType == 'A' && ev == 1) intent = 10;   // 0x4351CA: statetype A e evento de teclas == 1 -> intencao 10 (pulo no ar)
    if (fwd && back) intent = -1;        // as duas direcoes juntas cancelam qualquer intencao
    // Guarda automatica (Player_UpdateInput (0x00434DB0) + Player_InGuardDist (0x004347A0)): segurando "trás", sem "cima", com controle parado ou ja
    // guardando (151/153/155), e um oponente atacando dentro da guard.dist -> intencao 0x28 (sobrepoe as demais).
    // NAO portado: projeteis.
    bool guardIntent = false;
    p.guarding = false;                  // exe: *(player+0xEE8) = 0 antes do teste de guarda
    // 0x435241-0x43526C: o flag do statetype atual (nostandguard S, nocrouchguard C, noairguard A) pula a checagem de guarda inteira
    bool noGuardFlag = (p.stateType == 'S' && p.assertFlags[0]) || (p.stateType == 'C' && p.assertFlags[1]) || (p.stateType == 'A' && p.assertFlags[2]);
    if (back && !up && !noGuardFlag && p.enemy &&
        ((p.moveType == 'I' && (p.ctrl || p.stateNo == 52)) || p.stateNo == 151 || p.stateNo == 153 || p.stateNo == 155)) {
        const Player* o = p.enemy;
        if (o->moveType == 'A') {
            p.guarding = true;           // exe: +0xEE8 = 1 sempre que ha oponente atacando, ANTES de checar a distancia
            int gd = o->hitdef.guardDist >= 0 ? o->hitdef.guardDist : o->defaultGuardDist;
            float d = (p.posX - o->posX) * (float)o->facing;
            if (d >= 0.0f && d < (float)gd) guardIntent = true;
        }
    }
    if (guardIntent && !(fwd && back)) intent = 0x28;
    bool handled = false;
    if (intent == 0x28) {
        // exige movetype I e ctrl; nao reinicia a guarda se ja esta nos estados 120/130/131/132/140
        if (p.moveType == 'I' && p.ctrl) {
            p.guarding = true;
            int sn = p.stateNo;
            if (sn != 130 && sn != 131 && sn != 132 && sn != 120 && sn != 140) {
                if (p.stateType != 'A' && sn != 100) p.velX = 0;
                enter_state(p, 120);
            }
        }
        return;      // o exe pula a regra "estado 20 -> 0" e a de levantar
    }
    if (intent == 11) {
        if (p.ctrl) {
            if (p.stateNo != 100) p.velX = 0;                          // 0x43543A-0x43544D: fldz/fstp [+0x248] (vel.x = 0) salvo no estado 100
            if (p.stateNo != 10 && p.stateNo != 11) enter_state(p, 10);
        }
        handled = true;
    } else if (intent == 1) {
        if (p.ctrl && !p.assertFlags[6] && p.stateNo != 20) enter_state(p, 20);   // 0x435404: nowalk (+0x2B2) impede entrar no 20
        handled = true;
    } else if (intent == 7) {
        if (p.ctrl) enter_state(p, 40);
        handled = true;
    } else if (intent == 10) {
        // 0x435370-0x4353C4: exige ctrl e +0x278 > 0; compara airjump.height (int em +0x110) com -posY (double em +0x200):
        // entra no 45 so se height < -posY (fcompp + test ah,5). Depois +0x278--.
        if (p.ctrl && p.airJumpsLeft > 0) {
            float h = 0; ch_.constant("movement.airjump.height", h);
            if ((float)(int)h < -p.posY) { enter_state(p, 45); --p.airJumpsLeft; }
        }
        handled = true;
    }
    if (handled) return;
    // Sem intencao tratada: andando volta para o estado 0 (so troca os campos, sem validar o estado)
    if (p.stateNo == 20) { p.prevStateNo = 20; p.stateNo = 0; p.time = 0; p.persist.clear(); }
    // Agachado, com controle e sem segurar para baixo: levantar (estado 12)
    if (p.stateType == 'C' && p.ctrl && !keepCrouch) enter_state(p, 12);
}

// Porte da parte de fisica de Player_Update (0x0043C480) (mugen.exe 1.1b1), na ordem do exe:
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
    // Pausa de hit (hitpause / shake): o exe decrementa a pausa no lugar de mover (Player_Update (0x0043C480)).
    // Durante a pausa so rodam controllers com ignorehitpause (docs); Time e animacao ficam congelados.
    bool paused = p.pauseTime > 0;
    if (!paused) default_movement(p); else p.inputEvent = 0;
    // 0x41389E (FUN_00413170, logo depois do 434db0 de todos os jogadores e antes de Player_Update): zera os flags do AssertSpecial
    for (int i = 0; i < 8; ++i) p.assertFlags[i] = 0;
    // Dano pendente (Player_Update 0x43C7AB, antes do teste de pausa em 0x43C988): vida = vida - dano, entre 0 e lifeMax;
    // zera o pendente. Roda mesmo durante o hitpause. NAO portado: o desvio com o estado global 3 e a flag de depuracao.
    if (p.gethitDamage != 0) {
        int nl = p.life - p.gethitDamage;
        p.life = nl < 0 ? 0 : nl > p.lifeMax ? p.lifeMax : nl;
        p.gethitDamage = 0;
    }
    // Player_Update 0x43C96A: no tick 0 do estado 40 (pulo) o contador de pulos no ar recebe Const(movement.airjump.num) (+0x10c -> +0x278)
    if (p.stateNo == 40 && p.time == 0) {
        float n = 0;
        p.airJumpsLeft = ch_.constant("movement.airjump.num", n) ? (int)n : 0;
    }
    if (!paused) {
        physics(p);
        // Fim da guarda "implementado no motor" (comentario [State 140, 5] do common1.cns; codigo em Player_Update (0x0043C480),
        // 0x43CA12-0x43CA75): estado 140 com Time >= duracao da animacao -> 0 (em pe), 11 (agachado) ou 51 (no ar).
        if (p.stateNo == 140) {
            int lt = anim_looptime(p.anim);
            if (lt > 0 && p.time >= lt) enter_state(p, p.stateType == 'C' ? 11 : p.stateType == 'A' ? 51 : 0);
        }
    } else --p.pauseTime;
    // 1) estados especiais -3, -2, -1: uma passada; ChangeState aborta so o restante daquele estado
    for (int sn = -3; sn <= -1; ++sn) {
        const CnsStatedef* sd = ch_.state(sn);
        if (!sd) continue;
        for (size_t i = 0; i < sd->controllers.size(); ++i) {
            const CnsController& c = sd->controllers[i];
            if (paused && std::atoi(c.ignoreHitPause.c_str()) == 0) continue;
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
            if (paused && std::atoi(c.ignoreHitPause.c_str()) == 0) continue;
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
    if (!paused) {
        ++p.time;
        ++p.animElapsed;
        int lt = anim_looptime(p.anim);
        if (lt > 0 && p.animElapsed > lt) p.animElapsed -= lt;   // animacao em loop
        if (p.hitTime > 0) --p.hitTime;                          // tempo restante no estado de apanhar
    }
}
