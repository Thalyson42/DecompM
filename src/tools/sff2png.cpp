#include <cstdio>
#include <cstring>
int sff_check_header(const char* path);
int sff_extract_v2(const char* path, const char* outdir);
int main(int argc, char** argv) {
    if (argc == 2) return sff_check_header(argv[1]);
    if (argc == 3) return sff_extract_v2(argv[1], argv[2]) == 0 ? 0 : 1;
    std::puts("uso: sff2png arquivo.sff            (valida o cabecalho)\n"
              "     sff2png arquivo.sff pasta      (extrai sprites SFF v2 como PNG)");
    return 1;
}
