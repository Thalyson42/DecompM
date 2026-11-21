# Tabela trigger → opcode (extraída do mugen.exe 1.1b1)

Origem: cadeia de `stricmp` no tokenizer de expressões (todos os casos saltam para `0x00454AA0`).
Gerada por `tools/analise/extrair_triggers.py`. **Resultado heurístico**: o par nome → opcode foi lido do
`mov eax, imm` que segue cada comparação; conferir no desassemblado antes de tratar como definitivo.

| Endereço da comparação | Trigger | Opcode |
|---|---|---|
| 0x00453D4C | p2life | 10 |
| 0x00453EEA | frontedgedist | 11 |
| 0x00453F24 | frontedgebodydist | 11 |
| 0x00453F07 | backedgedist | 12 |
| 0x00453F49 | backedgebodydist | 12 |
| 0x00453F6E | life | 15 |
| 0x00453F8B | lifemax | 16 |
| 0x00453FA8 | power | 17 |
| 0x00453FC5 | powermax | 18 |
| 0x00453FE2 | alive | 19 |
| 0x00453FFF | command | 20 |
| 0x00453CBB | camerapos | 21 |
| 0x00453CD8 | camerazoom | 22 |
| 0x0045401C | ailevel | 25 |
| 0x00454039 | hitcount | 30 |
| 0x00454056 | uniqhitcount | 31 |
| 0x00454073 | movehit | 32 |
| 0x00454090 | moveguarded | 33 |
| 0x004540AD | movecontact | 34 |
| 0x004540CA | movereversed | 35 |
| 0x004540E7 | inguarddist | 36 |
| 0x00454104 | stateno | 40 |
| 0x00454121 | prevstateno | 41 |
| 0x0045413E | p2stateno | 42 |
| 0x0045415B | statetype | 43 |
| 0x00454178 | p2statetype | 44 |
| 0x00454195 | movetype | 45 |
| 0x004541B2 | p2movetype | 46 |
| 0x004541CF | ctrl | 47 |
| 0x004541EC | hitdefattr | 48 |
| 0x0045427D | random | 51 |
| 0x0045429A | numproj | 60 |
| 0x004543AB | p1name | 90 |
| 0x004543C8 | name | 90 |
| 0x004543E5 | p2name | 91 |
| 0x00454402 | p3name | 92 |
| 0x0045441F | p4name | 93 |
| 0x0045443C | authorname | 94 |
| 0x00454459 | numpartner | 95 |
| 0x00454476 | numenemy | 96 |
| 0x00454493 | id | 97 |
| 0x004544B0 | facing | 99 |
| 0x004544CD | hitfall | 100 |
| 0x004544EA | hitshakeover | 101 |
| 0x00454507 | hitover | 102 |
| 0x00454524 | hitvel | 103 |
| 0x00454541 | hitpausetime | 104 |
| 0x0045455E | canrecover | 105 |
| 0x004545EF | palno | 107 |
| 0x00453D69 | winperfect | 108 |
| 0x00453DC8 | drawgame | 111 |
| 0x00453DE5 | matchover | 112 |
| 0x00453E02 | gametime | 113 |
| 0x00453E1F | roundno | 114 |
| 0x00453E3C | roundstate | 115 |
| 0x00453E59 | matchno | 116 |
| 0x00453E76 | roundsexisted | 117 |
| 0x00453E93 | teamside | 120 |
| 0x00453EB0 | teammode | 121 |
| 0x00453ECD | ishometeam | 122 |
| 0x0045460C | numexplod | 150 |
| 0x00454629 | numtarget | 160 |
| 0x00454646 | gethitvar | 180 |
| 0x00454663 | const | 181 |
| 0x004546D7 | stagevar | 182 |
| 0x004546F4 | tickspersecond | 190 |
| 0x00454711 | ishelper | 201 |
| 0x0045472E | numhelper | 202 |
| 0x0045474B | parentdist | 203 |
| 0x00454768 | rootdist | 204 |
| 0x00454785 | standby | 205 |
| 0x004547A2 | gamewidth | 210 |
| 0x004547BF | gameheight | 211 |
| 0x004547DC | screenwidth | 212 |
| 0x004547F9 | screenheight | 213 |
| 0x00454816 | backedge | 214 |
| 0x00454833 | frontedge | 215 |
| 0x00454850 | leftedge | 216 |
| 0x0045486D | rightedge | 217 |
| 0x0045488A | topedge | 218 |
| 0x004548A7 | bottomedge | 219 |
| 0x004548C4 | pi | 300 |
| 0x004548E1 | e | 301 |
| 0x004548FE | ifelse | 305 |
| 0x0045491B | log | 310 |
| 0x00454938 | sin | 320 |
| 0x00454955 | cos | 321 |
| 0x00454972 | tan | 322 |
| 0x0045498F | asin | 323 |
| 0x004549AC | acos | 324 |
| 0x004549C9 | atan | 325 |
| 0x004549E6 | exp | 326 |
| 0x00454A03 | ln | 327 |
| 0x00454A20 | abs | 328 |
| 0x00454A3A | floor | 329 |
| 0x00454A54 | ceil | 330 |
| 0x00454A6E | animelemtime | 331 |
| 0x00454598 | animexist | 332 |
| 0x004545B5 | selfanimexist | 333 |
| 0x004545D2 | playeridexist | 334 |
| 0x004542F1 | projhittime | 337 |
| 0x0045432F | projguardedtime | 338 |
| 0x0045436D | projcontacttime | 339 |
| 0x004542B7 | numprojid | 340 |
| 0x00454209 | var | 343 |
| 0x00454226 | fvar | 344 |
| 0x00454243 | sysvar | 345 |
| 0x00454260 | sysfvar | 346 |
| 0x004542D4 | projcanceltime | 347 |
| 0x0045457B | animelemno | 348 |
| 0x00454680 | const240p | 349 |
| 0x0045469D | const480p | 350 |
| 0x004546BA | const720p | 351 |
| 0x00454A88 | cond | 400 |

## Opcodes com mais de um nome (aliases ou pares a conferir)

- 11: frontedgedist, frontedgebodydist
- 12: backedgedist, backedgebodydist
- 90: p1name, name

## Outro achado: tabela de funções Lua em 0x004F2208

88 entradas de 8 bytes `{ponteiro para nome, ponteiro para função}`: nomes como `life`, `animtime`, `statetype`, `command`,
`vel`, `pos`, `changestate`, `velset`, `__tostring`. É a API de scripts Lua do 1.1 (um wrapper por trigger/controller),
não a tabela do CNS. Cada ponteiro de função é o ponto de entrada de um wrapper e pode ser nomeado no decomp.
