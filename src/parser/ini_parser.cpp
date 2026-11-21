#include "parser/ini_parser.h"
#include <cctype>
#include <cstdio>

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Remove ';' e o resto da linha, exceto dentro de aspas duplas.
static std::string strip_comment(const std::string& s) {
    bool q = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') q = !q;
        else if (s[i] == ';' && !q) return s.substr(0, i);
    }
    return s;
}

bool ini_parse(const std::string& text, IniFile& out) {
    out.sections.clear();
    IniSection cur; cur.name = ""; cur.line = 0;
    bool curUsed = false;
    size_t pos = 0;
    int lineNo = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) pos = 3; // BOM
    while (pos <= text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string raw = text.substr(pos, e - pos);
        pos = e + 1;
        ++lineNo;
        std::string line = trim(strip_comment(raw));   // trim tambem remove o '\r'
        if (line.empty()) continue;
        if (line[0] == '[') {
            size_t close = line.find(']');
            if (close == std::string::npos) close = line.size();
            if (curUsed) out.sections.push_back(cur);
            cur = IniSection(); cur.line = lineNo; curUsed = true;
            cur.name = trim(line.substr(1, close - 1));
            continue;
        }
        IniEntry en; en.line = lineNo;
        size_t eq = line.find('=');
        if (eq == std::string::npos) { en.value = line; }
        else { en.key = trim(line.substr(0, eq)); en.value = trim(line.substr(eq + 1)); }
        if (!curUsed) { cur.line = 0; curUsed = true; }
        cur.entries.push_back(en);
    }
    if (curUsed) out.sections.push_back(cur);
    return true;
}

bool ini_load(const char* path, IniFile& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::string text; char buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);
    return ini_parse(text, out);
}

bool ini_iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

const IniEntry* ini_find(const IniSection& s, const std::string& key) {
    for (size_t i = 0; i < s.entries.size(); ++i)
        if (ini_iequals(s.entries[i].key, key)) return &s.entries[i];
    return 0;
}

std::string ini_unquote(const std::string& v) {
    if (v.size() >= 2 && v[0] == '"' && v[v.size() - 1] == '"') return v.substr(1, v.size() - 2);
    return v;
}
