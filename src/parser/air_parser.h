#pragma once
#include "parser/ini_parser.h"
#include <string>
#include <vector>

// Parser de animacoes (.air), seguindo docs/air.html do MUGEN 1.1b1.
// Reimplementacao baseada na especificacao e nos arquivos de teste; nao comparada ainda com o mugen.exe.
struct AirBox { int x1, y1, x2, y2; };

enum AirBlend { AIR_BLEND_NONE = 0, AIR_BLEND_ADD = 1, AIR_BLEND_SUB = 2 };

enum AirInterp { AIR_INTERP_OFFSET = 1, AIR_INTERP_BLEND = 2, AIR_INTERP_SCALE = 4, AIR_INTERP_ANGLE = 8 };

struct AirFrame {
    int   group, image;
    int   x, y;                 // deslocamento
    int   time;                 // em ticks (60/s); -1 = infinito
    bool  flipH, flipV;
    int   blend;                // AirBlend
    int   alphaSrc, alphaDst;   // 0..256 (A = 256,256; A1 = 256,128; S = 256,256 com subtracao)
    float xscale, yscale;       // 1.0 por padrao
    float angle;                // graus
    int   interp;               // mascara AirInterp (linhas "Interpolate" antes do elemento)
    std::vector<AirBox> clsn1;  // caixas de ataque (frame-especificas ou default)
    std::vector<AirBox> clsn2;  // caixas de colisao
};

struct AirAction {
    int number;
    int loopStart;              // indice do elemento onde o loop recomeca (0 por padrao)
    std::vector<AirFrame> frames;
    int interpBeforeFirst;      // "Interpolate" antes do 1o elemento: do ultimo para o primeiro
    // Soma dos tempos; -1 se o ultimo elemento tem tempo -1 (sem loop).
    int looptime() const;
};

// Converte um IniFile em acoes. warnings recebe mensagens de linhas estranhas (pode ser nulo).
bool air_parse(const IniFile& ini, std::vector<AirAction>& out, std::vector<std::string>* warnings);
bool air_load(const char* path, std::vector<AirAction>& out, std::vector<std::string>* warnings);
