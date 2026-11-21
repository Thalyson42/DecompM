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
