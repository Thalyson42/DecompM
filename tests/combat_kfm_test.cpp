// Teste de combate com o KFM real (dois KFM). Uso: combat_kfm_test <pasta-do-mugen>
// Baseado na documentacao; os valores NAO foram comparados com o mugen.exe.
#include "engine/world.h"
#include <cstdio>
#include <string>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf("FALHOU: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Duel {
    Engine e1, e2; Player p1, p2;
    Duel(const Character& ch, float dist) : e1(ch), e2(ch) {
        e1.init_player(p1); e2.init_player(p2);
        e1.enter_state(p1, 0); e2.enter_state(p2, 0);
        p1.ctrl = p2.ctrl = true; p1.facing = 1; p2.facing = -1; p1.posX = 0; p2.posX = dist;
        p1.enemy = &p2; p2.enemy = &p1;
    }
    void step() { e1.tick(p1); e2.tick(p2); resolve_hits(p1, e1, p2, e2); }
};

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: combat_kfm_test <pasta-do-mugen>"); return 2; }
    std::string root = argv[1]; if (root.back() != '/') root += '/';
    Character ch; std::string err;
    if (!ch.load(root + "chars/kfm/kfm.def", root + "data/", &err)) { std::printf("erro: %s\n", err.c_str()); return 2; }

    // A) soco fraco em pe (estado 200) a 50 px: acerta
    {
        Duel d(ch, 50);
        d.p1.commands.insert("x");
        int hitTick = -1;
        for (int t = 0; t < 12; ++t) { d.step(); d.p1.commands.clear(); if (hitTick < 0 && d.p2.stateNo == 5000) { hitTick = t; break; } }
        std::printf("A) hit no tick %d, vida p2=%d (pendente %d), p2 estado=%d\n", hitTick, d.p2.life, d.p2.gethitDamage, d.p2.stateNo);
        CHECK(hitTick == 3, "o hit deveria ocorrer no tick 3 (AnimElem = 3), ocorreu em %d", hitTick);
        CHECK(d.p2.life == 1000 && d.p2.gethitDamage == 23, "o dano fica pendente no tick do hit (vida=%d, pendente=%d)", d.p2.life, d.p2.gethitDamage);
        d.step();
        CHECK(d.p2.life == 1000 - 23 && d.p2.gethitDamage == 0, "no tick seguinte a vida cai 23 (vida=%d)", d.p2.life);
        CHECK(d.p1.moveHit && d.p1.moveContact, "p1 deveria ter MoveHit e MoveContact");
        CHECK(d.p2.moveType == 'H', "p2 deveria estar em movetype H, esta em %c", d.p2.moveType);
    }
    // B) mesmo golpe mas a 100 px: errou
    {
        Duel d(ch, 100);
        d.p1.commands.insert("x");
        for (int t = 0; t < 14; ++t) { d.step(); d.p1.commands.clear(); }
        CHECK(d.p2.life == 1000, "a 100 px o golpe nao deveria acertar, vida de p2 = %d", d.p2.life);
        CHECK(d.p2.stateNo == 0, "p2 deveria continuar no estado 0, esta em %d", d.p2.stateNo);
        CHECK(!d.p1.moveContact, "p1 nao deveria ter MoveContact");
    }
    // C) pausa e sequencia do defensor: 5000 (shake) -> 5001 (recuo) -> 0, empurrado para tras; um unico hit
    {
        Duel d(ch, 50);
        d.p1.commands.insert("x");
        int tick = 0, shakeEnd = -1, back = -1, to5001 = -1;
        for (; tick < 60; ++tick) {
            d.step(); d.p1.commands.clear();
            if (to5001 < 0 && d.p2.stateNo == 5001) to5001 = tick;
            if (to5001 >= 0 && d.p2.stateNo == 0) { back = tick; break; }
        }
        std::printf("C) 5001 no tick %d, de volta ao 0 no tick %d, p2.posX=%.1f, vida=%d\n", to5001, back, (double)d.p2.posX, d.p2.life);
        CHECK(to5001 > 0, "p2 deveria passar pelo estado 5001");
        CHECK(back > 0, "p2 deveria voltar ao estado 0");
        CHECK(d.p2.posX > 50.0f, "p2 deveria ser empurrado para tras (posX > 50), posX = %f", (double)d.p2.posX);
        CHECK(d.p2.life == 1000 - 23, "so um hit deveria ter acertado (hitonce), vida = %d", d.p2.life);
        (void)shakeEnd;
    }
    // D) pausa do atacante: durante o hitpause o Time do soco nao avanca
    {
        Duel d(ch, 50);
        d.p1.commands.insert("x");
        int t0 = -1;
        for (int t = 0; t < 6; ++t) { d.step(); d.p1.commands.clear(); if (d.p1.moveHit && t0 < 0) { t0 = d.p1.time; } }
        int a = d.p1.time; d.step(); d.step();
        CHECK(d.p1.pauseTime >= 0, "pauseTime nunca negativo");
        std::printf("D) p1.time=%d depois de 2 ticks em pausa=%d (pauseTime=%d)\n", a, d.p1.time, d.p1.pauseTime);
        CHECK(d.p1.time == a, "o Time de p1 deveria ficar congelado durante o hitpause");
    }
    // E) atingido por tras (os dois olham para a direita): High vira Low; o empurrao continua afastando de p1
    {
        Duel d(ch, 50);
        d.p2.facing = 1;
        d.p1.commands.insert("x");
        for (int t = 0; t < 4; ++t) { d.step(); d.p1.commands.clear(); }
        CHECK(d.p2.stateNo == 5000, "hit por tras tambem leva ao 5000, esta em %d", d.p2.stateNo);
        CHECK(d.p2.gethit.groundType == 2, "por tras, High deveria virar Low (2), veio %d", d.p2.gethit.groundType);
        for (int t = 0; t < 30; ++t) { d.step(); }
        CHECK(d.p2.posX > 50.0f, "o empurrao deve afastar p2 de p1 mesmo de costas, posX = %f", (double)d.p2.posX);
    }
    // F) ground.type Trip (3) leva ao estado 5070; None (0) so pausa, sem dano e sem estado de apanhar
    {
        Duel d(ch, 50);
        d.p1.commands.insert("x");
        for (int t = 0; t < 3; ++t) { d.step(); d.p1.commands.clear(); }
        d.e1.tick(d.p1); d.e2.tick(d.p2);                   // tick 3: o HitDef fica ativo
        d.p1.hitdef.groundType = 3;
        resolve_hits(d.p1, d.e1, d.p2, d.e2);
        CHECK(d.p2.stateNo == 5070, "Trip deveria levar ao estado 5070, esta em %d", d.p2.stateNo);
        Duel n(ch, 50);
        n.p1.commands.insert("x");
        for (int t = 0; t < 3; ++t) { n.step(); n.p1.commands.clear(); }
        n.e1.tick(n.p1); n.e2.tick(n.p2);
        n.p1.hitdef.groundType = 0;
        resolve_hits(n.p1, n.e1, n.p2, n.e2);
        CHECK(n.p2.stateNo == 0 && n.p2.life == 1000, "None: sem estado e sem dano (estado=%d vida=%d)", n.p2.stateNo, n.p2.life);
        CHECK(n.p2.pauseTime == 0 && n.p1.pauseTime == 0, "None nao pausa ninguem (p1=%d p2=%d)", n.p1.pauseTime, n.p2.pauseTime);
        CHECK(!n.p1.moveHit && !n.p1.moveContact, "None nao gera MoveHit nem MoveContact");
        CHECK(n.p2.gethitDamage == 0, "None nao gera dano pendente");
    }
    // G) guarda: p2 segura "tras" quando p1 ataca dentro da guard.dist -> estado 120 -> guarda o soco (150), sem dano
    {
        Duel d(ch, 50);
        d.p2.commands.insert("holdback");
        d.p1.commands.insert("x");
        int guardedAt = -1, firstGuardState = -1;
        bool flagsOk = false, hitFlag = false;
        for (int t = 0; t < 40; ++t) {
            d.step(); d.p1.commands.clear();
            if (firstGuardState < 0 && d.p2.stateNo >= 120 && d.p2.stateNo <= 132) firstGuardState = t;
            if (guardedAt < 0 && d.p1.moveGuarded) { guardedAt = t; flagsOk = d.p1.moveContact; hitFlag = d.p1.moveHit; }
        }
        std::printf("G) p2 entrou na guarda no tick %d, soco guardado no tick %d, vida p2=%d, estado=%d\n", firstGuardState, guardedAt, d.p2.life, d.p2.stateNo);
        CHECK(firstGuardState == 0, "p2 deveria entrar no estado 120 ja no tick 0 (oponente atacando dentro da guard.dist), foi no %d", firstGuardState);
        CHECK(guardedAt == 3, "o soco deveria ser guardado no tick 3, foi no %d", guardedAt);
        CHECK(d.p2.life == 1000, "guardado: o KFM nao tem guard damage, vida = %d", d.p2.life);
        CHECK(!hitFlag, "guardado nao e MoveHit");
        CHECK(flagsOk, "p1 deveria ter MoveContact junto com MoveGuarded");
    }
    // G2) flags nostandguard/nocrouchguard/noairguard (+0x2AC/+0x2AD/+0x2AE, FUN_00434db0 0x435241-0x43526C): com o flag do statetype
    //     atual o exe pula a checagem de guarda inteira (nao entra no 120 e o flag de guarda fica 0)
    {
        Duel d(ch, 50);
        d.p2.commands.insert("holdback");
        d.p1.commands.insert("x");
        bool entered120 = false;
        for (int t = 0; t < 4; ++t) {
            d.p2.assertFlags[0] = 1;          // nostandguard, marcado a cada tick (como faria o AssertSpecial)
            d.step(); d.p1.commands.clear();
            if (d.p2.stateNo >= 120 && d.p2.stateNo <= 132) entered120 = true;
        }
        CHECK(!entered120 && !d.p2.guarding, "com nostandguard p2 nao entra na guarda (estado=%d guarding=%d)", d.p2.stateNo, (int)d.p2.guarding);
        // o flag de OUTRO statetype nao impede: noairguard em pe continua guardando
        Duel e(ch, 50);
        e.p2.commands.insert("holdback");
        e.p1.commands.insert("x");
        bool ok120 = false;
        for (int t = 0; t < 3; ++t) {
            e.p2.assertFlags[2] = 1;
            e.step(); e.p1.commands.clear();
            if (e.p2.stateNo >= 120 && e.p2.stateNo <= 132) ok120 = true;
        }
        CHECK(ok120, "noairguard nao afeta a guarda em pe (estado=%d)", e.p2.stateNo);
    }
    // G3) golpe recebido ou guardado zera o contador de pulos no ar (+0x278; HitDef_Apply 0x43F6D4 e Player_Guard 0x43FAE3)
    {
        Duel d(ch, 50);
        d.p2.airJumpsLeft = 1;
        d.p1.commands.insert("x");
        for (int t = 0; t < 5; ++t) { d.step(); d.p1.commands.clear(); }
        CHECK(d.p2.stateNo >= 5000 && d.p2.airJumpsLeft == 0, "apanhar zera os pulos no ar (estado=%d, contador=%d)", d.p2.stateNo, d.p2.airJumpsLeft);
        Duel g(ch, 50);
        g.p2.airJumpsLeft = 1;
        g.p2.commands.insert("holdback"); g.p1.commands.insert("x");
        for (int t = 0; t < 6; ++t) { g.step(); g.p1.commands.clear(); }
        CHECK(g.p1.moveGuarded && g.p2.airJumpsLeft == 0, "guardar zera os pulos no ar (guardado=%d, contador=%d)", (int)g.p1.moveGuarded, g.p2.airJumpsLeft);
    }
    // H) sem segurar "tras" o mesmo soco acerta; e a 200 px (fora da guard.dist de 160) nao entra em guarda
    {
        Duel d(ch, 50);
        d.p1.commands.insert("x");
        for (int t = 0; t < 5; ++t) { d.step(); d.p1.commands.clear(); }      // hit no tick 3, vida cai no tick seguinte
        CHECK(d.p2.life == 1000 - 23 && !d.p1.moveGuarded, "sem holdback o soco acerta (vida=%d)", d.p2.life);
        Duel f(ch, 200);
        f.p2.commands.insert("holdback");
        f.p1.commands.insert("x");
        for (int t = 0; t < 6; ++t) { f.step(); f.p1.commands.clear(); }
        CHECK(f.p2.stateNo < 120 || f.p2.stateNo > 155, "a 200 px (fora da guard.dist) p2 nao deveria guardar, esta em %d", f.p2.stateNo);
        CHECK(f.p2.stateNo == 20, "segurando so \"tras\" fora da distancia p2 anda (estado 20), esta em %d", f.p2.stateNo);
    }
    // I) apos o recuo da guarda p2 recupera o controle e volta a um estado livre
    {
        Duel d(ch, 50);
        d.p2.commands.insert("holdback");
        d.p1.commands.insert("x");
        for (int t = 0; t < 80; ++t) { d.step(); d.p1.commands.clear(); if (t == 20) d.p2.commands.clear(); }
        std::printf("I) depois de 80 ticks: p2 estado=%d ctrl=%d posX=%.1f\n", d.p2.stateNo, (int)d.p2.ctrl, (double)d.p2.posX);
        CHECK(d.p2.posX > 50.0f, "a guarda deveria empurrar p2 para tras (posX = %f)", (double)d.p2.posX);
        CHECK(d.p2.stateNo == 0 && d.p2.ctrl, "(150 -> 151 -> 130 -> 140 -> 0) p2 deveria voltar ao estado 0 com controle (estado=%d ctrl=%d)", d.p2.stateNo, (int)d.p2.ctrl);
    }
    // J) guarda que mataria (vida - dano de guarda < 1) vira um hit normal (Player_Hit 0x43E480, sem chip-kill)
    {
        Duel d(ch, 50);
        d.p2.commands.insert("holdback");
        d.p1.commands.insert("x");
        for (int t = 0; t < 3; ++t) { d.step(); d.p1.commands.clear(); }
        d.e1.tick(d.p1); d.e2.tick(d.p2);          // tick 3: o HitDef esta ativo e p2 esta guardando
        CHECK(d.p2.guarding, "p2 deveria estar com o flag de guarda");
        d.p1.hitdef.guardDamage = 30; d.p2.life = 20;
        resolve_hits(d.p1, d.e1, d.p2, d.e2);
        CHECK(d.p2.stateNo == 5000 && d.p1.moveHit && !d.p1.moveGuarded, "guarda letal deveria virar hit normal (estado=%d)", d.p2.stateNo);
        d.e2.tick(d.p2);
        CHECK(d.p2.life == 0, "vida deveria chegar a 0 no tick seguinte, vida = %d", d.p2.life);
    }
    // K) hit que chega de longe (alem da guard.dist) mas com o defensor ja segurando "tras" e oponente atacando: o flag
    //    de guarda vale mesmo sem entrar no estado 120 (exe marca +0xEE8 antes de checar a distancia)
    {
        Duel d(ch, 50);
        d.p2.commands.insert("holdback");
        d.p1.commands.insert("x");
        d.step(); d.p1.commands.clear();
        d.p2.stateNo = 20;                          // forca p2 a nao estar em estado de guarda
        d.p2.moveType = 'I';
        d.e2.tick(d.p2);
        CHECK(d.p2.guarding, "o flag de guarda vale com oponente atacando, mesmo fora do estado 120");
    }
    // L) empurrao: corpos sobrepostos sao afastados pela soma das larguras (16 + 16 = 32), meio a meio
    {
        Duel d(ch, 20);
        World w;
        step_frame(d.p1, d.e1, d.p2, d.e2, w);
        float dist = d.p2.posX - d.p1.posX;
        std::printf("L) distancia depois do empurrao: %.1f (p1.x=%.1f p2.x=%.1f)\n", (double)dist, (double)d.p1.posX, (double)d.p2.posX);
        CHECK(dist > 31.9f && dist < 32.1f, "a distancia deveria ficar 32 (16+16), ficou %f", (double)dist);
        CHECK(d.p1.posX < -5.9f && d.p1.posX > -6.1f && d.p2.posX > 25.9f && d.p2.posX < 26.1f, "cada um recua 6 px (p1=%f p2=%f)", (double)d.p1.posX, (double)d.p2.posX);
        // sem sobreposicao: nada muda
        Duel e(ch, 40);
        step_frame(e.p1, e.e1, e.p2, e.e2, w);
        CHECK(e.p1.posX == 0.0f && e.p2.posX == 40.0f, "a 40 px nao ha empurrao (p1=%f p2=%f)", (double)e.p1.posX, (double)e.p2.posX);
    }
    // M) mesmo x (distancia < 10): recentra em +-5 e depois empurra pela soma das larguras menos a distancia original
    {
        Duel d(ch, 0);
        World w;
        step_frame(d.p1, d.e1, d.p2, d.e2, w);
        std::printf("M) mesmo x: p1.x=%.1f p2.x=%.1f\n", (double)d.p1.posX, (double)d.p2.posX);
        CHECK(d.p1.posX < d.p2.posX, "p1 (olhando para a direita) deveria ficar a esquerda");
        CHECK(d.p1.posX < -20.9f && d.p1.posX > -21.1f && d.p2.posX > 20.9f && d.p2.posX < 21.1f, "no mesmo x: -5-16 e +5+16 (p1=%f p2=%f)", (double)d.p1.posX, (double)d.p2.posX);
    }
    // N) pular por cima nao empurra; limites do cenario seguram o jogador
    {
        Duel d(ch, 20);
        d.p1.posY = -100.0f; d.p1.stateType = 'A'; d.p1.physics = 'N';   // p1 bem acima da altura (60) de p2, em pe
        World w;
        push_players(d.p1, d.e1, d.p2, d.e2);
        CHECK(d.p1.posX == 0.0f && d.p2.posX == 20.0f, "pulando por cima nao ha empurrao (p1=%f p2=%f)", (double)d.p1.posX, (double)d.p2.posX);
        Duel e(ch, 20);
        World wall; wall.stageLeft = -3.0f; wall.stageRight = 100.0f;
        step_frame(e.p1, e.e1, e.p2, e.e2, wall);
        CHECK(e.p1.posX == -3.0f, "o limite do cenario segura p1 em -3 (p1=%f)", (double)e.p1.posX);
    }

    // ===== Itens 1-6 da revisao (cada regra confirmada no assembly; ver docs/functions.md) =====
    // Atalho: monta um duelo, deixa o HitDef do soco ativo (tick 3) e devolve antes de resolver o hit.
    struct Ready { Duel d; Ready(const Character& c, float dist) : d(c, dist) {
        d.p1.commands.insert("x");
        for (int t = 0; t < 3; ++t) { d.step(); d.p1.commands.clear(); }
        d.e1.tick(d.p1); d.e2.tick(d.p2);              // tick 3: HitDef ativo
    } };

    // 1) hitflag: H so em pe, L so agachado, M os dois (FUN_00441200: S exige bit 4, C exige bit 8)
    CHECK(hitflags_parse("MAF") == 0x3e && hitflags_parse("maf") == 0x3e, "MAF = 0x3E");
    CHECK(hitflags_parse("MAF-") == 0x3c && hitflags_parse("MAF+") == 0x13e, "'-' limpa o bit 2 e '+' liga 0x100");
    CHECK(hitflags_parse("D") == 0x42 && hitflags_parse("HLAF") == 0x3e && hitflags_parse("") == 2, "D = 0x42; letras avulsas; vazio = 2");
    {
        Player t; t.stateType = 'S'; t.moveType = 'I';
        CHECK(hitflag_matches(t, hitflags_parse("H")) && !hitflag_matches(t, hitflags_parse("L")), "em pe: H sim, L nao");
        t.stateType = 'C';
        CHECK(!hitflag_matches(t, hitflags_parse("H")) && hitflag_matches(t, hitflags_parse("L")), "agachado: H NAO, L sim");
        CHECK(hitflag_matches(t, hitflags_parse("M")), "agachado: M sim");
        t.stateType = 'A';
        CHECK(!hitflag_matches(t, hitflags_parse("M")) && hitflag_matches(t, hitflags_parse("A")), "no ar: so A");
        Ready r(ch, 50);
        r.d.p2.stateType = 'C';
        r.d.p1.hitdef.hitflag = hitflags_parse("H");
        resolve_hits(r.d.p1, r.d.e1, r.d.p2, r.d.e2);
        CHECK(r.d.p2.stateNo == 0 && r.d.p2.gethitDamage == 0, "hitflag H nao acerta agachado (estado=%d)", r.d.p2.stateNo);
        Ready q(ch, 50);
        q.d.p2.stateType = 'C';
        q.d.p1.hitdef.hitflag = hitflags_parse("L");
        resolve_hits(q.d.p1, q.d.e1, q.d.p2, q.d.e2);
        CHECK(q.d.p2.stateNo == 5010, "hitflag L acerta agachado e leva ao 5010 (estado=%d)", q.d.p2.stateNo);
    }
    // 2) flag F: alvo caindo so e atingido se o hitflag tem F; '-' / '+' e movetype H
    {
        Player t; t.stateType = 'A'; t.moveType = 'H'; t.gethit.fall = 1;
        CHECK(!hitflag_matches(t, hitflags_parse("MA")) && hitflag_matches(t, hitflags_parse("MAF")), "caindo: exige F");
        t.gethit.fall = 0;
        CHECK(hitflag_matches(t, hitflags_parse("MA")), "em movetype H, o padrao (bit 2) deixa acertar");
        CHECK(!hitflag_matches(t, hitflags_parse("MA-")), "'-' nao acerta quem esta apanhando");
        CHECK(hitflag_matches(t, hitflags_parse("MA+")), "'+' acerta quem esta apanhando (sem guardar)");
        t.moveType = 'I';
        CHECK(!hitflag_matches(t, hitflags_parse("MA+")), "'+' nao acerta quem NAO esta apanhando");
        t.moveType = 'H'; t.guarding = true;
        CHECK(!hitflag_matches(t, hitflags_parse("MA+")), "'+' nao acerta quem esta apanhando mas guardando");
    }
    // 3) type None: sem dano, sem estado, sem pausa (atacante e oponente) — testado acima em F; aqui o air.type None
    {
        Ready r(ch, 50);
        r.d.p2.stateType = 'A'; r.d.p2.posY = -30.0f;   // no ar
        r.d.p1.hitdef.airType = 0; r.d.p1.hitdef.hitflag = hitflags_parse("A");
        r.d.p2.moveType = 'I';
        resolve_hits(r.d.p1, r.d.e1, r.d.p2, r.d.e2);
        CHECK(r.d.p2.stateNo == 0 && r.d.p1.pauseTime == 0 && r.d.p2.pauseTime == 0 && r.d.p2.gethitDamage == 0, "air.type None no ar: nada acontece");
    }
    // 4) tempo de hit: yvel != 0 usa air.hittime e NAO mexe no slidetime; yvel == 0 usa ground.hittime/slidetime
    {
        Ready r(ch, 50);
        r.d.p1.hitdef.groundVelY = -3.0f; r.d.p1.hitdef.groundVelYSet = true; r.d.p1.hitdef.airHitTime = 17; r.d.p1.hitdef.groundHitTime = 11;
        r.d.p2.gethit.slideTime = 99;
        resolve_hits(r.d.p1, r.d.e1, r.d.p2, r.d.e2);
        CHECK(r.d.p2.gethit.hitTime == 17, "yvel != 0 deveria usar air.hittime (17), usou %d", r.d.p2.gethit.hitTime);
        CHECK(r.d.p2.gethit.slideTime == 99, "yvel != 0 nao define o slidetime (ficou %d)", r.d.p2.gethit.slideTime);
        Ready q(ch, 50);
        q.d.p1.hitdef.airHitTime = 17; q.d.p1.hitdef.groundHitTime = 11; q.d.p1.hitdef.groundSlideTime = 5;
        q.d.p2.gethit.slideTime = 99;
        resolve_hits(q.d.p1, q.d.e1, q.d.p2, q.d.e2);
        CHECK(q.d.p2.gethit.hitTime == 11 && q.d.p2.gethit.slideTime == 5, "yvel == 0 usa ground (hit=%d slide=%d)", q.d.p2.gethit.hitTime, q.d.p2.gethit.slideTime);
        // queda com yvel omitido (0): o exe poe -0.001 e entao usa air.hittime
        Ready f(ch, 50);
        f.d.p1.hitdef.hitflag |= 1; f.d.p1.hitdef.airHitTime = 17;
        resolve_hits(f.d.p1, f.d.e1, f.d.p2, f.d.e2);
        CHECK(f.d.p2.gethit.yvel < 0.0f && f.d.p2.gethit.yvel > -0.002f && f.d.p2.gethit.hitTime == 17, "fall com yvel 0: -0.001 e air.hittime (yvel=%f hit=%d)", (double)f.d.p2.gethit.yvel, f.d.p2.gethit.hitTime);
    }
    // 5) dano: pendente aplicado no tick; defence; minimo 1; kill; alive
    {
        Player t; t.life = 100; t.defenceMul = 1.0f;
        CHECK(hit_damage(t, 23, 1) == 23, "dano simples");
        t.defenceMul = 0.5f;  CHECK(hit_damage(t, 23, 1) == 12, "defence 0.5: trunc(11.5+0.5) = 12, deu %d", hit_damage(t, 23, 1));
        t.defenceMul = 0.01f; CHECK(hit_damage(t, 23, 1) == 1, "o minimo 1 vale quando o defence zera o dano, deu %d", hit_damage(t, 23, 1));
        CHECK(hit_damage(t, 0, 1) == 0, "dano 0 continua 0");
        t.defenceMul = 1.0f;
        CHECK(hit_damage(t, 150, 1) == 150, "kill = 1: pode passar da vida");
        CHECK(hit_damage(t, 150, 0) == 99, "kill = 0: deixa 1 de vida (100 - 1 = 99)");
        t.alive = false;
        CHECK(hit_damage(t, 150, 0) == 150, "kill = 0 so protege quem esta vivo");
        t.alive = true; t.gethitDamage = 40;
        CHECK(hit_damage(t, 23, 1) == 23, "so a parcela nova e devolvida");
        // QUIRK do exe (0x43F971-0x43F97F): o teto vira (vida - pendente) - 1 = 59 e o pendente (40) e subtraido de novo: devolve 19
        CHECK(hit_damage(t, 100, 0) == 19, "kill = 0 com dano pendente reproduz o quirk do exe: 19, deu %d", hit_damage(t, 100, 0));
        // no motor: o padrao do HitDef e kill = 1; vida chega a 0 so no tick seguinte
        Ready k(ch, 50);
        k.d.p2.life = 20;
        resolve_hits(k.d.p1, k.d.e1, k.d.p2, k.d.e2);
        CHECK(k.d.p2.life == 20 && k.d.p2.gethitDamage == 23, "o dano nao e aplicado na hora");
        k.d.e2.tick(k.d.p2);
        CHECK(k.d.p2.life == 0, "kill = 1 (padrao): vida chega a 0 (vida=%d)", k.d.p2.life);
        Ready n(ch, 50);
        n.d.p2.life = 20; n.d.p1.hitdef.kill = 0;
        resolve_hits(n.d.p1, n.d.e1, n.d.p2, n.d.e2);
        n.d.e2.tick(n.d.p2);
        CHECK(n.d.p2.life == 1, "kill = 0: a vida para em 1 (vida=%d)", n.d.p2.life);
        Ready m(ch, 50);
        m.d.p2.defenceMul = 100.0f / 120.0f;
        resolve_hits(m.d.p1, m.d.e1, m.d.p2, m.d.e2);
        CHECK(m.d.p2.gethitDamage == 19, "defence 120: trunc(23*0.8333+0.5) = 19, deu %d", m.d.p2.gethitDamage);
        // init_player le data.life e data.defence do KFM
        Player ip; Engine ie(ch); ie.init_player(ip);
        CHECK(ip.life == 1000 && ip.lifeMax == 1000, "init_player: vida do KFM");
        std::printf("   KFM: data.defence -> defenceMul = %.4f\n", (double)ip.defenceMul);
    }
    // 6) hit por tras: High <-> Low so para animtype Light/Medium/Hard (0-2)
    {
        for (int at = 0; at <= 5; ++at) {
            Ready r(ch, 50);
            r.d.p2.facing = 1;                                  // por tras
            r.d.p1.hitdef.animType = at;
            r.d.p1.hitdef.groundVelY = -3.0f; r.d.p1.hitdef.groundVelYSet = true;   // yvel != 0: nao converte Back em Hard
            resolve_hits(r.d.p1, r.d.e1, r.d.p2, r.d.e2);
            int want = (at <= 2) ? 2 : 1;
            CHECK(r.d.p2.gethit.groundType == want, "animtype %d por tras: groundtype deveria ser %d, foi %d", at, want, r.d.p2.gethit.groundType);
        }
        // yvel == 0 e animtype Back (3): vira Hard (2) e entao a troca vale
        Ready b(ch, 50);
        b.d.p2.facing = 1; b.d.p1.hitdef.animType = 3;
        resolve_hits(b.d.p1, b.d.e1, b.d.p2, b.d.e2);
        CHECK(b.d.p2.gethit.animType == 2 && b.d.p2.gethit.groundType == 2, "Back no chao vira Hard e depois troca (anim=%d tipo=%d)", b.d.p2.gethit.animType, b.d.p2.gethit.groundType);
        // de frente nunca troca
        Ready f(ch, 50);
        resolve_hits(f.d.p1, f.d.e1, f.d.p2, f.d.e2);
        CHECK(f.d.p2.gethit.groundType == 1, "de frente nao troca (tipo=%d)", f.d.p2.gethit.groundType);
    }
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
