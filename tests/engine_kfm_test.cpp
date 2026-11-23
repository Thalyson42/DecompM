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

    // 2) andar: o movimento embutido do exe (Player_UpdateInput (0x00434DB0)) leva 0 -> 20 ao segurar para frente
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

    // 5) fisica: andar 10 ticks avanca ~2.4 por tick (pos += vel; o estado 20 refaz o VelSet a cada tick)
    {
        Player w; eng.enter_state(w, 0); w.ctrl = true; w.facing = 1;
        w.commands.insert("holdfwd");
        for (int i = 0; i < 11; ++i) eng.tick(w);
        CHECK(w.stateNo == 20, "andando: estado 20, esta em %d", w.stateNo);
        CHECK(w.posX > 23.5f && w.posX < 24.5f, "10 ticks andando deveriam dar posX ~24.0, deu %f", (double)w.posX);
        // virado para a esquerda: a velocidade guardada e negativa, mas o trigger "Vel X" continua positivo
        Player l; eng.enter_state(l, 0); l.ctrl = true; l.facing = -1;
        l.commands.insert("holdfwd");
        for (int i = 0; i < 11; ++i) eng.tick(l);
        CHECK(l.posX < -23.5f && l.posX > -24.5f, "olhando para a esquerda deveria andar para -24, deu %f", (double)l.posX);
        ExprValue v; std::string err; ExprPtr e = expr_parse("Vel X > 0", &err);
        struct C : ExprContext {
            Player& p; C(Player& pp) : p(pp) {}
            bool call(const std::string& n, const std::vector<ExprValue>&, ExprValue& o) { if (n == "Vel X") { o = ExprValue::Float(p.velX * (float)p.facing); return true; } return false; }
            bool assign(const std::string&, const std::vector<ExprValue>&, const ExprValue&, ExprValue&) { return false; }
            bool redirect(const std::string&, const std::vector<ExprValue>&, const ExprNode*, ExprValue&) { return false; }
        } cc(l);
        v = expr_eval(e.get(), cc);
        CHECK(v.kind == ExprValue::INT && v.i == 1, "Vel X relativo ao lado deveria ser > 0");
    }
    // 6) pulo completo: sobe, desce e pousa no estado 52 (gravidade = Const(movement.yaccel))
    {
        Player j; eng.enter_state(j, 0); j.ctrl = true; j.facing = 1;
        j.commands.insert("holdup");
        eng.tick(j);
        CHECK(j.stateNo == 40, "holdup leva ao estado 40, esta em %d", j.stateNo);
        j.commands.clear();
        float minY = 0; int landedAt = -1, tickNo = 1;
        for (; tickNo < 120; ++tickNo) {
            eng.tick(j);
            if (j.posY < minY) minY = j.posY;
            // o estado 52 pode durar menos de um tick: o common1.cns o encerra no mesmo tick (52 -> 0)
            if (j.stateNo == 52 || j.prevStateNo == 52) { landedAt = tickNo; break; }
        }
        std::printf("pulo: apice=%.1f, pouso no tick %d\n", (double)minY, landedAt);
        CHECK(landedAt > 30 && landedAt < 60, "o pouso deveria ocorrer entre os ticks 30 e 60, ocorreu em %d", landedAt);
        CHECK(minY < -70.0f && minY > -90.0f, "apice do pulo do KFM deveria ficar entre -70 e -90, deu %f", (double)minY);
        for (int i = 0; i < 10 && j.stateNo != 0; ++i) eng.tick(j);
        CHECK(j.stateNo == 0, "depois do pouso o estado deveria voltar a 0, esta em %d", j.stateNo);
        CHECK(j.posY > -0.01f && j.posY < 0.01f, "depois do pouso posY deveria ser 0 (PosSet do estado 52), deu %f", (double)j.posY);
    }

    std::printf("controllers/triggers ainda nao implementados encontrados:\n");
    for (std::map<std::string,int>::const_iterator it = p.skipped.begin(); it != p.skipped.end(); ++it)
        std::printf("   %s (%d)\n", it->first.c_str(), it->second);
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
