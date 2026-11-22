#include "engine/world.h"
#include <cmath>

static const int kG = 320;   // SUPOSICAO: valor do campo global +0x12888 (ftol); limiar 10*G/320 e meia-separacao 5*G/320

static int size_const(const Engine& e, const char* name, int def) {
    float k; return e.character().constant(name, k) ? (int)k : def;
}

// Largura do jogador: tipo de estado (ar x chao) e se esta virado para o lado "da frente" (frontFacing).
static int body_width(const Player& p, const Engine& e, int frontFacing) {
    bool front = p.facing == frontFacing;
    int size;
    if (p.stateType == 'A') size = front ? size_const(e, "size.air.front", 12)    : size_const(e, "size.air.back", 12);
    else                    size = front ? size_const(e, "size.ground.front", 16) : size_const(e, "size.ground.back", 15);
    int extra = 0;                       // Width controller: ainda nao portado
    // floor((extra + size) * escala + 0.5), escala = 1
    return (int)std::floor((float)(extra + size) + 0.5f);
}

static bool bodies_touch(const Player& a, const Engine& ea, const Player& b, const Engine& eb) {
    const AirFrame* fa = ea.current_frame(a);
    const AirFrame* fb = eb.current_frame(b);
    if (!fa || !fb) return false;
    for (size_t i = 0; i < fa->clsn2.size(); ++i)
        for (size_t j = 0; j < fb->clsn2.size(); ++j)
            if (boxes_overlap(world_box(a, fa->clsn2[i]), world_box(b, fb->clsn2[j]))) return true;
    return false;
}

bool push_players(Player& a, const Engine& ea, Player& b, const Engine& eb) {
    if (!bodies_touch(a, ea, b, eb)) return false;
    Player* P = &b; const Engine* eP = &eb;      // param_1 (ECX)
    Player* Q = &a; const Engine* eQ = &ea;      // in_EAX
    // quem esta em pe e tem o oponente acima da propria altura (pulo por cima) nao empurra
    float hP = (float)size_const(*eP, "size.height", 60), hQ = (float)size_const(*eQ, "size.height", 60);
    if (P->stateType == 'S' && hP < P->posY - Q->posY) return false;
    if (Q->stateType == 'S' && hQ < Q->posY - P->posY) return false;

    // ordem esquerda/direita; empate de x resolvido por lado e tipo de movimento (como no exe)
    bool swap;
    if (Q->posX < P->posX) swap = true;
    else if (Q->posX != P->posX) swap = false;
    else {
        int f = P->facing;
        if (f == Q->facing) {
            if (f == 1)       swap = (Q->moveType != 'A') ? (Q->stateType == 'A') : true;
            else if (f == -1) swap = (P->moveType != 'A') ? (P->stateType == 'A') : true;
            else              swap = false;
        } else swap = (Q->facing == 1);
    }
    Player* L; Player* R; const Engine* eL; const Engine* eR;
    if (swap) { L = Q; eL = eQ; R = P; eR = eP; } else { L = P; eL = eP; R = Q; eR = eQ; }

    int wL = body_width(*L, *eL, 1);             // o da esquerda usa a "frente" se olha para a direita
    int wR = body_width(*R, *eR, -1);            // o da direita usa a "frente" se olha para a esquerda
    float diff = R->posX - L->posX;
    float threshold = (float)(kG * 10) / 320.0f;
    if (diff < threshold) {                      // quase no mesmo lugar: recentra com separacao fixa
        float mid = (R->posX + L->posX) * 0.5f;
        float q = (float)(5 * kG) / 320.0f;
        L->posX = mid - q;
        R->posX = mid + q;
    }
    float total = (float)(wL + wR);
    if (total <= diff) return true;              // diff e o ORIGINAL (o exe nao o recalcula)
    float half = (total - diff) * 0.5f;
    L->posX -= half;
    R->posX += half;
    return true;
}

void clamp_to_stage(Player& p, const World& w) {
    if (p.posX < w.stageLeft)  p.posX = w.stageLeft;
    if (p.posX > w.stageRight) p.posX = w.stageRight;
}

void step_frame(Player& a, Engine& ea, Player& b, Engine& eb, const World& w) {
    ea.tick(a); eb.tick(b);
    push_players(a, ea, b, eb);
    clamp_to_stage(a, w); clamp_to_stage(b, w);
    resolve_hits(a, ea, b, eb);
}
