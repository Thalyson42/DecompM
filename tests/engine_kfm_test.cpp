// Teste de integracao com o KFM real. Uso: engine_kfm_test <pasta-do-mugen>   (ex.: mugen-1.1b1)
// Os assets NAO fazem parte do repositorio: aponte para a sua copia local do MUGEN.
#include "engine/player.h"
#include <cstdio>
#include <string>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf("FALHOU: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: engine_kfm_test <pasta-do-mugen>"); return 2; }
    std::string root = argv[1]; if (root.back() != '/') root += '/';
    Character ch; std::string err;
    if (!ch.load(root + "chars/kfm/kfm.def", root + "data/", &err)) { std::printf("erro: %s\n", err.c_str()); return 2; }
    std::printf("estados=%zu animacoes=%zu comandos=%zu avisos=%zu\n", ch.states.size(), ch.anims.size(), ch.commandNames.size(), ch.warnings.size());

    Engine eng(ch);
    Player p; p.facing = 1;
    CHECK(eng.enter_state(p, 0), "estado 0 deve existir");
    CHECK(p.stateType == 'S' && p.physics == 'S', "estado 0 deve ser S/S, veio %c/%c", p.stateType, p.physics);
    // O estado 0 nao define ctrl; quem da controle no inicio do round e o motor (fora do escopo por enquanto).
    p.ctrl = true;

    // 1) parado, sem comandos: continua no estado 0
    for (int i = 0; i < 30; ++i) eng.tick(p);
    CHECK(p.stateNo == 0, "sem comandos deveria continuar no estado 0, esta em %d", p.stateNo);

    // 2) andar: o movimento embutido do exe (FUN_00434db0) leva 0 -> 20 ao segurar para frente
    p.commands.insert("holdfwd");
    eng.tick(p);
    CHECK(p.stateNo == 20, "holdfwd deveria levar ao estado 20, esta em %d", p.stateNo);
    CHECK(p.velX > 2.3f && p.velX < 2.5f, "velocidade de andar deveria ser Const(velocity.walk.fwd.x)=2.4, veio %f", (double)p.velX);
    CHECK(p.anim == 20, "andando para frente usa a animacao 20, veio %d", p.anim);
    // 3) solta o botao: volta ao estado 0
    p.commands.clear();
    eng.tick(p);
    CHECK(p.stateNo == 0, "sem direcao o estado 20 deveria voltar ao 0, esta em %d", p.stateNo);
    // as duas direcoes juntas nao fazem nada
    p.commands.insert("holdfwd"); p.commands.insert("holdback");
    eng.tick(p);
    CHECK(p.stateNo == 0, "frente+tras nao deveria andar, esta em %d", p.stateNo);
    p.commands.clear();
    // agachar: down leva ao 10; ao soltar, levanta pelo 12
    p.commands.insert("holddown");
    eng.tick(p);
    CHECK(p.stateNo == 10, "holddown deveria levar ao estado 10, esta em %d", p.stateNo);
    CHECK(p.stateType == 'C', "estado 10 e agachado (C), veio %c", p.stateType);
    for (int i = 0; i < 20; ++i) eng.tick(p);              // segura: ele termina em 11 (agachado)
    CHECK(p.stateNo == 11, "segurando down o agachar deveria terminar no estado 11, esta em %d", p.stateNo);
    p.commands.clear();
    eng.tick(p);
    CHECK(p.stateNo == 12, "soltando down deveria ir ao estado 12, esta em %d", p.stateNo);
    for (int i = 0; i < 30 && p.stateNo != 0; ++i) eng.tick(p);
    CHECK(p.stateNo == 0, "o 12 deveria terminar no estado 0, esta em %d", p.stateNo);
    p.ctrl = true;
    // pular: up leva ao 40
    p.commands.insert("holdup");
    eng.tick(p);
    CHECK(p.stateNo == 40, "holdup deveria levar ao estado 40, esta em %d", p.stateNo);
    p.commands.clear();
    CHECK(eng.enter_state(p, 0), "estado 0 deve existir");
    p.ctrl = true;

    // 4) soco fraco em pe ("x"): kfm.cmd leva ao estado 200 e volta ao 0 quando AnimTime = 0
    p.commands.insert("x");
    eng.tick(p);
    CHECK(p.stateNo == 200, "comando x deveria levar ao estado 200, esta em %d", p.stateNo);
    CHECK(!p.ctrl, "no estado 200 o jogador nao tem controle");
    int anim = p.anim, lt = eng.anim_looptime(anim);
    std::printf("estado 200: anim=%d looptime=%d\n", anim, lt);
    p.commands.clear();
    int back = -1;
    for (int t = 1; t <= lt + 5; ++t) { eng.tick(p); if (p.stateNo != 200) { back = t; break; } }
    CHECK(back == lt, "deveria voltar ao estado 0 no tick %d (looptime), voltou no tick %d", lt, back);
    CHECK(p.stateNo == 0 && p.ctrl, "depois do golpe: estado 0 com controle (estado=%d ctrl=%d)", p.stateNo, (int)p.ctrl);

    std::printf("controllers/triggers ainda nao implementados encontrados:\n");
    for (std::map<std::string,int>::const_iterator it = p.skipped.begin(); it != p.skipped.end(); ++it)
        std::printf("   %s (%d)\n", it->first.c_str(), it->second);
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
