
#include "engine/combat.h"
#include <algorithm>
#include <cmath>

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

bool apply_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk);
static int state_code(char t) { return t == 'S' ? 1 : t == 'C' ? 2 : t == 'A' ? 3 : t == 'L' ? 4 : 0; }

// 0x00441200 (assembly confirmado):
//  - em estado L (4) exige o bit 0x40 (D);
//  - alvo em movetype H (2) exige o bit 2; se ele NAO esta guardando, pula o teste do bit 0x100 ('+');
//  - o bit 0x100 ('+') recusa o golpe quando o alvo nao esta em H (ou esta em H guardando);
//  - alvo caindo (gethit fall != 0) exige o bit 0x20 (F);
//  - por fim: S exige 4 (H), C exige 8 (L), A exige 0x10 (A), L exige 0x40 (D).
bool hitflag_matches(const Player& p, uint32_t flags) {
    int st = state_code(p.stateType);
    if (st == 4 && (flags & 0x40) == 0) return false;
    bool checkPlus = true;
    if (p.moveType == 'H') {
        if ((flags & 2) == 0) return false;
        if (!p.guarding) checkPlus = false;
    }
    if (checkPlus && (flags & 0x100) != 0) return false;
    if (p.gethit.fall != 0 && (flags & 0x20) == 0) return false;
    switch (st) {
        case 1: return (flags & 4) != 0;
        case 2: return (flags & 8) != 0;
        case 3: return (flags & 0x10) != 0;
        case 4: return (flags & 0x40) != 0;
        default: return false;
    }
}

// 0x0043F930 (assembly confirmado): d = trunc(defence * dano + 0.5) + pendente; se dano >= 1 e d == 0 -> 1;
// com kill == 0, se (vida - pendente - d) < 1 e o jogador esta vivo (+0xF00), d = (vida - pendente) - 1.
// Retorna d - pendente.
int hit_damage(const Player& p, int damage, int kill) {
    int pending = p.gethitDamage;
    int d = (int)(p.defenceMul * (float)damage + 0.5f) + pending;
    if (damage >= 1 && d == 0) d = 1;
    if (kill == 0) {
        int left = p.life - pending;
        if (left - d < 1 && p.alive) d = left - 1;
    }
    return d - pending;
}

namespace {
const float kFallMinYVel = -0.001f;       // constante em 0x004FDA00 (float32)

void swap_high_low(int& t) { if (t == 1) t = 2; else if (t == 2) t = 1; }

// HitDef_Apply (0x0043ECE0), ate a decisao "tem estado de apanhar": copia os dados do golpe para o alvo.
// Retorna true se o alvo ganhou um estado de apanhar (e preenche 'state'); false se o exe devolveria 0 (tipo None).
bool hit_apply_prepare(Player& tgt, const Player& att, const HitDef& hd, int& state) {
    GetHitVars& g = tgt.gethit;
    g.xvel = 0.0f; g.yvel = 0.0f;                         // 0x43EDCF: +0x1130 e +0x1134 zerados
    g.groundType = hd.groundType;                         // 0x43EDE9: +0x1100
    g.airType = hd.airType;                               // 0x43EDF5: +0x10FC
    if (hd.hitflag & 1) g.fall = 1;                       // 0x43EE01: test byte [hd+0x78], 1  (ko e forcenofall: nao portados)
    g.animType = hd.animType;                             // fall == 0 ? animtype : fall.animtype (fall.animtype nao lido: igual)
    bool noState = true;
    int st = state_code(tgt.stateType);
    float ndir = (float)(-att.facing);
    if (st == 1 || st == 2) {                             // 0x43F195: em pe / agachado
        if (hd.groundType != 0) {
            noState = false;
            state = tgt.stateType != 'S' ? 5010 : 5000;   // 0x43F1DD-0x43F1F1
            if (hd.groundType == 3) state = 5070;         // 0x43F1F7
            g.xvel = hd.groundVelX * ndir;                // valor * (-facing do atacante)
            g.yvel = hd.groundVelYSet ? hd.groundVelY : tgt.velY;   // omitido: mantem a velocidade y atual
            g.yaccel = hd.yaccel;
            if (g.fall != 0 && g.yvel == 0.0f) g.yvel = kFallMinYVel;   // 0x43F380-0x43F399
            if (g.yvel != 0.0f) {
                g.hitTime = hd.airHitTime;                // 0x43EF97: air.hittime; slidetime NAO e definido
            } else {
                g.hitTime = hd.groundHitTime;             // 0x43F3B9
                g.slideTime = hd.groundSlideTime;         // 0x43F3C5
                if (g.animType == 3) g.animType = 2;      // 0x43F3B2-0x43F3D3: Back vira Hard
            }
        }
    } else if (st == 3) {                                 // 0x43EFA8: no ar
        if (hd.airType != 0) {
            noState = false;
            state = hd.airType == 3 ? 5070 : 5020;        // 0x43EFCE / 0x43EFE9
            g.xvel = hd.airVelX * ndir;
            g.yvel = hd.airVelYSet ? hd.airVelY : tgt.velY;
            g.yaccel = hd.yaccel;
            g.hitTime = hd.airHitTime;
        }
    } else {
        tgt.skipped["hit-alvo-deitado(L)-nao-portado"]++; // estado 5080: ainda nao portado
    }
    // 0x43F3E1 ("common"): hit por tras troca High <-> Low, mas so para animtype Light(0), Medium(1) ou Hard(2) (0x43F40A-0x43F41B)
    int expected = -att.facing;                           // (p2facing != -1 ? 1 : -1) * ndir, com p2facing padrao
    if (tgt.facing != expected && g.animType >= 0 && g.animType <= 2) {
        swap_high_low(g.groundType);
        swap_high_low(g.airType);
    }
    return !noState;
}
} // namespace

bool can_hit(const Player& atk, const Player& def, const Engine& engDef, const Engine& engAtk) {
    // --- Player_CanHit (0x0043E280): HitDef ativo, alvo vivo, caixas se sobrepoem, juggle e o hitflag combina ---
    if (!atk.hitdef.active || atk.pauseTime > 0 || def.life <= 0) return false;
    const HitDef& h = atk.hitdef;
    const AirFrame* fa = engAtk.current_frame(atk);
    const AirFrame* fd = engDef.current_frame(def);
    if (!fa || !fd) return false;
    bool overlap = false;
    for (size_t i = 0; i < fa->clsn1.size() && !overlap; ++i)
        for (size_t j = 0; j < fd->clsn2.size() && !overlap; ++j)
            overlap = boxes_overlap(world_box(atk, fa->clsn1[i]), world_box(def, fd->clsn2[j]));
    if (!overlap) return false;
    // 0x43E415-0x43E43A: sem nojugglecheck (+0x2B1), se o atacante ja tem registro do alvo e o custo (+0x1264) nao e -1,
    // recusa quando pontos - custo < 0 (js em 0x43E432)
    if (!atk.assertFlags[5] && atk.juggleRec && atk.juggleCost != -1 && atk.juggleLeft - atk.juggleCost < 0) return false;
    if (!hitflag_matches(def, h.hitflag)) return false;   // 0x43E448: call 0x441200 com [att+0x12A8]
    return true;
}

bool resolve_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk) {
    if (!can_hit(atk, def, engDef, engAtk)) return false;
    return apply_hit(atk, def, engDef, engAtk);
}

bool apply_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk) {
    // --- Player_Hit (0x0043E480) ---
    HitDef hd = atk.hitdef;
    bool guarded = false;
    // guardado se: flag "guardando" (+0xEE8) E guardflag combina (0x441200) E o dano de guarda nao mata
    if (def.guarding && !atk.assertFlags[7] && hitflag_matches(def, hd.guardflag)) {   // 0x43E50D: byte +0x2B3 do atacante = unguardable
        int gd = hit_damage(def, hd.guardDamage, hd.guardKill);
        if (def.life - def.gethitDamage - gd > 0) guarded = true;   // sem chip-kill (flag global nao portada)
    }
    bool hitOnceOff = hd.hitOnce;

    // Juggle (Player_Hit): 0x43E619-0x43E6A2 cria o registro do alvo no atacante no primeiro golpe (guardado ou nao); 0x43E931-0x43E95D
    // desconta o custo (+0x1424) so quando o golpe NAO foi guardado e o alvo cai (hitdef+0x78 bit 0) ou ja caia (+0x1148).
    // Nao portado: o termo [hitdef+0x148] != 0 com o alvo no ar (significado nao lido).
    atk.juggleNow = atk.assertFlags[5] ? 0 : atk.juggleCost;              // 0x43E43A / 0x43DA27
    if (!atk.juggleRec) {
        atk.juggleRec = true;
        float k; atk.juggleLeft = engAtk.character().constant("data.airjuggle", k) ? (int)k : 0;   // CONFERIDO (#3): [atacante+0x64] = jogador+0x50 + consts.airjuggle (+0x14); gravado em CharData+0x2D0 (0x437620), copiado em 0x43A4F9
    }
    if (!guarded && (def.gethit.fall != 0 || (hd.hitflag & 1) != 0)) atk.juggleLeft -= atk.juggleNow;

    if (guarded) {
        // Player_Guard (0x0043F990): estados 150 / 152 / 154
        atk.moveContact = true; atk.moveGuarded = true;
        atk.pauseTime = hd.guardPause1 != -9999 ? hd.guardPause1 : hd.pause1;
        int shake     = hd.guardPause2 != -9999 ? hd.guardPause2 : hd.pause2;
        bool air = def.stateType == 'A';
        def.gethitDamage += hit_damage(def, hd.guardDamage, hd.guardKill);
        GetHitVars& g = def.gethit;
        g.animType = hd.animType; g.groundType = hd.groundType; g.airType = hd.airType;
        int hit   = hd.guardHitTime   != -9999 ? hd.guardHitTime   : hd.groundHitTime;
        int slide = hd.guardSlideTime != -9999 ? hd.guardSlideTime : hit;
        int ctrl  = hd.guardCtrlTime  != -9999 ? hd.guardCtrlTime  : slide;
        g.hitTime = hit; g.slideTime = slide; g.ctrlTime = ctrl;
        g.damage = hd.guardDamage; g.yaccel = hd.yaccel; g.yvel = 0;
        float gx = hd.guardVelX != -9999.0f ? hd.guardVelX : hd.groundVelX;
        if (air) { gx = hd.airVelX * 1.5f; g.yvel = hd.airVelY / 2.0f; }   // padrao do airguard (docs; a confirmar no exe)
        g.xvel = gx * (float)(-atk.facing);
        int gstate = def.stateType == 'C' ? 152 : air ? 154 : 150;
        def.ctrl = false;
        def.airJumpsLeft = 0;                                  // Player_Guard 0x43FAE3: +0x278 = 0
        def.guarding = true;                                   // Player_Guard grava +0xEE8 = 1
        if (engDef.enter_state(def, gstate)) { def.pauseTime = shake; def.hitTime = hit; }
        if (hitOnceOff) atk.hitdef.active = false;
        return true;
    }

    int state = 0;
    bool hit = hit_apply_prepare(def, atk, hd, state);
    if (hit) {
        // fim de HitDef_Apply (0x43F657...): dano acumulado, shake, movetype H, Time 0, ctrl 0, guarding 0
        def.gethitDamage += hit_damage(def, hd.damage, hd.kill);   // 0x43F876-0x43F883: add [tgt+0x1104], dano (aplicado no tick)
        def.guarding = false;                                      // 0x43F6C4: +0xEE8 = 0
        def.ctrl = false;                                          // 0x43F6E0: +0xEE4 = 0
        def.airJumpsLeft = 0;                                      // 0x43F6D4: +0x278 = 0
        GetHitVars& g = def.gethit; g.damage = hd.damage;
        if (engDef.enter_state(def, state)) {                      // o exe so troca o numero; aqui tambem aplicamos o Statedef
            def.pauseTime = hd.pause2;                             // gethit_shaketime = p2 pausetime (0x43F6E6-0x43F6E9)
            def.hitTime = g.hitTime;
        }
        // Player_Hit 0x43EA08: so com HitDef_Apply != 0 o atacante ganha pausa, MoveHit/MoveContact
        atk.moveContact = true; atk.moveHit = true;
        atk.pauseTime = hd.pause1;
    }
    // type None: HitDef_Apply devolve 0 (0x43F46A) -> sem dano, sem estado, sem shake do alvo e sem pausa do atacante.
    if (hitOnceOff) atk.hitdef.active = false;                     // 0x43E... fim de Player_Hit
    return true;
}

// 0x43DB56-0x43DB77 (e o espelho 0x43DBA6-0x43DBC7): A so acerta B se tem prioridade maior, ou se iguais e (tipoA == Hit e tipoB != Dodge).
static bool priority_wins(const HitDef& a, const HitDef& b) {
    if (a.priority > b.priority) return true;
    if (a.priority != b.priority) return false;
    return a.priorityType == 0 && b.priorityType != 2;
}

void resolve_hits(Player& a, Engine& ea, Player& b, Engine& eb) {
    // Os dois sentidos sao testados contra o estado do inicio da rodada (como a tabela montada em 0x43DB34 antes de aplicar os golpes).
    // Prioridade: se so um pode acertar o outro, acerta; se os dois podem, vale a regra acima (0x43DB56-0x43DBDE).
    bool ab = can_hit(a, b, eb, ea);
    bool ba = can_hit(b, a, ea, eb);
    bool abOk = ab && (!ba || priority_wins(a.hitdef, b.hitdef));
    bool baOk = ba && (!ab || priority_wins(b.hitdef, a.hitdef));
    // Simplificacao: os dois golpes sao calculados contra o estado do inicio da rodada.
    Player a0 = a, b0 = b;
    bool abDone = abOk && apply_hit(a0, b, eb, ea);
    bool baDone = baOk && apply_hit(b0, a, ea, eb);
    if (abDone) { a.moveContact = a0.moveContact; a.moveHit = a0.moveHit; a.moveGuarded = a0.moveGuarded; a.hitdef = a0.hitdef; a.pauseTime = a0.pauseTime; a.juggleNow = a0.juggleNow; a.juggleRec = a0.juggleRec; a.juggleLeft = a0.juggleLeft; }
    if (baDone) { b.moveContact = b0.moveContact; b.moveHit = b0.moveHit; b.moveGuarded = b0.moveGuarded; b.hitdef = b0.hitdef; b.pauseTime = b0.pauseTime; b.juggleNow = b0.juggleNow; b.juggleRec = b0.juggleRec; b.juggleLeft = b0.juggleLeft; }
    // FUN_00417400 (chamada em 0x413E15, depois do pipeline de hits): remove o registro quando o alvo sai do hitstun
    end_frame_juggle(a, b);
    end_frame_juggle(b, a);
}

// 0x417430-0x41749D (por alvo): alvo em movetype H mantem o registro, salvo o estado 5150 (0x141E) com Time > 0; sem nenhum alvo em H o
// registro e removido. Nao portado: a condicao do hitdef ativo com +0x12EC != 0 (0x417450-0x417460), significado nao lido.
void end_frame_juggle(Player& atk, const Player& def) {
    if (!atk.juggleRec) return;
    if (def.moveType == 'H' && (def.stateNo != 5150 || def.time == 0)) return;
    atk.juggleRec = false;
}
