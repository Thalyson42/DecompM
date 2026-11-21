#include <cstdio>
int sff_check_header(const char* path);
int main(int argc, char** argv) {
    if (argc > 1) return sff_check_header(argv[1]);
    std::puts("uso: sff2png arquivo.sff (por enquanto só valida o cabeçalho)");
    return 1;
}
