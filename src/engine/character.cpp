#include "engine/character.h"
#include <cctype>
#include <cstdlib>

static std::string lower(std::string s) { for (size_t i = 0; i < s.size(); ++i) s[i] = (char)std::tolower((unsigned char)s[i]); return s; }
static std::string dir_of(const std::string& p) { size_t k = p.find_last_of("/\\"); return k == std::string::npos ? "" : p.substr(0, k + 1); }
static std::string unq(std::string v) { return ini_unquote(v); }

static void merge_states(Character& ch, const CnsFile& f) {
    for (size_t i = 0; i < f.states.size(); ++i) {
        const CnsStatedef& s = f.states[i];
        std::map<int, CnsStatedef>::iterator it = ch.states.find(s.number);
        if (it != ch.states.end() && s.number < 0) {
            // estados especiais (-3,-2,-1): concatena os controllers (suposicao, a confirmar no exe)
            for (size_t k = 0; k < s.controllers.size(); ++k) it->second.controllers.push_back(s.controllers[k]);
        } else {
            ch.states[s.number] = s;
        }
    }
}

bool Character::load(const std::string& defPath, const std::string& dataDir, std::string* err) {
    IniFile def;
    if (!ini_load(defPath.c_str(), def)) { if (err) *err = "nao abriu " + defPath; return false; }
    std::string base = dir_of(defPath);
    std::string cmd, cns, st, stcommon, anim;
    for (size_t i = 0; i < def.sections.size(); ++i) {
        if (!ini_iequals(def.sections[i].name, "Files")) continue;
        const IniSection& s = def.sections[i];
        if (const IniEntry* e = ini_find(s, "cmd"))      cmd = unq(e->value);
        if (const IniEntry* e = ini_find(s, "cns"))      cns = unq(e->value);
        if (const IniEntry* e = ini_find(s, "st"))       st = unq(e->value);
        if (const IniEntry* e = ini_find(s, "stcommon")) stcommon = unq(e->value);
        if (const IniEntry* e = ini_find(s, "anim"))     anim = unq(e->value);
    }
    // 1) estados comuns (procura na pasta do personagem e depois em data/)
    if (!stcommon.empty()) {
        CnsFile f; std::vector<std::string> w;
        if (cns_load((base + stcommon).c_str(), f, &w) || cns_load((dataDir + stcommon).c_str(), f, &w)) merge_states(*this, f);
        else warnings.push_back("stcommon nao encontrado: " + stcommon);
    }
    // 2) estados proprios: cns e st
    std::vector<std::string> files;
    if (!st.empty())  files.push_back(st);
    if (!cns.empty() && cns != st) files.push_back(cns);
    for (size_t i = 0; i < files.size(); ++i) {
        CnsFile f; std::vector<std::string> w;
        if (!cns_load((base + files[i]).c_str(), f, &w)) { warnings.push_back("nao abriu " + files[i]); continue; }
        merge_states(*this, f);
        for (size_t k = 0; k < f.others.size(); ++k) {
            std::string nm = lower(f.others[k].name);
            if (consts.find(nm) == consts.end()) consts[nm] = f.others[k];
        }
    }
    // 3) cmd: estado -1 e lista de comandos
    if (!cmd.empty()) {
        CnsFile f; std::vector<std::string> w;
        if (cns_load((base + cmd).c_str(), f, &w)) {
            merge_states(*this, f);
            for (size_t k = 0; k < f.others.size(); ++k)
                if (ini_iequals(f.others[k].name, "Command"))
                    if (const IniEntry* e = ini_find(f.others[k], "name")) commandNames.insert(lower(unq(e->value)));
        } else warnings.push_back("nao abriu " + cmd);
    }
    // 4) animacoes
    if (!anim.empty()) {
        std::vector<AirAction> acts; std::vector<std::string> w;
        if (air_load((base + anim).c_str(), acts, &w)) for (size_t i = 0; i < acts.size(); ++i) anims[acts[i].number] = acts[i];
        else warnings.push_back("nao abriu " + anim);
    }
    return true;
}

const CnsStatedef* Character::state(int n) const {
    std::map<int, CnsStatedef>::const_iterator it = states.find(n);
    return it == states.end() ? 0 : &it->second;
}

bool Character::constant(const std::string& rawName, float& out) const {
    std::string name = lower(rawName);
    size_t dot = name.find('.');
    if (dot == std::string::npos) return false;
    std::map<std::string, IniSection>::const_iterator sec = consts.find(name.substr(0, dot));
    if (sec == consts.end()) return false;
    std::string rest = name.substr(dot + 1);
    int comp = 0;
    const IniEntry* e = ini_find(sec->second, rest);
    if (!e) {
        size_t d2 = rest.find_last_of('.');
        if (d2 == std::string::npos) return false;
        std::string suffix = rest.substr(d2 + 1);
        if (suffix != "x" && suffix != "y") return false;
        comp = suffix == "y" ? 1 : 0;
        e = ini_find(sec->second, rest.substr(0, d2));
        if (!e) return false;
    }
    // valor: "a" ou "a, b"
    std::string v = e->value; int idx = 0; size_t pos = 0;
    for (;;) {
        size_t c = v.find(',', pos);
        std::string part = v.substr(pos, c == std::string::npos ? std::string::npos : c - pos);
        if (idx == comp) { out = (float)std::atof(part.c_str()); return true; }
        if (c == std::string::npos) return false;
        pos = c + 1; ++idx;
    }
}
