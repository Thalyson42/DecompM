#pragma once
#include "engine/player.h"

// Deteccao e aplicacao de hits entre dois jogadores.
// Fonte da verdade: assembly do mugen.exe 1.1b1. Funcoes do exe portadas aqui (nomes do unleashed, so como referencia):
//   0x0043E280 Player_CanHit        0x00441200 Player_HitFlagMatches   0x0043E480 Player_Hit
//   0x0043ECE0 HitDef_Apply         0x0043F930 Player_HitDamage        0x0043F990 Player_Guard
// Portado: prioridade entre golpes simultaneos (0x43DB34), juggle (0x43E415, 0x43E619, 0x43E931, 0x417400), unguardable (0x43E50D).
// NAO portado ainda: ReversalDef (0x43FC60/0x43FD80), HitBy/NotHitBy, KO, projeteis, ordem de aplicacao com varios jogadores.
struct WorldBox { float x1, y1, x2, y2; };

// Caixa de colisao em coordenadas do mundo (espelhada quando o jogador olha para a esquerda).
WorldBox world_box(const Player& p, const AirBox& b);
bool boxes_overlap(const WorldBox& a, const WorldBox& b);

// Porte de Player_HitFlagMatches (0x00441200): o hitflag/guardflag 'flags' permite acertar 'p' no estado atual?
bool hitflag_matches(const Player& p, uint32_t flags);
// Porte de Player_HitDamage (0x0043F930): dano deste golpe somado ao pendente, com defence, minimo 1 e a regra de kill.
// Devolve apenas a parcela NOVA (o dano pendente ja existente nao entra).
int hit_damage(const Player& p, int damage, int kill);

// Player_CanHit (0x0043E280): 'atk' pode acertar 'def' agora? (HitDef ativo, caixas, juggle e hitflag.) Nao altera nada.
bool can_hit(const Player& atk, const Player& def, const Engine& engDef, const Engine& engAtk);
// Player_Hit (0x0043E480) sem repetir o CanHit: guarda ou acerta, registra e desconta juggle. Retorna true se houve contato.
bool apply_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk);
// Tenta um golpe de 'atk' em 'def' (can_hit + apply_hit). Retorna true se houve contato (acerto, guarda ou tipo None).
bool resolve_hit(Player& atk, Player& def, Engine& engDef, const Engine& engAtk);
// FUN_00417400: remove o registro de juggle do atacante quando o alvo nao esta mais em hitstun. Chamada ao fim de resolve_hits.
void end_frame_juggle(Player& atk, const Player& def);
// Roda os dois sentidos (P1 -> P2 e P2 -> P1).
void resolve_hits(Player& a, Engine& ea, Player& b, Engine& eb);
