# DecompM

Projeto de **recuperação** (decompilação) do MUGEN 1.1 beta 1 (Elecbyte, 2013) em C++.
O autor original abandonou o projeto; hoje só a comunidade usa a ferramenta.

- Objetivo: recuperar o comportamento do motor para preservação e manutenção.
- Não é para distribuição do executável nem dos assets originais.
- Alvo: `mugen.exe` PE32 x86, compilado com MSVC 9.0 (VS2008), sem símbolos.

## Escopo
| Módulo | Pasta | Status |
|---|---|---|
| Utilitários (sff2png, sprmake2, sndmaker) | `src/tools` | não iniciado |
| Formato SFF | `src/sff` | v1 e v2 mapeados; RLE8 e LZ5 decodificados (extração v2 para PNG funcionando); falta v1, RLE5, raw e PNG |
| Parsers DEF/CNS/AIR/CMD + expressões | `src/parser` | INI, AIR, CNS/CMD e expressões (parser + avaliador) prontos; falta ligar ao motor (triggers reais) |
| Estados, controllers, colisão | `src/engine` | máquina de estados + triggers e controllers básicos prontos; faltam física, colisão e hits |
| Renderização OpenGL | `src/render` | não iniciado |
| Chamadas à API Lua | `src/lua_bind` | não iniciado |

## O que NÃO é decompilado
Lua 5.1.4 (embutido no exe), zlib, SDL 1.2, SDL_mixer, SDL_ttf, SDL_gfx, pthreads e libresample.
Use o código-fonte original em `third_party/`. Veja `third_party/LEIA-ME.md`.

## Como trabalhar
1. Coloque o seu `mugen.exe` em `original/` (pasta ignorada pelo git).
2. Siga `ghidra/notas.md` para importar e analisar.
3. A cada função recuperada, registre-a em `docs/functions.md`.
4. Valide comparando a saída com a do exe original (arquivos gerados, logs, comportamento).

## Build
```
cmake -S . -B build
cmake --build build
```

## Aviso legal
O MUGEN é freeware de código fechado. Este repositório deve conter apenas código próprio.
Não inclua o `mugen.exe`, DLLs, personagens, stages, SFF/SND/AIR/CNS originais.
