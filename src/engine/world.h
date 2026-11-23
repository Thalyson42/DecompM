#pragma once
#include "engine/combat.h"

// Empurrao entre corpos e limites de tela/cenario. Porte de Players_Push (0x0043DE40) / Players_PushApart (0x0043DF60) / Player_ClampToStage (0x0043D8C0) do mugen.exe 1.1b1
// (aritmetica lida no assembly 0x43E03F-0x43E273). Veja docs/mugen_exe_achados.md.
// SUPOSICOES: G = 320 (campo global +0x12888, nao identificado), o jogador "interno" do par e o primeiro argumento (ECX),
// o campo de largura extra (Width controller) vale 0 e o fator de escala entre localcoords e 1.
struct World {
    float stageLeft, stageRight;     // limites do cenario (boundleft/boundright); +-1e9 = sem limite
    World() : stageLeft(-1.0e9f), stageRight(1.0e9f) {}
};

// Aplica o empurrao entre dois jogadores (se as caixas Clsn2 se sobrepoem). 'a' e o jogador "externo" do laco
// do exe e 'b' o "interno". Retorna true se os dois foram afastados.
bool push_players(Player& a, const Engine& ea, Player& b, const Engine& eb);

// Limita a posicao x ao cenario (Player_ClampToStage (0x0043D8C0)).
void clamp_to_stage(Player& p, const World& w);

// Um frame de simulacao completo para duas pessoas: tick dos dois, empurrao, limites e hits (ordem do exe).
void step_frame(Player& a, Engine& ea, Player& b, Engine& eb, const World& w);
