# Achados no mugen.exe 1.1b1 (decompilação do Ghidra, 2412 funções)

Endereços são VAs do exe (ImageBase 0x00400000). Nomes abaixo são NOSSOS; o Ghidra mostra `FUN_xxxxxxxx`.
Tudo aqui vem de ler o decompilado/assembly; confira antes de tratar como definitivo.

## Avaliador de triggers: `FUN_00456350`
- Assinatura: `void __cdecl (int* player, int node)`. 113 casos. Ao final: "Missing trigger code %i".
- O código do trigger é `*(node+0xc)` (máx. 400). O `switch` usa uma **tabela de bytes em 0x00458664** que converte
  opcode → número do caso. (A tabela nome → opcode está em `docs/trigger_opcodes.md`.)
- Exemplos (opcode → caso): ctrl 47 → 0x23, stateno 40 → 0x1C, prevstateno 41 → 0x1D, statetype 43 → 0x1F, command 20 → 0x11.

## Campos do jogador (índice de int a partir do ponteiro do jogador; offset em bytes = índice * 4)
| Campo | Índice | Offset |
|---|---|---|
| stateno | 0x333 | 0xCCC |
| prevstateno | 0x334 | 0xCD0 |
| tempo no estado (Time) | 0x3B5 | 0xED4 |
| statetype | 0x3B6 | 0xED8 |
| movetype | 0x3B7 | 0xEDC |
| ctrl | 0x3B9 | 0xEE4 |
| flags de comando ativos | - | +0xA34 (1 byte por comando, 0x80 bytes) |

## Troca de estado: `FUN_0043bf40` (`__fastcall`, novo estado em EAX, jogador em ESI)
Faz: `prev = stateno; stateno = EAX; Time = 0;` e valida o estado. Mensagem: "changed to invalid state %i (from state %i)".
O Ghidra esconde o argumento (registrador). 32 chamadas; valores literais vistos: 10, 11, 12, 40, 45, 51, 120, 180, 5030, 5120.

## Movimento "embutido": `FUN_00434db0` (chamada de `FUN_00413170`, o loop principal do frame)
O parado/andando/agachado/pulo NÃO está nos .cns/.cmd: está nesta função, que roda para cada jogador **antes** de
`FUN_0043c480` (atualização do jogador). Ela também atualiza o buffer de comandos.

Entradas: quatro flags em +0x390, +0x394, +0x398 e +0x39c (provavelmente cima, uma horizontal, baixo e a outra horizontal;
+0x39c parece ser "trás", pela guarda) e `statetype` (+0xED8: 1 = S, 2 = C, 3 = A).

Cálculo da "intenção" (`param_1`), confirmado no assembly (0x435340–0x435570):
| Intenção | Quando | Efeito (exige `ctrl` salvo indicação) |
|---|---|---|
| 1 | em pé e exatamente uma direção horizontal | entra no **estado 20** se `stateno != 20` e o byte +0x2B2 == 0 |
| 0xB | em pé e baixo (sobrepõe 1) | **estado 10** se `stateno` não é 10 nem 11 |
| 7 | em pé e cima (sobrepõe as anteriores) | **estado 40** (se o campo global +0x12754 != 3) |
| 10 | no ar e máscara de teclas == 1 | **estado 45** (pulo no ar), gasta um pulo (+0x278) |
| 0x28 | inimigo atacando perto e segurando "trás" | **estado 120** (guarda) |
| 900 | (não entendido) | chama FUN_0043C460 / FUN_0043C220 |
As duas direções horizontais juntas anulam qualquer intenção (`param_1 = -1`).

Sem intenção tratada, no fim da função:
1. `stateno == 20` → `prevstateno = 20; stateno = 0; Time = 0` (sem chamar a função de troca de estado).
2. `statetype == 2` (agachado) com `ctrl` e sem segurar baixo → **estado 12** (agachado → de pé).

Chamadas literais da função de troca (EAX): 45 (0x4353B2), 40 (0x4353E7), 20 (0x435411), 10 (0x43545F), 12 (0x4354C3), 120 (0x435550).
Porte em `Engine::default_movement` (`src/engine/player.cpp`); testado com o KFM real em `tests/engine_kfm_test.cpp`
(0 → 20 → 0, frente+trás não anda, baixo → 10 → 11, soltar → 12 → 0, cima → 40).
AINDA NÃO portado: guarda automática, pulo no ar, o campo +0x248, o byte +0x2B2 e a trava +0x12754.

## Ordem do frame (`FUN_00413170`)
... `FUN_00413e60` (fim de rodada: estados 170/175/180) → `FUN_0043c390` → **`FUN_00434db0` (movimento embutido)** →
`FUN_0043c480` (atualização do jogador) → `FUN_0043d650` → ... → `FUN_0043d950` (→ `FUN_0043fd80`, colisão/hits).

## Outras funções identificadas
- `FUN_0043c480`: atualização por tick do jogador (física/estado; grava estados 0, 52 e 5120).
- `FUN_00434030` / `FUN_00434100`: contam jogadores do mesmo time / de outro time (NumPartner / NumEnemy).
- Tabela de wrappers Lua em 0x004F2208 (ver `docs/trigger_opcodes.md`).

## Física e execução de estados: `FUN_0043c480` (por jogador, por tick)
Campos (índice de int; offset = índice*4): pos x/y/z = doubles em 0x7E/0x80/0x82; vel x/y/z = doubles em 0x92/0x94/0x96;
facing = 0x7A; physics = 0x3B8 (1 = S, 2 = C, 3 = A); pausa (hitpause) = 0x3BF; yaccel (float) = 0x4B;
stand.friction = 0x4C; crouch.friction = 0x4D.

Ordem dentro da função (confirmada no decompilado):
1. Se a pausa (0x3BF) < 1: `pos += vel` (x, y, z); senão decrementa a pausa. (Há um fator de escala entre localcoords, 1.0 no KFM.)
2. Física: S → `vel.x *= stand.friction`; C → `vel.x *= crouch.friction`; A → `vel.y += yaccel` e, se `stateno != 105`, `vel.y > 0` e `pos.y > 0`, pousa:
   `prevstateno = stateno; stateno = 52; Time = 0` (o PosSet y = 0 e o resto vêm do estado 52 do common1.cns).
3. Executa a máquina de estados (`FUN_0045be80`) em laço enquanto ela devolver "trocou de estado". Limite: 2501 voltas; depois aborta
   com "State machine stuck in loop (stopped after %i loops): %i for %s / Check your states for infinite ChangeState loops."

**A velocidade guardada é absoluta:** `VelSet` e `VelAdd` (em `FUN_00449020`) multiplicam o valor por `facing` (0x7A) ao gravar,
e o trigger `Vel X` desfaz isso. Por isso `pos += vel` não multiplica pelo lado.

Constantes: o exe conhece `velocity.jump.y`, `velocity.runjump.y` e `velocity.airjump.y`, que não existem nos .cns. No porte são
tratadas como o y de `jump.neu`, `runjump.fwd` e `airjump.neu` (SUPOSIÇÃO a confirmar no exe).

Porte: `Engine::physics` e `Engine::tick` (`src/engine/player.cpp`). Testes com o KFM real: 10 ticks andando = 24 px, virado para a
esquerda = -24 px, pulo com ápice -84.4 e pouso no tick 43 voltando a `posY = 0`.
Ordem do tick no porte: movimento embutido → física → estados -3/-2/-1 e atual.
Não portado: hitpause, escala entre localcoords, limiar de atrito do exe, o `z`.

## Combate (src/engine/combat.*): PRIMEIRA FATIA, baseada na documentação (NÃO verificada no exe)
Pistas no exe para conferir depois: `FUN_0043D950` → `FUN_0043FD80` (colisão/hits, 8 KB) e `FUN_0043ECE0` (16 KB, grava os estados 5020, 5070
e 5080 de apanhar). `FUN_0043A9A0` NÃO é de combate: valida que o personagem tem os sprites e animações obrigatórios
("%s (%i) missing required sprites/animation actions"), com a lista de estados/animações 10, 11, 12, 20, 21, 40..., 5000...
No pipeline do frame o hit vem depois da atualização do jogador (`FUN_0043C480`).

Implementado e testado com dois KFM (`tests/combat_kfm_test.cpp`): caixas Clsn1 x Clsn2 em coordenadas do mundo (espelhadas pelo lado),
HitDef (attr, hitflag, damage, pausetime, ground/air velocity, hittime, slidetime, animtype, ground/air type), pausa de hit
(atacante congela, defensor "treme"), estado de apanhar 5000/5010/5020 por statetype, GetHitVar, MoveContact/MoveHit, HitShakeOver,
HitOver, HitVelSet, AnimElem/AnimElemTime e TimeMod (sintaxe antiga).
Resultado do teste: soco 200 a 50 px acerta no tick 3 (AnimElem = 3), p2 perde 23, vai a 5000 (shake de 8 ticks), depois 5001 e volta ao 0
no tick 23, empurrado para 67 px. A 100 px o golpe erra.
Não implementado: guarda, prioridade entre golpes simultâneos, NotHitBy/HitBy, juggle, fall, cantos da tela, virar para o atacante.

## Combate: o que foi CONFERIDO no exe (FUN_0043FD80, FUN_0043ECE0, FUN_0043F930, FUN_00441410)
`FUN_0043FD80(atacante, defensor)` aplica o golpe; chama `FUN_0043ECE0(.., defensor, hitdef do atacante, x, y, facing, ..)` para calcular a resposta.
O HitDef do atacante é uma estrutura a partir do índice 0x48C (offset 0x1230); o "gethit" do defensor fica em +0x10F0..+0x117C.
Conferido e já aplicado no porte (`src/engine/combat.cpp`):
- Estado de apanhar: em pé → **5000**; agachado → **5010** (`(statetype != 1 ? 10 : 0) + 5000`); no ar → **5020** (0x139C); tipo Trip (3) → **5070** (0x13CE).
- `forcestand` converte agachado em em pé antes de escolher o estado (parâmetro ainda não lido no porte).
- Velocidade x do golpe = valor × **(-facing do atacante)**, em coordenada absoluta; se o y foi omitido no HitDef, o defensor **mantém a velocidade y atual**.
- Atingido por trás (defensor olhando para o mesmo lado do atacante): High ↔ Low.
- Tipo None (0): só pausa; o exe devolve antes de calcular dano e variáveis do golpe.
- Dano (`FUN_0043F930`): se o dano pedido é > 0 e o cálculo dá 0, vira 1. O dano fica acumulado em +0x1104 do defensor.
  Existe uma regra de "não matar" (campo +0xF00): o dano é limitado para deixar 1 de vida. NÃO portada (não sei ainda o que liga esse campo).
- No fim da resposta: `ctrl = 0`, `movetype = H (2)`, `Time = 0`, contador de acertos +1.
- `FUN_00441410` cria a faísca (spark) do hit; não aplica dano. `FUN_0043C0B0` ajusta o poder (limite 0..máximo).
- Poder: o atacante ganha `param_1[0x49C]`, limitado ao máximo (ver FUN_0043FD80).
Ainda por conferir: tempos exatos (hittime/slidetime/ctrltime), `p1stateno`/`p2stateno`, juggle, guarda (flag em hitdef+0xBC), prioridade.
