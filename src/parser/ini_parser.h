#pragma once
#include <string>
#include <vector>

// Parser generico dos arquivos de texto do MUGEN (.def, .cns, .cmd, .air, .cfg).
// Todos compartilham a mesma estrutura:  [Secao]  chave = valor  ; comentario
// Linhas sem '=' (ex.: frames do .air "0,0, 0,0, 5") ficam como entrada com chave vazia.
// Reimplementacao baseada no formato documentado (docs/*.html) e validada nos arquivos do 1.1b1;
// ainda NAO foi comparada com o parser do mugen.exe.
struct IniEntry {
    std::string key;     // como escrito no arquivo (compare sem diferenciar maiusculas)
    std::string value;   // sem comentario, sem espacos nas pontas
    int line;            // linha no arquivo (1-based)
};

struct IniSection {
    std::string name;    // texto entre [ ] sem espacos nas pontas; "" para linhas antes da 1a secao
    int line;
    std::vector<IniEntry> entries;
};

struct IniFile {
    std::vector<IniSection> sections;
};

bool ini_parse(const std::string& text, IniFile& out);
bool ini_load(const char* path, IniFile& out);

bool ini_iequals(const std::string& a, const std::string& b);
// Primeira entrada com essa chave (sem diferenciar maiusculas) ou nullptr.
const IniEntry* ini_find(const IniSection& s, const std::string& key);
// Remove aspas duplas externas, se houver.
std::string ini_unquote(const std::string& v);
