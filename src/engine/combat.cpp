#include "engine/combat.h"
#include <algorithm>
#include <cctype>

WorldBox world_box(const Player& p, const AirBox& b) {
    WorldBox w;
    float fx1 = (float)b.x1 * (float)p.facing, fx2 = (float)b.x2 * (float)p.facing;
    w.x1 = p.posX + std::min(fx1, fx2);
    w.x2 = p.posX + std::max(fx1, fx2);
    w.y1 = p.posY + (float)std::min(b.y1, b.y2);
    w.y2 = p.posY + (float)std::max(b.y1, b.y2);
    return w;
}

bool boxes_overlap(const WorldBox& a, const WorldBox& b) {
    return a.x1 < b.x2 && b.x1 < a.x2 && a.y1 < b.y2 && b.y1 < a.y2;
}

static bool has_flag(const std::string& flags, char c) {
    for (size_t i = 0; i < flags.size(); ++i) if (std::tolower((unsigned char)flags[i]) == std::tolower((unsigned char)c)) return true;
    return false;
}

// O hitflag permite acertar um oponente neste tipo de estado? (M = H + L)
static bool hitflag_allows(const HitDef& h, const Player& def) {
    const std::string& f = h.hitflag;
    bool inGethit = def.moveType == 'H';
    if (has_flag(f, '+') && !inGethit) return false;
    if (has_flag(f, '-') && inGethit) return false;
    switch (def.stateType) {
        case 'S': case 'C': return has_flag(f, 'H') || has_flag(f, 'L') || has_flag(f, 'M');
        case 'A': return has_flag(f, 'A');
        case 'L': return has_flag(f, 'D');
        default:  return false;
    }
}

bool resolve_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk) {
    if (!atk.hitdef.active || atk.pauseTime > 0 || def.life <= 0) return false;
    const HitDef& h = atk.hitdef;
    if (!hitflag_allows(h, def)) return false;
    const AirFrame* fa = engAtk.current_frame(atk);
    const AirFrame* fd = engDef.current_frame(def);
    if (!fa || !fd) return false;
    bool hit = false;
    for (size_t i = 0; i < fa->clsn1.size() && !hit; ++i)
        for (size_t j = 0; j < fd->clsn2.size() && !hit; ++j)
            hit = boxes_overlap(world_box(atk, fa->clsn1[i]), world_box(def, fd->clsn2[j]));
    if (!hit) return false;

    // --- aplica o golpe (regras lidas em FUN_0043FD80 / FUN_0043ECE0 do exe) ---
    HitDef hd = h;
    atk.moveContact = atk.moveHit = true;
    if (hd.hitOnce) atk.hitdef.active = false;
    atk.pauseTime = hd.pause1;
    bool air = def.stateType == 'A';
    int gtype = air ? hd.airType : hd.groundType;

    // Atingido por tras (os dois olham para o mesmo lado): High <-> Low
    int groundType = hd.groundType, airType = hd.airType;
    if (def.facing == atk.facing) {
        if (groundType == 1) groundType = 2; else if (groundType == 2) groundType = 1;
        if (airType == 1) airType = 2; else if (airType == 2) airType = 1;
        gtype = air ? airType : groundType;
    }
    // type None: so pausa (o exe devolve antes de calcular dano e variaveis do golpe)
    if (gtype == 0) { def.pauseTime = hd.pause2; return true; }

    // dano: minimo 1 se o golpe tem dano > 0; a vida nunca fica negativa
    int dmg = hd.damage;
    if (hd.damage > 0 && dmg == 0) dmg = 1;
    def.life = std::max(0, def.life - dmg);

    GetHitVars& g = def.gethit;
    g.animType = hd.animType; g.groundType = groundType; g.airType = airType;
    g.slideTime = hd.groundSlideTime; g.ctrlTime = hd.groundSlideTime;
    g.hitTime = air ? hd.airHitTime : hd.groundHitTime;
    g.fall = hd.fall; g.damage = dmg; g.yaccel = hd.yaccel;
    // x: valor * (-facing do atacante); y: se omitido no HitDef, mantem a velocidade y atual do oponente
    g.xvel = (air ? hd.airVelX : hd.groundVelX) * (float)(-atk.facing);
    bool ySet = air ? hd.airVelYSet : hd.groundVelYSet;
    g.yvel = ySet ? (air ? hd.airVelY : hd.groundVelY) : def.velY;

    // estado de apanhar: S -> 5000, C -> 5010, A -> 5020; Trip -> 5070
    int state = def.stateType == 'C' ? 5010 : air ? 5020 : 5000;
    if (gtype == 3) state = 5070;
    def.ctrl = false;
    if (!engDef.enter_state(def, state)) return true;      // estado ausente: o dano ja foi aplicado
    def.pauseTime = hd.pause2;                              // shake
    def.hitTime = g.hitTime;
    return true;
}

void resolve_hits(Player& a, Engine& ea, Player& b, Engine& eb) {
    // Simplificacao: os dois golpes sao calculados contra o estado do inicio da rodada (sem prioridade).
    Player a0 = a, b0 = b;
    bool ab = resolve_hit(a0, b, eb, ea);
    bool ba = resolve_hit(b0, a, ea, eb);
    if (ab) { a.moveContact = a0.moveContact; a.moveHit = a0.moveHit; a.hitdef = a0.hitdef; a.pauseTime = a0.pauseTime; }
    if (ba) { b.moveContact = b0.moveContact; b.moveHit = b0.moveHit; b.hitdef = b0.hitdef; b.pauseTime = b0.pauseTime; }
}
