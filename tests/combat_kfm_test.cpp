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
        for (int t = 0; t < 12; ++t) { d.step(); d.p1.commands.clear(); if (hitTick < 0 && d.p2.stateNo == 5000) hitTick = t; }
        std::printf("A) hit no tick %d, vida p2=%d, p2 estado=%d\n", hitTick, d.p2.life, d.p2.stateNo);
        CHECK(hitTick == 3, "o hit deveria ocorrer no tick 3 (AnimElem = 3), ocorreu em %d", hitTick);
        CHECK(d.p2.life == 1000 - 23, "p2 deveria perder 23 de vida, vida = %d", d.p2.life);
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
        CHECK(n.p2.stateNo == 0 && n.p2.life == 1000, "None so pausa: estado 0 e vida intacta (estado=%d vida=%d)", n.p2.stateNo, n.p2.life);
        CHECK(n.p2.pauseTime == 8 && n.p1.pauseTime == 8, "None ainda pausa os dois (p1=%d p2=%d)", n.p1.pauseTime, n.p2.pauseTime);
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
    // H) sem segurar "tras" o mesmo soco acerta; e a 200 px (fora da guard.dist de 160) nao entra em guarda
    {
        Duel d(ch, 50);
        d.p1.commands.insert("x");
        for (int t = 0; t < 4; ++t) { d.step(); d.p1.commands.clear(); }
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
    // J) guarda que mataria (vida - dano de guarda < 1) vira um hit normal (FUN_0043E480, sem chip-kill)
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
        CHECK(d.p2.life == 0, "vida deveria chegar a 0, vida = %d", d.p2.life);
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
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
