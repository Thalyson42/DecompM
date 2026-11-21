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
    // Soma dos tempos da acao (-1 = infinito, 0 = acao inexistente).
    int anim_looptime(int action) const;
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
};
