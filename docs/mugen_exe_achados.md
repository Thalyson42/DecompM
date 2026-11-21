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

## Movimento "embutido": `FUN_00434db0` (resolve a hipótese do estado 20)
O parado/andando/agachado/pulo NÃO está nos .cns/.cmd: está nesta função, que também atualiza o buffer de comandos.
- Ela calcula uma "intenção" de entrada (variável `param_1`): 1 = frente, 7, 10/11 = agachar, 0x28 = pular, 900...
- Entra no estado 20 se `ctrl != 0`, a intenção é 1 e o estado atual não é 20.
- No fim: se `stateno == 20` (andando) e a condição de andar não vale, volta a `stateno = 0`, `Time = 0`.
- Agachado (`statetype == 2`) com ctrl e sem a intenção de agachar: troca de estado (provavelmente 12, agachado → de pé).
- Chamadas literais para 10, 12, 40, 45 e 120 ficam em 0x4353B9–0x435557.
- AINDA NÃO entendido: como a intenção (`param_1`) é calculada a partir dos botões; o que são as intenções 7 e 900.

## Outras funções identificadas
- `FUN_0043c480`: atualização por tick do jogador (física/estado; grava estados 0, 52 e 5120).
- `FUN_00434030` / `FUN_00434100`: contam jogadores do mesmo time / de outro time (NumPartner / NumEnemy).
- Tabela de wrappers Lua em 0x004F2208 (ver `docs/trigger_opcodes.md`).
