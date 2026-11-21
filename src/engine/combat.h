#pragma once
#include "engine/player.h"

// Deteccao e aplicacao de hits entre dois jogadores. Baseado em docs/cns.html e docs/sctrls.html (HitDef);
// NAO verificado no exe (a aplicacao de hits parece ficar em FUN_0043ECE0). Ainda sem guarda, sem prioridade
// entre golpes simultaneos, sem NotHitBy/HitBy, sem juggle nem fall.
struct WorldBox { float x1, y1, x2, y2; };

// Caixa de colisao em coordenadas do mundo (espelhada quando o jogador olha para a esquerda).
WorldBox world_box(const Player& p, const AirBox& b);
bool boxes_overlap(const WorldBox& a, const WorldBox& b);

// Tenta um golpe de 'atk' em 'def'. Retorna true se acertou (e aplicou o hit).
bool resolve_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk);
// Roda os dois sentidos (P1 -> P2 e P2 -> P1).
void resolve_hits(Player& a, Engine& ea, Player& b, Engine& eb);
