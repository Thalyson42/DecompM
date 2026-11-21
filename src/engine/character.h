#pragma once
#include "parser/air_parser.h"
#include "parser/cns_parser.h"
#include "parser/ini_parser.h"
#include <map>
#include <set>
#include <string>
#include <vector>

// Dados carregados de um personagem: estados (common + proprios + cmd), constantes, animacoes e comandos.
// Reimplementacao baseada em docs/cns.html; a ordem de precedencia entre arquivos de estado ainda
// precisa ser conferida no mugen.exe.
struct Character {
    std::map<int, CnsStatedef> states;       // estados positivos: o ultimo arquivo carregado vence
    std::map<std::string, IniSection> consts;// [Data], [Size], [Velocity], [Movement]... (nome em minusculas)
    std::map<int, AirAction> anims;
    std::set<std::string> commandNames;      // nomes (minusculas) definidos em [Command] do .cmd
    std::vector<std::string> warnings;

    // defPath: caminho do .def do personagem. dataDir: pasta "data" do MUGEN (onde esta o common1.cns).
    bool load(const std::string& defPath, const std::string& dataDir, std::string* err);
    // Const(velocity.walk.fwd.x) -> true e preenche out. Aceita "secao.chave" e sufixo .x/.y.
    bool constant(const std::string& name, float& out) const;
    const CnsStatedef* state(int n) const;
};
