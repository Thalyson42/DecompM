#include "parser/cns_parser.h"
#include "parser/cns_controllers.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>

static bool starts_with_i(const std::string& s, const char* p) {
    size_t i = 0;
    for (; p[i]; ++i) if (i >= s.size() || std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)p[i])) return false;
    return true;
}
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
static void warn(std::vector<std::string>* w, int line, const std::string& m) {
    if (!w) return;
    char b[32]; std::snprintf(b, sizeof b, "linha %d: ", line);
    w->push_back(b + m);
}

std::string CnsStatedef::param(const std::string& key) const {
    for (size_t i = 0; i < params.size(); ++i) if (ini_iequals(params[i].key, key)) return params[i].value;
    return "";
}

bool cns_is_known_controller(const std::string& type) {
    for (unsigned i = 0; i < kCnsControllerCount; ++i) if (ini_iequals(type, kCnsControllers[i])) return true;
    return false;
}

bool cns_parse(const IniFile& ini, CnsFile& out, std::vector<std::string>* warnings) {
    out.states.clear(); out.others.clear();
    for (size_t si = 0; si < ini.sections.size(); ++si) {
        const IniSection& sec = ini.sections[si];
        if (starts_with_i(sec.name, "Statedef")) {
            CnsStatedef sd; sd.line = sec.line;
            sd.number = std::atoi(trim(sec.name.substr(8)).c_str());
            for (size_t i = 0; i < sec.entries.size(); ++i) {
                CnsParam p = { sec.entries[i].key, sec.entries[i].value, sec.entries[i].line };
                sd.params.push_back(p);
            }
            for (size_t i = 0; i < out.states.size(); ++i)
                if (out.states[i].number == sd.number) { warn(warnings, sd.line, "Statedef duplicado"); break; }
            out.states.push_back(sd);
        } else if (starts_with_i(sec.name, "State ") || starts_with_i(sec.name, "State\t")) {
            CnsController c; c.line = sec.line;
            std::string rest = trim(sec.name.substr(5));
            size_t comma = rest.find(',');
            c.stateNumber = std::atoi(rest.substr(0, comma).c_str());
            c.description = comma == std::string::npos ? "" : trim(rest.substr(comma + 1));
            for (size_t i = 0; i < sec.entries.size(); ++i) {
                const IniEntry& e = sec.entries[i];
                if (ini_iequals(e.key, "type")) c.type = e.value;
                else if (ini_iequals(e.key, "triggerall")) c.triggerAll.push_back(e.value);
                else if (starts_with_i(e.key, "trigger") && e.key.size() > 7 && std::isdigit((unsigned char)e.key[7])) {
                    int n = std::atoi(e.key.c_str() + 7);
                    if (n < 1 || n > 1000) { warn(warnings, e.line, "numero de trigger invalido: " + e.key); continue; }
                    if ((int)c.triggers.size() < n) c.triggers.resize(n);
                    c.triggers[n - 1].push_back(e.value);
                }
                else if (ini_iequals(e.key, "persistent")) c.persistent = e.value;
                else if (ini_iequals(e.key, "ignorehitpause")) c.ignoreHitPause = e.value;
                else { CnsParam p = { e.key, e.value, e.line }; c.params.push_back(p); }
            }
            if (out.states.empty()) { warn(warnings, c.line, "[State] antes de qualquer [Statedef]"); continue; }
            if (c.type.empty()) warn(warnings, c.line, "controller sem 'type'");
            else if (!cns_is_known_controller(c.type)) warn(warnings, c.line, "controller desconhecido: " + c.type);
            if (c.triggers.empty()) warn(warnings, c.line, "controller sem trigger1");
            else for (size_t k = 0; k < c.triggers.size(); ++k)
                if (c.triggers[k].empty()) { warn(warnings, c.line, "trigger numerado com lacuna (os seguintes sao ignorados)"); break; }
            if (c.stateNumber != out.states.back().number) warn(warnings, c.line, "numero do [State] difere do [Statedef] atual");
            out.states.back().controllers.push_back(c);
        } else {
            out.others.push_back(sec);
        }
    }
    return true;
}

bool cns_load(const char* path, CnsFile& out, std::vector<std::string>* warnings) {
    IniFile ini;
    if (!ini_load(path, ini)) return false;
    return cns_parse(ini, out, warnings);
}
