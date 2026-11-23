// Testes da interface de video com sprites SINTETICOS (nenhum asset do MUGEN). Uso: video_test
#include "video/video.h"
#include "video/video_null.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace video;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; std::printf("FALHOU: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Px { int r, g, b; };
static Px at(Video& v, int x, int y) {
    std::vector<uint8_t> f; int w, h; v.read_pixels(f, w, h);
    const uint8_t* p = &f[((size_t)y * w + x) * 4]; Px r = { p[0], p[1], p[2] }; return r;
}
static bool eq(Px p, int r, int g, int b, int tol = 0) { return std::abs(p.r - r) <= tol && std::abs(p.g - g) <= tol && std::abs(p.b - b) <= tol; }

int main() {
    // --- fabrica ---
    std::string err;
    CHECK(create(Backend::GLES, &err) == nullptr && !err.empty(), "GLES ainda nao existe e deve explicar por que");
    CHECK(create(Backend::Vulkan, &err) == nullptr, "Vulkan ainda nao existe");

    // --- backend Null: grava a sequencia de chamadas ---
    {
        std::unique_ptr<Video> v = create(Backend::Null, &err);
        VideoConfig cfg; v->init(cfg);
        uint8_t px[4] = { 1, 2, 3, 4 }; uint8_t pal[8] = { 0, 0, 0, 0, 255, 0, 0, 255 };
        TextureId t = v->create_texture_indexed(2, 2, px); PaletteId p = v->create_palette(pal, 2);
        SpriteDraw s; s.texture = t; s.palette = p;
        v->draw_sprite(s);                                   // fora do quadro: ignorado
        v->begin_frame(Color(0, 0, 0)); v->draw_sprite(s); v->fill_rect(Rect(0, 0, 4, 4), Color(), Blend::Alpha, 256, 0); v->end_frame();
        const std::vector<Call>& c = static_cast<NullVideo*>(v.get())->calls();
        CHECK(c.size() == 4 && c[0].kind == Call::Begin && c[1].kind == Call::Sprite && c[2].kind == Call::Rect && c[3].kind == Call::End,
              "sequencia gravada: begin, sprite, rect, end (veio %zu chamadas)", c.size());
    }

    std::unique_ptr<Video> v = create(Backend::Software, &err);
    VideoConfig cfg; cfg.logicalWidth = 64; cfg.logicalHeight = 48;
    CHECK(v && v->init(cfg), "o backend de software deve iniciar");
    // paleta: 0 transparente, 1 vermelho, 2 verde, 3 azul, 4 cinza 100
    uint8_t pal[5 * 4] = { 0,0,0,0,  255,0,0,255,  0,255,0,255,  0,0,255,255,  100,100,100,255 };
    PaletteId P = v->create_palette(pal, 5);
    uint8_t spr[16] = { 1,1,2,2,  1,1,2,2,  0,0,3,3,  0,0,3,3 };     // 4x4: vermelho, verde, [transparente], azul
    TextureId T = v->create_texture_indexed(4, 4, spr);
    CHECK(T != 0 && P != 0, "recursos criados");

    // 1) desenho simples e mascara (indice 0 transparente mostra o fundo)
    {
        v->begin_frame(Color(10, 20, 30));
        SpriteDraw s; s.texture = T; s.palette = P; s.x = 10; s.y = 10;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 10, 10), 255, 0, 0), "canto superior esquerdo vermelho");
        CHECK(eq(at(*v, 13, 10), 0, 255, 0), "direita verde");
        CHECK(eq(at(*v, 10, 13), 10, 20, 30), "indice 0 transparente deixa ver o fundo");
        CHECK(eq(at(*v, 13, 13), 0, 0, 255), "canto inferior direito azul");
        CHECK(eq(at(*v, 14, 10), 10, 20, 30) && eq(at(*v, 9, 10), 10, 20, 30), "fora do sprite continua o fundo");
    }
    // 2) sem mascara: o indice 0 vira opaco (a cor da paleta, preta)
    {
        v->begin_frame(Color(10, 20, 30));
        SpriteDraw s; s.texture = T; s.palette = P; s.x = 10; s.y = 10; s.mask = false;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 10, 13), 0, 0, 0), "sem mascara o indice 0 desenha a cor da paleta");
    }
    // 3) eixo e flip horizontal: com flipH o sprite se estende para a ESQUERDA do ponto de eixo
    {
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = T; s.palette = P; s.x = 20; s.y = 10; s.flipH = true;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 19, 10), 255, 0, 0), "flipH: a coluna vermelha fica junto do eixo, a esquerda dele");
        CHECK(eq(at(*v, 16, 10), 0, 255, 0), "flipH: o verde fica mais a esquerda");
        CHECK(eq(at(*v, 20, 10), 0, 0, 0), "flipH: nada a direita do eixo");
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw a; a.texture = T; a.palette = P; a.x = 20; a.y = 20; a.axisX = 2; a.axisY = 2;   // eixo no meio
        v->draw_sprite(a); v->end_frame();
        CHECK(eq(at(*v, 18, 18), 255, 0, 0) && eq(at(*v, 21, 18), 0, 255, 0), "o eixo (2,2) coloca o sprite em volta do ponto");
    }
    // 4) escala 2: cada texel vira 2x2 pixels
    {
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = T; s.palette = P; s.x = 10; s.y = 10; s.scaleX = 2; s.scaleY = 2;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 11, 11), 255, 0, 0) && eq(at(*v, 12, 10), 255, 0, 0) && eq(at(*v, 14, 10), 0, 255, 0), "escala 2 dobra o tamanho");
        CHECK(eq(at(*v, 17, 17), 0, 0, 255), "o canto azul vai para (10+7, 10+7)");
    }
    // 5) rotacao de 90 graus anti-horario: uma tira 4x1 passa a subir a partir do ponto
    {
        uint8_t strip[4] = { 1, 2, 3, 4 };
        TextureId S = v->create_texture_indexed(4, 1, strip);
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = S; s.palette = P; s.x = 10; s.y = 10; s.angle = 90;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 10, 9), 255, 0, 0), "90 graus: o primeiro texel fica logo acima do ponto");
        CHECK(eq(at(*v, 10, 6), 100, 100, 100) && eq(at(*v, 10, 7), 0, 0, 255), "90 graus: o quarto texel (cinza) fica na linha 6, o terceiro (azul) na 7");
    }
    // 6) PalFX: inverter, add e colorbal
    {
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = T; s.palette = P; s.x = 10; s.y = 10; s.palfx.invertAll = true;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 10, 10), 0, 255, 255), "invertall: vermelho vira ciano");
        uint8_t g[1] = { 4 }; TextureId G = v->create_texture_indexed(1, 1, g);
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw a; a.texture = G; a.palette = P; a.x = 5; a.y = 5; a.palfx.add[0] = 0.2f; a.palfx.add[1] = 0.2f; a.palfx.add[2] = 0.2f;
        v->draw_sprite(a);
        SpriteDraw m; m.texture = G; m.palette = P; m.x = 7; m.y = 5; m.palfx.mul[0] = 2.0f; m.palfx.mul[1] = 1.0f; m.palfx.mul[2] = 0.5f;
        v->draw_sprite(m);
        SpriteDraw c; c.texture = T; c.palette = P; c.x = 20; c.y = 5; c.palfx.colorBal = 0.0f;
        v->draw_sprite(c); v->end_frame();
        CHECK(eq(at(*v, 5, 5), 151, 151, 151, 1), "add 0.2: (100/255 + 0.2) * 255 = 151");
        CHECK(eq(at(*v, 7, 5), 200, 100, 50, 1), "mul (2, 1, 0.5) sobre cinza 100");
        Px red0 = at(*v, 20, 5);   // vermelho sem saturacao: cinza = 0.3125 * 255 = 80
        CHECK(eq(red0, 80, 80, 80, 1), "colorbal 0 deixa o vermelho cinza 80 (deu %d,%d,%d)", red0.r, red0.g, red0.b);
    }
    // 7) blend Alpha / Add / Sub e fill_rect
    {
        uint8_t one[1] = { 1 }; TextureId R = v->create_texture_indexed(1, 1, one);
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = R; s.palette = P; s.x = 3; s.y = 3; s.alphaSrc = 128;
        v->draw_sprite(s);
        v->fill_rect(Rect(10, 10, 2, 2), Color(100, 100, 100), Blend::Alpha, 256, 0);
        SpriteDraw add; add.texture = R; add.palette = P; add.x = 10; add.y = 10; add.blend = Blend::Add; add.alphaSrc = 256; add.alphaDst = 256;
        v->draw_sprite(add);
        v->fill_rect(Rect(20, 20, 2, 2), Color(100, 100, 100), Blend::Alpha, 256, 0);
        uint8_t dk[8] = { 60, 0, 0, 255 }; PaletteId P2 = v->create_palette(dk, 1);
        uint8_t zero[1] = { 0 }; TextureId Z = v->create_texture_indexed(1, 1, zero);
        SpriteDraw sub; sub.texture = Z; sub.palette = P2; sub.x = 20; sub.y = 20; sub.blend = Blend::Sub; sub.alphaSrc = 256; sub.alphaDst = 256; sub.mask = false;
        v->draw_sprite(sub); v->end_frame();
        CHECK(eq(at(*v, 3, 3), 128, 0, 0, 1), "alfa 128 sobre preto: vermelho pela metade");
        CHECK(eq(at(*v, 10, 10), 255, 100, 100), "add: cinza 100 + vermelho 255 = (255, 100, 100) com clamp");
        CHECK(eq(at(*v, 20, 20), 40, 100, 100), "sub: cinza 100 - (60, 0, 0) = (40, 100, 100)");
    }
    // 8) clip
    {
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = T; s.palette = P; s.x = 10; s.y = 10; s.hasClip = true; s.clip = Rect(10, 10, 2, 4);
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 11, 10), 255, 0, 0) && eq(at(*v, 12, 10), 0, 0, 0), "o clip corta o sprite em x = 12");
    }
    // 9) textura RGBA com alfa por pixel
    {
        uint8_t px[8] = { 0, 255, 0, 255,   0, 255, 0, 0 };
        TextureId C = v->create_texture_rgba(2, 1, px);
        v->begin_frame(Color(0, 0, 0));
        SpriteDraw s; s.texture = C; s.x = 5; s.y = 5;
        v->draw_sprite(s); v->end_frame();
        CHECK(eq(at(*v, 5, 5), 0, 255, 0) && eq(at(*v, 6, 5), 0, 0, 0), "RGBA: pixel opaco desenha, alfa 0 e descartado");
    }
    std::printf(fails ? "%d falha(s)\n" : "tudo certo\n", fails);
    return fails ? 1 : 0;
}
