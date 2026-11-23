# Registro de funções (mugen.exe, ImageBase 0x00400000)

| Endereço | Nome recuperado | Módulo | Status | Notas |
|---|---|---|---|---|
| 0x004DC80A | entry (CRT startup) | - | conhecido | EntryPoint do PE |

Status: `conhecido` (nome certo) / `rascunho` (comportamento ok, código não) / `igual` (saída idêntica ao original) / `biblioteca` (Lua, zlib etc., não decompilar).

## Notas de formato (SFF v2, validadas nos KFM do MUGEN 1.1b1)
- Dados comprimidos de sprite começam com 4 bytes = largura*altura (tamanho descomprimido).
- Formato 2 = RLE8, formato 4 = LZ5. 281/281 sprites de kfm.sff e kfm720.sff decodificam com tamanho exato.
- Sprite com dataLength == 0 é linkado: usa os dados do sprite em linkedIndex.
- Paleta: 4 bytes por cor (RGBA), offset relativo a ldata; índice 0 tratado como transparente.
- Os decodificadores seguem o algoritmo documentado pela comunidade e foram validados pela saída; ainda falta comparar com as funções do sff2png.exe no Ghidra.

## Notas de formato (AIR, validadas nos .air do MUGEN 1.1b1)
- kfm.air e kfm720.air: 117 ações e 655 elementos cada; fightfx.air: 19 ações e 158 elementos (contagem independente bate).
- As ações 5, 6, 10 e 12 do KFM têm looptime finito, como exige a documentação.
- Quirk do KFM (3 blocos, 5 linhas: 963, 1567, 1568, 1574 e 1575): bloco "Clsn1: N" seguido de linhas "Clsn2[i] = ...". O parser assume que a caixa pertence ao bloco declarado. CONFIRMAR no mugen.exe.

## Notas de formato (CNS/CMD, validadas nos arquivos do MUGEN 1.1b1)
- Contagens batem com `grep` independente: kfm.cns 58 Statedef + 318 controllers, common1.cns 51 + 282, kfm.cmd 1 + 38.
- Todos os `type` usados são controllers da lista oficial (89 nomes, docs/sctrls.html); todo controller tem trigger1 e não há lacunas de numeração.
- O parser é só estrutural: triggers e parâmetros ficam como texto. Próximo passo: tokenizer/avaliador de expressões.
- Regras de trigger seguidas da documentação: triggerall = AND com tudo; triggerN = grupos em OR; lacuna em N faz os seguintes serem ignorados.

## Notas de formato (expressões, validadas nos arquivos do MUGEN 1.1b1)
- `tests/expr_spec_tests.cpp`: 38 testes, baseados nos exemplos de docs/cns.html (precedência, divisão inteira, `**`, intervalos, `:=`, bottom).
- Erro na documentação: o exemplo `1.0 = (2 = (1 > 0) + !(0 < 1))` diz que `0 < 1` vale 0 e conclui 1; pelas regras o resultado é 0. O comportamento real do mugen.exe precisa ser conferido no Ghidra.
- `exprcheck` em kfm.cns, kfm720.cns, common1.cns e kfm.cmd: todos os triggers fazem parse (0 falhas). Falham só 5 parâmetros que NÃO são expressões aritméticas, e sim listas de atributos/flags que cada controller interpreta à sua maneira: `hitflag = M-` e `value = , NA, NP` (HitBy/NotHitBy). Precisam de parsers por controller.
- Limitações conhecidas do avaliador: exceções do "bottom" em `&&`/`||` ainda não tratadas; redirecionamento, triggers e variáveis dependem de um `ExprContext` (ainda sem implementação real).

## Motor de estados (src/engine), validado com o KFM real
- Ordem por tick (docs/cns.html): estados -3, -2, -1 (ChangeState aborta só o restante do estado), depois o estado atual; após ChangeState no estado atual, recomeça do início do novo estado. No fim: Time++ e avanço da animação.
- `tests/engine_kfm_test.cpp` (precisa do MUGEN local, `-DMUGEN_DIR=...`): o comando `x` leva ao estado 200 (kfm.cmd) e o personagem volta ao estado 0 exatamente no tick = looptime da anim 200 (12). É consistência interna, não comparação com o mugen.exe.
- **Achado (RESOLVIDO, ver `docs/mugen_exe_achados.md`):** não existe nenhum `ChangeState` para o estado 20 (andar) nem para fora dele em common1.cns, kfm.cns ou kfm.cmd. A entrada/saída 0 <-> 20 (e provavelmente agachar 10 e pular 40) parece ser lógica embutida no mugen.exe. LOCALIZAR NO GHIDRA (hipótese, ainda não confirmada).
- Constantes: `Const(velocity.walk.fwd.x)` = 2.4 resolvido a partir de [Velocity] do kfm.cns.
- Não implementados ainda (contados em `Player::skipped`): triggers AnimElem e MoveContact, controller PlaySnd e todo o resto (HitDef, Helper, Explod...). Sem física (gravidade, atrito), colisão ou hits.
- Suposições a confirmar no exe: concatenação de estados -1/-2/-3 de vários arquivos; semântica de `persistent = n`; AnimTime = tempo decorrido - looptime; AnimTime com looptime infinito (provisório -1).

## Análise do mugen.exe feita por script (sem Ghidra)
- Tokenizer de expressões: cadeia de `stricmp` de ~114 nomes de trigger → opcode, todos retornando por `0x00454AA0`. Tabela completa em `docs/trigger_opcodes.md` (script: `tools/analise/extrair_triggers.py`).
- Tabela de wrappers Lua em `0x004F2208` (88 entradas nome → função): API de scripts do 1.1, não é a tabela do CNS.
- Próximo alvo: achar o despacho por opcode (onde `ctrl` = 47 e `statetype` = 43 são calculados) e a lógica embutida de 0 <-> 20.

## Hipótese do estado 20 confirmada
A entrada/saída 0 <-> 20 (e agachar 10/12, pular 40/45) é código do exe em `FUN_00434db0`, não dos arquivos .cns/.cmd. Detalhes e offsets do jogador em `docs/mugen_exe_achados.md`.

## Movimento embutido portado
`Engine::default_movement` (porte de FUN_00434db0) roda no início de cada tick. Cobre: 0 <-> 20 (andar), 10/11/12 (agachar), 40 (pular). Faltam guarda automática (120), pulo no ar (45) e os campos +0x248, +0x2B2 e +0x12754 (ver docs/mugen_exe_achados.md).

## Física portada
`Engine::physics`: pos += vel; atrito S/C; gravidade e pouso (estado 52) em A. Velocidade guardada em coordenada absoluta (VelSet/VelAdd multiplicam pelo lado). Limite de loops da máquina de estados = 2501, como no exe. Detalhes em docs/mugen_exe_achados.md.

## Combate (primeira fatia)
`src/engine/combat.*`: Clsn1 x Clsn2, HitDef, pausa de hit, estado de apanhar 5000/5010/5020, GetHitVar. Baseado na documentação. Pendente: conferir em FUN_0043FD80 e FUN_0043ECE0 (ver docs/mugen_exe_achados.md).

## Guarda portada
InGuardDist (FUN_004347a0), intenção de guarda 0x28 em `default_movement` (FUN_00434db0), guarda do golpe (150/152/154) e fim da guarda (estado 140 -> 0/11/51, FUN_0043C480). Detalhes e o que ainda é suposição em docs/mugen_exe_achados.md.

## Empurrão entre corpos portado
`push_players`, `clamp_to_stage`, `step_frame` (`src/engine/world.*`): aritmética lida no assembly de FUN_0043DF60. Suposição: G = 320. Detalhes em docs/mugen_exe_achados.md.

## Combate: revisão com o assembly do mugen.exe (ver `tools/analise/asm.py`)
Regras de trabalho: a fonte da verdade é o assembly; o Ghidra só localiza; o mugen-unleashed é segunda opinião (nunca copiado); a documentação do MUGEN é só dica.
Nomes entre parênteses são os do unleashed, usados apenas como referência. Sinais: CONFIRMADO = li o assembly; PARCIAL = parte conferida.

| Endereço | Função (nome de referência) | Estado |
|---|---|---|
| 0x00441200 | `Player_HitFlagMatches` | CONFIRMADO, portado (`hitflag_matches`) |
| 0x00459FD0 | `HitFlags_Parse` | CONFIRMADO, portado (`hitflags_parse`) |
| 0x0043F930 | `Player_HitDamage` | CONFIRMADO, portado (`hit_damage`) |
| 0x0043ECE0 | `HitDef_Apply` | PARCIAL: tipo None, estados 5000/5010/5020/5070, yvel, tempos, troca High/Low, dano, shake; faltam KO, alvo deitado (5080), fall.* |
| 0x0043E480 | `Player_Hit` | PARCIAL: guarda x hit, pausa do atacante, MoveHit/MoveContact; faltam HitOverride, alvos, power, combo |
| 0x0043E280 | `Player_CanHit` | PARCIAL: caixas + hitflag; falta HitBy/NotHitBy |
| 0x0043C480 | `Player_Update` | PARCIAL: aplicação do dano pendente em 0x43C7AB |
| 0x0043FD80 / 0x0043FC60 | `Player_Reverse` / `Player_CanReverse` (ReversalDef) | NÃO portado (rótulo antigo "aplica o hit" estava ERRADO) |

### Item 1 — `hitflag`: em pé exige H, agachado exige L (0x441200)
```
00441243: lea eax,[esi-1]        ; esi = statetype (1 S, 2 C, 3 A, 4 L)
00441246: cmp eax,3 ; ja 441282  ; fora de 1..4 -> 0
0044124b: jmp [eax*4+0x441288]   ; tabela: 0x44125e, 0x44126a, 0x441252, 0x441276
0044125e: test dl,4    ; statetype 1 (em pe)    -> bit 4  (H)
0044126a: test dl,8    ; statetype 2 (agachado) -> bit 8  (L)
00441252: test dl,0x10 ; statetype 3 (ar)       -> bit 0x10 (A)
00441276: test dl,0x40 ; statetype 4 (deitado)  -> bit 0x40 (D)
```
Letras (`HitFlags_Parse`, 0x459FD0; começa com esi = 2): `H`=4, `L`=8, **`M`=0xC**, `A`=0x10, `F`=0x20, `D`=0x40, `P`=0x80, `+`=0x100, `-` limpa o bit 2.
Padrão quando `hitflag` é omitido: `or [ebp+0x11c], 0x3E` (0x44FEAA); `guardflag` omitido: 0 (0x44FE6F-0x44FE85). Logo `hitflag = H` não acerta agachado. Teste: `hitflag H/L/M`.
**Diferença em relação à documentação:** o bit 2 liga por padrão ("pode acertar quem está apanhando") e `-` o desliga; `+` liga 0x100 e exige alvo em movetype H sem guardar.

### Item 2 — flag F: alvo caindo exige F (0x441200)
```
00441235: cmp dword ptr [ecx+0x1148],0 ; je 441243     ; +0x1148 = gethit fall
0044123c/0044123e: test dl,0x20 ; je 441282            ; sem F -> retorna 0
```
`Player_CanHit` chama essa função com `[att+0x12A8]` depois do teste das caixas (0x43E440-0x43E44D). Teste: alvo com `gethit.fall` só é atingido com F.

### Item 3 — type None: sem dano, estado, pausa (0x43ECE0 / 0x43E480)
```
0043f195: cmp [ebp+0x124],0 ; je 43f3df      ; ground.type == None (alvo em pe/agachado)
0043efa8: cmp [ebp+0x13c],0 ; je 43f3df      ; air.type == None (alvo no ar)
0043f45a: cmp [esp+0x1c],0 ; je 43f473 ; cmp [ebp+0xbc],0 ; jne 43f473 ; xor eax,eax ; ret   ; "sem estado" e nao e ReversalDef -> retorna 0
0043e9f0: call 0x43ece0 ; mov ebx,eax ; test ebx,ebx ; jne 43ea08 ; cmp [esp+0x18],eax ; je 43eac9   ; retorno 0 -> pula a pausa do atacante (hitpause_next, 0xEF4)
0043f6e6: mov eax,[ebp+0x50] ; mov [esi+0x1110],eax   ; shake do alvo, DEPOIS do retorno de 0x43F46A
0043f876..0043f883: call 0x43f930 ; add [esi+0x1104],eax   ; dano, tambem depois
```
Efeito do None: não define estado, dano, shake do alvo, pausa do atacante, MoveHit nem MoveContact. Ainda são feitos antes do retorno: zerar `+0x1130/+0x1134` (xvel/yvel), copiar os tipos e a troca High/Low. O `hitonce` ainda desativa o HitDef (fim de `Player_Hit`). Teste: ground.type e air.type None.

### Item 4 — tempo de hit no chão com yvel != 0 (0x43ECE0)
```
0043f39f: fldz ; fcomp [esi+0x1134] ; ... jp 43ef97       ; yvel != 0 -> 0x43EF97
0043ef97: mov edx,[ebp+0x144] ; mov [esi+0x1114],edx       ; hittime = air.hittime (hd+0x144); NAO grava +0x1118 (slidetime)
0043f3b9: mov eax,[ebp+0x128] ; mov [esi+0x1114],eax       ; yvel == 0: hittime = ground.hittime
0043f3c5: mov ecx,[ebp+0x12c] ; mov [esi+0x1118],ecx       ;            slidetime = ground.slidetime
0043f3b2: cmp [esi+0x10f8],3 ... mov [esi+0x10f8],2        ; so no caminho yvel == 0: animtype Back (3) vira Hard (2)
0043f380..0043f399: se fall e yvel == 0 -> yvel = [0x4FDA00] = -0.001 (float)
```
Testes: yvel != 0 usa air.hittime e não mexe no slidetime; yvel == 0 usa ground; fall com yvel 0 vira -0.001.

### Item 5 — dano (0x43F930, 0x43ECE0 0x43F876, 0x43C480 0x43C7AB)
```
0043f930: fld [esi+0x1e4] ; fimul [esp+8] ; mov edi,[esi+0x1104] ; fadd qword [0x4fd830] ; call 0x4dc850 ; add eax,edi   ; d = trunc(defence*dano + 0.5) + pendente
0043f94e: cmp [esp+8],1 ; jl .. ; test eax,eax ; jne .. ; mov eax,1                                                          ; dano >= 1 e d == 0 -> 1
0043f95e: cmp [esp+0xc],0 ; jne 43f982                                                                                      ; kill != 0: sem teto
0043f965: mov ecx,[esi+0x1b8] ; sub ecx,edi ; mov edx,ecx ; sub edx,eax ; cmp edx,1 ; jge ..
0043f976: cmp [esi+0xf00],0 ; je ..  ; lea eax,[ecx-1]                                                                      ; kill == 0, vivo (+0xF00) e vida-pendente-d < 1 -> d = (vida-pendente)-1
0043f982: sub eax,edi ; ret                                                                                                 ; devolve d - pendente
0043f883: add [esi+0x1104],eax        ; HitDef_Apply ACUMULA o dano (nao mexe na vida)
0043c7ab: mov ecx,[ebp+0x1104] ; ... mov eax,[ebp+0x1b8] ; sub eax,ecx ; ... mov [ebp+0x1b8],eax ; clamp 0..[ebp+0x1bc] ; mov [ebp+0x1104],0   ; Player_Update aplica no tick, limita a [0, vida maxima] e zera
0044efa1: push 1 ... "kill"  /  0x44efbe: push 1 ... "guard.kill"                                                           ; padrao de kill e guard.kill = 1
```
Logo: por padrão o golpe PODE matar; com `kill = 0` e jogador vivo a vida fica em 1. O dano só chega à vida no tick seguinte (e também durante o hitpause, pois 0x43C7AB vem antes do teste de pausa em 0x43C988).
**Quirk do exe reproduzido:** com dano pendente e `kill = 0`, o teto vira `(vida - pendente) - 1` sem somar o pendente de volta, então devolve menos do que o esperado (teste: pendente 40, vida 100, dano 100 → 19).
O "mínimo 1" já está portado e só entra em ação com `defence` (`defenceMul = 100 / Const(data.defence)`, `Engine::init_player`).
Não portado: KO (`ko` = vida - pendente - dano <= 0 → `fall` = 1, `gethit_damage = vida`), chip-kill de guarda (flag global), o desvio do estado global 3 e a flag de depuração em 0x43C7C1-0x43C825.

### Item 6 — troca High↔Low por trás só para animtype Light/Medium/Hard (0x43ECE0)
```
0043f3e1: mov eax,[esi+0x1ec] ; test eax,eax ; jne .. ; mov eax,[esi+0x1e8]        ; facing do alvo (override se houver)
0043f3f3: cmp [ebp+0x120],-1 ; setne cl ; lea ecx,[ecx+ecx-1] ; imul ecx,[esp+0x28] ; cmp eax,ecx ; je 43f45a   ; esperado = (p2facing != -1 ? 1 : -1) * (-dir do atacante); igual -> sem troca
0043f40a: mov eax,[esi+0x10f8] ; test eax,eax ; jl 43f45a ; mov edx,2 ; cmp eax,edx ; jg 43f45a   ; so animtype 0..2
0043f41d..0043f454: troca 1<->2 em [esi+0x1100] (groundtype) e [esi+0x10fc] (airtype)
```
Com `p2facing` padrão a troca ocorre quando o alvo olha para o mesmo lado do atacante (golpe por trás). Teste: animtype 0–5 por trás; 3 só troca no chão com yvel 0 (vira 2 antes).

### Item 7 — rótulos corrigidos
`0x43FD80` = `Player_Reverse` e `0x43FC60` = `Player_CanReverse` (ReversalDef); a aplicação do hit é `Player_Hit` (0x43E480) + `HitDef_Apply` (0x43ECE0) e o teste de acerto é `Player_CanHit` (0x43E280). Os textos antigos em `docs/mugen_exe_achados.md` foram corrigidos.

## Vídeo (src/video): interface pronta, backends GLES/Vulkan pendentes
Interface `video::Video`, backends Null e Software com testes sintéticos (`tests/video_test.cpp`). O modelo (paleta, máscara, ordem do PalFX) vem dos shaders do original; as fórmulas de blend ainda precisam ser conferidas no exe (ver docs/video.md).
