#pragma once
#include "engine/character.h"
#include "parser/expr.h"
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Estado de um jogador e maquina de estados do MUGEN (um "tick" = 1/60 s).
// Implementado a partir da descricao de docs/cns.html. Fisica (gravidade, atrito, pouso), colisao, hits e
// som ainda NAO existem; controllers nao implementados sao contados em 'skipped'.
// HitDef ativo (controller HitDef). Os valores omitidos seguem o parser do exe (hitflag 0x3E, kill 1); o layout NAO e o do exe.
// (a aplicacao de hits e Player_Hit 0x43E480 + HitDef_Apply 0x43ECE0).
struct HitDef {
    bool  active;
    int   attrTypes;                 // bit 1 = S, 2 = C, 4 = A (arg1 de "attr")
    std::string attrKind;            // arg2 de "attr", ex.: "NA"
    uint32_t hitflag;                // mascara (HitFlags_Parse, 0x459FD0); padrao 0x3E; bit 0 = fall
    uint32_t guardflag;              // mascara; 0 = nao guardavel
    int   kill, guardKill;           // padrao 1 (exe: push 1 no parser do HitDef)
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
    // guarda (docs); kUnset = omitido -> o padrao e resolvido na hora do golpe
    int   guardDist;                 // x-distancia de guarda; padrao = Const(size.attack.dist) (exe: campo 0x4EB)
    int   guardPause1, guardPause2;
    float guardVelX;
    int   guardHitTime, guardSlideTime, guardCtrlTime;
    HitDef() : active(false), attrTypes(0), hitflag(0x3e), guardflag(0), kill(1), guardKill(1), damage(0), guardDamage(0), pause1(0), pause2(0),
        groundHitTime(0), groundSlideTime(0), airHitTime(20), groundVelX(0), groundVelY(0), airVelX(0), airVelY(0), groundVelYSet(false), airVelYSet(false),
        groundType(1), airType(1), animType(0), yaccel(0.35f), fall(0), priority(4), hitOnce(true),
        guardDist(-1), guardPause1(-9999), guardPause2(-9999), guardVelX(-9999.0f),
        guardHitTime(-9999), guardSlideTime(-9999), guardCtrlTime(-9999) {}
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
    bool  moveContact, moveHit, moveGuarded;   // vale ate trocar de estado
    bool  guarding;                   // exe: campo +0xEE8 (in_guard_dist); recalculado a cada tick por default_movement
    int   gethitDamage;               // exe: +0x1104; dano pendente, aplicado a vida no proximo tick
    int   airJumpsLeft;               // exe: +0x278; pulos no ar restantes. Recebe Const(movement.airjump.num) no tick 0 do estado 40; zera ao apanhar/guardar
    unsigned char assertFlags[8];     // exe: bytes +0x2AC+id (flags do AssertSpecial): 0 nostandguard, 1 nocrouchguard, 2 noairguard, 6 nowalk.
                                      // Limpos pelo exe DEPOIS de FUN_00434db0 e antes de Player_Update (0x41389E): o flag marcado no tick N vale no tick N+1.
    int   inputEvent;                 // valor tirado da fila de teclas (anel de 30 em +0x314) neste tick; 0 = fila vazia. O pulo no ar exige exatamente 1.
                                      // Quem produz os valores (0x4134C9, 0x433B90...) ainda NAO esta portado: o chamador preenche. Consumido por default_movement.
    float defenceMul;                 // exe: +0x1E4 = 100 / Const(data.defence)
    int   lifeMax;                    // exe: +0x1BC
    bool  alive;                      // exe: +0xF00 (trigger Alive)
    const Player* enemy;              // oponente (para InGuardDist); definido pelo chamador
    int   defaultGuardDist;           // Const(size.attack.dist); -1 = ainda nao lido
    int   life0;                      // vida inicial (so para informacao)
    std::set<std::string> commands;   // comandos ativos neste tick (minusculas), definidos pelo chamador
    std::map<std::string, int> persist; // contadores de "persistent" por controller, zerados ao trocar de estado
    std::map<std::string, int> skipped; // controllers sem implementacao ja encontrados -> quantas vezes
    Player();
};

// Converte as letras de hitflag/guardflag em mascara (porte de HitFlags_Parse, 0x00459FD0): comeca em 2; '-' limpa o bit 2;
// '+' liga 0x100; H=4, L=8, M=0xC, A=0x10, F=0x20, D=0x40, P=0x80; maiusculas e minusculas valem igual.
uint32_t hitflags_parse(const std::string& s);

class Engine {
public:
    explicit Engine(const Character& ch) : ch_(ch) {}
    // Valores iniciais que vem do personagem: vida, vida maxima e defence (Const(data.life), Const(data.defence)).
    void init_player(Player& p) const;
    // Entra em um estado (aplica os parametros do Statedef). Falso se o estado nao existe.
    bool enter_state(Player& p, int stateNo);
    // Um tick: estados -3, -2, -1, depois o estado atual; no fim avanca Time e a animacao.
    void tick(Player& p);
    // Movimento embutido do exe (Player_UpdateInput (0x00434DB0)): parado <-> andando (20), agachar (10/11/12), pular (40).
    // Roda no inicio de cada tick, antes dos estados -3/-2/-1 e do estado atual. Veja docs/mugen_exe_achados.md.
    void default_movement(Player& p);
    // Fisica por tick (parte de Player_Update (0x0043C480)): pos += vel; depois atrito (S/C) ou gravidade + pouso (A).
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
