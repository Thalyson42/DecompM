#include "video/video.h"
#include "video/video_null.h"

namespace video {

class NullVideoImpl : public NullVideo {
public:
    NullVideoImpl() : nextTex_(1), nextPal_(1), inFrame_(false) { caps_.renderer = "null"; }
    const VideoCaps& caps() const { return caps_; }
    Backend backend() const { return Backend::Null; }
    bool init(const VideoConfig& cfg) { cfg_ = cfg; calls_.clear(); return true; }
    void shutdown() {}
    void resize(int w, int h) { cfg_.windowWidth = w; cfg_.windowHeight = h; }
    TextureId create_texture_indexed(int, int, const uint8_t*) { return nextTex_++; }
    TextureId create_texture_rgba(int, int, const uint8_t*) { return nextTex_++; }
    void destroy_texture(TextureId) {}
    PaletteId create_palette(const uint8_t*, int) { return nextPal_++; }
    void update_palette(PaletteId, const uint8_t*, int) {}
    void destroy_palette(PaletteId) {}
    void begin_frame(const Color&) { inFrame_ = true; Call c; c.kind = Call::Begin; calls_.push_back(c); }
    void draw_sprite(const SpriteDraw& s) { if (!inFrame_) return; Call c; c.kind = Call::Sprite; c.sprite = s; calls_.push_back(c); }
    void fill_rect(const Rect& r, const Color&, Blend, int, int) { if (!inFrame_) return; Call c; c.kind = Call::Rect; c.rect = r; calls_.push_back(c); }
    void end_frame() { inFrame_ = false; Call c; c.kind = Call::End; calls_.push_back(c); }
    bool read_pixels(std::vector<uint8_t>& rgba, int& w, int& h) { w = cfg_.logicalWidth; h = cfg_.logicalHeight; rgba.assign((size_t)w * h * 4, 0); return true; }
    const std::string& last_error() const { return err_; }
    const std::vector<Call>& calls() const { return calls_; }
private:
    VideoCaps caps_; VideoConfig cfg_; uint32_t nextTex_, nextPal_; bool inFrame_; std::vector<Call> calls_; std::string err_;
};

std::unique_ptr<Video> create_null() { return std::unique_ptr<Video>(new NullVideoImpl()); }

} // namespace video
