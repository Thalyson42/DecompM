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
