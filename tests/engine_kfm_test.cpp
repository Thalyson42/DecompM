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

    // 7) pulo no ar (FUN_00434db0, intencao 10 -> estado 45). Assembly: 0x4351CA (evento de teclas == 1 com statetype A),
    //    0x435370-0x4353C4 (exige ctrl, +0x278 > 0 e airjump.height < -posY; entra no 45 e faz +0x278--).
    //    O contador +0x278 recebe Const(movement.airjump.num) no tick 0 do estado 40 (Player_Update 0x43C96A).
    {
        Player j; eng.enter_state(j, 0); j.ctrl = true; j.facing = 1;
        CHECK(j.airJumpsLeft == 0, "antes de pular o contador de pulos no ar e 0, veio %d", j.airJumpsLeft);
        j.commands.insert("holdup");
        eng.tick(j);
        j.commands.clear();
        CHECK(j.stateNo == 40, "holdup leva ao 40, esta em %d", j.stateNo);
        CHECK(j.airJumpsLeft == 1, "tick 0 do estado 40: contador = airjump.num (1), veio %d", j.airJumpsLeft);
        // logo depois de sair do chao (posY > -35) o pulo no ar ainda nao vale
        bool lowTested = false;
        for (int i = 0; i < 40 && !(j.posY < -40.0f); ++i) {
            eng.tick(j);
            if (!lowTested && j.stateNo == 50 && j.ctrl && j.posY > -30.0f && j.posY < 0.0f) {
                j.inputEvent = 1;
                eng.tick(j);
                CHECK(j.stateNo != 45 && j.airJumpsLeft == 1, "abaixo de airjump.height (35) nao pode pular no ar (estado=%d, contador=%d, posY=%f)", j.stateNo, j.airJumpsLeft, (double)j.posY);
                lowTested = true;
            }
        }
        CHECK(lowTested, "o teste de altura baixa nao rodou");
        CHECK(j.posY < -40.0f && j.ctrl, "deveria estar acima de 40 px com controle (posY=%f ctrl=%d estado=%d)", (double)j.posY, (int)j.ctrl, j.stateNo);
        // evento diferente de 1: nada acontece
        j.inputEvent = 0x11; eng.tick(j);
        CHECK(j.stateNo != 45, "so o evento 1 dispara o pulo no ar (estado=%d)", j.stateNo);
        // sem controle: nada acontece
        bool ctrlSave = j.ctrl; j.ctrl = false; j.inputEvent = 1; eng.tick(j); j.ctrl = ctrlSave;
        CHECK(j.stateNo != 45, "sem ctrl nao pula no ar (estado=%d)", j.stateNo);
        // evento 1 com controle, alto e com pulos sobrando: entra no 45 e gasta um pulo
        j.inputEvent = 1; eng.tick(j);
        CHECK(j.stateNo == 45 || j.prevStateNo == 45, "deveria entrar no estado 45 (estado=%d prev=%d)", j.stateNo, j.prevStateNo);
        CHECK(j.airJumpsLeft == 0, "o pulo no ar gasta um pulo (contador=%d)", j.airJumpsLeft);
        // sem pulos sobrando: um segundo evento nao entra de novo
        for (int i = 0; i < 6; ++i) eng.tick(j);
        j.ctrl = true; j.inputEvent = 1; eng.tick(j);
        CHECK(j.airJumpsLeft == 0 && j.stateNo != 45, "sem pulos sobrando nao entra no 45 de novo (contador=%d estado=%d; se entrasse, o contador iria a -1)", j.airJumpsLeft, j.stateNo);
        // o evento e consumido a cada tick (o exe tira um valor da fila por tick)
        CHECK(j.inputEvent == 0, "o evento deveria ser consumido por tick");
    }
    // 8) AssertSpecial nowalk (flag +0x2B2 = id 6, parser em 0x459748): o exe limpa os flags DEPOIS do 434db0 e antes do Player_Update
    //    (0x41389E, apos a chamada em 0x413821), entao o flag marcado no tick N vale para o movimento do tick N+1.
    {
        Player w; eng.enter_state(w, 0); w.ctrl = true; w.facing = 1;
        w.assertFlags[6] = 1; w.commands.insert("holdfwd");
        eng.tick(w);
        CHECK(w.stateNo == 0, "nowalk marcado: segurar frente nao anda (estado=%d)", w.stateNo);
        CHECK(w.assertFlags[6] == 0, "o flag e limpo depois do movimento embutido");
        eng.tick(w);
        CHECK(w.stateNo == 20, "sem o flag volta a andar (estado=%d)", w.stateNo);
        // pelo controller: copia do KFM com um AssertSpecial no estado -1
        Character c2 = ch;
        CnsController ac; ac.stateNumber = -1; ac.type = "AssertSpecial"; ac.line = 0;
        ac.triggers.push_back(std::vector<std::string>(1, "1"));
        CnsParam pr; pr.key = "flag"; pr.value = "nowalk"; pr.line = 0; ac.params.push_back(pr);
        CHECK(c2.states.count(-1) != 0, "KFM deveria ter o estado -1");
        c2.states[-1].controllers.push_back(ac);
        Engine e2(c2);
        Player u; e2.enter_state(u, 0); u.ctrl = true; u.facing = 1;
        e2.tick(u);                       // tick 0: o controller marca nowalk
        CHECK(u.assertFlags[6] == 1, "AssertSpecial nowalk deveria marcar o flag 6");
        u.commands.insert("holdfwd");
        for (int i = 0; i < 5; ++i) e2.tick(u);
        CHECK(u.stateNo == 0, "com AssertSpecial nowalk todo tick, frente nao anda (estado=%d)", u.stateNo);
        Player z; eng.enter_state(z, 0); z.ctrl = true; z.facing = 1; z.commands.insert("holdfwd");
        for (int i = 0; i < 5; ++i) eng.tick(z);
        CHECK(z.stateNo == 20, "sem AssertSpecial o mesmo KFM anda (estado=%d)", z.stateNo);
    }
    // 9) agachar zera a velocidade X (FUN_00434db0 0x43544D: fldz/fstp [+0x248], salvo estado 100)
    {
        Player k; eng.enter_state(k, 0); k.ctrl = true; k.facing = 1;
        k.velX = 3.0f; k.commands.insert("holddown");
        eng.tick(k);
        CHECK(k.stateNo == 10, "holddown entra no estado 10 (estado=%d)", k.stateNo);
        CHECK(k.velX == 0.0f || (k.velX > -0.001f && k.velX < 0.001f), "agachar zera a velocidade X (vel=%f)", (double)k.velX);
    }

    std::printf("controllers/triggers ainda nao implementados encontrados:\n");
    for (std::map<std::string,int>::const_iterator it = p.skipped.begin(); it != p.skipped.end(); ++it)
        std::printf("   %s (%d)\n", it->first.c_str(), it->second);
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
