#pragma once
// Interface de video do DecompM. O motor (src/engine) nunca chama OpenGL/GLES/Vulkan diretamente: so esta interface.
// Backends: Null (grava as chamadas, para testes), Software (referencia em CPU) e, depois, GLES e Vulkan.
// Modelo copiado do que o original faz (data/indexed.frag e data/rgba.frag, SHADERS que acompanham o MUGEN 1.1b1):
//  - sprites de 8 bits indexados + paleta de 256 cores (o original usa uma textura 1D; em GLES vira uma 2D de 256x1);
//  - indice 0 transparente (mascara) e alfa da paleta; sem mascara o alfa vira 1;
//  - PalFX na ordem: saturacao (colorbal) -> inverter -> (cor + add) * mul;
//  - alfa do sprite multiplicado por src_alphamul.
// As formulas de blend (add/sub/alpha) vem da documentacao e AINDA PRECISAM ser conferidas no exe (glBlendFunc/softblend).
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace video {

typedef uint32_t TextureId;   // 0 = invalido
typedef uint32_t PaletteId;   // 0 = invalido

enum class Backend { Null, Software, GLES, Vulkan };
const char* backend_name(Backend b);

enum class Blend { Alpha, Add, Sub };

struct Color { uint8_t r, g, b, a; Color() : r(0), g(0), b(0), a(255) {} Color(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255) : r(R), g(G), b(B), a(A) {} };
struct Rect  { float x, y, w, h; Rect() : x(0), y(0), w(0), h(0) {} Rect(float X, float Y, float W, float H) : x(X), y(Y), w(W), h(H) {} };

// Efeito de paleta (PalFX). Valores em 0..1; o padrao nao altera a cor.
struct PalFx {
    float add[3], mul[3];
    bool  invertAll;
    float colorBal;                 // 1 = sem mudanca, 0 = cinza
    PalFx() : invertAll(false), colorBal(1.0f) { for (int i = 0; i < 3; ++i) { add[i] = 0.0f; mul[i] = 1.0f; } }
};

// Um sprite na tela. Coordenadas em unidades LOGICAS (por padrao 320x240; o backend ajusta a janela real).
struct SpriteDraw {
    TextureId texture;
    PaletteId palette;              // 0 para texturas RGBA
    float x, y;                     // onde fica o ponto de eixo do sprite
    float axisX, axisY;             // eixo dentro do sprite, em pixels da textura
    float scaleX, scaleY;
    float angle;                    // graus, anti-horario
    bool  flipH, flipV;
    bool  mask;                     // true: o indice 0 / alfa 0 e transparente
    Blend blend;
    int   alphaSrc, alphaDst;       // 0..256 (padrao 256 / 0)
    PalFx palfx;
    bool  hasClip;
    Rect  clip;                     // em coordenadas logicas
    SpriteDraw() : texture(0), palette(0), x(0), y(0), axisX(0), axisY(0), scaleX(1), scaleY(1), angle(0),
        flipH(false), flipV(false), mask(true), blend(Blend::Alpha), alphaSrc(256), alphaDst(0), hasClip(false) {}
};

struct VideoConfig {
    int   logicalWidth, logicalHeight;   // 320x240 por padrao (localcoord do MUGEN)
    int   windowWidth, windowHeight;     // tamanho real da superficie (0 = igual ao logico)
    void* nativeWindow;                  // PC: SDL_Window*; Android: ANativeWindow*; Software/Null: ignorado
    bool  vsync;
    VideoConfig() : logicalWidth(320), logicalHeight(240), windowWidth(0), windowHeight(0), nativeWindow(0), vsync(true) {}
};

class Video {
public:
    virtual ~Video() {}
    virtual Backend backend() const = 0;
    virtual bool init(const VideoConfig& cfg) = 0;
    virtual void shutdown() = 0;
    // A janela real mudou (rotacao no Android, redimensionar no PC). O backend recalcula viewport/letterbox.
    virtual void resize(int windowWidth, int windowHeight) = 0;

    // Recursos. 'pixels8' tem w*h bytes de indice; 'rgba' tem w*h*4 bytes; 'palette' tem count*4 bytes (RGBA), count <= 256.
    virtual TextureId create_texture_indexed(int w, int h, const uint8_t* pixels8) = 0;
    virtual TextureId create_texture_rgba(int w, int h, const uint8_t* rgba) = 0;
    virtual void      destroy_texture(TextureId t) = 0;
    virtual PaletteId create_palette(const uint8_t* rgba, int count) = 0;
    virtual void      update_palette(PaletteId p, const uint8_t* rgba, int count) = 0;
    virtual void      destroy_palette(PaletteId p) = 0;

    // Um quadro: begin -> (draw_sprite | fill_rect)* -> end. Fora desse par as chamadas de desenho sao ignoradas.
    virtual void begin_frame(const Color& clear) = 0;
    virtual void draw_sprite(const SpriteDraw& s) = 0;
    virtual void fill_rect(const Rect& r, const Color& c, Blend blend, int alphaSrc, int alphaDst) = 0;
    virtual void end_frame() = 0;

    // Copia o ultimo quadro (tamanho logico, RGBA8, origem no canto superior esquerdo). Para testes e capturas de tela.
    virtual bool read_pixels(std::vector<uint8_t>& rgba, int& w, int& h) = 0;
    virtual const std::string& last_error() const = 0;
};

// Cria um backend. Retorna nullptr (e preenche *error) se o backend nao foi compilado ou nao esta disponivel.
std::unique_ptr<Video> create(Backend b, std::string* error);

} // namespace video
