#pragma once
#include "engine/character.h"
#include "parser/expr.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Estado de um jogador e maquina de estados do MUGEN (um "tick" = 1/60 s).
// Implementado a partir da descricao de docs/cns.html. Fisica (gravidade, atrito, pouso), colisao, hits e
// som ainda NAO existem; controllers nao implementados sao contados em 'skipped'.
// HitDef ativo (controller HitDef). Baseado em docs/sctrls.html; NAO verificado no exe ainda
// (a aplicacao de hits parece ficar em FUN_0043ECE0, 16 KB, a estudar).
struct HitDef {
    bool  active;
    int   attrTypes;                 // bit 1 = S, 2 = C, 4 = A (arg1 de "attr")
    std::string attrKind;            // arg2 de "attr", ex.: "NA"
    std::string hitflag;             // padrao "MAF"
    int   damage, guardDamage;
    int   pause1, pause2;            // pausetime: congela o atacante / faz o oponente tremer
    int   groundHitTime, groundSlideTime, airHitTime;
    float groundVelX, groundVelY, airVelX, airVelY;
    bool  groundVelYSet, airVelYSet;   // omitido: o exe mantem a velocidade y atual do oponente
    int   groundType, airType;       // 1 = High, 2 = Low, 3 = Trip, 0 = None
    int   animType;                  // 0 Light, 1 Medium, 2 Hard, 3 Back, 4 Up, 5 DiagUp
    float yaccel;
    int   fall;
    int   priority;
    bool  hitOnce;
    HitDef() : active(false), attrTypes(0), hitflag("MAF"), damage(0), guardDamage(0), pause1(0), pause2(0),
        groundHitTime(0), groundSlideTime(0), airHitTime(20), groundVelX(0), groundVelY(0), airVelX(0), airVelY(0), groundVelYSet(false), airVelYSet(false),
        groundType(1), airType(1), animType(0), yaccel(0.35f), fall(0), priority(4), hitOnce(true) {}
};

// Valores de GetHitVar(...) do ultimo golpe recebido.
struct GetHitVars {
    int animType, groundType, airType, slideTime, ctrlTime, hitTime, fall, damage;
    float xvel, yvel, yaccel, fallYvel;   // xvel em coordenada ABSOLUTA (ja multiplicada por -facing do atacante)
    GetHitVars() : animType(0), groundType(0), airType(0), slideTime(0), ctrlTime(0), hitTime(0), fall(0), damage(0),
        xvel(0), yvel(0), yaccel(0), fallYvel(0) {}
};

struct Player {
    int   stateNo, prevStateNo, time;
    bool  ctrl;
    int   anim, animElapsed;          // ticks desde o inicio da animacao atual
    float posX, posY, velX, velY;
    int   facing;                     // 1 = direita, -1 = esquerda
    int   life, power;
    char  stateType, moveType, physics;
    int   var[60];
    float fvar[40];
    // combate
    HitDef hitdef;
    GetHitVars gethit;
    int   pauseTime;                  // hitpause: ticks restantes (atacante) / shake (defensor)
    int   hitTime;                    // ticks restantes no estado de apanhar depois do shake
    bool  moveContact, moveHit;       // vale ate trocar de estado
    int   life0;                      // vida inicial (so para informacao)
    std::set<std::string> commands;   // comandos ativos neste tick (minusculas), definidos pelo chamador
    std::map<std::string, int> persist; // contadores de "persistent" por controller, zerados ao trocar de estado
    std::map<std::string, int> skipped; // controllers sem implementacao ja encontrados -> quantas vezes
    Player();
};

class Engine {
public:
    explicit Engine(const Character& ch) : ch_(ch) {}
    // Entra em um estado (aplica os parametros do Statedef). Falso se o estado nao existe.
    bool enter_state(Player& p, int stateNo);
    // Um tick: estados -3, -2, -1, depois o estado atual; no fim avanca Time e a animacao.
    void tick(Player& p);
    // Movimento embutido do exe (FUN_00434db0): parado <-> andando (20), agachar (10/11/12), pular (40).
    // Roda no inicio de cada tick, antes dos estados -3/-2/-1 e do estado atual. Veja docs/mugen_exe_achados.md.
    void default_movement(Player& p);
    // Fisica por tick (parte de FUN_0043c480): pos += vel; depois atrito (S/C) ou gravidade + pouso (A).
    void physics(Player& p);
    // Soma dos tempos da acao (-1 = infinito, 0 = acao inexistente).
    int anim_looptime(int action) const;
    // Elemento de animacao em exibicao (nullptr se a acao nao existe) e tempo desde o inicio do elemento n (1-based).
    const AirFrame* current_frame(const Player& p) const;
    bool anim_elem_time(const Player& p, int n, int& out) const;
    bool anim_exists(int action) const { return ch_.anims.count(action) != 0; }
    const Character& character() const { return ch_; }
private:
    friend struct PlayerCtx;
    const Character& ch_;
    std::map<std::string, std::shared_ptr<ExprNode> > cache_;
    const ExprNode* parse(const std::string& text);
    bool eval(Player& p, const std::string& text, ExprValue& out);
    bool triggered(Player& p, const CnsController& c);
    bool param_values(Player& p, const CnsController& c, const char* key, std::vector<ExprValue>& out);
    bool param_int(Player& p, const CnsController& c, const char* key, int& out);
    bool param_float(Player& p, const CnsController& c, const char* key, float& out);
    // Executa o controller. Retorna true se trocou de estado.
    bool exec(Player& p, const CnsController& c);
    void set_anim(Player& p, int action, int elem);
    void exec_hitdef(Player& p, const CnsController& c);
    std::string raw_param(const CnsController& c, const char* key) const;
};
