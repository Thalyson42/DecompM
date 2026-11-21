# Ghidra: importar o mugen.exe

1. Novo projeto, importe `mugen.exe` (PE, x86 32-bit little endian, compilador `windows`).
2. Rode a análise automática com *Decompiler Parameter ID*, *Windows x86 PE Exception Handling* e *Function ID* (se tiver as assinaturas de MSVC 9 / VS2008).
3. Fatos já conhecidos:
   - EntryPoint RVA 0xDC80A (VA 0x004DC80A)
   - .text 0x1000 (903 680 bytes), .rdata 0xDE000, .data 0x100000, .rsrc 0x106000
   - Imports: SDL, SDL_mixer, SDL_ttf, SDL_gfx, zlib1, pthreadVC2, libresample, OPENGL32, GLU32, MSVCR90
   - Lua 5.1.4 está linkado estaticamente (procure a string "$Lua: Lua 5.1.4")
4. Marque as funções do Lua como `biblioteca` em `docs/functions.md` e foque no resto.
5. Dicas: strings como "Error initializing lua state", "[Statedef", "[State " e nomes de controllers (ChangeState, VarSet...) levam às tabelas de dispatch.

## Sem Ghidra
Boa parte da análise inicial pode ser feita por script (capstone + pefile). Veja `tools/analise/` e `docs/trigger_opcodes.md`.
