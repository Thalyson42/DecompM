#include "video/video.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace video {

namespace {
struct Tex { int w, h; bool indexed; std::vector<uint8_t> data; };
inline float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
}

// Backend de referencia em CPU. Desenha num quadro RGBA8 do tamanho LOGICO (sem escala para a janela).
// Mesmo modelo do shader do original: paleta, mascara, PalFX (colorbal -> inverter -> (cor + add) * mul), alfa do sprite.
class SoftVideo : public Video {
public:
    SoftVideo() : w_(0), h_(0), nextTex_(1), nextPal_(1), inFrame_(false) {}
    Backend backend() const { return Backend::Software; }
    bool init(const VideoConfig& cfg) {
        if (cfg.logicalWidth <= 0 || cfg.logicalHeight <= 0) { err_ = "tamanho logico invalido"; return false; }
        w_ = cfg.logicalWidth; h_ = cfg.logicalHeight;
        fb_.assign((size_t)w_ * h_ * 4, 0);
        return true;
    }
    void shutdown() { tex_.clear(); pal_.clear(); fb_.clear(); }
    void resize(int, int) {}   // o quadro e sempre do tamanho logico; quem apresenta na janela faz a escala

    TextureId create_texture_indexed(int w, int h, const uint8_t* px) {
        if (w <= 0 || h <= 0 || !px) { err_ = "textura indexada invalida"; return 0; }
        Tex t; t.w = w; t.h = h; t.indexed = true; t.data.assign(px, px + (size_t)w * h);
        tex_[nextTex_] = t; return nextTex_++;
    }
    TextureId create_texture_rgba(int w, int h, const uint8_t* px) {
        if (w <= 0 || h <= 0 || !px) { err_ = "textura RGBA invalida"; return 0; }
        Tex t; t.w = w; t.h = h; t.indexed = false; t.data.assign(px, px + (size_t)w * h * 4);
        tex_[nextTex_] = t; return nextTex_++;
    }
    void destroy_texture(TextureId t) { tex_.erase(t); }
    PaletteId create_palette(const uint8_t* rgba, int count) {
        if (!rgba || count <= 0 || count > 256) { err_ = "paleta invalida"; return 0; }
        std::vector<uint8_t> p(256 * 4, 0);
        std::copy(rgba, rgba + count * 4, p.begin());
        pal_[nextPal_] = p; return nextPal_++;
    }
    void update_palette(PaletteId id, const uint8_t* rgba, int count) {
        std::map<PaletteId, std::vector<uint8_t> >::iterator it = pal_.find(id);
        if (it == pal_.end() || !rgba || count <= 0 || count > 256) return;
        std::copy(rgba, rgba + count * 4, it->second.begin());
    }
    void destroy_palette(PaletteId id) { pal_.erase(id); }

    void begin_frame(const Color& c) {
        inFrame_ = true;
        for (size_t i = 0; i < fb_.size(); i += 4) { fb_[i] = c.r; fb_[i + 1] = c.g; fb_[i + 2] = c.b; fb_[i + 3] = 255; }
    }

    void draw_sprite(const SpriteDraw& s) {
        if (!inFrame_) return;
        std::map<TextureId, Tex>::iterator ti = tex_.find(s.texture);
        if (ti == tex_.end()) return;
        const Tex& t = ti->second;
        const std::vector<uint8_t>* pal = 0;
        if (t.indexed) {
            std::map<PaletteId, std::vector<uint8_t> >::iterator pi = pal_.find(s.palette);
            if (pi == pal_.end()) return;
            pal = &pi->second;
        }
        if (s.scaleX == 0.0f || s.scaleY == 0.0f) return;
        // caixa de destino: cantos do sprite transformados
        const float rad = s.angle * 3.14159265358979f / 180.0f, cs = std::cos(rad), sn = std::sin(rad);
        float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
        for (int k = 0; k < 4; ++k) {
            float lx = ((k & 1) ? (float)t.w : 0.0f) - s.axisX, ly = ((k & 2) ? (float)t.h : 0.0f) - s.axisY;
            if (s.flipH) lx = -lx;
            if (s.flipV) ly = -ly;
            lx *= s.scaleX; ly *= s.scaleY;
            float dx = s.x + lx * cs + ly * sn, dy = s.y - lx * sn + ly * cs;   // y cresce para baixo; angulo anti-horario
            minx = std::min(minx, dx); maxx = std::max(maxx, dx); miny = std::min(miny, dy); maxy = std::max(maxy, dy);
        }
        int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(w_ - 1, (int)std::ceil(maxx));
        int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(h_ - 1, (int)std::ceil(maxy));
        if (s.hasClip) {
            x0 = std::max(x0, (int)std::floor(s.clip.x)); x1 = std::min(x1, (int)std::ceil(s.clip.x + s.clip.w) - 1);
            y0 = std::max(y0, (int)std::floor(s.clip.y)); y1 = std::min(y1, (int)std::ceil(s.clip.y + s.clip.h) - 1);
        }
        for (int py = y0; py <= y1; ++py)
            for (int px = x0; px <= x1; ++px) {
                // inversa: do pixel de destino (centro) para o sprite
                float dx = (float)px + 0.5f - s.x, dy = (float)py + 0.5f - s.y;
                float lx = dx * cs - dy * sn, ly = dx * sn + dy * cs;
                lx /= s.scaleX; ly /= s.scaleY;
                if (s.flipH) lx = -lx;
                if (s.flipV) ly = -ly;
                float fu = lx + s.axisX, fv = ly + s.axisY;
                int u = (int)std::floor(fu), v = (int)std::floor(fv);
                if (u < 0 || v < 0 || u >= t.w || v >= t.h) continue;
                float c[4];   // rgba 0..1
                if (t.indexed) {
                    const uint8_t* e = &(*pal)[t.data[(size_t)v * t.w + u] * 4];
                    c[0] = e[0] / 255.0f; c[1] = e[1] / 255.0f; c[2] = e[2] / 255.0f; c[3] = e[3] / 255.0f;
                } else {
                    const uint8_t* e = &t.data[((size_t)v * t.w + u) * 4];
                    c[0] = e[0] / 255.0f; c[1] = e[1] / 255.0f; c[2] = e[2] / 255.0f; c[3] = e[3] / 255.0f;
                }
                if (!s.mask) c[3] = 1.0f;          // sem mascara o alfa vira 1 (como no shader)
                if (c[3] == 0.0f) continue;        // alfa 0 descarta o pixel
                apply_palfx(c, s.palfx);
                blend_pixel(px, py, c, s.blend, s.alphaSrc, s.alphaDst);
            }
    }

    void fill_rect(const Rect& r, const Color& col, Blend blend, int as, int ad) {
        if (!inFrame_) return;
        int x0 = std::max(0, (int)std::floor(r.x)), x1 = std::min(w_, (int)std::ceil(r.x + r.w));
        int y0 = std::max(0, (int)std::floor(r.y)), y1 = std::min(h_, (int)std::ceil(r.y + r.h));
        float c[4] = { col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f };
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) blend_pixel(x, y, c, blend, as, ad);
    }
    void end_frame() { inFrame_ = false; }
    bool read_pixels(std::vector<uint8_t>& rgba, int& w, int& h) { rgba = fb_; w = w_; h = h_; return !fb_.empty(); }
    const std::string& last_error() const { return err_; }

private:
    static void apply_palfx(float* c, const PalFx& p) {
        if (p.colorBal != 1.0f) {                                  // saturacao
            float grey = c[0] * 0.3125f + c[1] * 0.4375f + c[2] * 0.25f;
            for (int i = 0; i < 3; ++i) c[i] = grey + (c[i] - grey) * p.colorBal;
        }
        if (p.invertAll) for (int i = 0; i < 3; ++i) c[i] = 1.0f - c[i];
        for (int i = 0; i < 3; ++i) c[i] = (c[i] + p.add[i]) * p.mul[i];   // (cor + add) * mul
    }
    // Formulas de blend da documentacao do MUGEN (AINDA nao conferidas no exe):
    //   Alpha: out = src*a + dst*(1-a), a = alfa do pixel * alphaSrc/256
    //   Add:   out = dst*alphaDst/256 + src*alphaSrc/256 * alfa
    //   Sub:   out = dst*alphaDst/256 - src*alphaSrc/256 * alfa   (minimo 0)
    void blend_pixel(int x, int y, const float* s, Blend b, int as, int ad) {
        uint8_t* d = &fb_[((size_t)y * w_ + x) * 4];
        float sa = s[3] * (float)as / 256.0f, da = (float)ad / 256.0f;
        for (int i = 0; i < 3; ++i) {
            float dst = d[i] / 255.0f, out;
            float src = clamp01(s[i]);
            if (b == Blend::Alpha)    out = src * sa + dst * (1.0f - sa);
            else if (b == Blend::Add) out = dst * da + src * sa;
            else                      out = dst * da - src * sa;
            d[i] = (uint8_t)(clamp01(out) * 255.0f + 0.5f);
        }
        d[3] = 255;
    }

    int w_, h_; uint32_t nextTex_, nextPal_; bool inFrame_;
    std::vector<uint8_t> fb_;
    std::map<TextureId, Tex> tex_;
    std::map<PaletteId, std::vector<uint8_t> > pal_;
    std::string err_;
};

std::unique_ptr<Video> create_software() { return std::unique_ptr<Video>(new SoftVideo()); }

} // namespace video
