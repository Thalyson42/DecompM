#pragma once
#include "parser/ini_parser.h"
#include <string>
#include <vector>

// Parser estrutural de .cns / .cmd: [Statedef N] + blocos [State N, desc] (controllers).
// NAO avalia expressoes (triggers ficam como texto); isso vem depois, com um tokenizer.
// Baseado em docs/cns.html. Reimplementacao validada nos arquivos do 1.1b1; ainda nao comparada com o mugen.exe.
struct CnsParam { std::string key, value; int line; };

struct CnsController {
    int stateNumber;                       // numero do [State N, ...]
    std::string description;               // texto depois da virgula
    int line;
    std::string type;                      // valor de "type" (nome do controller)
    std::vector<std::string> triggerAll;   // linhas "triggerall"
    // triggers[k] = condicoes de "trigger{k+1}" (todas devem ser verdadeiras: AND). Entre grupos: OR.
    std::vector< std::vector<std::string> > triggers;
    std::string persistent;                // texto bruto; vazio = padrao (1)
    std::string ignoreHitPause;            // texto bruto; vazio = padrao (0)
    std::vector<CnsParam> params;          // demais parametros, na ordem do arquivo
};

struct CnsStatedef {
    int number;
    int line;
    std::vector<CnsParam> params;          // parametros do Statedef (type, movetype, physics, anim, ...)
    std::vector<CnsController> controllers;
    // Valor de um parametro do Statedef (sem diferenciar maiusculas) ou "" se ausente.
    std::string param(const std::string& key) const;
};

struct CnsFile {
    std::vector<CnsStatedef> states;
    std::vector<IniSection> others;        // secoes de constantes: [Data], [Size], [Command], [Remap]...
};

bool cns_parse(const IniFile& ini, CnsFile& out, std::vector<std::string>* warnings);
bool cns_load(const char* path, CnsFile& out, std::vector<std::string>* warnings);
bool cns_is_known_controller(const std::string& type);
