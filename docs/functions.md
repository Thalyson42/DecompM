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
