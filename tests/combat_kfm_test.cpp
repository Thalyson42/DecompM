// Teste de combate com o KFM real (dois KFM). Uso: combat_kfm_test <pasta-do-mugen>
// Baseado na documentacao; os valores NAO foram comparados com o mugen.exe.
#include "engine/combat.h"
#include <cstdio>
#include <string>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf("FALHOU: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Duel {
    Engine e1, e2; Player p1, p2;
    Duel(const Character& ch, float dist) : e1(ch), e2(ch) {
        e1.enter_state(p1, 0); e2.enter_state(p2, 0);
        p1.ctrl = p2.ctrl = true; p1.facing = 1; p2.facing = -1; p1.posX = 0; p2.posX = dist;
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
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
