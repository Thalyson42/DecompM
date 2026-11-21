#include "parser/air_parser.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int AirAction::looptime() const {
    int t = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].time < 0) return -1;
        t += frames[i].time;
    }
    return t;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
static std::vector<std::string> split_commas(const std::string& s) {
    std::vector<std::string> v; size_t p = 0;
    for (;;) {
        size_t c = s.find(',', p);
        if (c == std::string::npos) { v.push_back(trim(s.substr(p))); break; }
        v.push_back(trim(s.substr(p, c - p))); p = c + 1;
    }
    return v;
}
static bool starts_with_i(const std::string& s, const char* p) {
    size_t n = std::strlen(p);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)p[i])) return false;
    return true;
}
static std::string upper(std::string s) { for (size_t i = 0; i < s.size(); ++i) s[i] = (char)std::toupper((unsigned char)s[i]); return s; }

static void warn(std::vector<std::string>* w, int line, const std::string& msg) {
    if (!w) return;
    char b[32]; std::snprintf(b, sizeof b, "linha %d: ", line);
    w->push_back(b + msg);
}

static void parse_blend(const std::string& raw, AirFrame& f) {
    std::string t = upper(raw);
    f.blend = AIR_BLEND_NONE; f.alphaSrc = 256; f.alphaDst = 0;
    if (t.empty()) return;
    if (t == "A")        { f.blend = AIR_BLEND_ADD; f.alphaSrc = 256; f.alphaDst = 256; }
    else if (t == "A1")  { f.blend = AIR_BLEND_ADD; f.alphaSrc = 256; f.alphaDst = 128; }
    else if (t == "S")   { f.blend = AIR_BLEND_SUB; f.alphaSrc = 256; f.alphaDst = 256; }
    else if (t.compare(0, 2, "AS") == 0) {
        size_t d = t.find('D');
        if (d != std::string::npos) {
            f.blend = AIR_BLEND_ADD;
            f.alphaSrc = std::atoi(t.substr(2, d - 2).c_str());
            f.alphaDst = std::atoi(t.substr(d + 1).c_str());
        }
    }
}

bool air_parse(const IniFile& ini, std::vector<AirAction>& out, std::vector<std::string>* warnings) {
    out.clear();
    for (size_t si = 0; si < ini.sections.size(); ++si) {
        const IniSection& sec = ini.sections[si];
        if (!starts_with_i(sec.name, "Begin Action")) continue;
        AirAction act;
        act.number = std::atoi(trim(sec.name.substr(12)).c_str());
        act.loopStart = 0; act.interpBeforeFirst = 0;

        std::vector<AirBox> def1, def2;     // caixas default (valem ate nova declaracao)
        std::vector<AirBox> cur1, cur2;     // caixas so do proximo elemento
        bool has1 = false, has2 = false;    // "Clsn1:" / "Clsn2:" declarado para o proximo elemento
        int  pendingInterp = 0;
        // alvo atual das linhas "ClsnX[i] = ...": 0 nenhum, 1/2 = tipo, e se e default
        int  target = 0; bool targetDefault = false; int expected = 0, got = 0;

        for (size_t ei = 0; ei < sec.entries.size(); ++ei) {
            const IniEntry& en = sec.entries[ei];
            if (!en.key.empty()) {
                // ClsnX[i] = x1,y1,x2,y2
                if (starts_with_i(en.key, "Clsn") && (en.key[4] == '1' || en.key[4] == '2') && en.key.find('[') != std::string::npos) {
                    std::vector<std::string> v = split_commas(en.value);
                    if (v.size() < 4) { warn(warnings, en.line, "caixa com menos de 4 valores"); continue; }
                    AirBox b = { std::atoi(v[0].c_str()), std::atoi(v[1].c_str()), std::atoi(v[2].c_str()), std::atoi(v[3].c_str()) };
                    int kt = en.key[4] - '0';
                    // O KFM tem blocos "Clsn1: 1" seguidos de linha "Clsn2[0] = ..." (provavel erro de digitacao).
                    // Suposicao: a caixa pertence ao bloco declarado. A CONFIRMAR no mugen.exe.
                    int t = target ? target : kt;
                    if (target && kt != target) warn(warnings, en.line, "caixa Clsn" + std::string(1, (char)('0' + kt)) + " dentro de bloco Clsn" + std::string(1, (char)('0' + target)) + " (assumindo o bloco declarado)");
                    std::vector<AirBox>& dst = targetDefault ? (t == 1 ? def1 : def2) : (t == 1 ? cur1 : cur2);
                    dst.push_back(b); ++got;
                } else warn(warnings, en.line, "chave inesperada: " + en.key);
                continue;
            }
            const std::string& v = en.value;
            if (starts_with_i(v, "Clsn")) {
                size_t colon = v.find(':');
                if (colon == std::string::npos || (v[4] != '1' && v[4] != '2')) { warn(warnings, en.line, "declaracao Clsn invalida"); continue; }
                int t = v[4] - '0';
                bool isDef = starts_with_i(v.substr(5), "Default");
                if (target && got != expected) warn(warnings, en.line, "quantidade de caixas diferente da declarada");
                expected = std::atoi(trim(v.substr(colon + 1)).c_str()); got = 0;
                target = t; targetDefault = isDef;
                if (isDef) { (t == 1 ? def1 : def2).clear(); }
                else { (t == 1 ? cur1 : cur2).clear(); (t == 1 ? has1 : has2) = true; }
                continue;
            }
            if (starts_with_i(v, "Loopstart")) { act.loopStart = (int)act.frames.size(); continue; }
            if (starts_with_i(v, "Interpolate")) {
                std::string k = upper(trim(v.substr(11)));
                if (k == "OFFSET") pendingInterp |= AIR_INTERP_OFFSET;
                else if (k == "BLEND") pendingInterp |= AIR_INTERP_BLEND;
                else if (k == "SCALE") pendingInterp |= AIR_INTERP_SCALE;
                else if (k == "ANGLE") pendingInterp |= AIR_INTERP_ANGLE;
                else warn(warnings, en.line, "Interpolate desconhecido: " + k);
                continue;
            }
            // elemento de animacao
            std::vector<std::string> p = split_commas(v);
            if (p.size() < 5) { warn(warnings, en.line, "elemento com menos de 5 campos: " + v); continue; }
            if (target && got != expected) warn(warnings, en.line, "quantidade de caixas diferente da declarada");
            target = 0;
            AirFrame f;
            f.group = std::atoi(p[0].c_str()); f.image = std::atoi(p[1].c_str());
            f.x = std::atoi(p[2].c_str());     f.y = std::atoi(p[3].c_str());
            f.time = std::atoi(p[4].c_str());
            std::string fl = p.size() > 5 ? upper(p[5]) : "";
            f.flipH = fl.find('H') != std::string::npos; f.flipV = fl.find('V') != std::string::npos;
            parse_blend(p.size() > 6 ? p[6] : "", f);
            f.xscale = (p.size() > 7 && !p[7].empty()) ? (float)std::atof(p[7].c_str()) : 1.0f;
            f.yscale = (p.size() > 8 && !p[8].empty()) ? (float)std::atof(p[8].c_str()) : 1.0f;
            f.angle  = (p.size() > 9 && !p[9].empty()) ? (float)std::atof(p[9].c_str()) : 0.0f;
            f.interp = pendingInterp;
            f.clsn1 = has1 ? cur1 : def1;
            f.clsn2 = has2 ? cur2 : def2;
            if (act.frames.empty()) { act.interpBeforeFirst = pendingInterp; }
            pendingInterp = 0; has1 = has2 = false; cur1.clear(); cur2.clear();
            act.frames.push_back(f);
        }
        out.push_back(act);
    }
    return true;
}

bool air_load(const char* path, std::vector<AirAction>& out, std::vector<std::string>* warnings) {
    IniFile ini;
    if (!ini_load(path, ini)) return false;
    return air_parse(ini, out, warnings);
}
