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
[CORRIGIDO] A aplicação do hit é `Player_Hit` (0x43E480) mais `HitDef_Apply` (0x43ECE0). `0x43FD80` é o `ReversalDef` (`Player_Reverse`). `FUN_0043A9A0` NÃO é de combate: valida que o personagem tem os sprites e animações obrigatórios
("%s (%i) missing required sprites/animation actions"), com a lista de estados/animações 10, 11, 12, 20, 21, 40..., 5000...
No pipeline do frame o hit vem depois da atualização do jogador (`FUN_0043C480`).

Implementado e testado com dois KFM (`tests/combat_kfm_test.cpp`): caixas Clsn1 x Clsn2 em coordenadas do mundo (espelhadas pelo lado),
HitDef (attr, hitflag, damage, pausetime, ground/air velocity, hittime, slidetime, animtype, ground/air type), pausa de hit
(atacante congela, defensor "treme"), estado de apanhar 5000/5010/5020 por statetype, GetHitVar, MoveContact/MoveHit, HitShakeOver,
HitOver, HitVelSet, AnimElem/AnimElemTime e TimeMod (sintaxe antiga).
Resultado do teste: soco 200 a 50 px acerta no tick 3 (AnimElem = 3), p2 perde 23, vai a 5000 (shake de 8 ticks), depois 5001 e volta ao 0
no tick 23, empurrado para 67 px. A 100 px o golpe erra.
Não implementado: guarda, prioridade entre golpes simultâneos, NotHitBy/HitBy, juggle, fall, cantos da tela, virar para o atacante.

## Combate: o que foi CONFERIDO no exe (Player_Hit 0x43E480, HitDef_Apply 0x43ECE0, Player_HitDamage 0x43F930)
[CORRIGIDO: antes este texto atribuía a aplicação do hit à 0x43FD80, que na verdade é o ReversalDef.]
`Player_Hit(atacante, defensor)` (0x43E480) aplica o golpe; chama `HitDef_Apply(.., defensor, hitdef do atacante, x, y, facing, ..)` para calcular a resposta.
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
- Poder: o atacante ganha `param_1[0x49C]`, limitado ao máximo (ver Player_Hit, 0x43E480).
Ainda por conferir: tempos exatos (hittime/slidetime/ctrltime), `p1stateno`/`p2stateno`, juggle, guarda (flag em hitdef+0xBC), prioridade.

## Guarda (src/engine/player.cpp, src/engine/combat.cpp)
CONFERIDO no exe:
- **Intenção de guarda 0x28** (`FUN_00434db0`): exige segurar "trás" (+0x39C), não segurar cima (+0x390), e (movetype I com `ctrl`, ou estado 52,
  ou estados 151/153/155); procura um oponente de outro time com movetype A e chama `FUN_004347a0`. Também há três flags por statetype do
  personagem (+0x2AC/+0x2AD/+0x2AE, bytes) que proíbem guardar (NÃO portadas) e o caso de projéteis (NÃO portado).
- **`FUN_004347a0` = trigger InGuardDist**: oponente com movetype A e `0 <= (x_do_jogador - x_do_oponente) * facing_do_oponente < guard.dist`
  do HitDef do oponente (campo 0x4EB; padrão = `Const(size.attack.dist)`, 160 no KFM). Há o mesmo teste para cada projétil do oponente.
- Com a intenção 0x28, movetype I e `ctrl`: se o estado não é 120/130/131/132/140, zera `vel.x` (salvo no ar ou estado 100) e entra no **estado 120**.
  Esse ramo termina a função sem a regra "estado 20 → 0".
- **Golpe guardado** (`FUN_0043F990`): estado 150 (em pé), 152 (agachado) ou 154 (no ar); `ctrl = 0`, `movetype = H`, `Time = 0`;
  velocidade x = guard.velocity.x × (-facing do atacante); o erro "Guard state error: wrong move type" ocorre se o statetype não é 1, 2 ou 3.
- **Fim da guarda (estado 140)** é "implementado no motor" (comentário `[State 140, 5]` do common1.cns): código em `FUN_0043C480`
  (0x43CA12–0x43CA75): com `Time >=` duração da animação, vai para 0 (em pé), 11 (agachado) ou 51 (no ar).
SUPOSIÇÕES restantes (o resto foi conferido na seção "Pipeline de hits"):
- `guardflag`: H guarda em pé, L agachado, M os dois, A no ar (significado exato de `FUN_00441200` ainda aberto);
- padrões de guard.velocity/hittime/slidetime/ctrltime vêm da documentação; guard.dist usa size.attack.dist.
Teste com dois KFM: p2 segura "trás" e entra no estado 120 no tick 0 (soco já ativo no exe: oponente em movetype A), guarda o soco no tick 3
sem perder vida, é empurrado e volta ao estado 0 com controle (150 → 151 → 130 → 140 → 0). Fora da distância (200 px) ele só anda.
Não portado: guard damage com a regra de dano mínimo, air guard completo, guarda de projéteis, as flags por statetype.

## Pipeline de hits (`FUN_0043D950`, por frame) e a regra de guarda CONFERIDA
Ordem em `FUN_0043D950` (leitura do decompilado):
1. Monta a lista de jogadores vivos (e com HitDef/projétil).
2. Decrementa o contador +0x10D4 de cada um (imunidade pós-hit do oponente, a confirmar) e zera +0x1424.
3. **Empurrão entre corpos** (`FUN_0043DE40`) para cada par de times diferentes: se os dois têm o flag de empurrão (+0x298) e as caixas
   de corpo se sobrepõem, afasta os jogadores (`FUN_0043DF60`, `FUN_0043D8C0`, `FUN_0043D750`). NÃO portado: é um recurso importante que falta.
4. [A RECONFERIR] Para cada par ordenado de times diferentes ele testa os dois casos: `Player_CanReverse` (0x43FC60) → `Player_Reverse` (0x43FD80), que são do
   ReversalDef, e `Player_CanHit` (0x43E280) → `Player_Hit` (0x43E480). Os passos 4–6 desta lista foram escritos com rótulos errados e serão refeitos
   quando a prioridade for portada.
5. **Prioridade** entre golpes simultâneos: compara os campos +0x12AC (prioridade) e +0x12B0 (tipo: 0 Hit, 2 Dodge...) dos dois e grava em
   `jogador+0x122C[outro.id]` se o golpe vale (NÃO portado).
6. Aplica os golpes permitidos com `FUN_0043E480(atacante, defensor)` e depois `FUN_004405C0` (par a par).

`FUN_0043E480` decide acerto x guarda (conferido):
- Sai se o oponente tem o contador +0x10D4 > 0.
- **Guardado** se: `defensor+0xEE8 != 0` (flag "guardando") E `FUN_00441200(defensor, guardflag do golpe) != 0` E o byte +0x2B3 do atacante é 0
  (golpes não guardáveis).
- Dano de guarda (`FUN_0043F930`): se `vida - dano_de_guarda < 1` o golpe NÃO é guardado e vira um hit normal (salvo uma flag global de chip-kill,
  a conferir); senão fica guardado.
- O flag +0xEE8 é zerado a cada tick em `FUN_00434DB0` e ligado: (a) quando o jogador segura "trás" sem "cima", com controle (ou estados 151/153/155)
  e há oponente atacando (movetype A) ANTES de checar a distância; (b) na intenção 0x28 (movetype I com controle); (c) em `FUN_0043F990` (golpe guardado).
Isso substitui a suposição anterior ("estado entre 120 e 155"). O porte usa `Player::guarding`.
Ainda aberto: o significado exato de `FUN_00441200` (porte usa guardflag H/L/M/A contra o statetype), o byte +0x2B3, a chip-kill e as tabelas de HitBy/NotHitBy (loop em defensor+0x1180).

## Empurrão entre corpos e limites (src/engine/world.*): PORTE EXATO da aritmética do exe
Funções: `FUN_0043DE40` (decide), `FUN_0043DF60` (empurra; aritmética lida no assembly 0x43E03F–0x43E273), `FUN_0043D8C0` (limite do cenário),
`FUN_0043D750` (limite da tela, NÃO portado). Roda para cada par de times diferentes, ANTES da detecção de hits.
- Só empurra se os dois têm o flag +0x298 (empurrar), as caixas **Clsn2** dos dois se sobrepõem e o flag global +0x11F78 != 1 (ou os dois têm +0x3C0).
- Quem está em pé (statetype S) e tem o oponente acima da própria altura (`size.height`, 60 no KFM) não empurra (passou por cima).
- Ordem esquerda/direita: menor x fica à esquerda; em empate de x vale o lado para onde olham e o movetype (A) / statetype (ar).
- Largura de cada um = `floor((extra + tamanho) * escala + 0.5)`; tamanho = size.ground.front/back ou size.air.front/back, usando "frente" se o jogador
  da esquerda olha para a direita (e o da direita olha para a esquerda). `extra` vem do controller Width (campos +0x2A4/+0x2A8; NÃO portado, vale 0).
- `diff = R.x - L.x`. Se `diff < 10*G/320` (quase no mesmo lugar) recentra: `L.x = meio - 5*G/320`, `R.x = meio + 5*G/320`.
  Depois, se `larguraL + larguraR > diff` (usando o `diff` ORIGINAL, sem recalcular), cada um recua `(soma - diff)/2`.
  `G` é um double global (`[0x5040E8]+0x12888`) ainda não identificado; o porte assume G = 320 (limiar 10, meia-separação 5).
- Limite do cenário: `x` preso a [boundleft, boundright] (escalado) — campos de cenário +0x1E3C/+0x1E40.
Testes (dois KFM): a 20 px afastam para 32 (−6/+26); no mesmo x ficam em −21/+21 (efeito do `diff` original, como no exe);
pulando por cima não empurra; o limite do cenário segura o jogador.
