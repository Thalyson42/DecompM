# Motor (src/engine): funções conferidas no assembly

Registro da trilha do motor. Endereços são VAs do mugen.exe 1.1b1 (ImageBase 0x00400000). Tudo abaixo foi lido no assembly (capstone + pefile);
o que NÃO foi provado está marcado. Testes: `tests/engine_kfm_test.cpp` (seções 7, 8 e 9) e `tests/combat_kfm_test.cpp` (G2 e G3).

## Movimento embutido: `FUN_00434db0` (continuação)

Layout confirmado do jogador (doubles no exe; o porte usa float): pos.x +0x1F8, pos.y +0x200, vel.x +0x248, vel.y +0x250
(`fadd/fmul qword` em 0x43C75F, 0x43C784, 0x43CD05, 0x43CD7F).

### Pulo no ar (intenção 10 -> estado 45)
- `0x4351CA`: `statetype == 3 (A)` e o valor tirado da fila de teclas neste tick (`[esp+0x14]`) `== 1` -> intenção = 10. A fila é um anel de 30
  entradas em `jogador+0x314`, índice de leitura `+0x3C0`, de escrita `+0x3C4` (0x434DB9-0x434DF6). O exe tira UM valor por tick.
- `0x435370-0x4353C4` (tratamento da intenção 10): exige `ctrl != 0` (+0xEE4) e `+0x278 > 0`; depois `fld qword [+0x200]; fchs; fild dword [+0x110]; fcompp`
  e `test ah,5 / jp`: entra no estado 45 (`FUN_0043BF40`) só quando `[+0x110] < -posY`. Depois `dec [+0x278]`.
- `+0x278` (pulos no ar restantes): recebe `[+0x10C]` em `0x43C972-0x43C978` quando `stateno == 40` e `Time == 0` (a tabela de saltos em 0x43D558
  só aponta o estado 40 para esse caso). É zerado em 0x43B781 (início), 0x43F6D4 (HitDef_Apply: golpe recebido) e 0x43FAE3 (Player_Guard).
- **Não provado:** que `+0x10C` é `movement.airjump.num` e `+0x110` é `movement.airjump.height`. A identificação vem do comportamento (contador copiado no
  estado 40, limiar de altura contra posY) e do kfm.cns (`airjump.num = 1`, `airjump.height = 35`); o carregador que grava esses campos no jogador não foi
  localizado (o que achei em 0x450A3C/0x450A5E é de outro objeto). Se o porte divergir do exe num personagem com valores diferentes, comece por aqui.
- **Não portado:** o produtor da fila de teclas (0x4134C9, 0x433B90-0x433D6A: valores como 0x11-0x14 e máscaras de direção). `Player::inputEvent`
  é preenchido pelo chamador e consumido em `default_movement`.

### Flags do AssertSpecial (bytes `jogador+0x2AC+id`)
- Parser de nomes em `0x4592A4` (cadeia de stricmp): `nostandguard` = 0, `nocrouchguard` = 1, `noairguard` = 2, `nowalk` = 6 (`mov [edi], id`).
  Executor em `0x44A0FB`: `byte [jogador+0x2AC+id] = 1` (lê `flag`, `flag2`, `flag3`).
- **Ordem por frame** (`FUN_00413170`): `0x413821` chama `434db0` de todos os jogadores -> `0x41389E` zera os bytes 0x2AC..0x2B4 -> `0x4138FF` chama
  `Player_Update` (onde o AssertSpecial roda). Logo o flag marcado no tick N vale no `434db0` do tick N+1.
- `nostandguard/nocrouchguard/noairguard` (`0x435241`, `0x435253`, `0x435265`): o flag do statetype atual pula a checagem de guarda inteira
  (o flag `guarding` fica 0 e a intenção 0x28 não é calculada).
- `nowalk` (`0x435404`): com o byte 0x2B2 != 0 a intenção 1 não entra no estado 20 (mas também não volta ao 0: o desvio vai direto ao fim).
- Não mapeado ainda: `unguardable` (o parser grava em outro destino, `[esi] = 1` em 0x4597CF) e os demais nomes. Ficam em `Player::skipped`.

### Agachar zera a velocidade X
`0x43543A-0x435447` (intenção 0xB): `fldz; fstp qword [+0x248]` (vel.x = 0) quando `stateno != 100`, antes de entrar no estado 10. Mesma regra
já portada na guarda (0x435548).

## Pontos em aberto (vistos nesta leitura, ainda não mexidos)
- O exe chama `434db0` para todo jogador ativo (+0x1B0 != 0), SEM testar o hitpause (0x413810-0x413821). O porte só chama `default_movement` quando
  `pauseTime == 0`. Durante o hitpause o exe também recalcula a intenção (com `ctrl` normalmente 0 nesse caso). Conferir antes de mudar.
- O pulo (intenção 7) e a guarda dependem de globais de rodada (`+0x12754 == 3`, `+0x11F78`) que o porte não modela.
- Se os flags do AssertSpecial são zerados mesmo durante o hitpause: o laço de 0x41389E testa `+0x1B0` e um array por jogador (`[edi+ecx*4]`) cujo
  significado não foi lido; o porte zera sempre.

## Pipeline de hits: prioridade, unguardable e juggle (`FUN_0043D950`, `Player_CanHit` 0x43E280, `Player_Hit` 0x43E480, `FUN_00417400`)
Testes: `tests/combat_kfm_test.cpp` (seções P, U e J) e `tests/engine_kfm_test.cpp` (AssertSpecial e `priority`).
Convenção lida no assembly: `Player_CanHit(eax = alvo, esi = atacante)`; `Player_Hit(atacante, alvo)` (primeiro e segundo argumento na pilha);
hitdef do atacante em `jogador+0x1230`.

### Tabela completa do AssertSpecial (parser 0x4592A4-0x4598E5, executor 0x44A0FB)
Flags por jogador (byte `jogador+0x2AC+id`): nostandguard 0, nocrouchguard 1, noairguard 2, noautoturn 3, noshadow 4, nojugglecheck 5, nowalk 6,
unguardable 7, invisible (nome lido, id 8 ainda não portado). Flags globais (outro array): intro, roundnotover 1, noko 2, nokosnd 3, nokoslow 4,
nomusic 5, globalnoshadow 6, timerfreeze 7, nobardisplay 8, nobg 9, nofg 10. Isto resolve a dúvida da seção anterior: `unguardable` é o byte `+0x2B3`
lido em 0x43E50D e `nojugglecheck` é o byte `+0x2B1` lido em 0x43E415. Portados: ids 0-7 por jogador.

### Prioridade entre golpes simultâneos (0x43DB34-0x43DBDE)
Para cada par (A, B): `A acerta B` = `CanHit(A,B)` e (`!CanHit(B,A)` ou A vence). A vence se `prioA > prioB`; se `prioA < prioB` perde; se iguais,
vence quando `tipoA == 0 (Hit)` e `tipoB != 2 (Dodge)`. O resultado vai para `tabela_de_B[id_de_A]` (`jogador+0x122C`) e depois `Player_Hit` roda para cada
entrada ligada, que é zerada logo após o golpe (0x43DC30-0x43DCE4). Dodge/Dodge ou Dodge/Hit com prioridade igual: ninguém acerta; Miss perde para Hit.
Tipo do `priority = N, Tipo`: lido pela primeira letra minúscula (0x44F255-0x44F28C): h = 0, m = 1, d = 2; padrão 4 e Hit (0x44F1DA, 0x44F1EA).
- **Não portado:** a ordem de aplicação dos golpes. O exe usa `[jogo+0x11E98] & 1` para escolher se aplica primeiro o golpe de P2 em P1 ou o de P1 em
  P2 (e a iteração entre vários jogadores). O porte continua calculando os dois contra o estado do início da rodada.

### `unguardable` (0x43E50D)
O golpe só é guardado se `[atacante+0x2B3] == 0`. Portado em `apply_hit`.

### Juggle
- **Checagem** (`CanHit`, 0x43E415-0x43E43A): se `[atacante+0x2B1] == 0` (nojugglecheck desligado) e o atacante já tem registro do alvo e `+0x1264 != -1`:
  se `registro.pontos - [atacante+0x1264] < 0`, não acerta. Em seguida `[atacante+0x1424] = [atacante+0x1264]` (custo deste golpe). `+0x1424` é zerado a
  cada frame (0x43DA27).
- **Registro** (`Player_Hit`, 0x43E619-0x43E6A2): a lista de alvos do atacante (`+0x2C8`, tabela hash com chave = ponteiro do alvo; `0x4349B0` procura) ganha um
  registro no primeiro golpe, guardado ou não. Pontos iniciais = `[atacante+0x64]`.
- **Desconto** (0x43E931-0x43E95D): só se o golpe NÃO foi guardado e (`alvo+0x1148 (gethit.fall) != 0` ou `hitdef+0x78 & 1` (o golpe derruba)
  ou (`hitdef+0x148 != 0` e alvo no ar)); `registro.pontos -= [atacante+0x1424]`. O exe zera `hitdef+0x34` depois.
- **Remoção** (`FUN_00417400`, chamada em 0x413E15, depois do pipeline de hits 0x413CCC): por alvo, se o alvo está em movetype H e (estado != 5150 ou
  Time == 0) o registro fica; sem nenhum alvo em H o registro é removido (e `+0x151C = 0`). Portado em `end_frame_juggle`, chamada ao fim de `resolve_hits`.
- **NÃO PROVADO (o porte assume, e os testes dependem disso):**
  1. **CONFERIDO (issue #3):** `[atacante+0x64]` = `Const(data.airjuggle)` (15 no KFM). O carregador grava a chave `airjuggle` em `CharData+0x2D0` (0x437620-0x43762E; chave `0x4F6E6C`; a string `data.airjuggle` é 0x4F6890, lida também em 0x437412 para o Lua). Em 0x43A4F9-0x43A507 o jogador recebe uma cópia de `CharData+0x2BC` (0x58 dwords) em `jogador+0x50`, então `jogador+0x64` = `+0x14` do bloco = `airjuggle`. `Player_Hit` lê `[atacante+0x64]` em 0x43E636. Chave ausente em `[Data]`: o exe aborta a carga (0x437638 → 0x439903); o porte cai em 0. Teste: `tests/combat_kfm_test.cpp`, bloco J2 (airjuggle trocado por 7, golpe normal e guardado).
  2. O custo `+0x1264` vem do `juggle` do Statedef: gravado em 0x45C07D quando o valor != -1 (omitido mantém o anterior). Não achei onde ele é zerado; o porte só
     lê o `juggle` do Statedef ao entrar no estado e o mantém se omitido.
  3. O `air.juggle` do HitDef: o parser grava em `hitdef+0x34` (0x450485), mas a execução do controller HitDef restaura o valor anterior logo depois
     (`mov ebx,[ebp+0x1264]` em 0x449CB2 e `mov [ebp+0x1264],ebx` em 0x449CEE). Pela leitura, o `air.juggle` do HitDef é descartado nesta versão. NÃO portado; confirmar antes de usar.
  4. O termo `[hitdef+0x148]` do desconto e a condição `+0x12EC != 0` da remoção (0x417450-0x417460): significado não lido.
